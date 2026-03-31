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
        ch_in<ch_bool> is_sub;
        ch_in<ch_bool> is_sra;
        ch_out<ch_uint<32>> result;
        ch_out<ch_bool> zero;
        ch_out<ch_bool> less;
    )
    
    ALU(ch::Component* parent = nullptr, const std::string& name = "alu")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        auto op_a = io().operand_a;
        auto op_b = io().operand_b;
        
        // 加法/减法
        auto add_result = op_a + op_b;
        auto sub_result = op_a - op_b;
        auto arith_result = select(is_sub, sub_result, add_result);
        
        // 逻辑运算
        auto and_result = op_a & op_b;
        auto or_result = op_a | op_b;
        auto xor_result = op_a ^ op_b;
        
        // 移位运算
        auto shift_amt = op_b & ch_uint<32>(31_d);
        auto srl_result = op_a >> shift_amt;
        auto sra_result = (op_a >> ch_uint<32>(31_d)) | (op_a >> shift_amt);
        auto sll_result = op_a << shift_amt;
        
        // 比较运算
        auto slt_result = select(op_a < op_b, ch_uint<32>(1_d), ch_uint<32>(0_d));
        auto sltu_result = select((op_a & ch_uint<32>(0x7FFFFFFF_d)) < (op_b & ch_uint<32>(0x7FFFFFFF_d)),
                                   ch_uint<32>(1_d), ch_uint<32>(0_d));
        
        // 根据 funct3 选择结果
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
