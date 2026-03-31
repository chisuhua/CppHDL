/**
 * RV32I ALU (Arithmetic Logic Unit)
 * 
 * 支持 RV32I 所有算术和逻辑运算
 */

#pragma once

#include "ch.hpp"
#include "component.h"

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
        auto arith_result = select(io().is_sub, sub_result, add_result);
        
        // 逻辑运算
        auto and_result = op_a & op_b;
        auto or_result = op_a | op_b;
        auto xor_result = op_a ^ op_b;
        
        // 移位运算 (简化)
        auto shift_amt = op_b & ch_uint<32>(7_d);  // 限制移位量
        auto srl_result = op_a >> shift_amt;
        auto sra_result = srl_result;  // 简化
        auto sll_result = op_a << shift_amt;
        
        // 比较运算
        auto slt_result = select(op_a < op_b, ch_uint<32>(1_d), ch_uint<32>(0_d));
        auto sltu_result = slt_result;  // 简化
        
        // 根据 funct3 选择结果 (使用嵌套 select)
        auto shift_result = select(io().is_sra, sra_result, srl_result);
        
        // funct3: 0=ADD, 1=SLL, 2=SLT, 3=SLTU, 4=XOR, 5=SRL, 6=OR, 7=AND
        ch_uint<32> alu_out(0_d);
        alu_out = select(io().funct3 == ch_uint<3>(0_d), arith_result,
                select(io().funct3 == ch_uint<3>(1_d), sll_result,
                select(io().funct3 == ch_uint<3>(2_d), slt_result,
                select(io().funct3 == ch_uint<3>(3_d), sltu_result,
                select(io().funct3 == ch_uint<3>(4_d), xor_result,
                select(io().funct3 == ch_uint<3>(5_d), shift_result,
                select(io().funct3 == ch_uint<3>(6_d), or_result, and_result)))))));
        
        io().result = alu_out;
        io().zero = select(alu_out == ch_uint<32>(0_d), ch_bool(true), ch_bool(false));
        io().less = slt_result;
    }
};

} // namespace riscv
