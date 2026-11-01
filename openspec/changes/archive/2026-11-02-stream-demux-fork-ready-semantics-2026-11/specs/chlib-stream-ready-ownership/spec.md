# Capability: chlib-stream-ready-ownership

## ADDED Requirements

### Requirement: 分流 / 分叉类组合子的 ready 语义 MUST 是「消费端 ⇒ 上游」

`chlib` 的**分流**（`stream_demux`）与**分叉**（`stream_fork`）组合子的
`input_stream.ready` MUST 由**消费端**（下游）驱动，MUST NOT 由上游驱动；
输出侧的 `ready` MUST 是「默认值 + 可覆盖」的字段。

#### Scenario: demux 的上游 ready MUST 由被选中输出的消费端驱动

- **WHEN** `stream_demux` 的 `select` 指向输出 i
- **THEN** `input_stream.ready` MUST 等于输出 i 的 `ready`
- **AND** MUST NOT 出现「输出的 `ready` 由输入的 `ready` 驱动」这种**方向颠倒**

#### Scenario: demux 的非选中输出 MUST NOT 影响上游 ready

- **WHEN** 未被 `select` 选中的输出的 `ready` 被消费端拉低
- **THEN** `input_stream.ready` MUST **不受影响**
- **AND** 该隔离 MUST 由判据覆盖（只断言被选中的一路不足以证明隔离）

#### Scenario: demux 的 select 越界 MUST 挡住上游

- **WHEN** `select` 不落在 `[0, N_OUTPUTS)` 内
- **THEN** 所有输出的 `valid` MUST 为 0
- **AND** `input_stream.ready` MUST 为 0（上游停顿 = **可观测**）
- **AND** MUST NOT 用「`ready = 1` 并丢弃」兜底（那是**静默丢数据**）

#### Scenario: fork 的上游 ready MUST 是各输出 ready 的 AND

- **WHEN** `stream_fork` 以同步模式（`synchronous = true`）构造
- **THEN** `input_stream.ready` MUST 等于**全部**输出 `ready` 的 AND
- **AND** 判据 MUST 覆盖**两面**（全部为高 ⇒ 1；**任一**为低 ⇒ 0）
  —— 只测一面时无法与「OR」「只看第一个」区分

#### Scenario: fork 的数据面 MUST NOT 依赖任何 ready

- **WHEN** 任一输出的 `ready` 为低
- **THEN** 各输出的 `valid` 与 `payload` MUST **保持不变**
- **AND** MUST NOT 出现「`valid` 依赖 `input_stream.ready`」这种数据面与
  反压面混用

#### Scenario: fork 的 synchronous=false MUST 在建图期响亮拒绝

- **WHEN** 调用 `stream_fork(..., false)`
- **THEN** MUST 在建图期抛出异常（`CHFATAL_EXCEPTION` ⇒ `std::runtime_error`）
- **AND** MUST NOT 返回一个「看起来像异步 fork」的半成品
- **AND** MUST 用**抛异常**的机制（`CHERROR` / `CHFATAL` 只打日志不抛 ⇒ 假安全网）

#### Scenario: 输出 ready MUST 是「默认值 + 可覆盖」

- **WHEN** 消费端不驱动任何输出 `ready`
- **THEN** 该字段 MUST 有**默认驱动**（恒就绪）⇒ 行为与既有旧语义**逐位一致**
- **AND** 消费端 MUST 能用 `<<=` **覆盖**它
- **AND** 组合子内部 MUST NOT 用 `operator=` 给它赋值（那是节点重绑定，不是连接）

#### Scenario: 上游 ready MUST 由组合子驱动（接线纪律）

- **WHEN** 调用方把 `input_stream` 传给 `stream_demux` / `stream_fork`
- **THEN** `input_stream` MUST 先 `as_master()`（⇒ 字段有节点）
- **AND** 调用方 MUST NOT 自行驱动 `input_stream.ready`（由组合子驱动）
- **AND** 驱动不了的三形态（无节点 / literal 目标 / 已被真信号驱动）MUST
  **响亮拒绝**（`CHFATAL_EXCEPTION`）

#### Scenario: 判据 MUST 可机械核验

- **WHEN** 检查 `stream_demux` / `stream_fork` 的 ready 归属
- **THEN** 二者 MUST 在护栏 R8 的扫描面内（MUST NOT 处于豁免状态）
- **AND** 二者共享的上游驱动实现 MUST 有独立的结构判据（`<<=` · `type_lit` ·
  `num_srcs()` · ≥3 处响亮拒绝 · **两处都必须调用它**）
- **AND** 行为判据 MUST 走双后端 parity（`DUAL_BACKEND_TEST_PAIR`）
