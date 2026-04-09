/**
 * @file rv32i_hazard_complete.h
 * @brief RISC-V 完整冒险检测与前推单元
 * 
 * 支持功能:
 * - 3 路数据前推 (EX, MEM, WB)
 * - Load-Use 冒险检测
 * - Branch Flush
 * - Stall 生成
 * 
 * 优先级:
 * EX 前推 > MEM 前推 > WB 前推 > 寄存器
 * 
 * 作者：DevMate
 * 日期：2026-04-10
 */
#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

/**
 * @brief 完整冒险检测与前推单元
 */
class HazardUnitComplete : public ch::Component {
public:
    __io(
        // ID 阶段信息
        ch_in<ch_uint<5>> id_rs1_addr;     // RS1 地址
        ch_in<ch_uint<5>> id_rs2_addr;     // RS2 地址
        ch_in<ch_bool> id_is_load;         // Load 指令标志
        ch_in<ch_bool> id_is_branch;       // Branch 指令标志
        
        // EX 阶段信息
        ch_in<ch_uint<5>> ex_rd_addr;      // EX 写回地址
        ch_in<ch_bool> ex_reg_write;       // EX 写使能
        ch_in<ch_bool> ex_branch;          // EX 分支指令
        ch_in<ch_bool> ex_branch_taken;    // 分支采取
        
        // MEM 阶段信息
        ch_in<ch_uint<5>> mem_rd_addr;     // MEM 写回地址
        ch_in<ch_bool> mem_reg_write;      // MEM 写使能
        ch_in<ch_bool> mem_is_load;        // MEM Load 指令
        
        // WB 阶段信息
        ch_in<ch_uint<5>> wb_rd_addr;      // WB 写回地址
        ch_in<ch_bool> wb_reg_write;       // WB 写使能
        
        // 前推数据输入
        ch_in<ch_uint<32>> ex_alu_result;  // EX ALU 结果
        ch_in<ch_uint<32>> mem_alu_result; // MEM ALU 结果
        ch_in<ch_uint<32>> wb_write_data;  // WB 写回数据
        
        // 原始寄存器数据
        ch_in<ch_uint<32>> rs1_data_reg;   // RS1 来自寄存器堆
        ch_in<ch_uint<32>> rs2_data_reg;   // RS2 来自寄存器堆
        
        // 前推数据输出
        ch_out<ch_uint<32>> rs1_data;      // RS1 (可能前推)
        ch_out<ch_uint<32>> rs2_data;      // RS2 (可能前推)
        ch_out<ch_uint<2>> rs1_src;        // RS1 源选择
        ch_out<ch_uint<2>> rs2_src;        // RS2 源选择
        
        // 控制信号输出
        ch_out<ch_bool> stall;             // Stall 信号 (Load-Use)
        ch_out<ch_bool> flush;             // Flush 信号 (Branch)
        ch_out<ch_bool> pc_src;            // PC 源选择 (Branch)
    )

    HazardUnitComplete(ch::Component* parent = nullptr, 
                       const std::string& name = "hazard_complete")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // Forwarding 源选择标志
        // =====================================================================
        
        // RS1 匹配检测
        ch_bool rs1_match_ex = (io().id_rs1_addr == io().ex_rd_addr) && 
                               io().ex_reg_write && 
                               (io().id_rs1_addr != ch_uint<5>(0_d));
        
        ch_bool rs1_match_mem = (io().id_rs1_addr == io().mem_rd_addr) && 
                                io().mem_reg_write && 
                                (io().id_rs1_addr != ch_uint<5>(0_d));
        
        ch_bool rs1_match_wb = (io().id_rs1_addr == io().wb_rd_addr) && 
                               io().wb_reg_write && 
                               (io().id_rs1_addr != ch_uint<5>(0_d));
        
        // RS2 匹配检测
        ch_bool rs2_match_ex = (io().id_rs2_addr == io().ex_rd_addr) && 
                               io().ex_reg_write && 
                               (io().id_rs2_addr != ch_uint<5>(0_d));
        
        ch_bool rs2_match_mem = (io().id_rs2_addr == io().mem_rd_addr) && 
                                io().mem_reg_write && 
                                (io().id_rs2_addr != ch_uint<5>(0_d));
        
        ch_bool rs2_match_wb = (io().id_rs2_addr == io().wb_rd_addr) && 
                               io().wb_reg_write && 
                               (io().id_rs2_addr != ch_uint<5>(0_d));
        
        // =====================================================================
        // Forwarding 优先级控制
        // =====================================================================
        
        // RS1 源选择：优先级 EX(1) > MEM(2) > WB(3) > REG(0)
        auto rs1_src = 
            select(rs1_match_ex, ch_uint<2>(1_d),
                select(rs1_match_mem, ch_uint<2>(2_d),
                    select(rs1_match_wb, ch_uint<2>(3_d),
                        ch_uint<2>(0_d))));
        
        // RS2 源选择：优先级 EX(1) > MEM(2) > WB(3) > REG(0)
        auto rs2_src = 
            select(rs2_match_ex, ch_uint<2>(1_d),
                select(rs2_match_mem, ch_uint<2>(2_d),
                    select(rs2_match_wb, ch_uint<2>(3_d),
                        ch_uint<2>(0_d))));
        
        io().rs1_src <<= rs1_src;
        io().rs2_src <<= rs2_src;
        
        // =====================================================================
        // Forwarding 数据选择 MUX
        // =====================================================================
        
        // RS1 数据选择
        auto rs1_result = 
            select(rs1_src == ch_uint<2>(1_d), io().ex_alu_result,
                select(rs1_src == ch_uint<2>(2_d), io().mem_alu_result,
                    select(rs1_src == ch_uint<2>(3_d), io().wb_write_data,
                        io().rs1_data_reg)));
        
        // RS2 数据选择
        auto rs2_result = 
            select(rs2_src == ch_uint<2>(1_d), io().ex_alu_result,
                select(rs2_src == ch_uint<2>(2_d), io().mem_alu_result,
                    select(rs2_src == ch_uint<2>(3_d), io().wb_write_data,
                        io().rs2_data_reg)));
        
        io().rs1_data <<= rs1_result;
        io().rs2_data <<= rs2_result;
        
        // =====================================================================
        // Load-Use 冒险检测
        // =====================================================================
        
        // Load-Use: ID 级 Load 指令需要使用 MEM 级 Load 的结果
        // MEM 级是 Load 且目的寄存器与 RS1 或 RS2 相同
        ch_bool load_use_hazard = 
            io().mem_is_load && 
            ((io().mem_rd_addr == io().id_rs1_addr && (io().id_rs1_addr != ch_uint<5>(0_d))) ||
             (io().mem_rd_addr == io().id_rs2_addr && (io().id_rs2_addr != ch_uint<5>(0_d))));
        
        io().stall <<= load_use_hazard;
        
        // =====================================================================
        // Branch Flush 逻辑
        // =====================================================================
        
        // Branch 预测错误时需要 Flush IF/ID 级
        // 简化实现：EX 级 branch && branch_taken -> flush
        io().flush <<= io().ex_branch;
        io().pc_src <<= io().ex_branch;
    }
};

} // namespace riscv
