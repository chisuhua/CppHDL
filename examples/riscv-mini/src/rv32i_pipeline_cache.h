/**
 * @file rv32i_pipeline_cache.h
 * @brief RISC-V 5 级流水线带 Cache 集成
 * 
 * 集成内容:
 * - I-Cache: 4KB, 2-way, LRU
 * - D-Cache: 4KB, 2-way, Write-Through
 * 
 * 作者：DevMate
 * 日期：2026-04-10
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "cpu/cache/i_cache.h"
#include "cpu/cache/d_cache.h"
#include "stages/if_stage.h"
#include "stages/id_stage.h"
#include "stages/ex_stage.h"
#include "stages/mem_stage.h"
#include "stages/wb_stage.h"
#include "hazard_unit.h"
#include "rv32i_pipeline_regs.h"

using namespace ch::core;

namespace riscv {

/**
 * @brief 带 Cache 的 5 级流水线 CPU
 * 
 * 架构:
 * IF → ID → EX → MEM → WB
 *  ↑                    ↓
 * I-Cache            D-Cache
 */
class Rv32iPipelineWithCache : public ch::Component {
public:
    __io(
        // AXI 指令接口 (I-Cache 填充)
        ch_out<ch_uint<32>> axi_instr_addr;
        ch_in<ch_bool> axi_instr_ready;
        ch_in<ch_uint<32>> axi_instr_data;
        
        // AXI 数据接口 (D-Cache 填充)
        ch_out<ch_uint<32>> axi_data_addr;
        ch_out<ch_uint<32>> axi_data_wdata;
        ch_out<ch_bool> axi_data_we;
        ch_out<ch_bool> axi_data_valid;
        ch_in<ch_bool> axi_data_ready;
        ch_in<ch_uint<32>> axi_data_rdata;
        
        // 外部接口
        ch_out<ch_bool> rst;
        ch_out<ch_bool> running;
    )

    Rv32iPipelineWithCache(ch::Component* parent = nullptr, 
                           const std::string& name = "rv32i_pipeline_cache")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // Cache 实例化
        // =====================================================================
        chlib::ICache icache{"icache"};
        chlib::DCache dcache{"dcache"};
        
        // =====================================================================
        // 流水线阶段实例化
        // =====================================================================
        IfStage  if_stage{"if_stage"};
        IdStage  id_stage{"id_stage"};
        ExStage  ex_stage{"ex_stage"};
        MemStage mem_stage{"mem_stage"};
        WbStage  wb_stage{"wb_stage"};
        
        // 冒险检测单元
        HazardUnit hazard{"hazard_unit"};
        
        // =====================================================================
        // IF 级连接 (从 I-Cache 取指)
        // =====================================================================
        // I-Cache 请求地址 = PC
        icache.io().addr <<= if_stage.io().instr_addr;
        icache.io().req <<= if_stage.io().valid;
        
        // IF 级从 I-Cache 读取指令
        if_stage.io().instr_data <<= icache.io().data;
        if_stage.io().instr_ready <<= icache.io().ready;
        
        // I-Cache AXI 接口连接
        icache.io().axi_addr <<= io().axi_instr_addr;
        icache.io().axi_valid <<= io().axi_instr_addr.valid;
        axi_instr_ready <<= icache.io().axi_ready;
        axi_instr_data <<= icache.io().axi_data;
        
        // =====================================================================
        // MEM 级连接 (通过 D-Cache 访存)
        // =====================================================================
        // D-Cache 请求地址 = MEM 级地址
        dcache.io().addr <<= mem_stage.io().alu_result;
        dcache.io().req <<= mem_stage.io().mem_valid;
        dcache.io().we <<= mem_stage.io().is_store;
        dcache.io().wdata <<= mem_stage.io().mem_data;
        
        // MEM 级从 D-Cache 读取数据
        mem_stage.io().mem_data_rdata <<= dcache.io().rdata;
        mem_stage.io().mem_ready <<= dcache.io().ready;
        
        // D-Cache AXI 接口连接
        dcache.io().axi_addr <<= io().axi_data_addr;
        dcache.io().axi_wdata <<= io().axi_data_wdata;
        dcache.io().axi_we <<= io().axi_data_we;
        dcache.io().axi_valid <<= io().axi_data_valid;
        axi_data_ready <<= dcache.io().axi_ready;
        axi_data_rdata <<= dcache.io().axi_rdata;
        
        // =====================================================================
        // 其他连接保持不变
        // =====================================================================
        // TODO: 完成 Hazard Unit 和 Forwarding 连接
    }
};

} // namespace riscv
