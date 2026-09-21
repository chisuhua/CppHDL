## ADDED Requirements

### Requirement: codegen 字面量宽度必须匹配目标 wire 宽度

The Verilog code generator **MUST** emit literal RHS values with a width prefix that matches the target wire's declared width. Specifically: for any `ch_uint<N>` expression involving a literal operand (e.g., `ch_uint<N>(int_value)` or `acc ^ ch_uint<N>(i)`), the generated Verilog literal **MUST** use `N`'h...` prefix, **NOT** the literal's `bv` storage width (typically 32 or 64 bits in CppHDL's `sdata_type`). This applies to all binary operations (XOR, ADD, SUB, MUL, AND, OR, SHL, SHR, etc.) and the LHS/RHS slots of `print_concat` where at least one side is a literal.

The implementation provides a 2-argument `verilogwriter::get_literal_str(uint32_t target_width, const sdata_type &val)` overload that takes the target width as the explicit first parameter. The legacy 1-argument overload is preserved with `[[deprecated("Use the 2-arg overload that takes target_width")]]` to prevent new callers from regressing this contract.

#### Scenario: ch_uint<8>(256) 产生 8'h100 字面量（不是 32'h100）

- **WHEN** 用户在 active context 中执行 `ch_uint<8> a(256_d);` 或等价 `auto a = ch_uint<8>(256);`
- **AND** 生成的 Verilog 模块中包含 `a` 的 assign 语句
- **THEN** 该 assign 语句 **MUST** 使用 `8'h100` 作为字面量（SystemVerilog 2017 字面量截断语义）
- **AND** **MUST NOT** 使用 `32'h100` 或 `64'h100`（这些会触发 Verilator 编译错误 "Too many digits for 8 bit number"）
- **NOTE**: `8'h100` 在 8-bit 上下文中表示值 `0x00`（低 8 位 = 0），符合 `ch_uint<8>(256)` 的语义（截断到 8 位）

#### Scenario: ch_uint<8>(999) 产生 8'h3E7 字面量（depth=1000 XOR chain 的关键 case）

- **WHEN** 用户执行 `auto lit = ch_uint<8>(999);` 或等价的 XOR 链累积场景 `acc = acc ^ ch_uint<8>(i);` 当 `i == 999`
- **AND** codegen 准备生成 assign RHS 字面量
- **THEN** 输出 **MUST** 是 `8'h3e7`（SystemVerilog 会截断 `0x3e7` = 999 到 8-bit 低 8 位 `0xe7` = 231）
- **AND** **MUST NOT** 是 `32'h3e7`（这是 bug 状态：Verilator 在 `--lint-only` / `--build` 阶段会拒绝 `8'h3e7` 写入 8-bit wire，报 "Too many digits for 8 bit number"）

#### Scenario: print_binary_op RHS 迁移到 2-arg 重载

- **WHEN** codegen 走 `verilogwriter::print_binary_op` 且 `rhs_node->type() == type_lit`
- **THEN** 输出 `rhs_name = get_literal_str(rhs_node->size(), lit_node->value())`
- **AND** **MUST NOT** 调用 1-arg `get_literal_str(lit_node->value())`（遗留的 deprecated 路径）
- **NOTE**: 用 `rhs_node->size()` 而非 `node->size()` —— RHS 字面量上下文必须匹配 RHS wire 宽度（ADD/SUB 等场景下结果节点可能因隐式 width-extend 比 RHS 宽）

#### Scenario: print_concat LHS/RHS 迁移到 2-arg 重载

- **WHEN** codegen 走 `verilogwriter::print_concat` 且 LHS 或 RHS 是 literal
- **THEN** 输出 `lhs_name = get_literal_str(node->size(), ...)` 和 `rhs_name = get_literal_str(node->size(), ...)`
- **AND** 整个 concat 的 assign **MUST** 通过 verilator `--lint-only` 解析（无 width mismatch 警告）

#### Scenario: 旧 1-arg API 仍可用但已 deprecated

- **WHEN** 任何代码调用 `get_literal_str(const sdata_type &val)`（1-arg 版本）
- **THEN** 编译器 **MUST** 发出 `-Wdeprecated-declarations` 警告
- **AND** 该函数仍可正常调用（不删除，保持向后兼容直到所有内部调用方迁移完毕）
- **AND** `src/codegen_verilog.cpp` TU 内通过 `set_source_files_properties(... COMPILE_OPTIONS "-Wno-deprecated-declarations")` 局部禁言（仅这一个 TU 不污染其他编译单元的警告状态）

#### Scenario: 64-bit 上限保持（ADR-035 wide-signal rejection）

- **WHEN** codegen 走 `get_literal_str(target_width, val)` 且 `target_width > 64`
- **THEN** 新实现仍用 `target_width` 作为字面量前缀（虽然 `std::hex << uint64_t` 会截断）
- **AND** `verilator_backend.cpp` 已在 ADR-035 强制：ch_in<ch_uint<N>> 中 N > 64 在 `initialize()` 时返回 false 并 CHERROR
- **AND** 因此 codegen 路径中 `target_width > 64` 实际上不会出现（防御性处理）
- **NOTE**: 未来如果 ADR 解除 64-bit 上限（ch_uint<256> verilator 支持），需新增多 limb 字面量格式（如 `{64'h..., 64'h..., 128'h...}`），独立 change 处理

#### Scenario: TC-07 depth=1000 XOR chain 通过 verilator --lint-only 解析

- **WHEN** `tests/benchmark/perf_main.cpp` 的 `Tc07XorChain` 用 depth=1000 编译并生成 Verilog
- **AND** Verilator 用 `verilator --lint-only -Wno-fatal` 解析输出文件
- **THEN** 解析成功，exit code 0
- **AND** 没有 `%Error: Too many digits for 8 bit number` 错误
- **NOTE**: 这是修复前必失败的场景（已实测：`%Error: top.v:3542:49: Too many digits for 8 bit number: '8'h109'`）。修复后所有 1000 个 XOR 节点的字面量宽度正确
