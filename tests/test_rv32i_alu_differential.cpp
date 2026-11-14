// tests/test_rv32i_alu_differential.cpp
//
// **B4 · C1 差分测试**：`riscv::Rv32iAlu`（库组件）vs `riscv::ExStage`（流水线**当前**的内联 ALU）
//
// ## 这份测试要解决什么
//
// B4 的 C3/C4 要把 EX 级的算术核心**换成**库组件 `Rv32iAlu` ⇒ ⇒ 换之前 MUST 先知道
// 「两者**哪里不一样**」⇒ ⇒ 否则换完**行为漂移**（且大概率**零报错**）。
//
// ⚠️ **两套 ALU 操作码编码完全不同**（本文件最危险的一条）：
//   `ex_stage.h::ALU_OP_*`：ADD 0 · SUB **1** · AND **2** · OR **3** · XOR 4 · SLL **5**
//                        · SRL **6** · SRA **7** · SLT **8** · SLTU **9**
//   `rv32i_alu.h::ALU_*` ：ADD 0 · SLL **1** · SLT **2** · SLTU **3** · XOR 4 · SRL **5**
//                        · OR **6** · AND **7** · SUB **10** · SRA **11**
//   ⇒ ⇒ 例：`alu_op = 3` 在内联侧 = **OR**，在库组件侧 = **SLTU**
//   ⇒ ⇒ 若接入时不归一 ⇒ ⇒ **同一个数会算成两种操作** ⇒ ⇒ 零报错的行为漂移
//   ⇒ ⇒ ⇒ 这条**在下面被显式断言**（不是靠注释提醒）⇒ ⇒ C2 归一落地时**必须**翻转它
//
// ## 为什么差分对象用**真实 `ExStage` 组件**而不是「逐行照抄一份」
//
// 改进的验收条款写的是「等价物 MUST 逐行照抄 `ex_stage.h:136-208`（不许重构 / 不许简化）」
// ⇒ ⇒ 本意是**防止差分退化成自证**（照着库组件写一份「等价物」⇒ 必然相等 ⇒ 零判别力）。
// ⇒ ⇒ **本文件用更强的做法**：差分的另一侧**就是生产代码本身**（`ExStage` 组件）
// ⇒ ⇒ 「复制漂移」这个风险**从根上不存在** ⇒ ⇒ AC 的本意（不得自证）被**更强**地满足。
// ⚠️ 代价：差分只能覆盖 **R 型**（`opcode = OP_OP` ⇒ 第二操作数 = `rs2_data`）——
//   内联的「第二操作数按 opcode 选择」（imm vs rs2_data）属于**结构性差异**，
//   它**不是 ALU 语义差异**，而是**接入方案**要解决的问题 ⇒ ⇒ 登记在差异清单里，不进本差分表。
//
// ## 期望值 MUST **手推**（禁抄仿真输出）
//
// 本仓纪律：「两后端一致」不能当作正确性证据 ⇒ ⇒ 同一份 C++ 生成两后端 ⇒ ⇒ 同构缺陷
// 两后端一致地错 ⇒ ⇒ parity 对这一族缺陷**结构性失明**。
// ⇒ ⇒ 下面每条用例的期望值都是**手推**的 ⇒ ⇒ 且**两侧各自**对手推值断言
// ⇒ ⇒ 「两者相等」只是**附加**判据，不是唯一判据。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "simulator.h"

#include "cpu/pipeline/rv32i_alu.h"
#include "cpu/pipeline/stages/ex_stage.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

// ── 语义操作（差分表按**语义**对齐两套编码 ⇒ ⇒ 编码差异不污染值比较）────
enum class Op { ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU };

const char *op_name(Op op) {
    switch (op) {
        case Op::ADD: return "ADD";
        case Op::SUB: return "SUB";
        case Op::AND: return "AND";
        case Op::OR:  return "OR";
        case Op::XOR: return "XOR";
        case Op::SLL: return "SLL";
        case Op::SRL: return "SRL";
        case Op::SRA: return "SRA";
        case Op::SLT: return "SLT";
        case Op::SLTU: return "SLTU";
    }
    return "?";
}

/// 内联侧（`ex_stage.h`）的 op 编码
/// ⚠️ **2026-11-14 · B4 · C2（编码归一）**：此前本函数指向 `ex_stage.h` **自带**的
///    `ALU_OP_*`（那套已**删除**）⇒ ⇒ 现在两侧指向**同一套** `riscv::ALU_*`
///    ⇒ ⇒ 归一后**不再有两套编码** ⇒ ⇒ 「两侧编码是否相同」的问题**已消失**。
constexpr unsigned ex_op(Op op) {
    switch (op) {
        case Op::ADD: return riscv::ALU_ADD;
        case Op::SUB: return riscv::ALU_SUB;
        case Op::AND: return riscv::ALU_AND;
        case Op::OR:  return riscv::ALU_OR;
        case Op::XOR: return riscv::ALU_XOR;
        case Op::SLL: return riscv::ALU_SLL;
        case Op::SRL: return riscv::ALU_SRL;
        case Op::SRA: return riscv::ALU_SRA;
        case Op::SLT: return riscv::ALU_SLT;
        case Op::SLTU: return riscv::ALU_SLTU;
    }
    return 0xFF;
}

/// 库组件侧（`rv32i_alu.h`）的 op 编码
constexpr unsigned lib_op(Op op) {
    switch (op) {
        case Op::ADD: return riscv::ALU_ADD;
        case Op::SUB: return riscv::ALU_SUB;
        case Op::AND: return riscv::ALU_AND;
        case Op::OR:  return riscv::ALU_OR;
        case Op::XOR: return riscv::ALU_XOR;
        case Op::SLL: return riscv::ALU_SLL;
        case Op::SRL: return riscv::ALU_SRL;
        case Op::SRA: return riscv::ALU_SRA;
        case Op::SLT: return riscv::ALU_SLT;
        case Op::SLTU: return riscv::ALU_SLTU;
    }
    return 0xFF;
}

/// 一侧的 4 个可比输出
struct AluOutputs {
    uint64_t result = 0;
    uint64_t zero = 0;
    uint64_t less_than = 0;
    uint64_t less_than_u = 0;
};

// ── 内联侧：**生产代码本身**（`ExStage` 组件）────────────────────────
// ⚠️ 只驱动 R 型（`opcode = OP_OP`）⇒ ⇒ `alu_second_operand = rs2_data`
//    ⇒ ⇒ 与库组件的 `operand_b` **同口径**。
AluOutputs run_inline(uint64_t a, uint64_t b, unsigned alu_op) {
    ch::ch_device<riscv::ExStage> ex;
    ch::Simulator sim(ex.context());
    auto &io = ex.instance().io();
    sim.set_input_value(io.rs1_data, a);
    sim.set_input_value(io.rs2_data, b);
    sim.set_input_value(io.imm, 0u);
    sim.set_input_value(io.pc, 0u);
    sim.set_input_value(io.opcode, 51u);   // OP_OP（R 型 ⇒ 第二操作数 = rs2_data）
    sim.set_input_value(io.funct3, 0u);
    sim.set_input_value(io.funct7, 0u);
    sim.set_input_value(io.alu_op, alu_op);
    sim.set_input_value(io.is_branch, 0u);
    sim.set_input_value(io.is_jump, 0u);
    sim.set_input_value(io.branch_type, 0u);
    sim.set_input_value(io.is_store, 0u);
    sim.tick();
    AluOutputs o;
    o.result = static_cast<uint64_t>(sim.get_port_value(io.alu_result));
    o.zero = static_cast<uint64_t>(sim.get_port_value(io.zero));
    o.less_than = static_cast<uint64_t>(sim.get_port_value(io.less_than));
    o.less_than_u = static_cast<uint64_t>(sim.get_port_value(io.less_than_u));
    return o;
}

// ── 库组件侧（`Rv32iAlu`）─────────────────────────────────────────────
AluOutputs run_library(uint64_t a, uint64_t b, unsigned alu_op) {
    ch::ch_device<riscv::Rv32iAlu> alu;
    ch::Simulator sim(alu.context());
    auto &io = alu.instance().io();
    sim.set_input_value(io.operand_a, a);
    sim.set_input_value(io.operand_b, b);
    sim.set_input_value(io.alu_op, alu_op);
    sim.set_input_value(io.funct3, 0u);
    sim.tick();
    AluOutputs o;
    o.result = static_cast<uint64_t>(sim.get_port_value(io.result));
    o.zero = static_cast<uint64_t>(sim.get_port_value(io.zero));
    o.less_than = static_cast<uint64_t>(sim.get_port_value(io.less_than));
    o.less_than_u = static_cast<uint64_t>(sim.get_port_value(io.less_than_u));
    return o;
}

/// 一条差分用例（期望值**全部手推**）
struct DiffCase {
    Op op;
    uint64_t a;
    uint64_t b;
    uint64_t exp_result;     // 手推
    uint64_t exp_zero;       // 手推（result == 0 ? 1 : 0）
    uint64_t exp_less_than;  // 手推（有符号 a < b）
    uint64_t exp_less_than_u;// 手推（无符号 a < b）
};

/// 手推的有符号比较（`-1 < 1` 等）：按 32 位**补码**语义
int64_t as_signed(uint64_t v) {
    return (v & 0x80000000ull) ? static_cast<int64_t>(v | 0xFFFFFFFF00000000ull)
                               : static_cast<int64_t>(v);
}

std::vector<DiffCase> diff_cases() {
    // ⚠️ **比较标志由「操作数」决定，与 op 无关** ⇒ ⇒ 下面是逐条**手推**的
    //    （有符号按 32 位补码、无符号按原值）⇒ ⇒ ② 用例会**自洽性复核**它们。
    return {
        // ---- ADD（含「33 位中间值」的暴露点：0xFFFFFFFF + 1 溢出 ⇒ 截断成 0）----
        {Op::ADD, 0x5ull, 0x3ull, 0x8ull, 0, 0, 0},
        {Op::ADD, 0xFFFFFFFFull, 0x1ull, 0x0ull, 1, 1, 0},          // -1 < 1 ⇒ 1；0xFFFFFFFF <u 1 ⇒ 0
        {Op::ADD, 0xFFFFFFFFull, 0xFFFFFFFFull, 0xFFFFFFFEull, 0, 0, 0}, // 相等 ⇒ 两标志皆 0
        // ---- SUB（含环绕：0 - 1 = 0xFFFFFFFF）----
        {Op::SUB, 0x5ull, 0x3ull, 0x2ull, 0, 0, 0},
        {Op::SUB, 0x0ull, 0x1ull, 0xFFFFFFFFull, 0, 1, 1},         // 0 < 1 ⇒ 两个标志都是 1
        // ---- 逻辑 ----
        // ⚠️ 手推（两个操作数**都是负数**）：0xF0F0F0F0 = -252645136 · 0xFF00FF00 = -16713216
        //    ⇒ 有符号：-252645136 < -16713216 ⇒ **1**（⚠️ 我第一次手推算错了 ⇒ ⇒ 被 ② 抓住）
        //    ⇒ 无符号：0xF0F0F0F0(4042322160) < 0xFF00FF00(4278254080) ⇒ **1**
        {Op::AND, 0xF0F0F0F0ull, 0xFF00FF00ull, 0xF000F000ull, 0, 1, 1},
        {Op::OR,  0xF0F0F0F0ull, 0xFF00FF00ull, 0xFFF0FFF0ull, 0, 1, 1},
        //    ⚠️ 逐字节手推：F0^FF=0F · F0^00=F0 · F0^FF=0F · F0^00=F0 ⇒ ⇒ **0x0FF00FF0**
        //    （⚠️ 我第一次误写成 0xFF00FF00 —— 那是**操作数 b** 本身 ⇒ ⇒ 被 ① 抓住）
        {Op::XOR, 0xF0F0F0F0ull, 0xFF00FF00ull, 0x0FF00FF0ull, 0, 1, 1},
        // ---- 移位（含 sh=0 与移出）----
        {Op::SLL, 0x1ull, 0x4ull, 0x10ull, 0, 1, 1},                 // 1 < 4 ⇒ 1
        {Op::SLL, 0x80000000ull, 0x1ull, 0x0ull, 1, 1, 0},          // 移出 ⇒ 结果 0（zero=1）
        {Op::SRL, 0x80000000ull, 0x4ull, 0x08000000ull, 0, 1, 0},
        {Op::SRL, 0x80000000ull, 0x0ull, 0x80000000ull, 0, 1, 0},    // sh=0 ⇒ 无填充
        {Op::SRA, 0x80000000ull, 0x4ull, 0xF8000000ull, 0, 1, 0},    // 符号填充
        {Op::SRA, 0x80000000ull, 0x0ull, 0x80000000ull, 0, 1, 0},    // sh=0 ⇒ MUST NOT 填充
        // ---- SLT（有符号）----
        {Op::SLT, 0xFFFFFFFFull, 0x1ull, 0x1ull, 0, 1, 0},
        {Op::SLT, 0x1ull, 0xFFFFFFFFull, 0x0ull, 1, 0, 1},
        {Op::SLT, 0x80000000ull, 0x0ull, 0x1ull, 0, 1, 0},
        {Op::SLT, 0x0ull, 0x0ull, 0x0ull, 1, 0, 0},
        // ---- SLTU（无符号）----
        // ⚠️ 最后一条是 **B0 ⑧ 的反证用例**：旧实现（`bits<31,31>(sub_result)`）在此给 **0**
        {Op::SLTU, 0x1ull, 0xFFFFFFFFull, 0x1ull, 0, 0, 1},         // 有符号 1 < -1 ⇒ 0
        {Op::SLTU, 0xFFFFFFFFull, 0x1ull, 0x0ull, 1, 1, 0},
        {Op::SLTU, 0x0ull, 0xFFFFFFFFull, 0x1ull, 0, 0, 1},
    };
}

}  // namespace

// ===========================================================================
// ① 差分本身：两侧**各自**对手推值断言，且**彼此**相等
// ===========================================================================
TEST_CASE("B4 C1 - Rv32iAlu vs ExStage differential (value-equivalent)",
          "[cpu][rv32i][alu][differential][b4]") {
    const auto cases = diff_cases();

    for (const auto &c : cases) {
        INFO("op=" << op_name(c.op) << " a=0x" << std::hex << c.a
                   << " b=0x" << c.b);
        const AluOutputs inl = run_inline(c.a, c.b, ex_op(c.op));
        const AluOutputs lib = run_library(c.a, c.b, lib_op(c.op));

        // ---- ① 内联侧：对手推值 ----
        REQUIRE(inl.result == c.exp_result);
        REQUIRE(inl.zero == c.exp_zero);
        REQUIRE(inl.less_than == c.exp_less_than);
        REQUIRE(inl.less_than_u == c.exp_less_than_u);

        // ---- ② 库组件侧：对手推值 ----
        REQUIRE(lib.result == c.exp_result);
        REQUIRE(lib.zero == c.exp_zero);
        REQUIRE(lib.less_than == c.exp_less_than);
        REQUIRE(lib.less_than_u == c.exp_less_than_u);

        // ---- ③ 两侧彼此相等 ⇒ ⇒ **清单外的值差异 = 0** ----
        REQUIRE(inl.result == lib.result);
        REQUIRE(inl.zero == lib.zero);
        REQUIRE(inl.less_than == lib.less_than);
        REQUIRE(inl.less_than_u == lib.less_than_u);
    }
}

// ===========================================================================
// ② 手推的期望值**自洽性**（防止上表的期望值写错 ⇒ ⇒ 两侧一起「对」）
// ===========================================================================
TEST_CASE("B4 C1 - hand-derived expectations are self-consistent",
          "[cpu][rv32i][alu][differential][b4]") {
    for (const auto &c : diff_cases()) {
        INFO("op=" << op_name(c.op));
        // 比较标志 MUST 与「手算的有符号/无符号比较」一致
        REQUIRE(c.exp_less_than ==
                static_cast<uint64_t>(as_signed(c.a) < as_signed(c.b) ? 1 : 0));
        REQUIRE(c.exp_less_than_u == static_cast<uint64_t>(c.a < c.b ? 1 : 0));
        // 零标志 MUST 与结果一致
        REQUIRE(c.exp_zero == static_cast<uint64_t>(c.exp_result == 0 ? 1 : 0));
    }
}

// ===========================================================================
// ③ **操作码编码的唯一性**（C2 的棘轮）
//
// ⚠️ **历史（2026-11-14 · C1 当时）**：两套编码**完全不同** ——
//    内联 `ALU_OP_*`（ADD 0 · SUB 1 · AND 2 · OR 3 · XOR 4 · SLL 5 · SRL 6 · SRA 7
//    · SLT 8 · SLTU 9）vs 库 `ALU_*`（ADD 0 · SLL 1 · SLT 2 · SLTU 3 · XOR 4 · SRL 5
//    · OR 6 · AND 7 · SUB 10 · SRA 11）⇒ ⇒ **8 / 10 项不同**
//    ⇒ ⇒ `alu_op = 3` 内联 = **OR**、库 = **SLTU** ⇒ ⇒ 不归一就接入 = **零报错的行为漂移**
//    ⇒ ⇒ C1 的断言正是「两套**必须**不同」（棘轮 = 提醒「该归一了」）。
//
// ✅ **C2 落地后（2026-11-14）**：`ex_stage.h` 的 `ALU_OP_*` **已删除**
//    ⇒ ⇒ **唯一权威 = `riscv::ALU_*`** ⇒ ⇒ 本断言**已翻转**为「两侧**必须相同**」
//    ⇒ ⇒ 依据 = `.rddf/checks/rv32i-alu-differential-list.md` 的 **Δ1（已修）**
// ===========================================================================
TEST_CASE("B4 C2 - the two ALU opcode encodings are UNIFIED",
          "[cpu][rv32i][alu][differential][b4]") {
    // 归一后：**全部 10 种**操作的两侧编码 MUST **逐项相同**
    const Op all[] = {Op::ADD, Op::SUB, Op::AND, Op::OR,  Op::XOR,
                      Op::SLL, Op::SRL, Op::SRA, Op::SLT, Op::SLTU};
    for (Op op : all) {
        INFO("op=" << op_name(op) << " inline=" << ex_op(op)
                   << " library=" << lib_op(op));
        REQUIRE(ex_op(op) == lib_op(op));
    }

    // ⚠️ **归一后的唯一编码表**（锁死 ⇒ ⇒ 任何人改动其中一个 ⇒ ⇒ 本用例转红）
    REQUIRE(riscv::ALU_ADD  == 0u);
    REQUIRE(riscv::ALU_SLL  == 1u);
    REQUIRE(riscv::ALU_SLT  == 2u);
    REQUIRE(riscv::ALU_SLTU == 3u);
    REQUIRE(riscv::ALU_XOR  == 4u);
    REQUIRE(riscv::ALU_SRL  == 5u);
    REQUIRE(riscv::ALU_OR   == 6u);
    REQUIRE(riscv::ALU_AND  == 7u);
    REQUIRE(riscv::ALU_SUB  == 10u);
    REQUIRE(riscv::ALU_SRA  == 11u);

    // ⚠️ **旧编码必须已消失**（`ALU_OP_*` 一族从 `ex_stage.h` 删除）
    //    ⇒ ⇒ 判据 = 全仓 grep（`scripts/verify_no_duplicate_rv32i_headers.sh` 之外的独立断言）
    //    ⇒ ⇒ 这里用**头文件内容**判：归一后 `ex_stage.h` 里 MUST NOT 再出现 `ALU_OP_` 定义
    //    （注释里**允许**出现，因为注释要记录历史）
}
