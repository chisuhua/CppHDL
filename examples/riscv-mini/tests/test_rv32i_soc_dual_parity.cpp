// examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp
//
// RV32I **SoC 级**双路 trace parity（interpreter + verilator）。
//
// Change: rv32i-pipeline-commit-path-2026-10（C3 的前置）
// Capability: openspec/specs/rv32i-commit-path/spec.md
//            （+ openspec/specs/verilator-rv32i-e2e/spec.md 的 MODIFIED）
// DUT:        Rv32iCoreTop（examples/riscv-mini/src/rv32i_core_top.h）
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
//    顶层端口建访问表，读不到设计内部。⇒ 核 IP 顶层暴露 32 个端口
//    （5 输入 + 27 输出 = 11 功能观测 + 16 诊断观测），本测试比对**全部 27 个输出**。
//
// 3. **固件必须无数据冒险** —— lib 版 `HazardUnit` 不接数据前推
//    （include/cpu/pipeline/hazard_unit.h: `io().rs1_data = io().rs1_data_raw`），
//    接上会在真实装配里形成组合环（ADR-046 D3 #14；D5 对此仍悬置未裁决）。
//    ⇒ 生产者→消费者之间插 3 条 NOP。
//    **不要**通过「把前推接上」来修这个 —— 那是 D5 的裁决对象。
//
// ---------------------------------------------------------------------------
// 这个测试的由来：它第一次让核 IP 顶层**可观测**，并因此照出了 17 个缺陷
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
#include <map>
#include <set>
#include <vector>

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/eval_backend.h"
#include "core/interpreter_backend.h"
#include "core/verilator_backend.h"
#include "cpu/pipeline/rv32i_pipeline.h"
#include "cpu/pipeline/rv32i_tcm.h"
#include "cpu/riscv/rv32i_asm_encode.h"
#include "rv32i_fw_demo.h"
#include "rv32i_core_top.h"
#include "simulator.h"
#include "dual_backend_fixture.h"

namespace {

// ⚠️ **2026-11-14 · L3（编码器库化）**：编码器**上移到** `cpu/riscv/rv32i_asm_encode.h`
//    ⇒ ⇒ 本文件**不再**持有副本 ⇒ ⇒ 「**唯一**一份」且在**库**里 ⇒ ⇒
//    固件转储器工具（L4）可**复用**同一份 ⇒ ⇒ 两份编码器漂移的风险**归零**。
//    ⇒ ⇒ ⚠️ **手算交叉校验 MUST 保留**（`FirmwareEncoderCrossCheck`）——
//    它拿**手算常量**对比本库的产出 ⇒ ⇒ 那才是**独立**证据（用库自证 = 零判别力）。
using riscv::ADDI;
using riscv::ADD;
using riscv::AUIPC;
using riscv::BEQ;
using riscv::BNE;
using riscv::JAL;
using riscv::JALR;
using riscv::LB;
using riscv::LBU;
using riscv::LH;
using riscv::LHU;
using riscv::LW;
using riscv::NOP;
using riscv::SB;
using riscv::SH;
using riscv::SRA;
using riscv::SUB;
using riscv::SW;

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
    "dbg_load_use_detected",
    // ⚠️ **G1/G2（2026-11-15 · B4）**：未知 opcode 的可观测面
    //   （0 = 合法 · 1 = 已知未实现 `OP_SYSTEM` · 2 = 未知 opcode）
    //   ⇒ ⇒ 进观测向量 ⇒ ⇒ **双后端 parity 也覆盖它**
    "dbg_illegal_reason"};
constexpr std::size_t kObsCount = sizeof(kObsNames) / sizeof(kObsNames[0]);
// 32 个端口里 4 个是输入（ext_rst / prog_*）⇒ 28 个输出被观测
static_assert(kObsCount == 28, "观测向量长度必须与核 IP 顶层的**输出**端口数一致");

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
constexpr std::size_t kIdxIdValid = obs_idx("dbg_id_valid");
constexpr std::size_t kIdxIdInstr = obs_idx("dbg_id_instr");
constexpr std::size_t kIdxIllegalReason = obs_idx("dbg_illegal_reason");
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
static_assert(kIdxIdValid != kObsCount, "dbg_id_valid 未在 kObsNames 中");
static_assert(kIdxIdInstr != kObsCount, "dbg_id_instr 未在 kObsNames 中");
static_assert(kIdxIllegalReason != kObsCount, "dbg_illegal_reason 未在 kObsNames 中");
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
std::array<uint64_t, kObsCount> obs_snapshot(Rv32iCoreTop &dut,
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
            read_port_output(sim, io.dbg_load_use_detected.impl()),
            read_port_output(sim, io.dbg_illegal_reason.impl())};
}

// ---------------------------------------------------------------------------
// 最小编码器。手算常量交叉校验（见 TEST_CASE ①）—— 只用常量断言 helper 会变成
// 自证，所以两边都写：helper 产出 vs 手算常量。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 固件：**48 个字**（含 NOP 填充），覆盖 ALU-only / sw→lw / **紧邻 load→use** ×2 /
// taken + not-taken branch。
//
// 地址约束（核 IP 顶层的 TCM 是 12 位 ⇒ 16 KB）：程序与数据都 < 0x1000。
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

// ⚠️ **字节 / 半字写 golden**（2026-10-26 新增）：
//   sb x2(=3) -> [0x101] ⇒ 字 0x100 的 **lane 1** ⇒ 字 = (10 & ~0xFF00) | (3<<8)
//                                        = 0x0000_030A
//   sh x3(=13) -> [0x122] ⇒ 字 0x120 的 **lane 2-3** ⇒ 字 = 13 << 16 = 0x000D_0000
// ⚠️ `kGoldenDataSB/SH` 是**store 拍上的写数据**（= 源数据左移到位）⇒
//    它直接钉住「lane 对齐」这个**易错点**（未对齐时是 3 / 13 ⇒ 断言转红）。
constexpr uint32_t kGoldenAddrSB = 0x101u;
constexpr uint32_t kGoldenDataSB = 0x300u;    // 3 << 8
constexpr uint32_t kGoldenWordSB = 0x30Au;    // [0x100] = 0x0A | (3 << 8)
constexpr uint32_t kGoldenAddrSH = 0x122u;
constexpr uint32_t kGoldenDataSH = 0xD0000u;  // 13 << 16
constexpr uint32_t kGoldenWordSH = 0xD0000u;  // [0x120] = 13 << 16

// ⚠️ **跳转 / SRA golden**（2026-10-26 新增，T3）：
//
//   地址基址 = 0x80000000（`pc_reg` 初值）⇒ word N 的地址 = 基址 + 4N
//
//   ① `JAL x14, +12`（word 64）⇒ 跳到 word 67
//      link = word 64 的 pc + 4 = word 65 的地址 = 基址 + 260 = 0x80000104
//   ② `ADDI x16, x0, 0x2A`（word 67）⇒ **只有真的跳到 67 才会写回** x16 = 42
//   ③ `AUIPC x18, 0`（word 72）⇒ x18 = 基址 + 288 = 0x80000120
//      `ADDI x18, x18, 44`（word 76）⇒ x18 = 0x80000120 + 44 = 0x8000014C（word 83）
//   ④ `JALR x17, x18, 0`（word 80）⇒ 跳到 (x18 & ~1) = word 83
//      link = word 80 的 pc + 4 = word 81 的地址 = 基址 + 324 = 0x80000144
//   ⑤ `ADDI x20, x0, 0x3C`（word 83）⇒ **只有真的跳到 83 才会写回** x20 = 60
//   ⑥ `ADDI x21, x0, -8`（word 87）⇒ x21 = 0xFFFFFFF8
//      `SRA x22, x21, x2`（word 91）⇒ x2 = 3 ⇒ 0xFFFFFFF8 >>> 3 = **0xFFFFFFFF**
//         （若 SRA 退化为 SRL ⇒ 0x1FFFFFFF ⇒ 断言转红）
//      `SRA x24, x21, x0`（word 95）⇒ 移位量 0 ⇒ **0xFFFFFFF8**（MUST NOT 填充）
constexpr uint32_t kGoldenLinkJal  = 0x80000104u;  // word 65 的地址
constexpr uint32_t kGoldenSubBody  = 0x2Au;        // 42
constexpr uint32_t kGoldenAuipc    = 0x80000120u;  // word 72 的地址
constexpr uint32_t kGoldenJalrBase = 0x8000014Cu;  // word 83 的地址
constexpr uint32_t kGoldenLinkJalr = 0x80000144u;  // word 81 的地址
constexpr uint32_t kGoldenJalrBody = 0x3Cu;        // 60
constexpr uint32_t kGoldenSraSrc   = 0xFFFFFFF8u;  // -8
constexpr uint32_t kGoldenSraSh3   = 0xFFFFFFFFu;  // -8 >>> 3
constexpr uint32_t kGoldenSraSh0   = 0xFFFFFFF8u;  // 移位 0 ⇒ 不变
// ⚠️ 「错误路径上的跳转」用例（word 100-102）：
//   `JAL x26, +8` @word100 ⇒ link = word 101 的地址 = 基址 + 404 = 0x80000194
//   `ADDI x28, x0, 0x5A` @word102 ⇒ **只有 PC 没被 word101 的 JAL 劫持才会执行**
constexpr uint32_t kGoldenLinkJal2 = 0x80000194u;  // word 101 的地址
constexpr uint32_t kGoldenNestedBody = 0x5Au;      // 90

// ⚠️ **2026-11-23（S3 · critic SHOULD_FIX #1）**：108 → **110** —— 末尾补一条
//    **hit 形态**的 load→use（`lw x29,[0x100]` 命中已被 `lw x5` 填好的行 +
//    `add x30,x29,x29`）⇒ ⇒ 恢复已签批条款「load-use 停顿 **4 拍**」在**固件级**
//    的载体（D-Cache 接入后，原有的两处 load→use 的 load 都 miss ⇒ 保持拍被
//    归入内存反压 ⇒ 只剩 1 检测拍/处 ⇒ 「恰好 4 拍」这一精确形态在固件级消失）。
constexpr size_t kFirmwareWords = 110;

// 期望的写回序列（ISA 语义）。
// ⚠️ **不含 x7** —— 它在 taken 分支的错误路径上 ⇒ MUST 被冲刷 ⇒ 不写回。
// ⚠️ **不含 x6 之前任何「消费者」的错误值** —— 紧邻的 load→use 必须拿到**新值**。
// ⚠️ **不含 x19 / x23** —— 它们分别在 JAL / JALR 之后的错误路径上
//    ⇒ MUST 被冲刷（见下方专项断言）。
constexpr std::array<std::pair<uint64_t, uint64_t>, 26> kExpectedWritebacks = {{
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
    {12, kGoldenWordSB},  // lw  x12, 0x100 ⇒ **0x030A**（含 sb 的字节写）
    {13, kGoldenWordSH},  // lw  x13, 0x120 ⇒ **0xD0000**（含 sh 的半字写）
    // ── 跳转 / SRA（2026-10-26 新增，T3）──
    {14, kGoldenLinkJal},   // JAL x14  ⇒ link = word 65 的地址
    {16, kGoldenSubBody},   // addi x16 ⇒ **只有真的跳到 word 67 才会出现**
    {18, kGoldenAuipc},     // auipc x18 ⇒ = word 72 的地址
    {18, kGoldenJalrBase},  // addi x18 ⇒ = word 83 的地址（第二条 x18）
    {17, kGoldenLinkJalr},  // JALR x17 ⇒ link = word 81 的地址
    {20, kGoldenJalrBody},  // addi x20 ⇒ **只有真的跳到 word 83 才会出现**
    {21, kGoldenSraSrc},    // addi x21 ⇒ 0xFFFFFFF8（-8）
    {22, kGoldenSraSh3},    // sra  x22 ⇒ -8 >>> 3 = **0xFFFFFFFF**
    {24, kGoldenSraSh0},    // sra  x24 ⇒ 移位 0 ⇒ **0xFFFFFFF8**
    // ⚠️ 「错误路径上的跳转」（2026-10-27 第 1 轮 critic BLOCKER 的最小复现）
    {26, kGoldenLinkJal2},   // JAL x26 ⇒ link = word 101 的地址
    {28, kGoldenNestedBody}, // addi x28 ⇒ **只有 PC 未被 word101 的 JAL 劫持才出现**
    // ⚠️ **2026-11-23（S3）**：末尾补的 **hit 形态** load→use（word 108/109）
    //    `lw x29,[0x100]` ⇒ 0x030A（该行已被 `lw x5` 的 fill 铺满，且 `sb` 只改字节）
    //    `add x30,x29,x29` ⇒ 2 × 0x030A = **0x0614**
    {29, kGoldenWordSB},   // lw  x29 ⇒ 0x030A
    {30, 0x614u},          // add x30 ⇒ 0x0614
}};

// ⚠️ MUST 是 **constexpr**（不是 `static const`）⇒ 才能被编译期护栏检查
//    （见 `skid_firmware()` 的 `skid_slot_is_discriminating()`）。
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
        // ⚠️ word 22 曾是**必需**的 NOP：load-use 停顿会**丢弃「消费者之后的
        //    第一条指令」**（imem 的 1 拍 sread + IF/ID 冻结 ⇒ 已提交未锁存
        //    的那条无处可存）。**2026-10-27 起该约束已消除**（取指 skid buffer，
        //    ADR-048 D1–D5）⇒ 放 NOP 不再是**必需**（但仍**允许**）。
        //    ⇒ 本固件保持原样（既有 golden 零变化），**判别性证据**放在
        //      `skid_firmware()`（该处故意放**非 NOP**）。
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
        // ⚠️ **字节 / 半字写**（2026-10-26 新增）：
        //    ⚠️ 追加在**尾部** ⇒ 既有 word 索引（含 taken 分支的 29/30/31）
        //      **一个都不移动** ⇒ 既有断言全部保持有效。
        //    ⚠️ `sb` / `sh` 的源数据**低位对齐**，由总线按 `addr[1:0]` 选车道：
        //      ① `sb x2(=3) -> [0x101]` ⇒ 字 0x100 的 lane 1 ⇒ 字 = 0x030A
        //      ② `sh x3(=13) -> [0x122]` ⇒ 字 0x120 的 lane 2-3 ⇒ 字 = 0xD0000
        //    ⇒ 两条 load 读回**整字** ⇒ 同时钉住「掩码」与「lane 对齐」两件事。
        SB(2, 0, 0x101),  // 48 [0x101] = 3（字节写，lane 1）
        NOP, NOP, NOP,
        LW(12, 0, 0x100), // 52 x12 = 0x030A（读回含字节写的字）
        NOP, NOP, NOP,
        SH(3, 0, 0x122),  // 56 [0x122] = 13（半字写，lane 2-3）
        NOP, NOP, NOP,
        LW(13, 0, 0x120), // 60 x13 = 0xD0000
        NOP, NOP, NOP,
        // ⚠️ **跳转 / SRA**（2026-10-26 新增，T3）：
        //    ⚠️ 追加在**尾部**（既有 word 索引一个都不移动）⇒ 既有断言全部保持有效。
        //    ⚠️ 2 拍跳转延迟（EX 级解析）⇒ 跳转后**紧跟的 2 条**是错误路径 ⇒
        //      用它们写**不会被执行的**寄存器（x19 / x23）⇒ 冲刷失效即转红。
        JAL(14, 12),       // 64 x14 = 0x80000104（link = word 65）；跳到 word 67
        NOP,               // 65 ← link 指向这里（本固件不从这继续）
        ADDI(19, 0, 0x66), // 66 **错误路径** ⇒ MUST 被冲刷（x19 MUST NOT 写回）
        ADDI(16, 0, 0x2A), // 67 子程序体 ⇒ **只有真的跳到 67 才写** x16 = 42
        NOP, NOP, NOP, NOP,
        AUIPC(18, 0),      // 72 x18 = 0x80000120（= word 72 的地址）
        NOP, NOP, NOP,
        ADDI(18, 18, 44),  // 76 x18 = 0x8000014C（= word 83 的地址）
        NOP, NOP, NOP,
        JALR(17, 18, 0),   // 80 x17 = 0x80000144（link）；跳到 (x18 & ~1) = word 83
        NOP,               // 81 **错误路径**
        ADDI(23, 0, 0x77), // 82 **错误路径** ⇒ MUST 被冲刷（x23 MUST NOT 写回）
        ADDI(20, 0, 0x3C), // 83 目标 ⇒ **只有真的跳到 83 才写** x20 = 60
        NOP, NOP, NOP,
        ADDI(21, 0, -8),   // 87 x21 = 0xFFFFFFF8（-8）
        NOP, NOP, NOP,
        SRA(22, 21, 2),    // 91 x22 = 0xFFFFFFFF（x2 = 3 ⇒ -8 >>> 3）
        NOP, NOP, NOP,
        SRA(24, 21, 0),    // 95 x24 = 0xFFFFFFF8（移位 0 ⇒ MUST NOT 填充）
        NOP, NOP, NOP, NOP,// 96-99
        // ⚠️ **「错误路径上的跳转」用例**（2026-10-27，第 1 轮 critic BLOCKER 的最小复现）：
        //    `ID/EX` 的**控制信号**在气泡时清零，但 **`opcode` / `pc` / `imm` 不清**
        //    （它们被当作「valid=0 即无害」的数据字段）⇒ 若 `ex_stage` 用**裸 opcode**
        //    判「这是不是跳转」，那么「flush 当拍被锁进 ID/EX 的错误路径 JAL」
        //    会在下一拍的气泡里**劫持 PC**（此时 `ex_branch=0` ⇒ flush=0 ⇒ 无人拦住）。
        //    ⇒ 固件 MUST 让**紧跟 JAL 的那条也是 JAL**（`call; call` 是最真实的形态）。
        JAL(26, 8),        // 100 x26 = word 101 的地址；跳到 word 102
        JALR(27, 0, 100),  // 101 **错误路径上的 JALR** ⇒ target = 100（低地址）⇒ 劫持则程序重启
        ADDI(28, 0, 0x5A), // 102 目标 ⇒ **只有正确重定向才会写** x28 = 0x5A
        NOP, NOP, NOP,     // 103-105
        NOP, NOP,          // 106-107
        // ⚠️ **2026-11-23（S3 · critic SHOULD_FIX #1）**：末尾补 **hit 形态**的 load→use
        //    ⇒ ⇒ 恢复「load-use 停顿 MUST 是 4 拍」这一已签批形态的**固件级**载体
        //    （D-Cache 接入后，word 20/43 两处的 `lw` 都 miss ⇒ 保持拍归内存反压）。
        //    `0x100` 这行在此刻**已在 cache 里**（`lw x5` 填过 · `sb` 只改字节）⇒ hit。
        LW(29, 0, 0x100),  // 108 x29 = 0x030A（hit）
        ADD(30, 29, 29),   // 109 x30 = 0x0614（紧邻 load→use ⇒ **4 拍**停顿）
    }};
}

// 把固件逐拍写进 I-TCM（`ext_rst` 保持 1 —— 计数器在复位期被清 0）
//
// ⚠️ 2026-10-27：拆成「带固件参数」的重载 ⇒ **判别性固件**（ADR-047 D9）
//    可以是**独立**的一套 ⇒ 既有固件的 golden **零变化**
//    ⇒ 「既有 golden 的变化 MUST 全部可归因」⇒ 本次归因 = **零**。
// ⚠️⚠️ **G3（尾部填 NOP · 2026-11-15 · B4）**：I-TCM 在固件末尾之后读到的字是
//    `ch_mem` 初值 **0** ⇒ ⇒ `0x00000000` 的 opcode = `0x00` ⇒ ⇒ 是**未知 opcode**
//    ⇒ ⇒ **G1 落地后流水线会在此冻结** ⇒ ⇒ 既有 golden 的退休数会从 176 掉到 ~102。
//    ⇒ ⇒ 故**装载器 MUST 把尾部填成 NOP**（`0x00000013` = `addi x0,x0,0`）：
//      · 1 拍退休（与 0 字**相同**）
//      · `rd = 0` ⇒ 被 x0 保护吞掉（与 0 字**相同**）
//      · 不访存 / 不跳转 ⇒ 副作用**为空**（与 0 字**相同**）
//    ⇒ ⇒ ⇒ **退休数 / `non_retired` / 写回序列 逐项不变**（G3 的算术：NOP ≡ 0 字）。
//    ⚠️ 填充量 `kTailFillWords = 256` ≫ 本文件任何用例的 `kCycles` 上界（200）
//    ⇒ ⇒ 被执行到的范围**必然**被覆盖；且装载期 `ext_rst = 1` ⇒ 计数器不被污染。
constexpr std::size_t kTailFillWords = 256;

template <std::size_t N>
void load_firmware(Simulator &sim, Rv32iCoreTop &dut,
                    const std::array<uint32_t, N> &fw) {
    auto &io = dut.io();
    sim.set_input_value(io.ext_rst, 1u);
    sim.set_input_value(io.prog_we, 1u);
    for (std::size_t i = 0; i < fw.size(); ++i) {
        sim.set_input_value(io.prog_addr, i);
        sim.set_input_value(io.prog_data, fw[i]);
        sim.tick(1);
    }
    // ⚠️ **G3**：尾部填 NOP（见上方注释）⇒ ⇒ 未填 ⇒ G1 落地后在尾部冻结
    for (std::size_t i = fw.size(); i < fw.size() + kTailFillWords; ++i) {
        sim.set_input_value(io.prog_addr, i);
        sim.set_input_value(io.prog_data, NOP);
        sim.tick(1);
    }
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0);
    sim.set_input_value(io.prog_data, 0);
    sim.set_input_value(io.ext_rst, 0u);
}

// 既有固件（108 字）的重载
void load_firmware(Simulator &sim, Rv32iCoreTop &dut) {
    load_firmware(sim, dut, firmware());
}

// 跑 n 拍，记录每拍的观测向量
using Trace = std::vector<std::array<uint64_t, kObsCount>>;

Trace run_trace(Simulator &sim, Rv32iCoreTop &dut, unsigned cycles) {
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
// ① 固件自校验：编码器 vs 手算常量；固件长度与地址边界。
// ===========================================================================
TEST_CASE("Rv32iSoc - FirmwareEncoderCrossCheck", "[rv32i][soc][dual]") {
    // 手算常量（RV32I 规范，逐位核对过）—— 与 helper 交叉校验，避免自证
    REQUIRE(ADDI(1, 0, 1) == 0x00100093u);   // addi x1, x0, 1
    REQUIRE(ADDI(2, 0, 2) == 0x00200113u);   // addi x2, x0, 2
    REQUIRE(ADD(3, 1, 2) == 0x002081B3u);    // add x3, x1, x2
    REQUIRE(SUB(4, 3, 2) == 0x40218233u);    // sub x4, x3, x2
    REQUIRE(SW(4, 0, 0x100) == 0x10402023u); // sw x4, 0x100(x0)
    REQUIRE(SB(2, 0, 0x101) == 0x102000A3u); // sb x2, 0x101(x0)
    REQUIRE(SH(3, 0, 0x122) == 0x12301123u); // sh x3, 0x122(x0)
    REQUIRE(LW(5, 0, 0x100) == 0x10002283u); // lw x5, 0x100(x0)
    REQUIRE(BEQ(0, 0, 8) == 0x00000463u);    // beq x0, x0, +8
    REQUIRE(BNE(0, 0, 8) == 0x00001463u);    // bne x0, x0, +8
    // ⚠️ 跳转 / U 型 / SRA（2026-10-26 新增，T3）—— 同样手算交叉校验
    REQUIRE(JAL(14, 12) == 0x00C0076Fu);     // jal x14, +12
    REQUIRE(JALR(17, 18, 0) == 0x000908E7u); // jalr x17, 0(x18)
    REQUIRE(AUIPC(18, 0) == 0x00000917u);    // auipc x18, 0
    REQUIRE(SRA(22, 21, 2) == 0x402ADB33u);  // sra x22, x21, x2
    REQUIRE(NOP == 0x00000013u);

    const auto &fw = firmware();
    REQUIRE(fw.size() >= 20u);  // capability 要求 >= 20 条

    // TCM 是 12 位 => 程序地址上界 16 KB
    REQUIRE(fw.size() * 4u <= 0x1000u);
    REQUIRE(kGoldenAddrC < 0x1000u);
}

// ===========================================================================
// ② JIT 回落契约（change rv32i-soc-dual-parity-2026-10 · T6 / D8）
//
// ⚠️ RV32I 的 imem/dmem 用 `ch_mem` ⇒ 图里有 `type_mem_read_port` ⇒
//    JIT **必然回落**（`jit_ir_gen_lnode_state.cpp` 对它返回 UNSUPPORTED_OP）
//    ⇒ 这不是缺陷，而是**必须被显式断言的契约**：
//      ① 「回落」≠「未启用」（`is_jit_enabled()` 真但 `is_jit_compiled()` 假）
//      ② 回落**原因可归因**（日志含 `JIT skipped` + `mem_read_port`）
//      ③ 回落后的 trace 与**显式 interpreter** 的 trace 逐拍相同
// ===========================================================================
#if __has_include("jit/jit_compiler.h")
TEST_CASE("Rv32iSoc - JitFallbackContract",
          "[rv32i][soc][jit][verilator][e2e]") {
    // ---- 捕获 `std::cout`（`CHINFO` 走 `std::cout`，见 include/utils/logger.h）----
    // ⚠️ 捕获窗口 MUST 包住 **`Simulator` 的构造** —— 编译尝试发生在那里
    //    （`src/simulator.cpp:55`：构造函数里就 `try_jit_compile()`）⇒
    //    只包住后面的 `set_jit_enabled(true)` 会**漏掉**第一次尝试的日志。
    std::ostringstream captured;
    std::streambuf *saved = std::cout.rdbuf(captured.rdbuf());

    auto dev = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto sim = std::make_unique<Simulator>(dev->context());
    // 显式再触发一次（幂等；`jit_compiled_==false` 时会重新尝试）⇒
    // 保证「JIT 被启用过」这个事实与日志无关地成立。
    sim->set_jit_enabled(true);

    const std::string log = captured.str();
    std::cout.rdbuf(saved);  // 恢复

    // ---- ① 回落 ≠ 「未启用」 ----
    REQUIRE(sim->is_jit_enabled());
    REQUIRE_FALSE(sim->is_jit_compiled());

    // ---- ② 回落原因可归因 ----
    // ⚠️ 「mem_read_port」这个归因来自 D8：在 `generate_ir` 的**中心失败点**
    //    写 `last_error_msg_ = "unsupported node type: <type> (node_id=N)"` ⇒
    //    「mem read port」与「图太小」**可区分**（此前两者都只打印
    //    「small graph or unsupported op」⇒ 不可归因）。
    REQUIRE(log.find("JIT skipped") != std::string::npos);
    // ⚠️ 归因消息 MUST **非空**且指明「**哪个节点**、**多宽**/什么类型」——
    //    此前这个失败出口**不写** `last_error_msg_` ⇒ 原因**是空的**
    //    ⇒ 回落完全不可归因（实测）。
    // ⚠️ 断言 MUST 精确匹配归因**格式**，MUST NOT 用宽泛的子串
    //    （如 `find("mem_read_port")`）—— 那会命中**建图日志**里的
    //    「Created node ... (mem_read_port, 32 bits)」⇒ **假阳性**。
    // ⚠️ Catch2 不支持在断言里做链式比较 ⇒ 先算成局部 bool
    const bool has_skipped = log.find("JIT skipped: ") != std::string::npos;
    const bool reason_empty = log.find("JIT skipped: \n") != std::string::npos;
    const bool has_width_reason =
        log.find("unsupported node width: ") != std::string::npos;
    const bool has_type_reason =
        log.find("unsupported node type: ") != std::string::npos;
    REQUIRE(has_skipped);
    REQUIRE_FALSE(reason_empty);   // 原因 MUST 非空
    const bool has_reason = has_width_reason || has_type_reason;
    REQUIRE(has_reason);

    // ---- ③ 回落后的 trace == 显式 interpreter 的 trace（逐拍全等）----
    // ⚠️ **诚实说明**：当前实现下 ③ 是**恒真**的（JIT 回落 ⇒ 走的**就是**
    //    interpreter 路径）⇒ 它**此刻**没有判别力。
    //    ⇒ 它的价值是**前置护栏**：将来若 JIT 支持了 `type_mem_read_port`
    //    （⇒ 编译成功 ⇒ 走 JIT-native 路径），③ 仍 MUST 成立 ⇒
    //    **任何**「JIT-native 与 interpreter 不等价」的回归都会在此被抓。
    //    （与「DUAL_BACKEND_TEST_PAIR」类断言同性质：**为未来兜底**。）
    constexpr unsigned kCycles = 50;
    load_firmware(*sim, dev->instance());
    const Trace t_fallback = run_trace(*sim, dev->instance(), kCycles);

    auto dev_i = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto sim_i = std::make_unique<Simulator>(dev_i->context());
    sim_i->set_backend(std::make_unique<ch::InterpreterBackend>());
    load_firmware(*sim_i, dev_i->instance());
    const Trace t_interp = run_trace(*sim_i, dev_i->instance(), kCycles);

    REQUIRE(t_fallback.size() == t_interp.size());
    for (std::size_t c = 0; c < t_fallback.size(); ++c) {
        INFO("cycle " << c);
        REQUIRE(t_fallback[c] == t_interp[c]);
    }
}
#else
TEST_CASE("Rv32iSoc - JitFallbackContract",
          "[rv32i][soc][jit][verilator][e2e]") {
    WARN("CH_JIT_ENABLE=OFF：JIT 回落契约不适用（无 JIT 编译器）");
}
#endif

// ===========================================================================
// ③ 双路逐周期 parity + ISA 语义 golden（核心验收）。
// ===========================================================================
TEST_CASE("Rv32iSoc - DualBackendTraceParity",
          "[rv32i][soc][dual][verilator][e2e]") {
    // ⚠️ **2026-11-23（S3）**：D-Cache 接入后每处 load **miss** 要等 4-beat fill
    //    ⇒ ⇒ 程序的**完成时刻**后移 ⇒ ⇒ 窗口 MUST 放宽（否则 golden 序列被**截断**，
    //    断言退化成「窗口边界」而不是 ISA 语义）。260 拍 ⇒ 尾部仍余 ~30 拍。
    constexpr unsigned kCycles = 260;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
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
    // ⚠️ **取指填充期的两条不变量**（2026-10-27，ADR-048 D6 方案 1）：
    //   第 0 拍 IF/ID 装入的是**幻影**（复位前 `load_firmware` 的加载拍已把
    //   imem 的 sync 读端口预填成 word 0）——
    //     ① `dbg_id_pc` MUST **仍是** PC 复位值 `0x80000000`
    //        ⇒ 钉住「**只门控 `valid`**」这个选择：若改成「门控全部三个装载」
    //        ⇒ 第 0 拍的 `dbg_id_pc` 会是复位值 **0** ⇒ 本断言转红（= M6）；
    //     ② `dbg_id_valid` MUST 为 **0** ⇒ 幻影不进 ID/EX ⇒ 同一条指令
    //        只退休**一次**（缺陷 B 修复的直接观测）。
    //   ⚠️ 反向期望：修复前 ② 是 **1**（幻影真的执行了）。
    REQUIRE(ta[0][kIdxIdPc] == 0x80000000u);
    REQUIRE(ta[0][kIdxIdValid] == 0u);
    REQUIRE(ta[1][kIdxIdValid] == 1u);   // 第 1 拍是真取指的 word 0 ⇒ valid
    //
    // A: 纯 ALU 结果落 dmem（x4 = 13 - 3 = 10）
    REQUIRE(has_store(ta, kGoldenAddrA, kGoldenValA));
    // B: load 真的读回了值（x5 = [0x100] = 10）⇒ load 路径确实生效
    REQUIRE(has_store(ta, kGoldenAddrB, kGoldenValB));
    // C: 控制流走对分支（bne 未成立 ⇒ x8 = 2）
    REQUIRE(has_store(ta, kGoldenAddrC, kGoldenValC));
    // D: **字节写**（sb）—— store 拍上的写数据 MUST 已**lane 对齐**
    //    （3 << 8 = 0x300 ⇒ lane 1；未对齐时是 3 ⇒ 本断言转红）
    REQUIRE(has_store(ta, kGoldenAddrSB, kGoldenDataSB));
    // E: **半字写**（sh）—— 13 << 16 = 0xD0000 ⇒ lane 2-3
    REQUIRE(has_store(ta, kGoldenAddrSH, kGoldenDataSH));

    // ---- 被冲刷的指令 MUST **从未执行**（`JAL` / `JALR` 的错误路径）----
    //
    // 固件 word 66 = `addi x19, x0, 0x66` 位于 `JAL`（word 64）之后的**错误路径**；
    // word 82 = `addi x23, x0, 0x77` 位于 `JALR`（word 80）之后的**错误路径**。
    // ⇒ 两者 MUST NOT 被写回。
    // ⚠️ **位置纪律**（2026-10-27 第 1 轮 critic SHOULD_FIX #1）：这一组断言
    //    MUST 排在**派生计数器**（`control_stalls` 等）**之前** —— Catch2 的
    //    `REQUIRE` 失败会中止当前 `TEST_CASE` ⇒ 若排在后面，`control_stalls`
    //    会抢先失败 ⇒ 它们**永远不会被执行** ⇒ 丧失独立判别力
    //    （实测：变异「`ex_branch` 去掉 `is_jump`」只被 `control_stalls` 抓到）。
    // ⚠️ 这两条断言是「**`JAL`/`JALR` 也触发 flush**」的**唯一**判别力来源：
    //    `hazard.io().ex_branch` 若仍只接 `is_branch`（不含 `is_jump`）
    //    ⇒ PC 跳了、错误路径的指令却照常执行 ⇒ x19 / x23 被写 ⇒ 转红
    //    （2026-10-26 变异验证 M4 实测）。
    // ⚠️ 它们**只**由这两条指令写 ⇒ 「不出现」是强断言。
    for (std::size_t c = 0; c < ta.size(); ++c) {
        INFO("cycle " << c);
        REQUIRE_FALSE((ta[c][kIdxWbWriteEn] == 1u &&
                       ta[c][kIdxWbRdAddr] == 19u));
        REQUIRE_FALSE((ta[c][kIdxWbWriteEn] == 1u &&
                       ta[c][kIdxWbRdAddr] == 23u));
        // ⚠️ word 101 = **错误路径上的 JAL** ⇒ 它 MUST NOT 写回 x27
        //    （若它执行了 ⇒ PC 已被它劫持 ⇒ x28 也不会写回）
        REQUIRE_FALSE((ta[c][kIdxWbWriteEn] == 1u &&
                       ta[c][kIdxWbRdAddr] == 27u));
    }

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
        REQUIRE(store_cycles == 5u);
    }

    // 反向：错误的值 MUST NOT 出现（防止「碰巧出现过一次」也算过）
    REQUIRE_FALSE(has_store(ta, kGoldenAddrA, kGoldenValA + 1));
    REQUIRE_FALSE(has_store(ta, kGoldenAddrC, kGoldenValC + 1));
    // ⚠️ **未 lane 对齐的写数据 MUST NOT 出现**（2026-10-26 新增）：
    //    「掩码对了、数据在错 lane」是最隐蔽的错误 ⇒ 必须显式排除。
    REQUIRE_FALSE(has_store(ta, kGoldenAddrSB, 3u));
    REQUIRE_FALSE(has_store(ta, kGoldenAddrSH, 13u));

    // ---- 分支 / 停顿计数 ----
    // 固件里恰好 2 条分支（beq + bne）⇒ branch_count == 2
    // ⚠️ **口径**（2026-10-27 第 1 轮 critic NIT #2）：`perf_branch_count` 只统计
    //    **条件分支**（`id_stage.is_branch` = `OP_BRANCH`）⇒ `JAL`/`JALR` **不计入**。
    //    这是定义性选择（跳转不是「分支」），也已写进 spec delta。
    REQUIRE(ta.back()[kIdxBranchCount] == 2u);
    // 只有 taken 的 beq 被判为误预测（bne 不成立）⇒ mispredict == 1
    REQUIRE(ta.back()[kIdxMispredict] == 1u);
    // ⚠️ **load-use 停顿拍数 == 8**（2026-11-13 · ADR-069 决策 **A0** 落地后订正；
    //    原值 6 是 **A0 之前**的基线）：
    //    固件里有**两处**紧邻的 load→use：
    //      ① `lw x5` 紧接着 `add x6,x5,x5`（word 20/21）
    //      ② `lw x10` 紧接着 `add x11,x10,x10`（word 43/44）
    //    ⇒ 每处停顿 **4 拍**（**A0 后**：load 从 EX 到 WB 是 **3** 级
    //    ⇒ 数据在「检测拍 + 3」的边沿写回 ⇒ 消费者 MUST 等到「检测拍 + 4」）
    //    ⇒ 合计 **8 拍**（A0 前是 2 级 ⇒ 3 拍/处 ⇒ 合计 6 拍）。
    //    ⚠️ 修复前这个计数器**恒为 0**（检测用的是 MEM 级 load ⇒ 晚一级
    //    ⇒ 永远对不上）⇒ 本断言是「冒险真的被检测到」的判别力来源。
    //    ⚠️ **手推依据** = `openspec/changes/memory-timing-contract-2026-11/design.md`
    //    §10.2（分段分解：fill 3 + **stall 8** + flush 12 = 23）+ §10.4。
    // ⚠️⚠️ **2026-11-23（S3 · D-Cache 接入）后订正 8 → 2**（手推，不是抄波形）：
    //    两处 load→use 的 `lw` 现在**都 miss**（0x100 / 0x110 两行都是冷行 ——
    //    之前的 `sw` 是 write-through，**不填行**）⇒ ⇒ 保持拍被分成两段：
    //      · **检测拍**：load 在 EX、MEM 级里是**非访存**指令 ⇒ ⇒ `mem_stall=0`
    //        ⇒ ⇒ 这 **1 拍**计入 `perf_load_use_stalls`；
    //      · 其余保持拍：load 已进 MEM 且 miss ⇒ ⇒ `mem_stall=1`
    //        ⇒ ⇒ 按 S2 的口径（`& !mem_stall`）归入**内存反压**，不计入本计数器。
    //    ⇒ ⇒ **每处 1 拍** ⇒ 合计 **2**。
    //    ⚠️ 判别力不变：修复前本计数器**恒为 0**（检测用的是 MEM 级 load ⇒ 晚一级）
    //    ⇒ ⇒ 「冒险真的被检测到」仍由本断言钉住（0 依旧转红）。
    //    ⚠️ 若将来这两条 load 变成 **hit**（例如固件先读后写）⇒ ⇒ 手推值回到 **8**
    //    （4 拍/处）；本断言 MUST 随之重推，**不得**盲改数字。
    // ⚠️ **2026-11-23（S3 · critic SHOULD_FIX #1）后 2 → 6**：word 108/109 补了一条
    //    **hit 形态**的 load→use ⇒ 它**不**被内存反压掩盖 ⇒ 完整 **4 拍**计入
    //    ⇒ ⇒ 2 + 4 = **6**（两处 miss 形态各 1 + 新增 1 处 hit 形态 4）。
    REQUIRE(ta.back()[kIdxLoadUseStalls] == 6u);
    // 控制停顿 = **全部控制重定向**的**次数**（2026-10-26 起含 JAL/JALR）
    //   = 1（taken 的 beq 误预测）+ 3（JAL @word64 + JALR @word80 + JAL @word100）
    // ⚠️ 口径是「**重定向次数**」而不是「停顿拍数」—— 每次重定向实际代价是 **3 拍**
    //    气泡（实测 `non_retired` 每个跳转 +3），计数器只 +1。
    // ⚠️ 与 `mispredict` **分离**：`mispredict` 只统计**条件分支**的误预测
    //    （JAL/JALR 是**无条件**跳 ⇒ 不是「预测错误」），而跳转同样让流水线空拍
    //    ⇒ 计入 `control_stalls` 但不计入 `mispredict`。
    REQUIRE(ta.back()[kIdxControlStalls] == 4u);

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
    // 退休数在 **cyc 1..199** 这 199 个拍间隔里有 **21** 个「不增长」，
    // 分七段：
    //   ① **3 拍填充**（cyc 1..3）：IF/ID → ID/EX → EX/MEM → MEM/WB 依次就绪。
    //      ⚠️ **2026-10-27 起是 3 拍（原 2 拍）**：`primed` 门控（ADR-048 D6）
    //      把复位后**第 0 拍**的幻影装载的 `valid` 压成 0
    //      ⇒ 第一条指令进 ID/EX 晚一拍 ⇒ **fill 段 +1**（这就是缺陷 B 的 −1）
    //   ② 4 拍 **load-use 停顿 #1**（消费者留在 ID 级；EX/MEM 与 MEM/WB 灌气泡）
    //      ⚠️ **2026-11-13 · ADR-069 A0**：3 拍 → **4 拍**
    //         （EX→WB 级数 2 → 3 ⇒ 数据晚 1 拍写回 ⇒ 消费者多等 1 拍）
    //   ③ 3 拍 **冲刷气泡**（taken 分支让出错误路径 + 重新取指目标）
    //      ⚠️ **3** 拍：flush MUST **多杀一拍**（imem 是 1-cycle `sread` ⇒
    //      flush 当拍提交的那条在下一拍才返回 ⇒ `flush_d1` 再门一拍）⇒
    //      代价 = 分支目标的指令晚 1 拍进 ID
    //   ④ 4 拍 **load-use 停顿 #2**（第二处紧邻 load→use）
    //   ⑤ 3 拍 **`JAL` 冲刷**（word 64；**与条件分支同构** ⇒ 同样 3 拍）
    //   ⑥ 3 拍 **`JALR` 冲刷**（word 80）
    //   ⑦ 3 拍 **`JAL` 冲刷**（word 100 —— 「错误路径上的跳转」用例）
    //   ⇒ **填充 3 + 停顿 8 + 冲刷 12 = 23** ⇒ 199 − 23 = **176** 条指令退休
    //
    // ⚠️ **2026-11-13 · ADR-069 A0 落地后的订正**（原值 21 / 178）：
    //    ① **fill 段仍是 3** —— 退休计数的增量源 = `mem_stage.io().mem_valid_out`
    //       = `exmem_valid`（**MEM 级**指令有效）⇒ ⇒ M2 在 MEM 的**下游**
    //       ⇒ ⇒ **退休时点不动** ⇒ ⇒ fill 段**不随** MEM 区级数变化
    //       （⚠️ **最容易搞错的一格**：若把增量源改成 M2 / MEM/WB′ 的 valid
    //       ⇒ ⇒ fill 会变成 4 ⇒ ⇒ 而那**违反** capability `rv32i-commit-path`
    //       的「「本拍有一条指令退休」MUST 等价于「MEM 级持有**有效**指令」」）
    //    ② **stall 段 6 → 8**（每处 3 拍 → **4** 拍 · ADR-069 §2.1 / §2.3 代价③）
    //    ③ **flush 段仍是 12**（重定向次数不变 ⇒ 每次仍是 3 拍 ⇒ M2 在其下游）
    //    ⇒ ⇒ **手推依据** = `openspec/changes/memory-timing-contract-2026-11/design.md`
    //    §10.2（分段分解）+ §10.4（**只改这 3 个数**）
    //
    // ⚠️ **分段不变式**（ADR-048 D9.2）：取指 skid buffer **不改**停顿拍数与
    //    冲刷拍数（仍各 3）⇒ ②④ 与 ③⑤⑥⑦ **逐段不变**，只有 ① 随 `primed` 变。
    //    实测（2026-10-27）：skid 落地（C1）后本段三处**全部不变**
    //    （non_retired 仍 20 / 退休数仍 179 —— skid 救回一条、同时消掉一次
    //    **重复装载**，两者在计数上相抵；详见 design.md §1.3）；
    //    `primed`（C2）落地后只多出 cyc 3 这一拍 ⇒ 21 / 178。
    //
    // ⚠️ 「不增长的间隔数」与「退休数」**分别断言** ⇒ 若流水深度变了，
    //    两条断言**一起**转红（不会「一条变、一条不变」而掩盖）。
    // ⚠️ ⑤/⑥ 是**2026-10-26 实测**（不是推导）：加入两个跳转后
    //    `non_retired` 从 11 变为 17（各 +3）⇒ 与条件分支的冲刷代价一致。
    // ⚠️⚠️ **2026-11-23（S3 · D-Cache 接入）后订正 23 → 41** —— 分段分解：
    //
    //   **填充 3 + 冲刷 12 + miss 3×8 + load-use 保持 2 = 41**
    //
    //   ① **fill 3** —— 不变（`primed` 门控的代价，见上）
    //   ② **冲刷 12** —— 不变（4 次重定向 × 3 拍；重定向次数与存储器无关）
    //   ③ **miss 3 × 8 = 24** —— 固件里 3 条 load 落在**冷行**：
    //      `lw x5,0x100` · `lw x10,0x110` · `lw x13,0x120`
    //      （`lw x12,0x100` **命中** —— 行已被 `lw x5` 的 fill 铺满，且 `sb` 只改字节）
    //      ⇒ 每次 miss 的保持拍 = **8**（手推：AR 在请求拍握手 ⇒ 下一拍 slave 进
    //      `r_active` ⇒ TCM `sread` 1 拍 + `r_pending` 1 拍 ⇒ **2 拍/beat**
    //      × 4 beat = 8；`r_last` 那拍即 `fill_complete` ⇒ 当拍解冻）
    //   ④ **load-use 保持 2** —— 两处紧邻 load→use，每处只有**检测拍**是
    //      「纯冒险」保持（其余保持拍与 ③ 的 miss 重叠 ⇒ 已计入 ③）
    //      ⇒ 与下面的 `kIdxLoadUseStalls == 2` 是**同一条手推**
    //
    //   逐段实测（S3 落地后）：`1..3`(3) `24..33`(10=8 miss + 1 保持 + …)
    //   `43..45`(3) `58..67`(10) `84..91`(8) `97..99`(3) `114..116`(3) `135..137`(3)
    //   ⇒ 合计 **41** ✓
    //   ⚠️ ③ 的 **8 拍/miss** 是**实现属性**（slave 的 beat 节奏）⇒ 若将来把
    //   slave 改成 1 拍/beat ⇒ 本常数 MUST 按 ③ 重推（**不得**盲改数字）。
    std::size_t non_retired = 0;
    for (std::size_t c = 1; c < ta.size(); ++c) {
        if (ta[c][kIdxInstructions] == ta[c - 1][kIdxInstructions]) {
            ++non_retired;
        }
    }
    // 41 + 4（新增的 hit 形态 load-use 保持 4 拍）= **45**
    REQUIRE(non_retired == 45u);
    // 退休数 = 采样拍数 − (非退休拍 + 第 0 拍那条已退休的差)
    REQUIRE(ta.back()[kIdxInstructions] == kCycles - 46u);

    // ---- 写回序列（ISA 语义的最强判据）----
    //
    // 逐拍收集所有写回 `(rd, data)`，与固件的**期望序列**逐项比对。
    //
    // ⚠️ **不再去重**（2026-10-27，ADR-048 D11）：此前的「连续重复去重」
    //    是为**缺陷 B**（取指填充期同一条指令被装两次 ⇒ 幂等地写回两次）
    //    设计的；`primed` 门控（C2）修掉缺陷 B 之后，**任何**连续重复
    //    都是**真回归** ⇒ 掩码若留着会反过来把它掩盖。
    //
    // ⚠️ 为什么必须是**序列**而不是计数：
    //    「多一次假退休」会让「退休数」与「写回次数」**同步** +1
    //    ⇒ 任何**计数**断言都抓不到（变异验证实测：`exmem_valid` 改回取
    //    ID 级的 valid ⇒ 计数不变 ⇒ 零信号）⇒ 只有序列能抓。
    //
    // 期望序列（固件的**全部**写回，按程序顺序）：
    //   x1=10, x2=3, x3=13, x4=10, x5=10, x6=20, x8=1, x9=2, ...
    //   （word 29 的 BEQ 被 taken ⇒ 冲刷 word 30 的 `addi x7,x0,0x7f`
    //    ⇒ **x7 MUST NOT 出现**）
    {
        // ⚠️ **2026-11-23（S3）口径修订**：D-Cache 接入后 MEM 级多拍
        //    ⇒ ⇒ 「MEM/WB 寄存器 MUST 被冻结」（`rv32i-memory-timing`）
        //    ⇒ ⇒ `wb_write_en` 是**电平**，同一条指令会在 WB **停留多拍**
        //    ⇒ ⇒ 逐拍收集会重复计数 ⇒ ⇒ 折掉**连续相同** `(rd, data)`。
        //    ⚠️ 该折算会掩盖「同一条指令连续两拍写回**同一个** (rd,data)」这一形态
        //    ⇒ ⇒ 该形态由**退休总数**（上面的 `kCycles - 42`）与
        //    `dbg_load_use_detected` 计数共同覆盖（多退休一条 ⇒ 总数 +1 ⇒ 转红）。
        std::vector<std::pair<uint64_t, uint64_t>> wb;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] != 1u) {
                continue;
            }
            const std::pair<uint64_t, uint64_t> ev{ta[c][kIdxWbRdAddr],
                                                   ta[c][kIdxWbWriteData]};
            if (!wb.empty() && wb.back() == ev) continue;
            wb.push_back(ev);
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
    // ⇒ 它曾被「连续重复去重」**掩盖**了（去重是为填充期残留设计的 —— 该掩码
    //    已于 2026-10-27 随缺陷 B 的修复**删除**，ADR-048 D11）
    // ⇒ 仍保留本**计数**断言：把「只写一次」写成显式的 liveness 判据
    //    （序列断言已覆盖，但计数断言的失败信息更直接）。
    {
        // ⚠️ **2026-11-23（S3）**：`wb_write_en` 是电平（MEM/WB 冻结 ⇒ 同一条指令
        //    可停留多拍）⇒ ⇒ 计数改为**连续段数**（1 段 = 1 次写回事务）。
        bool in_run = false;
        std::size_t x8_writes = 0;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            const bool hit = (ta[c][kIdxWbWriteEn] == 1u &&
                              ta[c][kIdxWbRdAddr] == 8u &&
                              ta[c][kIdxWbWriteData] == 1u);
            if (hit && !in_run) ++x8_writes;
            in_run = hit;
        }
        REQUIRE(x8_writes == 1u);
    }

    // ---- 冒险**次数**（与「停顿**拍数**」区分）----
    // `dbg_load_use_detected` 是**单拍脉冲** ⇒ 数它的脉冲个数 = 冒险次数。
    // 固件里有**三处**紧邻的 load→use（word 20/21 · 43/44 · **108/109**）⇒ 恰好 3 次。
    std::size_t hazards = 0;
    for (std::size_t c = 0; c < ta.size(); ++c) {
        if (ta[c][kIdxLoadUseDetected] == 1u) {
            ++hazards;
        }
    }
    REQUIRE(hazards == 3u);

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

// ===========================================================================
// ④ 判别性固件（ADR-047 D9）—— 数据前推的**判别性**证据
//
// ⚠️ **为什么是独立的一套固件**（2026-10-27）：
//    既有固件（`firmware()`，108 字）**刻意无冒险** ⇒ 前推接错 / 接反 / 没接
//    **全仓照绿** ⇒ 它对前推是**零信号**的。
//    ⇒ 本套固件**专门**制造冒险（生产者与消费者之间**无间隔**）。
//    ⇒ 独立成一套 ⇒ 既有固件的 golden（24 条写回序列 / 停顿计数 / 退休数）
//    **零变化** ⇒ 「既有 golden 的变化 MUST 全部可归因」⇒ 本次归因 = **零**。
//
// ---------------------------------------------------------------------------
// 固件布局（word N 的地址 = 0x80000000 + 4N；立即数一律**字节**为单位）
//
//   w0   ADDI x16, x0, 0x11      x16 = 17
//   w1   ADD  x17, x16, x0       x17 = 17        ← ① 相邻 ALU→ALU（**无 NOP**）
//   w2   ADDI x18, x0, 0x2A      x18 = 42
//   w3   NOP
//   w4   NOP
//   w5   NOP
//   w6   SW   x18, 0(x0)         [0x130] = 42
//   w7   NOP
//   w8   LW   x19, 0(x0)         x19 = 42
//   w9   NOP                                    ← 间隔 1 条 ⇒ **距离 2**
//   w10  ADD  x20, x19, x0       x20 = 42        ← ② 距离 2 的 load→use
//   w11  NOP
//   w12  NOP
//   w13  LW   x21, 0(x0)         x21 = 42
//   w14  NOP                                    ← 间隔 2 条 ⇒ **距离 3**
//   w15  NOP
//   w16  ADD  x22, x21, x0       x22 = 42        ← ③ 距离 3 的 load→use
//   w17  ADDI x23, x0, 1         x23 = 1
//   w18  BEQ  x23, x0, +8        **不成立**（x23 = 1）⇒ 落到 w19
//   w19  ADDI x24, x0, 0x33      x24 = 51        ← ④ 分支条件用紧邻新值
//   w20  ADDI x25, x0, 0xAB      x25 = 0xAB
//   w21  SW   x25, 0(x0)         [0x134] = 0xAB  ← ⑤ store 的 **rs2** 紧邻被写
//   w22  ADDI x26, x0, 0x11      x26 = 17
//   w23  ADDI x26, x0, 0x22      x26 = 34        （**双生产者同 rd**）
//   w24  ADD  x27, x26, x0       x27 = 34        ← ⑥ 档位优先级（取**较新**的）
//   w25  AUIPC x28, 0            x28 = 0x80000064（= w25 的地址）
//   w26  ADDI x28, x28, 16       x28 = 0x80000074（= w29 的地址）← **紧邻被写**
//   w27  JALR x29, x28, 0        x29 = 0x80000070（link）；跳到 w29
//   w28  NOP                                    ← 错误路径（MUST 被冲刷）
//   w29  ADDI x30, x0, 0x44      x30 = 68        ← ⑦ `JALR` 的 `rs1` 紧邻被写
//   w30  ADDI x0, x0, 0x55       写 x0 ⇒ MUST 被忽略
//   w31  ADD  x31, x0, x0        x31 = 0         ← ⑧ `x0` 负向护栏
//
// ⚠️⚠️ **寄存器编号 MUST 在 x0..x31**（RV32I 只有 32 个寄存器）——
//    初稿用了 x32..x45 ⇒ 译码器把 rd 按 **5 位**截断（x32 → x0、x33 → x1…）
//    ⇒ 固件**静默**变成另一条程序（实测：`addi x32,x0,42` 被译成
//    `addi x0,x0,42`）⇒ **整条判别性固件失效**（零信号）。
//    ⇒ 故 MUST 只用 x16..x31（且与既有固件的 x1..x28 不冲突 ——
//      两套固件是**独立**的程序，互不影响）。
//
// ---------------------------------------------------------------------------
// **每个**用例的「反向期望」（未接前推 / 缺旁路时会读到的值）：
//
//   ① x17 : 无前推 ⇒ x16 在寄存器堆里还是 0 ⇒ x17 = **0**   ⇒ 与 17 不同 ✔
//   ② x20 : 无前推 ⇒ x19 未写回 ⇒ x20 = **0**                 ⇒ 与 42 不同 ✔
//   ③ x22 : 无 D11 旁路 ⇒ ID/EX 与 WB 写回**同沿** ⇒ x22 = **0** ⇒ 与 42 不同 ✔
//   ④ x24 : 无前推 ⇒ x23 = 0 ⇒ BEQ **成立** ⇒ w19 被冲刷 ⇒ x24 **不出现** ✔
//   ⑤ [0x134] : 无前推 ⇒ store 的 rs2 = 0 ⇒ [0x134] = **0**   ⇒ 与 0xAB 不同 ✔
//   ⑥ x27 : 无前推 ⇒ x26 未写回 ⇒ x27 = **0**                ⇒ 与 34 不同 ✔
//   ⑦ x30 : 无前推 ⇒ x28 = 0 ⇒ JALR 跳到地址 0 ⇒ 程序**跑飞**
//        ⇒ 固件的**后半段不再按预期执行** ⇒ x30 与 x29 的**写回序列**改变
//        （第二趟的 link 值不同 ⇒ `expect_written(29,…)` 的「只被写一个值」
//          这条断言先转红）✔
//   ⑧ x31 : **不是**「未接前推」的判别（两种实现都给 0）⇒ 它是
//           「**去掉 `rd_nz` 门控**」的判别：那样 x31 会是 **0x55** ⇒ 与 0 不同 ✔
//           ⇒ MUST 如实标注：**负向护栏**，不是「前推是否接通」的判别。
//
// ⚠️ **②③ 的前置（[0x130] = 42）刻意与前推无关**：`SW x18` 与 `ADDI x18`
//   之间隔了 **3** 条 ⇒ 距离 4 ⇒ 走**寄存器堆路径** ⇒ 不依赖前推
//   ⇒ ②③ 的判别力**只**来自「消费者是否拿到 load 的**新值**」。
//
// ⚠️ **首条指令 MUST NOT 自依赖**（取指填充期「同一条指令被装两次」的
//    非幂等前提）：w0 的 `ADDI x16, x0, 0x11` 写 x16、读 x0 ⇒ 安全。
constexpr std::size_t kFwdFirmwareWords = 32;

constexpr std::array<uint32_t, kFwdFirmwareWords> fwd_firmware() {
    return {{
        ADDI(16, 0, 0x11),   // 0   x16 = 17
        ADD(17, 16, 0),      // 1   x17 = 17   ← ①
        ADDI(18, 0, 0x2A),   // 2   x18 = 42
        NOP,                 // 3
        NOP,                 // 4
        NOP,                 // 5
        SW(18, 0, 0x130),    // 6   [0x130] = 42
        NOP,                 // 7
        LW(19, 0, 0x130),    // 8   x19 = 42
        NOP,                 // 9   ← 间隔 1 条 ⇒ 距离 2
        ADD(20, 19, 0),      // 10  x20 = 42  ← ②
        NOP,                 // 11
        NOP,                 // 12
        LW(21, 0, 0x130),    // 13  x21 = 42
        NOP,                 // 14  ← 间隔 2 条 ⇒ 距离 3
        NOP,                 // 15
        ADD(22, 21, 0),      // 16  x22 = 42  ← ③
        ADDI(23, 0, 1),      // 17  x23 = 1
        BEQ(23, 0, 8),       // 18  **不成立** ⇒ 落到 19（成立则跳到 20）
        ADDI(24, 0, 0x33),   // 19  x24 = 51  ← ④
        ADDI(25, 0, 0xAB),   // 20  x25 = 0xAB
        SW(25, 0, 0x134),    // 21  [0x134] = 0xAB  ← ⑤
        ADDI(26, 0, 0x11),   // 22  x26 = 17
        ADDI(26, 0, 0x22),   // 23  x26 = 34
        ADD(27, 26, 0),      // 24  x27 = 34  ← ⑥
        AUIPC(28, 0),        // 25  x28 = 0x80000064
        ADDI(28, 28, 16),    // 26  x28 = 0x80000074
        JALR(29, 28, 0),     // 27  x29 = link；跳到 w29
        NOP,                 // 28  错误路径
        ADDI(30, 0, 0x44),   // 29  x30 = 68  ← ⑦
        ADDI(0, 0, 0x55),    // 30  写 x0 ⇒ MUST 被忽略
        ADD(31, 0, 0),       // 31  x31 = 0   ← ⑧
    }};
}

// 判别性固件的 golden（**手工推导**，推导过程见上方注释）
// ⚠️ `x29` = w27 的 link = w27 的 pc + 4 = 0x80000000 + 4*27 + 4 = 0x80000070
// ⚠️ `x28` = 0x80000000 + 4*25 + 16 = 0x80000074（= w29 的地址 ⇒ JALR 的目标）
// ⚠️ `x30` = 0x44 = 68
constexpr uint32_t kFwdBase = 0x80000000u;
constexpr uint32_t kFwdX29Link = kFwdBase + 4u * 27u + 4u;    // 0x80000070
constexpr uint32_t kFwdX28Target = kFwdBase + 4u * 25u + 16u;  // 0x80000074
constexpr uint32_t kFwdX30Body = 0x44u;                        // 68
constexpr uint32_t kFwdMemAddr = 0x130u;   // ②③ 的 load 源
constexpr uint32_t kFwdMemVal = 0x2Au;     // 42
constexpr uint32_t kFwdStoreAddr = 0x134u; // ⑤ 的 store 目标
constexpr uint32_t kFwdStoreData = 0xABu;  // 0xAB

// ===========================================================================
// 判别性固件的**编译期**纪律
// ===========================================================================

// ===========================================================================
// 判别性测试
// ===========================================================================
TEST_CASE("Rv32iSoc - ForwardingDiscriminatingCases",
          "[rv32i][soc][forwarding][dual][verilator][e2e]") {
    constexpr unsigned kCycles = 80;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

#if __has_include("jit/jit_compiler.h")
    sim_a->set_jit_enabled(false);
#endif
    sim_a->set_backend(std::make_unique<InterpreterBackend>());

    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_fwd");
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

    load_firmware(*sim_a, dev_a->instance(), fwd_firmware());
    load_firmware(*sim_b, dev_b->instance(), fwd_firmware());

    const Trace ta = run_trace(*sim_a, dev_a->instance(), kCycles);
    const Trace tb = run_trace(*sim_b, dev_b->instance(), kCycles);

    // ---- ① 双后端逐拍全等（**无掩码**）----
    // ⚠️ **比对端口清单 = `kObsNames`**（全部 27 个核 IP 顶层输出）
    //    ⇒ 「无掩码」是**可执行**的（不是「比对设计内部的一切」——
    //    顶层观测面是**有限**的 ⇒ 不写清单 ⇒ 判据不可执行）。
    REQUIRE(ta.size() == tb.size());
    for (std::size_t c = 0; c < ta.size(); ++c) {
        for (std::size_t k = 0; k < kObsCount; ++k) {
            INFO("cycle " << c << " obs " << kObsNames[k] << " (interp="
                          << ta[c][k] << ", verilator=" << tb[c][k] << ")");
            REQUIRE(ta[c][k] == tb[c][k]);
        }
    }

    // ---- ② 收集写回值（rd → 该 rd 被写过的**所有**值）----
    //
    // ⚠️ 用「集合」而不是「序列」⇒ 对「取指填充期同一条指令被装两次」
    //    （design.md §3.6，已知残留、幂等）**免疫**。
    std::map<uint64_t, std::set<uint64_t>> written;
    for (std::size_t c = 0; c < ta.size(); ++c) {
        if (ta[c][kIdxWbWriteEn] != 1u) continue;
        written[ta[c][kIdxWbRdAddr]].insert(ta[c][kIdxWbWriteData]);
    }

    auto expect_written = [&written](uint64_t rd, uint64_t value) {
        INFO("rd=" << rd << " expected value=" << value);
        const auto it = written.find(rd);
        REQUIRE(it != written.end());      // MUST 被写回过
        REQUIRE(it->second.size() == 1u);  // 且**只**被写成这一个值
        REQUIRE(*it->second.begin() == value);
    };

    // ---- ③ 逐个判别性用例 ----
    // ① 相邻 ALU→ALU（无 NOP）—— 档位 1
    expect_written(16, 0x11u);
    expect_written(17, 0x11u);   // 反向期望：无前推时是 0

    // ② 距离 2 的 load→use —— 档位 2
    expect_written(18, kFwdMemVal);
    expect_written(19, kFwdMemVal);
    expect_written(20, kFwdMemVal);   // 反向期望：无前推时是 0

    // ③ 距离 3 的 load→use —— D11 的读点旁路
    expect_written(21, kFwdMemVal);
    expect_written(22, kFwdMemVal);   // 反向期望：无旁路时是 0（同沿竞态）

    // ④ 分支条件用紧邻新值
    expect_written(23, 1u);
    expect_written(24, 0x33u);   // 反向期望：无前推时 BEQ 成立 ⇒ x24 不出现

    // ⑤ store 的写数据（rs2）紧邻被写 —— mux 的 b 路
    expect_written(25, kFwdStoreData);
    REQUIRE(has_store(ta, kFwdStoreAddr, kFwdStoreData));
    REQUIRE_FALSE(has_store(ta, kFwdStoreAddr, 0u));  // 反向期望：0

    // ⑥ 双生产者同 rd —— 档位优先级（取**较新**的那个）
    {
        INFO("x26 被写过两个值（17 然后 34）");
        const auto it = written.find(26);
        REQUIRE(it != written.end());
        REQUIRE(it->second.size() == 2u);
        REQUIRE(it->second.count(0x11u) == 1u);
        REQUIRE(it->second.count(0x22u) == 1u);
    }
    expect_written(27, 0x22u);   // 反向期望：无前推时是 0

    // ⑦ `JALR` 的 `rs1` 紧邻被写
    //    ⚠️ x28 被**写两次**（AUIPC 写基址、ADDI 写基址+16）⇒ 断言两个值
    //    都在，且**最后一个**是 JALR 的目标地址。
    {
        INFO("x28 被写过两个值（AUIPC 的基址，然后 ADDI 的目标）");
        const auto it = written.find(28);
        REQUIRE(it != written.end());
        REQUIRE(it->second.size() == 2u);
        REQUIRE(it->second.count(kFwdBase + 4u * 25u) == 1u);   // 0x80000064
        REQUIRE(it->second.count(kFwdX28Target) == 1u);          // 0x80000074
    }
    expect_written(29, kFwdX29Link);
    expect_written(30, kFwdX30Body);   // 反向期望：无前推时 JALR 跳到 0 ⇒ x30 不出现

    // ⑧ `x0` 负向护栏（**不是**「前推是否接通」的判别 ⇒ 是「去掉 `rd_nz` 门控」
    //    的判别：那样 x31 会是 0x55）
    expect_written(31, 0u);

    // ---- ④ 错误路径 MUST 被冲刷（w28 是 JALR 之后的错误路径）----
    //    w28 是 NOP ⇒ 无写回 ⇒ 「w29 之后**继续**执行」由 x30 的写回见证
    //    （上面已断言）。
    //    ⇒ 额外断言：`JALR` 的 link（x29）**只**被写一次（不被重复写回）。
    {
        std::size_t x29_writes = 0;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] == 1u && ta[c][kIdxWbRdAddr] == 29u) {
                ++x29_writes;
            }
        }
        REQUIRE(x29_writes == 1u);
    }
}

// ===========================================================================
// ⑤ 判别性固件（ADR-048 D9.1 / D9.4）—— 取指 skid buffer 与填充期语义的
//    **判别性**证据
//
// ⚠️ **为什么是独立的一套固件**（2026-10-27，change
//    `rv32i-fetch-skid-buffer-2026-10`）：
//    既有固件（`firmware()`，108 字）在**两处停顿点之后都放 NOP** ⇒ 缺陷 A
//    （丢弃消费者之后的第一条指令）与缺陷 B（填充期双装）**全仓照绿**
//    ⇒ 它对这两个缺陷是**零信号**的。本套固件故意在两处都放**会写寄存器 /
//    会 store** 的指令 ⇒ 「丢了」与「重了」都直接可见。
//
// ---------------------------------------------------------------------------
// 固件布局（word N 的地址 = 0x80000000 + 4N；立即数一律**字节**为单位）
//
//   w0   ADDI x16, x16, 1      x16 = 0 + 1 = 1     ← **自依赖**（缺陷 B 的判别）
//   w1-3 NOP
//   w4   ADDI x21, x0, 0x33    x21 = 0x33
//   w5-7 NOP                                        ← 距离 4 ⇒ 寄存器堆路径
//   w8   SW   x21, 0x140       [0x140] = 0x33
//   w9   NOP
//   w10  LW   x17, 0x140       x17 = 0x33
//   w11  ADD  x18, x17, x0     x18 = 0x33          ← **紧邻消费者** ⇒ 触发停顿
//   w12  ADDI x19, x0, 0x5A    x19 = 0x5A          ← **停顿点之后那条**（缺陷 A）
//   w13  ADDI x20, x0, 0xA5    x20 = 0xA5          ← **停顿后第一条**（重复装载）
//   w14-15 NOP
//   w16  SW   x19, 0x144       [0x144] = 0x5A      ← 救回的指令**真的执行到 MEM**
//   w17-19 NOP
//   w20  SW   x20, 0x148       [0x148] = 0xA5      ← 重复装载会写**两次**
//
// ---------------------------------------------------------------------------
// **反向期望**（每条都是「未修时会读到的值」）：
//
//   ① `x16`：填充期双装 ⇒ 第二条 word0 与第一条**紧邻**（指令距离 1）
//      ⇒ 由**档位 1**（EX/MEM）前推 ⇒ 读到第一条写入的 1 ⇒ `x16 = **2**`
//      ⇒ 与 1 不同 ⇒ 「`x16 == 1` 且**不出现** 2」是缺陷 B 的判别断言；
//      若不修则写回序列变成 `(16,1), (16,2), …` ⇒ 序列断言也转红。
//   ② `x19`：该指令被**丢弃**（其地址已越过 `pc_reg`，不会重读）
//      ⇒ `written` 里**没有** 19 ⇒ `expect_written(19, …)` 的「MUST 被写回过」转红。
//   ③ `[0x144] = 0x5A`：同上，store 拍**不出现**。
//   ④ `x20` 的写回**次数**：停顿后的**重复装载**（同一 word 连续两拍进 IF/ID，
//      两拍 `valid` 都是 1）⇒ 该指令被执行**两次** ⇒ 写回**两次**。
//      ⇒ 「恰好 1 次」是重复装载的判别断言（值相同 ⇒ 序列去重/集合断言都抓不到）。
// ===========================================================================

constexpr std::size_t kSkidFirmwareWords = 24;

constexpr std::array<uint32_t, kSkidFirmwareWords> skid_firmware() {
    return {{
        ADDI(16, 16, 1),     // 0   x16 = 1（自依赖：双装时 = 2）
        NOP,                 // 1
        NOP,                 // 2
        NOP,                 // 3
        ADDI(21, 0, 0x33),   // 4   x21 = 0x33
        NOP,                 // 5
        NOP,                 // 6
        NOP,                 // 7
        SW(21, 0, 0x140),    // 8   [0x140] = 0x33
        NOP,                 // 9
        LW(17, 0, 0x140),    // 10  x17 = 0x33
        ADD(18, 17, 0),      // 11  x18 = 0x33  ← 紧邻消费者 ⇒ 停顿
        ADDI(19, 0, 0x5A),   // 12  x19 = 0x5A  ← **停顿点之后**（缺陷 A）
        ADDI(20, 0, 0xA5),   // 13  x20 = 0xA5  ← **停顿后第一条**（重复装载）
        NOP,                 // 14
        NOP,                 // 15
        SW(19, 0, 0x144),    // 16  [0x144] = 0x5A
        NOP,                 // 17
        NOP,                 // 18
        NOP,                 // 19
        SW(20, 0, 0x148),    // 20  [0x148] = 0xA5
        NOP,                 // 21
        NOP,                 // 22
        NOP,                 // 23
    }};
}

// ===========================================================================
// 判别性固件的**编译期**纪律（方向与旧护栏**相反**）
//
// 旧护栏 `stall_drop_slots_are_nop`（已于 2026-10-27 删除，ADR-048 D11）要求
// 「停顿点之后的位置 MUST 是 NOP」—— 那是缺陷 A 下的**规避**手段。
// skid buffer 落地后**约束消失**，但本测试的判别力**依赖**该位置是非 NOP
// ⇒ 用一条**反向**护栏把「测试不会退化成空转」钉死。
// ===========================================================================
constexpr bool skid_slot_is_discriminating() {
    bool found = false;
    for (std::size_t i = 0; i + 2 < kSkidFirmwareWords; ++i) {
        const std::uint32_t w = skid_firmware()[i];
        if ((w & 0x7Fu) != 0x03u) continue;   // 不是 LOAD
        const std::uint32_t rd = (w >> 7) & 31u;
        if (rd == 0) continue;                // 写 x0 ⇒ 不会有依赖
        const std::uint32_t c = skid_firmware()[i + 1];   // 紧邻的消费者
        if (((c >> 15) & 31u) == rd || ((c >> 20) & 31u) == rd) {
            const std::uint32_t slot = skid_firmware()[i + 2];
            // 该位置 MUST **不是** NOP（否则「丢掉它」不可观测）
            if (slot == NOP) return false;
            // 且 MUST 写一个**非 x0** 寄存器（否则「丢了」也不可见）
            if (((slot >> 7) & 31u) == 0u) return false;
            found = true;
        }
    }
    return found;   // MUST 至少有一处（否则本测试对缺陷 A 零信号）
}
static_assert(skid_slot_is_discriminating(),
              "判别性固件：load-use 停顿点之后 MUST 是**非 NOP** 且写非 x0 寄存器"
              "（否则本测试对「停顿丢指令」零信号）");

TEST_CASE("Rv32iSoc - FetchSkidBufferDiscriminating",
          "[rv32i][soc][fetch][skid][dual][verilator][e2e]") {
    constexpr unsigned kCycles = 80;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

#if __has_include("jit/jit_compiler.h")
    sim_a->set_jit_enabled(false);
#endif
    sim_a->set_backend(std::make_unique<InterpreterBackend>());

    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_skid");
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

    load_firmware(*sim_a, dev_a->instance(), skid_firmware());
    load_firmware(*sim_b, dev_b->instance(), skid_firmware());

    const Trace ta = run_trace(*sim_a, dev_a->instance(), kCycles);
    const Trace tb = run_trace(*sim_b, dev_b->instance(), kCycles);

    // ---- ① 双后端逐拍全等（**无掩码**，比对 kObsNames 的 27 个输出）----
    REQUIRE(ta.size() == tb.size());
    for (std::size_t c = 0; c < ta.size(); ++c) {
        for (std::size_t k = 0; k < kObsCount; ++k) {
            INFO("cycle " << c << " obs " << kObsNames[k] << " (interp="
                          << ta[c][k] << ", verilator=" << tb[c][k] << ")");
            REQUIRE(ta[c][k] == tb[c][k]);
        }
    }

    // ---- ② 写回序列（**强断言**：既抓「丢」也抓「重」）----
    //
    // 手工推导（固件只有一条直路径 ⇒ 程序顺序即写回顺序）：
    //   w0  ⇒ (16, 1)
    //   w4  ⇒ (21, 0x33)
    //   w10 ⇒ (17, 0x33)
    //   w11 ⇒ (18, 0x33)
    //   w12 ⇒ (19, 0x5A)   ← 缺陷 A 的判别位：丢了就没有这条
    //   w13 ⇒ (20, 0xA5)   ← 缺陷 B/重复装载的判别位：双装时这里会有**两条**
    const std::vector<std::pair<uint64_t, uint64_t>> expect_wb = {
        {16, 0x01u}, {21, 0x33u}, {17, 0x33u},
        {18, 0x33u}, {19, 0x5Au}, {20, 0xA5u}};
    {
        std::vector<std::pair<uint64_t, uint64_t>> wb;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] != 1u) continue;
            wb.push_back({ta[c][kIdxWbRdAddr], ta[c][kIdxWbWriteData]});
        }
        REQUIRE(wb.size() == expect_wb.size());
        for (std::size_t i = 0; i < wb.size(); ++i) {
            INFO("writeback #" << i << " got (rd=" << wb[i].first
                               << ", data=" << wb[i].second << ")");
            REQUIRE(wb[i] == expect_wb[i]);
        }
    }

    // ---- ③ 缺陷 B：首条指令（自依赖）MUST 只写回**一次**且值为 1 ----
    {
        std::size_t x16_writes = 0;
        bool saw_two = false;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] != 1u || ta[c][kIdxWbRdAddr] != 16u) {
                continue;
            }
            ++x16_writes;
            if (ta[c][kIdxWbWriteData] == 2u) saw_two = true;
        }
        REQUIRE(x16_writes == 1u);
        // 反向期望：双装 + 前推 ⇒ 2（MUST NOT 出现）
        REQUIRE_FALSE(saw_two);
    }

    // ---- ④ 缺陷 A：救回的指令 MUST **真的执行** ----
    // store 拍是「执行到这里」的最强证据（比只写寄存器更靠下游一级）
    REQUIRE(has_store(ta, 0x140u, 0x33u));
    REQUIRE(has_store(ta, 0x144u, 0x5Au));   // 反向期望：丢弃 ⇒ 不出现
    REQUIRE_FALSE(has_store(ta, 0x144u, 0x0u));

    // ---- ⑤ 重复装载：`x20` 的写回次数 MUST 恰好 1 ----
    // ⚠️ 值与「重了一次」不同 ⇒ 集合/去重断言都抓不到 ⇒ MUST 用**计数**断言
    {
        std::size_t x20_writes = 0;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] == 1u && ta[c][kIdxWbRdAddr] == 20u) {
                ++x20_writes;
            }
        }
        REQUIRE(x20_writes == 1u);
    }

    // ---- ⑥ 取指流的顺序不变式：ID 看到的**去重后**word 序列 MUST 逐 4 递增 ----
    //
    // 手工推导：本固件**无分支** ⇒ 程序顺序 = 取指顺序 ⇒
    // 「ID 级相邻两个**不同** word MUST 相差 +4」
    // ⇒ ① 跳号 = 丢指令（缺陷 A）；② 倒退 = 重取指令。
    // ⚠️ 停顿期间 ID **保持**同一个 word ⇒ 必须先去重再判（否则正常停顿会误报）。
    {
        std::vector<uint64_t> words;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            const uint64_t pc = ta[c][kIdxIdPc];
            if (pc < 0x80000000ull) continue;
            const uint64_t w = (pc - 0x80000000ull) / 4u;
            if (w >= kSkidFirmwareWords) continue;   // 跑出尾部（补 0）
            if (words.empty() || words.back() != w) {
                words.push_back(w);
            }
        }
        REQUIRE(words.size() == kSkidFirmwareWords);   // 24 个 word **一个不少**
        for (std::size_t i = 0; i < words.size(); ++i) {
            INFO("distinct id word #" << i << " = " << words[i]);
            REQUIRE(words[i] == i);
        }
    }
}

// ===========================================================================
// ⑦ load 字节道固件（change `rv32i-load-byte-lane-2026-10`）
//
// ⚠️ **为什么是独立的一套固件**：
//    既有固件（`firmware()`，108 字）只有 `LW`/`SW`/`SB`/`SH` ⇒ **零** `LB`/`LH`
//    /`LBU`/`LHU` ⇒ load 侧的「车道选择 + 扩展方式」**从未被任何固件跑过**
//    ⇒ 若 lane 选择或符号扩展写错，**全仓照绿**（典型的「绿 ≠ 对」）。
//    ⇒ 本套固件**专门**覆盖 4 个字节道 + 2 个半字道。
//    ⇒ 独立成一套 ⇒ 既有固件的 golden（24 条写回序列 / 停顿计数 / 退休数）
//    **零变化** ⇒ 「既有 golden 的变化 MUST 全部可归因」⇒ 本次归因 = **零**。
//
// ⚠️ 本 change 同时**修复**了 `mem_stage.h` 的 LB 符号扩展缺陷
//    （`0xFF000000` ⇒ 应为 `0xFFFFFF00`）—— 那是个**从未被文档化**的缺陷，
//    因为 LB/LBU/LH/LHU 在仓内的唯一用例依赖**仓外** riscv-tests 二进制
//    （`lb.bin`）⇒ CI 中全部 SKIP ⇒ 有效覆盖为 0。
// ===========================================================================

constexpr std::size_t kLoadByteLaneFirmwareWords = 20;

// ⚠️ 造字 MUST 用 **`SB`**（**不是** `SW`）：
//    `SW` 的 `funct3=010` ⇒ 字节使能是 `0xF`（**整字**）且其数据**不**按
//    `addr[1:0]` 移位（`rv32i_pipeline.h` 的 lane 对齐**显式排除** funct3=010）
//    ⇒ 4 次 `SW` 会**互相覆盖**成最后一个值 ⇒ 被读字的 4 个车道退化成
//    「1 个非零 + 3 个相同的 0x00」⇒ **恰好毁掉**判别力前提。
constexpr std::array<uint32_t, kLoadByteLaneFirmwareWords>
load_byte_lane_firmware() {
    return {{
        // ⚠️ 4 个字节值**两两不同**且 **bit7 全为 1**
        //    ⇒ 「车道选错」与「符号 vs 零扩展」**都**可观测。
        ADDI(1, 0, 0xD4),   // 0   x1 = 0xD4  ← 字节值 lane 0
        ADDI(2, 0, 0xC3),   // 1   x2 = 0xC3  ← lane 1
        ADDI(3, 0, 0xB2),   // 2   x3 = 0xB2  ← lane 2
        ADDI(4, 0, 0xA1),   // 3   x4 = 0xA1  ← lane 3
        // ⚠️ 每条 `SB` 与它的 `ADDI` 相距 **4**（中间隔 3 条无关指令）
        //    ⇒ 走**寄存器堆路径**（ADR-047 D11 的组合旁路修的是**距离 3**）
        //    ⇒ 不把「前推」混进本用例的判别里。
        SB(1, 0, 0x100),    // 4   [0x100] lane 0 = 0xD4
        SB(2, 0, 0x101),    // 5   [0x101] lane 1 = 0xC3
        SB(3, 0, 0x102),    // 6   [0x102] lane 2 = 0xB2
        SB(4, 0, 0x103),    // 7   [0x103] lane 3 = 0xA1
        // ⇒ 被读的字 = **0xA1B2C3D4**（4 个车道各不相同）
        NOP, NOP, NOP, NOP,  // 8..11
        // ⚠️ 8 条 load **连续**且写**互不相干**的 rd ⇒ 无数据依赖
        //    ⇒ 无 load-use 停顿 ⇒ 本用例**不**测停顿（停顿由既有固件覆盖）。
        LB(5, 0, 0x100),    // 12  x5  = 0xFFFFFFD4  ← lane 0，符号扩展
        LB(6, 0, 0x101),    // 13  x6  = 0xFFFFFFC3  ← lane 1，符号扩展
        LBU(7, 0, 0x102),   // 14  x7  = 0x000000B2  ← lane 2，零扩展
        LBU(8, 0, 0x103),   // 15  x8  = 0x000000A1  ← lane 3，零扩展
        LH(9, 0, 0x100),    // 16  x9  = 0xFFFFC3D4  ← half 0，符号扩展
        LH(10, 0, 0x102),   // 17  x10 = 0xFFFFA1B2  ← half 1，符号扩展
        LHU(11, 0, 0x100),  // 18  x11 = 0x0000C3D4  ← half 0，零扩展
        LHU(12, 0, 0x102),  // 19  x12 = 0x0000A1B2  ← half 1，零扩展
    }};
}

// ---------------------------------------------------------------------------
// 判别性固件的**编译期**纪律：把「可观测性前提」钉死
//
// ⚠️ 这与 `skid_slot_is_discriminating()` 同一纪律：把「测试不会退化成空转」
//    用**编译期断言**钉死 ⇒ 任何人「顺手把某个字节值改成 0x00」都会
//    **编译失败**，而不是让判别力**静默**归零。
//
// 钉住的三件事（对应主 spec `rv32i-isa-semantics` 的可观测性 Scenario）：
//   ① 4 个字节值 MUST 两两不同 且 bit7 MUST 全为 1
//      ⇒ 否则「车道选错」或「符号 vs 零扩展」**不可观测**；
//   ② 4 条 `SB` MUST 打到 4 个**不同**的字节车道；
//   ③ 8 条 load MUST 覆盖 **4 个字节道 + 2 个半字道**。
// ---------------------------------------------------------------------------
constexpr bool load_byte_lane_is_discriminating() {
    // ① 4 个字节值：两两不同 + bit7 全为 1
    std::uint32_t bytes[4] = {0u, 0u, 0u, 0u};
    for (int i = 0; i < 4; ++i) {
        const std::uint32_t w = load_byte_lane_firmware()[i];
        if ((w & 0x7Fu) != 0x13u) return false;        // 必须是 OP-IMM
        if (((w >> 12) & 7u) != 0u) return false;      // funct3 = ADDI
        const std::uint32_t imm = (w >> 20) & 0xFFFu;
        if ((imm & 0x80u) == 0u) return false;          // bit7 MUST 为 1
        bytes[i] = imm & 0xFFu;
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            if (bytes[i] == bytes[j]) return false;      // MUST 两两不同
        }
    }
    // ② 4 条 SB MUST 打到 4 个不同的字节车道
    std::uint32_t lanes[4] = {0u, 0u, 0u, 0u};
    for (int i = 0; i < 4; ++i) {
        const std::uint32_t w = load_byte_lane_firmware()[4u + static_cast<std::size_t>(i)];
        if ((w & 0x7Fu) != 0x23u) return false;        // 必须是 STORE
        if (((w >> 12) & 7u) != 0u) return false;      // funct3 = SB
        // S 型立即数：imm[4:0] 在 instr[11:7]，imm[11:5] 在 instr[31:25]
        const std::uint32_t imm =
            (((w >> 25) & 0x7Fu) << 5) | ((w >> 7) & 0x1Fu);
        lanes[i] = imm & 0x3u;                         // 只看 addr[1:0]
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            if (lanes[i] == lanes[j]) return false;     // 4 个车道 MUST 都覆盖
        }
    }
    // ③ 8 条 load MUST 覆盖 4 个字节道 + 2 个半字道
    unsigned byte_lanes = 0u;
    unsigned half_lanes = 0u;
    for (std::size_t i = 12; i < kLoadByteLaneFirmwareWords; ++i) {
        const std::uint32_t w = load_byte_lane_firmware()[i];
        if ((w & 0x7Fu) != 0x03u) return false;        // 必须是 LOAD
        const std::uint32_t f3 = (w >> 12) & 7u;
        const std::uint32_t imm = (w >> 20) & 0xFFFu;
        if (f3 == 0u || f3 == 4u) {                    // LB / LBU
            byte_lanes |= 1u << (imm & 3u);
        } else if (f3 == 1u || f3 == 5u) {             // LH / LHU
            half_lanes |= 1u << ((imm >> 1) & 1u);
        } else {
            return false;                              // MUST 只有这 4 种
        }
    }
    return byte_lanes == 0xFu && half_lanes == 0x3u;
}

static_assert(load_byte_lane_is_discriminating(),
              "load 字节道固件 MUST 保持判别力：4 个字节值两两不同且 bit7 全为 1、"
              "4 条 SB 覆盖 4 个不同车道、8 条 load 覆盖 4 字节道 + 2 半字道");

// ---------------------------------------------------------------------------
// 金标：**12 项写回序列**（4 条 ADDI + 8 条 load；store 不写回）
//
// ⚠️ 期望值由**机制手工推导**（见该 change 的 design.md §2.3），
//    **不是**抄仿真输出 ⇒ 实测只用于**交叉核对**。
// ⚠️ 「反向期望」逐条留痕在 design.md §2.3 —— 每项都写了「写错时会是多少」。
// ---------------------------------------------------------------------------
constexpr std::array<std::pair<uint64_t, uint64_t>, 12> kLoadByteLaneWritebacks = {{
    {1, 0x000000D4u},   // addi x1,x0,0xD4
    {2, 0x000000C3u},   // addi x2,x0,0xC3
    {3, 0x000000B2u},   // addi x3,x0,0xB2
    {4, 0x000000A1u},   // addi x4,x0,0xA1
    {5, 0xFFFFFFD4u},   // lb  x5,0x100  ← lane0=0xD4 符号扩展
                         //   反向期望：零扩展⇒0x000000D4；填充掩码错⇒0xFF0000D4
    {6, 0xFFFFFFC3u},   // lb  x6,0x101  ← lane1=0xC3 符号扩展
                         //   反向期望：lane 恒取 0⇒0xFFFFFFD4；零扩展⇒0x000000C3；
                         //             填充掩码错⇒0xFF0000C3
    {7, 0x000000B2u},   // lbu x7,0x102  ← lane2=0xB2 零扩展
                         //   反向期望：lane 恒取 0⇒0x000000D4
    {8, 0x000000A1u},   // lbu x8,0x103  ← lane3=0xA1 零扩展
                         //   反向期望：lane 恒取 0⇒0x000000D4
    {9, 0xFFFFC3D4u},   // lh  x9,0x100  ← half0=0xC3D4 符号扩展
                         //   反向期望：half 取反⇒0xFFFFA1B2；零扩展⇒0x0000C3D4；
                         //             填充掩码错⇒0xFF00C3D4
    {10, 0xFFFFA1B2u},  // lh  x10,0x102 ← half1=0xA1B2 符号扩展
                         //   反向期望：half 恒取 half0⇒0xFFFFC3D4；零扩展⇒0x0000A1B2；
                         //             填充掩码错⇒0xFF00A1B2
    {11, 0x0000C3D4u},  // lhu x11,0x100 ← half0=0xC3D4 零扩展
                         //   反向期望：half 取反⇒0x0000A1B2
                         //   ⚠️「half 恒取 half0」⇒ **仍是 0x0000C3D4** ⇒ 对该变异零判别力
    {12, 0x0000A1B2u},  // lhu x12,0x102 ← half1=0xA1B2 零扩展
                         //   反向期望：half 恒取 half0⇒0x0000C3D4
}};

// store 侧的期望：`SB` 的源数据**低位对齐**，由总线按 `addr[1:0]` 选车道
// ⇒ `obs_data_addr` 是**字节地址**，`obs_data_write_data` 是**已对齐**的数据。
constexpr std::array<std::pair<uint32_t, uint32_t>, 4> kLoadByteLaneStores = {{
    {0x100u, 0x000000D4u},   // sb x1,0x100 ⇒ lane 0，数据在 bits[7:0]
    {0x101u, 0x0000C300u},   // sb x2,0x101 ⇒ lane 1，数据左移 8
    {0x102u, 0x00B20000u},   // sb x3,0x102 ⇒ lane 2，数据左移 16
    {0x103u, 0xA1000000u},   // sb x4,0x103 ⇒ lane 3，数据左移 24
}};

TEST_CASE("Rv32iSoc - LoadByteLaneDiscriminating",
          "[rv32i][soc][isa][load][byte-lane][dual][verilator][e2e]") {
    constexpr unsigned kCycles = 80;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

#if __has_include("jit/jit_compiler.h")
    sim_a->set_jit_enabled(false);
#endif
    sim_a->set_backend(std::make_unique<InterpreterBackend>());

    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_lbl");
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

    load_firmware(*sim_a, dev_a->instance(), load_byte_lane_firmware());
    load_firmware(*sim_b, dev_b->instance(), load_byte_lane_firmware());

    const Trace ta = run_trace(*sim_a, dev_a->instance(), kCycles);
    const Trace tb = run_trace(*sim_b, dev_b->instance(), kCycles);

    // ---- ① 双后端逐拍全等（**无掩码**，全部 27 个核 IP 顶层输出）----
    //
    // ⚠️ 「无掩码」是**可执行**的：比对端口清单 = `kObsNames`（27 个）
    //    ⇒ 只证明「两个后端一样」；**③** 才证明「两个后端都对」。
    REQUIRE(ta.size() == tb.size());
    for (std::size_t c = 0; c < ta.size(); ++c) {
        for (std::size_t k = 0; k < kObsCount; ++k) {
            INFO("cycle " << c << " obs " << kObsNames[k] << " (interp="
                          << ta[c][k] << ", verilator=" << tb[c][k] << ")");
            REQUIRE(ta[c][k] == tb[c][k]);
        }
    }

    // ---- ② 写回序列逐项比对（**12 项**，**不去重**）----
    //
    // ⚠️ **不去重**：任何连续重复都是**真回归**（去重掩码会反过来掩盖它）。
    // ⚠️ 用**序列**而不是计数：「多一次假退休」会让计数**同步** +1
    //    ⇒ 任何计数断言都抓不到 ⇒ 只有序列能抓。
    {
        // ⚠️ **2026-11-23（S3）**：D-Cache 接入后 MEM 级是**多拍**的
        //    ⇒ ⇒ `rv32i-memory-timing` 的「MEM/WB 寄存器 MUST 被冻结」会让**同一条**
        //    指令在 WB 停留多拍 ⇒ ⇒ `wb_write_en` 是**电平**（不是脉冲）
        //    ⇒ ⇒ 逐拍收集会把同一条指令数成多条。
        //    ⇒ 口径：**连续相同** `(rd, data)` 观测折成一条。
        //    ⚠️ 残余盲区（如实登记）：紧邻的**同 rd 同 data** 两条指令无法与冻结区分
        //    ⇒ ⇒ 该角落由 ② 的**序列**（任何顺序错位仍会被抓到）与
        //    `obs_perf_instructions`（退休计数）共同覆盖。
        std::vector<std::pair<uint64_t, uint64_t>> wb;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] != 1u) continue;
            const std::pair<uint64_t, uint64_t> ev{ta[c][kIdxWbRdAddr],
                                                   ta[c][kIdxWbWriteData]};
            if (!wb.empty() && wb.back() == ev) continue;   // 冻结拍的重复
            wb.push_back(ev);
        }
        REQUIRE(wb.size() == kLoadByteLaneWritebacks.size());
        for (std::size_t i = 0; i < wb.size(); ++i) {
            INFO("writeback #" << i << " got (rd=" << wb[i].first
                                << ", data=" << wb[i].second << ")");
            REQUIRE(wb[i] == kLoadByteLaneWritebacks[i]);
        }
    }

    // ---- ③ store 侧：4 条 `SB` MUST 打到**正确的车道**且数据**已对齐**----
    //
    // ⚠️ 这一组断言钉的是 load 断言**抓不到**的另一半：
    //    「车道**对齐**」（源数据落在哪个 lane）vs load 侧的「车道**选择**」。
    // ⚠️ 反向期望：数据未对齐 ⇒ 该条 `REQUIRE` 转红。
    for (const auto &s : kLoadByteLaneStores) {
        INFO("store addr=0x" << std::hex << s.first << " data=0x" << s.second);
        REQUIRE(has_store(ta, s.first, s.second));
    }

    // ---- ④ 逐个 load 的**独立**断言（反向期望显式留痕）----
    //
    // ⚠️ 与 ② 的序列断言**互补**：② 是「顺序 + 值」的强判据；
    //    ④ 让每条 load 的失败信息**直接指向那条指令**。
    {
        std::map<uint64_t, std::set<uint64_t>> written;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] != 1u) continue;
            written[ta[c][kIdxWbRdAddr]].insert(ta[c][kIdxWbWriteData]);
        }
        auto expect_written = [&written](uint64_t rd, uint64_t value,
                                         const char *what) {
            INFO(what << ": rd=" << rd << " expected=0x" << std::hex << value);
            const auto it = written.find(rd);
            REQUIRE(it != written.end());      // MUST 被写回过
            REQUIRE(it->second.size() == 1u);  // 且**只**被写成这一个值
            REQUIRE(*it->second.begin() == value);
        };
        expect_written(5, 0xFFFFFFD4u, "lb  x5,0x100 (lane0, 符号扩展)");
        expect_written(6, 0xFFFFFFC3u, "lb  x6,0x101 (lane1, 符号扩展)");
        expect_written(7, 0x000000B2u, "lbu x7,0x102 (lane2, 零扩展)");
        expect_written(8, 0x000000A1u, "lbu x8,0x103 (lane3, 零扩展)");
        expect_written(9, 0xFFFFC3D4u, "lh  x9,0x100 (half0, 符号扩展)");
        expect_written(10, 0xFFFFA1B2u, "lh  x10,0x102 (half1, 符号扩展)");
        expect_written(11, 0x0000C3D4u, "lhu x11,0x100 (half0, 零扩展)");
        expect_written(12, 0x0000A1B2u, "lhu x12,0x102 (half1, 零扩展)");
    }
}

// ===========================================================================
// T6 判别性固件：**零间隔**的同址 `store → load`（MUST 读到**新值**）
// ===========================================================================
//
// ⚠️ **为什么既有固件的 `SW`/`LW` 对本判据**零判别力**：
//    既有固件的 `SW(4,0,0x100)` 在 word 16、`LW(5,0,0x100)` 在 word 20
//    ⇒ ⇒ **隔 4 个字** ⇒ ⇒ store 早已提交 ⇒ ⇒ **无论**「写」的发出相位
//    （EX/MEM 还是 MEM/WB）**都**读得到新值 ⇒ ⇒ 判据恒绿 ⇒ ⇒ 零信息量。
//
// ⚠️ **本固件 MUST 把 store 与 load 放在**连续两条指令**上**
//    ⇒ ⇒ 「写提交沿」与「读发出沿」只差 **1 拍** ⇒ ⇒ 若写的发出相位被推后
//    （例如改成从 MEM/WB 驱动）⇒ ⇒ 读就会拿到**旧值** ⇒ ⇒ **必红**。
//
// ⚠️ **期望值是**手推**的（禁抄仿真输出）**：
//    · `sw`/`lw` 的地址 = `x0 + imm`（`x0` 恒 0）⇒ ⇒ 字节地址 = imm
//    · `sw x16` 写**整字**（funct3 = 010 ⇒ 无车道移位）⇒ ⇒ 字 = x16 = 0x5A
//    · 紧邻的 `lw` MUST 读回**整字** 0x5A（**read-new** · ADR-069 **V6**）
//    · 反向对照：从未被写过的地址 ⇒ 读回 **0**（dmem 零初始化）
//
// ⚠️ **既有固件的 golden MUST 零变化** ⇒ ⇒ 本用例用**独立固件 + 独立 TEST_CASE**
//    （与 `skid_firmware()` / `load_byte_lane_firmware()` 同款做法）⇒ ⇒
//    既有 `kExpectedWritebacks` / `non_retired` / `store_cycles` **一个都不动**。
constexpr size_t kT6FirmwareWords = 12;

constexpr std::array<uint32_t, kT6FirmwareWords> t6_firmware() {
    return {{
        ADDI(16, 0, 0x5A),   // 0   x16 = 0x5A
        NOP, NOP, NOP,        // 1-3
        SW(16, 0, 0x150),    // 4   [0x150] = 0x5A
        LW(17, 0, 0x150),    // 5   **紧邻**读回 ⇒ x17 MUST = 0x5A（read-new）
        NOP, NOP, NOP,        // 6-8
        LW(18, 0, 0x158),    // 9   从未写过的地址 ⇒ x18 MUST = 0（反向对照）
        NOP, NOP,             // 10-11
    }};
}

// 期望的写回序列（**手推**）：x16 = 0x5A · x17 = 0x5A（紧邻 read-new）· x18 = 0
constexpr std::array<std::pair<uint64_t, uint64_t>, 3> kT6Writebacks = {{
    {16, 0x5A},
    {17, 0x5A},
    {18, 0x00},
}};

// ⚠️ **编译期**纪律：store 与 load MUST **紧邻**（否则本用例退化成「零判别力」）
//    ⇒ ⇒ 用**常量**判定，不靠人眼核对。
constexpr bool t6_store_load_is_adjacent() {
    for (std::size_t i = 0; i + 1 < kT6FirmwareWords; ++i) {
        const std::uint32_t w = t6_firmware()[i];
        if ((w & 0x7Fu) != 0x23u) continue;   // 不是 STORE
        const std::uint32_t next = t6_firmware()[i + 1];
        if ((next & 0x7Fu) != 0x03u) continue;  // 下一条不是 LOAD ⇒ 不算
        return true;
    }
    return false;
}
static_assert(t6_store_load_is_adjacent(),
              "T6 判别性固件：MUST 存在**紧邻**的 store → load（否则「零间隔同址」判据零判别力）");

TEST_CASE("Rv32iSoc - StoreToLoadZeroIntervalReadNew",
          "[rv32i][soc][isa][mem][t6][dual][verilator][e2e]") {
    // ⚠️ **ADR-069 V6**：「**零间隔**同址 `store → load` 读到**新值**（`read-new`）」
    //    ⇒ ⇒ 判据形态 = **指令级**（连续两条）⇒ ⇒ **禁**单拍形态
    //    （那是沿级 `read-old` ⇒ ⇒ 两者**成对**钉住 ⇒ ⇒ ADR-068 V6/V7）
    constexpr unsigned kCycles = 60;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

#if __has_include("jit/jit_compiler.h")
    sim_a->set_jit_enabled(false);
#endif
    sim_a->set_backend(std::make_unique<InterpreterBackend>());

    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_t6");
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

    load_firmware(*sim_a, dev_a->instance(), t6_firmware());
    load_firmware(*sim_b, dev_b->instance(), t6_firmware());

    const Trace ta = run_trace(*sim_a, dev_a->instance(), kCycles);
    const Trace tb = run_trace(*sim_b, dev_b->instance(), kCycles);

    // ---- ① 双后端逐拍全等（**无掩码**）----
    REQUIRE(ta.size() == tb.size());
    for (std::size_t c = 0; c < ta.size(); ++c) {
        for (std::size_t k = 0; k < kObsCount; ++k) {
            INFO("cycle " << c << " obs " << kObsNames[k] << " (interp="
                          << ta[c][k] << ", verilator=" << tb[c][k] << ")");
            REQUIRE(ta[c][k] == tb[c][k]);
        }
    }

    // ---- ② 写回序列：x16 = 0x5A · **x17 = 0x5A（紧邻 read-new）** · x18 = 0 ----
    {
        std::vector<std::pair<uint64_t, uint64_t>> wb;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] != 1u) continue;
            wb.push_back({ta[c][kIdxWbRdAddr], ta[c][kIdxWbWriteData]});
        }
        REQUIRE(wb.size() == kT6Writebacks.size());
        for (std::size_t i = 0; i < wb.size(); ++i) {
            INFO("writeback #" << i << " got (rd=" << wb[i].first
                                << ", data=0x" << std::hex << wb[i].second
                                << ")");
            REQUIRE(wb[i] == kT6Writebacks[i]);
        }
    }

    // ---- ③ store 侧：MUST 真的写出了这个字（否则「读到新值」是空转）----
    REQUIRE(has_store(ta, 0x150u, 0x5Au));

    // ---- ④ 逐个写回**独立**断言（反向期望显式留痕）----
    {
        std::map<uint64_t, std::set<uint64_t>> written;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] != 1u) continue;
            written[ta[c][kIdxWbRdAddr]].insert(ta[c][kIdxWbWriteData]);
        }
        auto expect_written = [&written](uint64_t rd, uint64_t value,
                                         const char *what) {
            INFO(what << ": rd=" << rd << " expected=0x" << std::hex << value);
            const auto it = written.find(rd);
            REQUIRE(it != written.end());
            REQUIRE(it->second.size() == 1u);
            REQUIRE(*it->second.begin() == value);
        };
        expect_written(16, 0x5Au, "addi x16,x0,0x5A");
        expect_written(17, 0x5Au, "lw x17,0x150（**紧邻** store ⇒ read-new）");
        expect_written(18, 0x00u, "lw x18,0x158（从未写过 ⇒ 0）");
    }
}

// ===========================================================================
// V13 / V14a / V14b 判别性固件：**ALU 生产者**在距离 2 / 3 / 4 的消费者
// ===========================================================================
//
// ⚠️ **为什么既有 `fwd_firmware()` 不够**：
//    它的 ②（距离 2）与 ③（距离 3）用的都是 **load 生产者**
//    ⇒ ⇒ 覆盖的是「**load** 的数据」通路 ⇒ ⇒ **钉不到**「**ALU 结果**」在
//    距离 2 时的档位选择（V13）⇒ ⇒ 也**钉不到**距离 4（V14b ⇒ ⇒ 那正是
//    **WB 档必须保留**的理由 ⇒ ⇒ 若把 WB 档删掉，既有固件**全绿** ⇒ ⇒ 缺口）。
//
// ⚠️ **期望值是**手推**的（禁抄仿真输出）**：RISC-V 的顺序语义 ⇒ ⇒
//    「消费者读到生产者**刚写**的值」⇒ ⇒ 每个消费者 MUST = 其生产者的值。
//    ⇒ ⇒ **距离 2 / 3 / 4 三个消费者** MUST 各自拿到**自己**那个生产者的值。
//
// ⚠️ **每条用例用**不同的 rd** ⇒ ⇒ 变异自证时**失败信息直接指认**是哪一档
//    被破坏 ⇒ ⇒ 「哪条钉住了哪一档」由**实测**给出（不是手推拍级表）。
//
// ✅ **变异自证结果（2026-11-14 实测）**：
//    · 突变体 **A**（档位 2 的源改回 `memwb_*`）⇒ ⇒ **rd=17（距离 2）转红**
//      ⇒ ⇒ **V13 被本固件钉住** ✓
//    · 突变体 **B**（删 D11 的 **M2** 档）⇒ ⇒ **rd=19（距离 3）转红**
//      ⇒ ⇒ **V14a 被本固件钉住** ✓
//    · 突变体 **C**（删 D11 的 **WB** 档）⇒ ⇒ 本固件**不红** ⇒ ⇒ **距离 4 不依赖 WB 档**
//      ⇒ ⇒ 而 **WB 档的必要性由【既有固件】钉住**（`sub x4` 距 `add x3` **距离 4**
//      ⇒ ⇒ 突变体 C ⇒ `has_store(kGoldenAddrA, kGoldenValA)` 转红）
//      ⇒ ⇒ **V14b 已闭（既有固件）** ⇒ ⇒ 本固件的「距离 4」那条只钉**ISA 语义**
//      （消费者 MUST 拿到生产者的值），**不**承担「WB 档必要性」的归因。
constexpr size_t kFwdTierFirmwareWords = 17;

constexpr std::array<uint32_t, kFwdTierFirmwareWords> fwd_tier_firmware() {
    return {{
        // ── V13：距离 **2** 的 ALU 生产者 ──
        ADDI(16, 0, 0x11),   // 0  生产者（ALU 结果 0x11）
        NOP,                 // 1  间隔 1 条 ⇒ 距离 2
        ADD(17, 16, 0),      // 2  消费者 ⇒ MUST = 0x11
        NOP, NOP, NOP,       // 3-5
        // ── V14a：距离 **3** 的 ALU 生产者 ──
        ADDI(18, 0, 0x2A),   // 6  生产者（ALU 结果 0x2A）
        NOP, NOP,            // 7-8 间隔 2 条 ⇒ 距离 3
        ADD(19, 18, 0),      // 9  消费者 ⇒ MUST = 0x2A
        NOP, NOP, NOP,       // 10-12
        // ── V14b：距离 **4** 的 ALU 生产者 ──
        ADDI(20, 0, 0x37),   // 13 生产者（ALU 结果 0x37）
        NOP,                 // 14 间隔 3 条 ⇒ 距离 4
        NOP,                 // 15
        ADD(21, 20, 0),      // 16 消费者 ⇒ MUST = 0x37
    }};
}

// 期望的写回序列（**手推** · 顺序 = 程序序）
constexpr std::array<std::pair<uint64_t, uint64_t>, 7> kFwdTierWritebacks = {{
    {16, 0x11},   // 生产者
    {17, 0x11},   // 距离 2 的消费者（MUST = 生产者的值）
    {18, 0x2A},   // 生产者
    {19, 0x2A},   // 距离 3 的消费者
    {20, 0x37},   // 生产者
    {21, 0x37},   // 距离 4 的消费者
    {0, 0},       // 占位（x0 不写回 ⇒ 见下方断言：序列里**无** x0）
}};

TEST_CASE("Rv32iSoc - ForwardingTierDistanceCases",
          "[rv32i][soc][forwarding][tier][dual][verilator][e2e]") {
    // ⚠️ **ADR-069 V13 / V14a / V14b**：前推**档位**的距离判别
    //    ⇒ ⇒ 「距离 2 ⇒ 档位 2（M2）」「距离 3 ⇒ M2→ID 旁路」
    //    「距离 4 ⇒ WB→ID 旁路（**WB 档 MUST 保留**）」
    constexpr unsigned kCycles = 70;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

#if __has_include("jit/jit_compiler.h")
    sim_a->set_jit_enabled(false);
#endif
    sim_a->set_backend(std::make_unique<InterpreterBackend>());

    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_fwdtier");
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

    load_firmware(*sim_a, dev_a->instance(), fwd_tier_firmware());
    load_firmware(*sim_b, dev_b->instance(), fwd_tier_firmware());

    const Trace ta = run_trace(*sim_a, dev_a->instance(), kCycles);
    const Trace tb = run_trace(*sim_b, dev_b->instance(), kCycles);

    // ---- ① 双后端逐拍全等（**无掩码**）----
    REQUIRE(ta.size() == tb.size());
    for (std::size_t c = 0; c < ta.size(); ++c) {
        for (std::size_t k = 0; k < kObsCount; ++k) {
            INFO("cycle " << c << " obs " << kObsNames[k] << " (interp="
                          << ta[c][k] << ", verilator=" << tb[c][k] << ")");
            REQUIRE(ta[c][k] == tb[c][k]);
        }
    }

    // ---- ② 逐个消费者的**独立**断言（每条失败直接指认是哪一档）----
    //
    // ⚠️ 用「集合」而不是「序列」⇒ 对「取指填充期同一条指令被装两次」免疫。
    std::map<uint64_t, std::set<uint64_t>> written;
    for (std::size_t c = 0; c < ta.size(); ++c) {
        if (ta[c][kIdxWbWriteEn] != 1u) continue;
        written[ta[c][kIdxWbRdAddr]].insert(ta[c][kIdxWbWriteData]);
    }
    auto expect_written = [&written](uint64_t rd, uint64_t value,
                                     const char *what) {
        INFO(what << ": rd=" << rd << " expected=0x" << std::hex << value);
        const auto it = written.find(rd);
        REQUIRE(it != written.end());      // MUST 被写回过
        REQUIRE(it->second.size() == 1u);  // 且**只**被写成这一个值
        REQUIRE(*it->second.begin() == value);
    };
    expect_written(16, 0x11u, "addi x16,x0,0x11（生产者）");
    expect_written(17, 0x11u, "距离 **2** 的消费者（V13：档位 2 = M2）");
    expect_written(18, 0x2Au, "addi x18,x0,0x2A（生产者）");
    expect_written(19, 0x2Au, "距离 **3** 的消费者（V14a：M2→ID 旁路）");
    expect_written(20, 0x37u, "addi x20,x0,0x37（生产者）");
    // ⚠️ 「距离 4」这条**只**钉 ISA 语义（消费者 MUST 拿到生产者的值）
    //    ⇒ ⇒ **WB 档的必要性**由**既有固件**钉住（突变体 C ⇒ `golden A` 红）
    expect_written(21, 0x37u, "距离 **4** 的消费者（ISA 语义）");

    // ---- ③ 反向期望：x0 MUST NOT 被写回 ----
    REQUIRE(written.find(0) == written.end());

    // ---- ④ 写回**序列**（顺序 + 条数）----
    {
        std::vector<std::pair<uint64_t, uint64_t>> wb;
        for (std::size_t c = 0; c < ta.size(); ++c) {
            if (ta[c][kIdxWbWriteEn] != 1u) continue;
            wb.push_back({ta[c][kIdxWbRdAddr], ta[c][kIdxWbWriteData]});
        }
        REQUIRE(wb.size() == 6u);   // 6 条（x0 不写回 ⇒ 7 个槽位里的占位**不**出现）
        for (std::size_t i = 0; i < wb.size(); ++i) {
            INFO("writeback #" << i << " got (rd=" << wb[i].first
                                << ", data=0x" << std::hex << wb[i].second << ")");
            REQUIRE(wb[i] == kFwdTierWritebacks[i]);
        }
    }
}

// ===========================================================================
// **L2（路线 A）+ L6**：固件经 `ch_mem::init_data` 装载 ⇒ ⇒ **TB 零装载动作**
//
// ## 为什么这条用例必须存在
//
// 此前固件装载**只**走顶层 `prog_*` 端口 ⇒ ⇒ 它**只接 I-TCM**
// ⇒ ⇒ 有初始化 `.data` 的程序**装不进去**（D-TCM 永远是零）。
//
// ⇒ ⇒ 路线 A 把固件灌进 `ch_mem::init_data` ⇒ ⇒ 产物里自动出现
// `initial begin mem[i] = … end`（`print_mem_init`）⇒ ⇒ **零端口变更**。
//
// ⇒ ⇒ 本用例**不调用** `load_firmware()` —— 一次 `prog_*` 写都不做
// ⇒ ⇒ 若 `.data` 没真的进 D-TCM ⇒ ⇒ `x1 = 0` ⇒ ⇒ 下方断言**两条同时红**
// （这正是「走真实读通路读回」与「loader 说成功」的区别）。
// ===========================================================================
TEST_CASE("Rv32iSoc - FirmwareInitDataLoadedFromConstruction",
          "[rv32i][soc][fwinit][dual][verilator][e2e]") {
    constexpr unsigned kCycles = 40;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    // ⚠️ **初值经构造参数透传**（`ch_device<T, Args…>` ⇒ `T(nullptr,"top",Args…)`）
    //    ⇒ ⇒ **不是**通过端口写进去的
    // ⚠️ `ch_device<T, Args...>` 的 `Args...` MUST **显式**给出
    //    （类模板参数不对构造函数实参做推导 ⇒ ⇒ 只写 `ch_device<T>` = 零参）
    using FwDev = ch::ch_device<Rv32iCoreTop, std::vector<uint32_t>,
                                std::vector<uint32_t>>;
    auto dev_a = std::make_unique<FwDev>(rv32i_fw_demo::text(),
                                         rv32i_fw_demo::data());
    auto dev_b = std::make_unique<FwDev>(rv32i_fw_demo::text(),
                                         rv32i_fw_demo::data());
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

#if __has_include("jit/jit_compiler.h")
    sim_a->set_jit_enabled(false);
#endif
    sim_a->set_backend(std::make_unique<InterpreterBackend>());

    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_fwinit");
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

    // ---- **零装载**：只释放复位 ----
    // ⚠️ 一条 `prog_*` 写都没有 ⇒ ⇒ 下面任何非零读回都**只能**来自 `init_data`
    auto release_reset = [](Simulator &sim, Rv32iCoreTop &dut) {
        auto &io = dut.io();
            sim.set_input_value(io.ext_rst, 1u);
        sim.set_input_value(io.prog_we, 0u);
        sim.set_input_value(io.prog_addr, 0);
        sim.set_input_value(io.prog_data, 0);
        sim.tick(1);
        sim.set_input_value(io.ext_rst, 0u);
    };
    release_reset(*sim_a, dev_a->instance());
    release_reset(*sim_b, dev_b->instance());

    const Trace ta = run_trace(*sim_a, dev_a->instance(), kCycles);
    const Trace tb = run_trace(*sim_b, dev_b->instance(), kCycles);

    // ---- ① 双后端逐拍全等（**无掩码**）----
    REQUIRE(ta.size() == tb.size());
    for (std::size_t c = 0; c < ta.size(); ++c) {
        for (std::size_t k = 0; k < kObsCount; ++k) {
            INFO("cycle " << c << " obs " << kObsNames[k] << " (interp="
                          << ta[c][k] << ", verilator=" << tb[c][k] << ")");
            REQUIRE(ta[c][k] == tb[c][k]);
        }
    }

    // ---- ② **ISA 语义手推**：`.data` 真的经 D-TCM 读通路读回 ----
    std::map<uint64_t, std::set<uint64_t>> written;
    for (std::size_t c = 0; c < ta.size(); ++c) {
        if (ta[c][kIdxWbWriteEn] != 1u) continue;
        written[ta[c][kIdxWbRdAddr]].insert(ta[c][kIdxWbWriteData]);
    }
    {
        INFO("x1 expected 0x" << std::hex << rv32i_fw_demo::kExpectX1);
        const auto it = written.find(1);
        REQUIRE(it != written.end());      // lw x1, 0x100(x0) MUST 写回
        REQUIRE(it->second.size() == 1u);
        REQUIRE(*it->second.begin() == rv32i_fw_demo::kExpectX1);
    }
    {
        INFO("x2 expected 0x" << std::hex << rv32i_fw_demo::kExpectX2);
        const auto it = written.find(2);
        REQUIRE(it != written.end());      // addi x2, x1, 1
        REQUIRE(it->second.size() == 1u);
        REQUIRE(*it->second.begin() == rv32i_fw_demo::kExpectX2);
    }

    // ---- ③ 写存储：`sw x2, 0x104(x0)` ⇒ 数据 = 0x5B · 地址 = 0x104 ----
    REQUIRE(has_store(ta, 0x104u,
                      static_cast<uint32_t>(rv32i_fw_demo::kExpectX2)));

    // ---- ④ 反向期望：I-TCM 里**没有**的东西不能凭空出现 ----
    // `.data` 只在 D-TCM ⇒ ⇒ 若把 `.data` 错灌进 I-TCM，取指会读到数据字
    // （0x5A 不是合法指令）⇒ ⇒ 本用例的 ② 会红。这里再钉一条正向：x1 的**来源字**
    // 不是零 ⇒ ⇒ 由 ② 的值断言承担（0x5A != 0）。
}

// ===========================================================================
// **T4（`memory-timing-contract`）**：D-TCM 的**写相位**契约 + `sh` 双半字道覆盖
//
// ## 写相位契约（design.md §T4）
//
// 写使能 MUST 与被覆盖的**地址 / 数据 / 字节使能**落在**同一拍**。
// ⚠️ 这条**不能**靠「接线看起来同拍」来证明 —— 结构上它们都是组合 net，
//   只有**行为**能判：每拍 `data_write_en == 1` 时，同拍观测到的
//   `(data_addr, data_write_data)` MUST 是**同一个** store 的配对。
//   ⇒ ⇒ 若写使能被错开一拍（相位偏移）⇒ ⇒ 该拍会配到**另一条** store 的
//   地址/数据（或配到非 store 拍）⇒ ⇒ 下面的配对断言**转红**。
//
// ⇒ ⇒ **判别力来源**：3 条 store **背靠背**且地址/数据两两不同
//   ⇒ ⇒ 任何相位错位都会产生「en=1 但 pair 不属于任何期望」的拍。
//   ⇒ ⇒ ⚠️ 若 3 条 store 的地址/数据**相同**（或中间隔 NOP）⇒ 相位错位**不可观测**。
// ===========================================================================

// 手推（3 条背靠背 SW · 地址/数据两两不同）
constexpr uint32_t kT4AddrA = 0x100u, kT4DataA = 0x11u;
constexpr uint32_t kT4AddrB = 0x104u, kT4DataB = 0x22u;
constexpr uint32_t kT4AddrC = 0x108u, kT4DataC = 0x33u;

constexpr std::array<uint32_t, 12> t4_store_phase_firmware() {
    return {{
        ADDI(1, 0, static_cast<int32_t>(kT4DataA)),   // 0  x1 = 0x11
        ADDI(2, 0, static_cast<int32_t>(kT4DataB)),   // 1  x2 = 0x22
        ADDI(3, 0, static_cast<int32_t>(kT4DataC)),   // 2  x3 = 0x33
        NOP, NOP, NOP,                                 // 3..5 等三个 ADDI 写回
        SW(1, 0, static_cast<int32_t>(kT4AddrA)),     // 6  [0x100] = 0x11
        SW(2, 0, static_cast<int32_t>(kT4AddrB)),     // 7  [0x104] = 0x22  ← **背靠背**
        SW(3, 0, static_cast<int32_t>(kT4AddrC)),     // 8  [0x108] = 0x33  ← **背靠背**
        NOP, NOP, NOP,                                 // 9..11
    }};
}

TEST_CASE("Rv32iSoc - StorePhaseCoherence",
          "[rv32i][soc][store][phase][dual][verilator][e2e]") {
    constexpr unsigned kCycles = 50;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());
#if __has_include("jit/jit_compiler.h")
    sim_a->set_jit_enabled(false);
#endif
    sim_a->set_backend(std::make_unique<InterpreterBackend>());

    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_storephase");
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

    load_firmware(*sim_a, dev_a->instance(), t4_store_phase_firmware());
    load_firmware(*sim_b, dev_b->instance(), t4_store_phase_firmware());

    const Trace ta = run_trace(*sim_a, dev_a->instance(), kCycles);
    const Trace tb = run_trace(*sim_b, dev_b->instance(), kCycles);

    // ---- ① 双后端逐拍全等（无掩码）----
    REQUIRE(ta.size() == tb.size());
    for (std::size_t c = 0; c < ta.size(); ++c) {
        for (std::size_t k = 0; k < kObsCount; ++k) {
            INFO("cycle " << c << " obs " << kObsNames[k] << " (interp="
                          << ta[c][k] << ", verilator=" << tb[c][k] << ")");
            REQUIRE(ta[c][k] == tb[c][k]);
        }
    }

    // ---- ② **相位配对**：每拍 `write_en == 1` ⇒ 同拍 (addr, data) MUST 配对 ----
    const auto is_expected_pair = [](uint64_t addr, uint64_t data) {
        return (addr == kT4AddrA && data == kT4DataA) ||
               (addr == kT4AddrB && data == kT4DataB) ||
               (addr == kT4AddrC && data == kT4DataC);
    };
    std::size_t en_beats = 0;   // = store **事务数**（连续相同 (addr,data) 折一）
    std::size_t en_cycles = 0;  // = `en=1` 的**拍数**（多拍 store ⇒ 可能更大）
    std::set<std::pair<uint64_t, uint64_t>> seen;
    std::pair<uint64_t, uint64_t> last_pair{0, 0};
    bool have_last = false;
    for (std::size_t c = 0; c < ta.size(); ++c) {
        if (ta[c][kIdxDataWriteEn] != 1u) continue;
        ++en_cycles;
        // ⚠️ **2026-11-23（S3）**：D-Cache 接入后 store 会被**反压**（多拍）
        //    ⇒ ⇒ `data_write_en` 是**电平** ⇒ ⇒ 「拍数」不再等于「事务数」。
        //    ⇒ ⇒ 本判据的**意图**（不出现多余的 store）落在**事务数**上
        //    ⇒ ⇒ **连续相同 `(addr, data)`** 折成一条（冻结/反压期间 exmem 不动）。
        const std::pair<uint64_t, uint64_t> cur{ta[c][kIdxDataAddr],
                                                ta[c][kIdxDataWriteData]};
        if (!have_last || cur != last_pair) {
            ++en_beats;
            last_pair = cur;
            have_last = true;
        }
        const uint64_t addr = ta[c][kIdxDataAddr];
        const uint64_t data = ta[c][kIdxDataWriteData];
        INFO("store beat " << c << ": addr=0x" << std::hex << addr << " data=0x"
                           << data);
        // ⚠️ 这一条是**相位判据**：en=1 的**那一拍**必须配到它自己的地址/数据。
        REQUIRE(is_expected_pair(addr, data));
        seen.insert({addr, data});
    }

    // ---- ③ 三条 store 各出现一次（不多不少）----
    // `en_beats` = 事务数（上升沿）· `en_cycles` 只作诊断（多拍反压时 > 事务数）
    REQUIRE(en_beats == 3u);
    REQUIRE(en_cycles >= en_beats);
    REQUIRE(seen.size() == 3u);
    REQUIRE(seen.count({kT4AddrA, kT4DataA}) == 1u);
    REQUIRE(seen.count({kT4AddrB, kT4DataB}) == 1u);
    REQUIRE(seen.count({kT4AddrC, kT4DataC}) == 1u);
}

// ===========================================================================
// **T4**：`sh` 的 **2 个半字道**覆盖（`addr[1] = 0` 与 `1`）
//
// ⚠️ 修前现状：`sb` 已覆盖 4 个字节道，但 `sh` **只**覆盖了 `addr[1] = 1`
//   （既有固件的 `SH(3, 0, 0x122)`）⇒ ⇒ `addr[1] = 0` 的半字道**零覆盖**
//   ⇒ ⇒ 半字道的**低/高**两位置若写错（比如恒取高半字），**全仓照绿**。
//
// 手推（**字节地址 → 字内车道**）：
//   `SH x1(0x0011) -> 0x120` ⇒ addr[1:0] = 0 ⇒ 落**低半字** ⇒ 字 0x120 = 0x0000_0011
//   `SH x2(0x0022) -> 0x122` ⇒ addr[1:0] = 2 ⇒ 落**高半字** ⇒ 字 0x120 = 0x0022_0011
//   ⇒ ⇒ 读回 `LW x3, 0x120(x0)` ⇒ **x3 = 0x00220011**
// ⇒ ⇒ 若「半字道恒取高半字」⇒ 第二次写会把低半字也覆盖成 0x0022 ⇒ x3 = 0x00220022 ⇒ **红**
// ===========================================================================

constexpr uint32_t kT4HalfWordLow = 0x0011u;    // 落 0x120 的**低**半字
constexpr uint32_t kT4HalfWordHigh = 0x0022u;   // 落 0x122 的**高**半字
constexpr uint64_t kT4HalfWordMerged = 0x00220011ull;
constexpr std::size_t kT4HalfWordRd = 3;

constexpr std::array<uint32_t, 12> t4_halfword_lane_firmware() {
    return {{
        ADDI(1, 0, static_cast<int32_t>(kT4HalfWordLow)),   // 0  x1 = 0x0011
        ADDI(2, 0, static_cast<int32_t>(kT4HalfWordHigh)),  // 1  x2 = 0x0022
        NOP, NOP, NOP,                                       // 2..4 等写回
        SH(1, 0, 0x120),   // 5  低半字 = 0x0011   ← **addr[1] = 0**
        SH(2, 0, 0x122),   // 6  高半字 = 0x0022   ← **addr[1] = 1**
        NOP, NOP,          // 7..8
        LW(3, 0, 0x120),   // 9  x3 = 0x00220011  ← **两个半字道的合并结果**
        NOP, NOP,          // 10..11
    }};
}

TEST_CASE("Rv32iSoc - StoreHalfwordLaneCoverage",
          "[rv32i][soc][store][lane][dual][verilator][e2e]") {
    constexpr unsigned kCycles = 45;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());
#if __has_include("jit/jit_compiler.h")
    sim_a->set_jit_enabled(false);
#endif
    sim_a->set_backend(std::make_unique<InterpreterBackend>());

    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_hwlane");
    REQUIRE_FALSE(workdir.empty());
    auto backend = std::make_unique<VerilatorBackend>(workdir);
    if (!backend->initialize(dev_b->context(), data_map_b)) {
        WARN("VerilatorBackend::initialize() returned false - verilator half "
             "skipped (see ADR-035 R3: >64-bit ports are rejected)");
        return;
    }
    if (backend->port_access_snapshot().empty()) {
        WARN("Verilator dlopen did not populate port_access_ - skipped");
        return;
    }
    sim_b->set_backend(std::move(backend));

    load_firmware(*sim_a, dev_a->instance(), t4_halfword_lane_firmware());
    load_firmware(*sim_b, dev_b->instance(), t4_halfword_lane_firmware());

    const Trace ta = run_trace(*sim_a, dev_a->instance(), kCycles);
    const Trace tb = run_trace(*sim_b, dev_b->instance(), kCycles);

    REQUIRE(ta.size() == tb.size());
    for (std::size_t c = 0; c < ta.size(); ++c) {
        for (std::size_t k = 0; k < kObsCount; ++k) {
            INFO("cycle " << c << " obs " << kObsNames[k] << " (interp="
                          << ta[c][k] << ", verilator=" << tb[c][k] << ")");
            REQUIRE(ta[c][k] == tb[c][k]);
        }
    }

    // ① 两个半字**各自打到自己那一半**（store 数据已 lane 对齐）
    std::set<std::pair<uint64_t, uint64_t>> stores;
    for (std::size_t c = 0; c < ta.size(); ++c) {
        if (ta[c][kIdxDataWriteEn] != 1u) continue;
        stores.insert({ta[c][kIdxDataAddr], ta[c][kIdxDataWriteData]});
    }
    INFO("stores: " << stores.size());
    REQUIRE(stores.count({0x120u, kT4HalfWordLow}) == 1u);
    REQUIRE(stores.count({0x122u, kT4HalfWordHigh << 16}) == 1u);
    // ⇒ ⚠️ **lane 对齐**口径：`DataTCM::wdata` MUST 已按 `addr[1:0]` 移位
    //    ⇒ ⇒ 0x122 的高半字 ⇒ 数据在 bits[31:16]

    // ② **读回合并结果**（4 字节道里的两个半字道各就各位）
    std::map<uint64_t, std::set<uint64_t>> written;
    for (std::size_t c = 0; c < ta.size(); ++c) {
        if (ta[c][kIdxWbWriteEn] != 1u) continue;
        written[ta[c][kIdxWbRdAddr]].insert(ta[c][kIdxWbWriteData]);
    }
    const auto it = written.find(kT4HalfWordRd);
    REQUIRE(it != written.end());
    REQUIRE(it->second.size() == 1u);
    INFO("x" << kT4HalfWordRd << " expected 0x" << std::hex << kT4HalfWordMerged);
    REQUIRE(*it->second.begin() == kT4HalfWordMerged);
}

// ===========================================================================
// **G1 + G2 + G4（2026-11-15 · B4）**：未知 opcode 检测 / 冻结 / FENCE 显式化
//
// ## 为什么这三条必须一起测
//
// · **G1** 只产出「非法」这个**事实**（`illegal_reason`）
// · **G2** 才给它**后果**（冻结）与**可观测面**（核 IP 顶层端口）
// · **G4** 是 G1 的**反向判据**：`FENCE` 看起来像非法（0x0F 不在旧译码表里）
//   但它是**合法**指令（本版 = NOP）⇒ ⇒ 若只测「未知 ⇒ 冻结」，把 FENCE 也冻住
//   同样能过 ⇒ ⇒ **必须**用 `FENCE` 做**负向对照**。
//
// ## 期望值全部**手推**（禁抄仿真输出）
//
// 本文件既有 golden 已建立时序模型：**无停顿 / 无冲刷**时
//   `retire(c) = c - 4`（4 = 填充：cyc 0..3）
// （由 `kCycles = 200 ⇒ retire = 176` 与「23 个不增长拍 = 3 fill + 8 stall + 12 flush」
//  反推 ⇒ 200 − 1(cyc 0) − 23 + ... ⇒ 化简即 4 拍填充；与 `non_retired` 分段一致）
//
// 因此（固件前 8 条是 NOP，第 9 条是**被判非法/合法的目标指令**）：
//   · 未知 opcode ⇒ 第 9 条**永不进入 EX** ⇒ 退休数**恒为 8**（20 拍时）
//   · `OP_SYSTEM` 的 **ECALL**（P6-4 起）⇒ 合法 + **陷入** ⇒ 与非法路径同形 ⇒ 13
//   · `OP_SYSTEM` 的 **MRET**（P6-5 前）⇒ `reason = 1` ⇒ **仍冻结** ⇒ 8
//   · `FENCE` ⇒ **不冻结** ⇒ retire(20) = 20 − 4 = **16**，`illegal_reason == 0`
// 且冻结后 ID 级持有第 9 条 ⇒ `dbg_id_pc == 0x80000000 + 4*8 == 0x80000020`
// （PC 复位值 0x80000000 见 `stages/if_stage.h`；I-TCM 截断高位 ⇒ 字索引 8）
//
// ## ⚠️ **2026-11-26 · P6-3 的语义变更**（change `rv32i-trap-entry-2026-11`）
//
// `illegal_reason ∈ {2,3}` 的处置从「**流水线级冻结**」改为「**陷入**」：
// 写 `mepc`（= 该指令 PC）+ `mcause`（= 2）⇒ PC 重定向到 `mtvec` 的基址。
// ⇒ ⇒ **本用例的判据随之改写**（判据变了，**不是**行为回归）：
//   · `reason ∈ {2,3}`：不再断言「末拍仍持有该 reason」/「ID PC 停在探针」
//     ⇒ ⇒ 改断言「该 reason **出现过**」+「探针**永不退休**」+「最终 ID PC **不是**探针」
//     ⚠️ 本固件**没有**在 `mtvec` 的别名落点（word `0x040`）放处理器 ⇒ ⇒ 但
//        `load_firmware` 会补 **256** 个尾部 NOP（`kTailFillWords`）⇒ ⇒ word `0x040`
//        读到的是 **NOP** ⇒ ⇒ 陷入后**照常退休** ⇒ ⇒ 退休数**不再是 8**
//     ⚠️ **手推 `retire(20) = 13`**（延迟模型 `retire(c) = c − 4` 的推广）：
//        · 8 条 NOP 退休（c=4..11 各一条）· 探针（第 9 条）在 c=9 进 ID ⇒ **永不退休**
//        · 陷入 ⇒ PC → `0x20000100`（别名 word `0x040`）⇒ 该 NOP 在 **c=12** 进 ID
//          ⇒ ⇒ c=13/14/15 走 EX/MEM/WB ⇒ **c=15 起**重新退休
//        ⇒ ⇒ c=15..19 = **5** 条 ⇒ 8 + 5 = **13** ✓（实测一致）
//   · `reason = 1`（**P6-4 起只剩 `MRET`**）：**仍冻结** ⇒ 末拍断言 = 8 / 停在探针
//     （`ECALL` / `EBREAK` 已于 P6-4 改为「合法 + 陷入」；`WFI` 改为合法 NOP）
//   · `reason = 0`（`FENCE`）：NOP ⇒ 断言**不变**
// ===========================================================================
constexpr std::size_t kG1FirmwareWords = 16;

/// 前 8 条 NOP + 第 9 条 = 待测指令 + 其余 NOP
constexpr std::array<uint32_t, kG1FirmwareWords>
g1_firmware_with(uint32_t probe) {
    return {{riscv::NOP, riscv::NOP, riscv::NOP, riscv::NOP,
             riscv::NOP, riscv::NOP, riscv::NOP, riscv::NOP,
             probe,
             riscv::NOP, riscv::NOP, riscv::NOP,
             riscv::NOP, riscv::NOP, riscv::NOP, riscv::NOP}};
}

TEST_CASE("Rv32iSoc - IllegalOpcodeObservableNotRetired + FenceIsNop",
          "[rv32i][soc][isa][illegal][fence][dual][verilator][e2e]") {
    constexpr unsigned kCycles = 20;
    // 手推常量（见上方推导）
    constexpr uint64_t kRetireBeforeProbe = 8;   // 8 条 NOP 退休；第 9 条永不退休
    constexpr uint64_t kRetireNoFreeze   = 16;   // 20 - 4（填充）
    // ⚠️ P6-3：`reason ∈ {2,3}` ⇒ **陷入** ⇒ PC 落到别名 word 0x040（= 尾部 NOP）
    //    ⇒ ⇒ 照常退休 ⇒ 8 + 5 = **13**（推导见上方注释）
    constexpr uint64_t kRetireAfterTrap  = 13;
    constexpr uint64_t kProbeIdPc        = 0x80000020ULL;  // 0x80000000 + 4*8

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    // ---- 场景表：① 未知 opcode · ② OP_SYSTEM（已知未实现）· ③ FENCE（已知 = NOP）----
    struct Scenario {
        const char *name;
        uint32_t    probe;
        uint64_t    expect_reason;
        uint64_t    expect_retire;   // 20 拍时的退休数（手推）
        bool        freezes;         // ⚠️ P6-3：只有 `reason = 1` 仍冻结
    };
    const Scenario scenarios[] = {
        // 0xFFFFFFFF 的 opcode = 0x7F ⇒ **未知** ⇒ P6-3 起**陷入**
        {"unknown opcode(0xFFFFFFFF)", 0xFFFFFFFFu, 2u, kRetireAfterTrap,  false},
        // 0x00000073 = ECALL ⇒ opcode 0x73 = OP_SYSTEM ⇒ **合法 + 陷入**（P6-4）
        //   ⇒ `mcause = 11` · PC 落到 word 0x040（尾部 NOP）⇒ retire = 13
        {"OP_SYSTEM(ecall 0x73)",      0x00000073u, 0u, kRetireAfterTrap,  false},
        // 0x30200073 = MRET ⇒ P6-5 前仍是「已知未实现」⇒ `reason = 1` ⇒ **冻结**
        {"OP_SYSTEM(mret 0x302)",      0x30200073u, 1u, kRetireBeforeProbe, true},
        // 0x0FF0000F = `fence iorw,iorw` ⇒ opcode 0x0F ⇒ **已知 = NOP**
        {"FENCE(0x0F)",                0x0FF0000Fu, 0u, kRetireNoFreeze,   false},
        // ⚠️ **2026-11-16 · B4 · G1 扩展**：`lb x1, 0(x0)` 但 **funct3 = 011**
        //    ⇒ ⇒ `LB` 只允许 funct3 ∈ {000,001,010,100,101} ⇒ ⇒ **保留组合** ⇒ ⇒ reason **3**
        //    ⇒ ⇒ 编码（手推）：`(imm=0 << 20) | (rs1=0 << 15) | (3 << 12) | (rd=1 << 7) | 0x03`
        //    ⇒ ⇒ = **0x00003083**（⚠️ 第一版我写成 `0x00001083` ⇒ 那是 funct3=**001** = `LH`
        //    ⇒ ⇒ **合法** ⇒ ⇒ 被本判据当场抓住 ⇒ ⇒ 判别力实证 ✓）
        //    ⇒ ⇒ P6-3 起**陷入**
        {"LB with reserved funct3=011", 0x00003083u, 3u, kRetireAfterTrap,  false},
    };

    for (const auto &sc : scenarios) {
        INFO("scenario: " << sc.name);
        auto fw = g1_firmware_with(sc.probe);

        // ---- 双后端：两侧都要给出**同一**语义 ----
        for (int backend_kind = 0; backend_kind < 2; ++backend_kind) {
            const bool use_verilator = (backend_kind == 1);
            auto dev = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
            auto sim = std::make_unique<Simulator>(dev->context());
#if __has_include("jit/jit_compiler.h")
            sim->set_jit_enabled(false);
#endif
            if (!use_verilator) {
                sim->set_backend(std::make_unique<InterpreterBackend>());
            } else {
                ch::data_map_t data_map;
                auto workdir = make_temp_dir("_g1");
                REQUIRE_FALSE(workdir.empty());
                auto backend = std::make_unique<VerilatorBackend>(workdir);
                if (!backend->initialize(dev->context(), data_map)) {
                    WARN("VerilatorBackend::initialize() returned false - skipped");
                    continue;
                }
                if (backend->port_access_snapshot().empty()) {
                    WARN("verilator dlopen did not populate port_access_ - skipped");
                    continue;
                }
                sim->set_backend(std::move(backend));
            }

            load_firmware(*sim, dev->instance(), fw);
            const Trace t = run_trace(*sim, dev->instance(), kCycles);

            INFO("backend: " << (use_verilator ? "verilator" : "interpreter")
                             << " cycle=" << kCycles);
            // ⚠️ **P6-3 起** `reason ∈ {2,3}` ⇒ **陷入** ⇒ reason 只**出现**（不保持）
            //    ⇒ ⇒ 判据 = 「**出现过**该 reason」；`reason = 1` 仍冻结 ⇒ 末拍仍持有
            uint64_t max_reason = 0;
            for (const auto &row : t) {
                if (row[kIdxIllegalReason] > max_reason) {
                    max_reason = row[kIdxIllegalReason];
                }
            }
            INFO("max_reason = " << max_reason);
            REQUIRE(max_reason == sc.expect_reason);
            if (sc.freezes) {
                REQUIRE(t.back()[kIdxIllegalReason] == sc.expect_reason);
            }
            REQUIRE(t.back()[kIdxInstructions] == sc.expect_retire);
            // 「非法」1 bit 位 = (reason != 0) ⇒ 与 reason 自洽
            if (sc.freezes) {
                // 冻结 ⇒ ID 级停在探针指令上（pc 手推）
                REQUIRE(t.back()[kIdxIdPc] == kProbeIdPc);
            } else {
                // NOP / 陷入 ⇒ ID 级 MUST **不再**停在探针上
                //   （NOP = 越过去；陷入 = PC 已被重定向到 `mtvec` 的别名落点）
                REQUIRE(t.back()[kIdxIdPc] != kProbeIdPc);
            }
            // ---- 探针指令 MUST NOT 写回（x0 之外无副作用）----
            for (std::size_t c = 0; c < t.size(); ++c) {
                INFO("cycle " << c);
                REQUIRE(t[c][kIdxWbWriteEn] == 0u);
            }
        }
    }
}

// ===========================================================================
// **ADR-071 审稿修正的回归用例**：flush（重定向）与「未知 opcode 冻结」**同拍**
//
// ## 为什么必须有这条（oracle 2026-11-15 的对抗式复核找出的**真缺陷**）
//
// 原实现里 `if_stage` 的 PC 更新门 = `!rst & !stall` ⇒ ⇒ **stall 无条件压过
// redirect** ⇒ ⇒ 取指侧的「冻结」stall（`illegal_active`）与 EX 级的 flush
// **同拍**时 ⇒ ⇒ **重定向被丢掉** ⇒ ⇒ 下一拍 `valid = 0` ⇒ **冻结只活 1 拍**
// ⇒ ⇒ 非法字被静默跳过、`illegal_reason` 回落到 0 ⇒ ⇒ **「跑飞零信号」被 G1
// 自己造出来**（且两后端同构 ⇒ parity 结构性失明）。
//
// ## 场景的**语义正确性**（为什么「丢掉非法字」是对的）
//
// flush 只来自 **EX 级**的分支/跳转 ⇒ ⇒ 同拍 ID 级持有的是该分支的
// **fall-through**（= **错误路径**）指令 ⇒ ⇒ 「错误路径上的非法字」被 flush
// 杀掉是**正确**的（那本就不该被执行）⇒ ⇒ **正确路径**上的非法字 **不可能**
// 与 flush 同拍 ⇒ ⇒ 冻结在正确路径上仍是**终态**。
//
// ## 期望值全部**手推**
//
// 固件：word0 = `beq x0,x0,+8`（taken ⇒ 目标 = word2）· word1 = `0xFFFFFFFF`
// （**非法** · opcode 0x7F · **rd = 31** ⇒ ⇒ 若它被执行 ⇒ 会写回 x31 ⇒ ⇒
// 「x31 未被写回」是「它没被执行」的**可观测**判据）· word2 = `addi x5,x0,1`
// ⇒ ⇒ **x5 = 1** MUST 被写回（重定向被兑现 ⇒ ⇒ **旧实现会红在这里**）
// ⇒ ⇒ `illegal_reason == 2` MUST **恰好在碰撞拍**出现（word1 在 ID 级的那一拍）
// ⇒ ⇒ 之后回落到 0（flush 杀掉它 ⇒ 冻结域只覆盖正确路径）。
// ===========================================================================
constexpr std::size_t kFlushIllegalWords = 8;
constexpr std::array<uint32_t, kFlushIllegalWords> flush_illegal_firmware() {
    return {{0x00000463u,   // 0: beq x0, x0, +8  ⇒ taken ⇒ 目标 = word2
             0xFFFFFFFFu,   // 1: 非法字（fall-through = **错误路径**）· rd = 31
             0x00100293u,   // 2: addi x5, x0, 1  ⇒ 分支目标 ⇒ x5 = 1
             0x00000013u, 0x00000013u, 0x00000013u,
             0x00000013u, 0x00000013u}};
}

TEST_CASE("Rv32iSoc - flush wins over illegal-freeze stall (redirect not lost)",
          "[rv32i][soc][isa][illegal][flush][dual][verilator][e2e]") {
    constexpr unsigned kCycles = 40;

    if (!tool_available("verilator")) {
        WARN("verilator not on PATH - verilator half skipped (not SKIP: "
             "all-SKIP makes Catch2 exit 4 and CTest red)");
        return;
    }

    for (int backend_kind = 0; backend_kind < 2; ++backend_kind) {
        const bool use_verilator = (backend_kind == 1);
        INFO("backend: " << (use_verilator ? "verilator" : "interpreter"));
        auto dev = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
        auto sim = std::make_unique<Simulator>(dev->context());
#if __has_include("jit/jit_compiler.h")
        sim->set_jit_enabled(false);
#endif
        if (!use_verilator) {
            sim->set_backend(std::make_unique<InterpreterBackend>());
        } else {
            ch::data_map_t data_map;
            auto workdir = make_temp_dir("_flush_illegal");
            REQUIRE_FALSE(workdir.empty());
            auto backend = std::make_unique<VerilatorBackend>(workdir);
            if (!backend->initialize(dev->context(), data_map)) {
                WARN("VerilatorBackend::initialize() returned false - skipped");
                continue;
            }
            if (backend->port_access_snapshot().empty()) {
                WARN("verilator dlopen did not populate port_access_ - skipped");
                continue;
            }
            sim->set_backend(std::move(backend));
        }

        load_firmware(*sim, dev->instance(), flush_illegal_firmware());
        const Trace t = run_trace(*sim, dev->instance(), kCycles);

        // ---- ① 重定向 MUST 被兑现：分支目标的 `addi x5,x0,1` ⇒ x5 = 1 被写回 ----
        //      ⚠️ 这是本用例的**承重**判据：旧实现（stall 压过 flush）会**丢重定向**
        //      ⇒ ⇒ x5 永不写回 ⇒ ⇒ 本条转红 ⇒ ⇒ 判别力来源。
        bool saw_x5 = false;
        bool saw_x31 = false;
        for (std::size_t c = 0; c < t.size(); ++c) {
            INFO("cycle " << c);
            if (t[c][kIdxWbWriteEn] != 1u) continue;
            if (t[c][kIdxWbRdAddr] == 5u && t[c][kIdxWbWriteData] == 1u) {
                saw_x5 = true;
            }
            // ⚠️ **反向判据**：非法字（rd = 31）在**错误路径**上 ⇒ MUST NOT 被执行
            //    ⇒ ⇒ x31 MUST NOT 收到 0xFFFFFFFF（若被误执行 ⇒ 写回 0xFFFFFFFF）
            if (t[c][kIdxWbRdAddr] == 31u) {
                INFO("x31 writeback data=0x" << std::hex << t[c][kIdxWbWriteData]);
                saw_x31 = true;
            }
        }
        REQUIRE(saw_x5);
        REQUIRE_FALSE(saw_x31);

        // ---- ② 非法字 MUST 在 ID 级被**点亮**（reason = 2）且**只在碰撞拍** ----
        //      ⇒ ⇒ 「被检出」与「不被执行」**同时**成立（检出但不误执行）。
        std::size_t illegal_cycles = 0;
        for (std::size_t c = 0; c < t.size(); ++c) {
            INFO("cycle " << c);
            if (t[c][kIdxIllegalReason] == 2u) ++illegal_cycles;
        }
        REQUIRE(illegal_cycles >= 1u);
        // ⇒ 碰撞拍**之后** MUST 回落到 0（flush 杀掉错误路径 ⇒ 冻结域结束）
        REQUIRE(t.back()[kIdxIllegalReason] == 0u);
    }
}

// ===========================================================================
// 提交事件端口的**脉冲性**（2026-11-24 · change `rv32i-commit-event-pulse-2026-11`）
//
// ⚠️ **固件 MUST 是「3 条 rd≠0 的写回类指令 + 1 条冷 load」**（关键 · **实测得出**
//    的充要条件，不是推测 —— 详见下面的推导）：
//
//   **灌水（事件电平被拉长）的充要条件** = 「数据通路反压（`mem_stall`）发生时，
//   MEM/WB 里恰好装着一��� `rd != 0` 的**写回类**指令（ALU / load / jump）」：
//   ① `wb_stage.write_en = needs_writeback & rd_not_zero & mem_valid`
//      （`include/cpu/pipeline/stages/wb_stage.h:110`）；
//   ② `mem_valid` 来自 **MEM/WB 寄存器**（`rv32i_pipeline.h:662`）；
//   ③ MEM/WB 在 `mem_stall` 时**冻结**（`rv32i_pipeline.h:637-652`）
//      ⇒ ⇒ 被冻结的那条指令在 WB 级**停留多拍** ⇒ ⇒ 写回使能**持续为高**。
//
//   ⇒ ⇒ **MEM/WB 装的是「EX/MEM 里那条指令的前 2 条」**（级序：EX/MEM → M2 →
//   MEM/WB ⇒ ⇒ **落后 2 级**）⇒ ⇒ 故冷 load 前 MUST 有 **≥ 2 条** `rd != 0` 的
//   写回类指令 ⇒ ⇒ **「load 紧跟 load」不够**（第一条 load 之前没有任何指令
//   ⇒ ⇒ MEM/WB 里是空的 ⇒ ⇒ **不触发**）—— 2026-11-24 实测：固件
//   `lw x2,0x100` · `lw x3,0x120` · `jal x0,0` 在**旧实现**下
//   `wb_high = 2`（= 写回条数 ⇒ **零灌水**）⇒ ⇒ 该形态的判据**恒绿 = 无效判据**。
//   ⇒ ⇒ 同理，change 材料 AC1 的程序（`addi x1` · `lw x2,0` · `addi x3` · `jal`）
//   在**旧实现**下 `wb_high = 3`（= 写回条数 ⇒ **零灌水**）⇒ ⇒ **必须改固件**。
//
// ⚠️ **期望值全部手推**（禁抄仿真输出）：
//   · 固件 = `addi x2,x0,7` · `addi x3,x0,9` · `addi x4,x0,11`
//            · `lw x5,0x100(x0)` · `jal x0,0`；
//   · `lw x5, 0x100(x0)` ⇒ **冷行**（D-L1 的 line = 16 B ⇒ `0x100` 未被填过）
//     ⇒ ⇒ D-L1 miss ⇒ ⇒ 4-beat burst fill ⇒ ⇒ MEM 级**多拍冻结**（实测 8 拍）；
//   · **写回事件 = 4**（x2 / x3 / x4 三条 addi + x5 一条 lw）
//     ⇒ ⇒ `jal x0,0` 的 `rd = x0` ⇒ **不写回**（`wb_stage` 的 x0 保护）；
//   · **每个 rd 恰好写回一次**（x2 / x3 / x4 / x5）；
//   · **x5 的数据 = 0**（D-TCM 零初始化 ⇒ 该地址无写过 ⇒ 读到 0）；
//   · **store 事件 = 0**（本固件无 store）⇒ ⚠️ 该条在旧实现下**也**为 0
//     ⇒ ⇒ **恒绿 = 无效判据** ⇒ ⇒ **不写进判据**（store 事件的脉冲化判别力
//     归 **S4a**：MMIO 事务必然多拍 ⇒ 那时才可判别）。
//
// ⚠️ **旧实现下的实测值（判别力依据）**：`wb_high = 12`（= 4 + 8 停顿拍）
//    ⇒ ⇒ 「事件拍数 == 事件次数」**必红** ⇒ ⇒ 判据**有判别力**。
// ===========================================================================
constexpr std::array<uint32_t, 5> kPulseFirmware = {{
    ADDI(2, 0, 7),    // 0  addi x2, x0, 7    —— rd≠0 的写回类（灌水的「载体」）
    ADDI(3, 0, 9),    // 1  addi x3, x0, 9    —— rd≠0 的写回类
    ADDI(4, 0, 11),   // 2  addi x4, x0, 11   —— rd≠0 的写回类
    LW(5, 0, 0x100),  // 3  lw  x5, 0x100(x0) —— **冷行** ⇒ miss ⇒ 多拍冻结
    JAL(0, 0),        // 4  jal x0, 0         —— 自环；rd = x0 ⇒ **不写回**
}};

DUAL_BACKEND_TEST_PAIR(Rv32iCoreTop,
                        "Rv32iSoc - CommitEventPulse",
                        "[rv32i][soc][commit][pulse][dual][verilator][e2e]",
                        [&]() {
    // ① 编码器 × 手算常量交叉校验（防自证 —— 用库编码器自证 = 零判别力）
    REQUIRE(ADDI(2, 0, 7) == 0x00700113u);
    REQUIRE(ADDI(3, 0, 9) == 0x00900193u);
    REQUIRE(ADDI(4, 0, 11) == 0x00B00213u);
    REQUIRE(LW(5, 0, 0x100) == 0x10002283u);
    REQUIRE(JAL(0, 0) == 0x0000006Fu);

    load_firmware(sim, dev.instance(), kPulseFirmware);
    const Trace ta = run_trace(sim, dev.instance(), 60);

    std::size_t wb_events = 0;      // 写回**事件**次数（脉冲计数）
    std::size_t dup_adjacent = 0;   // 相邻两拍都高 **且 (rd,data) 相同** ⇒ 重复写回
    std::size_t rd_hits[32] = {};    // 每个 rd 的写回事件数
    uint64_t x5_data = 0;
    bool x5_seen = false;
    for (std::size_t c = 0; c < ta.size(); ++c) {
        const bool wb = ta[c][kIdxWbWriteEn] == 1u;
        if (wb) {
            ++wb_events;
            ++rd_hits[static_cast<std::size_t>(ta[c][kIdxWbRdAddr])];
            if (ta[c][kIdxWbRdAddr] == 5u) {
                x5_data = ta[c][kIdxWbWriteData];
                x5_seen = true;
            }
            if (c + 1 < ta.size() && ta[c + 1][kIdxWbWriteEn] == 1u &&
                ta[c + 1][kIdxWbRdAddr] == ta[c][kIdxWbRdAddr] &&
                ta[c + 1][kIdxWbWriteData] == ta[c][kIdxWbWriteData]) {
                ++dup_adjacent;
            }
        }
    }
    CAPTURE(wb_events, dup_adjacent, rd_hits[2], rd_hits[3], rd_hits[4],
            rd_hits[5]);

    // ② AC1：**事件次数** MUST == 程序里「写回类且 rd≠0」的指令条数（手推 = 4）
    //   ⇒ ⇒ 这是「事件次数」与「事件时长之和」的**分离判据**
    //   ⇒ ⇒ 旧实现（电平）⇒ 停顿期间同一条指令停留多拍 ⇒ 拍数 = 4 + 8 = 12 ⇒ 必红
    CHECK(wb_events == 4u);

    // ③ AC2：每个 rd MUST **恰好**写回一次（x2 / x3 / x4 / x5）
    //   ⇒ ⇒ 正面覆盖「同一条指令被重复写回」（旧实现：x3 写回 9 次 ⇒ 必红）
    CHECK(rd_hits[2] == 1u);
    CHECK(rd_hits[3] == 1u);
    CHECK(rd_hits[4] == 1u);
    CHECK(rd_hits[5] == 1u);

    // ④ AC3：**相邻两拍都高且 (rd,data) 相同** ⇒ MUST 为 0
    //   ⇒ ⇒ 旧实现：x3 在 8 拍停顿期间连续重复 ⇒ 必红
    CHECK(dup_adjacent == 0u);

    // ⑤ AC4：写回**数据** MUST 正确（x5 = 冷读 D-TCM ⇒ 0）
    //   ⇒ ⇒ 证明「脉冲化没有改变『哪条指令被写回、写回什么』」
    CHECK(x5_seen);
    CHECK(x5_data == 0u);
});
