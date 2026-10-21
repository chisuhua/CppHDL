# Capability: mem-read-timing

## RENAMED Requirements

- FROM: `### Requirement: Simulator classifies mem read ports as sequential`
- TO: `### Requirement: Simulator classifies async mem read ports as combinational`

- FROM: `### Requirement: same-cycle write+aread race has an executable pre-write regression fence`
- TO: `### Requirement: same-cycle write+aread race has an executable regression fence`

## MODIFIED Requirements

### Requirement: Simulator classifies async mem read ports as combinational

The Simulator SHALL 将 `type_mem_read_port` 的 **async** 子类（`ch_mem::aread`）
放入 `combinational_instr_list_`，在 `eval_combinational()` 中求值（0-cycle，
与生成的 Verilog `assign proxy = mem[addr];` 对齐）；
`sync` 子类（`ch_mem::sread`）SHALL 继续留在 `sequential_instr_list_`，
在时钟高电平阶段的 `eval_sequential()` 中求值（1-cycle，与 `always_ff` 对齐）。

**为何 async 与 sync 必须分开**：两者在生成的 Verilog 里本就是**两种不同的电路**
（`assign` vs `always_ff`）；Simulator 用同一条求值路径服务两者是 drift 的根源。
`sread` 的 1-cycle 语义被 RV32I 的 `pc_lag` 对齐依赖 → MUST NOT 改动。

#### Scenario: aread 在地址变化的同一拍反映新值

- **WHEN** `addr_async` 在某个 tick 的输入阶段被改为新地址
- **THEN** 该 tick 结束后的 `get_port_value(rdata_async)` SHALL 是**新地址**的值
  （0-cycle，与 Verilog `assign` 一致）
- **AND** `get_port_value(rdata_sync)` SHALL 仍是**旧地址**的值（1-cycle）

#### Scenario: sread 保持 1-cycle 语义

- **WHEN** `addr_sync` 在某个 tick 的输入阶段被改为新地址
- **THEN** 该 tick 结束后的 `get_port_value(rdata_sync)` SHALL 仍是旧地址的值
- **AND** 下一次 tick 之后 SHALL 是新地址的值
- **AND** RV32I 的 `pc_lag` / FLUSH 对齐 MUST NOT 受影响（imem MUST 继续用 `sread`）

### Requirement: known simulator-vs-Verilog drift is documented

The following drift between Simulator 与生成 Verilog SHALL 在
`docs/simulation/ARCHITECTURE.md` 的 "Memory read port timing" 小节和 root
`AGENTS.md` CONVENTIONS 中明确记录：

#### Scenario: Register-driven read address skew（**本 change 后消除**）

- **WHEN** 读地址由寄存器驱动（FIFO `raddr` 模式）
- **THEN** Verilog 中 `assign` 立刻反映 `mem[new_addr]`（0-cycle）
- **THEN** Simulator 中 async read port 的代理同样立刻反映 `mem[new_addr]`
  （0-cycle，与 Verilog 一致）
- **AND** 该 skew 的文档条目 MUST 被移除或改写为「已消除」

#### Scenario: Same-cycle write+read race

- **WHEN** `we=1` 在同一 tick 写 `mem[X]` 且 `aread` 同时读 `mem[X]`
- **THEN** Simulator aread 返回写**后**值（与 Verilog `assign` 在 NBA 更新后一致）
- **THEN** 该差异 SHALL NOT 触发任何回归 fail
- **AND** 下游若依赖「写前值」语义（例如 ChipForge dmem 的 `reg_file <<=`），
  MUST 通过**下游寄存器**提供 1-cycle 依赖，而不是依赖 Simulator 的写前值

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
  （`tests/test_mem_comb_read.cpp:62-108`），MUST NOT 为其新建 fixture
- **THEN** 测试 MUST 把 `we=1` / `wdata=Y` / `waddr=X` / `aread_addr=X`
  放在**同一个** `tick()` 块内，使同拍语义无歧义
