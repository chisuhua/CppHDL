# Capability: mem-read-timing

## MODIFIED Requirements

### Requirement: aread combinational assign emission has a reference-based regression fence

The codebase SHALL provide at least one Catch2 `TEST_CASE` that pins the `ch_mem::aread` combinational `assign` emission in generated Verilog. The cleanup change `2026-10-11-cleanup-250198a-aread-combinational-debt` delivered exactly such a test:

- **NAME**: `aread Verilog codegen emits assign (no always_ff)`
- **LOCATION**: `tests/test_mem_comb_read.cpp`（**按 TEST_CASE 名称引用**，MUST NOT 写行号 —— 行号会随同文件的新增用例漂移）
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

### Requirement: same-cycle write+aread race has an executable regression fence

The codebase SHALL provide at least one Catch2 `TEST_CASE` that pins the
Simulator's chosen semantic for the known race (spec R4 second Scenario):
`aread` returns the **post-write** value when `we=1` writes the same address
in the same `tick()` input phase. This requirement is an **executable layer**
lock; without it, future Simulator "fixes" that change the timing could
silently drift dmem by 1 cycle without any test failing.

⚠️ 该 Requirement 的**钉住值从「写前值」反转为「写后值」** —— 这是本 change
对既有契约的**显式推翻**，理由：写前值是「Simulator 与 Verilog 不一致」的产物
（Verilog 的 `assign` 在 NBA 更新后返回写后值），把它钉死等于把 drift 制度化。
下游若需要写前值，MUST 用**下游寄存器**显式提供 1-cycle 延迟。

#### Scenario: Same-cycle write+aread race test exists

- **WHEN** 一个 CppHDL 内存端口被测试台在同一 `tick()` 输入阶段设置
  `we=1`、`wdata=Y`、`waddr=X`、`aread_addr=X`
- **THEN** 测试套件 SHALL 至少包含一个 Catch2 `TEST_CASE`
  （tag `[mem][aread][race]`，位于 `tests/test_mem_comb_read.cpp`）
- **THEN** 该 `TEST_CASE` MUST 断言 `rdata_async == mem_post_write_value_at_X`
  （写后值）
- **THEN** 运行该测试对「让 `aread` 观察到同拍写入（即返回写前值）」的假想改动
  MUST 导致该 `TEST_CASE` 失败（证明测试非空转）
- **THEN** 任何未来对该竞态行为的改动 MUST 先有本 Requirement 的**显式变更**，
  而不是仅做代码级调整（per project zero-debt policy）

#### Scenario: Test location and fixture reuse

- **WHEN** 该竞态测试位于 `tests/test_mem_comb_read.cpp`
- **THEN** 它 SHALL 复用既有的 `CombReadFixture`
  （**按 fixture 类名引用**，MUST NOT 写行号 —— 该 fixture 会随新增用例而
  增删端口，行号必然漂移），MUST NOT 为其新建 fixture
- **THEN** 测试 MUST 把 `we=1` / `wdata=Y` / `waddr=X` / `aread_addr=X`
  放在**同一个** `tick()` 块内，使同拍语义无歧义
- **AND** 该竞态用例的读地址 MUST 保持**输入驱动**（与既有实现一致）——
  读端口的**寄存器驱动**语义由本 Requirement 的第一个 Scenario 单独覆盖
