/**
 * @file test_rv32i_csr_instructions.cpp
 * @brief **RV32I CSR 指令**（6 种 `funct3`）的流水线级判据
 *        （change `rv32i-csr-instructions-2026-11` · P6-2）
 *
 * ## DUT
 *
 * `src/rv32i_core_top.h`（核 IP 顶层）· 用**构造参数**装载 itcm / dtcm 字镜像
 * （`firmware-load-chain` L2 的路线 A ⇒ ⇒ 零 `prog_*` 写 ⇒ ⇒ 端口面零变化）。
 *
 * ## 观测面（**不新增端口**）
 *
 * | 端口 | 用途 |
 * |---|---|
 * | `dbg_wb_write_en` / `dbg_wb_rd_addr` / `dbg_wb_write_data` | 写回**事件**（单拍脉冲）⇒ 逐条写回的 `rd:data` 序列 |
 * | `dbg_id_instr` / `dbg_id_valid` | 取指链（首个取指 == 镜像首字） |
 * | `dbg_illegal_reason` | **ID 级**非法原因（`0` 合法 · `1` 已知未实现 · `3` 保留组合） |
 * | `obs_perf_instructions` | 退休计数（冻结 ⇒ 恒 0） |
 *
 * ## 期望值 MUST **手推**（禁抄仿真输出）
 *
 * 解释器与 Verilog codegen 由**同一份 C++** 生成 ⇒ ⇒ 同构缺陷**两后端一致地错**
 * ⇒ ⇒ 双后端 parity 对该族缺陷**结构性失明** ⇒ ⇒ 本文件的期望值全部由
 * RISC-V 规范 + `CsrBank` 的复位值（`mscratch` / `mip` = 0）**手推**。
 *
 * ## 判据 ↔ 验收条目
 *
 * | TEST_CASE | AC | 判别力（变异 ⇒ 必红的那一条） |
 * |---|---|---|
 * | 6 种 `funct3` 语义 + 读回 | AC1 / AC2 | 任一 `csr_wdata` 分支写错 |
 * | `CSRRC` 掩码 = `& ~x` | AC4 | 改成 `^ x` ⇒ `x4` / `x6` 读到错值 |
 * | 立即数型用 `zimm` | AC5 | 改成 rs1 的值 ⇒ `x3` / `x4` / `x6` 错值 |
 * | `rd=0` 不读但写 · `CSRRW` 源 0 仍写 | AC6 | 「源为 0 不写」⇒ `x3` 错值 |
 * | 相邻两条有序（读到新值） | AC3 | 去掉 `csr_stall` ⇒ `x3` 读到旧值 0 |
 * | `funct3=0` ⇒ reason 1 且冻结 | AC8 | 把 `OP_SYSTEM` 整体归合法 ⇒ reason 0 |
 * | `funct3=4` ⇒ reason 3 且冻结 | AC7 | 不区分 `funct3` ⇒ 拿到 1 |
 * | `mip` 读 0（`mtip`/`meip` tie 0 的功能面） | AC9 | 见注释（功能面 + 端口表面双证） |
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
// CSR 编码（**派生自 RISC-V 位布局** ⇒ ⇒ 不手写魔数）
// ---------------------------------------------------------------------------
constexpr uint32_t kOpSystem = 0x73u;
constexpr uint32_t kCsrMscratch = 0x340u;
constexpr uint32_t kCsrMip = 0x344u;

constexpr uint32_t kF3Csrrw = 1u;
constexpr uint32_t kF3Csrrs = 2u;
constexpr uint32_t kF3Csrrc = 3u;
constexpr uint32_t kF3Csrrwi = 5u;
constexpr uint32_t kF3Csrrsi = 6u;
constexpr uint32_t kF3Csrrci = 7u;

/// `csr` 放在 I 型的 `imm[11:0]` 字段（`instr[31:20]`）
constexpr uint32_t enc_csr(uint32_t rd, uint32_t f3, uint32_t csr, uint32_t rs1) {
    return riscv::enc_i(rd, rs1, f3, static_cast<int32_t>(csr), kOpSystem);
}

// ---------------------------------------------------------------------------
// 运行框架
// ---------------------------------------------------------------------------
struct RunOut {
    uint64_t retired = 0;
    std::vector<std::pair<uint32_t, uint64_t>> writebacks;  ///< 按**时间顺序**
    std::vector<uint32_t> fetched;
    std::vector<uint32_t> illegal_reasons;                  ///< 出现过的**非零**值（去重）
};

/// 镜像的**最小长度**（字）
///
/// ⚠️ **MUST ≥ 0x048**（2026-11-26 · P6-3）：trap 落点 = `mtvec` = `0x2000_0100`
///    ⇒ ⇒ 核顶层折成 **12 位字索引** ⇒ ⇒ 别名索引 = **`0x040`** = **字 64**
///    ⇒ ⇒ 镜像 MUST 覆盖它 ⇒ ⇒ 否则该字读到 **0**（= 又一条**未知 opcode**）
///    ⇒ ⇒ `illegal_reason = 2` 会**混入**本文件的非法原因判据 ⇒ ⇒ 判据失真。
constexpr std::size_t kImageWords = 0x048u;

/// trap 落点的**别名字索引**（`(mtvec >> 2) & 0xFFF` · `mtvec` = `0x2000_0100`）
constexpr std::size_t kTrapAliasWord = 0x040u;

/// 跑一个程序（`itcm` = 字镜像 · 尾部补 NOP 到 `kImageWords` 字 · `dtcm` 空）
///
/// ⚠️ **采样口径与 `tb_rv32i_soc.cpp` 逐字一致**：`sim.tick(1)` 之后取端口值。
RunOut run_program(const std::vector<uint32_t> &prog, std::size_t cycles = 160) {
    std::vector<uint32_t> itcm = prog;
    if (itcm.size() < kImageWords) {
        itcm.resize(kImageWords, riscv::NOP);
    }
    const std::vector<uint32_t> dtcm;

    using FwDev = ch::ch_device<Rv32iCoreTop, const std::vector<uint32_t> &,
                                const std::vector<uint32_t> &>;
    auto dev = std::make_unique<FwDev>(itcm, dtcm);
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    // ⚠️ 核级 TB 没有 MMIO 从设备 ⇒ ⇒ 8 根 AXI4-Lite 输入 MUST 显式 tie 0
    //    （未驱动端口读到的是**未定义值** ⇒ ⇒ 期望值会「碰巧」通过）。
    sim.set_input_value(io.axil_awready, 0u);
    sim.set_input_value(io.axil_wready, 0u);
    sim.set_input_value(io.axil_bresp, 0u);
    sim.set_input_value(io.axil_bvalid, 0u);
    sim.set_input_value(io.axil_arready, 0u);
    sim.set_input_value(io.axil_rdata, 0u);
    sim.set_input_value(io.axil_rresp, 0u);
    sim.set_input_value(io.axil_rvalid, 0u);

    RunOut out;
    for (std::size_t c = 0; c < cycles; ++c) {
        sim.tick(1);

        const uint64_t id_valid =
            static_cast<uint64_t>(sim.get_port_value(io.dbg_id_valid));
        const uint64_t wb_en =
            static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_write_en));
        const uint64_t wb_rd =
            static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_rd_addr));
        const uint64_t wb_data =
            static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_write_data));
        const uint64_t reason =
            static_cast<uint64_t>(sim.get_port_value(io.dbg_illegal_reason));

        if (id_valid != 0) {
            out.fetched.push_back(static_cast<uint32_t>(
                static_cast<uint64_t>(sim.get_port_value(io.dbg_id_instr))));
        }
        if (wb_en != 0) {
            out.writebacks.emplace_back(static_cast<uint32_t>(wb_rd), wb_data);
        }
        if (reason != 0) {
            bool seen = false;
            for (const uint32_t r : out.illegal_reasons) {
                if (r == static_cast<uint32_t>(reason)) {
                    seen = true;
                }
            }
            if (!seen) {
                out.illegal_reasons.push_back(static_cast<uint32_t>(reason));
            }
        }
        out.retired =
            static_cast<uint64_t>(sim.get_port_value(io.obs_perf_instructions));
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

} // namespace

// ===========================================================================
// AC1 / AC2：6 种 `funct3` 的 `csr_wdata` 语义 + 读回
//
// 手推（`mscratch` 复位 = 0）：
//   lui  x1, 0x12345        ⇒ x1 = 0x1234_5000
//   addi x1, x1, 0x678      ⇒ x1 = 0x1234_5678
//   csrrw  x2, mscratch, x1 ⇒ x2 = 0x0        · mscratch = 0x1234_5678
//   csrrs  x3, mscratch, x0 ⇒ x3 = 0x1234_5678（不写：rs1 = x0）
//   csrrc  x4, mscratch, x1 ⇒ x4 = 0x1234_5678 · mscratch = 0x1234_5678 & ~0x1234_5678 = 0
//   csrrwi x5, mscratch, 0x1f ⇒ x5 = 0x0      · mscratch = 0x1f
//   csrrsi x6, mscratch, 0x10 ⇒ x6 = 0x1f     · mscratch = 0x1f | 0x10 = 0x1f
//   csrrci x7, mscratch, 0x0f ⇒ x7 = 0x1f     · mscratch = 0x1f & ~0x0f = 0x10
//   csrrs  x8, mscratch, x0   ⇒ x8 = 0x10
// ===========================================================================
TEST_CASE("CSR - 6 种 funct3 的 wdata 语义与读回", "[riscv][csr]") {
    const std::vector<uint32_t> prog = {
        riscv::LUI(1, 0x12345),
        riscv::ADDI(1, 1, 0x678),
        enc_csr(2, kF3Csrrw, kCsrMscratch, 1),
        enc_csr(3, kF3Csrrs, kCsrMscratch, 0),
        enc_csr(4, kF3Csrrc, kCsrMscratch, 1),
        enc_csr(5, kF3Csrrwi, kCsrMscratch, 0x1f),
        enc_csr(6, kF3Csrrsi, kCsrMscratch, 0x10),
        enc_csr(7, kF3Csrrci, kCsrMscratch, 0x0f),
        enc_csr(8, kF3Csrrs, kCsrMscratch, 0),
        riscv::JAL(0, 0),
    };

    const RunOut out = run_program(prog);

    REQUIRE(!out.fetched.empty());
    CHECK(out.fetched[0] == prog[0]);          // 取指真的来自装载的镜像
    CHECK(out.illegal_reasons.empty());        // 合法 CSR 从不产生非零 reason
    CHECK(out.retired >= prog.size());         // 全部退休（不冻结）

    require_writebacks(out, {
        {1, 0x12345000ull},
        {1, 0x12345678ull},
        {2, 0x0ull},
        {3, 0x12345678ull},
        {4, 0x12345678ull},
        {5, 0x0ull},
        {6, 0x1full},
        {7, 0x1full},
        {8, 0x10ull},
    });
}

// ===========================================================================
// AC4：`CSRRC` / `CSRRCI` 的掩码 MUST 是「按位取反后 AND」
//
// 手推（`old` 与 `x` 的**判别位** = `old = 0` 且 `x = 1` 的位 ⇒ `& ~x` 得 0、`^ x` 得 1）：
//   addi   x1, x0, 0x0F       ⇒ x1 = 0x0F
//   csrrw  x0, mscratch, x1   ⇒ mscratch = 0x0F（rd=0 ⇒ 不读，仍写）
//   addi   x2, x0, 0xF0       ⇒ x2 = 0xF0
//   csrrc  x3, mscratch, x2   ⇒ x3 = 0x0F · mscratch = 0x0F & ~0xF0 = 0x0F
//   csrrs  x4, mscratch, x0   ⇒ x4 = 0x0F      （变异 `^ x` ⇒ 0xFF）
//   csrrci x5, mscratch, 0x10 ⇒ x5 = 0x0F · mscratch = 0x0F & ~0x10 = 0x0F
//   csrrs  x6, mscratch, x0   ⇒ x6 = 0x0F      （变异 `^ x` ⇒ 0x1F）
// ===========================================================================
TEST_CASE("CSR - CSRRC 掩码是 &~x 而不是 ^x", "[riscv][csr]") {
    const std::vector<uint32_t> prog = {
        riscv::ADDI(1, 0, 0x0F),
        enc_csr(0, kF3Csrrw, kCsrMscratch, 1),
        riscv::ADDI(2, 0, 0xF0),
        enc_csr(3, kF3Csrrc, kCsrMscratch, 2),
        enc_csr(4, kF3Csrrs, kCsrMscratch, 0),
        enc_csr(5, kF3Csrrci, kCsrMscratch, 0x10),
        enc_csr(6, kF3Csrrs, kCsrMscratch, 0),
        riscv::JAL(0, 0),
    };

    const RunOut out = run_program(prog);
    require_writebacks(out, {
        {1, 0x0Full},
        {2, 0xF0ull},
        {3, 0x0Full},
        {4, 0x0Full},
        {5, 0x0Full},
        {6, 0x0Full},
    });
}

// ===========================================================================
// AC5：立即数型 MUST 用 `zimm`（**不是** `rs1` 字段当寄存器号去读的值）
//
// 手推（`x5` = 诱饵 = 0x3F ⇒ 若误用 rs1 值，`csrrsi` 的源会变成 0x3F）：
//   addi   x5, x0, 0x3F       ⇒ x5 = 0x3F（诱饵）
//   csrrwi x0, mscratch, 0x10 ⇒ mscratch = 0x10（rd=0 ⇒ 无写回）
//   csrrsi x2, mscratch, 0x05 ⇒ x2 = 0x10 · mscratch = 0x10 | 0x05 = 0x15
//   csrrs  x3, mscratch, x0   ⇒ x3 = 0x15      （变异用 rs1 值 ⇒ 0x3F）
//   csrrwi x4, mscratch, 0x05 ⇒ x4 = 0x15 · mscratch = 0x05
//   csrrs  x6, mscratch, x0   ⇒ x6 = 0x05      （变异用 rs1 值 ⇒ 0x3F）
// ===========================================================================
TEST_CASE("CSR - 立即数型用 zimm 而不是 rs1 值", "[riscv][csr]") {
    const std::vector<uint32_t> prog = {
        riscv::ADDI(5, 0, 0x3F),
        enc_csr(0, kF3Csrrwi, kCsrMscratch, 0x10),
        enc_csr(2, kF3Csrrsi, kCsrMscratch, 0x05),
        enc_csr(3, kF3Csrrs, kCsrMscratch, 0),
        enc_csr(4, kF3Csrrwi, kCsrMscratch, 0x05),
        enc_csr(6, kF3Csrrs, kCsrMscratch, 0),
        riscv::JAL(0, 0),
    };

    const RunOut out = run_program(prog);
    require_writebacks(out, {
        {5, 0x3Full},
        {2, 0x10ull},
        {3, 0x15ull},
        {4, 0x15ull},
        {6, 0x05ull},
    });
    // ⚠️ `rd = 0` ⇒ 永不出现在写回序列里（`rd_not_zero` 门控）
    for (const auto &wb : out.writebacks) {
        CHECK(wb.first != 0u);
    }
}

// ===========================================================================
// AC6：`rd = 0` ⇒ **不读**（但 `CSRRW` 仍写）；`CSRRW` 的源为 0 ⇒ **仍写**
//
// 手推：
//   addi  x1, x0, 0x11       ⇒ x1 = 0x11
//   csrrw x0, mscratch, x1   ⇒ mscratch = 0x11（rd=0 ⇒ 无写回）
//   csrrs x2, mscratch, x0   ⇒ x2 = 0x11
//   csrrw x0, mscratch, x0   ⇒ mscratch = 0x0（**源 = 0 仍 MUST 写**）
//   csrrs x3, mscratch, x0   ⇒ x3 = 0x0       （变异「源 0 不写」⇒ 0x11）
//   addi  x4, x0, 0x22       ⇒ x4 = 0x22
//   csrrw x0, mscratch, x4   ⇒ mscratch = 0x22
//   csrrs x5, mscratch, x0   ⇒ x5 = 0x22
// ===========================================================================
TEST_CASE("CSR - rd=0 不读但写 · CSRRW 源为 0 仍写", "[riscv][csr]") {
    const std::vector<uint32_t> prog = {
        riscv::ADDI(1, 0, 0x11),
        enc_csr(0, kF3Csrrw, kCsrMscratch, 1),
        enc_csr(2, kF3Csrrs, kCsrMscratch, 0),
        enc_csr(0, kF3Csrrw, kCsrMscratch, 0),
        enc_csr(3, kF3Csrrs, kCsrMscratch, 0),
        riscv::ADDI(4, 0, 0x22),
        enc_csr(0, kF3Csrrw, kCsrMscratch, 4),
        enc_csr(5, kF3Csrrs, kCsrMscratch, 0),
        riscv::JAL(0, 0),
    };

    const RunOut out = run_program(prog);
    require_writebacks(out, {
        {1, 0x11ull},
        {2, 0x11ull},
        {3, 0x0ull},
        {4, 0x22ull},
        {5, 0x22ull},
    });
    for (const auto &wb : out.writebacks) {
        CHECK(wb.first != 0u);
    }
}

// ===========================================================================
// AC3：相邻两条 CSR MUST 有序（后者读到**新值**）
//
// 手推：
//   addi  x1, x0, 0x2A       ⇒ x1 = 0x2A
//   csrrw x2, mscratch, x1   ⇒ x2 = 0x0（旧值）· mscratch = 0x2A
//   csrrs x3, mscratch, x0   ⇒ x3 = 0x2A（**新值** ⇒ 变异去掉 `csr_stall` ⇒ 0x0）
//
// ⚠️ 为什么必须停 1 拍：`CsrBank` 只有**一个**地址口 ⇒ ⇒ 读者在 ID 的那一拍，
//    写者的写**尚未落在沿上** ⇒ ⇒ 直接锁存会读到旧值（违反 RISC-V 的 CSR 顺序要求）。
// ===========================================================================
TEST_CASE("CSR - 相邻两条 MUST 有序（读到新值）", "[riscv][csr]") {
    const std::vector<uint32_t> prog = {
        riscv::ADDI(1, 0, 0x2A),
        enc_csr(2, kF3Csrrw, kCsrMscratch, 1),
        enc_csr(3, kF3Csrrs, kCsrMscratch, 0),
        riscv::JAL(0, 0),
    };

    const RunOut out = run_program(prog);
    require_writebacks(out, {
        {1, 0x2Aull},
        {2, 0x0ull},
        {3, 0x2Aull},
    });
}

// ===========================================================================
// AC8：`funct3 = 0`（ECALL）⇒ `illegal_reason == 1` 且**冻结**
//
// ⚠️ 判别力：把 `OP_SYSTEM` 整体归「合法」⇒ reason 0 ⇒ 会当 NOP 退休
//    ⇒ ⇒ 那比「冻结」**更糟**（错误被静默吞掉）。
// ===========================================================================
TEST_CASE("CSR - funct3=0（ECALL）⇒ reason 1 且冻结", "[riscv][csr][illegal]") {
    const uint32_t ecall = enc_csr(0, 0, 0, 0);
    const RunOut out = run_program({ecall}, 60);

    REQUIRE(!out.fetched.empty());
    CHECK(out.fetched[0] == ecall);
    INFO("illegal_reasons = " << out.illegal_reasons.size());
    REQUIRE(out.illegal_reasons.size() == 1u);
    CHECK(out.illegal_reasons[0] == 1u);   // 已知未实现（P6-4 / P6-5）
    CHECK(out.retired == 0u);              // 冻结 ⇒ 永不退休
}

// ===========================================================================
// AC7：`funct3 = 4`（ISA 保留组合）⇒ `illegal_reason == 3`
//
// ⚠️ 判别力：原实现把整个 `OP_SYSTEM` 归 `1` ⇒ 保留组合**拿不到** `3`
//    （T0.3 的实测：`retired=0` 但 reason 无法区分 1 / 3）。
//
// ⚠️ **2026-11-26 · P6-3 语义变更**：触发集 = `reason ∈ {2,3}` ⇒ ⇒ `reason = 3`
//    **不再冻结** ⇒ ⇒ **陷入**（PC 重定向到 `mtvec`）⇒ ⇒ 「不退休」这条
//    **不能再用 `retired == 0` 断言**（陷入后的落点会继续退休）。
//    ⇒ ⇒ 本文件只保留**译码级**的判别力（`reason == 3` 可观测）；
//    ⇒ ⇒ 「陷入」的判据（`mepc` / `mcause` / 重定向）归
//    **`test_rv32i_trap_entry.cpp`**（P6-3 的**归属文件**）。
// ===========================================================================
TEST_CASE("CSR - funct3=4（保留组合）⇒ reason 3", "[riscv][csr][illegal]") {
    const uint32_t reserved = enc_csr(0, 4, kCsrMscratch, 0);
    // ⚠️ 镜像 MUST 在**别名落点** `0x040` 放**自环**（`jal x0, 0`）：
    //    P6-3 起 `reason = 3` ⇒ **陷入** ⇒ PC 重定向到 `mtvec` 的别名落点
    //    ⇒ ⇒ 若那里是 NOP ⇒ NOP 会**跑出镜像末尾** ⇒ ⇒ 读到 0（= 未知 opcode）
    //    ⇒ ⇒ `reason = 2` 会**混入**本用例的非法原因判据 ⇒ ⇒ 判据失真。
    std::vector<uint32_t> prog(kImageWords, riscv::NOP);
    prog[0] = reserved;
    prog[kTrapAliasWord] = riscv::JAL(0, 0);
    const RunOut out = run_program(prog, 60);

    REQUIRE(!out.fetched.empty());
    CHECK(out.fetched[0] == reserved);
    std::string rs;
    for (std::size_t i = 0; i < out.illegal_reasons.size(); ++i) {
        rs += std::to_string(out.illegal_reasons[i]);
        rs += ",";
    }
    INFO("illegal_reasons = " << rs);
    REQUIRE(out.illegal_reasons.size() == 1u);
    CHECK(out.illegal_reasons[0] == 3u);   // 保留组合
}

// ===========================================================================
// AC9：`mip` 读 0（`mtip` / `meip` 显式 tie 0 的**功能面**）
//
// ⚠️ **判别力的边界（如实登记）**：`mtip` / `meip` 若**未驱动**，本解释器
//    读到的是 0（= 与 tie 0 **同值**）⇒ ⇒ **本条对「漏驱动」结构性失明**。
//    ⇒ ⇒ 「漏驱动」的判别在**端口表面**：无 driver 的 `ch_in` 会成为产物顶层
//    `input`（`openspec/specs/rtl-hierarchical-output/spec.md` 的端口表契约）
//    ⇒ ⇒ 由 `scripts/verify_riscv_soc_core_ports.sh`（53 端口逐字比对）兜住
//    ⇒ ⇒ 本条只证明**功能语义**（无中断源 ⇒ `mip` 恒 0）。
// ===========================================================================
TEST_CASE("CSR - mip 读 0（mtip/meip tie 0 的功能面）", "[riscv][csr]") {
    const std::vector<uint32_t> prog = {
        enc_csr(1, kF3Csrrs, kCsrMip, 0),   // csrrs x1, mip, x0
        riscv::JAL(0, 0),
    };

    const RunOut out = run_program(prog);
    require_writebacks(out, {{1, 0x0ull}});
}
