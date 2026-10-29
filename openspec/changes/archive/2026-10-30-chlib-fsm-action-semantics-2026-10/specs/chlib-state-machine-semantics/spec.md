# Capability: chlib-state-machine-semantics

## MODIFIED Requirements

### Requirement: FSM 的寄存器与输出 MUST 有唯一写入点 / 驱动点

每个寄存器 MUST 有**唯一**的 `next` 写入点；每个输出 MUST **只**驱动一次。

⇒ ⇒ 各状态的候选值 MUST 用 `select`（按 `sm.is_in(...)` 多路选择）组合成
**单一表达式**，写在 `sm.build()` **之后**。

⇒ ⇒ **首选**是框架的**状态守卫贡献 API**（`sm.contrib(target, S, value)` +
`sm.resolve_hold(reg)` / `sm.resolve_or(target, default)`）：由框架把各状态的
候选值合成为**一条** select 链，用户仍**只写一次** `reg->next` / 输出赋值
⇒ ⇒ 贡献 API 与手写 `select` 链**都合法**，两者 MUST NOT 混用于同一 target。

⚠️ **已知盲区（冻结在此 ⇒ 下一任读者不必从 ADR 考古）**：贡献 API 的**混合
写入**检出是**不对称**的 —— 「先直接写、后贡献」与「贡献与直接写并存于同一
target」**可**检出（`resolve` 时检查该寄存器已有 next 源），但「**`resolve`
之后再直接写**」**无法**检出（除非改动 `core/reg.h` 的 next 语义 ⇒ 超出本次
范围）⇒ ⇒ 该盲区与「手写 `select` 链合法」并存 ⇒ 构成一条**已知的静默通道**。

#### Scenario: 多状态各驱动同一输出

- **WHEN** 同一个 `ch_out` 在**两个以上** state's `on_active` 闭包内被驱动
- **THEN** 只有**最后发射**的那个驱动生效
- **AND** 正确写法 MUST 是 `build()` 之后的**单一** `select` 表达式
  **或** 由 `sm.resolve_*` 生成的**单一**表达式

#### Scenario: 状态守卫贡献 API 与手写 select 链的关系

- **WHEN** 某寄存器的各状态候选值由 `sm.contrib(...)` 声明
- **THEN** 该寄存器 MUST 通过 `sm.resolve_hold(...)` 或 `sm.resolve_or(...)`
  取得**唯一**的 next 表达式
- **AND** MUST NOT 对同一 target **既**贡献**又**直接写 `next`
  （「先直接写后贡献」与「并存」**可**检出；「`resolve` 之后再直接写」
  **不可**检出 ⇒ 属上条的已知盲区）
- **AND** 同一 `(target, state)` MUST NOT 重复贡献
