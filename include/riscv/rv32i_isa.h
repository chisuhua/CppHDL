/**
 * RV32I Instruction Set Definition
 * 
 * RISC-V 基础整数指令集 (RV32I)
 * 参考：https://riscv.org/specifications/
 */

#pragma once

#include "ch.hpp"

using namespace ch::core;

namespace riscv {

// ============================================================================
// RV32I 指令格式
// ============================================================================

/**
 * R 格式指令 (寄存器 - 寄存器运算)
 * [31:25] funct7, [24:20] rs2, [19:15] rs1, [14:12] funct3, [11:7] rd, [6:0] opcode
 */
struct Instr_R {
    ch_uint<7> opcode;
    ch_uint<5> rd;
    ch_uint<3> funct3;
    ch_uint<5> rs1;
    ch_uint<5> rs2;
    ch_uint<7> funct7;
};

/**
 * I 格式指令 (立即数运算/加载)
 * [31:20] imm[11:0], [19:15] rs1, [14:12] funct3, [11:7] rd, [6:0] opcode
 */
struct Instr_I {
    ch_uint<7> opcode;
    ch_uint<5> rd;
    ch_uint<3> funct3;
    ch_uint<5> rs1;
    ch_uint<12> imm;
};

/**
 * S 格式指令 (存储)
 * [31:25] imm[11:5], [24:20] rs2, [19:15] rs1, [14:12] funct3, [11:7] imm[4:0], [6:0] opcode
 */
struct Instr_S {
    ch_uint<7> opcode;
    ch_uint<5> imm_lo;
    ch_uint<3> funct3;
    ch_uint<5> rs1;
    ch_uint<5> rs2;
    ch_uint<7> imm_hi;
};

/**
 * B 格式指令 (分支)
 * [31] imm[12], [30:25] imm[10:5], [24:20] rs2, [19:15] rs1, [14:12] funct3, [11:8] imm[4:1], [7] imm[11], [6:0] opcode
 */
struct Instr_B {
    ch_uint<7> opcode;
    ch_uint<4> imm_lo;
    ch_uint<3> funct3;
    ch_uint<5> rs1;
    ch_uint<5> rs2;
    ch_uint<6> imm_mid;
    ch_uint<1> imm_hi;
    ch_uint<1> imm_12;
};

/**
 * U 格式指令 (上位立即数)
 * [31:12] imm[31:12], [11:7] rd, [6:0] opcode
 */
struct Instr_U {
    ch_uint<7> opcode;
    ch_uint<5> rd;
    ch_uint<20> imm;
};

/**
 * J 格式指令 (跳转)
 * [31] imm[20], [30:21] imm[10:1], [20] imm[11], [19:12] imm[19:12], [11:7] rd, [6:0] opcode
 */
struct Instr_J {
    ch_uint<7> opcode;
    ch_uint<5> rd;
    ch_uint<8> imm_mid;
    ch_uint<1> imm_11;
    ch_uint<10> imm_lo;
    ch_uint<1> imm_20;
};

// ============================================================================
// 操作码定义
// ============================================================================

namespace opcode {
    constexpr unsigned LOAD   = 0b0000011;  // 加载
    constexpr unsigned STORE  = 0b0100011;  // 存储
    constexpr unsigned OP_IMM = 0b0010011;  // 立即数运算
    constexpr unsigned OP     = 0b0110011;  // 寄存器运算
    constexpr unsigned LUI    = 0b0110111;  // 上位立即数
    constexpr unsigned AUIPC  = 0b0010111;  // PC 相对上位立即数
    constexpr unsigned BRANCH = 0b1100011;  // 分支
    constexpr unsigned JALR   = 0b1100111;  // 寄存器跳转
    constexpr unsigned JAL    = 0b1101111;  // 跳转
}

// ============================================================================
// funct3 定义
// ============================================================================

namespace funct3 {
    // 加载/存储
    constexpr unsigned LB  = 0b000;
    constexpr unsigned LH  = 0b001;
    constexpr unsigned LW  = 0b010;
    constexpr unsigned LBU = 0b100;
    constexpr unsigned LHU = 0b101;
    constexpr unsigned SB  = 0b000;
    constexpr unsigned SH  = 0b001;
    constexpr unsigned SW  = 0b010;
    
    // 立即数运算
    constexpr unsigned ADDI  = 0b000;
    constexpr unsigned SLTI  = 0b010;
    constexpr unsigned SLTIU = 0b011;
    constexpr unsigned XORI  = 0b100;
    constexpr unsigned ORI   = 0b110;
    constexpr unsigned ANDI  = 0b111;
    constexpr unsigned SLLI  = 0b001;
    constexpr unsigned SRLI = 0b101;
    constexpr unsigned SRAI = 0b101;
    
    // 寄存器运算
    constexpr unsigned ADD  = 0b000;
    constexpr unsigned SUB  = 0b000;
    constexpr unsigned SLL  = 0b001;
    constexpr unsigned SLT  = 0b010;
    constexpr unsigned SLTU = 0b011;
    constexpr unsigned XOR  = 0b100;
    constexpr unsigned SRL  = 0b101;
    constexpr unsigned SRA  = 0b101;
    constexpr unsigned OR   = 0b110;
    constexpr unsigned AND  = 0b111;
    
    // 分支
    constexpr unsigned BEQ  = 0b000;
    constexpr unsigned BNE  = 0b001;
    constexpr unsigned BLT  = 0b100;
    constexpr unsigned BGE  = 0b101;
    constexpr unsigned BLTU = 0b110;
    constexpr unsigned BGEU = 0b111;
}

// ============================================================================
// funct7 定义 (用于区分 ADD/SUB, SRL/SRA)
// ============================================================================

namespace funct7 {
    constexpr unsigned ADD  = 0b0000000;
    constexpr unsigned SUB  = 0b0100000;
    constexpr unsigned SLL  = 0b0000000;
    constexpr unsigned SRL  = 0b0000000;
    constexpr unsigned SRA  = 0b0100000;
}

// ============================================================================
// 寄存器定义
// ============================================================================

namespace reg {
    constexpr unsigned ZERO = 0;   // 硬连线零
    constexpr unsigned RA   = 1;   // 返回地址
    constexpr unsigned SP   = 2;   // 栈指针
    constexpr unsigned GP   = 3;   // 全局指针
    constexpr unsigned TP   = 4;   // 线程指针
    constexpr unsigned T0   = 5;   // 临时寄存器
    constexpr unsigned T1   = 6;
    constexpr unsigned T2   = 7;
    constexpr unsigned S0   = 8;   // 保存寄存器
    constexpr unsigned FP   = 8;   // 帧指针 (同 S0)
    constexpr unsigned S1   = 9;
    constexpr unsigned A0   = 10;  // 函数参数/返回值
    constexpr unsigned A1   = 11;
    constexpr unsigned A2   = 12;
    constexpr unsigned A3   = 13;
    constexpr unsigned A4   = 14;
    constexpr unsigned A5   = 15;
    constexpr unsigned A6   = 16;
    constexpr unsigned A7   = 17;
    constexpr unsigned S2   = 18;
    constexpr unsigned S3   = 19;
    constexpr unsigned S4   = 20;
    constexpr unsigned S5   = 21;
    constexpr unsigned S6   = 22;
    constexpr unsigned S7   = 23;
    constexpr unsigned S8   = 24;
    constexpr unsigned S9   = 25;
    constexpr unsigned S10  = 26;
    constexpr unsigned S11  = 27;
    constexpr unsigned T3   = 28;
    constexpr unsigned T4   = 29;
    constexpr unsigned T5   = 30;
    constexpr unsigned T6   = 31;
}

// ============================================================================
// 异常码定义
// ============================================================================

namespace exception {
    constexpr unsigned INSTR_ADDR_MISALIGN = 0;  // 指令地址未对齐
    constexpr unsigned INSTR_ACCESS_FAULT  = 1;  // 指令访问错误
    constexpr unsigned ILLEGAL_INSTR       = 2;  // 非法指令
    constexpr unsigned BREAKPOINT          = 3;  // 断点
    constexpr unsigned LOAD_ADDR_MISALIGN  = 4;  // 加载地址未对齐
    constexpr unsigned LOAD_ACCESS_FAULT   = 5;  // 加载访问错误
    constexpr unsigned STORE_ADDR_MISALIGN = 6;  // 存储地址未对齐
    constexpr unsigned STORE_ACCESS_FAULT  = 7;  // 存储访问错误
}

// ============================================================================
// CSR 寄存器定义 (简化)
// ============================================================================

namespace csr {
    constexpr unsigned MSTATUS  = 0x300;  // 机器状态
    constexpr unsigned MISA     = 0x301;  // ISA 和能力
    constexpr unsigned MEDELEG  = 0x302;  // 机器异常委托
    constexpr unsigned MIDELEG  = 0x303;  // 机器中断委托
    constexpr unsigned MIE      = 0x304;  // 机器中断使能
    constexpr unsigned MTVEC    = 0x305;  // 机器陷阱向量基址
    constexpr unsigned MEPC     = 0x341;  // 机器异常程序计数器
    constexpr unsigned MCAUSE   = 0x342;  // 机器陷阱原因
    constexpr unsigned MTVAL    = 0x343;  // 机器陷阱值
    constexpr unsigned MIP      = 0x344;  // 机器中断挂起
}

} // namespace riscv
