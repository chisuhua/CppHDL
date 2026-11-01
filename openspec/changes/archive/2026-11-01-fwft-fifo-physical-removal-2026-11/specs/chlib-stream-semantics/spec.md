# Capability: chlib-stream-semantics

## MODIFIED Requirements

### Requirement: FIFO 类组件的输出时序 MUST 被定义

`chlib::stream_fifo` 的 `pop_stream.payload` MUST 是 **FWFT（0-cycle）**语义：
MUST 与 `pop_stream.valid` **同拍**有效，且呈现**当前队首**。

⚠️ **同名不同义**：`sync_fifo::q` 是「**沿弹出**」语义（1-cycle 寄存视图），
`stream_fifo::pop_stream.payload` 是「**队首 show-ahead**」语义（0-cycle）。
两者 MUST NOT 被当作可互换。

#### Scenario: push 沿之后同拍呈现刚写入的数据

- **WHEN** 一个元素被 push 进空 FIFO
- **THEN** 在该沿之后的**同一拍**，MUST 同时满足 `valid == 1` 与
  `payload == 刚写入的数据`
- **AND** 该「写穿透（write-through）」语义 MUST 与 `mem-read-timing` capability
  已确立的「同拍写 + 同址组合读 ⇒ 返回写后值」一致

#### Scenario: pop 沿之后 payload 指向下一个队首

- **WHEN** 一个元素被 pop 出
- **THEN** 在该沿之后，`payload` MUST 指向**下一个**队首
- **AND** 消费者 MUST 在 pop 沿**之前**采样才能拿到**正在被弹出**的那个元素
  （与 ADR-044 裁决 5 同型）

#### Scenario: empty 时的数据输出 MUST 是 don't-care

- **WHEN** FIFO 为空
- **THEN** 数据输出 MUST 被视为 **don't-care**
- **AND** MUST NOT 被强制为 0（那会用**假的**「数据是 0」掩盖 FIFO 空的真实状态）
- **AND** 测试 MUST NOT 在 `empty` 时断言 payload 的具体值

> ⚠️ 两后端实测都返回 0（存储零初始化）⇒ 极易「顺手钉 0」把 don't-care 固化成契约。

#### Scenario: DEPTH MUST 是 2 的幂

- **WHEN** 声明一个 FIFO 的 `DEPTH`
- **THEN** `DEPTH` MUST 是 2 的幂（非 2 的幂会经 `compute_idx_width` 映射到
  下一个 2 的幂 ⇒ 实际深度 ≠ 声明值）
- **AND** 该约束 MUST 在编译期暴露

#### Scenario: fwft_fifo MUST NOT 被当作 FWFT 参照

> ⚠️ **本 Scenario 的标题已过时**（组件已**物理删除** ⇒ 「MUST NOT 被当作
> 参照」升级为「**MUST 不存在**」）⇒ 标题**保留**的原因：OpenSpec CLI 按
> **标题**匹配 Scenario ⇒ 改标题会被判为「丢弃旧 + 新增」⇒ **拒绝归档**
> （本仓 ADR-053 的同型先例：只能**原地订正内容**，不能删/改标题）。

- **WHEN** 参照仓库中的 FIFO 组件
- **THEN** `chlib::fwft_fifo` MUST **不存在**（名字 over-promise：实测其 `q`
  在写入后仍恒为初值，直到发起读才出现队首 ⇒ ⇒ 名字承诺 FWFT 而行为不是）
- **AND** 它的**唯一**消费者是它自己的测试，而该用例对 FWFT 语义**零判别力**
  （只断言复位后的 `empty` / `full`）⇒ ⇒ 删除**零判别力损失**
- **AND** MUST NOT 重新引入**名为 FWFT 却非 show-ahead** 的组件
  ⇒ ⇒ 该「不得回归」MUST 由**结构护栏**守住（源文件 MUST NOT 再出现该符号）
- **AND** FWFT 语义 MUST 以本 Requirement 为准（真 show-ahead = `sync_fifo`
  的 `q_async`，0-cycle `aread`）

> ⚠️ **收口订正（2026-10-22，对抗审稿 BLOCKER）**：本 capability 归档时曾误并入两条
> 「构造期纪律 / 两阶段 ready」Requirement。它们描述的是**尚未实施的后续立项**
> `chlib-stream-ready-ownership-2026-10`，且现存代码（如 `stream.h` 的组合子硬编码
> `ready = true`）并不满足 ⇒ 以「MUST 已生效契约」存在于正式主 spec 属于
> 「契约与实现不一致」。这两条已从 in-force 契约移除，待该 follow-up change
> 以 ADDED delta 重新提出。**本 capability 的 in-force 契约共 3 条。**
