## ADDED Requirements

### Requirement: codegen 字面量值 MUST mask 到目标 wire 宽度内

The Verilog code generator **MUST** emit literal RHS values whose numeric value fits within the target wire's declared width. Specifically: for any `ch_uint<N>` expression involving a literal operand (e.g., `ch_uint<N>(int_value)` or `acc ^ ch_uint<N>(i)`), the generated Verilog literal's hex digit count **MUST** fit in `N` bits when written as `N'h<value>`. The implementation **MUST** mask `value` to `width` bits inside `verilogwriter::get_literal_str` before formatting the literal: `value_masked = value & ((1ULL << width) - 1)` (with width=0 returning 0, width>=64 returning the original value). This applies to all binary operations (XOR, ADD, SUB, MUL, AND, OR, SHL, SHR, etc.) RHS slots where the operand is a literal, and to the LHS/RHS slots of `print_concat` where at least one side is a literal.

The legacy 1-argument `verilogwriter::get_literal_str(const sdata_type &val)` signature is preserved unchanged — the fix is purely internal mask arithmetic. Empirical evidence (`include/ast/ast_nodes.h:litimpl` constructs with `value.bitwidth()`) shows `lit_node->size() == val.bv_.size()`, so the existing single-argument API already receives the correct target width via `val.bv_.size()`.

#### Scenario: ch_uint<8>(256) produces 8'h0 literal (not 8'h100)

- **WHEN** user executes `ch_uint<8> a(256_d);` or equivalent `auto a = ch_uint<8>(256);` in an active context
- **AND** the codegen emits the assign statement for `a`
- **THEN** the literal **MUST** be `8'h0` (i.e., `256 & 0xFF == 0`)
- **AND** **MUST NOT** be `8'h100` (which has 3 hex digits, exceeding 8-bit capacity and triggers Verilator `%Error: Too many digits for 8 bit number`)
- **NOTE**: `8'h100` is NOT a "valid SystemVerilog 2017 truncation" — Verilator treats it as a hard syntax error, not a width-truncation warning. `-Wno-fatal` does not rescue this error.

#### Scenario: ch_uint<8>(999) produces 8'he7 literal (TC-07 depth=1000 critical case)

- **WHEN** user executes `auto lit = ch_uint<8>(999);` or the XOR chain accumulation `acc = acc ^ ch_uint<8>(i);` where `i == 999`
- **AND** codegen prepares the RHS literal for `acc`
- **THEN** the literal **MUST** be `8'he7` (i.e., `999 & 0xFF == 231 == 0xE7`)
- **AND** **MUST NOT** be `8'h3e7` (the bug state — Verilator rejects with `%Error: Too many digits for 8 bit number`)
- **AND** the resulting TC-07 depth=1000 verilog file **MUST** parse under `verilator --lint-only -Wno-fatal` without any `%Error: Too many digits` message

#### Scenario: ch_uint<8>(0) and ch_uint<8>(1) remain unchanged (no mask regression)

- **WHEN** user executes `ch_uint<8> a(0);` or `ch_uint<8> a(1);`
- **THEN** the literal **MUST** remain `8'h0` and `8'h1` respectively
- **AND** the mask implementation **MUST NOT** alter values already within `[0, 2^width)`

#### Scenario: mask behavior at width boundary

- **WHEN** codegen processes a literal of `width=0` (defensive, should not occur in practice)
- **THEN** the implementation returns `8'h0` (or appropriate zero-valued literal) without undefined behavior from `(1ULL << 0) - 1`
- **WHEN** codegen processes a literal of `width >= 64` (ADR-035 64-bit cap)
- **THEN** the implementation preserves the original value without mask truncation (since `uint64_t` cannot represent the full `1ULL << 64` mask)

#### Scenario: TC-07 depth=1000 XOR chain verilog passes verilator --lint-only

- **WHEN** `tests/benchmark/perf_main.cpp` `Tc07XorChain` compiles with `depth=1000` and generates Verilog
- **AND** Verilator parses the output file via `verilator --lint-only -Wno-fatal`
- **THEN** parsing succeeds with exit code 0
- **AND** the output contains no `%Error: Too many digits for N bit number` message
- **NOTE**: This was the primary symptom of the bug pre-fix (verified: `%Error: top.v:3542:49: Too many digits for 8 bit number: '8'h109'`). After the mask fix, all 1000 XOR nodes generate literals with hex digit counts fitting in 8 bits.
