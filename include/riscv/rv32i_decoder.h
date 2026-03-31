/**
 * RV32I Instruction Decoder
 * 
 * 将 32 位指令译码为控制信号
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "rv32i_isa.h"

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
        auto funct7 = (io().instr >> ch_uint<32>(25_d)) & ch_uint<32>(127_d);
        
        // 输出寄存器地址
        io().rs1_addr = rs1;
        io().rs2_addr = rs2;
        io().rd_addr = rd;
        
        // 立即数扩展 (根据不同指令类型)
        // I 格式：imm[11:0]
        auto imm_i = bits<11, 0>(io().instr);
        
        // S 格式：imm[11:5] + imm[4:0]
        auto imm_s_lo = bits<11, 7>(io().instr);
        auto imm_s_hi = bits<31, 25](io().instr);
        auto imm_s = (imm_s_hi << ch_uint<32>(5_d)) | imm_s_lo;
        
        // B 格式：imm[12|10:5|4:1|11]
        auto imm_b_11 = bits<7, 7](io().instr);
        auto imm_b_4_1 = bits<11, 8](io().instr);
        auto imm_b_10_5 = bits<30, 25](io().instr);
        auto imm_b_12 = bits<31, 31](io().instr);
        auto imm_b = (imm_b_12 << ch_uint<32>(12_d)) | (imm_b_11 << ch_uint<32>(11_d)) |
                     (imm_b_10_5 << ch_uint<32>(5_d)) | (imm_b_4_1 << ch_uint<32>(1_d));
        
        // U 格式：imm[31:12]
        auto imm_u = bits<31, 12](io().instr) << ch_uint<32>(12_d);
        
        // J 格式：imm[20|10:1|11|19:12]
        auto imm_j_19_12 = bits<19, 12](io().instr);
        auto imm_j_11 = bits<20, 20](io().instr);
        auto imm_j_10_1 = bits<30, 21](io().instr);
        auto imm_j_20 = bits<31, 31](io().instr);
        auto imm_j = (imm_j_20 << ch_uint<32>(20_d)) | (imm_j_19_12 << ch_uint<32>(12_d)) |
                     (imm_j_11 << ch_uint<32>(11_d)) | (imm_j_10_1 << ch_uint<32>(1_d));
        
        // 选择正确的立即数
        ch_uint<32> imm_val(0_d);
        imm_val = select(opcode == ch_uint<32>(opcode::LOAD), imm_i, imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::STORE), imm_s, imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::OP_IMM), imm_i, imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::BRANCH), imm_b, imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::LUI), imm_u, imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::AUIPC), imm_u, imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::JAL), imm_j, imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::JALR), imm_i, imm_val);
        
        io().imm = imm_val;
        
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
        io().mem_to_reg = io().mem_read;
        
        // 分支/跳转
        io().branch = select(opcode == ch_uint<32>(99_d), ch_bool(true), ch_bool(false));
        io().jump = is_j_type;
        io().jalr = select(opcode == ch_uint<32>(103_d), ch_bool(true), ch_bool(false));
        
        // ALU 操作
        io().alu_op = funct3;
        
        // 减法 (SUB vs ADD) - 简化处理
        io().alu_sub = ch_bool(false);
        
        // 算术右移 (SRA vs SRL) - 简化处理
        io().alu_sra = ch_bool(false);
    }
};

} // namespace riscv
