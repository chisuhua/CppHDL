# Capability: cpu-cache-subsystem

## ADDED Requirements

### Requirement: riscv-mini cache wrapper MUST NOT use C-style ch_reg array

`examples/riscv-mini/src/i_cache_prefetch.h` 与 `examples/riscv-mini/src/d_cache_write_buffer.h` **不允许**使用 `ch_reg<T>[N]` C-style 数组（runtime init 触发 SIGSEGV）。MUST 用 `ch_mem<T, N>` 替代（per ADR-039 + cache-complete-doa-note §6.1 R1）。

#### Scenario: line-level grep 检查

- **WHEN** `grep -nE "ch_reg<[^>]+>\s*\[" examples/riscv-mini/src/i_cache_prefetch.h examples/riscv-mini/src/d_cache_write_buffer.h`
- **THEN** 输出必须为空（无 C-style ch_reg 数组）
- **AND** 如有命中，则该 wrapper MUST 用 `ch_mem<T, N>` 替换

#### Scenario: instantiation smoke 测试必通过

- **WHEN** `cd build && ./examples/riscv-mini/riscv_mini_cache_smoke "[fixed]"`
- **THEN** instantiation 必通过；不允许 SIGSEGV