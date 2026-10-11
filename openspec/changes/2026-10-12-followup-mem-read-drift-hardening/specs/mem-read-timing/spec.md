# Capability: mem-read-timing (R5 addition)

## Purpose

追加到既有 `mem-read-timing` capability，把已文档化的 `aread` combinational `assign` 发射契约（既有 R1）与 known simulator↔Verilog drift 契约（既有 R4）从"文档层契约"提升到"执行层 fence"：

- **R5.first（柔性契约）**：把 cleanup 已有的 `[mem][comb-read][codegen]` `TEST_CASE` `aread Verilog codegen emits assign (no always_ff)` 引用为本契约的实现——避免与 cleanup 测试重复造 fence；如未来 cleanup 测试被改名/移除，spec 仍要求等效 fence 接力
- **R5.second（硬性契约）**：新增至少一条带 `[mem][aread][race]` tag 的 `TEST_CASE`，覆盖**同 cycle** write+aread 返回 pre-write 值的执行层语义（spec R4 第二个 Scenario）

本 R5 不修改既有 R1-R4 任何文字。

In English: Two-tier executable-layer contract for `mem-read-timing`. First requirement **references** (does not duplicate) the existing cleanup `[mem][comb-read][codegen]` test as the golden Verilog emission fence; second requirement **requires new** `[mem][aread][race]` test to fence the same-cycle write+read race semantic that has been documented in R4 but never pinned at the executable layer.

## ADDED Requirements

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
