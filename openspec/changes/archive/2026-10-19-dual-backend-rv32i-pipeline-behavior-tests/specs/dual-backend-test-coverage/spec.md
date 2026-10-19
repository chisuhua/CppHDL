# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约：通过 `DUAL_BACKEND_TEST` macro + `add_dual_backend_test` CMake function，让**任何可观测顶层 `ch_out` 端口、且具有 `ch_device` / `ch_module` 形态的设计**自动走两套 backend，逐周期比较 `ch_out` 端口值，使得 codegen 错误与底层 eval 错误在测试时即时暴露，而非延迟到 nightly / nightly-rv32i 集成时才发现。

In English: Defines the contract for CppHDL unit tests uniformly exercising both InterpreterBackend and VerilatorBackend through a `DUAL_BACKEND_TEST` macro + `add_dual_backend_test` CMake function. The macro MUST automatically construct two `ch_device<T>` instances (one per backend), run the same per-cycle assertions across both, and surface codegen or eval errors at test-time rather than during nightly RV32I integration.

## ADDED Requirements

### Requirement: RV32I microarchitecture behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

`tests/test_rv32i_pipeline_behavior.cpp`（`_behavior` suffix 区别原 `test_rv32i_pipeline.cpp`） MUST 用 `DUAL_BACKEND_TEST_PAIR` macro 覆盖 RV32I microarchitecture 子组件（`ForwardingUnit` / `BranchDetector` / `BranchConditionUnit` / `BranchPredictor` / `ControlHazardUnit` / `HazardDetectionUnit` / `IfIdPipelineReg`）。**原** `tests/test_rv32i_pipeline.cpp` **MUST NOT 修改**（保留 interpreter-only baseline per dual-backend-test-coverage spec R7）。

#### Scenario: file 命名 + space 互斥

- **WHEN** 新增 RV32I microarchitecture behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_rv32i_pipeline.cpp`
- **AND** 原 `tests/test_rv32i_pipeline.cpp` MUST NOT 修改

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

#### Scenario: clk-toggle spike 隔离

- **WHEN** fixture 是 `IfIdPipelineReg`（含 `ch_in<ch_bool> clk` 需手动 toggle）
- **THEN** 必须先单独 spike 验证 verilator half 行为
- **AND** 若 spike 失败，lambda 内 MUST 用 `current_kind()` 分支降级
- **AND** MUST NOT 用 R3 兼容约束以外的隐式时钟假设