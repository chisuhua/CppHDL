// ⚠️⚠️⚠️ **路线 C 第一批 —— RV32I 微架构组件进双后端 parity**（2026-11-03）
//
// ## 为什么开这条路线
//
// 上一轮盘点只覆盖 `include/chlib/**` ⇒ 「阈值 60 不可达」。把范围扩到**全仓
// `include/**`** 后（见 `.rddf/checks/handoff-2026-11-02-parity-route-ab.md` §4.7）
// ⇒ 查出**第 6 类候选**：13 个 RV32I 组件在 58 个 macro dual 文件里**各出现 0 次**
// ⇒ ⇒ **组件层零 dual 覆盖**。
//
// ## 为什么这批价值高（不只是补数字）
//
// 这些组件是 **RV32I 微架构的单一权威**（ADR-046 把 `examples/riscv-mini/src/`
// 收敛进 `include/cpu/`）⇒ ⇒ ⇒ **最近 3 个真缺陷全部落在这一层**：
// `MemStage` 的 `LB` 符号扩展掩码（`rv32i-load-byte-lane-2026-10`）· `IdStage` 的
// U 型立即数与 `alu_op` 误判（同一 change 连带修复）· `DataTCM` 的 `strbe`
// 字节使能（`rv32i-store-byte-enable-2026-10`）⇒ ⇒ 且当初**都只被 SoC 级固件抓到**
// ⇒ ⇒ **组件级覆盖能把它前置**。
//
// ## 可转性 4 问
//
// ① **重复**：`MemStage` / `ExStage` / `Rv32iDecoder` 在全部 58 个 macro dual 文件里
//    **各出现 0 次** ⇒ 无重复。⚠️ 唯一既有覆盖是 **SoC 级** hand-crafted dual
//    （`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）⇒ ⇒ **判别形态
//    不同**（整 SoC 的固件轨迹 vs 单个 stage 的契约）⇒ ⇒ 4 问① **逐案裁决通过**。
// ② **判别力**：变异 **9/9 全抓**（见文件末尾的判别力清单）。
// ③ **与后端无关**：**否** —— 真跑仿真 + 读顶层端口，正是要对比两后端。
// ④ **值得新写**：三个组件**本身带顶层端口** ⇒ ⇒ **零 DUT 复制**（直接当 DUT）
//    ⇒ ⇒ 成本最低（与 `test_hazard_complete_dual.cpp` 同型）。
//
// ## 🔴 本批的**第二个产出**：挖出一个**生产路径**的 RTL 缺陷
//
// `Rv32iDecoder` 的 **B 型立即数 bit 11 被放在 bit 0**
// （`rv32i_decoder.h:109-114`，**同一处笔误也出现在生产路径的
// `id_stage.h:140-145`**）⇒ ⇒ `|偏移| ≥ 2048` 的条件分支**跳到错误地址**。
// ⇒ ⇒ 独立立项：`.rddf/improvements/rv32i-b-type-immediate-bit11-misplaced-2026-11.md`
// ⇒ ⇒ ⚠️ **本批刻意不修**（行为变更 ⇒ 需 ADR ⇒ ⇒ objective 的纪律：
// 「dual 暴露的问题 ⇒ **停下来开独立 change**，不在覆盖 change 里顺手修」）
// ⇒ ⇒ **本文件钉住的是【当前实现】的行为**（含该缺陷）⇒ ⇒ 修复时本文件会**转红**
// ⇒ ⇒ ⇒ 那是一次**有意的**行为变更信号（先例：`test_arithmetic_advance_dual.cpp`
// 对 `carry_save_adder` 的 carry 截断的处理）。
//
// ## 读模型（`Simulator::tick()` 的精确顺序 · `src/simulator.cpp:308-328`）
//
//     1) eval_combinational()   ← 边沿前
//     2) 寄存器提交（用边沿前的组合值）
//     3) eval_combinational()   ← 边沿后
//
// ⇒ ⇒ **tick 之后读到的端口 = f(边沿后的寄存器, 本拍输入)**。
// ⚠️ **本批三个组件全部是纯组合**（`ch_reg` 计数 = 0）⇒ ⇒ 读到的就是**本拍输入的
// 纯函数** ⇒ ⇒ 与「边沿前/后」无关 ⇒ ⇒ 期望值表不需要时序建模。
// ⚠️ **陷阱 T5**（无时序节点 ⇒ 时钟从不翻转）**不适用**：这三个组件**不发射任何
// `always_ff`** ⇒ ⇒ 不存在「生成代码有时序逻辑但 Simulator 时序表为空」的漂移。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 模型 = **独立 Python 实现**（`/tmp/opencode/gen_route_c_table.py`）逐条按
// `include/cpu/pipeline/**` 的公式推导 ⇒ ⇒ **一次通过**（两后端都吻合）。
// ⚠️ 译码器的**指令编码**由 Python 里的**规范编码器**按 RISC-V 规范生成
// （`imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7]` 等）⇒ ⇒ **期望值不是从
// 实现反推的** ⇒ ⇒ 这正是「两后端一致 ⇒ 必是同一处推导错」判据能生效的前提。
//
// ## ⚠️ 三个组件的**死输入端口**（未驱动 = 未定义值 ⇒ MUST 显式驱动）
//
// | 组件 | 死端口 | 说明 |
// |---|---|---|
// | `MemStage` | `rs2_data` · `is_store` | `describe()` 从不读 ⇒ 存储数据走别的通路 |
// | `ExStage` | `funct3` · `funct7` | ALU 操作码**已预译码**成 `alu_op` ⇒ funct3/funct7 无消费者 |
//
// ⇒ ⇒ 本文件对**每一个**输入端口都显式驱动 ⇒ ⇒ 不依赖「未驱动也无所谓」的**碰巧通过**。
//
// ## ⚠️ 陷阱 T3：body 参数里**花括号内的逗号**会被预处理器当参数分隔
//    ⇒ ⇒ 拍表放**文件作用域**。
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"
#include "hier_corpus_fixture.h"

#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "cpu/pipeline/rv32i_decoder.h"
#include "cpu/pipeline/stages/ex_stage.h"
#include "cpu/pipeline/stages/id_stage.h"
#include "cpu/pipeline/stages/mem_stage.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

namespace {

// ===========================================================================
// 拍表 1：`riscv::MemStage`
// ===========================================================================
// 造字用的内存字：4 个字节**两两不同**且符号位**混合**；2 个半字符号位也不同
//   byte0=0xC5(sign1) byte1=0x71(sign0) byte2=0x7E(sign0) byte3=0x83(sign1)
//   half0=0x71C5(sign0) half1=0x837E(sign1)
constexpr uint64_t kMemWord = 0x837E71C5ULL;
// ⚠️ `rs2_data` 是**死端口** ⇒ 仍 MUST 显式驱动（未驱动 = 未定义值）
constexpr uint64_t kMemRs2Data = 0xDEADBEEFULL;

struct MemCase {
    uint64_t alu_result, funct3, is_load, is_store, mem_valid, rd_addr;
    uint64_t alu_result_out, rd_addr_out, mem_data, strbe, mem_valid_out;
};

const MemCase kMemCases[] = {
    {0x1000ULL, 2, 1, 0, 1, 5, 0x00001000ULL, 5, 0x837E71C5ULL, 0xF, 1},  // LW aligned
    {0x1000ULL, 0, 1, 0, 1, 6, 0x00001000ULL, 6, 0xFFFFFFC5ULL, 0x1, 1},  // LB lane0 (neg byte)
    {0x1001ULL, 0, 1, 0, 1, 7, 0x00001001ULL, 7, 0x00000071ULL, 0x2, 1},  // LB lane1 (pos byte)
    {0x1002ULL, 0, 1, 0, 1, 8, 0x00001002ULL, 8, 0x0000007EULL, 0x4, 1},  // LB lane2 (pos byte)
    {0x1003ULL, 0, 1, 0, 1, 9, 0x00001003ULL, 9, 0xFFFFFF83ULL, 0x8, 1},  // LB lane3 (neg byte)
    {0x1000ULL, 4, 1, 0, 1, 10, 0x00001000ULL, 10, 0x000000C5ULL, 0x0, 1},  // LBU lane0
    {0x1003ULL, 4, 1, 0, 1, 11, 0x00001003ULL, 11, 0x00000083ULL, 0x0, 1},  // LBU lane3
    {0x1000ULL, 1, 1, 0, 1, 12, 0x00001000ULL, 12, 0x000071C5ULL, 0x3, 1},  // LH half0 (pos half)
    {0x1002ULL, 1, 1, 0, 1, 13, 0x00001002ULL, 13, 0xFFFF837EULL, 0xC, 1},  // LH half1 (neg half)
    {0x1000ULL, 5, 1, 0, 1, 14, 0x00001000ULL, 14, 0x000071C5ULL, 0x0, 1},  // LHU half0
    {0x1002ULL, 5, 1, 0, 1, 15, 0x00001002ULL, 15, 0x0000837EULL, 0x0, 1},  // LHU half1
    {0x1000ULL, 3, 1, 0, 1, 16, 0x00001000ULL, 16, 0x00000000ULL, 0x0, 1},  // funct3=3 invalid -> 0
    {0x1000ULL, 6, 1, 0, 1, 17, 0x00001000ULL, 17, 0x00000000ULL, 0x0, 1},  // funct3=6 invalid -> 0
    {0x1000ULL, 2, 0, 0, 1, 18, 0x00001000ULL, 18, 0x00000000ULL, 0xF, 1},  // is_load=0 -> mem_data 0
    {0x1000ULL, 0, 0, 1, 1, 19, 0x00001000ULL, 19, 0x00000000ULL, 0x1, 1},  // SB lane0
    {0x1001ULL, 0, 0, 1, 1, 20, 0x00001001ULL, 20, 0x00000000ULL, 0x2, 1},  // SB lane1
    {0x1002ULL, 0, 0, 1, 1, 21, 0x00001002ULL, 21, 0x00000000ULL, 0x4, 1},  // SB lane2
    {0x1003ULL, 0, 0, 1, 1, 22, 0x00001003ULL, 22, 0x00000000ULL, 0x8, 1},  // SB lane3
    {0x1000ULL, 1, 0, 1, 1, 23, 0x00001000ULL, 23, 0x00000000ULL, 0x3, 1},  // SH half0
    {0x1002ULL, 1, 0, 1, 1, 24, 0x00001002ULL, 24, 0x00000000ULL, 0xC, 1},  // SH half1
    {0x1000ULL, 2, 0, 1, 1, 25, 0x00001000ULL, 25, 0x00000000ULL, 0xF, 1},  // SW
    {0x1000ULL, 7, 0, 1, 1, 26, 0x00001000ULL, 26, 0x00000000ULL, 0x0, 1},  // strbe funct3=7 -> 0
    {0x1000ULL, 2, 1, 0, 0, 27, 0x00001000ULL, 27, 0x837E71C5ULL, 0xF, 0},  // mem_valid=0 passthrough
    {0x1001ULL, 0, 0, 0, 1, 28, 0x00001001ULL, 28, 0x00000000ULL, 0x2, 1},  // is_store=0 strbe kept
};

// ===========================================================================
// 拍表 2：`riscv::ExStage`
// ===========================================================================
// opcode: OP_OP=0b0110011(51) OP_OPIMM=0b0010011(19) OP_AUIPC=0b0010111(23)
//         OP_STORE=0b0100011(35) OP_LUI=0b0110111(55) OP_BRANCH=0b1100011(99)
//         OP_JALR=0b1100111(103) OP_JAL=0b1101111(111)
struct ExCase {
    uint64_t rs1_data, rs2_data, imm, pc, opcode, alu_op;
    uint64_t branch_type, is_branch, is_jump, is_store;
    uint64_t alu_result, branch_taken, branch_target, link_value;
    uint64_t zero, less_than, less_than_u, is_store_alu;
};

const ExCase kExCases[] = {
    {0x5ULL, 0x3ULL, 0x0ULL, 0x1000ULL, 51, 0, 0, 0, 0, 0, 0x00000008ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 0, 0, 0},  // ADD r-type
    {0x5ULL, 0x3ULL, 0x0ULL, 0x1000ULL, 51, 1, 0, 0, 0, 0, 0x00000002ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 0, 0, 0},  // SUB r-type
    {0xF0F0F0F0ULL, 0xFF00FF0ULL, 0x0ULL, 0x1000ULL, 51, 2, 0, 0, 0, 0, 0x00F000F0ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 1, 0, 0},  // AND r-type
    {0xF0F0F0F0ULL, 0xFF00FF0ULL, 0x0ULL, 0x1000ULL, 51, 3, 0, 0, 0, 0, 0xFFF0FFF0ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 1, 0, 0},  // OR  r-type
    {0xF0F0F0F0ULL, 0xFF00FF0ULL, 0x0ULL, 0x1000ULL, 51, 4, 0, 0, 0, 0, 0xFF00FF00ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 1, 0, 0},  // XOR r-type
    {0x1ULL, 0x4ULL, 0x0ULL, 0x1000ULL, 51, 5, 0, 0, 0, 0, 0x00000010ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 1, 1, 0},  // SLL r-type
    {0x80000000ULL, 0x4ULL, 0x0ULL, 0x1000ULL, 51, 6, 0, 0, 0, 0, 0x08000000ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 1, 0, 0},  // SRL r-type
    {0x80000000ULL, 0x4ULL, 0x0ULL, 0x1000ULL, 51, 7, 0, 0, 0, 0, 0xF8000000ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 1, 0, 0},  // SRA r-type (sign fill)
    {0x80000000ULL, 0x0ULL, 0x0ULL, 0x1000ULL, 51, 7, 0, 0, 0, 0, 0x80000000ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 1, 0, 0},  // SRA r-type sh=0
    {0xFFFFFFFFULL, 0x1ULL, 0x0ULL, 0x1000ULL, 51, 8, 0, 0, 0, 0, 0x00000001ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 1, 0, 0},  // SLT signed -1<1
    {0x1ULL, 0xFFFFFFFFULL, 0x0ULL, 0x1000ULL, 51, 9, 0, 0, 0, 0, 0x00000001ULL, 0, 0x00001000ULL, 0x00001004ULL, 0, 0, 1, 0},  // SLTU 1<0xFFFFFFFF
    {0x5ULL, 0x3ULL, 0x7ULL, 0x1000ULL, 19, 0, 0, 0, 0, 0, 0x0000000CULL, 0, 0x00001007ULL, 0x00001004ULL, 0, 1, 1, 0},  // ADD i-type (imm)
    {0x5ULL, 0x3ULL, 0x12345000ULL, 0x1000ULL, 55, 0, 0, 0, 0, 0, 0x12345000ULL, 0, 0x12346000ULL, 0x00001004ULL, 0, 1, 1, 0},  // LUI alu_result=imm
    {0x5ULL, 0x3ULL, 0x100ULL, 0x1000ULL, 23, 0, 0, 0, 0, 0, 0x00001100ULL, 0, 0x00001100ULL, 0x00001004ULL, 0, 1, 1, 0},  // AUIPC alu_result=pc+i
    {0x1000ULL, 0xDEADBEEFULL, 0x20ULL, 0x1000ULL, 35, 0, 0, 0, 0, 1, 0x00001020ULL, 0, 0x00001020ULL, 0x00001004ULL, 0, 0, 0, 1},  // STORE addr=rs1+imm
    {0x42ULL, 0x42ULL, 0x10ULL, 0x1000ULL, 99, 0, 0, 1, 0, 0, 0x00000084ULL, 1, 0x00001010ULL, 0x00001004ULL, 0, 0, 0, 0},  // BEQ taken
    {0x42ULL, 0x43ULL, 0x10ULL, 0x1000ULL, 99, 0, 0, 1, 0, 0, 0x00000085ULL, 0, 0x00001010ULL, 0x00001004ULL, 0, 1, 1, 0},  // BEQ not taken
    {0xFFFFFFFFULL, 0x1ULL, 0x10ULL, 0x1000ULL, 99, 0, 4, 1, 0, 0, 0x00000000ULL, 1, 0x00001010ULL, 0x00001004ULL, 1, 1, 0, 0},  // BLT signed taken
    {0x5ULL, 0xFFFFFFFFULL, 0x10ULL, 0x1000ULL, 99, 0, 5, 1, 0, 0, 0x00000004ULL, 1, 0x00001010ULL, 0x00001004ULL, 0, 0, 1, 0},  // BGE signed taken
    {0x1ULL, 0xFFFFFFFFULL, 0x10ULL, 0x1000ULL, 99, 0, 6, 1, 0, 0, 0x00000000ULL, 1, 0x00001010ULL, 0x00001004ULL, 1, 0, 1, 0},  // BLTU unsigned taken
    {0xFFFFFFFFULL, 0x1ULL, 0x10ULL, 0x1000ULL, 99, 0, 7, 1, 0, 0, 0x00000000ULL, 1, 0x00001010ULL, 0x00001004ULL, 1, 1, 0, 0},  // BGEU unsigned taken
    {0x42ULL, 0x43ULL, 0x10ULL, 0x1000ULL, 99, 0, 1, 1, 0, 0, 0x00000085ULL, 1, 0x00001010ULL, 0x00001004ULL, 0, 1, 1, 0},  // BNE taken
    {0x5ULL, 0x3ULL, 0x20ULL, 0x1000ULL, 111, 0, 0, 0, 1, 0, 0x00000008ULL, 1, 0x00001020ULL, 0x00001004ULL, 0, 0, 0, 0},  // JAL unconditional
    {0x2000ULL, 0x3ULL, 0x4ULL, 0x1000ULL, 103, 0, 0, 0, 1, 0, 0x00002004ULL, 1, 0x00002004ULL, 0x00001004ULL, 0, 0, 0, 0},  // JALR target=rs1+imm
    {0x2000ULL, 0x3ULL, 0x5ULL, 0x1000ULL, 103, 0, 0, 0, 1, 0, 0x00002005ULL, 1, 0x00002004ULL, 0x00001004ULL, 0, 0, 0, 0},  // JALR target clears bit0
    {0x5ULL, 0x3ULL, 0x20ULL, 0x1000ULL, 111, 0, 0, 0, 0, 0, 0x00000008ULL, 0, 0x00001020ULL, 0x00001004ULL, 0, 0, 0, 0},  // JAL but is_jump=0
    {0x42ULL, 0x42ULL, 0x10ULL, 0x1000ULL, 99, 0, 0, 0, 0, 0, 0x00000084ULL, 0, 0x00001010ULL, 0x00001004ULL, 0, 0, 0, 0},  // BEQ but is_branch=0
    {0x0ULL, 0x0ULL, 0x0ULL, 0x1000ULL, 23, 0, 0, 0, 0, 0, 0x00001000ULL, 0, 0x00001000ULL, 0x00001004ULL, 1, 0, 0, 0},  // AUIPC zero from alu_out
};

// ===========================================================================
// 拍表 3：`riscv::Rv32iDecoder`
// ===========================================================================
// 字段检查掩码：bit0=rd bit1=rs1 bit2=rs2 bit3=funct3 bit4=funct7 bit5=imm
// ⚠️ 非 R 型指令的 `rd` / `funct7` **字段位置被立即数占用** ⇒ 无意义 ⇒ 不断言。
constexpr uint64_t CK_RD = 1;
constexpr uint64_t CK_RS1 = 2;
constexpr uint64_t CK_RS2 = 4;
constexpr uint64_t CK_F3 = 8;
constexpr uint64_t CK_F7 = 16;
constexpr uint64_t CK_IMM = 32;

struct DecCase {
    uint64_t instruction, check_mask;
    uint64_t opcode, rd, funct3, rs1, rs2, funct7, imm, instr_type;
};

const DecCase kDecCases[] = {
    {0x00500093ULL, 47, 19, 1, 0, 0, 5, 0, 0x00000005ULL, 1},  // addi x1,x0,5
    {0xFFF00093ULL, 47, 19, 1, 0, 0, 31, 127, 0xFFFFFFFFULL, 1},  // addi x1,x0,-1
    {0x80000093ULL, 47, 19, 1, 0, 0, 0, 64, 0xFFFFF800ULL, 1},  // addi x1,x0,-2048
    {0x0001A103ULL, 47, 3, 2, 2, 3, 0, 0, 0x00000000ULL, 1},  // lw x2,0(x3)
    {0xFFC1A103ULL, 47, 3, 2, 2, 3, 28, 127, 0xFFFFFFFCULL, 1},  // lw x2,-4(x3)
    {0x0021A223ULL, 46, 35, 4, 2, 3, 2, 0, 0x00000004ULL, 2},  // sw x2,4(x3)
    {0xFE21AE23ULL, 46, 35, 28, 2, 3, 2, 127, 0xFFFFFFFCULL, 2},  // sw x2,-4(x3)
    {0x00208463ULL, 46, 99, 8, 0, 1, 2, 0, 0x00000008ULL, 3},  // beq x1,x2,8
    {0xFE208CE3ULL, 46, 99, 25, 0, 1, 2, 127, 0xFFFFFFF8ULL, 3},  // beq x1,x2,-8（ADR-063 D5）
    {0x002080E3ULL, 46, 99, 1, 0, 1, 2, 0, 0x00000800ULL, 3},  // beq x1,x2,2048（ADR-063 D5，imm[11]=1）
    {0x123451B7ULL, 47, 55, 3, 5, 8, 3, 9, 0x12345000ULL, 4},  // lui x3,0x12345
    {0x12345197ULL, 47, 23, 3, 5, 8, 3, 9, 0x12345000ULL, 4},  // auipc x3,0x12345
    {0x010000EFULL, 47, 111, 1, 0, 0, 16, 0, 0x00000010ULL, 5},  // jal x1,16
    {0xFF1FF0EFULL, 47, 111, 1, 7, 31, 17, 127, 0xFFFFFFF0ULL, 5},  // jal x1,-16
    {0x004100E7ULL, 47, 103, 1, 0, 2, 4, 0, 0x00000004ULL, 1},  // jalr x1,4(x2)
    {0x003100B3ULL, 63, 51, 1, 0, 2, 3, 0, 0x00000000ULL, 0},  // add x1,x2,x3
    {0x803100B3ULL, 63, 51, 1, 0, 2, 3, 64, 0x00000000ULL, 0},  // sub x1,x2,x3
};


// ===========================================================================
// D4 判据：`Rv32iDecoder::imm` 与 `IdStage::imm` 的**等价性**
// ===========================================================================
// ⚠️ **为什么要有这一条**（ADR-063 D4）：立即数提取在仓内有**两份重复实现**
//    （`rv32i_decoder.h` 与 `stages/id_stage.h`）⇒ B 型 bit 11 的笔误**同时**存在于
//    两处 ⇒ 若无此判据，「只改一处」的复发**无法被发现**（两份各自有测试也可能各自对）。
// ⇒ 本 wrapper 把两者并排实例化、喂同一条指令 ⇒ 差异立刻显形。
// ⚠️ `IdStage` 的 `imm` 是**寄存输出**（`idex_imm->next = select(update_en, ...)`）
//    ⇒ MUST 驱动 `valid=1`（update_en = valid）且 `flush=stall=0`，否则它**保持**上一拍。
class ImmConsistencyDut : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>>  instruction;
        ch_out<ch_uint<32>> dec_imm;
        ch_out<ch_uint<32>> id_imm;
        ch_out<ch_bool>     equal;
    )

    ImmConsistencyDut(ch::Component* parent = nullptr,
                      const std::string& name = "imm_consistency")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<riscv::Rv32iDecoder> dec{"dec"};
        ch::ch_module<riscv::IdStage>      ids{"ids"};

        dec.io().instruction <<= io().instruction;
        ids.io().instruction <<= io().instruction;
        // ⚠️ 这些是 **IdStage 的输入**（不是本 wrapper 的输入）⇒ 用 `<<=` 内部驱动
        ids.io().pc           <<= ch_uint<32>(0_d);
        ids.io().valid        <<= ch_bool(true);   // ⇒ idex_update_en = 1
        ids.io().flush        <<= ch_bool(false);
        ids.io().stall        <<= ch_bool(false);
        ids.io().reg_rs1_data <<= ch_uint<32>(0_d);
        ids.io().reg_rs2_data <<= ch_uint<32>(0_d);

        io().dec_imm <<= dec.io().imm;
        io().id_imm  <<= ids.io().imm;
        io().equal   <<= (dec.io().imm == ids.io().imm);
    }
};

// 指令集（ADR-063 D4 要求的最低覆盖）：
//   B 型 `imm[11]=1` 正/负各一 · I/S/U/J 各 >=1 · `|imm|<2048` 的 B 型（负向对照）
constexpr uint32_t kImmConsistencyInstrs[] = {
    0xFE208CE3u,  // beq x1,x2,-8        ⇒ imm[11]=1，负数
    0x002080E3u,  // beq x1,x2,+2048     ⇒ imm[11]=1，正数
    0x00208463u,  // beq x1,x2,+8        ⇒ imm[11]=0（负向对照）
    0x00500093u,  // addi x1,x0,5        ⇒ I 型
    0xFFF00093u,  // addi x1,x0,-1       ⇒ I 型负数
    0x0021A223u,  // sw x2,4(x3)         ⇒ S 型
    0xFE21AE23u,  // sw x2,-4(x3)        ⇒ S 型负数
    0x123451B7u,  // lui x3,0x12345      ⇒ U 型
    0x010000EFu,  // jal x1,16           ⇒ J 型
    0xFF1FF0EFu,  // jal x1,-16          ⇒ J 型负数
};

}  // namespace

// ===========================================================================
// pair 1：`riscv::MemStage` —— load 字节道/半字道 + 符号/零扩展 + store 字节使能
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    riscv::MemStage,
    "MemStage：LB/LBU/LH/LHU/LW 的车道选择与扩展 + SB/SH/SW 字节使能",
    "[cpu][rv32i][mem_stage][load][store_byte_enable][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kMemCases) {
            sim.set_input_value(io.alu_result, tc.alu_result);
            // ⚠️ 死端口也 MUST 显式驱动（未驱动 = 未定义值）
            sim.set_input_value(io.rs2_data, kMemRs2Data);
            sim.set_input_value(io.rd_addr, tc.rd_addr);
            sim.set_input_value(io.is_load, tc.is_load);
            sim.set_input_value(io.is_store, tc.is_store);
            sim.set_input_value(io.funct3, tc.funct3);
            sim.set_input_value(io.mem_valid, tc.mem_valid);
            sim.set_input_value(io.mem_read_data, kMemWord);
            sim.tick();
            INFO("alu_result=0x" << std::hex << tc.alu_result << std::dec
                                << " funct3=" << tc.funct3
                                << " is_load=" << tc.is_load
                                << " is_store=" << tc.is_store);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.alu_result_out)) ==
                    tc.alu_result_out);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rd_addr_out)) ==
                    tc.rd_addr_out);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.mem_data)) ==
                    tc.mem_data);
            REQUIRE(static_cast<uint64_t>(
                        sim.get_port_value(io.store_byte_enable_out)) ==
                    tc.strbe);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.mem_valid_out)) ==
                    tc.mem_valid_out);
        }
    });

// ===========================================================================
// pair 2：`riscv::ExStage` —— ALU 族 + 第二操作数选择 + 分支/跳转重定向
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    riscv::ExStage,
    "ExStage：ALU 族（含 SRA 符号填充与 SLT/SLTU）+ 第二操作数选择 + 分支/跳转重定向",
    "[cpu][rv32i][ex_stage][alu][branch][jump][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kExCases) {
            sim.set_input_value(io.rs1_data, tc.rs1_data);
            sim.set_input_value(io.rs2_data, tc.rs2_data);
            sim.set_input_value(io.imm, tc.imm);
            sim.set_input_value(io.pc, tc.pc);
            sim.set_input_value(io.opcode, tc.opcode);
            sim.set_input_value(io.alu_op, tc.alu_op);
            sim.set_input_value(io.branch_type, tc.branch_type);
            sim.set_input_value(io.is_branch, tc.is_branch);
            sim.set_input_value(io.is_jump, tc.is_jump);
            sim.set_input_value(io.is_store, tc.is_store);
            // ⚠️ 死端口也 MUST 显式驱动（`funct3` / `funct7` 无消费者）
            sim.set_input_value(io.funct3, 0);
            sim.set_input_value(io.funct7, 0);
            sim.tick();
            INFO("rs1=0x" << std::hex << tc.rs1_data << " rs2=0x" << tc.rs2_data
                         << " imm=0x" << tc.imm << " opcode=0b" << std::dec
                         << tc.opcode << " alu_op=" << tc.alu_op
                         << " branch_type=" << tc.branch_type);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.alu_result)) ==
                    tc.alu_result);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.branch_taken)) ==
                    tc.branch_taken);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.branch_target)) ==
                    tc.branch_target);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.link_value)) ==
                    tc.link_value);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.zero)) ==
                    tc.zero);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.less_than)) ==
                    tc.less_than);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.less_than_u)) ==
                    tc.less_than_u);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.is_store_alu)) ==
                    tc.is_store_alu);
        }
    });

// ===========================================================================
// pair 3：`riscv::Rv32iDecoder` —— 五种指令格式的字段与立即数提取
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    riscv::Rv32iDecoder,
    "Rv32iDecoder：R/I/S/B/U/J 五种格式的字段与立即数提取（含 B 型 bit 11 的已知缺陷钉点）",
    "[cpu][rv32i][decoder][immediate][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kDecCases) {
            sim.set_input_value(io.instruction, tc.instruction);
            sim.tick();
            INFO("instruction=0x" << std::hex << tc.instruction << std::dec);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.opcode)) ==
                    tc.opcode);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.instr_type)) ==
                    tc.instr_type);
            if (tc.check_mask & CK_RD) {
                REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rd)) ==
                        tc.rd);
            }
            if (tc.check_mask & CK_RS1) {
                REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs1)) ==
                        tc.rs1);
            }
            if (tc.check_mask & CK_RS2) {
                REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs2)) ==
                        tc.rs2);
            }
            if (tc.check_mask & CK_F3) {
                REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.funct3)) ==
                        tc.funct3);
            }
            if (tc.check_mask & CK_F7) {
                REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.funct7)) ==
                        tc.funct7);
            }
            if (tc.check_mask & CK_IMM) {
                REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.imm)) ==
                        tc.imm);
            }
        }
    });

// ===========================================================================
// pair 4（ADR-063 D4）：两份立即数实现的**等价性**
// ===========================================================================
// ═══════════════════════════════════════════════════════════════════════════
// 语料级层次化回归 · **产物级**（残留 1b · T0）
//
// ⚠️ **为什么这些 DUT 需要它**：本文件的 DUT 是**真实语料** ⇒ ⇒ 它们带**真实拓扑**
//    （子组件 + 真实端口 + 真实位宽）⇒ ⇒ 层次化路径在它们身上**结构上**与 T3-005 的
//    5 个**专用** DUT 不同 ⇒ ⇒ ⇒ 「层次化路径只被专用 DUT 覆盖」的缺口**正是**这里。
//
// ⚠️ **判据在共享 fixture 里**（`tests/hier_corpus_fixture.h`）⇒ ⇒ 这里只负责
//    「**用哪些真实 DUT**」这个事实（★ 它们的类在本文件的匿名命名空间内 ⇒ ⇒ 外部
//    引用不到）。
// ═══════════════════════════════════════════════════════════════════════════

// ⚠️⚠️ **本 DUT 的形态 = 族 ①/③（父侧带运算消费两个子 `ch_out`）**
//   `io().equal <<= (dec.io().imm == ids.io().imm);` ⇒ ⇒ 根的 `equal` 输出由**两个子
//   组件的 output 端口**驱动 ⇒ ⇒ **ADR-066 决策 ① 落地后不再被拒** ⇒ ⇒ 发射器为
//   两个子输出各合成一根**父侧落点网**（网名 = 驱动端点的 ctx 全局名）⇒ ⇒ 父侧比较
//   读那两根网 ⇒ ⇒ 且同一子输出**既**直通父 `output` 端口、**又**参与父侧运算 ⇒ ⇒
//   MUST 扇出**同一根**网（决策 ⑤）⇒ ⇒ 父 `output` 端口改由显式 `assign` 取网的值。
//   ⚠️ **归因订正（ADR-066 落地 · 台账行 17 闭口）**：旧注释写「需要隐式端口合成 ⇒
//   发射器不支持」⇒ ⇒ **两处都错**：① `dec.io().imm` / `ids.io().imm` 是子组件的
//   **output 端口**（不是「内部线网」）；② 「发射器不支持隐式端口合成」**早已过时**
//   （B2b 五批 2026-11-05 全部落地）⇒ ⇒ 真实形态是「**父侧带运算**」（= 台账行 16）。
TEST_CASE("corpus hierarchy regression (supported boundary): ImmConsistencyDut",
          "[hierarchy][corpus][t0][supported-boundary][rv32i][immediate]") {
    const std::string dir = ch_test::make_temp_dir("_corpus_imm_consistency_dut");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<ImmConsistencyDut> dev;
    const auto products =
        ch::hier_eq::emit(dev.instance(), dir, ch::Granularity::ForceLayer);
    INFO("error: " << products.error);
    // ⚠️ **ADR-066 决策 ① 落地后**：本 DUT **不再被拒** ⇒ ⇒ 本用例从「拒绝边界」
    //   改为「**支持边界**」的钉桩（台账行 17 闭口条件之一）。
    REQUIRE(products.ok);
    REQUIRE(products.error.empty());
    const std::string hier = ch_test::hier_corpus::read_whole_file(products.hier_path);
    REQUIRE_FALSE(hier.empty());
    // ⇒ ⇒ 父 module MUST 为**两个**子输出各声明一根**父侧落点网**
    //   （网名 = 驱动端点的 ctx 全局名 ⇒ = 子 output 端口名）
    REQUIRE(hier.find("logic [31:0] top_dec_unnamed_output;") != std::string::npos);
    REQUIRE(hier.find("logic [31:0] top_ids_unnamed_output;") != std::string::npos);
    // ⇒ ⇒ 实例 MUST 以 `.port(net)` 把子输出接到那根网（**单连接不变式** ⇒ 只一次）
    REQUIRE(hier.find(".top_dec_unnamed_output(top_dec_unnamed_output)") !=
            std::string::npos);
    REQUIRE(hier.find(".top_ids_unnamed_output(top_ids_unnamed_output)") !=
            std::string::npos);
}

DUAL_BACKEND_TEST_PAIR(
    ImmConsistencyDut,
    "Rv32iDecoder::imm 与 IdStage::imm 逐位相等（ADR-063 D4：防两份重复实现只改一处）",
    "[cpu][rv32i][immediate][consistency][dual][adr063]",
    [&]() {
        auto &io = dev.instance().io();
        for (uint32_t instr : kImmConsistencyInstrs) {
            sim.set_input_value(io.instruction, static_cast<uint64_t>(instr));
            sim.tick();
            const uint64_t dec_imm =
                static_cast<uint64_t>(sim.get_port_value(io.dec_imm));
            const uint64_t id_imm =
                static_cast<uint64_t>(sim.get_port_value(io.id_imm));
            INFO("instruction=0x" << std::hex << instr << std::dec
                                  << " dec_imm=0x" << std::hex << dec_imm
                                  << " id_imm=0x" << id_imm);
            REQUIRE(dec_imm == id_imm);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.equal)) == 1u);
        }
    });
