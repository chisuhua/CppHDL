# Capability: rv32i-isa-semantics

## ADDED Requirements

### Requirement: 冻结与 `flush` 同拍时 MUST 以**重定向优先**

「未知 opcode ⇒ 冻结」引入了本仓**第二个** stall 来源 ⇒ ⇒ 它与 `flush`（分支/跳转重定向）
**可以同拍** ⇒ ⇒ 此时 **MUST 以重定向优先**：

- 判据 MUST 形如 `pc_update_en = !rst & !(stall & !(flush & branch_valid))`
  ⇒ ⇒ **仅当确有重定向**（`flush & branch_valid`）才让 `flush` 优先。
- ID 级的 `flush` 优先级 MUST 与 PC 侧**一致**（`idex_bubble = flush | stall`）。
- **MUST NOT** 让 stall 把重定向冻住（那会丢 redirect，且下一拍 `valid = 0`
  ⇒ 冻结只活 1 拍 ⇒ 非法字被**静默**跳过）。
- 「`flush` 但无重定向」（`branch_valid = 0`）时 PC MUST 仍冻结（组件级既有契约）。

#### Scenario: 重定向 MUST 被兑现（taken 分支 + fall-through 非法字）

- **WHEN** EX 级有一条 taken 分支，同拍 ID 级持有其 fall-through（**错误路径**）上的非法字
- **THEN** PC MUST 更新为分支目标
- **AND** 分支目标处的指令 MUST 执行（其写回 MUST 出现）
- **AND** 非法字 MUST NOT 被执行（其 `rd` MUST NOT 收到写回）

#### Scenario: 非法字 MUST 在碰撞拍被点亮

- **WHEN** 上述碰撞发生
- **THEN** 操作码分类 MUST 在 ID 级持有该非法字的那一拍为 `2`（未知）
- **AND** `flush` 生效后 MUST 回落到 `0`（冻结域只覆盖正确路径）

#### Scenario: 正确路径的冻结仍是终态

- **WHEN** ID 级持有一条未知 opcode 指令，且**同拍无** `flush`
- **THEN** PC MUST 停在原地（不前进）
- **AND** 该指令 MUST NOT 进入 EX、MUST NOT 退休

#### Scenario: 组件级既有契约 MUST 保持

- **WHEN** `stall = 1` · `flush = 1` · `branch_valid = 0`（流水线内不可达的组件级场景）
- **THEN** PC MUST 冻结（**不改动**既有钉住的判据）
