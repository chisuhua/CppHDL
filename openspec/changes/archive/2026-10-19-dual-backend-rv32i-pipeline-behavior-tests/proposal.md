# Change: dual-backend-rv32i-pipeline-behavior-tests — RV32I microarchitecture dual-backend behavior parity

## Why

**现状**：
- `tests/test_rv32i_pipeline.cpp` 是 RV32I 5-stage pipeline 的 microarchitecture 级测试套件（**12 个 TEST_CASE**），覆盖独立小组件 `ch::ch_device<T>`：`IfIdPipelineReg` / `ForwardingUnit` / `BranchDetector` / `BranchConditionUnit` / `BranchPredictor` / `ControlHazardUnit` / `HazardDetectionUnit` 等
- 每个 TEST_CASE 已用 `ch_device<T>` + `Simulator` 完整跑通 InterpreterBackend
- 全部 12 个 TEST_CASE 都用 Catch2 `SECTION` 子句嵌套（grep 命中 22 个 SECTION）—— `dual-backend-test-coverage` spec R6 #5 明确排除 "Catch2 SECTION 重入测试"（macro 展开为 2 个 TEST_CASE，每个 half 独立遍历 SECTION 会破坏 fixture 行为）
- 此外 ~1/3 SECTION 命中 R6 其他排除项（R6 #1/#2 编译-only 实例化 / SKIP / SUCCEED / 纯数学 IPC 断言）—— 实际有 runtime 语义的 SECTION **~15 个**
- `dual-backend-test-coverage` Phase 4（`features/`，`test_rv32i-integration` phase-5 入口）原目的即 dual 化 RV32I 测试
- 前置依赖 `examples/riscv-mini/src/i_cache_prefetch.h` SIGSEGV 已修（2026-10-20 commit `b987bb7` fix-riscv-mini-cache-prefetch-sigsegv）—— 阻塞解除

**真实代价**：

| 历史痛点 | 暴露点 |
|---|---|
| RV32I 5-stage pipeline 子组件（forwarding / branch / hazard）只走 interpreter | 任何 RV32I microarchitecture codegen drift 必须 nightly-rv32i SOC 集成才发现，PR 合并前不可观测 |
| issue #25 "always_ff may not propagate" 残余风险 | interpreter 完全没暴露，verilator 才暴露；RV32I 子组件级 dual 化是"分而治之"最小代价路径 |
| RV32I Phase 1-3 已 PASS（13/13+13/13+5/5），但每条 instruction path 在 RV32I 子组件级只走 interpreter | 子组件 codegen 错误在 e2e SOC test 才暴露，debug 必须从 SOC trace 回溯到具体子组件 |

**预期收益**：

1. **PR feedback 提早**：任何 RV32I microarchitecture codegen 回归在对应 TEST_CASE 立即失败（≤45 min PR feedback），不等 nightly-rv32i SOC
2. **debug 粒度提升**：从"5-stage SOC trace 错"下沉到"具体子组件 parity 错"
3. **兑现 feature fragment `feat-rv32i-integration` phase-5 入口**（AGENTS.md 顶部 feature fragments 表）
4. **clk-toggle spike 趟平 verilator 契约边界**：hier-input / 2-step clock toggle / 手动 clk 输入 与 backend 内建 clk 三者实战

**预期成本**：

- 一次性 CI 墙钟增量 ~30-60 sec/PR（warm cache < 1 sec / dual-test）
- 改造工作量 ~2 人天（含 0.5 天 clk spike buffer）
- ~15 个 TEST_CASE × 2 backend = 30 个 test case halves（净增 +15 TEST_CASE；其中 4 个 pipeline-reg 涉及 clk spike）

**Out of Scope**（明确划线）：
- ❌ `tests/test_rv32i_pipeline.cpp` 不动（保留 interpreter-only baseline；spec R7 + AXI-behavior requirement 两次固化"原文件不动"哲学）
- ❌ `Rv32iSoc` 顶层嵌套 dual 化（`ch::ch_module<Rv32iPipeline>` + `ch::ch_module<InstrTCM<20, 32>>` + ...）—— macro R2 锁定 ch_device 形态
- ❌ `Rv32iPipelineTop` 顶层 dual 化——同上，多 ch_in port 未指定 driver 即 R6 #3 类问题
- ❌ `examples/riscv-mini/tests/*.cpp`（23 个 wrapper / pipeline / riscv-tests test）——这些走 `run_all_ported_tests.sh` 入口（28 examples），与 ctest 双轨
- ❌ 拆 SECTION 改造原文件（违反 R6 #5 + 偏离 `_behavior` 后缀 pattern）
- ❌ `tests/test_verilator_complex_e2e.cpp`（既有 RV32I 短指令 trace parity 路径）—— spec 锁定为 macro reference impl

## What Changes

### 1. 新建 `tests/test_rv32i_pipeline_behavior.cpp`

**~15 个 `DUAL_BACKEND_TEST_PAIR`**（oracle 推荐命名 `_behavior` 后缀，与 `test_axi_spi_behavior` 一致），按风险分 3 批：

**Batch 1（纯组合逻辑，无 clk 风险，13 个）**：
- `BranchDetector - detects BEQ`
- `BranchDetector - detects BNE`
- `BranchDetector - non-branch instruction`
- `BranchConditionUnit - BEQ equal`
- `BranchConditionUnit - BEQ not equal`
- `BranchConditionUnit - BLT signed less`
- `BranchConditionUnit - BLTU unsigned less`
- `ForwardingUnit - detects EX hazard`
- `ForwardingUnit - detects MEM hazard`
- `ForwardingUnit - no hazard - use register file`
- `ControlHazardUnit - Branch flush logic`
- `HazardDetectionUnit - LOAD-USE detect`
- `HazardDetectionUnit - LOAD-USE no stall`

**Batch 2（clk spike 先行，2 个 + spike task）**：
- 若 spike 通过：`IfIdPipelineReg - transfers PC and instruction` + `IfIdPipelineReg - clears instr_valid on flush`
- 若 spike 失败：lambda 内用 `current_kind()` 分支或降级为 golden-value parity（断言）

**Batch 3（可选，1 个）**：
- `BranchPredictor - BNT strategy test`（通过当前 SECTION line 403 推断）

### 2. CMakeLists.txt 注册

```cmake
add_dual_backend_test(test_rv32i_pipeline_behavior
    SOURCES test_rv32i_pipeline_behavior.cpp
    TIMEOUT 300  # 30 dual pairs 串行；RESOURCE_LOCK "verilator_backend" 强
)
```

### 3. spec `dual-backend-test-coverage` 增量

新增 `## ADDED Requirements`（仿照 cache Sync requirement 格式）：

```
### Requirement: RV32I microarchitecture behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

tests/test_rv32i_pipeline_behavior.cpp（_behavior suffix 区别原
tests/test_rv32i_pipeline.cpp）MUST 用 DUAL_BACKEND_TEST_PAIR macro 覆盖
RV32I microarchitecture 子组件（ForwardingUnit / BranchDetector /
BranchConditionUnit / BranchPredictor / ControlHazardUnit /
HazardDetectionUnit / IfIdPipelineReg）。原 tests/test_rv32i_pipeline.cpp
MUST NOT 修改（保留 interpreter-only baseline）。

#### Scenario: file 命名 + space 互斥

- **WHEN** 新增 RV32I microarchitecture behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_rv32i_pipeline.cpp`
- **AND** 原 `test_rv32i_pipeline.cpp` MUST NOT 修改

#### Scenario: macro 接口约束

- **WHEN** 测试访问 `ch_out<ch_uint<32>>` 等端口值
- **THEN** MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`
- **AND** MUST NOT 用 `port.impl()`（违反 macro R3）

#### Scenario: 顶层 SOC dual 化排除

- **WHEN** fixture 是 `Rv32iSoc`（嵌套 ch::ch_module<...> + 多 ch_in port）
- **THEN** MUST NOT 改 dual 化（超 macro R2/R3 scope）
- **AND** 顶层 SOC dual 验证走既有 tests/test_verilator_complex_e2e.cpp
  ComplexE2E2 - Rv32iShortInstructionTraceParity 路径

#### Scenario: R6 #5 SECTION 排除保留

- **WHEN** 原 tests/test_rv32i_pipeline.cpp 保留 SECTION 嵌套 TEST_CASE
- **THEN** 该 TEST_CASE MUST NOT 改 dual 化（macro R6 #5 排除 SECTION 重入）
- **AND** dual 验证必须走新文件 test_rv32i_pipeline_behavior.cpp 的独立 TEST_CASE
```

## Acceptance

#### Scenario: ~15 个 TEST_CASE_PAIR 全 PASS

- **WHEN** `cd build && cmake --build . --target test_rv32i_pipeline_behavior && ctest -L verilator_e2e -LE perf -R test_rv32i_pipeline_behavior --output-on-failure`
- **THEN** test_rv32i_pipeline_behavior 通过；~30 个 test cases（15 dual pairs）全 PASS

#### Scenario: 端口值 parity

- **WHEN** interpreter half 与 verilator half 都跑同一份 lambda body（forwarding / branch / hazard / pipeline-reg）
- **THEN** 对 ch_out 端口的逐周期 REQUIRE 断言两端都成立（per macro R3 契约）

#### Scenario: 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误

#### Scenario: 资源锁注入

- **WHEN** `cd build && ctest -N -L verilator_e2e -R test_rv32i_pipeline_behavior`
- **THEN** RESOURCE_LOCK "verilator_backend"，LABELS "base;verilator_e2e"，TIMEOUT 300

#### Scenario: 原文件不动

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** `tests/test_rv32i_pipeline.cpp` MUST NOT 改动（保留 interpreter-only baseline）
- **AND** `examples/riscv-mini/src/*.h` 与 `examples/riscv-mini/tests/*.cpp` MUST NOT 改动
- **AND** `tests/test_verilator_complex_e2e.cpp` MUST NOT 改动（spec 锁定 macro reference impl）

#### Scenario: CTEST_TOTAL +1 sync

- **WHEN** `bash scripts/verify_test_counts.sh` exit 0
- **THEN** CTEST_TOTAL 166 → 167 + root AGENTS.md NOTES §Test counts 同步

## Capabilities

### ADDED Requirements

#### Requirement: RV32I microarchitecture behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

`tests/test_rv32i_pipeline_behavior.cpp`（`_behavior` suffix 区别原 `test_rv32i_pipeline.cpp`） MUST 用 `DUAL_BACKEND_TEST_PAIR` macro 覆盖 RV32I microarchitecture 子组件（`ForwardingUnit` / `BranchDetector` / `BranchConditionUnit` / `BranchPredictor` / `ControlHazardUnit` / `HazardDetectionUnit` / `IfIdPipelineReg`）。**原** `tests/test_rv32i_pipeline.cpp` **MUST NOT 修改**（保留 interpreter-only baseline per dual-backend-test-coverage spec R7）。

#### Scenario: file 命名 + space 互斥

- **WHEN** 新增 RV32I microarchitecture behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_rv32i_pipeline.cpp`
- **AND** 原 `test_rv32i_pipeline.cpp` MUST NOT 修改

#### Scenario: macro 接口约束

- **WHEN** 测试访问 `ch_out<ch_uint<32>>` 等端口值
- **THEN** MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`
- **AND** MUST NOT 用 `port.impl()` 跳过 ch_out 接口（违反 macro R3）

#### Scenario: 顶层 SOC dual 化排除

- **WHEN** fixture 是 `Rv32iSoc` 或 `Rv32iPipelineTop`（嵌套 `ch::ch_module<...>` + 多 `ch_in<...>` port 需 driver）
- **THEN** MUST NOT 改 dual 化（超 macro R2/R3 scope）
- **AND** 顶层 SOC dual 验证走既有 `tests/test_verilator_complex_e2e.cpp`
  `ComplexE2E2 - Rv32iShortInstructionTraceParity` 路径

#### Scenario: R6 #5 SECTION 排除保留

- **WHEN** 原 `tests/test_rv32i_pipeline.cpp` 保留 SECTION 嵌套 TEST_CASE
- **THEN** 该 TEST_CASE MUST NOT 改 dual 化（macro R6 #5 排除 SECTION 重入）
- **AND** dual 验证必须走新文件 `test_rv32i_pipeline_behavior.cpp` 的独立 TEST_CASE