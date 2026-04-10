/**
 * @file hazard_unit_bundle.h
 * @brief Hazard Unit 通用 Bundle 接口
 * 
 * 设计目标：
 * 1. 定义冒险检测与前推单元的标准接口
 * 2. 支持不同流水线配置（5 级、6 级、8 级等）
 * 3. 通过 Bundle 实现模块化连接
 * 
 * @author: DevMate
 * @date: 2026-04-10
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "core/bundle/bundle_base.h"

using namespace ch::core;

namespace cpu {

/**
 * @brief Hazard Unit 通用接口配置
 */
struct HazardUnitConfig {
    static constexpr unsigned REG_ADDR_WIDTH = 5;   // 寄存器地址位宽
    static constexpr unsigned DATA_WIDTH = 32;      // 数据位宽
    static constexpr unsigned FORWARD_SEL_WIDTH = 2; // 前推选择位宽
};

/**
 * @brief Hazard Unit Bundle 接口
 * 
 * @tparam RegAddrT 寄存器地址类型
 * @tparam DataT 数据类型
 */
template<
    typename RegAddrT = ch_uint<HazardUnitConfig::REG_ADDR_WIDTH>,
    typename DataT = ch_uint<HazardUnitConfig::DATA_WIDTH>
>
class HazardUnitBundle : public ch::core::bundle_base<HazardUnitBundle<RegAddrT, DataT>> {
    using Self = HazardUnitBundle<RegAddrT, DataT>;
    
public:
    // ========================================================================
    // 流水线寄存器地址输入
    // ========================================================================
    
    /** ID 阶段 RS1 地址 */
    RegAddrT id_rs1_addr;
    
    /** ID 阶段 RS2 地址 */
    RegAddrT id_rs2_addr;
    
    /** EX 阶段目标寄存器地址 */
    RegAddrT ex_rd_addr;
    
    /** MEM 阶段目标寄存器地址 */
    RegAddrT mem_rd_addr;
    
    /** WB 阶段目标寄存器地址 */
    RegAddrT wb_rd_addr;
    
    // ========================================================================
    // 写使能信号
    // ========================================================================
    
    /** EX 级将写寄存器 */
    ch_bool ex_reg_write;
    
    /** MEM 级将写寄存器 */
    ch_bool mem_reg_write;
    
    /** WB 级将写寄存器 */
    ch_bool wb_reg_write;
    
    // ========================================================================
    // 指令类型
    // ========================================================================
    
    /** MEM 级是加载指令 */
    ch_bool mem_is_load;
    
    /** EX 级是分支指令 */
    ch_bool ex_branch;
    
    /** 分支已采取（EX 级确认） */
    ch_bool ex_branch_taken;
    
    // ========================================================================
    // 前推数据输入
    // ========================================================================
    
    /** EX 级 ALU 结果 */
    DataT ex_alu_result;
    
    /** MEM 级 ALU 结果 */
    DataT mem_alu_result;
    
    /** WB 级写回数据 */
    DataT wb_write_data;
    
    // ========================================================================
    // 原始寄存器数据输入
    // ========================================================================
    
    /** 来自寄存器文件的 RS1 */
    DataT rs1_data_raw;
    
    /** 来自寄存器文件的 RS2 */
    DataT rs2_data_raw;
    
    // ========================================================================
    // 前推控制输出
    // ========================================================================
    
    /** RS1 的前推源选择：0=寄存器，1=EX，2=MEM，3=WB */
    ch_out<ch_uint<HazardUnitConfig::FORWARD_SEL_WIDTH>> forward_a;
    
    /** RS2 的前推源选择 */
    ch_out<ch_uint<HazardUnitConfig::FORWARD_SEL_WIDTH>> forward_b;
    
    // ========================================================================
    // 前推数据输出
    // ========================================================================
    
    /** 前推后的 RS1 数据 */
    ch_out<DataT> rs1_data;
    
    /** 前推后的 RS2 数据 */
    DataT rs2_data;  // 注意：这里应该是 ch_out，但为了兼容性暂时保留
    
    // ========================================================================
    // 控制信号输出
    // ========================================================================
    
    /** IF 级停顿 */
    ch_out<ch_bool> stall_if;
    
    /** ID 级停顿 */
    ch_out<ch_bool> stall_id;
    
    /** ID/EX 流水线寄存器刷新 */
    ch_out<ch_bool> flush_id_ex;
    
    // ========================================================================
    // 构造函数
    // ========================================================================
    
    HazardUnitBundle() = default;
    
    explicit HazardUnitBundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }
    
    // ========================================================================
    // Bundle 字段注册
    // ========================================================================
    
    CH_BUNDLE_FIELDS_T(
        id_rs1_addr, id_rs2_addr, ex_rd_addr, mem_rd_addr, wb_rd_addr,
        ex_reg_write, mem_reg_write, wb_reg_write,
        mem_is_load, ex_branch, ex_branch_taken,
        ex_alu_result, mem_alu_result, wb_write_data,
        rs1_data_raw, rs2_data_raw,
        forward_a, forward_b, rs1_data, rs2_data,
        stall_if, stall_id, flush_id_ex
    )
    
    // ========================================================================
    // Bundle 方向设置
    // ========================================================================
    
    void as_master() {
        // 输入
        this->template make_input<RegAddrT>(
            id_rs1_addr, id_rs2_addr, ex_rd_addr, mem_rd_addr, wb_rd_addr);
        this->make_input(
            ex_reg_write, mem_reg_write, wb_reg_write,
            mem_is_load, ex_branch, ex_branch_taken);
        this->template make_input<DataT>(
            ex_alu_result, mem_alu_result, wb_write_data,
            rs1_data_raw, rs2_data_raw);
        
        // 输出
        this->template make_output<ch_uint<HazardUnitConfig::FORWARD_SEL_WIDTH>>(
            forward_a, forward_b);
        this->template make_output<DataT>(rs1_data, rs2_data);
        this->make_output(stall_if, stall_id, flush_id_ex);
    }
};

} // namespace cpu
