## ADDED Requirements

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