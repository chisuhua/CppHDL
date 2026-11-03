## ADDED Requirements

### Requirement: interpreter 的赋值 MUST 保持目标节点的声明宽度

`interpreter` 的每条指令在写目标槽位时 MUST 保持该节点**声明的位宽**
（`node->size()`）—— 超出部分**截断**、不足部分**零扩展**
⇒ MUST NOT 让目标槽位的宽度变成源操作数的宽度。

⇒ 判据：对**每个**写槽位的实现，`eval` 前后 `data_map[node_id].bitwidth()`
MUST 恒等于 `node->size()`。

#### Scenario: `mux` 槽位的宽度 MUST NOT 随被选分支变化

- **WHEN** 一个 `mux` 的真分支宽 `N`、假分支宽 `M`（`M < N`），且**较窄**的分支被选中
- **THEN** 该 `mux` 节点槽位的位宽 MUST 仍为 `N`
- **AND** 较窄分支的值 MUST 被**零扩展**到 `N` 位（高位为 `0`）
- **NOTE**: `bitvector::operator=` 会 **RESIZE** 目标 ⇒ ⇒ 窄分支被选中时槽位被压窄
         ⇒ ⇒ 下游按声明宽度取位（`bit_select(m, 2)`）会**越界**。修法 = `assign_truncate`
         （`instr_output` / `instr_proxy` / `instr_reg` 的既有惯例）。

#### Scenario: 该宽度契约 MUST 有可判别的判据（含「值层面不可判别」的情形）

- **WHEN** 为上述契约写判据
- **THEN** 判据 MUST 覆盖「宽分支 → 窄分支」后**高位归零**（抓「不清高位」）
- **AND** MUST 覆盖窄分支被选中时**高位取位不越界**
- **NOTE**: 「改回拷贝赋值」这一类回归在**值层面不可判别**（压窄只丢已为 0 的高位
         ⇒ ⇒ 值逐位等价）⇒ ⇒ 其唯一可观测量是**越界错误日志** ⇒ ⇒
         判据 MUST 用输出的错误串（`FAIL_REGULAR_EXPRESSION`）兜住这一格。

#### Scenario: 该契约 MUST 在两后端一致

- **WHEN** 上述判据运行
- **THEN** 它 MUST 是**双后端**（interpreter + verilator）用例
- **AND** 两半边 MUST 给出相同结果
- **NOTE**: 该缺陷**只在 interpreter 侧**存在（Verilog 的 `assign` 天然保持声明宽度）
         ⇒ ⇒ 这正是双后端 parity 的价值所在。
