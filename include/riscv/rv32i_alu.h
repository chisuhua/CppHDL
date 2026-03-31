/**
 * RV32I ALU (Arithmetic Logic Unit)
 * 
 * 支持 RV32I 所有算术和逻辑运算
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "rv32i_isa.h"

using namespace ch::core;

namespace riscv {

class ALU : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> operand_a;
        ch_in<ch_uint<32>> operand_b;
        ch_in<ch_uint<3>> funct3;
        ch_in<ch_bool> is_sub;   // 减法标志
        ch_in<ch_bool> is_sra;   // 算术右移标志
        ch_out<ch_uint<32>> result;
        ch_out<ch_bool> zero;    // 结果为零
        ch_out<ch_bool> less;    // 小于标志
    )
    
    ALU(ch::Component* parent = nullptr, const std::string& name = "alu")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 加法/减法
        auto add_result = operand_a + operand_b;
        auto sub_result = operand_a - operand_b;
        auto arith_result = select(is_sub, sub_result, add_result);
        
        // 逻辑运算
        auto and_result = operand_a & operand_b;
        auto or_result = operand_a | operand_b;
        auto xor_result = operand_a ^ operand_b;
        
        // 移位运算
        auto srl_result = operand_a >> (operand_b & ch_uint<32>(31_d));
        auto sra_result = (operand_a >> ch_uint<32>(31_d)) | 
                          (operand_a >> (operand_b & ch_uint<32>(31_d)));
        auto sll_result = operand_a << (operand_b & ch_uint<32>(31_d));
        
        // 比较运算
        auto slt_result = select(operand_a < operand_b, ch_uint<32>(1_d), ch_uint<32>(0_d));
        auto sltu_result = select((operand_a & ch_uint<32>(0x7FFFFFFF_d)) < (operand_b & ch_uint<32>(0x7FFFFFFF_d)),
                                   ch_uint<32>(1_d), ch_uint<32>(0_d));
        
        // 根据 funct3 选择结果
        // funct3: 000=ADD/SUB, 001=SLL, 010=SLT, 011=SLTU, 100=XOR, 101=SRL/SRA, 110=OR, 111=AND
        auto shift_result = select(is_sra, sra_result, srl_result);
        
        ch_uint<32> alu_out(0_d);
        alu_out = select(funct3 == ch_uint<3>(funct3::ADD), arith_result, alu_out);
        alu_out = select(funct3 == ch_uint<3>(funct3::SLL), sll_result, alu_out);
        alu_out = select(funct3 == ch_uint<3>(funct3::SLT), slt_result, alu_out);
        alu_out = select(funct3 == ch_uint<3>(funct3::SLTU), sltu_result, alu_out);
        alu_out = select(funct3 == ch_uint<3>(funct3::XOR), xor_result, alu_out);
        alu_out = select(funct3 == ch_uint<3>(funct3::SRL), shift_result, alu_out);
        alu_out = select(funct3 == ch_uint<3>(funct3::OR), or_result, alu_out);
        alu_out = select(funct3 == ch_uint<3>(funct3::AND), and_result, alu_out);
        
        io().result = alu_out;
        io().zero = (alu_out == ch_uint<32>(0_d));
        io().less = slt_result;
    }
};

} // namespace riscv
