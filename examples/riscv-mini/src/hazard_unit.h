/**
 * @file hazard_unit.h
 * @brief RISC-V 流水线冒险检测单元（数据前推、停顿、刷新控制）
 * 
 * 参考：include/chlib/forwarding.h - 使用单独的 ch_uint 控制信号
 * 
 * 功能：
 * 1. 数据前推 (Forwarding): EX→EX, MEM→EX, WB→EX
 * 2. Load-Use 冒险检测：生成停顿信号
 * 3. 控制冒险处理：分支刷新
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */
#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

/**
 * @brief 冒险检测与前推单元
 * 
 * 参考 forwarding.h 模式：使用单独的控制信号端口而非 bundle 结构体
 */
class HazardUnit : public ch::Component {
public:
    __io(
        // ========================================================================
        // 流水线寄存器地址信息
        // ========================================================================
        ch_in<ch_uint<5>> id_rs1_addr;       // ID 阶段 RS1 地址
        ch_in<ch_uint<5>> id_rs2_addr;       // ID 阶段 RS2 地址
        ch_in<ch_uint<5>> ex_rd_addr;        // EX 阶段目标寄存器
        ch_in<ch_uint<5>> mem_rd_addr;       // MEM 阶段目标寄存器
        ch_in<ch_uint<5>> wb_rd_addr;        // WB 阶段目标寄存器
        
        // ========================================================================
        // 写使能信号
        // ========================================================================
        ch_in<ch_bool> ex_reg_write;         // EX 级将写寄存器
        ch_in<ch_bool> mem_reg_write;        // MEM 级将写寄存器
        ch_in<ch_bool> wb_reg_write;         // WB 级将写寄存器
        
        // ========================================================================
        // 指令类型
        // ========================================================================
        ch_in<ch_bool> mem_is_load;          // MEM 级是加载指令
        ch_in<ch_bool> ex_branch;            // EX 级是分支指令
        ch_in<ch_bool> ex_branch_taken;      // 分支已采取
        
        // ========================================================================
        // 前推数据输入
        // ========================================================================
        ch_in<ch_uint<32>> ex_alu_result;    // EX 级 ALU 结果
        ch_in<ch_uint<32>> mem_alu_result;   // MEM 级 ALU 结果
        ch_in<ch_uint<32>> wb_write_data;    // WB 级写回数据
        
        // ========================================================================
        // 原始寄存器数据
        // ========================================================================
        ch_in<ch_uint<32>> rs1_data_raw;     // 来自寄存器文件的 RS1
        ch_in<ch_uint<32>> rs2_data_raw;     // 来自寄存器文件的 RS2
        
        // ========================================================================
        // 前推控制输出 (参考 forwarding.h 模式：使用 ch_uint 而非 bundle)
        // ========================================================================
        // forward_a/b: 0=来自寄存器，1=来自 EX，2=来自 MEM
        ch_out<ch_uint<2>> forward_a;        // RS1 的前推源选择
        ch_out<ch_uint<2>> forward_b;        // RS2 的前推源选择
        
        // ========================================================================
        // 前推数据输出（经过前推MUX 后的数据）
        // ========================================================================
        ch_out<ch_uint<32>> rs1_data;        // 前推后的 RS1 数据
        ch_out<ch_uint<32>> rs2_data;        // 前推后的 RS2 数据
        
        // ========================================================================
        // 冒险控制输出 (单独信号，不使用 bundle)
        // ========================================================================
        ch_out<ch_bool> stall;               // Load-Use 停顿信号
        ch_out<ch_bool> flush;               // 分支冲刷信号
        ch_out<ch_bool> pc_src;              // PC 源选择 (分支跳转)
    )
    
    HazardUnit(ch::Component* parent = nullptr, const std::string& name = "hazard") 
        : ch::Component(parent, name) {}
    
    void create_ports() override { 
        new (io_storage_) io_type; 
    }
    
    void describe() override {
        // 将 IO 端口赋值给 auto 变量（避免类型转换问题）
        auto ex_reg_wr = io().ex_reg_write;
        auto mem_reg_wr = io().mem_reg_write;
        auto wb_reg_wr = io().wb_reg_write;
        auto is_load = io().mem_is_load;
        auto is_branch = io().ex_branch;
        auto branch_taken = io().ex_branch_taken;
        
        auto ex_rd = io().ex_rd_addr;
        auto mem_rd = io().mem_rd_addr;
        auto wb_rd = io().wb_rd_addr;
        auto id_rs1 = io().id_rs1_addr;
        auto id_rs2 = io().id_rs2_addr;
        
        // ========================================================================
        // 数据前推逻辑（参考 forwarding.h 模式）
        // ========================================================================
        
        // 检查非零寄存器 (x0 不需要前推)
        auto ex_rd_nz = ex_rd != ch_uint<5>(0_d);
        auto mem_rd_nz = mem_rd != ch_uint<5>(0_d);
        
        // RS1 地址匹配检测
        auto rs1_match_ex = ex_rd == id_rs1;
        auto rs1_match_mem = mem_rd == id_rs1;
        
        // RS2 地址匹配检测
        auto rs2_match_ex = ex_rd == id_rs2;
        auto rs2_match_mem = mem_rd == id_rs2;
        
        // 前推使能条件：reg_write && rd!=0 && match
        auto forward_ex_to_a = ex_reg_wr & ex_rd_nz & rs1_match_ex;
        auto forward_mem_to_a = mem_reg_wr & mem_rd_nz & rs1_match_mem;
        
        auto forward_ex_to_b = ex_reg_wr & ex_rd_nz & rs2_match_ex;
        auto forward_mem_to_b = mem_reg_wr & mem_rd_nz & rs2_match_mem;
        
        // 前推控制信号输出 (优先级：EX=1 > MEM=2 > REG=0)
        io().forward_a = select(forward_ex_to_a,
                                ch_uint<2>(1_d),
                                select(forward_mem_to_a,
                                       ch_uint<2>(2_d),
                                       ch_uint<2>(0_d)));
        
        io().forward_b = select(forward_ex_to_b,
                                ch_uint<2>(1_d),
                                select(forward_mem_to_b,
                                       ch_uint<2>(2_d),
                                       ch_uint<2>(0_d)));
        
        // ========================================================================
        // 数据前推多选器：根据控制信号选择数据源
        // ========================================================================
        auto sel_a_is_ex = (io().forward_a == ch_uint<2>(1_d));
        auto sel_a_is_mem = (io().forward_a == ch_uint<2>(2_d));
        
        auto rs1_from_ex = select(sel_a_is_ex, io().ex_alu_result, io().rs1_data_raw);
        auto rs1_from_mem = select(sel_a_is_mem, io().mem_alu_result, rs1_from_ex);
        io().rs1_data = rs1_from_mem;
        
        auto sel_b_is_ex = (io().forward_b == ch_uint<2>(1_d));
        auto sel_b_is_mem = (io().forward_b == ch_uint<2>(2_d));
        
        auto rs2_from_ex = select(sel_b_is_ex, io().ex_alu_result, io().rs2_data_raw);
        auto rs2_from_mem = select(sel_b_is_mem, io().mem_alu_result, rs2_from_ex);
        io().rs2_data = rs2_from_mem;
        
        // ========================================================================
        // Load-Use 冒险检测
        // ========================================================================
        auto rs1_match_load = mem_rd == id_rs1;
        auto rs2_match_load = mem_rd == id_rs2;
        auto any_rs_match = rs1_match_load | rs2_match_load;
        
        // Load-Use 停顿条件：MEM 是 load && rd!=0 && (匹配 RS1 或 RS2)
        auto load_use_hazard = is_load & mem_rd_nz & any_rs_match;
        
        // ========================================================================
        // 控制冒险处理
        // ========================================================================
        auto branch_flush = is_branch & branch_taken;
        
        // ========================================================================
        // 控制信号输出
        // ========================================================================
        io().stall = load_use_hazard;
        io().flush = branch_flush;
        io().pc_src = branch_flush;
    }
};

} // namespace riscv
