# Capability: chlib-stream-semantics

> ⚠️ **收口订正（2026-10-22，对抗审稿 BLOCKER）**：本 delta 归档时曾包含两条
> 「构造期纪律 / 两阶段 ready」Requirement；它们对应**尚未实施**的后续立项
> `chlib-stream-ready-ownership-2026-10`，且现存代码并不满足 ⇒ 已从主 spec
> 与本 delta 移除（归口 follow-up change 重新提出）。本 delta **实际生效**
> 的 ADDED 为下方 3 条。

## ADDED Requirements

### Requirement: FIFO 数据通路的指针 MUST 受空/满门控（自保护）

`chlib::sync_fifo`（`include/chlib/fifo.h`）的**写口**、**写指针**与**读指针**
MUST 全部使用与 `count` **同源**的门控信号（`will_write` = `wren & !is_full(count)`、
`will_read` = `rden & !is_empty(count)`）。

⇒ 契约 MUST 从「**调用方保证** `rden` 只在非空时拉高」改为「**自保护**」：
调用方 MAY 在任何时候拉高 `wren` / `rden`，FIFO MUST NOT 因此损坏数据或失步。

#### Scenario: 空 FIFO 上连续拉高 rden MUST NOT 前进读指针

- **WHEN** FIFO 为空且 `rden` 连续为高
- **THEN** 读指针 MUST NOT 前进
- **AND** `count` MUST 保持 0
- **AND** 之后写入的元素 MUST 能被正常读出（不得因指针漂移而丢失）

#### Scenario: 满 FIFO 上再写 MUST NOT 破坏已写入的数据

- **WHEN** FIFO 已满且 `wren` 仍为高
- **THEN** 写指针 MUST NOT 前进
- **AND** 已写入的元素 MUST NOT 被覆写（排空后 MUST 逐个等于原值）
- **AND** `count` MUST 保持深度值（MUST NOT 溢出回绕成「满而空」）

#### Scenario: 排空后再写 MUST 能被读回

- **WHEN** FIFO 被填满、在满时多写一次、随后排空、再写入一个新元素
- **THEN** 新元素 MUST 能被读回（MUST NOT 落在读指针不会到达的槽位）

#### Scenario: count MUST 只统计真正发生的传输

- **WHEN** 空 FIFO 上 `rden` 为高，或满 FIFO 上 `wren` 为高
- **THEN** `count` MUST NOT 改变
- **AND** `count` MUST 与指针的推进**保持同源**（MUST NOT 出现「计数与指针脱钩」）

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

- **WHEN** 参照仓库中的 FIFO 组件
- **THEN** MUST NOT 把 `chlib::fwft_fifo` 当作 FWFT（show-ahead）实现的参照
  （实测其 `q` 在写入后仍恒为初值，直到发起读才出现队首）
- **AND** FWFT 语义 MUST 以本 Requirement 为准

### Requirement: `chlib::stream_fifo` 的行为 MUST 有双后端 parity 覆盖

`chlib::stream_fifo` 的 **payload / valid 序列** MUST 在 `InterpreterBackend` 与
`VerilatorBackend` 上**逐拍一致**。

⚠️ FWFT 的 0-cycle 语义是历史上漂移过的地方（`sim-mem-read-timing-align-2026-10`
才发现 interpreter 与 Verilog 差 1 拍）⇒ 只跑 interpreter 的断言 MUST NOT 被当作
该契约已被守住的证据。

#### Scenario: payload / valid 逐拍一致

- **WHEN** 用 `DUAL_BACKEND_TEST_PAIR` 跑 `chlib::stream_fifo`
- **AND** 序列包含 push / pop / 空 FIFO / 满 FIFO
- **THEN** 两后端的 `payload` 与 `valid` **逐拍**序列 MUST 完全一致
