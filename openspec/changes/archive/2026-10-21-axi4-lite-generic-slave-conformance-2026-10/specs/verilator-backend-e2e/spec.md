# Capability: verilator-backend-e2e

## Purpose

定义 VerilatorBackend 的端到端行为契约（详见主 spec
`openspec/specs/verilator-backend-e2e/spec.md`）。

本 delta 由 `axi4-lite-generic-slave-conformance-2026-10` 引入：
`AxiLiteSlave` 收敛到「AW→W 相位分离 + 响应保持」规范形态后，
`WriteReadReg0ThroughBackend` 的写事务时序与写拍后的信号期望 MUST 同步更新。

## MODIFIED Requirements

### Requirement: VerilatorBackend MUST 正确传播 Vtop 输出到 Simulator 的 data_map

The Verilator backend (`src/core/verilator_backend.cpp`) SHALL propagate Vtop output field values — both combinational wires (`assign ...` expressions) and sequential register updates (`always_ff @(posedge clk)` blocks) — back to the Simulator's `data_map_` after each `eval()` cycle. The dispatch sequence MUST allow Verilator's edge-detection infrastructure (`VlClockSig` / `VlClockReceiver`) to function correctly by NOT round-tripping the clock field through `data_map_` during `eval_sequential()`.

#### Scenario: Combinational wire reflected in data_map after single tick

- **WHEN** a port is driven by a combinational output expression in the generated Verilog
  （`assign ...` 表达式，其输入 MAY 含寄存器）
- **AND** the upstream inputs are set via `sim.set_input_value(...)`
- **AND** `sim.tick()` is called once
- **THEN** the corresponding entry in `sim.data_map()` SHALL match the assigned value
- **AND** the existing `tests/test_axi_lite_verilator_e2e.cpp::WriteReadReg0ThroughBackend` test case **MUST** see `bvalid_v == 1` after the **W-beat tick**（`WVALID` 被拉高且 `WREADY` 拉高的那一拍之后）
- **AND** that test case MUST see `awready_v == 0` after the W-beat tick
  （`AxiLiteSlave` 收敛到规范形态后，AW 握手即置 `busy`，`awready` MUST 为 0；
  与 `awready == 1` 的旧期望互斥，故本 Scenario 的期望以
  `axi-slave-conformance` capability 为准）
- **AND** that test case MUST assert the AW→W phase separation
  （AW 一拍、W 另一拍），MUST NOT 依赖「AW 与 W 同拍」

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
  （AW 一拍、W 另一拍 —— `AxiLiteSlave` 的 AW→W 相位分离）
- **AND** a subsequent read transaction reads `addr=0x00`
- **THEN** `sim.data_map()[io().rdata.id()]` SHALL equal `0xDEADBEEF`
- **AND** the existing `tests/test_axi_lite_verilator_e2e.cpp::WriteReadReg0ThroughBackend` test case **MUST** pass with `REQUIRE(rdata_val == 0xDEADBEEF)` (currently an `INFO` log only)
