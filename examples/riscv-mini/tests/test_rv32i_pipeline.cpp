/**
 * @file test_rv32i_pipeline.cpp
 * @brief RV32I 5 级流水线集成测试 (Phase 3C)
 * 
 * 测试范围:
 * 1. 流水线寄存器功能验证
 * 2. 数据前推单元验证
 * 3. 分支预测单元验证
 * 4. 冒险检测逻辑验证
 * 5. Stall/Flush 控制验证
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../src/rv32i_pipeline_regs.h"
#include "../src/rv32i_forwarding.h"
#include "../src/rv32i_branch_predict.h"
#include "../src/hazard_unit.h"
#include "cpu/forwarding.h"

#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace riscv;

// Helper to convert sdata_type to bool
inline bool to_bool(const sdata_type& val) { return val != 0; }

// ============================================================================
// 测试用例 1: 流水线寄存器基本功能
// ============================================================================
TEST_CASE("Pipeline Registers - Basic Functionality", "[pipeline][regs]") {
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
// 测试用例 2: 流水线寄存器数据传输
// ============================================================================
TEST_CASE("Pipeline Registers - Data Transfer", "[pipeline][transfer]") {
    SECTION("IfIdPipelineReg transfers PC and instruction") {
        ch::ch_device<IfIdPipelineReg> reg;
        ch::Simulator sim(reg.context());
        
        sim.set_input_value(reg.instance().io().if_pc, 0x00000100);
        sim.set_input_value(reg.instance().io().if_instruction, 0x00100093);
        sim.set_input_value(reg.instance().io().if_instr_valid, true);
        sim.set_input_value(reg.instance().io().rst, false);
        sim.set_input_value(reg.instance().io().stall, false);
        sim.set_input_value(reg.instance().io().flush, false);
        
        sim.set_input_value(reg.instance().io().clk, true);
        sim.tick();
        sim.set_input_value(reg.instance().io().clk, false);
        sim.tick();
        
        auto out_pc = static_cast<uint64_t>(sim.get_port_value(reg.instance().io().id_pc));
        auto out_instr = static_cast<uint64_t>(sim.get_port_value(reg.instance().io().id_instruction));
        
        REQUIRE(out_pc == 0x00000100);
        REQUIRE(out_instr == 0x00100093);
    }
}

// ============================================================================
// 测试用例 3: 流水线寄存器 Flush 控制
// ============================================================================
TEST_CASE("Pipeline Registers - Flush Control", "[pipeline][flush]") {
    SECTION("IfIdPipelineReg clears instr_valid on flush") {
        ch::ch_device<IfIdPipelineReg> reg;
        ch::Simulator sim(reg.context());
        
        // 第 1 周期：写入初始值
        sim.set_input_value(reg.instance().io().if_pc, 0x100);
        sim.set_input_value(reg.instance().io().if_instruction, 0x00100093);
        sim.set_input_value(reg.instance().io().if_instr_valid, true);
        sim.set_input_value(reg.instance().io().rst, false);
        sim.set_input_value(reg.instance().io().stall, false);
        sim.set_input_value(reg.instance().io().flush, false);
        sim.set_input_value(reg.instance().io().clk, true);
        sim.tick();
        sim.set_input_value(reg.instance().io().clk, false);
        sim.tick();
        
        // Flush 激活
        sim.set_input_value(reg.instance().io().flush, true);
        sim.set_input_value(reg.instance().io().clk, true);
        sim.tick();
        sim.set_input_value(reg.instance().io().clk, false);
        sim.tick();
        
        auto out_valid = to_bool(sim.get_port_value(reg.instance().io().id_instr_valid));
        REQUIRE(out_valid == false);
    }
}

// ============================================================================
// 测试用例 4: 前推单元基本功能
// ============================================================================
TEST_CASE("Forwarding Unit - Basic Functionality", "[forwarding][unit]") {
    SECTION("ForwardingUnit compiles and instantiates") {
        ch::Component* parent = nullptr;
        ForwardingUnit unit(parent, "test_forward");
        (void)unit;
    }
    
    SECTION("ForwardingUnit detects EX/MEM hazard") {
        ch::ch_device<ForwardingUnit> unit;
        ch::Simulator sim(unit.context());
        
        sim.set_input_value(unit.instance().io().id_ex_rs1_addr, 1);
        sim.set_input_value(unit.instance().io().id_ex_rs2_addr, 2);
        sim.set_input_value(unit.instance().io().ex_mem_rd, 1);
        sim.set_input_value(unit.instance().io().ex_mem_reg_write, 1);
        sim.set_input_value(unit.instance().io().mem_wb_rd, 0);
        sim.set_input_value(unit.instance().io().mem_wb_reg_write, 0);
        
        sim.tick();  // 评估组合逻辑
        
        auto forward_a = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().forward_a));
        REQUIRE(forward_a == 1);  // EX/MEM 前推
    }
    
    SECTION("ForwardingUnit detects MEM/WB hazard") {
        ch::ch_device<ForwardingUnit> unit;
        ch::Simulator sim(unit.context());
        
        sim.set_input_value(unit.instance().io().id_ex_rs1_addr, 2);
        sim.set_input_value(unit.instance().io().id_ex_rs2_addr, 3);
        sim.set_input_value(unit.instance().io().ex_mem_rd, 5);
        sim.set_input_value(unit.instance().io().ex_mem_reg_write, 0);
        sim.set_input_value(unit.instance().io().mem_wb_rd, 2);
        sim.set_input_value(unit.instance().io().mem_wb_reg_write, 1);
        
        sim.tick();  // 评估组合逻辑
        
        auto forward_a = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().forward_a));
        REQUIRE(forward_a == 2);  // MEM/WB 前推
    }
    
    SECTION("ForwardingUnit no hazard - use register file") {
        ch::ch_device<ForwardingUnit> unit;
        ch::Simulator sim(unit.context());
        
        sim.set_input_value(unit.instance().io().id_ex_rs1_addr, 1);
        sim.set_input_value(unit.instance().io().id_ex_rs2_addr, 2);
        sim.set_input_value(unit.instance().io().ex_mem_rd, 3);
        sim.set_input_value(unit.instance().io().ex_mem_reg_write, 0);
        sim.set_input_value(unit.instance().io().mem_wb_rd, 4);
        sim.set_input_value(unit.instance().io().mem_wb_reg_write, 0);
        
        sim.tick();  // 评估组合逻辑
        
        auto forward_a = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().forward_a));
        REQUIRE(forward_a == 0);  // 寄存器文件
    }
}

// ============================================================================
// 测试用例 5: 前推数据选择器
// ============================================================================
TEST_CASE("Forwarding Mux - Data Selection", "[forwarding][mux]") {
    SECTION("ForwardingMux compiles and instantiates") {
        ch::Component* parent = nullptr;
        chlib::ForwardingMux<32> mux(parent, "test_mux");
        (void)mux;
    }
    
    SECTION("ForwardingMux selects EX/MEM result") {
        ch::ch_device<chlib::ForwardingMux<32>> mux;
        ch::Simulator sim(mux.context());
        
        sim.set_input_value(mux.instance().io().forward_a, 1);
        sim.set_input_value(mux.instance().io().forward_b, 0);
        sim.set_input_value(mux.instance().io().rs1_data_reg, 100);
        sim.set_input_value(mux.instance().io().rs2_data_reg, 200);
        sim.set_input_value(mux.instance().io().ex_mem_alu_result, 42);
        sim.set_input_value(mux.instance().io().mem_wb_alu_result, 99);
        sim.set_input_value(mux.instance().io().mem_wb_mem_read_data, 88);
        sim.set_input_value(mux.instance().io().mem_wb_is_load, false);
        
        sim.tick();  // 评估组合逻辑
        
        auto alu_a = static_cast<uint64_t>(sim.get_port_value(mux.instance().io().alu_input_a));
        REQUIRE(alu_a == 42);  // EX/MEM 结果
    }
}

// ============================================================================
// 测试用例 6: 分支检测单元
// ============================================================================
TEST_CASE("Branch Detector - Instruction Detection", "[branch][detect]") {
    SECTION("BranchDetector detects BEQ") {
        ch::ch_device<BranchDetector> detector;
        ch::Simulator sim(detector.context());
        
        sim.set_input_value(detector.instance().io().instruction, 0x00000063);
        
        sim.tick();
        
        auto is_branch = to_bool(sim.get_port_value(detector.instance().io().is_branch));
        auto branch_type = static_cast<uint64_t>(sim.get_port_value(detector.instance().io().branch_type));
        
        REQUIRE(is_branch == true);
        REQUIRE(branch_type == 0);
    }
    
    SECTION("BranchDetector detects BNE") {
        ch::ch_device<BranchDetector> detector;
        ch::Simulator sim(detector.context());
        
        sim.set_input_value(detector.instance().io().instruction, 0x00001063);
        
        sim.tick();
        
        auto is_branch = to_bool(sim.get_port_value(detector.instance().io().is_branch));
        auto branch_type = static_cast<uint64_t>(sim.get_port_value(detector.instance().io().branch_type));
        
        REQUIRE(is_branch == true);
        REQUIRE(branch_type == 1);
    }
    
    SECTION("BranchDetector non-branch instruction") {
        ch::ch_device<BranchDetector> detector;
        ch::Simulator sim(detector.context());
        
        sim.set_input_value(detector.instance().io().instruction, 0x00000013);
        
        sim.tick();
        
        auto is_branch = to_bool(sim.get_port_value(detector.instance().io().is_branch));
        REQUIRE(is_branch == false);
    }
}

// ============================================================================
// 测试用例 7: 分支条件计算
// ============================================================================
TEST_CASE("Branch Condition - Calculation", "[branch][condition]") {
    SECTION("BranchConditionUnit BEQ equal") {
        ch::ch_device<BranchConditionUnit> unit;
        ch::Simulator sim(unit.context());
        
        sim.set_input_value(unit.instance().io().branch_type, 0);
        sim.set_input_value(unit.instance().io().rs1_data, 42);
        sim.set_input_value(unit.instance().io().rs2_data, 42);
        
        sim.tick();
        
        auto condition = to_bool(sim.get_port_value(unit.instance().io().branch_condition));
        REQUIRE(condition == true);
    }
    
    SECTION("BranchConditionUnit BEQ not equal") {
        ch::ch_device<BranchConditionUnit> unit;
        ch::Simulator sim(unit.context());
        
        sim.set_input_value(unit.instance().io().branch_type, 0);
        sim.set_input_value(unit.instance().io().rs1_data, 42);
        sim.set_input_value(unit.instance().io().rs2_data, 43);
        
        sim.tick();
        
        auto condition = to_bool(sim.get_port_value(unit.instance().io().branch_condition));
        REQUIRE(condition == false);
    }
    
    SECTION("BranchConditionUnit BLT signed less") {
        ch::ch_device<BranchConditionUnit> unit;
        ch::Simulator sim(unit.context());
        
        // branch_type: 100 = 4 (BLT)
        sim.set_input_value(unit.instance().io().branch_type, 4);
        sim.set_input_value(unit.instance().io().rs1_data, 10);
        sim.set_input_value(unit.instance().io().rs2_data, 20);
        
        sim.tick();
        
        auto condition_val = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().branch_condition));
        auto branch_type_val = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().branch_type));
        std::cout << "DEBUG BLT: branch_type=" << branch_type_val << ", rs1=10, rs2=20, condition=" << condition_val << std::endl;
        
        // 10 < 20 (signed) should be true
        REQUIRE(condition_val == 1);
    }
    
    SECTION("BranchConditionUnit BLTU unsigned less") {
        ch::ch_device<BranchConditionUnit> unit;
        ch::Simulator sim(unit.context());
        
        // branch_type: 110 = 6 (BLTU)
        sim.set_input_value(unit.instance().io().branch_type, 6);
        sim.set_input_value(unit.instance().io().rs1_data, 10);
        sim.set_input_value(unit.instance().io().rs2_data, 20);
        
        sim.tick();
        
        auto condition_val = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().branch_condition));
        std::cout << "DEBUG BLTU: branch_type=6, rs1=10, rs2=20, condition=" << condition_val << std::endl;
        
        // 10 < 20 (unsigned) should be true
        REQUIRE(condition_val == 1);
    }
}

// ============================================================================
// 测试用例 8: 分支预测器集成
// ============================================================================
TEST_CASE("Branch Predictor - Integration", "[branch][predictor]") {
    SECTION("BranchPredictor compiles and instantiates") {
        ch::Component* parent = nullptr;
        BranchPredictor predictor(parent, "test_predictor");
        (void)predictor;
    }
    
    SECTION("BranchPredictor BNT strategy") {
        ch::ch_device<BranchPredictor> predictor;
        ch::Simulator sim(predictor.context());
        
        sim.set_input_value(predictor.instance().io().instruction, 0x00000063);
        
        sim.tick();
        sim.set_input_value(predictor.instance().io().rs1_data, 0);
        sim.set_input_value(predictor.instance().io().rs2_data, 0);
        sim.set_input_value(predictor.instance().io().pc, 0x100);
        
        auto is_branch = to_bool(sim.get_port_value(predictor.instance().io().is_branch));
        auto branch_taken = to_bool(sim.get_port_value(predictor.instance().io().branch_taken));
        auto predict_error = to_bool(sim.get_port_value(predictor.instance().io().predict_error));
        
        REQUIRE(is_branch == true);
        REQUIRE(branch_taken == true);
        REQUIRE(predict_error == true);
    }
    
    SECTION("ControlHazardUnit generates flush") {
        ch::ch_device<ControlHazardUnit> hazard;
        ch::Simulator sim(hazard.context());
        
        sim.set_input_value(hazard.instance().io().is_branch, true);
        sim.set_input_value(hazard.instance().io().predict_error, true);
        sim.set_input_value(hazard.instance().io().branch_taken, true);
        sim.set_input_value(hazard.instance().io().pc, 0x100);
        sim.set_input_value(hazard.instance().io().instr_valid, true);
        sim.set_input_value(hazard.instance().io().branch_target, 0x200);
        
        sim.tick();
        
        auto flush = to_bool(sim.get_port_value(hazard.instance().io().flush_if_id));
        REQUIRE(flush == true);
    }
}

// ============================================================================
// 测试用例 9: 冒险检测单元
// ============================================================================
TEST_CASE("Hazard Detection Unit - LOAD-USE", "[hazard][load-use]") {
    SECTION("HazardDetectionUnit detects LOAD-USE hazard") {
        ch::ch_device<HazardDetectionUnit> unit;
        ch::Simulator sim(unit.context());
        
        sim.set_input_value(unit.instance().io().id_ex_rs1_addr, 1);
        sim.set_input_value(unit.instance().io().id_ex_rs2_addr, 2);
        sim.set_input_value(unit.instance().io().id_ex_reg_write, 1);
        sim.set_input_value(unit.instance().io().ex_mem_is_load, 1);
        sim.set_input_value(unit.instance().io().ex_mem_rd, 1);
        sim.set_input_value(unit.instance().io().ex_mem_reg_write, 1);
        
        sim.tick();
        
        auto stall_if = to_bool(sim.get_port_value(unit.instance().io().stall_if));
        auto stall_id = to_bool(sim.get_port_value(unit.instance().io().stall_id));
        auto flush_id_ex = to_bool(sim.get_port_value(unit.instance().io().flush_id_ex));
        
        REQUIRE(stall_if == true);
        REQUIRE(stall_id == true);
        REQUIRE(flush_id_ex == true);
    }
    
    SECTION("HazardDetectionUnit no hazard") {
        ch::ch_device<HazardDetectionUnit> unit;
        ch::Simulator sim(unit.context());
        
        sim.set_input_value(unit.instance().io().id_ex_rs1_addr, 1);
        sim.set_input_value(unit.instance().io().id_ex_rs2_addr, 2);
        sim.set_input_value(unit.instance().io().id_ex_reg_write, true);
        sim.set_input_value(unit.instance().io().ex_mem_is_load, false);
        sim.set_input_value(unit.instance().io().ex_mem_rd, 1);
        sim.set_input_value(unit.instance().io().ex_mem_reg_write, true);
        
        auto stall_if = to_bool(sim.get_port_value(unit.instance().io().stall_if));
        REQUIRE(stall_if == false);
    }
}

// Tests use Catch2 main from catch_amalgamated.cpp
