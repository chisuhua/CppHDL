/**
 * RV32I Instruction Decoder
 * 
 * 将 32 位指令译码为控制信号 (简化版)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

class InstrDecoder : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> instr;
        
        // 控制信号
        ch_out<ch_bool> reg_write;
        ch_out<ch_bool> alu_src;
        ch_out<ch_bool> mem_read;
        ch_out<ch_bool> mem_write;
        ch_out<ch_bool> mem_to_reg;
        ch_out<ch_bool> branch;
        ch_out<ch_bool> jump;
        ch_out<ch_bool> jalr;
        
        // ALU 控制
        ch_out<ch_uint<3>> alu_op;
        ch_out<ch_bool> alu_sub;
        ch_out<ch_bool> alu_sra;
        
        // 寄存器地址
        ch_out<ch_uint<5>> rs1_addr;
        ch_out<ch_uint<5>> rs2_addr;
        ch_out<ch_uint<5>> rd_addr;
        
        // 立即数
        ch_out<ch_uint<32>> imm;
    )
    
    InstrDecoder(ch::Component* parent = nullptr, const std::string& name = "decoder")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 提取指令字段
        auto opcode = io().instr & ch_uint<32>(127_d);
        auto rd = (io().instr >> ch_uint<32>(7_d)) & ch_uint<32>(31_d);
        auto funct3 = (io().instr >> ch_uint<32>(12_d)) & ch_uint<32>(7_d);
        auto rs1 = (io().instr >> ch_uint<32>(15_d)) & ch_uint<32>(31_d);
        auto rs2 = (io().instr >> ch_uint<32>(20_d)) & ch_uint<32>(31_d);
        
        // 输出寄存器地址
        io().rs1_addr = rs1;
        io().rs2_addr = rs2;
        io().rd_addr = rd;
        
        // 立即数 (简化：I 格式)
        auto imm_i = (io().instr >> ch_uint<32>(20_d)) & ch_uint<32>(4095_d);
        io().imm = imm_i;
        
        // 控制信号生成 (使用十进制操作码)
        // LOAD=3, STORE=35, OP_IMM=19, OP=51, LUI=55, AUIPC=23, BRANCH=99, JALR=103, JAL=111
        auto is_r_type = select(opcode == ch_uint<32>(51_d), ch_bool(true), ch_bool(false));
        auto is_i_type = select(opcode == ch_uint<32>(19_d), ch_bool(true),
                                select(opcode == ch_uint<32>(3_d), ch_bool(true),
                                       select(opcode == ch_uint<32>(103_d), ch_bool(true), ch_bool(false))));
        auto is_u_type = select(opcode == ch_uint<32>(55_d), ch_bool(true),
                                select(opcode == ch_uint<32>(23_d), ch_bool(true), ch_bool(false)));
        auto is_j_type = select(opcode == ch_uint<32>(111_d), ch_bool(true), ch_bool(false));
        
        io().reg_write = select(is_r_type, ch_bool(true),
                                select(is_i_type, ch_bool(true),
                                       select(is_u_type, ch_bool(true),
                                              select(is_j_type, ch_bool(true), ch_bool(false)))));
        
        // ALU 源选择
        io().alu_src = select(is_i_type, ch_bool(true),
                              select(is_u_type, ch_bool(true), ch_bool(false)));
        
        // 内存控制
        io().mem_read = select(opcode == ch_uint<32>(3_d), ch_bool(true), ch_bool(false));
        io().mem_write = select(opcode == ch_uint<32>(35_d), ch_bool(true), ch_bool(false));
        io().mem_to_reg = select(io().mem_read, ch_bool(true), ch_bool(false));
        
        // 分支/跳转
        io().branch = select(opcode == ch_uint<32>(99_d), ch_bool(true), ch_bool(false));
        io().jump = is_j_type;
        io().jalr = select(opcode == ch_uint<32>(103_d), ch_bool(true), ch_bool(false));
        
        // ALU 操作
        io().alu_op = funct3;
        
        // 简化：暂不支持 SUB 和 SRA
        io().alu_sub = ch_bool(false);
        io().alu_sra = ch_bool(false);
    }
};

} // namespace riscv
