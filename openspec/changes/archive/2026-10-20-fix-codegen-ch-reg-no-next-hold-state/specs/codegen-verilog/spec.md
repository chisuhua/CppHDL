# Capability: codegen-verilog

## Purpose

定义 CppHDL Verilog 代码生成器的契约（详见主 spec `openspec/specs/build-cpp-standard/spec.md` 与 `verilator-reg-init/spec.md`）。

## ADDED Requirements

### Requirement: ch_reg 无显式 next 时 MUST emit hold-state always_ff

`verilogwriter::print_reg` 当 `ch_reg<T>(init_value)` 未调用 `->next = ...` 时（`next_node == nullptr`） MUST emit hold-state always_ff block，让 reset pulse 期间 init value 正确应用到 VerilatorBackend。

#### Scenario: hold-state always_ff 模板

- **WHEN** `ch_reg<T>(init_value)` 但无 `->next`
- **THEN** MUST emit `always_ff @(posedge default_clock or posedge default_reset)` 块
- **AND** `if (!default_reset)` 分支 MUST hold（`reg <= reg` 自我赋值）
- **AND** `else` 分支 MUST 设 init value（`reg <= reset_value`，reset_value 由 init 字面量计算）

#### Scenario: 与现有 reset_value 计算路径对齐

- **WHEN** reviewer 阅读 `print_reg` 源码 line 679-697 (reset_value 计算) + line 732-765 (新 hold-state always_ff)
- **THEN** reset_value MUST 在 if 分支和 else 分支都使用
- **AND** MUST NOT 仅在 if 分支使用（否则 else 分支的 ch_reg init value 不应用）
- **AND** MUST NOT 仅在 else 分支使用（否则 if 分支的 ch_reg next value 不应用）

#### Scenario: dual parity 通过

- **WHEN** interpreter backend 应用 init value (via `regimpl::create_instruction` line 38-41)
- **AND** VerilatorBackend 应用 init value (via always_ff else 分支)
- **THEN** 两后端在 reset pulse 后读 ch_reg init value MUST 一致

#### Scenario: 不破现有 if 分支路径

- **WHEN** `ch_reg<T>(init_value)` 有显式 `->next = ...` 赋值
- **THEN** MUST NOT 改变 if 分支行为（保持 `emit_always_ff(reg, next, reset_value)` 不变）
- **AND** MUST NOT emit 重复 always_ff block