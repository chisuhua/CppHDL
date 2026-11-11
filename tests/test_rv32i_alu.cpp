// tests/test_rv32i_alu.cpp
//
// `riscv::Rv32iAlu` 的 ALU 语义判据。
//
// ⚠️ **判别力**（本文件 MUST 抓住的具体缺陷）：
//   修复前 `rv32i_alu.h` 的 SLTU 实现是
//     `auto lt_unsigned_sig = bits<31, 31>(sub_result);`   // 「简化实现」
//   ⇒ ⇒ `sub_result` 是 **32 位**减法结果 ⇒ ⇒ 其 bit31 是**结果位**而非**借位位**
//   ⇒ ⇒ 手推反证：`0 - 0xFFFFFFFF` = `0x00000001` ⇒ bit31 = **0** ⇒ 旧实现得 **0**
//   ⇒ ⇒ 而 RISC-V SLTU 语义要求 `0 <u 0xFFFFFFFF` = **1** ⇒ ⇒ **缺陷成立**
//   ⇒ ⇒ 本文件 MUST 转红。
//
// ⚠️ **期望值 MUST 手推**（禁抄仿真输出）—— 本仓的纪律：
//   「两后端一致」**不能**当作正确性证据 ⇒ ⇒ 同一份 C++ 生成两后端 ⇒ ⇒ 同构缺陷两后端
//   一致地错 ⇒ ⇒ parity 对这一族缺陷**结构性失明** ⇒ ⇒ 期望值必须**手推**。
//
// ⚠️ **与 `ex_stage.h` 的同口径**：仓内权威实现
//   （`include/cpu/pipeline/stages/ex_stage.h:189`）用
//     `auto sltu_result_sig = (io().rs1_data < alu_second_operand);`
//   ⇒ ⇒ 与本文件期望的 SLTU 语义**一致** ⇒ ⇒ 两处不是「各自猜」，是**同一口径**。
//   ⚠️ **但两者的 ALU 操作码编码不同**（`ex_stage.h` 的 `ALU_OP_SLTU = 9`
//   vs `rv32i_alu.h` 的 `ALU_SLTU = 3`）⇒ ⇒ 本文件 MUST 用 `rv32i_alu.h` 自己的常量
//   ⇒ ⇒ **禁**用字面量 `3`（那是 RISC-V **funct3** 编码，不是 ALU op 编码 ——
//   两者巧合相等，但**语义不同**，用字面量 = 埋雷）。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "simulator.h"

#include "cpu/pipeline/rv32i_alu.h"

#include <cstdint>

namespace {

// ── DUT 驱动：把 ALU 组合逻辑跑一拍并取结果 ────────────────────────────
// ⚠️ `ch_module` MUST NOT 在 standalone 测试里用（`Component::current()` 为 nullptr
//   ⇒ ⇒ SIGFGV）⇒ ⇒ 这里 MUST 用 `ch_device`（根 `AGENTS.md` §COMPONENT LIFECYCLE）。
struct AluResult {
    uint64_t result;
    uint64_t zero;
    uint64_t less_than;
    uint64_t less_than_u;
};

AluResult execute_alu(uint64_t a, uint64_t b, uint64_t alu_op, uint64_t funct3 = 0) {
    ch::ch_device<riscv::Rv32iAlu> alu;
    ch::Simulator sim(alu.context());
    sim.set_input_value(alu.instance().io().operand_a, a);
    sim.set_input_value(alu.instance().io().operand_b, b);
    sim.set_input_value(alu.instance().io().alu_op, alu_op);
    sim.set_input_value(alu.instance().io().funct3, funct3);
    sim.tick();
    AluResult r{};
    r.result = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().result));
    r.zero = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().zero));
    r.less_than = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().less_than));
    r.less_than_u = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().less_than_u));
    return r;
}

// ═══════════════════════════════════════════════════════════════════════
// 判据 1（**核心判别力**）：SLTU MUST 是**无符号**比较
// ═══════════════════════════════════════════════════════════════════════
TEST_CASE("Rv32iAlu: SLTU MUST be an UNSIGNED comparison", "[cpu][rv32i][alu]") {
    // 手推（期望值）：
    //   · 0 <u 0xFFFFFFFF ⇒ 1（0 是无符号最小值）
    //   · 旧实现 `bits<31,31>(0 - 0xFFFFFFFF)` = `bits<31,31>(0x00000001)` = **0** ⇒ 缺陷
    {
        const AluResult r = execute_alu(0, 0xFFFFFFFF, riscv::ALU_SLTU);
        CAPTURE(r.result);
        REQUIRE(r.result == 1);
    }
    // 手推：1 <u 0xFFFFFFFF ⇒ 1（1 - 0xFFFFFFFF = 0xFFFFFF02 ⇒ bit31 = 1 ⇒ 旧实现**碰巧**对
    //   ⇒ ⇒ 这一条**不能**判别缺陷 ⇒ ⇒ 判别力在上面那条）
    {
        const AluResult r = execute_alu(1, 0xFFFFFFFF, riscv::ALU_SLTU);
        REQUIRE(r.result == 1);
    }
    // 手推：0xFFFFFFFF <u 0 ⇒ 0（0xFFFFFFFF 是无符号最大值）
    //   ⇒ 旧实现：0xFFFFFFFF - 0 = 0xFFFFFFFF ⇒ bit31 = 1 ⇒ 旧实现得 **1** ⇒ 也是错的
    {
        const AluResult r = execute_alu(0xFFFFFFFF, 0, riscv::ALU_SLTU);
        CAPTURE(r.result);
        REQUIRE(r.result == 0);
    }
    // 手推：0x80000000 <u 0x7FFFFFFF ⇒ 0（无符号下 0x80000000 是**大**的）
    //   ⇒ 旧实现：0x80000000 - 0x7FFFFFFF = 0x00000001 ⇒ bit31 = 0 ⇒ 碰巧对
    {
        const AluResult r = execute_alu(0x80000000, 0x7FFFFFFF, riscv::ALU_SLTU);
        REQUIRE(r.result == 0);
    }
    // 手推：0x7FFFFFFF <u 0x80000000 ⇒ 1（无符号下 0x7FFFFFFF 是**小**的）
    //   ⇒ 旧实现：0x7FFFFFFF - 0x80000000 = 0xFFFFFFFF ⇒ bit31 = 1 ⇒ 碰巧对
    {
        const AluResult r = execute_alu(0x7FFFFFFF, 0x80000000, riscv::ALU_SLTU);
        REQUIRE(r.result == 1);
    }
    // 手推：相等 ⇒ 0（`0 - 0 = 0` ⇒ bit31 = 0）
    {
        const AluResult r = execute_alu(0x12345678, 0x12345678, riscv::ALU_SLTU);
        REQUIRE(r.result == 0);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// 判据 2：**反向**判据 —— SLT（有符号）MUST NOT 被这次修复带偏
// ═══════════════════════════════════════════════════════════════════════
// ⚠️ 只查「SLTU 对了」的话，**把 SLT 也改成无符号**同样会绿
//   ⇒ ⇒ 必须同时钉住 SLT 的**有符号**语义（两者**必须不同**）。
TEST_CASE("Rv32iAlu: SLT MUST stay SIGNED (must differ from SLTU)", "[cpu][rv32i][alu]") {
    // 手推（有符号）：0x80000000 (-2^31) <s 0 ⇒ **1**
    //   ⇒ SLTU 在同一组操作数上 MUST 得 **0**（0x80000000 是无符号大值）
    {
        const AluResult slt = execute_alu(0x80000000, 0, riscv::ALU_SLT);
        const AluResult sltu = execute_alu(0x80000000, 0, riscv::ALU_SLTU);
        CAPTURE(slt.result);
        CAPTURE(sltu.result);
        REQUIRE(slt.result == 1);
        REQUIRE(sltu.result == 0);
    }
    // 手推（有符号）：0 <s 0x80000000 (-2^31) ⇒ **0**
    //   ⇒ SLTU 在同一组操作数上 MUST 得 **1**
    {
        const AluResult slt = execute_alu(0, 0x80000000, riscv::ALU_SLT);
        const AluResult sltu = execute_alu(0, 0x80000000, riscv::ALU_SLTU);
        REQUIRE(slt.result == 0);
        REQUIRE(sltu.result == 1);
    }
    // 手推（有符号）：-1 (0xFFFFFFFF) <s 1 ⇒ **1**
    {
        const AluResult r = execute_alu(0xFFFFFFFF, 1, riscv::ALU_SLT);
        REQUIRE(r.result == 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// 判据 3：`less_than_u` 与 SLTU 结果 MUST 一致（同一语义的两个出口）
// ═══════════════════════════════════════════════════════════════════════
TEST_CASE("Rv32iAlu: less_than_u MUST agree with the SLTU result", "[cpu][rv32i][alu]") {
    for (uint64_t a : {uint64_t{0}, uint64_t{1}, uint64_t{0x7FFFFFFF}, uint64_t{0x80000000},
                       uint64_t{0xFFFFFFFF}}) {
        for (uint64_t b : {uint64_t{0}, uint64_t{1}, uint64_t{0x7FFFFFFF}, uint64_t{0x80000000},
                           uint64_t{0xFFFFFFFF}}) {
            const AluResult r = execute_alu(a, b, riscv::ALU_SLTU);
            CAPTURE(a);
            CAPTURE(b);
            REQUIRE(r.less_than_u == r.result);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// 判据 4：**其余** ALU 操作 MUST NOT 被这次修复带偏（回归护栏）
// ═══════════════════════════════════════════════════════════════════════
TEST_CASE("Rv32iAlu: non-SLTU operations MUST be unchanged", "[cpu][rv32i][alu]") {
    // 手推：ADD 10 + 5 = 15
    REQUIRE(execute_alu(10, 5, riscv::ALU_ADD).result == 15);
    // 手推：SUB 10 - 5 = 5
    REQUIRE(execute_alu(10, 5, riscv::ALU_SUB).result == 5);
    // 手推：SLL 1 << 4 = 16
    REQUIRE(execute_alu(1, 4, riscv::ALU_SLL).result == 16);
    // 手推：XOR 0xF0 ^ 0x0F = 0xFF
    REQUIRE(execute_alu(0xF0, 0x0F, riscv::ALU_XOR).result == 0xFF);
    // 手推：OR 0xF0 | 0x0F = 0xFF
    REQUIRE(execute_alu(0xF0, 0x0F, riscv::ALU_OR).result == 0xFF);
    // 手推：AND 0xF0 & 0x3C = 0x30
    REQUIRE(execute_alu(0xF0, 0x3C, riscv::ALU_AND).result == 0x30);
    // 手推：SRL 0x80000000 >> 4 = 0x08000000
    REQUIRE(execute_alu(0x80000000, 4, riscv::ALU_SRL).result == 0x08000000);
    // 手推：SRA 0x80000000 >>> 4 = 0xF8000000（算术右移 ⇒ 补符号位）
    REQUIRE(execute_alu(0x80000000, 4, riscv::ALU_SRA).result == 0xF8000000);
    // 手推：SRA 移位量 0 ⇒ MUST NOT 填充（`32 - 0 = 32` ⇒ 越界）
    REQUIRE(execute_alu(0x80000000, 0, riscv::ALU_SRA).result == 0x80000000);
}

}   // namespace
