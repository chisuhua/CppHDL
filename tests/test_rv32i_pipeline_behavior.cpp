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
 *   Batch 2（含 ch_reg 的时序组件）= 1 + 1 —— ✅ 2026-10-27 实施
 *     （change dual-backend-rv32i-integration-2026-10）：
 *     - IfIdPipelineReg - transfers PC and instruction
 *     - IfIdPipelineReg - clears instr_valid on flush
 *     ⚠️ 原计划里的「Task 3 clk spike」**不需要**：读码确认 `IfIdPipelineReg` 的 `clk`
 *       是**死端口**（`rv32i_pipeline_regs.h:190` 声明了它，`describe()` 从不读它）
 *       ⇒ 寄存器推进靠隐式时钟 ⇒ 该能力已由 `tests/test_dual_backend_clock.cpp`
 *       证明 ⇒ 真实风险 = **输入端口漏驱动读到未定义值**（见文件末尾 Batch 2/3 段）。
 *
 *   Batch 3（纯组合）= 1 —— ✅ 2026-10-27 实施：
 *     - BranchPredictor - BNT strategy
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
// ============================================================================
// Batch 2 — 含 ch_reg 的时序组件（2 个） + Batch 3 — 纯组合（1 个）
// ============================================================================
// ⚠️ **机制说明**（change `dual-backend-rv32i-integration-2026-10` 的 design.md §2）：
//
// ① **不存在**「含 clk 的组件能否走 dual」的风险 —— `IfIdPipelineReg` 的 `clk` 是
//    **死端口**（`include/cpu/pipeline/rv32i_pipeline_regs.h:190` 声明了它，
//    但 `describe()`（:203-227）**从不读** `io().clk`）；寄存器推进靠 `ch_reg::next`
//    + Simulator/Verilog 的**隐式时钟** ⇒ 该能力已由 `tests/test_dual_backend_clock.cpp`
//    （Phase 1，spec R10「2-step clock toggle fixture」）证明。
//    ⇒ 隐含地推翻了「Task 3 clk spike」这一步（原文件头与 plan T6 的说法）。
//
// ② **唯一**实质风险 = **输入端口漏驱动读到未定义值**（`AGENTS.md §ANTI-PATTERNS`：
//    实测 `strbe` 读全 1、`rst_n` 读 0）⇒ 未驱动的端口在两后端未必同值
//    ⇒ **同时红或同时绿都可能是假绿** ⇒ 下文 7 个 / 4 个输入端口**逐个显式驱动**。
//
// ③ ⚠️ **不**照抄原 interpreter-only 用例（`tests/test_rv32i_pipeline.cpp:104-107`）
//    的手动 clk toggle：
//        set clk=true → tick() → set clk=false → tick()
//    那是**死端口上的两次额外 tick** ⇒ 会把「一拍」变成三拍 ⇒ 若将来该端口被接回
//    内部逻辑，用例会因**多打两拍**而静默改变语义。
//    ⇒ 本实现**只显式置 `clk=1`**（满足「不读未定义值」），节拍数由 `sim.tick()` 决定
//    （`sim.tick()` 本身已做 2-step toggle，Verilog 侧亦然）。

// ----------------------------------------------------------------------------
// Batch 2-a: IfIdPipelineReg - transfers PC and instruction（原 line 69 SECTION body）
// ----------------------------------------------------------------------------
DUAL_BACKEND_TEST_PAIR(
    IfIdPipelineReg,
    "IfIdPipelineReg - transfers PC and instruction",
    "[pipeline][regs][behavior][transfer][dual]",
    [&]() {
        // 金标推导（**手工推导**，非抄仿真输出；见 design.md §4.1）：
        //   update = select(!rst, select(!stall, !flush, false), false)
        //          = select(!rst=1, select(!stall=1, !flush=1, false), false) = true
        //   ⇒ pc_reg    ← if_pc          = 0x100
        //   ⇒ instr_reg ← if_instruction = 0x00100093
        //   ⇒ valid_reg ← if_instr_valid = 1
        //
        // **反向期望**（判别力）：若 IF/ID 的装载被 stall/flush 误门控
        //   ⇒ id_pc 停在 INIT(0) ⇒ `0x100 != 0` ⇒ 本断言不是恒真、也不是初值巧合。
        sim.set_input_value(dev.instance().io().if_pc, 0x100_h);
        sim.set_input_value(dev.instance().io().if_instruction, 0x00100093_h);
        sim.set_input_value(dev.instance().io().if_instr_valid, true);
        // ⚠️ 该组件的 `rst` 是**高有效**（`update = select(!io().rst, ...)`
        //    ⇒ `rst=0` 才是「正常工作」）⇒ 驱动 **0**（不是 1！）
        sim.set_input_value(dev.instance().io().rst, false);
        sim.set_input_value(dev.instance().io().stall, false);
        sim.set_input_value(dev.instance().io().flush, false);
        // ⚠️ `clk` 是**死端口** ⇒ 仍 MUST 显式驱动（否则读到未定义值）；
        //    但**只置 1**，不做手动 toggle（见上方 ③）
        sim.set_input_value(dev.instance().io().clk, true);

        sim.tick();

        auto id_pc = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().id_pc));
        auto id_instr = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().id_instruction));
        auto id_valid = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().id_instr_valid));

        REQUIRE(id_pc == 0x100);
        REQUIRE(id_instr == 0x00100093);
        REQUIRE(id_valid == 1);
    });

// ----------------------------------------------------------------------------
// Batch 2-b: IfIdPipelineReg - clears instr_valid on flush（原 line 100 SECTION body）
// ----------------------------------------------------------------------------
DUAL_BACKEND_TEST_PAIR(
    IfIdPipelineReg,
    "IfIdPipelineReg - clears instr_valid on flush",
    "[pipeline][regs][behavior][flush][dual]",
    [&]() {
        // 金标推导（**手工推导**，见 design.md §4.2）：
        //  第 1 拍（flush=0）：update = true ⇒ 装入
        //      pc_reg=0x100 / instr_reg=0x00100093 / valid_reg=1
        //  第 2 拍（flush=1）：
        //      update = select(!rst, select(!stall, !flush, false), false)
        //             = select(1, select(1, 0, false), false) = **false**
        //      ⇒ pc_reg->next   = select(false, if_pc, pc_reg)   = **保持 0x100**
        //      ⇒ instr_reg->next = select(false, if_instruction, …) = **保持 0x00100093**
        //      ⇒ valid_reg->next = select(update=false, instr_ready,
        //                            select(flush=true, false, valid_reg))
        //                         = select(flush, false, valid_reg) = **false**
        //      ⇒ **flush 只清 valid，不清 pc / instruction**
        //
        // **反向期望**（判别力）：
        //   ① flush 后 `id_instr_valid` MUST NOT 仍为 1（清零语义）
        //   ② `id_pc` MUST **仍是** 0x100 ⇒ 若实现把 flush 误写成「清一切」
        //      ⇒ id_pc 会变 0 ⇒ 本断言能抓到
        sim.set_input_value(dev.instance().io().if_pc, 0x100_h);
        sim.set_input_value(dev.instance().io().if_instruction, 0x00100093_h);
        sim.set_input_value(dev.instance().io().if_instr_valid, true);
        sim.set_input_value(dev.instance().io().rst, false);
        sim.set_input_value(dev.instance().io().stall, false);
        // ⚠️ **第 1 拍前 MUST 驱动 `flush`**（否则该拍 7 个输入端口只驱动了 6 个
        //    ⇒ 读到未定义值 ⇒ 本次实测恰好是 0 而在两后端「碰巧」等价于 flush=0，
        //    但那是**未定义行为依赖**（`AGENTS.md §ANTI-PATTERNS`：`strbe` 实测读全 1）
        //    ⇒ 若某后端读到 1，`update=false` ⇒ 下面的 `valid_after_load == 1` 会
        //    **假性转红**）
        sim.set_input_value(dev.instance().io().flush, false);
        sim.set_input_value(dev.instance().io().clk, true);

        // 第 1 拍：正常装载
        sim.tick();

        auto valid_after_load = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().id_instr_valid));
        REQUIRE(valid_after_load == 1);   // 前置条件：先装上 valid

        // 第 2 拍：flush ⇒ 只清 valid
        sim.set_input_value(dev.instance().io().flush, true);
        sim.tick();

        auto id_valid = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().id_instr_valid));
        auto id_pc = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().id_pc));
        auto id_instr = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().id_instruction));

        REQUIRE(id_valid == 0);       // valid 被清零
        REQUIRE(id_pc == 0x100);      // pc **保持**（flush 不清 pc）
        REQUIRE(id_instr == 0x00100093);  // instruction **保持**
    });

// ----------------------------------------------------------------------------
// Batch 3: BranchPredictor - BNT strategy（原 line 403 SECTION body）
// ----------------------------------------------------------------------------
DUAL_BACKEND_TEST_PAIR(
    BranchPredictor,
    "BranchPredictor - BNT strategy",
    "[pipeline][branch][behavior][predict][dual]",
    [&]() {
        // 金标推导（**手工推导**，见 design.md §4.3）：
        //   instruction = 0x00000063 = BEQ x0, x0, +0
        //     opcode = bits<6,0>  = 0x63 = 99 ⇒ is_branch = 1
        //     funct3 = bits<14,12> = 0    ⇒ is_beq = 1
        //   rs1_data = rs2_data = 0 ⇒ equal = 1
        //   branch_condition = (is_beq & equal) | … = 1
        //   branch_taken = is_branch & branch_condition = 1
        //   predict_error = branch_taken（BNT：预测「不跳」⇒ 实际跳 ⇒ 预测错）= 1
        //   B 型立即数 = 0 ⇒ imm_extended = 0 ⇒ branch_target = pc + 0 = 0x100
        //
        // **反向期望**：`predict_error` MUST NOT 为 0（BNT 的定义）；
        //   `branch_taken` MUST NOT 为 0；`branch_target` MUST NOT 是 pc 之外的乱值。
        sim.set_input_value(dev.instance().io().instruction, 0x00000063_h);
        sim.set_input_value(dev.instance().io().rs1_data, 0x0_h);
        sim.set_input_value(dev.instance().io().rs2_data, 0x0_h);
        sim.set_input_value(dev.instance().io().pc, 0x100_h);

        sim.tick();

        auto is_branch = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().is_branch));
        auto branch_taken = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().branch_taken));
        auto predict_error = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().predict_error));
        auto branch_target = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().branch_target));

        REQUIRE(is_branch == 1);
        REQUIRE(branch_taken == 1);
        REQUIRE(predict_error == 1);
        REQUIRE(branch_target == 0x100);
    });
