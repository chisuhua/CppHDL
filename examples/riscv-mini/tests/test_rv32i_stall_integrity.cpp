/**
 * @file test_rv32i_stall_integrity.cpp
 * @brief **核级判据**：MEM 级反压期间，被冻结在 EX 级的指令的**操作数完整性**
 *
 * ## 背景（缺陷本体）
 *
 * MEM 级反压（`data_ready = 0`）时，核的三条路径会同时**冻结**：
 * `id_stage.freeze`（ID/EX 全字段保持）· `exmem_en`（EX/MEM 不前进）
 * · `m2_*` / `memwb_*`（M2 与 MEM/WB 保持）。
 *
 * 但**旁路档位**（`fwd_a_d1` / `fwd_b_d1`，`rv32i_pipeline.h`）此前**没有**
 * 跟着冻结：它们的判据来自 `HazardUnit` 的 `forward_a/b`，而后者用的是
 * **ID 级输入指令**的 rs 地址（`id_stage.rs1_addr_id` = **组合**译码）
 * ⇒ ⇒ 反压期间 ID 输入已翻到**后继指令** ⇒ ⇒ 每拍刷新出来的档位是
 * **后继指令的决策** ⇒ ⇒ 它被施加到 EX 级那条**被冻结**的指令上
 * ⇒ ⇒ 那条指令的操作数被**别的指令的旁路值**改写。
 *
 * **可观测后果**（本文件的判据）：
 * - 寄存器**值**被改写（ALU 结果错）；
 * - **分支方向**被翻转（控制面：可能假重定向 / 丢重定向）。
 *
 * ## 触发链（本文件在**核级**判的原因）
 *
 * 核里 D-L1 是 `DCacheSync`：**冷启动的第一次访问必然未命中**
 * ⇒ ⇒ AXI 回填 ⇒ `dcache.ready = 0` ⇒ `data_ready_calc = 0`
 * ⇒ ⇒ `mem_stall = 1` ⇒ ⇒ **多拍冻结**（实测 ≈ 9 拍）。
 *
 * ⇒ ⇒ 因此本文件**不需要**「在第几拍拉低就绪信号」这类**时序激励**
 * （那会把实现时序写进判据）⇒ ⇒ 冻结是**自然发生**的 ⇒ 判据稳定。
 *
 * ## 两个触发形态（缺一不可）
 *
 * | 形态 | 条件 | 现象 |
 * |---|---|---|
 * | **M1 · 后继别名型** | 后继指令的 rs 与 EX / EX-MEM 的 rd **相等** | 操作数被**前推值**改写 |
 * | **M2 · 决策回退型** | 被冻结指令**自身**在冻结入口档位就**非 0**（距离-2 生产者此刻在 M2） | 操作数**回退**到 `idex_rs1_data` / `idex_rs2_data` 的**陈旧**值 |
 *
 * ⇒ ⇒ 「只抑制假命中」的修法（给 `forward_*` 加有效位门控）**堵不住 M2**
 * ⇒ ⇒ 两个用例都必须有（M2 用例同时是「冻结期清零档位」这种错法的杀手）。
 *
 * ⚠️ **A 口 / B 口 MUST 分别覆盖**：A1 的冻结指令只消费 **rs1**（`addi` 不消费 rs2）
 * ⇒ ⇒ A2 **必须**把被污染的操作数放在 **rs2**（`add x6, x0, x5`）
 * ⇒ ⇒ 否则「只回退 `fwd_b_d1`」的变异会**恒绿**（critic 复核指出）。
 *
 * ## 期望值一律手推（禁抄仿真输出）
 *
 * ## 程序来源
 *
 * 三个程序都由 `riscv32-unknown-elf-as -march=rv32i` 汇编 + `objdump` 逐条核对
 * ⇒ 指令字**不是手写的**（手写位段曾出错两次：rd 位段 / B 型 imm 位段）。
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../../../tests/catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "rv32i_core_top.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace {

// ── 观测面 ────────────────────────────────────────────────────────────────
// 只用核的**既有根端口**（不新增端口）：
//   · `dbg_wb_write_en` / `dbg_wb_rd_addr` / `dbg_wb_write_data` ⇒ 写回事件
//   · `dbg_id_pc` ⇒ 取指 PC（**冻结的直接见证**：连续多拍相同 ⇒ 流水线停了）
constexpr std::uint64_t kItcmBase = 0x80000000ull;
constexpr std::uint64_t kDtcmBase = 0x80010000ull;

struct RunResult {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;  // (rd, data)
    std::vector<std::uint64_t> id_pc;                             // 每拍取指 PC
    std::uint64_t retired = 0;
    std::size_t longest_pc_stall = 0;   // **连续相同 id_pc 的最大拍数**
};

RunResult run_core(const std::vector<std::uint32_t>& itcm,
                   const std::vector<std::uint32_t>& dtcm,
                   std::size_t max_cycles) {
    using Dev = ch::ch_device<Rv32iCoreTop, const std::vector<std::uint32_t>&,
                               const std::vector<std::uint32_t>&>;
    auto dev = std::make_unique<Dev>(itcm, dtcm);
    ch::Simulator sim(dev->context());
    auto& io = dev->instance().io();

    // ⚠️ **核级没有 MMIO 从设备** ⇒ 8 根 `axil_*` 输入**显式 tie 0**
    //    （未驱动 ⇒ 读到未定义值 ⇒ 期望值会「碰巧」通过 ⇒ 恒绿判据）
    sim.set_input_value(io.ext_rst, 0u);
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0u);
    sim.set_input_value(io.prog_data, 0u);
    sim.set_input_value(io.axil_awready, 0u);
    sim.set_input_value(io.axil_wready, 0u);
    sim.set_input_value(io.axil_bresp, 0u);
    sim.set_input_value(io.axil_bvalid, 0u);
    sim.set_input_value(io.axil_arready, 0u);
    sim.set_input_value(io.axil_rdata, 0u);
    sim.set_input_value(io.axil_rresp, 0u);
    sim.set_input_value(io.axil_rvalid, 0u);

    RunResult r;
    r.id_pc.reserve(max_cycles);
    std::size_t same_run = 0;
    std::uint64_t prev_pc = ~0ull;

    for (std::size_t c = 0; c < max_cycles; ++c) {
        sim.tick(1);

        const auto en = static_cast<std::uint64_t>(sim.get_port_value(io.dbg_wb_write_en));
        const auto rd = static_cast<std::uint64_t>(sim.get_port_value(io.dbg_wb_rd_addr));
        const auto wd = static_cast<std::uint64_t>(sim.get_port_value(io.dbg_wb_write_data));
        if (en != 0) r.writes.emplace_back(rd, wd & 0xFFFFFFFFull);

        const auto pc = static_cast<std::uint64_t>(sim.get_port_value(io.dbg_id_pc));
        r.id_pc.push_back(pc);
        if (pc == prev_pc) {
            ++same_run;
        } else {
            same_run = 0;
            prev_pc = pc;
        }
        if (same_run + 1 > r.longest_pc_stall) r.longest_pc_stall = same_run + 1;
    }
    r.retired = static_cast<std::uint64_t>(sim.get_port_value(io.obs_perf_instructions));
    return r;
}

// 「最后一次写回某寄存器的值」；从未被写过 ⇒ 返回 false
bool last_write(const RunResult& r, unsigned rd, std::uint64_t& value) {
    bool found = false;
    for (const auto& w : r.writes) {
        if (w.first == rd) {
            value = w.second;
            found = true;
        }
    }
    return found;
}

// D-TCM 的初值：字 0 = 0x1234_56FF ⇒ **字节 0 = 0xFF**
//   （`lb` 读它 ⇒ 0xFF 符号扩展 ⇒ 0xFFFF_FFFF；`lw` 读它 ⇒ 0x1234_56FF）
const std::vector<std::uint32_t> kDtcmImage = {0x123456FFu};

constexpr std::size_t kCycles = 160;

}  // namespace

// ============================================================================
// A1 · M1（后继别名型）：被冻结指令的**寄存器值**被改写
// ============================================================================
TEST_CASE("核级反压：冻结期操作数完整性（M1 · 后继别名型）",
          "[riscv-mini][pipeline][operand-integrity]") {
    // 程序（工具链汇编 + objdump 核对）：
    //   0x00 lui  x2, 0x80010    → x2 = 0x8001_0000（D-TCM 区首）
    //   0x04 lui  x3, 0x80010    → x3 = 0x8001_0000
    //   0x08 lb   x14, 0(x2)     → **D-L1 冷未命中** ⇒ 多拍反压开始
    //   0x0c addi x7, x0, -1     → 冻结期被冻在 EX 的那条（**用 rs1**）
    //   0x10 bne  x14, x7, 0x18  → ID 输入那条：rs1 = 14（= EX/MEM 里 lb 的 rd）
    //                              rs2 = 7 （= EX 里 addi 的 rd）⇒ **两个别名**
    //   0x14 jal  x0, 0          → 顺序落空：停机（x8 **不被写**）
    //   0x18 addi x8, x0, 0x7ff  → 分支成立路径：x8 = 0x7FF
    //   0x1c jal  x0, 0
    const std::vector<std::uint32_t> itcm = {
        0x80010137u, 0x800101B7u, 0x00010703u, 0xFFF00393u,
        0x00771463u, 0x0000006Fu, 0x7FF00413u, 0x0000006Fu};

    auto r = run_core(itcm, kDtcmImage, kCycles);

    // ── 前置：**冻结真的发生过**（否则下面的判据是恒绿的假绿）──
    //   见证 = 取指 PC **连续多拍相同**（流水线被停住 ⇒ PC 不前进）
    INFO("longest_pc_stall=" << r.longest_pc_stall << " retired=" << r.retired);
    REQUIRE(r.longest_pc_stall >= 3);

    std::uint64_t v = 0;
    REQUIRE(last_write(r, 14, v));          // lb 确实退了
    REQUIRE(v == 0xFFFFFFFFull);            // 手推：D-TCM 字 0 的字节 0 = 0xFF ⇒ 符号扩展
    REQUIRE(last_write(r, 7, v));           // addi x7, x0, -1 确实退了
    REQUIRE(v == 0xFFFFFFFFull);            // 手推：rs1 = x0 = 0 ⇒ 0 + (-1)
    REQUIRE_FALSE(last_write(r, 8, v));     // bne MUST NOT 成立（x14 == x7）⇒ x8 不被写
}

// ============================================================================
// A2 · M2（决策回退型）：操作数**回退**到陈旧值
// ============================================================================
TEST_CASE("核级反压：冻结期操作数完整性（M2 · 决策回退型）",
          "[riscv-mini][pipeline][operand-integrity]") {
    // 程序（工具链汇编 + objdump 核对）：
    //   0x00 lui  x2, 0x80010    → x2 = 0x8001_0000
    //   0x04 addi x5, x0, 42     → x5 = 42（**生产者**）
    //   0x08 lw   x14, 0(x2)     → **D-L1 冷未命中** ⇒ 多拍反压开始
    //   0x0c add  x6, x0, x5     → 冻结期被冻在 EX 的那条：消费 **rs2**（距离 2
    //                              ⇒ 生产者此刻在 M2 ⇒ 冻结入口档位 = MEM 档）
    //   0x10 addi x9, x0, 7      → ID 输入那条：rs = 0/0 ⇒ **无别名**
    //                              ⇒ 旧实现把档位刷成 0 ⇒ 操作数**回退**到陈旧值
    //
    // ⚠️ **为什么用 `add x6, x0, x5`（rs1 = x0 · rs2 = x5）而不是 `addi x6, x5, 1`**：
    //   判据 MUST **分别**覆盖 **A 口（rs1 档位 `fwd_a_d1`）** 与 **B 口（rs2 档位
    //   `fwd_b_d1`）** 两个派生状态 —— A1 的冻结指令 `addi x7, x0, -1` **只走 rs1**
    //   （`addi` 不消费 rs2）⇒ ⇒ 若本用例也走 rs1，则**只回退 `fwd_b_d1` 的变异
    //   会恒绿**（critic 复核指出）。⇒ ⇒ 本用例把被污染的操作数放在 **rs2**
    //   （rs1 = x0 ⇒ 它的档位本来恒 0 ⇒ 与污染无关）。
    const std::vector<std::uint32_t> itcm = {
        0x80010137u, 0x02A00293u, 0x00012703u, 0x00500333u, 0x00700493u};

    auto r = run_core(itcm, kDtcmImage, kCycles);

    INFO("longest_pc_stall=" << r.longest_pc_stall << " retired=" << r.retired);
    REQUIRE(r.longest_pc_stall >= 3);

    std::uint64_t v = 0;
    REQUIRE(last_write(r, 5, v));
    REQUIRE(v == 42ull);                    // 手推：42
    // ⚠️ **本用例是 M2（**B 口**）的判别点**：x6 MUST 经 **MEM 档**拿到 x5 = 42
    //    ⇒ x6 = 0 + 42 = **42**。
    //    旧实现（档位被刷成 0）⇒ x6 的 **rs2** 读到 `idex_rs2_data` 的**陈旧** x5 = 0
    //    ⇒ x6 = 0 ⇒ **必红**。
    //    「冻结期清零档位」这种错法 ⇒ 同样 x6 = 0 ⇒ 本用例**同时**杀掉它。
    REQUIRE(last_write(r, 6, v));
    REQUIRE(v == 42ull);
    REQUIRE(last_write(r, 9, v));
    REQUIRE(v == 7ull);
}

// ============================================================================
// A3 · 控制面：冻结中的条件分支**方向**不得被翻转
// ============================================================================
TEST_CASE("核级反压：冻结期控制面完整性（分支方向不得翻转）",
          "[riscv-mini][pipeline][operand-integrity]") {
    // 程序（工具链汇编 + objdump 核对）：
    //   0x00 lui  x2, 0x80010    → x2 = 0x8001_0000（D-TCM 区首）
    //   0x04 addi x5, x0, 42     → x5 = 42（**生产者**：距 beq = 2 ⇒ 冻结入口档位 = MEM 档）
    //   0x08 lw   x14, 0(x2)     → **D-L1 冷未命中** ⇒ 多拍反压开始（EX/MEM = 这条 lw）
    //   0x0c beq  x5, x0, 0x18   → **冻结期被冻在 EX 的那条**（条件分支 ⇒ 控制面）
    //                              ⇒ 手推：42 != 0 ⇒ 分支**不成立** ⇒ 顺序落空 ⇒ x8 不被写
    //   0x10 addi x9, x0, 0x11   → 顺序落空路径：x9 = 0x11
    //   0x14 jal  x0, 0          → 停机
    //   0x18 addi x8, x0, 0x22   → 分支成立路径：x8 = 0x22
    //
    // ⚠️ **为什么这个形态才有判别力（前面三版恒绿的根因）**：
    //   判据要求「污染**改变分支的比较结果**」⇒ ⇒ 被冻结分支的 rs1 在
    //   **冻结入口**档位就 MUST **非 0**（否则污染把它刷成 0 ⇒ 读回的仍是
    //   寄存器堆里的**同一个值** ⇒ 比较结果不变 ⇒ 恒绿）。
    //   ⇒ ⇒ 这里让 beq 读**距离 2** 的 x5（生产者此刻在 MEM/WB）
    //   ⇒ ⇒ 冻结入口档位 = **2（MEM 档）** ⇒ 正确实现读到 **42**（写回值）
    //   ⇒ ⇒ 后继指令（IF/ID 那条）rs = 0/0 ⇒ **无别名** ⇒ 旧实现把档位刷成 **0**
    //     ⇒ ⇒ 操作数**回退**到 `idex_rs1_data` 的**陈旧值** = 0（x5 尚未写回）
    //     ⇒ ⇒ 0 == 0 ⇒ 分支**成立** ⇒ 跳 0x18 ⇒ x8 = 0x22 ⇒ **必红**。
    //
    // ⚠️ **实测踩过的三版恒绿形态（勿回退）**：
    //   ① `beq x0,x0`   —— 立即成立 ⇒ 在冻结开始**之前**就 flush ⇒ 根本没经历冻结；
    //   ② `beq x14,x0`  —— 冻结入口档位 = 2，污染后档位**仍是** 2 ⇒ 值不变；
    //   ③ `beq x14,x14` —— rs1 == rs2 ⇒ 两个操作数被**同样**污染 ⇒ 值不变。
    const std::vector<std::uint32_t> itcm = {
        0x80010137u, 0x02A00293u, 0x00012703u, 0x00028663u,
        0x01100493u, 0x0000006Fu, 0x02200413u};

    auto r = run_core(itcm, kDtcmImage, kCycles);

    INFO("longest_pc_stall=" << r.longest_pc_stall << " retired=" << r.retired);
    REQUIRE(r.longest_pc_stall >= 3);

    std::uint64_t v = 0;
    REQUIRE(last_write(r, 5, v));
    REQUIRE(v == 42ull);                    // 手推：42
    REQUIRE(last_write(r, 14, v));
    REQUIRE(v == 0x123456FFull);            // 手推：D-TCM 字 0 = 0x1234_56FF
    // ⚠️ **控制面判别点**：分支方向 MUST NOT 被翻转。
    //    正确：`beq x5, x0` ⇒ 42 != 0 ⇒ **不成立** ⇒ 顺序落空 ⇒ x8 **不被写**。
    //    旧实现：档位被刷成 0 ⇒ rs1 **回退**到陈旧值 0 ⇒ 0 == 0 ⇒ **成立**
    //    ⇒ ⇒ 跳 0x18 ⇒ x8 = 0x22 ⇒ **必红**。
    REQUIRE_FALSE(last_write(r, 8, v));    // 分支成立路径不该被执行
    REQUIRE(last_write(r, 9, v));          // 顺序落空路径**必须**执行
    REQUIRE(v == 0x11ull);
}
