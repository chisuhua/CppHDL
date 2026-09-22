# verilator-backend-e2e Specification

## Purpose
TBD - created by archiving change adr-035-phase-3-complete. Update Purpose after archive.
## Requirements
### Requirement: dlopen 真实 Vtop 与 extern "C" 符号解析
`VerilatorBackend::initialize()` 在 `verilator --cc --build` 成功生成 `obj_dir/Vtop` 后，**MUST** 真实 `dlopen` 该二进制并 `dlsym` 解析 5 个符号 (`new_Vtop`, `eval_Vtop`, `final_Vtop`, `delete_Vtop`, `set_input_Vtop`, `get_output_Vtop`, `get_field_ptr_Vtop`)。任一符号缺失 **MUST** `dlclose` + 返回 false，且 `CHWARN` log 缺失符号名。

#### Scenario: 7 符号全部成功解析
- **WHEN** `dlopen obj_dir/Vtop` 成功且 7 个 dlsym 都返回非 nullptr
- **THEN** `factory()` 创建 Vtop 实例，top_instance_ 非 nullptr，eval_fn_/final_fn_ 非 nullptr
- **THEN** `build_port_access_table()` 调 `get_field_ptr_Vtop` 填充所有 port 的 field_ptr
- **THEN** 第二次 `initialize()` 复用 dl_handle_ 不重新 dlopen

#### Scenario: 符号缺失优雅失败
- **WHEN** dlsym 任意一个返回 nullptr
- **THEN** `dlclose(dl_handle_)` + `dl_handle_ = nullptr` + 返回 false
- **THEN** `CHWARN("dlsym missing symbol: factory=%p eval=%p ...", ...)` log 指针地址

### Requirement: data_map ↔ Vtop 双向同步 (Phase 3.3 核心契约)
`VerilatorBackend::sync_inputs_to_vtop()` 与 `sync_outputs_from_vtop()` **MUST** 真正遍历 `port_access_` 表，把 `data_map_->at(id)` 写入 Vtop 字段、从 Vtop 字段读回 `data_map_->at(id)`。**MUST NOT** 仅 `CHINFO` log 而不读写数据。

#### Scenario: input field 写入生效
- **WHEN** `data_map_->at(id) = new_value`，然后 `eval_combinational()` 被调用
- **THEN** Vtop 内部对应 field_ptr 指向的位在 eval 后反映 new_value（通过 `get_output_Vtop` 读回能验）

#### Scenario: output field 读回正确
- **WHEN** Vtop 内部某个 output field 在 eval 后产生新值
- **THEN** `sync_outputs_from_vtop()` 把该新值写回 `data_map_->at(id)`

#### Scenario: 多 port 顺序与 SpinalHDL 一致
- **WHEN** port_access_ 有 N 个 entry（按 port id 升序）
- **THEN** sync_inputs 顺序写入 id=0, 1, ..., N-1（与 SpinalHDL VerilatorBackend `ISignalAccess*[]` 模式一致）

### Requirement: 3-eval/tick 时钟模型 (Phase 3.4 契约)
1 cycle **MUST** 等于 3 次 eval 调用，对应 ADR-009 仿真求值顺序：`eval_combinational → eval → eval_combinational`。

#### Scenario: 1 cycle 后 sequential 信号翻转
- **WHEN** 一个 4-bit counter 设计跑 Verilator sim 1 cycle (3 evals)
- **THEN** counter_value 增 1（与解释器 cycle-1 后 counter_value == 1 一致）

#### Scenario: default_clock 字段在 eval 前翻转
- **WHEN** `eval_sequential()` 被调用
- **THEN** 第一次 sync_inputs 前 Vtop.default_clock 字段被置 1 (posedge 模拟)
- **THEN** `eval_fn_()` 调用后 Vtop.default_clock 字段被置 0（falling edge）

#### Scenario: clock_node_id_ 未命中时跳过
- **WHEN** context 没有 type_clock 节点
- **THEN** clock_node_id_ == UINT32_MAX，eval_sequential 仅 sync + eval 不翻转 default_clock

### Requirement: SHA-1 cache hit 不触发 verilator 子进程 (Phase 3.5 契约)
第二次 `initialize()` 对同一 verilog source **MUST** 检测 `~/.cache/cpphdl/verilator/<key>/Vtop` 存在并跳过 `verilator --cc --build` 调用。

#### Scenario: cache miss 触发编译
- **WHEN** cache_path_for_key() 返回路径不存在
- **THEN** invoke_verilator() 被调用，`verilator --cc --build` 子进程 spawn
- **THEN** 编译成功后 Vtop 拷贝到 cache 路径

#### Scenario: cache hit 跳过编译 (< 100ms)
- **WHEN** 同一 verilog source 第二次 initialize()，cache 路径存在
- **THEN** invoke_verilator() **NOT** 被调用
- **THEN** dlopen_top() 直接 load cached Vtop
- **THEN** 整个 initialize() 路径耗时 < 100ms

### Requirement: VCD trace dump 写出有效文件 (Phase 3.6 契约)
`enable_vcd(true)` 后每个 cycle 写入 `obj_dir/sim.vcd`，文件 **MUST** 包含所有 type_input/type_output/type_reg 信号的 VCD 头 + 时间戳 + 值。

#### Scenario: VCD header 写入
- **WHEN** `enable_vcd(true)` + `initialize()` + 0 cycle
- **THEN** `sim.vcd` 文件存在，包含 `$timescale`, `$scope module top`, `$var` declarations

#### Scenario: cycle 信号 dump
- **WHEN** 跑 100 cycle
- **THEN** `sim.vcd` 文件包含 100 个 `#<time>` 时间戳 + 信号值行
- **THEN** 文件 size > 1KB

### Requirement: e2e 端到端与解释器结果一致 (Phase 7.5 契约)
Verilator sim 后端跑 `samples/counter.cpp` 与 ChipForge `tests/cpu/test_cpu_chmem_vendored_elf.cpp` **MUST** 产生与 CppHDL 解释器**逻辑等价**的结果（counter_value 相等、tohost=1、cycle 数 ±10%）。

#### Scenario: counter sim 50 cycle 一致
- **WHEN** `samples/counter.cpp` 跑 Verilator sim 50 cycle (150 evals)
- **THEN** counter_value == 50（与 CppHDL 解释器跑 50 cycle 的 counter_value 一致）

#### Scenario: ChipForge 5-stage CPU vendored ELF tohost=1
- **WHEN** `rv32ui-p-add` ELF 跑 Verilator sim 50 cycle
- **THEN** DMem[0x1000] (tohost offset) == 1
- **THEN** cycle 数与 CppHDL sim 跑同 ELF 的 cycle 数相差 ≤ 10%

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

### Requirement: VerilatorBackend MUST 正确传播 Vtop 输出到 Simulator 的 data_map

The Verilator backend (`src/core/verilator_backend.cpp`) SHALL propagate Vtop output field values — both combinational wires (`assign ...` expressions) and sequential register updates (`always_ff @(posedge clk)` blocks) — back to the Simulator's `data_map_` after each `eval()` cycle. The dispatch sequence MUST allow Verilator's edge-detection infrastructure (`VlClockSig` / `VlClockReceiver`) to function correctly by NOT round-tripping the clock field through `data_map_` during `eval_sequential()`.

#### Scenario: Combinational wire reflected in data_map after single tick

- **WHEN** a port is driven by a pure `assign` expression in the generated Verilog (no register in the path)
- **AND** the upstream inputs are set via `sim.set_input_value(...)`
- **AND** `sim.tick()` is called once
- **THEN** the corresponding entry in `sim.data_map()` SHALL match the assigned value (e.g., `awready == 1` when `awvalid == 1 && busy == 0`)
- **AND** the existing `tests/test_axi_lite_verilator_e2e.cpp::WriteReadReg0ThroughBackend` test case **MUST** see `awready_v == 1`, `wready_v == 1`, and `bvalid_v == 1` after the write tick

#### Scenario: Sequential register reflected in data_map after N ticks

- **WHEN** a port is driven by an `always_ff @(posedge clk)` block in the generated Verilog
- **AND** the upstream inputs are set via `sim.set_input_value(...)`
- **AND** `sim.tick(N)` is called for `N >= 1`
- **THEN** after the `N`-th tick the corresponding entry in `sim.data_map()` SHALL reflect the register's value after `N` clock edges

#### Scenario: ch_uint<32> counter increments to 50 under VerilatorBackend

- **WHEN** the design is `ch::ch_reg<ch_uint<32>> reg(0_d); reg->next = reg + 1_d; io().out = reg;` (CounterFixture<32> per `tests/test_verilator_e2e_harness.cpp:60-77`)
- **AND** `sim.tick(50)` is called via the Verilator backend
- **THEN** `sim.data_map()[io().out.id()]` SHALL equal `50`
- **AND** the existing `tests/test_verilator_e2e_harness.cpp:128-167` test case **MUST** pass with `REQUIRE(actual == 50)` (downgrade from current `CHECK(actual == 50)`)

#### Scenario: ch_uint<4> counter wraps to 50 % 16 under VerilatorBackend

- **WHEN** the design is `ch::ch_reg<ch_uint<4>> reg(0_d); reg->next = reg + 1_d;` (CounterFixture<4>)
- **AND** `sim.tick(50)` is called
- **THEN** `sim.data_map()[io().out.id()]` SHALL equal `50 % 16 == 2`
- **AND** the existing `tests/test_verilator_e2e_harness.cpp:170-208` test case **MUST** pass with `REQUIRE(actual == 50 % 16)`

#### Scenario: AXI4-Lite write-then-read data integrity under VerilatorBackend

- **WHEN** the design wraps `ch::ch_module<AxiLiteSlave<32, 32, 4>>` (`tests/test_axi_lite_verilator_e2e.cpp::AxiLiteTop`)
- **AND** a write transaction writes `0xDEADBEEF` to `addr=0x00` (reg0)
- **AND** a subsequent read transaction reads `addr=0x00`
- **THEN** `sim.data_map()[io().rdata.id()]` SHALL equal `0xDEADBEEF`
- **AND** the existing `tests/test_axi_lite_verilator_e2e.cpp::WriteReadReg0ThroughBackend` test case **MUST** pass with `REQUIRE(rdata_val == 0xDEADBEEF)` (currently an `INFO` log only)

### Requirement: VerilatorBackend MUST 实施 3 步时钟序列在 eval_sequential

The Verilator backend's `eval_sequential()` SHALL execute a 3-step clock toggle sequence per Verilator 5.x requirement to correctly trigger `always_ff @(posedge clk)` blocks: (1) `clk=0` + `eval()`, (2) `clk=1` + `eval()` (posedge triggers always_ff), (3) `clk=0` + `eval()` (cleanup, prevents double-trigger). The clock and reset Vtop pointers SHALL be saved during `build_port_access_table()` and accessed directly (NOT round-tripped through `data_map_`) to preserve Verilator's `__Vclklast__` edge-detection state.

#### Scenario: eval_sequential performs 3-step clock toggle

- **WHEN** `VerilatorBackend::eval_sequential()` is invoked
- **THEN** it SHALL execute exactly 3 calls to `eval_fn_()` interleaved with `clk=0` / `clk=1` / `clk=0` Vtop field writes
- **AND** `sync_inputs_to_vtop()` SHALL skip the clock and reset fields during this sequence (`exclude_clock=true` parameter)

#### Scenario: sync_outputs_from_vtop runs after eval_sequential

- **WHEN** `VerilatorBackend::eval_sequential()` returns
- **THEN** `sync_outputs_from_vtop()` SHALL have been called to refresh all user-output fields from Vtop into `data_map_`
- **AND** the clock and reset fields SHALL be excluded from this sync (to avoid round-trip)

