/**
 * @file test_rv32i_pipeline.cpp
 * @brief RV32I 5 级流水线集成测试 (Phase 3C)
 * 
 * 测试范围:
 * 1. 流水线寄存器实例化 (IF/ID/EX/MEM/WB)
 * 2. HazardUnit 前推 + 冒险检测
 * 3. BranchDetector 分支检测
 * 
 * 作者：DevMate
 * 最后修改：2026-04-13
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../src/rv32i_pipeline_regs.h"
#include "../src/rv32i_forwarding.h"
#include "../src/rv32i_branch_predict.h"
#include "../src/hazard_unit.h"

#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace riscv;

// Helper to convert sdata_type to bool
inline bool to_bool(const sdata_type& val) { return val != 0; }

// ============================================================================
// 测试用例 1: 流水线寄存器实例化
// ============================================================================
TEST_CASE("Pipeline Registers - Instantiation", "[pipeline][regs]") {
    SECTION("IfIdPipelineReg compiles and instantiates") {
        ch::Component* parent = nullptr;
        IfIdPipelineReg reg(parent, "test_if_id");
        (void)reg;
    }
    
    SECTION("IdExPipelineReg compiles and instantiates") {
        ch::Component* parent = nullptr;
        IdExPipelineReg reg(parent, "test_id_ex");
        (void)reg;
    }
    
    SECTION("ExMemPipelineReg compiles and instantiates") {
        ch::Component* parent = nullptr;
        ExMemPipelineReg reg(parent, "test_ex_mem");
        (void)reg;
    }
    
    SECTION("MemWbPipelineReg compiles and instantiates") {
        ch::Component* parent = nullptr;
        MemWbPipelineReg reg(parent, "test_mem_wb");
        (void)reg;
    }
}

// ============================================================================
// 测试用例 2: HazardUnit 前推逻辑（匹配当前 API）
// ============================================================================
TEST_CASE("HazardUnit: Forwarding EX hazard", "[hazard][forwarding]") {
    ch::ch_device<HazardUnit> unit;
    ch::Simulator sim(unit.context());
    
    // ID 阶段读寄存器 x1
    sim.set_input_value(unit.instance().io().id_rs1_addr, 1);
    sim.set_input_value(unit.instance().io().id_rs2_addr, 2);
    
    // EX 阶段写寄存器 x1
    sim.set_input_value(unit.instance().io().ex_rd_addr, 1);
    sim.set_input_value(unit.instance().io().ex_reg_write, 1);
    
    // MEM/WB 阶段不冲突
    sim.set_input_value(unit.instance().io().mem_rd_addr, 0);
    sim.set_input_value(unit.instance().io().mem_reg_write, 0);
    sim.set_input_value(unit.instance().io().wb_rd_addr, 0);
    sim.set_input_value(unit.instance().io().wb_reg_write, 0);
    
    // 原始数据
    sim.set_input_value(unit.instance().io().rs1_data_raw, 100);
    sim.set_input_value(unit.instance().io().rs2_data_raw, 200);
    sim.set_input_value(unit.instance().io().ex_alu_result, 42);
    sim.set_input_value(unit.instance().io().mem_alu_result, 0);
    sim.set_input_value(unit.instance().io().wb_write_data, 0);
    
    // 非 load 非 branch
    sim.set_input_value(unit.instance().io().mem_is_load, 0);
    sim.set_input_value(unit.instance().io().ex_branch, 0);
    sim.set_input_value(unit.instance().io().ex_branch_taken, 0);
    
    sim.tick();
    
    auto forward_a = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().forward_a));
    auto rs1_data = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().rs1_data));
    auto stall = to_bool(sim.get_port_value(unit.instance().io().stall));
    
    REQUIRE(forward_a == 1);       // EX 前推 (优先级最高)
    REQUIRE(rs1_data == 42);       // 前推后的数据
    REQUIRE(stall == false);       // 无 load-use 冒险
}

// ============================================================================
// 测试用例 3: HazardUnit Load-Use 冒险检测
// ============================================================================
TEST_CASE("HazardUnit: LOAD-USE hazard detection", "[hazard][load-use]") {
    ch::ch_device<HazardUnit> unit;
    ch::Simulator sim(unit.context());
    
    sim.set_input_value(unit.instance().io().id_rs1_addr, 3);
    sim.set_input_value(unit.instance().io().id_rs2_addr, 0);
    
    // EX 阶段无写操作
    sim.set_input_value(unit.instance().io().ex_rd_addr, 0);
    sim.set_input_value(unit.instance().io().ex_reg_write, 0);
    
    // MEM 阶段是 load，写 x3
    sim.set_input_value(unit.instance().io().mem_rd_addr, 3);
    sim.set_input_value(unit.instance().io().mem_reg_write, 1);
    sim.set_input_value(unit.instance().io().mem_is_load, 1);
    
    // WB 阶段无写操作
    sim.set_input_value(unit.instance().io().wb_rd_addr, 0);
    sim.set_input_value(unit.instance().io().wb_reg_write, 0);
    
    sim.set_input_value(unit.instance().io().rs1_data_raw, 0);
    sim.set_input_value(unit.instance().io().rs2_data_raw, 0);
    sim.set_input_value(unit.instance().io().ex_alu_result, 0);
    sim.set_input_value(unit.instance().io().mem_alu_result, 0);
    sim.set_input_value(unit.instance().io().wb_write_data, 0);
    
    sim.set_input_value(unit.instance().io().ex_branch, 0);
    sim.set_input_value(unit.instance().io().ex_branch_taken, 0);
    
    sim.tick();
    
    auto stall = to_bool(sim.get_port_value(unit.instance().io().stall));
    auto forward_a = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().forward_a));
    
    REQUIRE(forward_a == 2);        // MEM 匹配（但数据未就绪）
    REQUIRE(stall == true);         // Load-use → stall 等待数据
}

// ============================================================================
// 测试用例 4: BranchDetector 简单检测
// ============================================================================
TEST_CASE("BranchDetector: BEQ detection", "[branch][detect]") {
    ch::ch_device<BranchDetector> detector;
    ch::Simulator sim(detector.context());
    
    // BEQ x0, x0, 0 → 0x00000063
    sim.set_input_value(detector.instance().io().instruction, 0x00000063);
    
    sim.tick();
    
    auto is_branch = to_bool(sim.get_port_value(detector.instance().io().is_branch));
    auto branch_type = static_cast<uint64_t>(sim.get_port_value(detector.instance().io().branch_type));
    
    REQUIRE(is_branch == true);
    REQUIRE(branch_type == 0);     // BEQ
}

// ============================================================================
// 测试用例 5: HazardUnit 无前推情况
// ============================================================================
TEST_CASE("HazardUnit: No hazard - use register file", "[hazard][no-hazard]") {
    ch::ch_device<HazardUnit> unit;
    ch::Simulator sim(unit.context());
    
    sim.set_input_value(unit.instance().io().id_rs1_addr, 5);
    sim.set_input_value(unit.instance().io().id_rs2_addr, 6);
    
    // 无写操作
    sim.set_input_value(unit.instance().io().ex_rd_addr, 0);
    sim.set_input_value(unit.instance().io().ex_reg_write, 0);
    sim.set_input_value(unit.instance().io().mem_rd_addr, 0);
    sim.set_input_value(unit.instance().io().mem_reg_write, 0);
    sim.set_input_value(unit.instance().io().wb_rd_addr, 0);
    sim.set_input_value(unit.instance().io().wb_reg_write, 0);
    
    sim.set_input_value(unit.instance().io().rs1_data_raw, 55);
    sim.set_input_value(unit.instance().io().rs2_data_raw, 66);
    sim.set_input_value(unit.instance().io().ex_alu_result, 0);
    sim.set_input_value(unit.instance().io().mem_alu_result, 0);
    sim.set_input_value(unit.instance().io().wb_write_data, 0);
    
    sim.set_input_value(unit.instance().io().mem_is_load, 0);
    sim.set_input_value(unit.instance().io().ex_branch, 0);
    sim.set_input_value(unit.instance().io().ex_branch_taken, 0);
    
    sim.tick();
    
    auto forward_a = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().forward_a));
    auto rs1_data = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().rs1_data));
    auto stall = to_bool(sim.get_port_value(unit.instance().io().stall));
    
    REQUIRE(forward_a == 0);       // 来自寄存器文件
    REQUIRE(rs1_data == 55);       // 原始寄存器数据
    REQUIRE(stall == false);       // 无冒险
}

// Tests use Catch2 main from catch_amalgamated.cpp
