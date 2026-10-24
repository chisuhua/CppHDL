/**
 * @file rv32i_core.h
 * @brief RISC-V 5 级流水线冒险检测单元
 * 
 * 注意：Rv32iCore 类已删除（骨架代码，无实际连线）
 * 此文件仅保留 HazardDetectionUnit 组件
 * 
 * 日期：2026-04-09
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "rv32i_regfile.h"
#include "rv32i_alu.h"
#include "rv32i_decoder.h"
#include "rv32i_pc.h"
#include "rv32i_pipeline_regs.h"
#include "rv32i_forwarding.h"
#include "rv32i_branch_predict.h"

using namespace ch::core;

namespace riscv {

/** 冒险检测单元 */
class HazardDetectionUnit : public ch::Component {
public:
    __io(
        ch_in<ch_uint<5>>  id_ex_rs1_addr; ch_in<ch_uint<5>>  id_ex_rs2_addr;
        ch_in<ch_bool>     id_ex_reg_write; ch_in<ch_bool>     ex_mem_is_load;
        ch_in<ch_uint<5>>  ex_mem_rd; ch_in<ch_bool>     ex_mem_reg_write;
        ch_out<ch_bool>    stall_if; ch_out<ch_bool>    stall_id; ch_out<ch_bool>    flush_id_ex
    )
    HazardDetectionUnit(ch::Component* p = nullptr, const std::string& n = "hazard") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // ========================================================================
        // LOAD-USE 冒险检测
        // ========================================================================
        // 条件：
        // 1. EX/MEM 阶段是 LOAD 指令
        // 2. EX/MEM 阶段要写寄存器 (rd != 0)
        // 3. ID/EX 阶段的 rs1 或 rs2 与 LOAD 的 rd 相同
        // 4. ID/EX 阶段要写寄存器
        
        auto load_rd_nz = io().ex_mem_rd != ch_uint<5>(0_d);  // rd != 0
        auto rs1_match = io().ex_mem_rd == io().id_ex_rs1_addr;  // rs1 地址匹配
        auto rs2_match = io().ex_mem_rd == io().id_ex_rs2_addr;  // rs2 地址匹配
        auto rs_match = rs1_match | rs2_match;  // 任一匹配
        
        // LOAD-USE 冒险条件
        auto load_use_hazard = io().ex_mem_is_load & load_rd_nz & rs_match 
                               & io().ex_mem_reg_write & io().id_ex_reg_write;
        
        // ========================================================================
        // stall/flush 信号生成
        // ========================================================================
        io().stall_if = load_use_hazard;
        io().stall_id = load_use_hazard;
        io().flush_id_ex = load_use_hazard;  // 注意：实际实现中可能需要 & !branch_taken
    }
};

} // namespace riscv
