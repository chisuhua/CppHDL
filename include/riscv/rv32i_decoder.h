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
        ch_out<ch_bool> reg_write;      // 寄存器写使能
        ch_out<ch_bool> alu_src;        // ALU 源选择 (0=寄存器，1=立即数)
        ch_out<ch_bool> mem_read;       // 内存读
        ch_out<ch_bool> mem_write;      // 内存写
        ch_out<ch_bool> mem_to_reg;     // 内存数据到寄存器
        ch_out<ch_bool> branch;         // 分支指令
        ch_out<ch_bool> jump;           // 跳转指令
        ch_out<ch_bool> jalr;           // 寄存器跳转
        
        // ALU 控制
        ch_out<ch_uint<3>> alu_op;      // ALU 操作
        ch_out<ch_bool> alu_sub;        // ALU 减法
        ch_out<ch_bool> alu_sra;        // ALU 算术右移
        
        // 寄存器地址
        ch_out<ch_uint<5>> rs1_addr;
        ch_out<ch_uint<5>> rs2_addr;
        ch_out<ch_uint<5>> rd_addr;
        
        // 立即数
        ch_out<ch_int<32>> imm;
    )
    
    InstrDecoder(ch::Component* parent = nullptr, const std::string& name = "decoder")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 提取指令字段
        auto opcode = io().instr & ch_uint<32>(0x7F_d);
        auto rd = (io().instr >> ch_uint<32>(7_d)) & ch_uint<32>(0x1F_d);
        auto funct3 = (io().instr >> ch_uint<32>(12_d)) & ch_uint<32>(7_d);
        auto rs1 = (io().instr >> ch_uint<32>(15_d)) & ch_uint<32>(0x1F_d);
        auto rs2 = (io().instr >> ch_uint<32>(20_d)) & ch_uint<32>(0x1F_d);
        auto funct7 = (io().instr >> ch_uint<32>(25_d)) & ch_uint<32>(0x7F_d);
        
        // 输出寄存器地址
        io().rs1_addr = rs1;
        io().rs2_addr = rs2;
        io().rd_addr = rd;
        
        // 立即数扩展 (根据不同指令类型)
        // I 格式：imm[11:0]
        auto imm_i = bits<31, 20>(io().instr);
        
        // S 格式：imm[11:5] + imm[4:0]
        auto imm_s_lo = bits<11, 7>(io().instr);
        auto imm_s_hi = bits<31, 25>(io().instr);
        auto imm_s = (imm_s_hi << ch_uint<32>(5_d)) | imm_s_lo;
        
        // B 格式：imm[12|10:5|4:1|11]
        auto imm_b_11 = bits<7, 7>(io().instr);
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
        ch_int<32> imm_val(0_d);
        imm_val = select(opcode == ch_uint<32>(opcode::LOAD), ch_int<32>(imm_i), imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::STORE), ch_int<32>(imm_s), imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::OP_IMM), ch_int<32>(imm_i), imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::BRANCH), ch_int<32>(imm_b), imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::LUI), ch_int<32>(imm_u), imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::AUIPC), ch_int<32>(imm_u), imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::JAL), ch_int<32>(imm_j), imm_val);
        imm_val = select(opcode == ch_uint<32>(opcode::JALR), ch_int<32>(imm_i), imm_val);
        
        io().imm = imm_val;
        
        // 控制信号生成
        // 寄存器写使能：R 格式、I 格式、U 格式、J 格式
        auto is_r_type = (opcode == ch_uint<32>(opcode::OP));
        auto is_i_type = (opcode == ch_uint<32>(opcode::OP_IMM)) || 
                         (opcode == ch_uint<32>(opcode::LOAD)) ||
                         (opcode == ch_uint<32>(opcode::JALR));
        auto is_u_type = (opcode == ch_uint<32>(opcode::LUI)) || 
                         (opcode == ch_uint<32>(opcode::AUIPC));
        auto is_j_type = (opcode == ch_uint<32>(opcode::JAL));
        
        io().reg_write = is_r_type || is_i_type || is_u_type || is_j_type;
        
        // ALU 源选择
        io().alu_src = is_i_type || is_u_type;  // 立即数作为第二操作数
        
        // 内存控制
        io().mem_read = (opcode == ch_uint<32>(opcode::LOAD));
        io().mem_write = (opcode == ch_uint<32>(opcode::STORE));
        io().mem_to_reg = io().mem_read;
        
        // 分支/跳转
        io().branch = (opcode == ch_uint<32>(opcode::BRANCH));
        io().jump = is_j_type;
        io().jalr = (opcode == ch_uint<32>(opcode::JALR));
        
        // ALU 操作
        io().alu_op = funct3;
        
        // 减法 (SUB vs ADD)
        io().alu_sub = (funct7 == ch_uint<32>(funct7::SUB)) && 
                       (funct3 == ch_uint<32>(funct3::SUB));
        
        // 算术右移 (SRA vs SRL)
        io().alu_sra = (funct7 == ch_uint<32>(funct7::SRA)) && 
                       (funct3 == ch_uint<32>(funct3::SRA));
    }
};

} // namespace riscv
