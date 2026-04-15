/**
 * @file if_stage.h
 * @brief RISC-V 5 级流水线 - 取指级 (Instruction Fetch)
 * 
 * 功能:
 * - PC 寄存器 (32 位) 带时钟使能
 * - 指令存储器接口
 * - PC+4 计算
 * - 分支目标选择
 * - 停顿 (stall) 控制
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "rv32i_tcm.h"

using namespace ch::core;

namespace riscv {

/**
 * @brief IF 级 (取指级)
 * 
 * 从指令存储器中取指令，并传递给 ID 级
 */
class IfStage : public ch::Component {
public:
    __io(
        // 控制信号
        ch_in<ch_bool>      stall;          // 流水线停顿
        ch_in<ch_bool>      flush;          // 冲刷流水线 (分支预测错误)
        ch_in<ch_bool>      rst;            // 复位
        
        // 分支控制
        ch_in<ch_uint<32>>  branch_target;  // 分支目标地址
        ch_in<ch_bool>      branch_valid;   // 分支有效
        
        // 输出到 ID 级
        ch_out<ch_uint<32>> pc;             // 当前 PC
        ch_out<ch_uint<32>> instruction;    // 取出的指令
        ch_out<ch_bool>     valid;          // 有效信号
        
        // 指令存储器接口
        ch_out<ch_uint<32>> instr_addr;     // 指令地址输出
        ch_in<ch_uint<32>>  instr_data;     // 指令数据输入
        ch_in<ch_bool>      instr_ready;    // 指令存储器就绪
    )
    
    IfStage(ch::Component* parent = nullptr, const std::string& name = "if_stage")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // PC 寄存器 (32 位)
        // PC 寄存器存储当前指令地址
        // 复位值: 0x80000000 (ITCM 基地址, riscv-tests 入口点)
        ch_reg<ch_uint<32>> pc_reg(ch_uint<32>(0x80000000), "pc_reg");
        
        // ========================================================================
        // PC 更新逻辑
        // ========================================================================
        // 计算 PC+4 (RISC-V 指令固定 4 字节)
        auto pc_plus_4 = pc_reg + ch_uint<32>(4_d);
        
        // 选择下一个 PC 值：
        // - 分支有效时：使用分支目标地址
        // - 正常情况：使用 PC+4
        ch_uint<32> next_pc_target = select(
            io().branch_valid,
            io().branch_target,
            pc_plus_4
        );
        
        // PC 更新使能信号：
        // - 不复位 且 不 stall 且 不 flush 时更新
        auto pc_update_en = select(
            io().rst,
            ch_bool(false),
            select(io().stall, ch_bool(false), ch_bool(true))
        );
        
        // flush 时 PC 重置为 0 (简化处理，实际应该是分支目标)
        auto pc_flush_val = select(
            io().flush,
            ch_uint<32>(0_d),
            next_pc_target
        );
        
        // PC 寄存器更新
        pc_reg->next = select(
            pc_update_en,
            pc_flush_val,
            pc_reg
        );
        
        // ========================================================================
        // 指令存储器接口
        // ========================================================================
        // 输出当前 PC 作为指令地址
        io().instr_addr = pc_reg;
        
        // 从指令存储器读取的指令 (组合逻辑)
        ch_uint<32> fetched_instr(0_d, "fetched_instr");
        fetched_instr = io().instr_data;
        
        // ========================================================================
        // 流水线寄存器 (IF 级到 ID 级的输出)
        // ========================================================================
        // IF/ID 流水线寄存器
        ch_reg<ch_uint<32>> if_id_pc_reg(0_d, "if_id_pc");
        ch_reg<ch_uint<32>> if_id_instr_reg(0_d, "if_id_instr");
        ch_reg<ch_bool>     if_id_valid_reg(false, "if_id_valid");
        
        // IF/ID 寄存器更新使能：
        // - 指令存储器就绪
        // - 不复位
        // - 不 stall
        // - 不 flush
        auto if_id_update_en = select(
            io().rst,
            ch_bool(false),
            select(
                io().stall || io().flush,
                ch_bool(false),
                ch_bool(true)
            )
        );
        
        // IF/ID 寄存器更新
        if_id_pc_reg->next = select(
            if_id_update_en,
            pc_reg,
            if_id_pc_reg
        );
        
        if_id_instr_reg->next = select(
            if_id_update_en,
            fetched_instr,
            if_id_instr_reg
        );
        
        // 有效信号在 flush 时清零
        if_id_valid_reg->next = select(
            if_id_update_en,
            io().instr_ready,
            select(
                io().flush,
                ch_bool(false),
                if_id_valid_reg
            )
        );
        
        // ========================================================================
        // 输出到 ID 级
        // ========================================================================
        io().pc = if_id_pc_reg;
        io().instruction = if_id_instr_reg;
        io().valid = if_id_valid_reg;
    }
};

} // namespace riscv
