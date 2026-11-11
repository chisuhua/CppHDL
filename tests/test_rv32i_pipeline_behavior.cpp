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
 *     - ⚠️ 2026-11-11：`ForwardingUnit` ×3 已退役（C5+C6 同批）⇒ ⇒ 对应 pair 已删（**这是缩面**）
 *     - ControlHazardUnit ×1 (flush logic)
 *     - ⚠️ 2026-11-11：`HazardDetectionUnit` ×2 已退役（C5+C6 同批）⇒ ⇒ 对应 pair 已删（**这是缩面**）
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
 *
 *   Batch 4（取指 skid buffer 的**防御性契约**）= 3 —— ✅ 2026-10-27 实施
 *     （change rv32i-fetch-skid-buffer-2026-10，ADR-048 D5 / D9.4）：
 *     - IfStage - flush cycle MUST NOT fill skid buffer
 *     - IfStage - flush MUST clear a non-empty skid buffer
 *     - IfStage - skid pc MUST pair with the instruction (fetch_pc, not pc_reg)
 *     ⚠️ 「flush ∥ skid 非空」在**流水线级不可达** ⇒ SoC 级变异验证抓不到
 *       「删掉 flush 清空 skid」的变异 ⇒ **组件级是唯一判别力来源**。
 * 显式不迁移清单（per oracle 调研 + R6 #1/#2/#6 排除）：
 *   - 原 test_rv32i_pipeline.cpp line 47-69 (4 个 pipeline reg 裸构造)        R6 #1/#2
 *   - 原 test_rv32i_pipeline.cpp line 144 (ForwardingUnit instantiate) —— ⚠️ 已退役 ⇒ 该项随之删除
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
#include "cpu/pipeline/rv32i_pipeline_regs.h"
#include "cpu/pipeline/stages/if_stage.h"

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

// ============================================================================
// Batch 4 — 取指 skid buffer 的**防御性契约**（ADR-048 D5 / D9.4）
// ============================================================================
// ⚠️ **为什么必须在组件级测**：流水线级「`flush` ∥ `skid` 非空」**不可达**
//    （skid 非空 ⇒ 停顿窗口 ⇒ EX 是气泡 ⇒ 无分支 ⇒ 无 flush）
//    ⇒ SoC 级变异验证**抓不到**「删掉 flush 清空 skid」的变异
//    ⇒ 该防御逻辑否则就是「看起来有保护」（ADR-048 D5）。
//
// ⚠️ 端口驱动纪律：`IfStage` 有 **7 个输入端口**，逐个显式驱动
//    （未驱动端口读到的是**未定义值**，两后端未必同值 ⇒ 假绿）。
//    ⚠️ 该组件的 `rst` 是**高有效**（`pc_update_en` / `if_id_update_en`
//    都是 `select(io().rst, 0, …)`）⇒ 正常工作驱动 **0**（不是 1！）。
//
// 逐拍金标（**手工推导**，见每个用例的注释）：寄存器初值
//   `pc_reg = 0x80000000`、`fetch_pc = 0`、`skid_* = 0`、`primed = 0`。

namespace {
// 把「一拍」的所有输入一起驱动（7 个端口全覆盖）
template <typename Sim, typename Dev>
void if_tick(Sim &sim, Dev &dev, bool rst, bool stall, bool flush,
             uint32_t instr_data) {
    auto &io = dev.instance().io();
    sim.set_input_value(io.rst, rst);
    sim.set_input_value(io.stall, stall);
    sim.set_input_value(io.flush, flush);
    sim.set_input_value(io.instr_data, instr_data);
    sim.tick();
}
}  // namespace

// ----------------------------------------------------------------------------
// Batch 4-a: flush 同拍 ⇒ skid **不被填充**
// ----------------------------------------------------------------------------
DUAL_BACKEND_TEST_PAIR(
    IfStage,
    "IfStage - flush cycle MUST NOT fill skid buffer",
    "[pipeline][fetch][skid][behavior][flush][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // 常量输入（一次性驱动；`rst` 之外全部显式给值）
        sim.set_input_value(io.branch_valid, false);
        sim.set_input_value(io.branch_target, 0x0_h);
        sim.set_input_value(io.instr_ready, true);

        // 金标（手工推导，每行是该拍**结束后**的状态）：
        //  tick0 (rst=1)   : 全冻结；`fetch_pc ← pc_reg = 0x80000000`
        //  tick1 (stall=0) : `pc_reg = 0x80000004`；IF/ID ← (0x11111111, 0x80000000)
        //  tick2 (flush=1) : ⚠️ **本用例的被测点** —— MUST NOT 填充
        //  tick3 (stall=1) : `flush_d1 = 1` ⇒ 仍不填充
        //  tick4 (stall=1) : 正常填充 ⇒ skid ← (0xCCCC0003, `fetch_pc` = 0x80000004)
        //  tick5 (stall=0) : 排空 ⇒ IF/ID ← skid
        if_tick(sim, dev, true, false, false, 0x0u);
        if_tick(sim, dev, false, false, false, 0x11111111u);
        if_tick(sim, dev, false, true, true, 0xAAAA0001u);   // ⚠️ flush 同拍
        if_tick(sim, dev, false, true, false, 0xBBBB0002u);  // flush_d1 = 1
        if_tick(sim, dev, false, true, false, 0xCCCC0003u);  // 真正的填充
        if_tick(sim, dev, false, false, false, 0xDDDD0004u); // 排空

        auto instr = static_cast<uint64_t>(
            sim.get_port_value(io.instruction));
        auto pc = static_cast<uint64_t>(sim.get_port_value(io.pc));
        // **反向期望**：若 flush 拍被填充 ⇒ skid 里是 0xAAAA0001 ⇒ 这里是它
        REQUIRE(instr == 0xCCCC0003u);
        // ⚠️ 本断言只是**正向值校验**（skid 的 pc 与指令**成对**）：
        //    本场景里填充发生在**停顿的第 3 拍**，而 `pc_reg` 在停顿期间被冻结
        //    ⇒ `pc_reg` 与 `fetch_pc` **恰好相等**（都是 0x80000004）
        //    ⇒ 本用例**不能**区分「skid_pc 取 pc_reg」的变异（M1）
        //    ⇒ M1 的判别力来自：① 下方 Batch 4-c（停顿**首拍**填充）；
        //      ② SoC 级判别性用例的 word 序列不变式。
        REQUIRE(pc == 0x80000004u);
    });

// ----------------------------------------------------------------------------
// Batch 4-b: skid 已非空时来 flush ⇒ skid **被清空**
// ----------------------------------------------------------------------------
DUAL_BACKEND_TEST_PAIR(
    IfStage,
    "IfStage - flush MUST clear a non-empty skid buffer",
    "[pipeline][fetch][skid][behavior][flush][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.branch_valid, false);
        sim.set_input_value(io.branch_target, 0x0_h);
        sim.set_input_value(io.instr_ready, true);

        // 金标（手工推导，每行是**该拍结束后**的状态）：
        //  tick0 (rst=1)   : 全冻结；`fetch_pc ← pc_reg = 0x80000000`
        //  tick1 (stall=0) : `pc_reg = 0x80000004`；IF/ID ← (0x11111111, 0x80000000)
        //                    ⇒ `primed ← 1`
        //  tick2 (stall=1) : 填充 ⇒ skid ← (0x22222222, `fetch_pc` = 0x80000000)
        //                    ⚠️ `fetch_pc` 比 `pc_reg` **晚一拍** ⇒ 此刻 `pc_reg`
        //                      已是 0x80000004 而 `fetch_pc` 仍是 0x80000000
        //  tick3 (flush=1) : ⚠️ **本用例的被测点** —— skid MUST 被清空
        //  tick4 (stall=0) : `flush_d1 = 1` ⇒ IF/ID 冻结（不排空）；
        //                    `pc_reg = 0x80000008`
        //  tick5 (stall=0) : skid 已空 ⇒ IF/ID ← 流上 (0x55555555, `fetch_pc` = 0x80000004)
        //                    ⚠️ `fetch_pc` 比 `pc_reg` **晚一拍**（`fetch_pc_reg->next = pc_reg`）
        if_tick(sim, dev, true, false, false, 0x0u);
        if_tick(sim, dev, false, false, false, 0x11111111u);
        if_tick(sim, dev, false, true, false, 0x22222222u);   // 填充
        if_tick(sim, dev, false, true, true, 0x33333333u);    // ⚠️ flush ⇒ 清空
        if_tick(sim, dev, false, false, false, 0x44444444u);  // flush_d1 = 1
        if_tick(sim, dev, false, false, false, 0x55555555u);  // 流上装载

        auto instr = static_cast<uint64_t>(
            sim.get_port_value(io.instruction));
        auto pc = static_cast<uint64_t>(sim.get_port_value(io.pc));
        // **反向期望**：若 flush 未清空 skid ⇒ 排空给出 **0x22222222**（本断言转红）
        REQUIRE(instr == 0x55555555u);
        REQUIRE(pc == 0x80000004u);
    });

// ----------------------------------------------------------------------------
// Batch 4-c: skid_pc MUST 取 `fetch_pc_reg`（**不是** `pc_reg`）—— ADR-048 D2.1
// ----------------------------------------------------------------------------
// ⚠️ **为什么单独一个用例**：Batch 4-a/4-b 的填充都发生在停顿的**第 3 拍**，
//    那时 `pc_reg` 已被冻结、`fetch_pc` 已追上 ⇒ 二者相等 ⇒ **无法区分** M1。
//    停顿**首拍**才是二者相差 4 字节的时刻（`fetch_pc` 比 `pc_reg` 晚一拍）。
DUAL_BACKEND_TEST_PAIR(
    IfStage,
    "IfStage - skid pc MUST pair with the instruction (fetch_pc, not pc_reg)",
    "[pipeline][fetch][skid][behavior][align][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.branch_valid, false);
        sim.set_input_value(io.branch_target, 0x0_h);
        sim.set_input_value(io.instr_ready, true);

        // 金标（手工推导，每行是该拍**结束后**的状态）：
        //  tick0 (rst=1)   : 全冻结；`fetch_pc ← 0x80000000`
        //  tick1 (stall=0) : `pc_reg = 0x80000004`、`fetch_pc = 0x80000000`
        //                    IF/ID ← (0x11111111, 0x80000000)
        //  tick2 (stall=1) : **停顿首拍** ⇒ 填充 ⇒ skid ← (0x22222222, **0x80000000**)
        //                    ⚠️ 若误取 `pc_reg` ⇒ skid_pc = **0x80000004**
        //  tick3 (stall=0) : 排空 ⇒ IF/ID ← skid = (0x22222222, 0x80000000)
        if_tick(sim, dev, true, false, false, 0x0u);
        if_tick(sim, dev, false, false, false, 0x11111111u);
        if_tick(sim, dev, false, true, false, 0x22222222u);   // 停顿首拍 ⇒ 填充
        if_tick(sim, dev, false, false, false, 0x99999999u);  // 排空

        auto instr = static_cast<uint64_t>(
            sim.get_port_value(io.instruction));
        auto pc = static_cast<uint64_t>(sim.get_port_value(io.pc));
        REQUIRE(instr == 0x22222222u);
        // **反向期望**：`skid_pc` 误取 `pc_reg` ⇒ 这里是 **0x80000004**（错 4 字节）
        REQUIRE(pc == 0x80000000u);
    });
