/**
 * @file test_rv32i_trap_entry.cpp
 * @brief **RV32I trap 入口接线**（P6-3）的**核级**判据
 *        （change `rv32i-trap-entry-2026-11`）
 *
 * ## DUT
 *
 * `src/rv32i_core_top.h`（核 IP 顶层）· 用**构造参数**装载 itcm 字镜像
 * （与 P6-2 的 `test_rv32i_csr_instructions.cpp` **同型** ⇒ ⇒ 零 `prog_*` 写
 * ⇒ ⇒ 端口面零变化）。
 *
 * ⚠️ **为什么 MUST 用核级 DUT（而不是直接实例化 `Rv32iPipeline`）**：
 * imem 是 **1 拍 `sread`**（ADR-041 强制）⇒ ⇒ 取指数据比取指地址**晚 1 拍**
 * ⇒ ⇒ 直接把镜像「组合」喂给流水线的 `instr_data` ⇒ ⇒ **取指与 PC 错开一个字**
 * ⇒ ⇒ 译码到的指令与 `dbg_id_pc` **不对应** ⇒ ⇒ 判据全红且**测的不是产品形态**。
 * ⇒ ⇒ 核级 DUT 的 I-TCM 是**真 `sread`** ⇒ ⇒ 取指对齐由**产品**保证。
 *
 * ## 取指索引口径（**必须与核顶层一致**）
 *
 * 核顶层把字节地址折成 **12 位字索引**：`bits<kTcmAddrWidth + 1, 2>(instr_addr)`
 * ⇒ ⇒ **高位被丢弃** ⇒ ⇒ `pc = 0x8000_0100`（`mtvec` 复位值 · **P6-4 起**；原
 * `0x2000_0100` 靠同一别名「碰巧」工作）⇒ ⇒ 索引 **`0x040`**
 * ⇒ ⇒ 「trap 之后从落点继续执行」的处理器**必须**放在 **字 `0x040`** 起。
 *
 * ## 期望值 MUST **手推**（禁抄仿真输出）
 *
 * 解释器与 Verilog codegen 由**同一份 C++** 生成 ⇒ ⇒ 同构缺陷**两后端一致地错**
 * ⇒ ⇒ 双后端 parity 对该族缺陷**结构性失明** ⇒ ⇒ 本文件的期望值全部由
 * RISC-V 规范 + `CsrBank` 的**复位值**（`mtvec` = `0x8000_0100` · `mepc` = `0`
 * · `mcause` = `0`）**手推**。
 *
 * ## 判据 ↔ 验收条目
 *
 * | TEST_CASE | AC | 判别力（变异 ⇒ 必红的那一条） |
 * |---|---|---|
 * | 未知 opcode ⇒ 陷入：`mepc` / `mcause` 写对 + PC 重定向 | AC1 / AC2 / AC3 | `mepc` 写成 0 ⇒ `x1` 读回 0 |
 * | 在途更老指令照常退休（trap **不杀** EX/MEM/WB） | AC4 | trap 改成杀整条流水线 ⇒ `x3` 无写回 |
 * | 控制流冲刷优先于 trap（错误路径上的非法字**不** trap） | AC5 | 去掉 `!flush` ⇒ `mepc` 被写坏 |
 * | `ECALL` ⇒ 陷入：`mcause = 11`（from **M**）· `mepc` = 自身 | P6-4 AC1 | 原因码写成 2 ⇒ `x2` 读回 2；ECALL 归 reason 1（冻结）⇒ 无写回 |
 * | `EBREAK` ⇒ 陷入：`mcause = 3`（breakpoint） | P6-4 AC2 | 原因码写成 2 ⇒ `x2` 读回 2 |
 * | `SRET`（`0x102`）⇒ `reason = 3` ⇒ 陷入（`mcause = 2`） | P6-4 AC3 | 归 reason 1 ⇒ 冻结 ⇒ 无写回 |
 * | `WFI`（`0x105`）⇒ 合法 NOP（**不** trap · 指令流继续） | P6-4 AC4 | 冻结 / 判非法 ⇒ 后续 `csrrs` 不执行 |
 * | `MRET`（`0x302`）⇒ `reason = 1` **仍冻结**（P6-5 前） | P6-4 AC6 | 提前当 NOP / trap ⇒ 有写回 |
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "rv32i_core_top.h"

#include "cpu/riscv/rv32i_asm_encode.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ---------------------------------------------------------------------------
// 常量（**全部派生或手推**）
// ---------------------------------------------------------------------------
constexpr uint32_t kOpSystem = 0x73u;
constexpr uint32_t kF3Csrrs = 2u;
constexpr uint32_t kCsrMepc = 0x341u;
constexpr uint32_t kCsrMcause = 0x342u;

/// 触发指令的 PC = **PC 复位值** = I-TCM 基址（`riscv::kPcReset`）
constexpr uint32_t kPcReset = 0x80000000u;
/// trap 目标的**别名**字索引（`(mtvec >> 2) & 0xFFF`）
constexpr std::size_t kTrapAliasWord = 0x040u;
/// 镜像长度（**必须** ≥ `kTrapAliasWord + 4` ⇒ ⇒ 别名落点之后的字都在镜像内）
constexpr std::size_t kImageWords = 0x048u;

/// `csrrs rd, csr, x0`（**不写** CSR · 只读回旧值）
constexpr uint32_t enc_csrrs(uint32_t rd, uint32_t csr) {
    return riscv::enc_i(rd, 0, kF3Csrrs, static_cast<int32_t>(csr), kOpSystem);
}

/// **未知 opcode**（`opcode = 0`）⇒ ⇒ `illegal_reason = 2` ⇒ ⇒ 触发 trap
constexpr uint32_t kIllegalUnknown = 0x00000000u;
/// `ECALL`（`imm = 0x000`）⇒ ⇒ **合法**（`reason = 0`）· `sys_trap_kind = 1` ⇒ trap
constexpr uint32_t kEcall = 0x00000073u;
/// `EBREAK`（`imm = 0x001`）⇒ ⇒ **合法** · `sys_trap_kind = 2` ⇒ trap（`mcause = 3`）
constexpr uint32_t kEbreak = 0x00100073u;
/// `SRET`（`imm = 0x102`）⇒ ⇒ `misa.S = 0`（M-only）⇒ ⇒ **不可编码** ⇒ `reason = 3`
constexpr uint32_t kSret = 0x10200073u;
/// `WFI`（`imm = 0x105`）⇒ ⇒ **合法 NOP**（规范明文允许）⇒ `reason = 0`
constexpr uint32_t kWfi = 0x10500073u;
/// `MRET`（`imm = 0x302`）⇒ ⇒ **已知未实现**（P6-5）⇒ `reason = 1`（冻结）
constexpr uint32_t kMret = 0x30200073u;
/// `OP_SYSTEM` · `funct3 = 4`（ISA **保留**组合）⇒ ⇒ `illegal_reason = 3`
constexpr uint32_t kF3Reserved = 4u;

// ---------------------------------------------------------------------------
// 逐拍采样
// ---------------------------------------------------------------------------
struct Sample {
    uint64_t instr_addr = 0;  ///< `obs_instr_addr`（当拍取指地址）
    uint64_t id_instr = 0;    ///< `dbg_id_instr`（ID 级指令字）
    uint64_t id_pc = 0;       ///< `dbg_id_pc`（ID 级 PC）
    uint64_t id_valid = 0;    ///< `dbg_id_valid`
    uint64_t reason = 0;      ///< `dbg_illegal_reason`
    uint64_t retired = 0;     ///< `obs_perf_instructions`（退休计数）
};

struct RunOut {
    std::vector<Sample> samples;
    std::vector<std::pair<uint32_t, uint64_t>> writebacks;  ///< 按**时间顺序**
};

/// 跑一个程序（`itcm` = 字镜像 · `dtcm` 空）
///
/// ⚠️ **采样口径与 `test_rv32i_csr_instructions.cpp` 逐字一致**：`sim.tick(1)` 之后取端口值。
RunOut run_program(const std::vector<uint32_t> &image, std::size_t cycles = 160) {
    const std::vector<uint32_t> dtcm;

    using FwDev = ch::ch_device<Rv32iCoreTop, const std::vector<uint32_t> &,
                                const std::vector<uint32_t> &>;
    auto dev = std::make_unique<FwDev>(image, dtcm);
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    // ⚠️ **核级 TB 没有 MMIO 从设备** ⇒ ⇒ 8 根 AXI4-Lite 输入 MUST 显式 tie 0
    //    （未驱动端口读到的是**未定义值** ⇒ ⇒ 期望值会「碰巧」通过）。
    sim.set_input_value(io.axil_awready, 0u);
    sim.set_input_value(io.axil_wready, 0u);
    sim.set_input_value(io.axil_bresp, 0u);
    sim.set_input_value(io.axil_bvalid, 0u);
    sim.set_input_value(io.axil_arready, 0u);
    sim.set_input_value(io.axil_rdata, 0u);
    sim.set_input_value(io.axil_rresp, 0u);
    sim.set_input_value(io.axil_rvalid, 0u);
    // ⚠️ **复位显式 tie 0**（固件由构造参数预装 ⇒ ⇒ 无装载期）
    //    ⇒ 未驱动端口读出**未定义值** ⇒ ⇒ 「碰巧通过」的风险。
    sim.set_input_value(io.ext_rst, 0u);
    sim.set_input_value(io.prog_we, 0u);

    RunOut out;
    for (std::size_t c = 0; c < cycles; ++c) {
        sim.tick(1);

        Sample s;
        s.instr_addr = static_cast<uint64_t>(sim.get_port_value(io.obs_instr_addr));
        s.id_instr = static_cast<uint64_t>(sim.get_port_value(io.dbg_id_instr));
        s.id_pc = static_cast<uint64_t>(sim.get_port_value(io.dbg_id_pc));
        s.id_valid = static_cast<uint64_t>(sim.get_port_value(io.dbg_id_valid));
        s.reason = static_cast<uint64_t>(sim.get_port_value(io.dbg_illegal_reason));
        s.retired = static_cast<uint64_t>(sim.get_port_value(io.obs_perf_instructions));
        out.samples.push_back(s);

        if (sim.get_port_value(io.dbg_wb_write_en) != 0) {
            out.writebacks.emplace_back(
                static_cast<uint32_t>(
                    static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_rd_addr))),
                static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_write_data)));
        }
    }
    return out;
}

std::string fmt_writebacks(const std::vector<std::pair<uint32_t, uint64_t>> &wbs) {
    std::string s;
    for (std::size_t i = 0; i < wbs.size(); ++i) {
        if (i != 0) {
            s += ",";
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%u:0x%llx",
                      static_cast<unsigned>(wbs[i].first),
                      static_cast<unsigned long long>(wbs[i].second));
        s += buf;
    }
    return s;
}

/// 写回序列 == 手推序列（失败时打印两串十六进制）
void require_writebacks(const RunOut &out,
                        const std::vector<std::pair<uint32_t, uint64_t>> &expected) {
    const bool ok = (out.writebacks == expected);
    INFO("actual   = " << fmt_writebacks(out.writebacks));
    INFO("expected = " << fmt_writebacks(expected));
    REQUIRE(ok);
}

/// 造一个「NOP 填充 + 指定位置覆盖」的 itcm 镜像
std::vector<uint32_t> make_image(const std::vector<std::pair<std::size_t, uint32_t>> &patches) {
    std::vector<uint32_t> image(kImageWords, riscv::NOP);
    for (const auto &p : patches) {
        REQUIRE(p.first < image.size());
        image[p.first] = p.second;
    }
    return image;
}

} // namespace

// ===========================================================================
// AC1 / AC2 / AC3：未知 opcode ⇒ 陷入 ⇒ `mepc` / `mcause` 写对 + PC 重定向
//
// 手推：
//   · 触发指令（`opcode = 0` ⇒ `reason = 2`）在 **PC 复位值** `0x8000_0000`
//     ⇒ ⇒ `mepc` = **该指令自身的 PC** = `0x8000_0000`
//   · `mcause` = **illegal instruction** = **2**
//   · PC 重定向到 `mtvec` 基址 = `0x8000_0100`（`CsrBank` 的复位值 · **P6-4 起** ·
//     原 `0x2000_0100` 靠「取指丢高位」的偶然别名落在同一 word）
//     ⇒ ⇒ 核顶层的字索引 = `(0x8000_0100 >> 2) & 0xFFF` = **`0x040`**
//     ⇒ ⇒ 处理器 MUST 放在字 `0x040` ⇒ ⇒ 读回 `mepc` / `mcause` 才会发生
//   · `rd = 0` ⇒ 永不出现在写回序列里 ⇒ ⇒ 写回序列**恰好**两条
// ===========================================================================
TEST_CASE("TRAP - 未知 opcode 陷入：mepc / mcause 写对且 PC 重定向到 mtvec",
          "[riscv][trap]") {
    const std::vector<uint32_t> image = make_image({
        {0x000u, kIllegalUnknown},
        {kTrapAliasWord + 0u, enc_csrrs(1, kCsrMepc)},
        {kTrapAliasWord + 1u, enc_csrrs(2, kCsrMcause)},
        {kTrapAliasWord + 2u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    // ⚠️ 写回序列 = **恰好**两条（处理器只读不写 · `rd != 0` ⇒ 两条都出现）
    require_writebacks(out, {
        {1, 0x80000000ull},   // `mepc` = 触发指令自身的 PC
        {2, 0x2ull},          // `mcause` = illegal instruction
    });

    // 触发拍的非法原因 = **2**（未知 opcode）⇒ ⇒ 至少出现过一次
    bool saw_reason2 = false;
    for (const Sample &s : out.samples) {
        if (s.reason == 2u) {
            saw_reason2 = true;
        }
    }
    CHECK(saw_reason2);

    // ---- AC2 / AC6：触发指令**不退休** + IF/ID 被杀（**连续两拍** ID 级空）----
    // ⚠️ 触发拍 = **第一条** `reason == 2` 的拍（触发指令在 ID ⇒ 该拍 reason 唯一）
    std::size_t trigger = out.samples.size();
    for (std::size_t i = 0; i < out.samples.size(); ++i) {
        if (out.samples[i].reason == 2u) {
            trigger = i;
            break;
        }
    }
    REQUIRE(trigger < out.samples.size());
    REQUIRE(trigger + 2u < out.samples.size());
    // 触发拍：ID 级**有效**（触发指令正在 ID 级）
    CHECK(out.samples[trigger].id_valid == 1u);
    // 触发拍**之前**：退休计数为 **0**（触发指令是第一条 ⇒ ⇒ 它**从未退休**）
    CHECK(out.samples[trigger].retired == 0u);
    // 其后**连续两拍**：ID 级**空**
    //   · T+1：`if_id_valid_reg` 被清零（kill 站点 ④ ⇒ ⇒ 否则**活锁**）
    //   · T+2：`trap_flush_d1` 多杀一拍（imem 是 1 拍 `sread` ⇒ ⇒ 在途指令不进 ID）
    CHECK(out.samples[trigger + 1u].id_valid == 0u);
    CHECK(out.samples[trigger + 2u].id_valid == 0u);
}

// ===========================================================================
// AC4：在途**更老**的指令照常退休（trap **不杀** EX/MEM/WB）
//
// 手推：`addi x3, x0, 0x11` 在 **EX 级**时下一条（非法字）触发 trap
//       ⇒ ⇒ trap **不杀** EX/MEM/WB ⇒ ⇒ `x3` 照常写回 **`0x11`**
//       ⇒ ⇒ `mepc` = **第二条**指令的 PC = `0x8000_0004`
// ===========================================================================
TEST_CASE("TRAP - 在途更老的指令照常退休（trap 不杀 EX/MEM/WB）", "[riscv][trap]") {
    const std::vector<uint32_t> image = make_image({
        {0x000u, riscv::ADDI(3, 0, 0x11)},
        {0x001u, kIllegalUnknown},
        {kTrapAliasWord + 0u, enc_csrrs(1, kCsrMepc)},
        {kTrapAliasWord + 1u, enc_csrrs(2, kCsrMcause)},
        {kTrapAliasWord + 2u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    // （C4b：核心断言在 `require_writebacks` 辅助函数内 ⇒ 此处补一条直接的计数断言）
    CHECK(out.writebacks.size() == 3u);
    require_writebacks(out, {
        {3, 0x11ull},          // 在途更老的指令照常退休
        {1, 0x80000004ull},    // `mepc` = **第二条**指令的 PC
        {2, 0x2ull},           // `mcause` = illegal instruction
    });
}

// ===========================================================================
// AC5：**控制流冲刷优先于 trap**（错误路径上的非法字 MUST NOT 触发 trap）
//
// 手推：`beq x0, x0, +8` ⇒ 目标 = `0x8000_0008`（字 **2**）⇒ ⇒
//       fall-through 槽（字 **1**）上的非法字**在错误路径上** ⇒ ⇒ 不 trap
//       ⇒ ⇒ `mepc` **保持复位值 0** ⇒ ⇒ 读回值 = **0**
//       ⇒ ⇒ `mcause` 同理 = **0**
// ===========================================================================
TEST_CASE("TRAP - flush 优先：错误路径上的非法字 MUST NOT 触发 trap", "[riscv][trap]") {
    const std::vector<uint32_t> image = make_image({
        {0x000u, riscv::BEQ(0, 0, 8)},   // ⇒ taken ⇒ 目标 = 字 2
        {0x001u, kIllegalUnknown},        // ⇒ fall-through 槽上的非法字（错误路径）
        {0x002u, enc_csrrs(1, kCsrMepc)},
        {0x003u, enc_csrrs(2, kCsrMcause)},
        {0x004u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    // （C4b：核心断言在 `require_writebacks` 辅助函数内 ⇒ 此处补一条直接的计数断言）
    CHECK(out.writebacks.size() == 2u);
    require_writebacks(out, {
        {1, 0x0ull},   // `mepc` 保持复位值 ⇒ **没 trap**
        {2, 0x0ull},   // `mcause` 保持复位值 ⇒ **没 trap**
    });
}

// ===========================================================================
// AC7（触发集的另一半）：`illegal_reason = 3`（ISA 保留组合）⇒ ⇒ **陷入**
//
// 手推：`OP_SYSTEM` + `funct3 = 4` ⇒ ⇒ 译码器给出 `reason = 3`（保留组合）
//       ⇒ ⇒ 在触发集 `{2,3}` 内 ⇒ ⇒ 陷入 ⇒ ⇒ `mepc` = 该指令自身的 PC
//       ⇒ ⇒ `mcause` = **illegal instruction** = **2**
//       ⇒ ⇒ 处理器在别名落点 `0x040` ⇒ ⇒ 读回 `mepc` / `mcause`
//
// ⚠️ **与 P6-2 文件的分工**：那边只判「译码级」的 `reason == 3`（P6-2 的归属）；
//    这边判「`reason = 3` ⇒ 陷入」的**完整语义**（P6-3 的归属）。
// ===========================================================================
TEST_CASE("TRAP - reason=3（保留组合）⇒ 陷入（mepc = 触发指令的 PC）",
          "[riscv][trap]") {
    const uint32_t reserved =
        riscv::enc_i(0, 0, kF3Reserved, static_cast<int32_t>(kCsrMcause), kOpSystem);
    const std::vector<uint32_t> image = make_image({
        {0x000u, reserved},
        {kTrapAliasWord + 0u, enc_csrrs(1, kCsrMepc)},
        {kTrapAliasWord + 1u, enc_csrrs(2, kCsrMcause)},
        {kTrapAliasWord + 2u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    // （C4b：核心断言在 `require_writebacks` 辅助函数内 ⇒ 此处补一条直接的计数断言）
    CHECK(out.writebacks.size() == 2u);
    require_writebacks(out, {
        {1, 0x80000000ull},   // `mepc` = 触发指令自身的 PC
        {2, 0x2ull},          // `mcause` = illegal instruction
    });
}

// ===========================================================================
// AC7：`CsrBank` **单写口仲裁** —— trap 上下文 MUST 优先（**同拍冲突**）
//
// 手推：
//   word 0  `addi x1, x0, 0x2A`            ⇒ x1 = 0x2A
//   word 1  `csrrw x0, mscratch, x1`       ⇒ 本应写 `mscratch` = 0x2A
//   word 2  非法字（`opcode = 0`）          ⇒ trap（PC = 0x8000_0008）
//   ⇒ ⇒ `csrrw` 在 **EX 级**的那一拍 ⇔ 非法字在 **ID 级** ⇒ ⇒ **同拍**
//   ⇒ ⇒ 写口 MUST 让给 **trap 上下文**（写 `mepc`）⇒ ⇒ `mscratch` 的写被**抑制**
//   ⇒ ⇒ 处理器读 `mscratch` ⇒ **0**（复位值）⇒ ⇒ ★ 变异（让 CSR 指令优先）⇒ 0x2A
//   ⇒ ⇒ `mepc` = **0x8000_0008**（非法字自身的 PC）
// ===========================================================================
TEST_CASE("TRAP - 单写口仲裁：trap 上下文优先于 CSR 指令的写", "[riscv][trap]") {
    constexpr uint32_t kCsrMscratch = 0x340u;
    const std::vector<uint32_t> image = make_image({
        {0x000u, riscv::ADDI(1, 0, 0x2A)},
        {0x001u, riscv::enc_i(0, 1, /*funct3=*/1u, static_cast<int32_t>(kCsrMscratch),
                              kOpSystem)},   // csrrw x0, mscratch, x1
        {0x002u, kIllegalUnknown},
        {kTrapAliasWord + 0u, enc_csrrs(3, kCsrMscratch)},   // ⇒ x3 = mscratch
        {kTrapAliasWord + 1u, enc_csrrs(4, kCsrMepc)},       // ⇒ x4 = mepc
        {kTrapAliasWord + 2u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    // （C4b：核心断言在 `require_writebacks` 辅助函数内 ⇒ 此处补一条直接的计数断言）
    CHECK(out.writebacks.size() == 3u);
    require_writebacks(out, {
        {1, 0x2Aull},          // `addi` 照常退休
        {3, 0x0ull},           // `mscratch` 的写被抑制 ⇒ 仍是复位值 0
        {4, 0x80000008ull},    // `mepc` = 第二条 `addi` 之后那条非法字的 PC
    });
}

// ===========================================================================
// AC12 / V9（**P6-4 订正**）：`ECALL` ⇒ **陷入**（`mcause = 11`）
//
// 手推：
//   · `ECALL` 在 word 0（PC = `0x8000_0000`）⇒ ⇒ `mepc` = **该指令自身**
//   · `mcause` = **11**（ECALL from **M**；`8` 是 U-mode ⇒ 本核不可达）
//   · 规范（§2.1.3.1）：「performs no other operation」⇒ ⇒ ECALL **不退休**
//   · 处理器在落点 word `0x040`（`mtvec` = `0x8000_0100` ⇒ 同 word）
//   · ⚠️ `ECALL` 是**合法指令**（`reason = 0`）⇒ ⇒ 采样里 MUST **不出现**非零 reason
//     （若把它归 reason 1 ⇒ **冻结** ⇒ 无写回；若归 reason 0 但不触发 trap ⇒
//     当 NOP 退休 ⇒ 同样无写回 ⇒ ⇒ 本用例**双向**钉住）
// ===========================================================================
TEST_CASE("TRAP - ECALL 陷入：mcause = 11（from M）· mepc = ECALL 自身",
          "[riscv][trap][p6-4]") {
    const std::vector<uint32_t> image = make_image({
        {0x000u, kEcall},
        {kTrapAliasWord + 0u, enc_csrrs(1, kCsrMepc)},
        {kTrapAliasWord + 1u, enc_csrrs(2, kCsrMcause)},
        {kTrapAliasWord + 2u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    require_writebacks(out, {
        {1, 0x80000000ull},   // `mepc` = ECALL 自身
        {2, 11ull},           // `mcause` = ECALL from M-mode
    });

    // `ECALL` **不是**非法指令 ⇒ reason 恒 0
    for (const Sample &s : out.samples) {
        CHECK(s.reason == 0u);
    }

    // ECALL 在 ID 级那一拍 ⇒ 尚未退休（它永不退休）
    std::size_t ecall_in_id = out.samples.size();
    for (std::size_t i = 0; i < out.samples.size(); ++i) {
        if (out.samples[i].id_valid == 1u && out.samples[i].id_instr == kEcall) {
            ecall_in_id = i;
            break;
        }
    }
    REQUIRE(ecall_in_id < out.samples.size());
    CHECK(out.samples[ecall_in_id].retired == 0u);
}

// ===========================================================================
// P6-4 AC2：`EBREAK` ⇒ 陷入（`mcause = 3` · breakpoint）
//
// 手推：`EBREAK`（`imm = 0x001`）⇒ 合法 · `sys_trap_kind = 2`
//       ⇒ ⇒ `mcause` = **3** · `mepc` = EBREAK 自身 = `0x8000_0000`
//       ⚠️ `3` 在**中断侧**是 MSI ⇒ 靠 `mcause[31] = 0` 区分（本核 P6-6 才实现中断）
// ===========================================================================
TEST_CASE("TRAP - EBREAK 陷入：mcause = 3（breakpoint）", "[riscv][trap][p6-4]") {
    const std::vector<uint32_t> image = make_image({
        {0x000u, kEbreak},
        {kTrapAliasWord + 0u, enc_csrrs(1, kCsrMepc)},
        {kTrapAliasWord + 1u, enc_csrrs(2, kCsrMcause)},
        {kTrapAliasWord + 2u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    require_writebacks(out, {
        {1, 0x80000000ull},   // `mepc` = EBREAK 自身
        {2, 3ull},            // `mcause` = breakpoint
    });
    for (const Sample &s : out.samples) {
        CHECK(s.reason == 0u);   // 合法指令
    }
}

// ===========================================================================
// P6-4 AC3：`SRET`（`imm = 0x102`）⇒ **不可编码**（`misa.S = 0`）⇒ `reason = 3` ⇒ 陷入
//
// 手推：SRET 是 S-mode 的 trap-return ⇒ M-only 实现**没有** `sepc` / `sstatus`
//       ⇒ ⇒ 必须抛 **illegal-instruction**（`mcause = 2`）⇒ ⇒ `mepc` = SRET 自身
//       ⚠️ **MUST NOT** 归 `reason = 1`：那在本核语义里是**冻结** ⇒ SRET 会 **hang**
//       （规范要求异常 ⇒ 软件可处理）
// ===========================================================================
TEST_CASE("TRAP - SRET（0x102）不可编码 ⇒ reason 3 ⇒ 陷入（mcause = 2）",
          "[riscv][trap][p6-4]") {
    const std::vector<uint32_t> image = make_image({
        {0x000u, kSret},
        {kTrapAliasWord + 0u, enc_csrrs(1, kCsrMepc)},
        {kTrapAliasWord + 1u, enc_csrrs(2, kCsrMcause)},
        {kTrapAliasWord + 2u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    require_writebacks(out, {
        {1, 0x80000000ull},   // `mepc` = SRET 自身
        {2, 2ull},            // `mcause` = illegal instruction
    });

    bool saw_reason3 = false;
    for (const Sample &s : out.samples) {
        if (s.reason == 3u) {
            saw_reason3 = true;
        }
        CHECK(s.reason != 1u);   // 不得落进「冻结」桶
    }
    CHECK(saw_reason3);
}

// ===========================================================================
// P6-4 AC5：`funct3 = 0` 的**未定义 imm**（`0x002`）⇒ `reason = 3` ⇒ 陷入
//
// 手推：合法 imm 只有 `0x000` / `0x001` / `0x105` / `0x302` ⇒ ⇒ `0x002` 是
//       **本实现不可编码** ⇒ ⇒ `mcause = 2`（illegal instruction）
// ===========================================================================
TEST_CASE("TRAP - SYSTEM funct3=0 的未定义 imm ⇒ reason 3 ⇒ 陷入",
          "[riscv][trap][p6-4]") {
    constexpr uint32_t kUndefinedImm = 0x002u;
    const uint32_t undef = riscv::enc_i(0, 0, 0, static_cast<int32_t>(kUndefinedImm),
                                        kOpSystem);
    const std::vector<uint32_t> image = make_image({
        {0x000u, undef},
        {kTrapAliasWord + 0u, enc_csrrs(1, kCsrMepc)},
        {kTrapAliasWord + 1u, enc_csrrs(2, kCsrMcause)},
        {kTrapAliasWord + 2u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    require_writebacks(out, {
        {1, 0x80000000ull},
        {2, 2ull},
    });
    bool saw_reason3 = false;
    for (const Sample &s : out.samples) {
        if (s.reason == 3u) {
            saw_reason3 = true;
        }
    }
    CHECK(saw_reason3);
}

// ===========================================================================
// P6-4 AC4：`WFI`（`imm = 0x105`）⇒ **合法 NOP**（不 trap · 指令流继续）
//
// 手推（规范 §2.1.3.3）：「**a legal implementation is to simply implement the
//       WFI instruction as a NOP**」⇒ ⇒ `reason = 0` · 不触发 trap
//       ⇒ ⇒ 后续 `csrrs` 照常执行 ⇒ ⇒ `mepc` / `mcause` 仍是**复位值 0**
//       ⚠️ 判别力：冻结（旧行为）或判非法 ⇒ ⇒ 两条 `csrrs` **永不执行** ⇒ 写回为空
// ===========================================================================
TEST_CASE("TRAP - WFI（0x105）⇒ 合法 NOP：不 trap、指令流继续", "[riscv][trap][p6-4]") {
    const std::vector<uint32_t> image = make_image({
        {0x000u, kWfi},
        {0x001u, enc_csrrs(1, kCsrMepc)},
        {0x002u, enc_csrrs(2, kCsrMcause)},
        {0x003u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    require_writebacks(out, {
        {1, 0x0ull},   // 无 trap ⇒ mepc 保持复位值
        {2, 0x0ull},   // 无 trap ⇒ mcause 保持复位值
    });
    for (const Sample &s : out.samples) {
        CHECK(s.reason == 0u);
    }
    // 指令流确实前进了（WFI 退休后才有后续指令退休）
    CHECK(out.samples.back().retired > 0u);
}

// ===========================================================================
// P6-4 AC6：`MRET`（`imm = 0x302`）⇒ **仍 `reason = 1`（冻结）**（P6-5 前）
//
// 手推：MRET 的完整语义（`pc ← mepc` + `MIE/MPIE/MPP` 弹栈）归 **P6-5**
//       ⇒ ⇒ 本项**不得**提前把它当 NOP / trap ⇒ ⇒ 维持「已知未实现」= 冻结
//       ⇒ ⇒ 落点后的读回指令**永不执行** ⇒ 无写回 · 退休恒 0 · reason 恒 1
// ===========================================================================
TEST_CASE("TRAP - MRET（0x302）P6-5 前仍 reason=1 冻结", "[riscv][trap][p6-4]") {
    const std::vector<uint32_t> image = make_image({
        {0x000u, kMret},
        {kTrapAliasWord + 0u, enc_csrrs(1, kCsrMepc)},
        {kTrapAliasWord + 1u, enc_csrrs(2, kCsrMcause)},
        {kTrapAliasWord + 2u, riscv::JAL(0, 0)},
    });

    const RunOut out = run_program(image);

    REQUIRE(out.writebacks.empty());
    for (const Sample &s : out.samples) {
        CHECK(s.retired == 0u);
        CHECK(s.reason != 2u);
        CHECK(s.reason != 3u);
    }
    bool saw_reason1 = false;
    for (const Sample &s : out.samples) {
        if (s.reason == 1u) {
            saw_reason1 = true;
        }
    }
    CHECK(saw_reason1);
}
