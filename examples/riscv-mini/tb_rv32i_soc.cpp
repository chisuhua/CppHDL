// examples/riscv-mini/tb_rv32i_soc.cpp
//
// RV32I SoC —— **激励驱动**（三角色分离 · `example-file-boundary-2026-11`）
//
// ## 三角色（与 `examples/axi4` / `examples/stream` 同型）
//
// | 角色 | 载体 | 说明 |
// |---|---|---|
// | 设计定义 | `src/rv32i_pipeline_top.h` | `Rv32iPipelineTop` · **无 `main`** |
// | **激励驱动** | **本文件** | `main` · 装载程序 → 驱动 → 断言 → 结论行 |
// | 激励源 | `tb_rv32i_soc.sv` | **手写** · Verilog 侧独立 TB |
//
// ## 一种**验证模式** + 三种**发射形态**（与 `tb_*.cpp` 同型）
//
// · **默认**（无参）= **验证模式**：解释器路径跑 7 字小程序 ⇒ 24 条手推判据
//   ⇒ 打印 `RESULT: checks=24 fails=0`（协议行 · 十进制 · 单行 · 无 ANSI）
// · **`--emit-rtl --out <dir>`** = **扁平** RTL 产物
//   ⇒ ⇒ 与被取代的 `emit_rtl.cpp` **逐字节同构**（同一 factory + 同一库层 CLI）
// · **`--emit-rtl --hierarchical --out <dir>`** = **层次化**产物
//   ⇒ ⇒ `--hierarchical` 是**库层 CLI 的既有选项**（`rtl_emit::CliOptions::hierarchical`）
//   ⇒ ⇒ 与被取代的 `emit_rtl_hier.cpp` **逐字节同构**
// · **`--emit-rtl --firmware-init --out <dir>`** = **带固件初值**的扁平产物
//   ⇒ ⇒ 与被取代的 `emit_rtl_fw_init.cpp` **逐字节同构**（同一 factory + 同一组
//   `init_data`）⇒ ⇒ TB 侧只换**设计工厂**，发射流程仍走库层 CLI
//
// ## 期望值出处
//
// `.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc`
// ⇒ ⇒ **手推**（RV32I ISA 语义 + `if_stage.h` 的 `pc_reg` 复位值）
// ⇒ ⇒ **禁**从仿真输出转录（那会把实现的缺陷固化成期望）。
//
// ## ⚠️ 为什么 C++ 侧能覆盖 `.sv` 侧的**全部** 24 条判据
//
// `.sv` TB MUST 保持手写（`.verif` 的**封闭原语集**表达不了 4 个形态 ⇒
// ADR-076 复核（2026-11-21）已判**终态** ⇒ ⇒ 不得迁 `.verif`）。
// ⇒ ⇒ **但本文件是普通 C++**（无封闭原语集）⇒ ⇒ 那 4 个形态在此**全部可表达**：
// 首事件捕获 / 窗口内集合成员 / 序列相邻差 / 跨拍单调不减。
// ⇒ ⇒ C++ 侧与 `.sv` 侧**同 24 条判据**、**实现独立**（解释器路径 vs Verilog 路径）
// ⇒ ⇒ 两侧互为**跨后端佐证**（期望值同源于上面的登记文档）。
//
// ## ⚠️ 硬件值 MUST NOT 参与 C++ 控制流（根 `AGENTS.md` §ANTI-PATTERNS）
//
// 本文件**只读**端口值（`sim.get_port_value`）⇒ ⇒ 判据是**运行期**观测，
// 不参与**建图期**求值 ⇒ ⇒ 与该禁令无冲突（禁的是 `describe()` 里的 `if (io().x)`）。

#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "rv32i_pipeline_top.h"

#include "cpu/riscv/rv32i_asm_encode.h"
#include "rtl_emit/cli.h"
#include "rv32i_fw_demo.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using riscv::ADD;
using riscv::ADDI;
using riscv::JAL;
using riscv::NOP;
using riscv::SW;

// ---------------------------------------------------------------------------
// 7 字小程序（**手推** · 编码推导见 expectations 文档的「小程序」表）
//
// | 字 | 指令 | 语义 |
// |---|---|---|
// | 0 | `addi x1, x0, 42` | x1 = 42 |
// | 1 | `nop` | 等 x1 写回 |
// | 2 | `addi x2, x0, 11` | x2 = 11 |
// | 3 | `nop` | 等 x2 写回 |
// | 4 | `add x3, x1, x2` | x3 = 42 + 11 = **53** |
// | 5 | `sw x3, 0(x0)` | [字节地址 0] = **53** |
// | 6 | `jal x0, 0` | 自环（停机） |
//
// ⚠️ 字 1 / 字 3 之间**无数据冒险**：本流水线**已接**数据前推
// （`include/chlib/forwarding.h` · ADR-047 D2）⇒ ⇒ `nop` 只是留余量。
// ---------------------------------------------------------------------------

constexpr std::size_t kProgWords = 7;

/// 字 7..63 填 **NOP**（**不是 0**）—— G1 纪律（2026-11-15 · B4）：
/// I-TCM 在程序末尾之后读到的是 `ch_mem` 初值 **0** ⇒ ⇒ `0x00000000` 的
/// opcode = `0x00` ⇒ ⇒ **未知 opcode** ⇒ ⇒ 流水线会在此冻结 ⇒ ⇒ 必填 NOP。
constexpr std::size_t kFillWords = 64;

/// 跑多少拍（与 `.sv` TB 的 `repeat (60) @(posedge clk)` 一致）。
constexpr std::size_t kCycles = 60;

/// 取指 pc 的基址（`if_stage.h` 的 `pc_reg` 复位值 = `0x80000000`）。
constexpr uint32_t kPcBase = 0x80000000u;

/// **手算**编码常量（**独立于**库编码器的交叉校验证据 —— 仓内 `rv32i_asm_encode.h`
/// 明文要求：「若只用本头的 helper 去断言本头的 helper ⇒ ⇒ 自证 ⇒ ⇒ 零判别力」）。
constexpr uint32_t kHandEncoded[kProgWords] = {
    0x02A00093u, // 0  addi x1, x0, 42   imm=0x02A · rs1=0 · funct3=000 · rd=1 · op=0010011
    0x00000013u, // 1  nop               imm=0 · rs1=0 · funct3=000 · rd=0 · op=0010011
    0x00B00113u, // 2  addi x2, x0, 11   imm=0x00B · rs1=0 · funct3=000 · rd=2 · op=0010011
    0x00000013u, // 3  nop               同字 1
    0x002081B3u, // 4  add  x3, x1, x2    funct7=0 · rs2=2 · rs1=1 · funct3=000 · rd=3 · op=0110011
    0x00302023u, // 5  sw   x3, 0(x0)     imm=0 · rs2=3 · rs1=0 · funct3=010 · imm[4:0]=0 · op=0100011
    0x0000006Fu, // 6  jal  x0, 0         imm=0 · rd=0 · op=1101111
};

/// 程序字（取自**库**编码器 `riscv::ADDI/ADD/SW/JAL` —— 全仓**唯一**一份编码器）。
constexpr uint32_t kProgram[kProgWords] = {
    ADDI(1, 0, 42), NOP, ADDI(2, 0, 11), NOP, ADD(3, 1, 2), SW(3, 0, 0), JAL(0, 0),
};

// ⚠️ **手算常量 × 库编码器**的交叉校验（编译期 ⇒ 零运行期成本 ⇒ 恒生效）。
static_assert(kProgram[0] == kHandEncoded[0], "字 0 编码与手算常量不符");
static_assert(kProgram[1] == kHandEncoded[1], "字 1 编码与手算常量不符");
static_assert(kProgram[2] == kHandEncoded[2], "字 2 编码与手算常量不符");
static_assert(kProgram[3] == kHandEncoded[3], "字 3 编码与手算常量不符");
static_assert(kProgram[4] == kHandEncoded[4], "字 4 编码与手算常量不符");
static_assert(kProgram[5] == kHandEncoded[5], "字 5 编码与手算常量不符");
static_assert(kProgram[6] == kHandEncoded[6], "字 6 编码与手算常量不符");

/// 写回期望值（ISA 语义手推）：`addi x1,42` · `addi x2,11` · `add x3,x1,x2` = 53。
struct WritebackExpect {
    uint32_t rd;
    uint64_t data;
};
constexpr WritebackExpect kWritebacks[3] = {
    {1, 42},
    {2, 11},
    {3, 53},
};

/// store 期望值（ISA 语义手推）：`sw x3, 0(x0)` ⇒ 字节地址 0 · 数据 53。
constexpr uint32_t kStoreAddr = 0u;
constexpr uint64_t kStoreData = 53u;

/// 判据计数器（协议：失败进 stderr · 结论行进 stdout）。
class Checker {
public:
    void check(bool ok, const std::string &what) {
        ++checks_;
        if (!ok) {
            ++fails_;
            std::cerr << "CHECK FAIL: " << what << "\n";
        }
    }
    std::size_t checks() const { return checks_; }
    std::size_t fails() const { return fails_; }

private:
    std::size_t checks_ = 0;
    std::size_t fails_ = 0;
};

/// 发射模式的 design factory（与被取代的 `emit_rtl.cpp` **同构**）。
std::unique_ptr<ch::Component> make_emit_design(ch::core::context *) {
    return std::make_unique<Rv32iPipelineTop>(nullptr, "top");
}

/// 带固件初值的 design factory（与被取代的 `emit_rtl_fw_init.cpp` **同构**）。
std::unique_ptr<ch::Component> make_emit_design_fw_init(ch::core::context *) {
    // ⚠️ **固件不进端口**：走构造参数 ⇒ ⇒ `init_data` ⇒ ⇒ 产物自带 `initial`
    // ⇒ ⇒ **零端口变更**（顶层端口表与基线产物逐字相同）。
    return std::make_unique<Rv32iPipelineTop>(nullptr, "top", rv32i_fw_demo::text(),
                                              rv32i_fw_demo::data());
}

// ---------------------------------------------------------------------------
// 验证模式（解释器路径）
// ---------------------------------------------------------------------------

int run_verification() {
    Checker ck;

    auto dev = std::make_unique<ch::ch_device<Rv32iPipelineTop>>();
    Simulator sim(dev->context());
    auto &dut = dev->instance();
    auto &io = dut.io();

    // ── 装载程序（装载期 `ext_rst` 保持 1 ⇒ 性能计数器不被装载污染）──
    sim.set_input_value(io.ext_rst, 1u);
    sim.set_input_value(io.prog_we, 1u);
    for (std::size_t i = 0; i < kProgWords; ++i) {
        sim.set_input_value(io.prog_addr, static_cast<uint64_t>(i));
        sim.set_input_value(io.prog_data, kProgram[i]);
        sim.tick(1);
    }
    for (std::size_t i = kProgWords; i < kFillWords; ++i) {
        sim.set_input_value(io.prog_addr, static_cast<uint64_t>(i));
        sim.set_input_value(io.prog_data, NOP);
        sim.tick(1);
    }
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0u);
    sim.set_input_value(io.prog_data, 0u);

    // ── 释放复位（框架复位由 `ch_reg` 的 INIT 承担；`ext_rst` 是 SoC 自有的复位）──
    sim.set_input_value(io.ext_rst, 0u);

    // ── 逐拍观测 ──
    std::vector<uint32_t> fetch_pcs;              // 有效取指的 pc 序列（上限同 `.sv` TB）
    std::vector<std::pair<uint32_t, uint32_t>> writebacks; // (rd, data)
    std::vector<std::pair<uint64_t, uint64_t>> stores;     // (addr, data)
    bool prog_word_seen[kProgWords] = {};
    bool perf_decreased = false;
    uint64_t last_perf = 0;
    bool perf_sampled = false;

    for (std::size_t c = 0; c < kCycles; ++c) {
        sim.tick(1);

        const uint64_t id_valid = static_cast<uint64_t>(sim.get_port_value(io.dbg_id_valid));
        const uint64_t id_pc = static_cast<uint64_t>(sim.get_port_value(io.dbg_id_pc));
        const uint64_t id_instr = static_cast<uint64_t>(sim.get_port_value(io.dbg_id_instr));
        const uint64_t wb_en = static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_write_en));
        const uint64_t wb_rd = static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_rd_addr));
        const uint64_t wb_data = static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_write_data));
        const uint64_t st_en = static_cast<uint64_t>(sim.get_port_value(io.obs_data_write_en));
        const uint64_t st_addr = static_cast<uint64_t>(sim.get_port_value(io.obs_data_addr));
        const uint64_t st_data = static_cast<uint64_t>(sim.get_port_value(io.obs_data_write_data));
        const uint64_t perf = static_cast<uint64_t>(sim.get_port_value(io.obs_perf_instructions));

        if (id_valid != 0) {
            if (fetch_pcs.size() < 16) {
                fetch_pcs.push_back(static_cast<uint32_t>(id_pc));
            }
            // S2：按 **pc + 指令字** 配对定位「某个程序字被取到」
            // ⇒ ⇒ **不用**指令值单判（NOP 别名 ⇒ 字 1 / 字 3 / 字 7..63 全是 0x00000013
            //      ⇒ 单判会把「取到字 7」误判成「取到字 1」⇒ ⇒ 判据被放宽）。
            for (std::size_t k = 0; k < kProgWords; ++k) {
                if (id_pc == static_cast<uint64_t>(kPcBase) + 4u * static_cast<uint64_t>(k) &&
                    id_instr == static_cast<uint64_t>(kProgram[k])) {
                    prog_word_seen[k] = true;
                }
            }
        }
        if (wb_en != 0) {
            writebacks.emplace_back(static_cast<uint32_t>(wb_rd), wb_data);
        }
        if (st_en != 0) {
            stores.emplace_back(st_addr, st_data);
        }
        if (perf_sampled && perf < last_perf) {
            perf_decreased = true;
        }
        last_perf = perf;
        perf_sampled = true;
    }

    // ── S1：复位后第一次取指的 pc = 0x80000000（spec-derived）──
    ck.check(!fetch_pcs.empty(), "S1a: 复位后至少取到一条指令");
    ck.check(!fetch_pcs.empty() && fetch_pcs[0] == kPcBase,
             "S1b: 首次取指 pc == 0x80000000");

    // ── S2：7 个程序字**每个**都被取到（窗口内集合成员）──
    for (std::size_t k = 0; k < kProgWords; ++k) {
        ck.check(prog_word_seen[k],
                 "S2: 程序字 " + std::to_string(k) + " 被取到");
    }

    // ── S3：取指 pc 按 +4 递增（前 6 段；字 6 是 `jal` ⇒ 会跳转 ⇒ 不再 +4）──
    for (std::size_t i = 1; i < kProgWords; ++i) {
        const bool ok = i < fetch_pcs.size() &&
                        (fetch_pcs[i] - fetch_pcs[i - 1]) == 4u;
        ck.check(ok, "S3: 取指 pc 第 " + std::to_string(i) + " 段递增 +4");
    }

    // ── S4：写回 x1=42 / x2=11 / x3=53（rd 与 data 各判一次）──
    for (const auto &wb : kWritebacks) {
        bool rd_seen = false;
        bool rd_data_seen = false;
        for (const auto &ev : writebacks) {
            if (ev.first != wb.rd) continue;
            rd_seen = true;
            if (ev.second == wb.data) rd_data_seen = true;
        }
        ck.check(rd_seen, "S4: 写回 rd=x" + std::to_string(wb.rd));
        ck.check(rd_data_seen,
                 "S4: 写回 x" + std::to_string(wb.rd) + " 的数据 == " +
                     std::to_string(wb.data));
    }

    // ── S5：写存储 数据=53 且 地址=0（`sw x3, 0(x0)`）──
    bool store_ok = false;
    for (const auto &ev : stores) {
        if (ev.first == kStoreAddr && ev.second == kStoreData) store_ok = true;
    }
    ck.check(store_ok, "S5: 写存储 addr=0 且 data=53");

    // ── S6：退休计数单调不减 且 > 0（impl-contract · **不锁死具体值**）──
    ck.check(!perf_decreased, "S6a: 退休计数单调不减");
    ck.check(last_perf > 0u, "S6b: 退休计数 > 0");

    // ── 结论行（协议：十进制 · 单行 · 无 ANSI）──
    std::cout << "RESULT: checks=" << ck.checks() << " fails=" << ck.fails() << "\n";
    return ck.fails() == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char **argv) {
    std::vector<std::string> rest;
    rest.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        rest.emplace_back(argv[i]);
    }

    // ── 模式判定：**位置无关**剥离模式标记（其余 argv 原序保留）──
    // ⚠️ `--hierarchical` **不剥离** ⇒ ⇒ 它是**库层 CLI 的既有选项**
    //    （`rtl_emit::CliOptions::hierarchical`）⇒ ⇒ 透传给 `run_emit_cli`。
    std::vector<std::string> mode_flags;
    rest = ch::rtl_emit::strip_mode_flags(rest, {"--emit-rtl", "--firmware-init"},
                                          &mode_flags);

    const bool want_emit =
        std::find(mode_flags.begin(), mode_flags.end(), "--emit-rtl") != mode_flags.end();
    const bool want_fw_init =
        std::find(mode_flags.begin(), mode_flags.end(), "--firmware-init") != mode_flags.end();

    if (want_emit || want_fw_init) {
        // ── 发射模式 fail-closed：`--out` 缺失 / 取值形态非法 ⇒ exit 2 ──
        // ⚠️ `--firmware-init` 也是**进入发射模式**的标记（它只改设计工厂 ⇒
        //   单独出现时**不得**被静默忽略）⇒ ⇒ 两条标记共用同一套 `--out` 校验。
        std::string error;
        if (!ch::rtl_emit::require_out_dir(rest, nullptr, &error)) {
            std::cerr << "FATAL: 发射模式（--emit-rtl / --firmware-init）" << error
                      << "（拒绝回落到默认 rtl_out/）\n";
            return 2;
        }
        return ch::rtl_emit::run_emit_cli(
            rest, want_fw_init ? make_emit_design_fw_init : make_emit_design);
    }

    // ── 验证模式 fail-closed：收到**任何**未知参数 ⇒ exit 2 ──
    if (!rest.empty()) {
        std::cerr << "FATAL: 未知参数（默认模式只接受无参）：" << rest[0] << "\n";
        return 2;
    }

    return run_verification();
}
