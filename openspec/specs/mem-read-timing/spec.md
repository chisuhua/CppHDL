# mem-read-timing Specification

## Purpose

定义内存读端口（`ch_mem::aread` / `ch_mem::sread`）在生成 Verilog 与 Simulator 中的时序语义，并明确两者间的已知 drift（reg-driven address skew、same-cycle write+read race）。该能力保证下游用户对生成的 Verilog 与 Simulator 行为有可预期、可追溯的契约。

In English: Defines the timing semantics of `ch_mem::aread` (async_read, 0-cycle combinational in Verilog) and `ch_mem::sread` (sync_read, 1-cycle registered in Verilog), and documents the known simulator↔Verilog drift that the ChipForge dmem design tolerates via a downstream register.
## Requirements
### Requirement: aread emits combinational assign in Verilog

The `ch_mem::aread` (async_read) port SHALL emit `assign proxy = mem[addr];`（组合读，0-cycle 延迟）作为生成 Verilog 中的输出。

#### Scenario: aread Verilog codegen

- **WHEN** 用户调用 `mem.aread(addr, name)` 且端口有数据代理输出
- **THEN** 生成的 Verilog SHALL 包含 `assign <proxy_name> = <mem_name>[<addr_name>]; // Comb read <proxy_name>`

### Requirement: sread emits always_ff in Verilog

The `ch_mem::sread` (sync_read) port SHALL emit `always_ff @(posedge default_clock or posedge default_reset)`（寄存读，1-cycle 延迟）作为生成 Verilog 中的输出。

#### Scenario: sread Verilog codegen

- **WHEN** 用户调用 `mem.sread(addr, enable, name)` 且端口有数据代理输出
- **THEN** 生成的 Verilog SHALL 包含 `always_ff ... <proxy_name> <= <mem_name>[<addr_name>];` 块，且包含复位分支清除代理

### Requirement: Simulator classifies mem read ports as sequential

The Simulator SHALL 将 `type_mem_read_port`（包含 sync 与 async 两类）放入 `sequential_instr_list_`，并在时钟高电平阶段的 `eval_sequential()` 中求值（`src/simulator_init.cpp:116-121`）。

#### Scenario: aread 与 sread 在 Simulator 中行为

- **WHEN** 在 tick 前设置 `addr_async` / `addr_sync` 输入，然后调用 `simulator.tick()`
- **THEN** 两类端口的代理在 tick 内同步更新；tick 末尾的 `eval_combinational()`（`src/simulator.cpp:328`）将代理传播到输出端口
- **THEN** 在 Simulator 中 `get_port_value(rdata_async)` 与 `get_port_value(rdata_sync)` 在 tick 调用后均可见新值（观察端行为相同）

### Requirement: known simulator-vs-Verilog drift is documented

The following drift between Simulator 与生成 Verilog SHALL 在 `docs/simulation/ARCHITECTURE.md` 的 "Memory read port timing" 小节和 root `AGENTS.md` CONVENTIONS 中明确记录：

#### Scenario: Register-driven read address skew

- **WHEN** 读地址由寄存器驱动（FIFO `raddr` 模式）
- **THEN** Verilog 中 `assign` 立刻反映 `mem[new_addr]`（0-cycle）
- **THEN** Simulator 中代理在下一个 posedge 才更新 `mem[addr_at_posedge]`（有效 1-cycle 滞后）

#### Scenario: Same-cycle write+read race

- **WHEN** `we=1` 在同一 tick 写 `mem[X]` 且 `aread` 同时读 `mem[X]`
- **THEN** Simulator aread 返回写前值
- **THEN** Verilog `assign` 在 NBA 更新后返回写后值
- **THEN** 该差异 SHALL NOT 触发任何回归 fail（设计意图：由下游 register 提供 1-cycle 依赖，如 ChipForge CPU `reg_file <<=`）

### Requirement: aread combinational assign emission has a reference-based regression fence

The codebase SHALL provide at least one Catch2 `TEST_CASE` that pins the `ch_mem::aread` combinational `assign` emission in generated Verilog. The cleanup change `2026-10-11-cleanup-250198a-aread-combinational-debt` delivered exactly such a test:

- **NAME**: `aread Verilog codegen emits assign (no always_ff)`
- **LOCATION**: `tests/test_mem_comb_read.cpp` (TEST_CASE at lines 216-277 as of issue #27)
- **CATCH2 TAG**: `[mem][comb-read][codegen]` (current implementation) — alternately `[mem][aread][golden]` is accepted per future migration

This requirement delegates implementation to that test; any future refactor that renames or removes that test MUST replace it with an equivalent test (same assertions, same coverage scope, same tag-either side of the or).

#### Scenario: Golden Verilog emission pin is present and effective

- **WHEN** a CppHDL design instantiates `ch_mem::aread(addr)` and the test suite runs `ch::toVerilog(...)` on its context
- **THEN** running `./build/tests/test_mem_comb_read` with the codegen tag SHALL find and pass at least one `TEST_CASE`
- **THEN** that `TEST_CASE` MUST assert the aread proxy is driven by `assign ... = mem[<addr>]` (with the "Comb read" comment marking the line) and SHALL NOT be assigned within any `always_ff` block
- **THEN** running that `TEST_CASE` against a hypothetical reversion of the codegen to `always_ff` MUST cause it to fail (proving the test is non-vacuous)
- **THEN** the test SHALL NOT depend on file-system state outside `std::filesystem::temp_directory_path()` (portable across Linux/macOS/Windows; matches cleanup's `unique_temp_verilog_path()` helper contract)

#### Scenario: Migration path when cleanup test is renamed

- **WHEN** a future refactor renames the existing cleanup `#3` test from `[mem][comb-read][codegen]` to a new tag (e.g. `[mem][aread][golden]`), or removes it entirely
- **THEN** the replacement test MUST keep the same assertion scope (assign emission + no always_ff + cross-tag agnostic)
- **THEN** the replacement test MAY use either `[mem][aread][golden]` or `[mem][comb-read][codegen]` as its Catch2 tag

### Requirement: same-cycle write+aread race has an executable pre-write regression fence

The codebase SHALL provide at least one Catch2 `TEST_CASE` that pins the Simulator's chosen semantic for the known race (spec R4 second Scenario): `aread` returns the **pre-write** value when `we=1` writes the same address in the same `tick()` input phase. This requirement is an **executable layer** lock; without it, future Simulator "fixes" that change the timing could silently drift dmem by 1 cycle without any test failing.

#### Scenario: Same-cycle write+aread race test exists

- **WHEN** a CppHDL memory port is exercised by a testbench that sets `we=1`, `wdata=Y`, `waddr=X`, and `aread_addr=X` within the **same** `tick()` input phase
- **THEN** the test suite SHALL contain at least one Catch2 `TEST_CASE` tagged `[mem][aread][race]` in `tests/test_mem_comb_read.cpp` (file location chosen for proximity to the other `[mem]` tests and to share the `CombReadFixture`)
- **THEN** that `TEST_CASE` MUST assert `rdata_async == mem_initial_value_at_X` (pre-write value, locked per spec R4 third bullet "Simulator returns pre-write value")
- **THEN** running the test against a hypothetical refactor that lets `aread` observe the same-tick `we=1` write (i.e., returns post-write) MUST cause the `TEST_CASE` to fail
- **THEN** any future Simulator "fix" to this race behavior MUST be preceded by a deliberate spec change to R4 second Scenario (not just a code-level adjustment), per project zero-debt policy

#### Scenario: Test location and fixture reuse

- **WHEN** the new race test is added to `tests/test_mem_comb_read.cpp`
- **THEN** it SHALL reuse the existing `CombReadFixture` (defined at `tests/test_mem_comb_read.cpp:62-108`) without modification — the fixture's `addr_sync` input is shared between sread address and write address, which is sufficient for the race scenario
- **THEN** the test SHALL place its inputs (`we=1`, `wdata=Y`, `waddr=X`, `aread_addr=X`) in a single `tick()` block so that the same-cycle semantics are unambiguously exercised

