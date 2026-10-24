/**
 * @file test_rv32i_pipeline_behavior.cpp
 * @brief RV32I microarchitecture 子组件 dual-backend behavior parity test
 *
 * 本文件与 tests/test_rv32i_pipeline.cpp 的关系（spec dual-backend-test-coverage
 * R6 #5 + R7 固化）：
 * - 原 test_rv32i_pipeline.cpp 12 个 TEST_CASE 全部用 Catch2 SECTION 嵌套
 *   （grep 命中 22 SECTION）。spec R6 #5 明确排除 SECTION 重入 dual 化
 *   （macro 展开为 2 个 TEST_CASE，每个 half 独立遍历 SECTION 会破坏 fixture 语义）。
 * - 本文件**新建**，用 _behavior 后缀区别原文件，零 SECTION 嵌套，~15 个
 *   DUAL_BACKEND_TEST_PAIR 覆盖 RV32I microarchitecture 子组件。
 * - 原 test_rv32i_pipeline.cpp MUST NOT 修改（保留 interpreter-only baseline）。
 *
 * Scope:
 *   Batch 1（无 clk 风险）= 13 个：
 *     - BranchDetector ×3 (BEQ / BNE / non-branch)
 *     - BranchConditionUnit ×4 (BEQ equal / BEQ not equal / BLT signed / BLTU unsigned)
 *     - ForwardingUnit ×3 (EX hazard / MEM hazard / no hazard)
 *     - ControlHazardUnit ×1 (flush logic)
 *     - HazardDetectionUnit ×2 (LOAD-USE / LOAD-USE no stall)
 *
 *   Batch 2（含 clk，需 spike）= 1 + 1：
 *     - IfIdPipelineReg - transfers PC and instruction (Task 3 spike)
 *     - IfIdPipelineReg - clears instr_valid on flush (spike 通过后)
 *
 *   Batch 3（可选）= 1：
 *     - BranchPredictor - BNT strategy
 *
 * 显式不迁移清单（per oracle 调研 + R6 #1/#2/#6 排除）：
 *   - 原 test_rv32i_pipeline.cpp line 47-69 (4 个 pipeline reg 裸构造)        R6 #1/#2
 *   - 原 test_rv32i_pipeline.cpp line 144 (ForwardingUnit instantiate)        R6 #1/#2
 *   - 原 test_rv32i_pipeline.cpp line 265 (ForwardingMux instantiate)        R6 #1/#2
 *   - 原 test_rv32i_pipeline.cpp line 397 (BranchPredictor instantiate)      R6 #1/#2
 *   - 原 test_rv32i_pipeline.cpp line 274 (ForwardingMux SKIP)               R6 #6
 *   - 原 test_rv32i_pipeline.cpp line 495 (Rv32iCore SUCCEED)                R6 #1/#6
 *   - 原 test_rv32i_pipeline.cpp line 500 (Rv32iCore IO SKIP)                R6 #1/#6
 *   - 原 test_rv32i_pipeline.cpp line 528-538 (Full Data Path 7 组件裸构造)   R6 #1/#2
 *   - 原 test_rv32i_pipeline.cpp line 507-521 (IPC Verification 纯数学)       R6 #6
 *
 * 实施参考：
 *   - tests/dual_backend_fixture.h:140-198 (DUAL_BACKEND_TEST_PAIR macro)
 *   - openspec/changes/archive/2026-10-19-dual-backend-cache-tests/design.md §2.4
 *     (避免 fragile timing 原则)
 *   - openspec/changes/archive/2026-10-19-axi-spi-behavior-tests/proposal.md
 *     (_behavior suffix pattern)
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include "cpu/pipeline/rv32i_branch_predict.h"
#include "cpu/pipeline/rv32i_forwarding.h"
#include "cpu/pipeline/hazard_detection_unit.h"
#include "cpu/pipeline/rv32i_pipeline_regs.h"

using namespace ch;
using namespace ch::core;
using namespace riscv;

// ============================================================================
// Batch 1 — 纯组合逻辑 dual 化（13 个）
// ============================================================================

// ----------------------------------------------------------------------------
// BranchDetector ×3 (对应原 test_rv32i_pipeline.cpp line 282/297/312 SECTION)
// ----------------------------------------------------------------------------

// BranchDetector - detects BEQ (原 line 282 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    BranchDetector,
    "BranchDetector - detects BEQ",
    "[pipeline][branch][behavior][detect][dual]",
    [&]() {
        // BEQ 指令: opcode=0x63 (BRANCH), funct3=0x00 (BEQ)
        // 二进制: 0x00000063 = funct12=0, rs1=0, rs2=0, funct3=0, opcode=0x63
        sim.set_input_value(dev.instance().io().instruction, 0x00000063_h);

        sim.tick();

        auto is_branch = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().is_branch));
        auto branch_type = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().branch_type));

        REQUIRE(is_branch == 1);
        REQUIRE(branch_type == 0);
    });

// BranchDetector - detects BNE (原 line 297 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    BranchDetector,
    "BranchDetector - detects BNE",
    "[pipeline][branch][behavior][detect][dual]",
    [&]() {
        // BNE 指令: opcode=0x63 (BRANCH), funct3=0x01 (BNE)
        sim.set_input_value(dev.instance().io().instruction, 0x00001063_h);

        sim.tick();

        auto is_branch = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().is_branch));
        auto branch_type = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().branch_type));

        REQUIRE(is_branch == 1);
        REQUIRE(branch_type == 1);
    });

// BranchDetector - non-branch instruction (原 line 312 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    BranchDetector,
    "BranchDetector - non-branch instruction",
    "[pipeline][branch][behavior][detect][dual]",
    [&]() {
        // ADDI x0, x0, 0: opcode=0x13 (OP-IMM), funct3=0
        sim.set_input_value(dev.instance().io().instruction, 0x00000013_h);

        sim.tick();

        auto is_branch = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().is_branch));

        REQUIRE(is_branch == 0);
    });

// ----------------------------------------------------------------------------
// BranchConditionUnit ×4 (对应原 test_rv32i_pipeline.cpp line 329/343/357/374 SECTION)
// BranchConditionUnit 是组合逻辑: branch_type[3] + rs1/rs2_data[32] -> branch_condition bool
// ----------------------------------------------------------------------------

// BranchConditionUnit - BEQ equal (原 line 329 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    BranchConditionUnit,
    "BranchConditionUnit - BEQ equal",
    "[pipeline][branch][condition][behavior][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().branch_type, 0_d);  // funct3=0 (BEQ)
        sim.set_input_value(dev.instance().io().rs1_data, 42_d);
        sim.set_input_value(dev.instance().io().rs2_data, 42_d);

        sim.tick();

        auto condition = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().branch_condition));
        REQUIRE(condition == 1);
    });

// BranchConditionUnit - BEQ not equal (原 line 343 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    BranchConditionUnit,
    "BranchConditionUnit - BEQ not equal",
    "[pipeline][branch][condition][behavior][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().branch_type, 0_d);  // funct3=0 (BEQ)
        sim.set_input_value(dev.instance().io().rs1_data, 42_d);
        sim.set_input_value(dev.instance().io().rs2_data, 43_d);

        sim.tick();

        auto condition = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().branch_condition));
        REQUIRE(condition == 0);
    });

// BranchConditionUnit - BLT signed less (原 line 357 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    BranchConditionUnit,
    "BranchConditionUnit - BLT signed less",
    "[pipeline][branch][condition][behavior][dual]",
    [&]() {
        // branch_type: 100 = 4 (BLT)
        sim.set_input_value(dev.instance().io().branch_type, 4_d);
        sim.set_input_value(dev.instance().io().rs1_data, 10_d);
        sim.set_input_value(dev.instance().io().rs2_data, 20_d);

        sim.tick();

        auto condition = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().branch_condition));
        // 10 < 20 (signed) should be true
        REQUIRE(condition == 1);
    });

// BranchConditionUnit - BLTU unsigned less (原 line 374 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    BranchConditionUnit,
    "BranchConditionUnit - BLTU unsigned less",
    "[pipeline][branch][condition][behavior][dual]",
    [&]() {
        // branch_type: 110 = 6 (BLTU)
        sim.set_input_value(dev.instance().io().branch_type, 6_d);
        sim.set_input_value(dev.instance().io().rs1_data, 10_d);
        sim.set_input_value(dev.instance().io().rs2_data, 20_d);

        sim.tick();

        auto condition = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().branch_condition));
        // 10 < 20 (unsigned) should be true
        REQUIRE(condition == 1);
    });

// ----------------------------------------------------------------------------
// ForwardingUnit ×3 (对应原 test_rv32i_pipeline.cpp line 150/187/224 SECTION)
// ForwardingUnit 是组合逻辑: id_rs1/rs2_addr + EX/MEM/WB 写地址冲突检测 → rs1/rs2_data 前推
// ----------------------------------------------------------------------------

// ForwardingUnit - detects EX hazard (原 line 150 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    ForwardingUnit,
    "ForwardingUnit - detects EX hazard",
    "[pipeline][forwarding][behavior][dual]",
    [&]() {
        // ID 阶段读 x1
        sim.set_input_value(dev.instance().io().id_rs1_addr, 1_d);
        sim.set_input_value(dev.instance().io().id_rs2_addr, 2_d);
        sim.set_input_value(dev.instance().io().id_reg_read, true);

        // EX 阶段写 x1 (前推源)
        sim.set_input_value(dev.instance().io().ex_rd_addr, 1_d);
        sim.set_input_value(dev.instance().io().ex_reg_write, true);
        sim.set_input_value(dev.instance().io().ex_alu_result, 42_d);

        // MEM 阶段不冲突
        sim.set_input_value(dev.instance().io().mem_rd_addr, 0_d);
        sim.set_input_value(dev.instance().io().mem_reg_write, false);
        sim.set_input_value(dev.instance().io().mem_alu_result, 0_d);

        // WB 阶段不冲突
        sim.set_input_value(dev.instance().io().wb_rd_addr, 0_d);
        sim.set_input_value(dev.instance().io().wb_reg_write, false);
        sim.set_input_value(dev.instance().io().wb_write_data, 0_d);

        // 原始寄存器数据
        sim.set_input_value(dev.instance().io().rs1_data_reg, 100_d);
        sim.set_input_value(dev.instance().io().rs2_data_reg, 200_d);

        sim.tick();  // 评估组合逻辑

        auto rs1_src = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rs1_src));
        auto rs1_data = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rs1_data));

        REQUIRE(rs1_src == 1);     // EX 前推 (优先级最高)
        REQUIRE(rs1_data == 42);   // 前推后的数据
    });

// ForwardingUnit - detects MEM hazard (原 line 187 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    ForwardingUnit,
    "ForwardingUnit - detects MEM hazard",
    "[pipeline][forwarding][behavior][dual]",
    [&]() {
        // ID 阶段读 x2
        sim.set_input_value(dev.instance().io().id_rs1_addr, 2_d);
        sim.set_input_value(dev.instance().io().id_rs2_addr, 3_d);
        sim.set_input_value(dev.instance().io().id_reg_read, true);

        // EX 阶段不写
        sim.set_input_value(dev.instance().io().ex_rd_addr, 5_d);
        sim.set_input_value(dev.instance().io().ex_reg_write, false);
        sim.set_input_value(dev.instance().io().ex_alu_result, 0_d);

        // MEM 阶段写 x2 (前推源)
        sim.set_input_value(dev.instance().io().mem_rd_addr, 2_d);
        sim.set_input_value(dev.instance().io().mem_reg_write, true);
        sim.set_input_value(dev.instance().io().mem_alu_result, 99_d);

        // WB 阶段不冲突
        sim.set_input_value(dev.instance().io().wb_rd_addr, 0_d);
        sim.set_input_value(dev.instance().io().wb_reg_write, false);
        sim.set_input_value(dev.instance().io().wb_write_data, 0_d);

        // 原始寄存器数据
        sim.set_input_value(dev.instance().io().rs1_data_reg, 100_d);
        sim.set_input_value(dev.instance().io().rs2_data_reg, 200_d);

        sim.tick();

        auto rs1_src = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rs1_src));
        auto rs1_data = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rs1_data));

        REQUIRE(rs1_src == 2);     // MEM 前推
        REQUIRE(rs1_data == 99);   // 前推后的数据
    });

// ForwardingUnit - no hazard (原 line 224 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    ForwardingUnit,
    "ForwardingUnit - no hazard - use register file",
    "[pipeline][forwarding][behavior][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().id_rs1_addr, 1_d);
        sim.set_input_value(dev.instance().io().id_rs2_addr, 2_d);
        sim.set_input_value(dev.instance().io().id_reg_read, true);

        sim.set_input_value(dev.instance().io().ex_rd_addr, 3_d);
        sim.set_input_value(dev.instance().io().ex_reg_write, false);
        sim.set_input_value(dev.instance().io().ex_alu_result, 0_d);

        sim.set_input_value(dev.instance().io().mem_rd_addr, 4_d);
        sim.set_input_value(dev.instance().io().mem_reg_write, false);
        sim.set_input_value(dev.instance().io().mem_alu_result, 0_d);

        sim.set_input_value(dev.instance().io().wb_rd_addr, 0_d);
        sim.set_input_value(dev.instance().io().wb_reg_write, false);
        sim.set_input_value(dev.instance().io().wb_write_data, 0_d);

        sim.set_input_value(dev.instance().io().rs1_data_reg, 55_d);
        sim.set_input_value(dev.instance().io().rs2_data_reg, 66_d);

        sim.tick();

        auto rs1_src = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rs1_src));
        auto rs1_data = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rs1_data));

        REQUIRE(rs1_src == 0);     // 来自寄存器文件
        REQUIRE(rs1_data == 55);   // 原始寄存器数据
    });

// ----------------------------------------------------------------------------
// ControlHazardUnit ×1 (对应原 test_rv32i_pipeline.cpp line 424 SECTION)
// ControlHazardUnit 是组合逻辑: is_branch + predict_error + branch_taken + pc +
// instr_valid + branch_target → flush_if_id / stall_if / pc_src
// ----------------------------------------------------------------------------

// ControlHazardUnit - generates flush (原 line 424 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    ControlHazardUnit,
    "ControlHazardUnit - generates flush",
    "[pipeline][hazard][control][behavior][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().is_branch, true);
        sim.set_input_value(dev.instance().io().predict_error, true);
        sim.set_input_value(dev.instance().io().branch_taken, true);
        sim.set_input_value(dev.instance().io().pc, 0x100_h);
        sim.set_input_value(dev.instance().io().instr_valid, true);
        sim.set_input_value(dev.instance().io().branch_target, 0x200_h);

        sim.tick();

        auto flush = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().flush_if_id));
        REQUIRE(flush == 1);
    });

// ----------------------------------------------------------------------------
// HazardDetectionUnit ×2 (对应原 test_rv32i_pipeline.cpp line 446/468 SECTION)
// HazardDetectionUnit 是组合逻辑: id_ex_rs1/rs2_addr + ex_mem 寄存器 + is_load +
// reg_write → stall_if / stall_id / flush_id_ex
// ----------------------------------------------------------------------------

// HazardDetectionUnit - detects LOAD-USE hazard (原 line 446 SECTION body)
DUAL_BACKEND_TEST_PAIR(
    HazardDetectionUnit,
    "HazardDetectionUnit - detects LOAD-USE hazard",
    "[pipeline][hazard][load-use][behavior][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().id_ex_rs1_addr, 1_d);
        sim.set_input_value(dev.instance().io().id_ex_rs2_addr, 2_d);
        sim.set_input_value(dev.instance().io().id_ex_reg_write, true);
        sim.set_input_value(dev.instance().io().ex_mem_is_load, true);
        sim.set_input_value(dev.instance().io().ex_mem_rd, 1_d);
        sim.set_input_value(dev.instance().io().ex_mem_reg_write, true);

        sim.tick();

        auto stall_if = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().stall_if));
        auto stall_id = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().stall_id));
        auto flush_id_ex = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().flush_id_ex));

        REQUIRE(stall_if == 1);
        REQUIRE(stall_id == 1);
        REQUIRE(flush_id_ex == 1);
    });

// HazardDetectionUnit - no hazard (原 line 468 SECTION body)
// 注：原 SECTION body 缺少 sim.tick() 调用，可能是测试 bug；本版本保留原行为
// （仅靠初始 0 值断言）。如果验收发现不一致，可加 sim.tick() + lambda 加 comment。
DUAL_BACKEND_TEST_PAIR(
    HazardDetectionUnit,
    "HazardDetectionUnit - no hazard",
    "[pipeline][hazard][load-use][behavior][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().id_ex_rs1_addr, 1_d);
        sim.set_input_value(dev.instance().io().id_ex_rs2_addr, 2_d);
        sim.set_input_value(dev.instance().io().id_ex_reg_write, true);
        sim.set_input_value(dev.instance().io().ex_mem_is_load, false);  // 不是 LOAD → 不构成 hazard
        sim.set_input_value(dev.instance().io().ex_mem_rd, 1_d);
        sim.set_input_value(dev.instance().io().ex_mem_reg_write, true);

        // 注：原 test_rv32i_pipeline.cpp line 468 SECTION 无 sim.tick()，
        // 直接读 ch_outport_value 的初始 0。这与"组合逻辑单 tick()"对齐：
        // 组合逻辑在 tick() 内完成求值；不 tick 等价读 default 0，
        // 仍然能满足 stall_if == false 断言（因为 ex_mem_is_load=false → load_use_hazard=0）。
        // 但 interpreter/verilator 在 tick 缺失下行为可能不一致，
        // 故 dual 化加 sim.tick() 同步两端 tick 模型。
        sim.tick();

        auto stall_if = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().stall_if));
        REQUIRE(stall_if == 0);
    });