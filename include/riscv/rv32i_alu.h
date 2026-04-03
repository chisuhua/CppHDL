/**
 * @file rv32i_alu.h
 * @brief RV32I 算术逻辑单元 (ALU)
 * 
 * 支持 10 种 RISC-V 操作:
 * - ADD, SUB (加减法)
 * - SLL (逻辑左移)
 * - SLT, SLTU (有/无符号小于比较)
 * - XOR (异或)
 * - SRL, SRA (逻辑/算术右移)
 * - OR (或)
 * - AND (与)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

// ALU 操作码定义 (与 funct3 对应)
constexpr unsigned ALU_ADD  = 0;
constexpr unsigned ALU_SLL  = 1;
constexpr unsigned ALU_SLT  = 2;
constexpr unsigned ALU_SLTU = 3;
constexpr unsigned ALU_XOR  = 4;
constexpr unsigned ALU_SRL  = 5;
constexpr unsigned ALU_OR   = 6;
constexpr unsigned ALU_AND  = 7;
constexpr unsigned ALU_SUB  = 10;  // funct7[5] = 1
constexpr unsigned ALU_SRA  = 11;  // SRL + funct7[5] = 1

class Rv32iAlu : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> operand_a;
        ch_in<ch_uint<32>> operand_b;
        ch_in<ch_uint<4>>  alu_op;     // ALU 操作码
        ch_in<ch_uint<3>>  funct3;     // 指令 funct3 字段 (用于内存操作)
        ch_out<ch_uint<32>> result;
        ch_out<ch_bool>     zero;      // 零标志
        ch_out<ch_bool>     less_than; // 小于标志 (有符号，用于 SLT/BLT)
        ch_out<ch_bool>     equal;     // 等于标志 (用于 BEQ)
        ch_out<ch_bool>     less_than_u; // 小于标志 (无符号，用于 SLTU/BLTU)
        // 内存操作辅助输出 (新增)
        ch_out<ch_uint<2>>  mem_size;  // 内存访问大小
                                       // 0 = byte (8 位)
                                       // 1 = half (16 位)
                                       // 2 = word (32 位)
        ch_out<ch_bool>     mem_signed; // 是否符号扩展
                                       // true = 符号扩展 (LB/LH)
                                       // false = 零扩展 (LBU/LHU)
    )
    
    Rv32iAlu(ch::Component* parent = nullptr, const std::string& name = "rv32i_alu")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // ALU 计算 (组合逻辑)
        // ========================================================================
        
        ch_uint<32> res(0_d);
        ch_bool lt(false);
        
        // ADD: res = a + b
        auto add_result = io().operand_a + io().operand_b;
        
        // SUB: res = a - b
        auto sub_result = io().operand_a - io().operand_b;
        
        // SLL: res = a << b[4:0]
        auto shift_amt = bits<4, 0>(io().operand_b);
        auto sll_result = io().operand_a << shift_amt;
        
        // SLT: res = (a < b) ? 1 : 0 (有符号)
        // 使用最高位判断符号
        auto a_sign = bits<31, 31>(io().operand_a);
        auto b_sign = bits<31, 31>(io().operand_b);
        auto diff_sign = a_sign != b_sign;
        auto a_neg = (a_sign == ch_bool(true));
        auto lt_signed_sig = select(diff_sign, a_neg, bits<31, 31>(sub_result));
        
        // SLTU: res = (a < b) ? 1 : 0 (无符号)
        // 如果 a - b 借位，则 a < b
        auto lt_unsigned_sig = bits<31, 31>(sub_result);  // 简化实现
        
        // XOR: res = a ^ b
        auto xor_result = io().operand_a ^ io().operand_b;
        
        // SRL: res = a >> b[4:0] (逻辑右移)
        auto srl_result = io().operand_a >> shift_amt;
        
        // SRA: res = a >> b[4:0] (算术右移，符号扩展)
        // 简化实现：对于正数同 SRL，负数需要特殊处理
        auto sra_result = srl_result;  // TODO: 完善算术右移
        
        // OR: res = a | b
        auto or_result = io().operand_a | io().operand_b;
        
        // AND: res = a & b
        auto and_result = io().operand_a & io().operand_b;
        
        // 根据 alu_op 选择结果
        // alu_op 编码：
        //   0 = ADD, 1 = SLL, 2 = SLT, 3 = SLTU, 4 = XOR, 5 = SRL, 6 = OR, 7 = AND
        //   10 = SUB (ADD + funct7[5]), 11 = SRA (SRL + funct7[5])
        
        res = select(io().alu_op == ch_uint<4>(ALU_ADD),  add_result, res);
        res = select(io().alu_op == ch_uint<4>(ALU_SLL),  sll_result, res);
        res = select(io().alu_op == ch_uint<4>(ALU_SLT),  zext<32>(lt_signed_sig), res);
        res = select(io().alu_op == ch_uint<4>(ALU_SLTU), zext<32>(lt_unsigned_sig), res);
        res = select(io().alu_op == ch_uint<4>(ALU_XOR),  xor_result, res);
        res = select(io().alu_op == ch_uint<4>(ALU_SRL),  srl_result, res);
        res = select(io().alu_op == ch_uint<4>(ALU_OR),   or_result, res);
        res = select(io().alu_op == ch_uint<4>(ALU_AND),  and_result, res);
        res = select(io().alu_op == ch_uint<4>(ALU_SUB),  sub_result, res);
        res = select(io().alu_op == ch_uint<4>(ALU_SRA),  sra_result, res);
        
        // 零标志：result == 0
        io().zero = (res == ch_uint<32>(0_d));
        
        // 小于标志 (有符号，用于后续分支判断)
        io().less_than = lt_signed_sig;
        
        // 等于标志：rs1 == rs2 (用于 BEQ 分支)
        io().equal = (io().operand_a == io().operand_b);
        
        // 小于标志 (无符号，用于后续分支判断)
        io().less_than_u = (io().operand_a < io().operand_b);
        
        io().result = res;
        
        // ========================================================================
        // 内存操作辅助输出 (新增)
        // ========================================================================
        
        // 根据 funct3 确定内存访问大小
        // funct3 encoding for load/store:
        //   000 = LB/LBU (byte, 8 位)
        //   001 = LH/LHU (half, 16 位)
        //   010 = LW (word, 32 位)
        //   100 = LBU (zero-extended byte)
        //   101 = LHU (zero-extended half)
        auto is_byte = (io().funct3 == ch_uint<3>(0_d)) || 
                       (io().funct3 == ch_uint<3>(4_d));
        auto is_half = (io().funct3 == ch_uint<3>(1_d)) || 
                       (io().funct3 == ch_uint<3>(5_d));
        auto is_word = (io().funct3 == ch_uint<3>(2_d));
        
        io().mem_size = select(is_byte, ch_uint<2>(0_d),
                          select(is_half, ch_uint<2>(1_d),
                          select(is_word, ch_uint<2>(2_d),
                                 ch_uint<2>(0_d))));
        
        // 根据 funct3 确定是否符号扩展
        // 符号扩展：LB (000), LH (001)
        // 零扩展：LBU (100), LHU (101)
        auto is_signed = (io().funct3 == ch_uint<3>(0_d)) || 
                         (io().funct3 == ch_uint<3>(1_d));
        io().mem_signed = is_signed;
    }
};

} // namespace riscv
