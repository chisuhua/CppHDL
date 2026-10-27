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
#include "cpu/pipeline/rv32i_pipeline_regs.h"
#include "cpu/pipeline/rv32i_forwarding.h"
#include "cpu/pipeline/rv32i_branch_predict.h"
#include "cpu/pipeline/hazard_unit.h"
#include "forwarding_dut_fixture.h"

#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace riscv;

// Helper to convert sdata_type to bool
inline bool to_bool(const sdata_type& val) { return val != 0; }

// ============================================================================
// 测试用例 1: 流水线寄存器**行为**（不是「构造没崩」）
// ============================================================================
// ⚠️ 2026-10-23（change `riscv-mini-test-triage-2026-10`，第四轮对抗审稿 BLOCKER-2）：
//    本用例此前 4 个 SECTION 各只有 `(void)reg;` —— **零断言**，是本 change
//    立论对象「账面覆盖」的同型物（且本 target 正是本 change 新注册进 CTest 的）。
//    现改为驱动 `ch_device<T>` 并断言可观测输出。
//    `update = select(!rst, select(!stall, !flush, 0), 0)`（rv32i_pipeline_regs.h:210）
//    ⇒ 锁存条件 = `rst==0 && stall==0 && flush==0`。
TEST_CASE("Pipeline Registers - latch / stall / flush behavior", "[pipeline][regs]") {
    SECTION("IfIdPipelineReg latches only when rst/stall/flush all clear") {
        ch::ch_device<IfIdPipelineReg> reg;
        ch::Simulator sim(reg.context());
        sim.set_input_value(reg.instance().io().rst, 1);
        sim.set_input_value(reg.instance().io().stall, 0);
        sim.set_input_value(reg.instance().io().flush, 0);
        sim.set_input_value(reg.instance().io().if_pc, 0x80000010);
        sim.set_input_value(reg.instance().io().if_instruction, 0x00000013);
        sim.set_input_value(reg.instance().io().if_instr_valid, 1);
        sim.tick();
        // rst=1 ⇒ 冻结在复位值
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(reg.instance().io().id_pc)) == 0u);

        sim.set_input_value(reg.instance().io().rst, 0);
        sim.tick();
        // 锁存条件满足 ⇒ 输出反映输入
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(reg.instance().io().id_pc)) == 0x80000010u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(reg.instance().io().id_instruction)) == 0x00000013u);
        REQUIRE(to_bool(sim.get_port_value(reg.instance().io().id_instr_valid)));

        // stall=1 且输入改变 ⇒ MUST 保持旧值（不锁存）
        sim.set_input_value(reg.instance().io().if_pc, 0xDEADBEEF);
        sim.set_input_value(reg.instance().io().stall, 1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(reg.instance().io().id_pc)) == 0x80000010u);

        // flush=1（stall=0）⇒ valid MUST 被冲刷
        sim.set_input_value(reg.instance().io().stall, 0);
        sim.set_input_value(reg.instance().io().flush, 1);
        sim.tick();
        REQUIRE(!to_bool(sim.get_port_value(reg.instance().io().id_instr_valid)));
    }

    SECTION("MemWbPipelineReg latches write-back fields") {
        ch::ch_device<MemWbPipelineReg> reg;
        ch::Simulator sim(reg.context());
        sim.set_input_value(reg.instance().io().rst, 0);
        sim.set_input_value(reg.instance().io().stall, 0);
        sim.set_input_value(reg.instance().io().flush, 0);
        sim.set_input_value(reg.instance().io().mem_read_data, 0x000000AA);
        sim.set_input_value(reg.instance().io().mem_alu_result, 0x000000BB);
        sim.set_input_value(reg.instance().io().mem_reg_write, 1);
        sim.set_input_value(reg.instance().io().mem_is_load, 1);
        sim.set_input_value(reg.instance().io().mem_rd, 7);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(reg.instance().io().wb_read_data)) == 0xAAu);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(reg.instance().io().wb_alu_result)) == 0xBBu);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(reg.instance().io().wb_rd)) == 7u);
        REQUIRE(to_bool(sim.get_port_value(reg.instance().io().wb_reg_write)));
        REQUIRE(to_bool(sim.get_port_value(reg.instance().io().wb_is_load)));
    }

    // ⚠️ 2026-10-23（第五轮对抗审稿 SHOULD_FIX）：`hazard_unit.h:134` 文档化的
    //    优先级「EX=1 > MEM=2 > REG=0」此前**无判别力** —— 上面两个用例从不让
    //    `ex_rd_addr` 与 `mem_rd_addr` **同时**命中同一个 `id_rs1`（一个置 0）。
    //    实测把优先级倒过来（MEM 盖过 EX）全部用例仍绿。本 SECTION 让两者同时命中。
    SECTION("HazardUnit: EX beats MEM when both match same rs1") {
        // ⚠️ 2026-10-24（ADR-046 D3 #14）：前推数据由 `chlib::ForwardingMux` 承担 ⇒
        //    DUT 改为组合（断言值不变）。
        ch::ch_device<riscv_test::ForwardingDut> unit;
        ch::Simulator sim(unit.context());

        sim.set_input_value(unit.instance().io().id_rs1_addr, 5);
        sim.set_input_value(unit.instance().io().id_rs2_addr, 0);
        // EX 与 MEM **同时**写 x5 ⇒ 必须选 EX（优先级 1），而非 MEM（2）
        sim.set_input_value(unit.instance().io().ex_rd_addr, 5);
        sim.set_input_value(unit.instance().io().ex_reg_write, 1);
        sim.set_input_value(unit.instance().io().mem_rd_addr, 5);
        sim.set_input_value(unit.instance().io().mem_reg_write, 1);
        sim.set_input_value(unit.instance().io().rs1_data_raw, 100);
        sim.set_input_value(unit.instance().io().rs2_data_raw, 200);
        sim.set_input_value(unit.instance().io().ex_alu_result, 0xE1);
        sim.set_input_value(unit.instance().io().mem_alu_result, 0xE2);
        sim.set_input_value(unit.instance().io().mem_is_load, 0);
        // ⚠️ **新增端口 MUST 显式驱动**（2026-10-27，ADR-047 D6）：
        //    前推判定带 `& !ex_is_load` 门控 ⇒ 未驱动 ⇒ 未定义值。
        sim.set_input_value(unit.instance().io().ex_is_load, 0);
        sim.set_input_value(unit.instance().io().ex_branch, 0);
        sim.set_input_value(unit.instance().io().ex_branch_taken, 0);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(sim.get_port_value(unit.instance().io().forward_a)) == 1u);
        // 前推的**数据**也 MUST 来自 EX（`hazard_unit.h:134` 的优先级）
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(unit.instance().io().rs1_data)) == 0xE1u);
    }
}

// ============================================================================
// 测试用例 2: HazardUnit 前推逻辑（匹配当前 API）
// ============================================================================
TEST_CASE("HazardUnit: Forwarding EX hazard", "[hazard][forwarding]") {
    // ⚠️ 2026-10-24（ADR-046 D3 #14）：前推数据由 `chlib::ForwardingMux` 承担 ⇒
    //    DUT 改为组合（断言值不变）。
    ch::ch_device<riscv_test::ForwardingDut> unit;
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
    
    // 原始数据
    sim.set_input_value(unit.instance().io().rs1_data_raw, 100);
    sim.set_input_value(unit.instance().io().rs2_data_raw, 200);
    sim.set_input_value(unit.instance().io().ex_alu_result, 42);
    sim.set_input_value(unit.instance().io().mem_alu_result, 0);
    
    // 非 load 非 branch
    sim.set_input_value(unit.instance().io().mem_is_load, 0);
    // ⚠️ **新增端口 MUST 显式驱动**（2026-10-27，ADR-047 D6）：
    //    前推判定带 `& !ex_is_load` 门控 ⇒ 未驱动 ⇒ 未定义值。
    sim.set_input_value(unit.instance().io().ex_is_load, 0);
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
    // ⚠️ **新增端口 MUST 显式驱动**（2026-10-27，ADR-047 D6）：
    //    前推判定带 `& !ex_is_load` 门控 ⇒ 未驱动 ⇒ 未定义值。
    sim.set_input_value(unit.instance().io().ex_is_load, 0);
    
    // WB 阶段无写操作
    
    sim.set_input_value(unit.instance().io().rs1_data_raw, 0);
    sim.set_input_value(unit.instance().io().rs2_data_raw, 0);
    
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
// 测试用例 4b: HazardUnit 的 EX 级 load MUST NOT 被前推（组件级真值表）
//
// ⚠️ ADR-047 D6 的**组件级**验收（2026-10-27）：
//    「前推判定漏 `& !ex_is_load`」这个变异在**固件级不可达**
//    （EX 级 load 命中前推的当拍，load-use 停顿必然同时生效 ⇒ 消费者不进 EX
//      ⇒ 误选只打进气泡 ⇒ 结果被丢弃）⇒ MUST 由**组件级真值表**验收
//    ⇒ 否则该变异会**假绿**。
//    ⇒ 本用例是该变异的**唯一**判别力来源。
// ============================================================================
TEST_CASE("HazardUnit: EX-level load MUST NOT be forwarded (level 1)",
          "[hazard][forwarding][load-use]") {
    ch::ch_device<HazardUnit> unit;
    ch::Simulator sim(unit.context());

    // ID 级消费者读 x5；EX 级那条指令写 x5（rd 匹配 ⇒ 若不门控就会命中档位 1）
    sim.set_input_value(unit.instance().io().id_rs1_addr, 5);
    sim.set_input_value(unit.instance().io().id_rs2_addr, 0);
    sim.set_input_value(unit.instance().io().ex_rd_addr, 5);
    sim.set_input_value(unit.instance().io().ex_reg_write, 1);
    sim.set_input_value(unit.instance().io().mem_rd_addr, 0);
    sim.set_input_value(unit.instance().io().mem_reg_write, 0);
    sim.set_input_value(unit.instance().io().mem_is_load, 0);
    sim.set_input_value(unit.instance().io().ex_branch, 0);
    sim.set_input_value(unit.instance().io().ex_branch_taken, 0);
    sim.set_input_value(unit.instance().io().rs1_data_raw, 0);
    sim.set_input_value(unit.instance().io().rs2_data_raw, 0);

    // ---- ① EX 级**不是** load ⇒ 档位 `1`（EX 级前推）MUST 成立 ----
    sim.set_input_value(unit.instance().io().ex_is_load, 0);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(unit.instance().io().forward_a)) == 1u);

    // ---- ② EX 级**是** load ⇒ 档位 `1` MUST 被**排除**（保守边界）----
    //    档位 `1` 的数据是 EX/MEM 级的 ALU 结果 ⇒ 对 load 而言那是**访存地址**
    //    ⇒ 前推它就是**前推错值**。
    sim.set_input_value(unit.instance().io().ex_is_load, 1);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(unit.instance().io().forward_a)) != 1u);
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
    
    sim.set_input_value(unit.instance().io().rs1_data_raw, 55);
    sim.set_input_value(unit.instance().io().rs2_data_raw, 66);
    
    sim.set_input_value(unit.instance().io().mem_is_load, 0);
    // ⚠️ **新增端口 MUST 显式驱动**（2026-10-27，ADR-047 D6）：
    //    前推判定带 `& !ex_is_load` 门控 ⇒ 未驱动 ⇒ 未定义值。
    sim.set_input_value(unit.instance().io().ex_is_load, 0);
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
