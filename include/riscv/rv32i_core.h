/**
 * @file rv32i_core.h
 * @brief RV32I RISC-V 处理器核心
 * 
 * 实现 RV32I 基础整数指令集 (40 条指令):
 * - 整数运算：ADD, SUB, SLL, SRL, SRA, AND, OR, XOR
 * - 比较运算：SLT, SLTU, SLTI, SLTIU
 * - 移位运算：SLLI, SRLI, SRAI
 * - 跳转指令：JAL, JALR
 * - 分支指令：BEQ, BNE, BLT, BGE, BLTU, BGEU
 * - 加载指令：LB, LH, LW, LBU, LHU
 * - 存储指令：SB, SH, SW
 * - 上位立即数：LUI, AUIPC
 * 
 * 不包含：
 * - CSR 指令 (M 模式)
 * - 乘除法 (M 扩展)
 * - 原子操作 (A 扩展)
 * - 浮点运算 (F/D 扩展)
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "riscv/rv32i_isa.h"
#include "riscv/rv32i_regs.h"
#include "riscv/rv32i_alu.h"
#include "riscv/rv32i_decoder.h"
#include "riscv/rv32i_pc.h"

using namespace ch::core;

namespace riscv {

// ============================================================================
// RV32I 核心
// ============================================================================

class RV32ICore : public ch::Component {
public:
    __io(
        // AXI4 指令接口 (简化)
        ch_out<ch_uint<32>> i_addr;
        ch_in<ch_bool> i_valid;
        ch_out<ch_bool> i_ready;
        ch_in<ch_uint<32>> i_data;
        
        // AXI4 数据接口 (简化)
        ch_out<ch_uint<32>> d_addr;
        ch_out<ch_bool> d_write;
        ch_out<ch_uint<32>> d_wdata;
        ch_out<ch_bool> d_valid;
        ch_in<ch_bool> d_ready;
        ch_in<ch_uint<32>> d_rdata;
        
        // 控制接口
        ch_in<ch_bool> reset;
        ch_in<ch_bool> halt;
        ch_out<ch_bool> running;
    )
    
    RV32ICore(ch::Component* parent = nullptr, const std::string& name = "rv32i_core")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 程序计数器
        // ========================================================================
        
        ch::ch_module<ProgramCounter> pc{"pc"};
        pc.io().reset <<= io().reset;
        pc.io().pc_update <<= ch_bool(true);  // 每周期更新
        pc.io().pc_next <<= ch_uint<32>(0_d); // 由跳转逻辑控制
        pc.io().pc <<= io().i_addr;
        
        // ========================================================================
        // 指令存储器接口
        // ========================================================================
        
        // 指令地址 = PC
        io().i_addr = pc.io().pc;
        io().i_ready = ch_bool(true);  // 简化：假设单周期指令存储器
        
        // ========================================================================
        // 指令译码
        // ========================================================================
        
        ch::ch_module<InstrDecoder> decoder{"decoder"};
        decoder.io().instr <<= io().i_data;
        
        // ========================================================================
        // 寄存器文件
        // ========================================================================
        
        ch::ch_module<RegisterFile> regfile{"regfile"};
        
        // 读端口 1
        regfile.io().rs1_addr <<= decoder.io().rs1_addr;
        ch_uint<32> rs1_data;
        rs1_data <<= regfile.io().rs1_data;
        
        // 读端口 2
        regfile.io().rs2_addr <<= decoder.io().rs2_addr;
        ch_uint<32> rs2_data;
        rs2_data <<= regfile.io().rs2_data;
        
        // 写端口
        regfile.io().rd_addr <<= decoder.io().rd_addr;
        regfile.io().rd_write_en <<= decoder.io().reg_write;
        
        // ========================================================================
        // ALU
        // ========================================================================
        
        ch::ch_module<ALU> alu{"alu"};
        
        // ALU 输入 A: 寄存器数据
        alu.io().operand_a <<= rs1_data;
        
        // ALU 输入 B: 寄存器数据或立即数
        ch_uint<32> alu_operand_b;
        alu_operand_b = select(decoder.io().alu_src, decoder.io().imm, rs2_data);
        alu.io().operand_b <<= alu_operand_b;
        
        // ALU 控制
        alu.io().alu_op <<= decoder.io().alu_op;
        alu.io().is_sub <<= decoder.io().alu_sub;
        alu.io().is_sra <<= decoder.io().alu_sra;
        
        ch_uint<32> alu_result;
        alu_result <<= alu.io().result;
        
        // ========================================================================
        // 数据存储器接口
        // ========================================================================
        
        // 数据地址 = ALU 结果 (基址 + 偏移)
        io().d_addr = alu_result;
        io().d_wdata = rs2_data;
        io().d_write = decoder.io().mem_write;
        io().d_valid = select(decoder.io().mem_read || decoder.io().mem_write,
                              ch_bool(true), ch_bool(false));
        
        // 数据读取
        ch_uint<32> mem_data;
        mem_data = select(decoder.io().mem_read, io().d_rdata, ch_uint<32>(0_d));
        
        // ========================================================================
        // 写回逻辑
        // ========================================================================
        
        ch_uint<32> writeback_data;
        writeback_data = select(decoder.io().mem_to_reg, mem_data, alu_result);
        
        // PC 相对跳转 (AUIPC, JAL)
        auto pc_relative = select(decoder.io().jump || decoder.io().alu_src,
                                   alu_result, ch_uint<32>(0_d));
        writeback_data = select(decoder.io().jump, pc.io().pc, writeback_data);
        
        // 写回寄存器
        regfile.io().rd_data <<= writeback_data;
        
        // ========================================================================
        // PC 更新逻辑
        // ========================================================================
        
        // 分支目标地址
        auto branch_target = alu_result;
        
        // 分支条件
        auto branch_taken = select(decoder.io().branch,
                                    select(alu.io().zero, ch_bool(true), ch_bool(false)),
                                    ch_bool(false));
        
        // 跳转目标 (JAL/JALR)
        auto jump_target = select(decoder.io().jalr, alu_result, branch_target);
        
        // 下一个 PC
        auto next_pc = select(decoder.io().jump || decoder.io().jalr,
                              jump_target,
                              select(branch_taken,
                                     branch_target,
                                     pc.io().pc + ch_uint<32>(4_d)));
        pc.io().pc_next <<= next_pc;
        
        // ========================================================================
        // 状态输出
        // ========================================================================
        
        io().running = select(io().halt, ch_bool(false), ch_bool(true));
    }
};

} // namespace riscv
