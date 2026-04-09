/**
 * @file rv32i_forwarding.h
 * @brief RISC-V 5 级流水线前推单元
 * 
 * 支持 3 路数据前推:
 * - EX→EX: ALU 结果直接前推
 * - MEM→EX: MEM 级 ALU 结果前推
 * - WB→EX: WB 级数据前推
 * 
 * 前推优先级：EX(1) > MEM(2) > WB(3) > REG(0)
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
 * @brief 前推源选择
 */
enum class ForwardSrc : unsigned {
    REG = 0,  // 来自寄存器堆
    EX = 1,   // 来自 EX/MEM 流水线寄存器
    MEM = 2,  // 来自 MEM/WB 流水线寄存器
    WB = 3    // 来自 WB 阶段
};

/**
 * @brief 前推单元配置
 */
struct ForwardingConfig {
    static constexpr unsigned RS1_SRC_BITS = 2;
    static constexpr unsigned RS2_SRC_BITS = 2;
};

/**
 * @brief 前推控制信号
 */
struct ForwardingControl {
    ch_bool rs1_forward_ex;     // RS1 从 EX 前推
    ch_bool rs1_forward_mem;    // RS1 从 MEM 前推
    ch_bool rs2_forward_ex;     // RS2 从 EX 前推
    ch_bool rs2_forward_mem;    // RS2 从 MEM 前推
};

/**
 * @brief 前推单元
 * 
 * 根据冒险检测结果，选择正确的数据源
 */
class ForwardingUnit : public ch::Component {
public:
    __io(
        // ID 级寄存器读地址
        ch_in<ch_uint<5>> id_rs1_addr;
        ch_in<ch_uint<5>> id_rs2_addr;
        ch_in<ch_bool> id_reg_read;
        
        // EX 级写寄存器信息
        ch_in<ch_uint<5>> ex_rd_addr;
        ch_in<ch_bool> ex_reg_write;
        
        // MEM 级写寄存器信息
        ch_in<ch_uint<5>> mem_rd_addr;
        ch_in<ch_bool> mem_reg_write;
        
        // WB 级写寄存器信息
        ch_in<ch_uint<5>> wb_rd_addr;
        ch_in<ch_bool> wb_reg_write;
        
        // 原始寄存器数据
        ch_in<ch_uint<32>> rs1_data_reg;
        ch_in<ch_uint<32>> rs2_data_reg;
        
        // 前推数据
        ch_in<ch_uint<32>> ex_alu_result;
        ch_in<ch_uint<32>> mem_alu_result;
        ch_in<ch_uint<32>> wb_write_data;
        
        // 前推选通输出
        ch_out<ch_uint<32>> rs1_data;
        ch_out<ch_uint<32>> rs2_data;
        
        // 前推控制信号输出
        ch_out<ch_uint<2>> rs1_src;
        ch_out<ch_uint<2>> rs2_src;
    )

    ForwardingUnit(ch::Component* parent = nullptr, 
                   const std::string& name = "forwarding_unit")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // RS1 前推逻辑
        // =====================================================================
        ch_bool rs1_match_ex = (io().id_rs1_addr == io().ex_rd_addr) && 
                               io().ex_reg_write && 
                               (io().id_rs1_addr != ch_uint<5>(0_d));
        
        ch_bool rs1_match_mem = (io().id_rs1_addr == io().mem_rd_addr) && 
                                io().mem_reg_write && 
                                (io().id_rs1_addr != ch_uint<5>(0_d));
        
        ch_bool rs1_match_wb = (io().id_rs1_addr == io().wb_rd_addr) && 
                               io().wb_reg_write && 
                               (io().id_rs1_addr != ch_uint<5>(0_d));
        
        // RS1 数据选择：优先级 EX > MEM > WB > REG
        auto rs1_result = select(rs1_match_ex, io().ex_alu_result,
            select(rs1_match_mem, io().mem_alu_result,
                select(rs1_match_wb, io().wb_write_data,
                    io().rs1_data_reg)));
        
        io().rs1_data <<= rs1_result;
        
        // RS1 源选择
        io().rs1_src = select(rs1_match_ex, ch_uint<2>(1_d),
            select(rs1_match_mem, ch_uint<2>(2_d),
                select(rs1_match_wb, ch_uint<2>(3_d),
                    ch_uint<2>(0_d))));
        
        // =====================================================================
        // RS2 前推逻辑
        // =====================================================================
        ch_bool rs2_match_ex = (io().id_rs2_addr == io().ex_rd_addr) && 
                               io().ex_reg_write && 
                               (io().id_rs2_addr != ch_uint<5>(0_d));
        
        ch_bool rs2_match_mem = (io().id_rs2_addr == io().mem_rd_addr) && 
                                io().mem_reg_write && 
                                (io().id_rs2_addr != ch_uint<5>(0_d));
        
        ch_bool rs2_match_wb = (io().id_rs2_addr == io().wb_rd_addr) && 
                               io().wb_reg_write && 
                               (io().id_rs2_addr != ch_uint<5>(0_d));
        
        // RS2 数据选择：优先级 EX > MEM > WB > REG
        auto rs2_result = select(rs2_match_ex, io().ex_alu_result,
            select(rs2_match_mem, io().mem_alu_result,
                select(rs2_match_wb, io().wb_write_data,
                    io().rs2_data_reg)));
        
        io().rs2_data <<= rs2_result;
        
        // RS2 源选择
        io().rs2_src = select(rs2_match_ex, ch_uint<2>(1_d),
            select(rs2_match_mem, ch_uint<2>(2_d),
                select(rs2_match_wb, ch_uint<2>(3_d),
                    ch_uint<2>(0_d))));
    }
};

} // namespace riscv
