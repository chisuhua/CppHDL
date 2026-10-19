# Design: dual-backend-rv32i-pipeline-behavior-tests

> 实施级设计。Scope: 新建 tests/test_rv32i_pipeline_behavior.cpp，覆盖 RV32I microarchitecture 子组件 dual 行为 parity。

## 1. 文件结构

```
openspec/changes/dual-backend-rv32i-pipeline-behavior-tests/
├── proposal.md
├── design.md  (本文件)
├── tasks.md
└── specs/dual-backend-test-coverage/spec.md
tests/
└── test_rv32i_pipeline_behavior.cpp    # 新建: ~15 DUAL_BACKEND_TEST_PAIR
tests/CMakeLists.txt                     # 改: add_dual_backend_test 注册
```

## 2. RV32I 子组件 dual 化可行性矩阵

来源：`tests/test_rv32i_pipeline.cpp` 全文 grep + 各组件头文件 inspect。

| 组件 | 头文件 | 顶层 ch_out 数 | 最大位宽 | aread? | wide? | R6 通过 | dual |
|------|----------|------|---------|-------|-------|---------|------|
| `BranchDetector` | examples/riscv-mini/src/rv32i_branch_predict.h | ≤ 2 | 32-bit | ❌ | ❌ | ✅ | ✅ Batch 1 |
| `BranchConditionUnit` | 同上 | ≤ 4 | 32-bit | ❌ | ❌ | ✅ | ✅ Batch 1 |
| `ForwardingUnit` | examples/riscv-mini/src/rv32i_forwarding.h | ≤ 4 | 32-bit | ❌ | ❌ | ✅ | ✅ Batch 1 |
| `ControlHazardUnit` | examples/riscv-mini/src/hazard_unit.h | ≤ 4 | 32-bit | ❌ | ❌ | ✅ | ✅ Batch 1 |
| `HazardDetectionUnit` | examples/riscv-mini/src/hazard_detection.h | ≤ 2 | 32-bit | ❌ | ❌ | ✅ | ✅ Batch 1 |
| `IfIdPipelineReg` | examples/riscv-mini/src/rv32i_pipeline_regs.h | ≤ 4 | 32-bit | ❌ | ❌ | ⚠️ clk 弹 | ✅ Batch 2 |
| `BranchPredictor` | examples/riscv-mini/src/rv32i_branch_predict.h | ≤ 4 | 32-bit | ❌ | ❌ | ✅ | ✅ Batch 3 |

**结论**：所有候选组件 ≤64-bit、0 aread、0 wide、0 STATIC_REQUIRE。R6 通过率 100%。

## 3. ~15 个 TEST_CASE 设计

### 3.1 helper function

```cpp
// 通用：组合逻辑 dual 直接 set/get_port_value，时序单元需 spike clk。
// 详见 Batch 2 §3.3。
```

### 3.2 macro pattern（统一）

```cpp
#include "dual_backend_fixture.h"

DUAL_BACKEND_TEST_PAIR(
    BranchDetector,
    "BranchDetector - detects BEQ",
    "[pipeline][branch][behavior][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().opcode, OPCODE_BEQ);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().is_branch)) == 1);
    });
```

**注意**：macro 注入 `dev` (ch_device<DutType>&) + `sim` (Simulator&)。用户 body 不再声明。

### 3.3 Batch 2 clk-toggle spike（关键风险）

`IfIdPipelineReg` 是时序单元，含 `ch_in<ch_bool> clk` + `ch_in<ch_bool> rst`。原 test_rv32i_pipeline.cpp 第 87-90 行手动 toggle `clk`：

```cpp
sim.set_input_value(reg.instance().io().clk, true);
sim.tick();
sim.set_input_value(reg.instance().io().clk, false);
sim.tick();
```

但 VerilatorBackend 契约（ROOT AGENTS.md VERILATOR BACKEND RULES）规定 clock 由 backend 的 2-step toggle 直接驱动（`clock_field_ptr_` 直写）；`sync_inputs_to_vtop(exclude_clock=true)` 跳过 clock。

**spike 方案**：
1. **Task 1（spike 1 个 TEST_CASE）**：用 macro dual 化 `IfIdPipelineReg - transfers PC and instruction`
   - lambda body：**不改**手动 toggle clk 模式（保留原 `set_input_value(clk, true); tick(); set_input_value(clk, false); tick()`）
   - **预期风险**：verilator half 可能 clk 行为不一致（`sync_inputs_to_vtop(exclude_clock=true)` 跳过手动 clk set → 用户 clk toggle 被吞）
2. **若 spike 失败**：lambda 内用 `current_kind()` 分支
   ```cpp
   if (ch_test::dual_backend::current_kind() ==
       ch_test::dual_backend::BackendKind::Interpreter) {
       sim.set_input_value(dev.instance().io().clk, true);
       sim.tick();
       sim.set_input_value(dev.instance().io().clk, false);
       sim.tick();
   } else {
       sim.tick();  // verilator backend 自动 2-step
       sim.tick();
   }
   ```
3. **若 spike 通过**：Batch 2 剩下 1 个 TEST_CASE 直接复用

### 3.4 转换 map（与 test_rv32i_pipeline.cpp line number 对照）

来源：tests/test_rv32i_pipeline.cpp 全文 grep + oracle 调研。

| 原 TEST_CASE/SECTION (line) | fixture | dual 化 |
|---|---|---|
| line 282 `BranchDetector detects BEQ` | BranchDetector | ✅ Batch 1 |
| line 297 `BranchDetector detects BNE` | BranchDetector | ✅ Batch 1 |
| line 312 `BranchDetector non-branch instruction` | BranchDetector | ✅ Batch 1 |
| line 329 `BranchConditionUnit BEQ equal` | BranchConditionUnit | ✅ Batch 1 |
| line 343 `BranchConditionUnit BEQ not equal` | BranchConditionUnit | ✅ Batch 1 |
| line 357 `BranchConditionUnit BLT signed less` | BranchConditionUnit | ✅ Batch 1 |
| line 374 `BranchConditionUnit BLTU unsigned less` | BranchConditionUnit | ✅ Batch 1 |
| line 150 `ForwardingUnit detects EX hazard` | ForwardingUnit | ✅ Batch 1 |
| line 187 `ForwardingUnit detects MEM hazard` | ForwardingUnit | ✅ Batch 1 |
| line 224 `ForwardingUnit no hazard - use register file` | ForwardingUnit | ✅ Batch 1 |
| line 424 `ControlHazardUnit flush logic` | ControlHazardUnit | ✅ Batch 1 |
| line 446 `HazardDetectionUnit LOAD-USE` | HazardDetectionUnit | ✅ Batch 1 |
| line 468 `HazardDetectionUnit LOAD-USE no stall` | HazardDetectionUnit | ✅ Batch 1 |
| line 76 `IfIdPipelineReg transfers PC and instruction` | IfIdPipelineReg | ⚠️ Batch 2 spike |
| line 104 `IfIdPipelineReg clears instr_valid on flush` | IfIdPipelineReg | ⚠️ Batch 2 (spike 后) |
| line 403 `BranchPredictor BNT strategy` (推断) | BranchPredictor | ✅ Batch 3 (可选) |

## 4. 显式不迁移清单（在新文件头注释引用原文件行号）

来源：oracle 调研识别 + R6 #1/#2/#6 排除。

| 原 line | 类别 | R6 排除 | 不 dual 理由 |
|---|---|---|---|
| 47-69 | 4 个 pipeline reg 裸构造 (IfId/IdEx/ExMem/MemWb) | #1 / #2 | 编译-only instantiate，无 Simulator |
| 144 | `ForwardingUnit` instantiate | #1 / #2 | 编译-only instantiate |
| 265 | `ForwardingMux` instantiate | #1 / #2 | 编译-only instantiate（类不存在，SKIP） |
| 397 | `BranchPredictor` instantiate | #1 / #2 | 编译-only instantiate |
| 274 | `ForwardingMux` SKIP | #6 | SKIP-only stub，无 runtime 语义 |
| 495 | `Rv32iCore` SUCCEED | #1 / #6 | SUCCEED-only stub |
| 500 | `Rv32iCore` IO SKIP | #1 / #6 | SKIP-only stub |
| 528-538 | `Full Data Path` 7 组件裸构造 | #1 / #2 | 编译-only instantiate |
| 507-521 | `IPC Verification` 纯数学断言 | #6 | `REQUIRE` 对两个 double，不碰硬件 |

## 5. CMakeLists.txt 改动

```cmake
# 现有:
add_catch_test(test_rv32i_pipeline test_rv32i_pipeline.cpp)
target_include_directories(test_rv32i_pipeline PRIVATE ${PROJECT_SOURCE_DIR}/examples/riscv-mini/src)

# 追加（不删现有）:
add_dual_backend_test(test_rv32i_pipeline_behavior
    SOURCES test_rv32i_pipeline_behavior.cpp
    TIMEOUT 300
)
target_include_directories(test_rv32i_pipeline_behavior PRIVATE ${PROJECT_SOURCE_DIR}/examples/riscv-mini/src)
```

**TIMEOUT 300**：RESOURCE_LOCK "verilator_backend" 串行下，30 test case halves × warm cache < 1 sec = < 30 sec；cold cache 13.47 sec/test_verilator_backend × 1 极限下还有 270 sec 余量。

## 6. spec 增量（详见 specs/dual-backend-test-coverage/spec.md）

```
### Requirement: RV32I microarchitecture behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR
- file naming MUST _behavior suffix
- 原 test_rv32i_pipeline.cpp MUST NOT 修改
- 顶层 SOC dual 化排除
- R6 #5 SECTION 排除保留
```

## 7. 实施 TDD 5 步

1. **新建最小 dual 化样例**：选最简单的 `BranchDetector - detects BEQ` 1 个 TEST_CASE → DUAL_BACKEND_TEST_PAIR，interpreter half 跑通
2. **同 TEST_CASE verilator half 跑通**：warm cache < 5 sec，验证 ch_out 端口值在两 half 一致
3. **批量迁移 Batch 1 剩下 12 个 TEST_CASE**（保持原 tag `[...]` 不变；追加 `[behavior][dual]` 标识）
4. **Batch 2 spike**：1 个 IfIdPipelineReg TEST_CASE dual 化，根据 spike 结果决定 Batch 2 第二个
5. **Batch 3（可选）**：BranchPredictor BNT
6. **CMakeLists.txt 改 add_dual_backend_test**（LABELS / RESOURCE_LOCK / ENVIRONMENT / TIMEOUT）
7. **archive 前置 gate**：openspec validate --strict + ctest 全量 + verify_test_counts.sh + AGENTS.md sync

## 8. 风险

| 风险 | 等级 | mitigation |
|------|------|------------|
| Batch 2 clk-toggle spike 失败 | 🟡 | 已规划 3 步式降级（保留 + current_kind 分支 + golden-value parity） |
| 子组件 codegen 在 verilator 端出错（首次 dual 暴露的 bug） | 🟡 | 这是本 change 目的（暴露 codegen bug），按 R 行为 |
| Cold cache 累积（15+ 不同 SHA-1）墙钟超 300 sec | 🟢 | warm cache < 1 sec / dual-test，仅冷启动一次性 |
| Macro lambda body 与原 TEST_CASE body 不等价（缺 ch_in driver） | 🟡 | 第一个 TEST_CASE 确认所有 ch_in 都 set_input_value 驱动 |

## 9. 反向约束

- `verilator-e2e-coverage` spec: 不动 `tests/test_verilator_*.cpp` 测试体（reference impl）
- `verilator-rv32i-e2e` spec: 不动 `tests/test_verilator_complex_e2e.cpp` RV32I 短指令 trace parity 路径
- `cpu-cache-subsystem` spec: 不动 cache 子系统（已 ADR-041 Sync/Async 落地）
- ROOT AGENTS.md VERILATOR BACKEND RULES: ch_out 64-bit cap / cache writeback / 2-step clock toggle / pulse default_reset 不破
- ROOT AGENTS.md CONVENTIONS: ch_in driver via sim.set_input_value 不破

## 10. 验收命令清单

```bash
# 编译
cmake --build build -j$(nproc) --target test_rv32i_pipeline_behavior 2>&1 | grep -E "error|Error"  # 0 errors

# openspec 验证
openspec validate dual-backend-rv32i-pipeline-behavior-tests --strict  # valid
openspec validate --specs --strict  # 14+1 passed (含本 change +1 requirement)

# 测试
ctest -LE perf --output-on-failure  # 不退化（155+15=170 non-perf PASS）
ctest -L verilator_e2e -LE perf -R test_rv32i_pipeline_behavior --output-on-failure  # 30 test cases 全绿

# 计数同步
cd build && ctest -N | tail -1  # CTEST_TOTAL 167（+1）
bash scripts/verify_test_counts.sh  # exit 0（CTEST_TOTAL 167 + chlib/root 不变）

# AGENTS.md sync
# - CTEST_TOTAL 166 → 167
# - root tests/test_*.cpp : 96 → 97
```

## 11. CTEST_TOTAL 影响

**CTEST_TOTAL 166 → 167**（+1 ctest target：test_rv32i_pipeline_behavior）。
**非 perf 测试数 155 → 170**（+15 = 30 dual halves）。

需要 sync：
- 根 AGENTS.md NOTES §Test counts: `CTEST_TOTAL: 166 → 167` + "root tests/test_*.cpp: 96 → 97"
- 根 AGENTS.md feature fragments `feat-dual-backend-coverage` phase_refs（已含 phase-5，本 change 兑现 phase-5）

---

**实施前必读**：`openspec/changes/archive/2026-10-19-dual-backend-cache-tests/design.md` §6 风险表（保守时序 spike 策略）+ §2.4 "避免 fragile timing" 原则。Batch 2 clk spike 必须先于 Batch 1 完整收敛。