// examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp
//
// RV32I **SoC 级**双路 trace parity（interpreter + verilator）。
//
// Change: rv32i-pipeline-commit-path-2026-10（C3 的前置）
// Capability: openspec/specs/rv32i-commit-path/spec.md
//            （+ openspec/specs/verilator-rv32i-e2e/spec.md 的 MODIFIED）
// DUT:        Rv32iPipelineTop（examples/riscv-mini/src/rv32i_pipeline_top.h）
//
// ⚠️ DUT 在 **example 侧** ⇒ 本测试 MUST 住在 example 侧：根 `tests/**` 不许
//    include example 路径（护栏 C3 / ADR-046「库测试 MUST NOT 经 include 路径
//    依赖 example 源码」）。
//
// ---------------------------------------------------------------------------
// 三条方法学约束（都来自实测，不是偏好）
// ---------------------------------------------------------------------------
//
// 1. **不能用 `DUAL_BACKEND_TEST_PAIR` 做逐周期 diff** —— 该宏的语义是
//    「同一 body 各跑一遍」，每个 run 只看得见自己的后端。跨后端逐周期比对
//    必须「同一 TEST_CASE 内 2×ch_device + 2×Simulator」
//    （先例：tests/test_verilator_complex_e2e.cpp 的 SpiFsm 用例）。
//
// 2. **观测点全走顶层 ch_out** —— VerilatorBackend 只对 `port_access_` 里的
//    顶层端口建访问表，读不到设计内部。⇒ SoC 顶层暴露 32 个端口
//    （5 输入 + 27 输出 = 11 功能观测 + 16 诊断观测），本测试比对**全部 27 个输出**。
//
// 3. **固件必须无数据冒险** —— lib 版 `HazardUnit` 不接数据前推
//    （include/cpu/pipeline/hazard_unit.h: `io().rs1_data = io().rs1_data_raw`），
//    接上会在真实装配里形成组合环（ADR-046 D3 #14；D5 对此仍悬置未裁决）。
//    ⇒ 生产者→消费者之间插 3 条 NOP。
//    **不要**通过「把前推接上」来修这个 —— 那是 D5 的裁决对象。
//
// ---------------------------------------------------------------------------
// 这个测试的由来：它第一次让 SoC 顶层**可观测**，并因此照出了 17 个缺陷
// （见 .rddf/improvements/rv32i-pipeline-commit-path-2026-10.md §2）。
// 断言分三层：
//   ① 编码器 × 手算常量交叉校验（防自证）；
//   ② 逐周期双后端全向量一致（27 个输出观测值，**无掩码**）；
//   ③ ISA 语义 golden（3 个 store 的地址与数据 + 分支/停顿计数 + 退休数）。
//      ③ 是关键：② 只证明「两个后端一样」，③ 才证明「两个后端都对」。
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/eval_backend.h"
#include "core/interpreter_backend.h"
#include "core/verilator_backend.h"
#include "cpu/pipeline/rv32i_pipeline.h"
#include "cpu/pipeline/rv32i_tcm.h"
#include "rv32i_pipeline_top.h"
#include "simulator.h"

namespace {

// ---------------------------------------------------------------------------
// 观测向量：顺序必须与 `obs_snapshot()` 一致，改动两处必须同步。
// 全部 <= 64 bit（ADR-035 R3 verilator 端口上限）。
// ---------------------------------------------------------------------------
constexpr const char *kObsNames[] = {
    // —— 功能观测（11）——
    "obs_instr_addr",         "obs_data_addr",
    "obs_data_write_data",    "obs_data_write_en",
    "obs_perf_cycles",        "obs_perf_instructions",
    "obs_perf_branch_count",  "obs_perf_branch_mispredict",
    "obs_perf_load_use_stalls", "obs_perf_control_stalls",
    "obs_data_rdata",
    // —— 诊断观测（16）：指令流 / 有效链 / 写回 / 冒险脉冲 ——
    "dbg_id_instr",           "dbg_id_pc",
    "dbg_id_valid",           "dbg_id_is_branch",
    "dbg_id_is_load",         "dbg_id_is_store",
    "dbg_branch_taken",       "dbg_wb_write_en",
    "dbg_wb_write_data",      "dbg_wb_rd_addr",
    "dbg_exmem_alu_result",   "dbg_exmem_imm",
    "dbg_exmem_rs2_data",
    // 有效链的逐级观测（契约 R1「有效链 MUST 逐级传递」）
    "dbg_exmem_valid",        "dbg_mem_valid_out",
    // load-use 冒险检测脉冲（与「停顿拍数」区分）
    "dbg_load_use_detected"};
constexpr std::size_t kObsCount = sizeof(kObsNames) / sizeof(kObsNames[0]);
// 32 个端口里 5 个是输入（ext_clk / ext_rst / prog_*）⇒ 27 个输出被观测
static_assert(kObsCount == 27, "观测向量长度必须与 SoC 顶层的**输出**端口数一致");

// 观测向量的**编译期**下标查找（避免手抄下标 ⇒ 下标漂移会静默读错字段；
// 名字写错 ⇒ 编译期失败而不是读到别的字段）
constexpr std::size_t obs_idx(const char *name) {
    for (std::size_t i = 0; i < kObsCount; ++i) {
        const char *a = kObsNames[i];
        const char *b = name;
        while (*a != '\0' && *a == *b) {
            ++a;
            ++b;
        }
        if (*a == '\0' && *b == '\0') {
            return i;
        }
    }
    return kObsCount;  // 未找到 ⇒ 调用点的 static_assert 会失败
}
constexpr std::size_t kIdxInstructions = obs_idx("obs_perf_instructions");
constexpr std::size_t kIdxBranchCount = obs_idx("obs_perf_branch_count");
constexpr std::size_t kIdxMispredict = obs_idx("obs_perf_branch_mispredict");
constexpr std::size_t kIdxLoadUseStalls = obs_idx("obs_perf_load_use_stalls");
constexpr std::size_t kIdxControlStalls = obs_idx("obs_perf_control_stalls");
constexpr std::size_t kIdxLoadUseDetected = obs_idx("dbg_load_use_detected");
constexpr std::size_t kIdxDataWriteEn = obs_idx("obs_data_write_en");
constexpr std::size_t kIdxDataAddr = obs_idx("obs_data_addr");
constexpr std::size_t kIdxDataWriteData = obs_idx("obs_data_write_data");
constexpr std::size_t kIdxIdPc = obs_idx("dbg_id_pc");
constexpr std::size_t kIdxIdInstr = obs_idx("dbg_id_instr");
constexpr std::size_t kIdxWbWriteEn = obs_idx("dbg_wb_write_en");
constexpr std::size_t kIdxWbWriteData = obs_idx("dbg_wb_write_data");
constexpr std::size_t kIdxWbRdAddr = obs_idx("dbg_wb_rd_addr");
static_assert(kIdxInstructions != kObsCount, "obs_perf_instructions 未在 kObsNames 中");
static_assert(kIdxBranchCount != kObsCount, "obs_perf_branch_count 未在 kObsNames 中");
static_assert(kIdxMispredict != kObsCount, "obs_perf_branch_mispredict 未在 kObsNames 中");
static_assert(kIdxLoadUseStalls != kObsCount, "obs_perf_load_use_stalls 未在 kObsNames 中");
static_assert(kIdxControlStalls != kObsCount, "obs_perf_control_stalls 未在 kObsNames 中");
static_assert(kIdxLoadUseDetected != kObsCount, "dbg_load_use_detected 未在 kObsNames 中");
static_assert(kIdxDataWriteEn != kObsCount, "obs_data_write_en 未在 kObsNames 中");
static_assert(kIdxDataAddr != kObsCount, "obs_data_addr 未在 kObsNames 中");
static_assert(kIdxDataWriteData != kObsCount, "obs_data_write_data 未在 kObsNames 中");
static_assert(kIdxIdPc != kObsCount, "dbg_id_pc 未在 kObsNames 中");
static_assert(kIdxIdInstr != kObsCount, "dbg_id_instr 未在 kObsNames 中");
static_assert(kIdxWbWriteEn != kObsCount, "dbg_wb_write_en 未在 kObsNames 中");
static_assert(kIdxWbWriteData != kObsCount, "dbg_wb_write_data 未在 kObsNames 中");
static_assert(kIdxWbRdAddr != kObsCount, "dbg_wb_rd_addr 未在 kObsNames 中");

bool tool_available(const char *cmd) {
    std::string which_cmd = std::string("command -v ") + cmd + " >/dev/null 2>&1";
    return std::system(which_cmd.c_str()) == 0;
}

std::string make_temp_dir(const std::string &suffix) {
    char tmpl[] = "/tmp/cpphdl_vl_soc_XXXXXX";
    char *dir = ::mkdtemp(tmpl);
    if (!dir) return {};
    std::string result(dir);
    result += suffix;
    ::mkdir(result.c_str(), 0755);
    return result;
}

uint64_t read_port_output(Simulator &sim, ch::core::lnodeimpl *ln) {
    auto it = sim.data_map().find(ln->id());
    if (it == sim.data_map().end()) return UINT64_MAX;
    return static_cast<uint64_t>(it->second);
}

// 顶层 27 个输出观测端口的当前值（顺序 == kObsNames）
std::array<uint64_t, kObsCount> obs_snapshot(Rv32iPipelineTop &dut,
                                             Simulator &sim) {
    auto &io = dut.io();
    return {read_port_output(sim, io.obs_instr_addr.impl()),
            read_port_output(sim, io.obs_data_addr.impl()),
            read_port_output(sim, io.obs_data_write_data.impl()),
            read_port_output(sim, io.obs_data_write_en.impl()),
            read_port_output(sim, io.obs_perf_cycles.impl()),
            read_port_output(sim, io.obs_perf_instructions.impl()),
            read_port_output(sim, io.obs_perf_branch_count.impl()),
            read_port_output(sim, io.obs_perf_branch_mispredict.impl()),
            read_port_output(sim, io.obs_perf_load_use_stalls.impl()),
            read_port_output(sim, io.obs_perf_control_stalls.impl()),
            read_port_output(sim, io.obs_data_rdata.impl()),
            read_port_output(sim, io.dbg_id_instr.impl()),
            read_port_output(sim, io.dbg_id_pc.impl()),
            read_port_output(sim, io.dbg_id_valid.impl()),
            read_port_output(sim, io.dbg_id_is_branch.impl()),
            read_port_output(sim, io.dbg_id_is_load.impl()),
            read_port_output(sim, io.dbg_id_is_store.impl()),
            read_port_output(sim, io.dbg_branch_taken.impl()),
            read_port_output(sim, io.dbg_wb_write_en.impl()),
            read_port_output(sim, io.dbg_wb_write_data.impl()),
            read_port_output(sim, io.dbg_wb_rd_addr.impl()),
            read_port_output(sim, io.dbg_exmem_alu_result.impl()),
            read_port_output(sim, io.dbg_exmem_imm.impl()),
            read_port_output(sim, io.dbg_exmem_rs2_data.impl()),
            read_port_output(sim, io.dbg_exmem_valid.impl()),
            read_port_output(sim, io.dbg_mem_valid_out.impl()),
            read_port_output(sim, io.dbg_load_use_detected.impl())};
}

// ---------------------------------------------------------------------------
// 最小编码器。手算常量交叉校验（见 TEST_CASE ①）—— 只用常量断言 helper 会变成
// 自证，所以两边都写：helper 产出 vs 手算常量。
// ---------------------------------------------------------------------------
constexpr uint32_t NOP = 0x00000013u;  // addi x0, x0, 0

constexpr uint32_t enc_r(uint32_t rd, uint32_t rs1, uint32_t rs2,
                         uint32_t funct3, uint32_t funct7) {
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) |
           (rd << 7) | 0x33u /* OP */;
}

constexpr uint32_t enc_i(uint32_t rd, uint32_t rs1, uint32_t funct3,
                         int32_t imm, uint32_t opcode) {
    return ((static_cast<uint32_t>(imm) & 0xFFFu) << 20) | (rs1 << 15) |
           (funct3 << 12) | (rd << 7) | opcode;
}

constexpr uint32_t enc_s(uint32_t rs2, uint32_t rs1, uint32_t funct3,
                         int32_t imm) {
    uint32_t u = static_cast<uint32_t>(imm) & 0xFFFu;
    return ((u >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) |
           ((u & 0x1Fu) << 7) | 0x23u /* STORE */;
}

constexpr uint32_t enc_b(uint32_t rs1, uint32_t rs2, uint32_t funct3,
                         int32_t imm) {
    uint32_t u = static_cast<uint32_t>(imm) & 0x1FFFu;  // 13-bit, ±4 KiB
    uint32_t b12 = (u >> 12) & 1u;
    uint32_t b11 = (u >> 11) & 1u;
    uint32_t b10_5 = (u >> 5) & 0x3Fu;
    uint32_t b4_1 = (u >> 1) & 0xFu;
    return (b12 << 31) | (b10_5 << 25) | (rs2 << 20) | (rs1 << 15) |
           (funct3 << 12) | (b4_1 << 8) | (b11 << 7) | 0x63u /* BRANCH */;
}

// 便捷别名（funct3/funct7 按 RV32I 规范）
constexpr uint32_t ADDI(uint32_t rd, uint32_t rs1, int32_t imm) {
    return enc_i(rd, rs1, 0x0u, imm, 0x13u);
}
constexpr uint32_t ADD(uint32_t rd, uint32_t rs1, uint32_t rs2) {
    return enc_r(rd, rs1, rs2, 0x0u, 0x00u);
}
constexpr uint32_t SUB(uint32_t rd, uint32_t rs1, uint32_t rs2) {
    return enc_r(rd, rs1, rs2, 0x0u, 0x20u);
}
constexpr uint32_t SW(uint32_t rs2, uint32_t rs1, int32_t imm) {
    return enc_s(rs2, rs1, 0x2u, imm);
}
constexpr uint32_t LW(uint32_t rd, uint32_t rs1, int32_t imm) {
    return enc_i(rd, rs1, 0x2u, imm, 0x03u);
}
constexpr uint32_t BEQ(uint32_t rs1, uint32_t rs2, int32_t imm) {
    return enc_b(rs1, rs2, 0x0u, imm);
}
constexpr uint32_t BNE(uint32_t rs1, uint32_t rs2, int32_t imm) {
    return enc_b(rs1, rs2, 0x1u, imm);
}

// ---------------------------------------------------------------------------
// 固件：**48 个字**（含 NOP 填充），覆盖 ALU-only / sw→lw / **紧邻 load→use** ×2 /
// taken + not-taken branch。
//
// 地址约束（SoC 顶层的 TCM 是 12 位 ⇒ 16 KB）：程序与数据都 < 0x1000。
//
// ISA 语义 golden（golden A/B/C）：
//   x1 = 10, x2 = 3  =>  x3 = 13, x4 = 10
//   sw x4 -> [0x100]  =>  [0x100] == 10          (golden A: 纯 ALU 结果落 dmem)
//   lw x5 <- [0x100]  =>  x5 == 10
//   sw x5 -> [0x104]  =>  [0x104] == 10          (golden B: load 真的读回了值)
//   beq (taken) 冲刷 word 29 ⇒ x6 保持 0
//   bne (not taken) => x8 = 2
//   sw x8 -> [0x108]  =>  [0x108] == 2           (golden C: 控制流走对分支)
// ---------------------------------------------------------------------------
constexpr uint32_t kGoldenAddrA = 0x100u;
constexpr uint32_t kGoldenAddrB = 0x104u;
constexpr uint32_t kGoldenAddrC = 0x108u;
constexpr uint32_t kGoldenValA = 10u;
constexpr uint32_t kGoldenValB = 20u;   // x6 = x5 + x5 = 20（load-use 停顿后仍正确）
constexpr uint32_t kGoldenValC = 2u;

constexpr size_t kFirmwareWords = 48;

// 期望的写回序列（ISA 语义）。
// ⚠️ **不含 x7** —— 它在 taken 分支的错误路径上 ⇒ MUST 被冲刷 ⇒ 不写回。
// ⚠️ **不含 x6 之前任何「消费者」的错误值** —— 紧邻的 load→use 必须拿到**新值**。
constexpr std::array<std::pair<uint64_t, uint64_t>, 11> kExpectedWritebacks = {{
    {1, 10},   // addi x1, x0, 10
    {2, 3},    // addi x2, x0, 3
    {3, 13},   // add  x3, x1, x2
    {4, 10},   // sub  x4, x3, x2
    {5, 10},   // lw   x5, 0x100(x0)
    {6, 20},   // add  x6, x5, x5   ← load-use 停顿后拿到的**新值**
    {8, 1},    // addi x8, x0, 1
    {9, 2},    // addi x9, x0, 2
    {10, 7},   // addi x10, x0, 7  ← 「较老指令写同一 rd」用例的前半
    {10, 0},   // lw   x10, 0x110  ← load 读到 0（dmem 零初始化）
    {11, 0},   // add  x11, x10,x10 ⇒ **0**（若提前解除 ⇒ 会是 14）
}};

// ⚠️ MUST 是 **constexpr**（不是 `static const`）⇒ 才能被编译期护栏检查
//    （见下 `stall_drop_slots_are_nop`）。
constexpr std::array<uint32_t, kFirmwareWords> firmware() {
    return {{
        ADDI(1, 0, 10),   // 0  x1 = 10
        NOP, NOP, NOP,
        ADDI(2, 0, 3),    // 4  x2 = 3
        NOP, NOP, NOP,
        ADD(3, 1, 2),     // 8  x3 = 13
        NOP, NOP, NOP,
        SUB(4, 3, 2),     // 12 x4 = 10
        NOP, NOP, NOP,
        SW(4, 0, 0x100),  // 16 [0x100] = 10   (golden A)
        NOP, NOP, NOP,
        LW(5, 0, 0x100),  // 20 x5 = 10
        // ⚠️ **紧邻的 load → use**：考验 load-use 冒险的**停顿**是否真的生效
        //    （若停顿不生效，x6 会拿到旧值 0 ⇒ golden B 变 0）。
        ADD(6, 5, 5),     // 21 x6 = 20
        // ⚠️ word 22 **MUST 是 NOP**：load-use 停顿会**丢弃「消费者之后的
        //    第一条指令」**（imem 的 1 拍 sread + IF/ID 冻结 ⇒ 已提交未锁存
        //    的那条无处可存）⇒ 该位置只能是 NOP（详见 design.md §3.10）。
        NOP,
        NOP, NOP,
        SW(6, 0, 0x104),  // 25 [0x104] = 20   (golden B)
        NOP, NOP, NOP,
        BEQ(0, 0, 8),     // 29 taken => jump to 31
        ADDI(7, 0, 0x7f), // 30 错误路径 ⇒ MUST 被冲刷（x7 保持 0）
        NOP,              // 31 分支目标
        ADDI(8, 0, 1),    // 32
        NOP, NOP, NOP,
        BNE(0, 0, 8),     // 36 not taken => 落到 37
        ADDI(9, 0, 2),    // 37 x9 = 2
        NOP, NOP, NOP,
        SW(9, 0, 0x108),  // 41 [0x108] = 2    (golden C)
        // ⚠️ **「较老指令写同一 rd」回归用例**（2026-10-25，第 14 轮 critic
        //    BLOCKER 1 的实测复现）：
        //      addi x10, x0, 7      ← **较老**指令，写 x10
        //      lw   x10, 0x110(x0)  ← 被阻塞的 load，**也**写 x10
        //      add  x11, x10, x10   ← 紧邻消费者
        //    ⇒ 若停顿的解除条件是「按 rd 匹配 WB 的写回」，则**较老**的
        //      `addi x10`（只比 load 老一条）在下一拍到达 WB ⇒ rd 匹配成立
        //      ⇒ 停顿**提前一拍**解除 ⇒ 消费者在 load 写回的**同一边沿**
        //        锁存操作数（`aread` 是组合读 ⇒ 读到边沿前的旧值 7）
        //      ⇒ **x11 = 14 而不是 0**。
        //    ⇒ 期望：`[0x110]` 从未被写过（dmem 零初始化）⇒ load 读到 0
        //      ⇒ x11 = 0。
        ADDI(10, 0, 7),   // 42 较老指令，写 x10
        LW(10, 0, 0x110), // 43 被阻塞的 load，**也**写 x10（读到 0）
        ADD(11, 10, 10),  // 44 消费者 ⇒ x11 = 0
        NOP, NOP, NOP,
    }};
}

// 把固件逐拍写进 I-TCM（`ext_rst` 保持 1 —— 计数器在复位期被清 0）
void load_firmware(Simulator &sim, Rv32iPipelineTop &dut) {
    auto &io = dut.io();
    const auto &fw = firmware();
    sim.set_input_value(io.ext_rst, 1u);
    sim.set_input_value(io.ext_clk, 0u);
    sim.set_input_value(io.prog_we, 1u);
    for (std::size_t i = 0; i < fw.size(); ++i) {
        sim.set_input_value(io.prog_addr, i);
        sim.set_input_value(io.prog_data, fw[i]);
        sim.tick(1);
    }
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0);
    sim.set_input_value(io.prog_data, 0);
    sim.set_input_value(io.ext_rst, 0u);
}

// 跑 n 拍，记录每拍的观测向量
using Trace = std::vector<std::array<uint64_t, kObsCount>>;

Trace run_trace(Simulator &sim, Rv32iPipelineTop &dut, unsigned cycles) {
    Trace t;
    t.reserve(cycles);
    for (unsigned c = 0; c < cycles; ++c) {
        sim.tick(1);
        t.push_back(obs_snapshot(dut, sim));
    }
    return t;
}

// 某个 store 拍（addr, data）是否出现过
bool has_store(const Trace &t, uint32_t addr, uint32_t data) {
    // ⚠️ 下标 MUST 用**编译期**查找（与本文件其它地方同一套纪律），
    //    不得手抄 —— 手抄下标会随观测向量增删而**静默读错字段**。
    for (const auto &snap : t) {
        if (snap[kIdxDataWriteEn] != 1) continue;
        if (snap[kIdxDataAddr] == addr && snap[kIdxDataWriteData] == data) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ===========================================================================
// ⚠️ **编译期护栏**：load-use 停顿点之后的位置 MUST 是 NOP
//
// 背景（design.md §3.10，第 17 轮 critic BLOCKER）：load-use 停顿会**丢弃
// 「消费者之后的第一条指令」**（imem 是 1-cycle `sread` ⇒ 该指令的地址已在
// 停顿前提交、数据在停顿首拍返回，而 IF/ID 在停顿期间冻结 ⇒ 无处可存）。
// ⇒ 固件 MUST 保证那个位置是 NOP，否则该指令会**静默消失**。
//
// 判定条件与 `hazard_unit.h` 的检测条件**同构**
// （`ex_is_load & rd!=0 & (rs1==rd | rs2==rd)`）⇒ 不误报；
// 方向是 fail-safe（将来加 U/J 型指令时只会**多**要求 NOP，不会放过）。
//
// ⚠️ **compile-time** ⇒ 零运行时成本、零误报、固件一改就编译失败。
// ===========================================================================
constexpr bool stall_drop_slots_are_nop() {
    for (std::size_t i = 0; i + 2 < kFirmwareWords; ++i) {
        const std::uint32_t w = firmware()[i];
        if ((w & 0x7Fu) != 0x03u) {
            continue;  // 不是 LOAD
        }
        const std::uint32_t rd = (w >> 7) & 31u;
        if (rd == 0) {
            continue;  // 写 x0 ⇒ 不会有依赖
        }
        const std::uint32_t c = firmware()[i + 1];  // 紧邻的消费者
        if (((c >> 15) & 31u) == rd || ((c >> 20) & 31u) == rd) {
            if (firmware()[i + 2] != NOP) {
                return false;  // 被丢的位置 MUST 是 NOP
            }
        }
    }
    return true;
}
static_assert(stall_drop_slots_are_nop(),
              "load-use 停顿点之后的位置 MUST 是 NOP（design.md §3.10）："
              "该指令会被停顿静默丢弃 ⇒ 固件 MUST 用 NOP 填充该位置");

// ===========================================================================
// ① 固件自校验：编码器 vs 手算常量；固件长度与地址边界。
// ===========================================================================
TEST_CASE("Rv32iSoc - FirmwareEncoderCrossCheck", "[rv32i][soc][dual]") {
    // 手算常量（RV32I 规范，逐位核对过）—— 与 helper 交叉校验，避免自证
    REQUIRE(ADDI(1, 0, 1) == 0x00100093u);   // addi x1, x0, 1
    REQUIRE(ADDI(2, 0, 2) == 0x00200113u);   // addi x2, x0, 2
    REQUIRE(ADD(3, 1, 2) == 0x002081B3u);    // add x3, x1, x2
    REQUIRE(SUB(4, 3, 2) == 0x40218233u);    // sub x4, x3, x2
    REQUIRE(SW(4, 0, 0x100) == 0x10402023u); // sw x4, 0x100(x0)
    REQUIRE(LW(5, 0, 0x100) == 0x10002283u); // lw x5, 0x100(x0)
    REQUIRE(BEQ(0, 0, 8) == 0x00000463u);    // beq x0, x0, +8
    REQUIRE(BNE(0, 0, 8) == 0x00001463u);    // bne x0, x0, +8
    REQUIRE(NOP == 0x00000013u);

    const auto &fw = firmware();
    REQUIRE(fw.size() >= 20u);  // capability 要求 >= 20 条

    // TCM 是 12 位 => 程序地址上界 16 KB
    REQUIRE(fw.size() * 4u <= 0x1000u);
    REQUIRE(kGoldenAddrC < 0x1000u);
}

// ===========================================================================
// ② 双路逐周期 parity + ISA 语义 golden（核心验收）。
// ===========================================================================
TEST_CASE("Rv32iSoc - DualBackendTraceParity",
          "[rv32i][soc][dual][verilator][e2e]") {
    constexpr unsigned kCycles = 200;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iPipelineTop>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iPipelineTop>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

    // interpreter 侧双钉：显式 backend + 关 JIT（CH_JIT_ENABLE=ON 时默认走 JIT）
#if __has_include("jit/jit_compiler.h")
    sim_a->set_jit_enabled(false);
#endif
    sim_a->set_backend(std::make_unique<InterpreterBackend>());

    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_soc");
    REQUIRE_FALSE(workdir.empty());
    auto backend = std::make_unique<VerilatorBackend>(workdir);
    if (!backend->initialize(dev_b->context(), data_map_b)) {
        WARN("VerilatorBackend::initialize() returned false - verilator half "
             "skipped (see ADR-035 R3: >64-bit ports are rejected)");
        return;
    }
    if (backend->port_access_snapshot().empty()) {
        WARN("verilator dlopen did not populate port_access_ - skipped");
        return;
    }
    sim_b->set_backend(std::move(backend));

    load_firmware(*sim_a, dev_a->instance());
    load_firmware(*sim_b, dev_b->instance());

    const Trace ta = run_trace(*sim_a, dev_a->instance(), kCycles);
    const Trace tb = run_trace(*sim_b, dev_b->instance(), kCycles);

    REQUIRE(ta.size() == tb.size());

    // ---- 逐拍 dump（诊断工具）：RV32I_SOC_DEBUG=N ⇒ 打印前 N 拍的观测向量 ----
    if (std::getenv("RV32I_SOC_DEBUG") != nullptr) {
        const unsigned long lim = std::strtoul(
            std::getenv("RV32I_SOC_DEBUG"), nullptr, 10);
        for (std::size_t c = 0; c < ta.size() && c < lim; ++c) {
            std::cout << "cyc " << c;
            for (std::size_t k = 0; k < kObsCount; ++k) {
                std::cout << " | " << ta[c][k] << "/" << tb[c][k];
            }
            std::cout << "\n";
        }
    }

    // ---- 逐周期 diff（全部 27 个输出观测值，**无掩码**）----
    //
    // ⚠️ 这里**不需要**掩码：修复前之所以要掩码 `data_addr`/`data_write_data`，
    //    是因为流水线本身是坏的（两个后端在 EX/MEM 寄存器上真的不一致）。
    //    掩码会把「真分歧」藏起来 ⇒ 现在要求**逐拍全等**。
    for (std::size_t c = 0; c < ta.size(); ++c) {
        for (std::size_t k = 0; k < kObsCount; ++k) {
            INFO("cycle " << c << " obs " << kObsNames[k] << " (interp="
                          << ta[c][k] << ", verilator=" << tb[c][k] << ")");
            REQUIRE(ta[c][k] == tb[c][k]);
        }
    }

    // ---- ③ ISA 语义 golden（两端一致 ≠ 两端都对）----
    //
    // A: 纯 ALU 结果落 dmem（x4 = 13 - 3 = 10）
    REQUIRE(has_store(ta, kGoldenAddrA, kGoldenValA));
    // B: load 真的读回了值（x5 = [0x100] = 10）⇒ load 路径确实生效
    REQUIRE(has_store(ta, kGoldenAddrB, kGoldenValB));
    // C: 控制流走对分支（bne 未成立 ⇒ x8 = 2）
    REQUIRE(has_store(ta, kGoldenAddrC, kGoldenValC));

    // ⚠️ **store 拍的**个数** MUST 恰好是 3**（2026-10-25，第 14 轮 critic
    //    SHOULD_FIX #2）：`has_store` 只是**存在性**断言 ⇒ 若回归出第 4 个
    //    **spurious** store 拍（正是 X1 防的「气泡拍静默误写 dmem」），
    //    存在性断言**全绿** ⇒ 必须补**计数**断言。
    {
        std::size_t store_cycles = 0;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxDataWriteEn] == 1u) {
                ++store_cycles;
            }
        }
        REQUIRE(store_cycles == 3u);
    }

    // 反向：错误的值 MUST NOT 出现（防止「碰巧出现过一次」也算过）
    REQUIRE_FALSE(has_store(ta, kGoldenAddrA, kGoldenValA + 1));
    REQUIRE_FALSE(has_store(ta, kGoldenAddrC, kGoldenValC + 1));

    // ---- 分支 / 停顿计数 ----
    // 固件里恰好 2 条分支（beq + bne）⇒ branch_count == 2
    REQUIRE(ta.back()[kIdxBranchCount] == 2u);
    // 只有 taken 的 beq 被判为误预测（bne 不成立）⇒ mispredict == 1
    REQUIRE(ta.back()[kIdxMispredict] == 1u);
    // ⚠️ **load-use 停顿拍数 == 6**（2026-10-25，A1-a 多拍停顿）：
    //    固件里有**两处**紧邻的 load→use：
    //      ① `lw x5` 紧接着 `add x6,x5,x5`（word 20/21）
    //      ② `lw x10` 紧接着 `add x11,x10,x10`（word 43/44）
    //    ⇒ 每处停顿 **3 拍**（load 从 EX 到 WB 是 2 级 ⇒ 数据在「检测拍 + 2」
    //    的边沿写回 ⇒ 消费者 MUST 等到「检测拍 + 3」）⇒ 合计 **6 拍**。
    //    ⚠️ 修复前这个计数器**恒为 0**（检测用的是 MEM 级 load ⇒ 晚一级
    //    ⇒ 永远对不上）⇒ 本断言是「冒险真的被检测到」的判别力来源。
    REQUIRE(ta.back()[kIdxLoadUseStalls] == 6u);
    // 控制停顿 == 误预测次数
    REQUIRE(ta.back()[kIdxControlStalls] == 1u);

    // ---- 被冲刷的指令 MUST **从未执行**（控制冒险的核心契约）----
    //
    // 固件 word 30 = `addi x7,x0,0x7f` 位于 taken 分支的错误路径 ⇒
    // 它 MUST NOT 被写回（x7 保持 0）。
    // ⚠️ 这条断言是**新增**的（2026-10-25，第十三轮审稿）：此前的实现里
    // ID/EX 没有 flush 通路 ⇒ 错误路径的指令照常执行并写回了 x6=0x7f，
    // 而旧断言**从不查 x6** ⇒ 零信号。
    for (std::size_t c = 0; c < ta.size(); ++c) {
        INFO("cycle " << c);
        // ⚠️ Catch2 不支持链式比较 ⇒ 整体加括号
        // ⚠️ 下标 MUST 用**编译期**查找（`obs_idx`）—— 本文件已立此纪律
        REQUIRE_FALSE((ta[c][kIdxWbWriteEn] == 1u &&
                       ta[c][kIdxWbRdAddr] == 7u));  // wb_write_en && rd == x7
    }

    // ---- 退休指令数（按**逐拍实测**的非退休拍推导，不拍脑袋）----
    //
    // 退休数在 **cyc 1..199** 这 199 个拍间隔里有 **11** 个「不增长」，
    // 分四段：
    //   ① 2 拍填充（cyc 1..2：IF/ID → ID/EX → EX/MEM → MEM/WB 依次就绪；
    //      cyc 0 是第 1 拍，退休数从 0 起，故填充段落在 cyc 1..2）
    //   ② 3 拍 **load-use 停顿 #1**（消费者留在 ID 级；EX/MEM 与 MEM/WB 灌气泡）
    //   ③ 3 拍 **冲刷气泡**（taken 分支让出错误路径 + 重新取指目标）
    //      ⚠️ **3** 拍：flush MUST **多杀一拍**（imem 是 1-cycle `sread` ⇒
    //      flush 当拍提交的那条在下一拍才返回 ⇒ `flush_d1` 再门一拍）⇒
    //      代价 = 分支目标的指令晚 1 拍进 ID
    //   ④ 3 拍 **load-use 停顿 #2**（第二处紧邻 load→use）
    // ⇒ 199 − 11 = 188 条指令退休
    //
    // ⚠️ 「不增长的间隔数」与「退休数」**分别断言** ⇒ 若流水深度变了，
    //    两条断言**一起**转红（不会「一条变、一条不变」而掩盖）。
    std::size_t non_retired = 0;
    for (std::size_t c = 1; c < ta.size(); ++c) {
        if (ta[c][kIdxInstructions] == ta[c - 1][kIdxInstructions]) {
            ++non_retired;
        }
    }
    REQUIRE(non_retired == 11u);
    REQUIRE(ta.back()[kIdxInstructions] == kCycles - 12u);

    // ---- 写回序列（ISA 语义的最强判据）----
    //
    // 逐拍收集所有写回 `(rd, data)`，**去掉连续重复项**（已知残留：取指
    // 填充期同一条指令被装两次 ⇒ 幂等地写回两次 —— 见 design.md §3.6），
    // 然后与固件的**期望序列**逐项比对。
    //
    // ⚠️ 为什么必须是**序列**而不是计数：
    //    「多一次假退休」会让「退休数」与「写回次数」**同步** +1
    //    ⇒ 任何**计数**断言都抓不到（变异验证实测：`exmem_valid` 改回取
    //    ID 级的 valid ⇒ 计数不变 ⇒ 零信号）⇒ 只有序列能抓。
    //
    // 期望序列（固件 **48 字**；word 29 的 BEQ 被 taken ⇒ 冲刷 word 30 的
    // `addi x7,x0,0x7f` ⇒ **x7 MUST NOT 出现**）：
    //   x1=10, x2=3, x3=13, x4=10, x5=10, x6=20, x8=1, x9=2
    {
        std::vector<std::pair<uint64_t, uint64_t>> wb;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] != 1u) {
                continue;
            }
            const std::pair<uint64_t, uint64_t> item{
                ta[c][kIdxWbRdAddr], ta[c][kIdxWbWriteData]};
            if (!wb.empty() && wb.back() == item) {
                continue;  // 连续重复 ⇒ 已知残留（幂等写回）
            }
            wb.push_back(item);
        }
        REQUIRE(wb.size() == kExpectedWritebacks.size());
        for (std::size_t i = 0; i < wb.size() && i < kExpectedWritebacks.size();
             ++i) {
            INFO("writeback #" << i << " got (rd=" << wb[i].first
                                << ", data=" << wb[i].second << ")");
            REQUIRE(wb[i] == kExpectedWritebacks[i]);
        }
    }

    // ---- ⚠️ **判别性断言**：`(8,1)` MUST 恰好写回**一次** ----
    //
    // 背景（第 16 轮 critic BLOCKER B1）：taken 分支的 flush 此前**漏杀**
    // 「flush 当拍提交地址」的那条在飞取指（imem 是 1-cycle `sread` ⇒ 数据在
    // 下一拍才返回 ⇒ 只在 flush 拍门控不够）⇒ 该错误路径指令被执行了**两次**。
    // ⇒ 它被上面的「连续重复去重」**掩盖**了（去重是为 §3.6 的填充期残留设计的）
    // ⇒ 必须**单独**断言「**原始**写回序列里 `(8,1)` 只出现一次」。
    {
        std::size_t x8_writes = 0;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] == 1u && ta[c][kIdxWbRdAddr] == 8u &&
                ta[c][kIdxWbWriteData] == 1u) {
                ++x8_writes;
            }
        }
        REQUIRE(x8_writes == 1u);
    }

    // ---- 冒险**次数**（与「停顿**拍数**」区分）----
    // `dbg_load_use_detected` 是**单拍脉冲** ⇒ 数它的脉冲个数 = 冒险次数。
    // 固件里有**两处**紧邻的 load→use ⇒ 恰好 2 次。
    std::size_t hazards = 0;
    for (std::size_t c = 0; c < ta.size(); ++c) {
        if (ta[c][kIdxLoadUseDetected] == 1u) {
            ++hazards;
        }
    }
    REQUIRE(hazards == 2u);

    // ---- 指令流与 PC MUST 始终对齐（imem 是 1-cycle sread ⇒ 极易错开）----
    // 取指字索引 = (dbg_id_pc - 0x80000000) / 4
    for (std::size_t c = 0; c < ta.size(); ++c) {
        const uint64_t pc = ta[c][kIdxIdPc];
        if (pc < 0x80000000ull) continue;
        const uint64_t word = (pc - 0x80000000ull) / 4u;
        const uint64_t instr = ta[c][kIdxIdInstr];
        if (word >= firmware().size()) continue;  // 程序跑出尾部（补 0）
        INFO("cycle " << c << " pc word " << word);
        REQUIRE(instr == firmware()[word]);
    }
}
