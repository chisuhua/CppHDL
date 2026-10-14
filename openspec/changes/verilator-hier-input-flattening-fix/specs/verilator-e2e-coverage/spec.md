## MODIFIED Requirements

### Requirement: Verilator 与 Interpreter trace parity 单测覆盖 ≥ 3 个 fixture

- **MODIFIED**: 移除 invariant 5 中"trace fixture MUST NOT 依赖 always_ff 寄存器传播（issue #25）"的旧误诊。issue #25 的真正残余症状是"层次化设计的子模块 input port 被错误展平为顶层 input port 而无 assign"（由 `verilator-hier-input-flattening-fix` 修复）。修复后 always_ff 寄存器由 Verilator 的 2-step 时钟翻转自动触发，寄存器驱动的 ch_out 是合法观测点。

#### Scenario: 修复后寄存器驱动的 ch_out 是合法观测点

- **WHEN** fixture 用 `ch_reg<ch_uint<32>> reg(0_d); reg->next = reg + 1_d; io().out = reg;`（CounterFixture，reg-driven ch_out）
- **AND** 跑 50 cycle
- **THEN** interpreter 与 verilator 端 `data_map_[io().out.id()]` 在 cycle 50 都 == 50（修复前 verilator 也通过，因为无 ch_module 嵌套；修复后仍通过）
- **AND** 同样的 reg-driven ch_out 在有 ch_module 嵌套的 RV32I fixture 中（perf_instructions = commit_count）也必须正确传播到 verilator 端（修复前 = 0，修复后 = 累加值）

#### Scenario: 修复前 vs 修复后行为对照

- **WHEN** 比较 verilator-rv32i-pipeline-e2e change (archive 2026-10-14) 的 `tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` 修复前后
- **THEN** 修复前：`perf_instructions == 0`（D8 (b) SKIP）
- **AND** 修复后：`perf_instructions == 12`（真实跑通）
- **NOTE**: 这是"verilator-hier-input-flattening-fix" change 的 success criteria 之一