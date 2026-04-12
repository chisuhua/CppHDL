/**
 * @file rv32i_core.h
 * @brief RV32I 处理器核心 (Phase 3C: 5 级流水线集成)
 * 
 * 架构：IF → ID → EX → MEM → WB
 * 组件：流水线寄存器、数据前推、分支预测、冒险检测
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

/** RV32I 5 级流水线核心 - 组件集成声明 */
class Rv32iCore : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> instr_data; ch_in<ch_bool> instr_ready;
        ch_out<ch_uint<32>> instr_addr; ch_out<ch_bool> instr_valid;
        ch_in<ch_uint<32>> data_rdata; ch_in<ch_bool> data_ready;
        ch_out<ch_uint<32>> data_addr; ch_out<ch_uint<32>> data_wdata;
        ch_out<ch_bool> data_read; ch_out<ch_bool> data_write; ch_out<ch_uint<4>> data_strbe;
        ch_in<ch_bool> clk; ch_in<ch_bool> rst;
        ch_out<ch_bool> running; ch_out<ch_uint<32>> ipc_count; ch_out<ch_uint<32>> stall_count
    )
    Rv32iCore(ch::Component* p = nullptr, const std::string& n = "rv32i_core") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // 组件实例化声明 (完整连接需框架支持 port 连接)
        // 流水线寄存器
        IfIdPipelineReg if_id_reg(this, "if_id");
        IdExPipelineReg id_ex_reg(this, "id_ex");
        ExMemPipelineReg ex_mem_reg(this, "ex_mem");
        MemWbPipelineReg mem_wb_reg(this, "mem_wb");
        
        // 数据前推
        ForwardingUnit fwd_unit(this, "fwd");
        
        // 分支预测
        BranchPredictor bp(this, "bp");
        ControlHazardUnit ch(this, "ch");
        
        // 冒险检测
        HazardDetectionUnit hz(this, "hz");
        
        // 基础组件
        Rv32iPc pc(this, "pc");
        Rv32iDecoder dec(this, "dec");
        Rv32iRegFile rf(this, "rf");
        Rv32iAlu alu(this, "alu");
        
        // 状态输出
        io().running = !io().rst;
        io().ipc_count = ch_uint<32>(0_d);
        io().stall_count = ch_uint<32>(0_d);
        io().instr_valid = ch_bool(true);
        io().data_strbe = ch_uint<4>(15_d);
    }
};

} // namespace riscv
