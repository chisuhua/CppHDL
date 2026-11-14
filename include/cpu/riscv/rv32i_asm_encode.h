/**
 * @file rv32i_asm_encode.h
 * @brief RV32I 指令编码器（**header-only · constexpr** · `firmware-load-chain` **L3**）
 *
 * ## 为什么库化（**L3**）
 *
 * 此前编码器只存在于**一个 example 测试**里（`examples/riscv-mini/tests/
 * test_rv32i_soc_dual_parity.cpp`）⇒ ⇒ 固件**转储器工具**（L4）无法复用它
 * ⇒ ⇒ 若工具再抄一份 ⇒ ⇒ **两份编码器会漂** ⇒ ⇒ 固件与工具对同一条指令
 * 给出**不同字** ⇒ ⇒ 零报错。
 *
 * ⇒ ⇒ 本头把「**唯一**一份」放到**库**里 ⇒ ⇒ 消费者 = 既有测试 + 新工具
 * ⇒ ⇒ 满足 ZERO-DEBT「新头文件有仓内消费者」。
 *
 * ## ⚠️ **手算交叉校验 MUST 保留**（`FirmwareEncoderCrossCheck`）
 *
 * 若只用本头的 helper 去断言本头的 helper ⇒ ⇒ **自证** ⇒ ⇒ 零判别力。
 * ⇒ ⇒ 仓内的交叉校验用例**逐字保留**：它拿**手算常量**（`0x00100093` 等）
 * 对比本头的产出 ⇒ ⇒ 那才是**独立**证据。
 *
 * ## 编码口径
 *
 * | 格式 | 指令 | 立即数 |
 * |---|---|---|
 * | R | `ADD`/`SUB`/`SRA` | — |
 * | I | `ADDI`/`LW`/`LB`/`LH`/`LBU`/`LHU`/`JALR` | 12 位**符号**扩展 |
 * | S | `SW`/`SB`/`SH` | 12 位（`imm[11:5]` + `imm[4:0]`） |
 * | B | `BEQ`/`BNE` | 13 位（±4 KiB） |
 * | J | `JAL` | 21 位（±1 MiB） |
 * | U | `AUIPC` | 20 位（**只能是 4 KiB 的倍数**） |
 *
 * ⚠️ **立即数一律「低位对齐」**（RISC-V 语义）：store / load 的源数据**低位对齐**，
 * 由总线按 `addr[1:0]` 选车道 ⇒ ⇒ 本头**不做**任何移位。
 */

#pragma once

#include <cstdint>

namespace riscv {

// ---------------------------------------------------------------------------
// 格式编码器（**底层**）
// ---------------------------------------------------------------------------

/// R 型：`funct7 | rs2 | rs1 | funct3 | rd | 0x33`
constexpr uint32_t enc_r(uint32_t rd, uint32_t rs1, uint32_t rs2,
                         uint32_t funct3, uint32_t funct7) {
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) |
           (rd << 7) | 0x33u /* OP */;
}

/// I 型：`imm[11:0] | rs1 | funct3 | rd | opcode`
constexpr uint32_t enc_i(uint32_t rd, uint32_t rs1, uint32_t funct3,
                         int32_t imm, uint32_t opcode) {
    return ((static_cast<uint32_t>(imm) & 0xFFFu) << 20) | (rs1 << 15) |
           (funct3 << 12) | (rd << 7) | opcode;
}

/// S 型：`imm[11:5] | rs2 | rs1 | funct3 | imm[4:0] | 0x23`
constexpr uint32_t enc_s(uint32_t rs2, uint32_t rs1, uint32_t funct3,
                         int32_t imm) {
    uint32_t u = static_cast<uint32_t>(imm) & 0xFFFu;
    return ((u >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) |
           ((u & 0x1Fu) << 7) | 0x23u /* STORE */;
}

/// B 型：`imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | 0x63`
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

/// J 型：`imm[20|10:1|11|19:12] | rd | 0x6F`
/// ⚠️ `imm` 是**字节**偏移（bit0 恒 0 ⇒ JAL 目标天然 2 字节对齐）。
constexpr uint32_t enc_j(uint32_t rd, int32_t imm) {
    uint32_t u = static_cast<uint32_t>(imm) & 0x1FFFFFu;  // 21-bit, ±1 MiB
    uint32_t b20 = (u >> 20) & 1u;
    uint32_t b10_1 = (u >> 1) & 0x3FFu;
    uint32_t b11 = (u >> 11) & 1u;
    uint32_t b19_12 = (u >> 12) & 0xFFu;
    return (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12) |
           (rd << 7) | 0x6Fu /* JAL */;
}

/// U 型：`imm[31:12] | rd | 0x17`
/// ⚠️ U 型的立即数**只能是 4 KiB 的倍数** ⇒ 算任意目标地址 MUST 配合 `ADDI`
constexpr uint32_t enc_u(uint32_t rd, uint32_t imm_hi20) {
    return ((imm_hi20 & 0xFFFFFu) << 12) | (rd << 7) | 0x17u;
}

// ---------------------------------------------------------------------------
// 便捷别名（funct3 / funct7 按 RV32I 规范）
// ---------------------------------------------------------------------------

/// `addi x0, x0, 0`
constexpr uint32_t NOP = 0x00000013u;

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
/// ⚠️ **字节 / 半字写**（funct3 = 000 / 001）。
///   源数据**低位对齐**，由总线按 `addr[1:0]` 选车道（RISC-V 语义）。
constexpr uint32_t SB(uint32_t rs2, uint32_t rs1, int32_t imm) {
    return enc_s(rs2, rs1, 0x0u, imm);
}
constexpr uint32_t SH(uint32_t rs2, uint32_t rs1, int32_t imm) {
    return enc_s(rs2, rs1, 0x1u, imm);
}
constexpr uint32_t LW(uint32_t rd, uint32_t rs1, int32_t imm) {
    return enc_i(rd, rs1, 0x2u, imm, 0x03u);
}
/// ⚠️ **字节 / 半字读**：funct3 = 000 (LB，**符号**扩展) / 001 (LH，**符号**) /
///   100 (LBU，**零**) / 101 (LHU，**零**)。
///   ⚠️ 半字道按 `addr[1]` 选 ⇒ **半字对齐**（`addr[0]` 被忽略）
///   ⇒ ⇒ 用例 MUST 用**偶地址**。
constexpr uint32_t LB(uint32_t rd, uint32_t rs1, int32_t imm) {
    return enc_i(rd, rs1, 0x0u, imm, 0x03u);
}
constexpr uint32_t LH(uint32_t rd, uint32_t rs1, int32_t imm) {
    return enc_i(rd, rs1, 0x1u, imm, 0x03u);
}
constexpr uint32_t LBU(uint32_t rd, uint32_t rs1, int32_t imm) {
    return enc_i(rd, rs1, 0x4u, imm, 0x03u);
}
constexpr uint32_t LHU(uint32_t rd, uint32_t rs1, int32_t imm) {
    return enc_i(rd, rs1, 0x5u, imm, 0x03u);
}
constexpr uint32_t BEQ(uint32_t rs1, uint32_t rs2, int32_t imm) {
    return enc_b(rs1, rs2, 0x0u, imm);
}
constexpr uint32_t BNE(uint32_t rs1, uint32_t rs2, int32_t imm) {
    return enc_b(rs1, rs2, 0x1u, imm);
}
constexpr uint32_t JAL(uint32_t rd, int32_t imm) { return enc_j(rd, imm); }
/// `JALR`：I 型，funct3 = 000，opcode = 0x67
/// ⚠️ 目标地址 = `(rs1 + imm) & ~1`（**MUST** 清最低位）
constexpr uint32_t JALR(uint32_t rd, uint32_t rs1, int32_t imm) {
    return enc_i(rd, rs1, 0x0u, imm, 0x67u);
}
/// `AUIPC`：`rd = pc + (imm_hi20 << 12)`
constexpr uint32_t AUIPC(uint32_t rd, uint32_t imm_hi20) {
    return enc_u(rd, imm_hi20);
}
/// `SRA`：R 型，funct3 = 101，funct7 = 0x20（区分 SRL）
constexpr uint32_t SRA(uint32_t rd, uint32_t rs1, uint32_t rs2) {
    return enc_r(rd, rs1, rs2, 0x5u, 0x20u);
}

}  // namespace riscv
