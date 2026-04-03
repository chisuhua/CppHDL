/**
 * @file rv32i_forwarding.h
 * @brief RV32I 数据前推单元定义
 * 
 * 功能：检测数据冒险 (RAW) 并实现前推逻辑，避免流水线停顿
 * 
 * 冒险类型:
 * - RAW (Read After Write): 需要前推 ✅
 * - WAW (Write After Write): RV32I 顺序执行，不会发生
 * - WAR (Write After Read): RV32I 顺序执行，不会发生
 * 
 * 前推路径:
 * - EX → EX: EX/MEM 阶段的 ALU 结果前推到 ID/EX 的 ALU 输入
 * - MEM → EX: MEM/WB 阶段的访存结果前推到 ID/EX 的 ALU 输入
 * - WB → EX: MEM/WB 阶段的写回结果前推到 ID/EX 的 ALU 输入
 * 
 * 前推优先级:
 * 1. EX/MEM (最近的结果，优先级最高)
 * 2. MEM/WB (较旧的结果)
 * 3. 寄存器文件 (无前推)
 */

#pragma once

#include "ch.hpp"
#include "riscv/rv32i_pipeline_regs.h"

using namespace ch::core;

namespace riscv {

// ============================================================================
// 前推检测逻辑 (组合逻辑)
// ============================================================================
/**
 * @brief 前推检测单元
 * 
 * 比较寄存器地址，检测是否需要前推
 * 
 * 输入:
 * - id_ex_rs1_addr: ID/EX 阶段 RS1 寄存器地址 (5 位)
 * - id_ex_rs2_addr: ID/EX 阶段 RS2 寄存器地址 (5 位)
 * - ex_mem_rd:      EX/MEM 阶段目的寄存器地址 (5 位)
 * - ex_mem_reg_write: EX/MEM 阶段寄存器写使能
 * - mem_wb_rd:      MEM/WB 阶段目的寄存器地址 (5 位)
 * - mem_wb_reg_write: MEM/WB 阶段寄存器写使能
 * 
 * 输出:
 * - forward_a: RS1 的前推源选择 (2 位)
 * - forward_b: RS2 的前推源选择 (2 位)
 * 
 * 前推条件 (RS1 为例，RS2 同理):
 * 1. EX/MEM 前推：ex_mem_reg_write & (ex_mem_rd != 0) & (ex_mem_rd == id_ex_rs1_addr)
 * 2. MEM/WB 前推：mem_wb_reg_write & (mem_wb_rd != 0) & (mem_wb_rd == id_ex_rs1_addr)
 */
class ForwardingUnit : public ch::Component {
public:
    __io(
        // 来自 ID/EX 阶段：需要前推的寄存器地址
        ch_in<ch_uint<5>>  id_ex_rs1_addr;     // RS1 寄存器地址
        ch_in<ch_uint<5>>  id_ex_rs2_addr;     // RS2 寄存器地址
        
        // 来自 EX/MEM 阶段：可能产生前推的信息
        ch_in<ch_uint<5>>  ex_mem_rd;          // 目的寄存器地址
        ch_in<ch_bool>     ex_mem_reg_write;   // 寄存器写使能
        
        // 来自 MEM/WB 阶段：可能产生前推的信息
        ch_in<ch_uint<5>>  mem_wb_rd;          // 目的寄存器地址
        ch_in<ch_bool>     mem_wb_reg_write;   // 寄存器写使能
        
        // 前推控制输出
        ch_out<ch_uint<2>> forward_a;          // RS1 前推源选择 (0=REG, 1=EX, 2=MEM)
        ch_out<ch_uint<2>> forward_b           // RS2 前推源选择 (0=REG, 1=EX, 2=MEM)
    )
    
    ForwardingUnit(ch::Component* parent = nullptr, const std::string& name = "forwarding_unit")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // =========================================================================
        // RS1 前推检测逻辑
        // =========================================================================
        // 检查 EX/MEM 前推条件：reg_write & (rd != 0) & (rd == rs)
        auto ex_mem_rd_nonzero = io().ex_mem_rd != ch_uint<5>(0_d);
        auto ex_mem_rd_match_rs1 = io().ex_mem_rd == io().id_ex_rs1_addr;
        auto ex_mem_forward_a = io().ex_mem_reg_write & ex_mem_rd_nonzero & ex_mem_rd_match_rs1;
        
        // 检查 MEM/WB 前推条件
        auto mem_wb_rd_nonzero = io().mem_wb_rd != ch_uint<5>(0_d);
        auto mem_wb_rd_match_rs1 = io().mem_wb_rd == io().id_ex_rs1_addr;
        auto mem_wb_forward_a = io().mem_wb_reg_write & mem_wb_rd_nonzero & mem_wb_rd_match_rs1;
        
        // RS1 前推源选择 (优先级：EX/MEM > MEM/WB > REG)
        io().forward_a = select(ex_mem_forward_a, 
                                ch_uint<2>(1_d),  // EX/MEM 前推
                                select(mem_wb_forward_a,
                                       ch_uint<2>(2_d),  // MEM/WB 前推
                                       ch_uint<2>(0_d))); // 寄存器文件
        
        // =========================================================================
        // RS2 前推检测逻辑 (与 RS1 相同)
        // =========================================================================
        auto ex_mem_rd_match_rs2 = io().ex_mem_rd == io().id_ex_rs2_addr;
        auto ex_mem_forward_b = io().ex_mem_reg_write & ex_mem_rd_nonzero & ex_mem_rd_match_rs2;
        
        auto mem_wb_rd_match_rs2 = io().mem_wb_rd == io().id_ex_rs2_addr;
        auto mem_wb_forward_b = io().mem_wb_reg_write & mem_wb_rd_nonzero & mem_wb_rd_match_rs2;
        
        // RS2 前推源选择 (优先级：EX/MEM > MEM/WB > REG)
        io().forward_b = select(ex_mem_forward_b, 
                                ch_uint<2>(1_d),  // EX/MEM 前推
                                select(mem_wb_forward_b,
                                       ch_uint<2>(2_d),  // MEM/WB 前推
                                       ch_uint<2>(0_d))); // 寄存器文件
    }
};

// ============================================================================
// 前推数据选择器 (多路复用器)
// ============================================================================
/**
 * @brief 前推数据选择单元
 * 
 * 根据前推控制信号，选择正确的数据源
 * 
 * 输入:
 * - forward_a/b: 前推源选择信号 (来自 ForwardingUnit)
 * - rs1_data: 从寄存器文件读取的 RS1 数据
 * - rs2_data: 从寄存器文件读取的 RS2 数据
 * - ex_mem_alu_result: EX/MEM 阶段的 ALU 结果
 * - mem_wb_alu_result: MEM/WB 阶段的 ALU 结果
 * - mem_wb_mem_read_data: MEM/WB 阶段的内存读数据
 * - mem_wb_is_load: MEM/WB 阶段是否为 LOAD 指令
 * 
 * 输出:
 * - alu_input_a: 前推后的 ALU 输入 A
 * - alu_input_b: 前推后的 ALU 输入 B
 */
class ForwardingMux : public ch::Component {
public:
    __io(
        // 前推控制信号 (来自 ForwardingUnit)
        ch_in<ch_uint<2>>  forward_a;
        ch_in<ch_uint<2>>  forward_b;
        
        // 寄存器文件输出 (原始数据)
        ch_in<ch_uint<32>> rs1_data_reg;
        ch_in<ch_uint<32>> rs2_data_reg;
        
        // EX/MEM 阶段数据 (前推源 1)
        ch_in<ch_uint<32>> ex_mem_alu_result;
        
        // MEM/WB 阶段数据 (前推源 2)
        ch_in<ch_uint<32>> mem_wb_alu_result;
        ch_in<ch_uint<32>> mem_wb_mem_read_data;
        ch_in<ch_bool>     mem_wb_is_load;
        
        // ALU 输入输出 (前推后的数据)
        ch_out<ch_uint<32>> alu_input_a;
        ch_out<ch_uint<32>> alu_input_b
    )
    
    ForwardingMux(ch::Component* parent = nullptr, const std::string& name = "forwarding_mux")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // =========================================================================
        // MEM/WB 结果选择：LOAD 指令用内存读数据，其他用 ALU 结果
        // =========================================================================
        auto mem_wb_result = select(io().mem_wb_is_load, 
                                    io().mem_wb_mem_read_data, 
                                    io().mem_wb_alu_result);
        
        // =========================================================================
        // ALU 输入 A 选择 (RS1 路径)
        // forward_a = 0: 寄存器文件
        // forward_a = 1: EX/MEM ALU 结果
        // forward_a = 2: MEM/WB 结果
        // =========================================================================
        auto sel_a_is_reg = io().forward_a == ch_uint<2>(0_d);
        auto sel_a_is_ex = io().forward_a == ch_uint<2>(1_d);
        
        io().alu_input_a = select(sel_a_is_reg,
                                  io().rs1_data_reg,
                                  select(sel_a_is_ex,
                                         io().ex_mem_alu_result,
                                         mem_wb_result));
        
        // =========================================================================
        // ALU 输入 B 选择 (RS2 路径)
        // forward_b = 0: 寄存器文件
        // forward_b = 1: EX/MEM ALU 结果
        // forward_b = 2: MEM/WB 结果
        // =========================================================================
        auto sel_b_is_reg = io().forward_b == ch_uint<2>(0_d);
        auto sel_b_is_ex = io().forward_b == ch_uint<2>(1_d);
        
        io().alu_input_b = select(sel_b_is_reg,
                                  io().rs2_data_reg,
                                  select(sel_b_is_ex,
                                         io().ex_mem_alu_result,
                                         mem_wb_result));
    }
};

} // namespace riscv
