# Capability: chlib-stream-ready-ownership

## ADDED Requirements

### Requirement: 链内中间级的读使能 MUST 由「下一级的容量信号」驱动

`chlib` 的**链式组合**（`StreamBuilder` 的 `queue<>()` 链、`stream_pipeline.h`
的 pipe 链）中，**链内中间级**（非链首、非链尾）的读使能（`pop_stream.ready`
的**驱动源**）MUST 是**下一级的容量信号**（下一级 `push_stream.ready` = `!full`）；
**链尾**由**消费端 ready** 驱动。

⇒ ⇒ **链内中间级不是对外暴露的组合子** ⇒ ⇒ ⇒ capability 既有「fifo 类
ready 所有权」条款 **MUST NOT** 适用于它 ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这是【适用面】的界定，
不是【例外】** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒

⇒ ⇒ **判据 MUST 可机械核验**（⇒ ⇒ 详见 Scenario）。

#### Scenario: 链内中间级的读使能 MUST 来自下一级的容量信号

- **WHEN** 检查链式组合（`StreamBuilder` 链 / pipe 链）的**中间级**
- **THEN** 其 `pop_stream.ready` MUST 有驱动源
- **AND** 该驱动源 MUST 是**下一级的** `push_stream.ready`（= `!full`）
- **AND** ⇒ ⇒ **链尾**的 `pop_stream.ready` MUST 由**消费端 ready** 驱动
- **AND** ⇒ ⇒ **MUST NOT** 形成组合环（⇒ ⇒ 源是下一级**寄存器** `full` 的
  组合输出 ⇒ ⇒ 本级读使能落在**寄存器输入**）

#### Scenario: 本不变式的适用范围 MUST 显式列出

- **WHEN** 检查本不变式覆盖哪些组合子
- **THEN** MUST 覆盖：`StreamBuilder` 链的**中间级** ·
      `chlib::stream_m2s_pipe` / `stream_s2m_pipe` / `stream_half_pipe`
- **AND** ⇒ ⇒ **对外暴露**的组合子 MUST **NOT** 适用本不变式
      （⇒ ⇒ 它们属既有「转发类 / fifo 类」条款的适用范围）

#### Scenario: 链内中间级 MUST NOT 被读作「fifo 类的例外」

- **WHEN** 阅读 capability 的适用范围
- **THEN** 本条 MUST 陈述**适用面**（哪些链内中间级在范围内）
- **AND** MUST 说明「链内中间级不是对外暴露的组合子」
- **AND** MUST NOT 表述为既有条款的**例外**
- **AND** ⇒ ⇒ **对外暴露**的组合子 MUST NOT 适用本不变式

#### Scenario: 判据 MUST 可机械核验

- **WHEN** 检查链内每一级的 `pop_stream.ready`
- **THEN** MUST 有驱动源
- **AND** 该驱动源 MUST 是**下一级的** `push_stream.ready`
- **AND** ⇒ ⇒ `stream_pipeline.h` MUST 被纳入**护栏的扫描面**
      （⇒ ⇒ 否则 pipe 族永远在护栏之外）

#### Scenario: pipe 族 MUST 纳入本不变式的适用范围

- **WHEN** 检查 `chlib::stream_m2s_pipe` / `stream_s2m_pipe` / `stream_half_pipe`
- **THEN** 它们 MUST 被本条覆盖（⇒ ⇒ 与 `StreamBuilder` 链**同型**）
- **AND** ⇒ ⇒ 它们的**上游** ready MUST 由下游容量 / 消费端驱动
- **AND** ⇒ ⇒ 「**只在 `ready = true` 常量接线**下活着」的状态 MUST **不再成立**

#### Scenario: 上游反压 MUST 真的到达调用方

- **WHEN** 调用方按 **ready 协议**驱动（⇒ ⇒ 用 `as_master()` + `<<=`）
- **THEN** 调用方持有的 `source.ready` MUST **有驱动**（`num_srcs() > 0`）
- **AND** 其**值 MUST 随占用变化**（占用满 ⇒ ⇒ 落 0）
- **AND** ⇒ ⇒ **MUST NOT** 锁死某个字面量（消费速率影响占用）

#### Scenario: 链内 MUST NOT 出现组合环

- **WHEN** 把中间级的读使能接到下一级的容量信号
- **THEN** 图 MUST **无环**（⇒ ⇒ 源是**寄存器** `full` 的组合输出 ⇒ ⇒
  本级读使能落在**寄存器输入**）
- **AND** ⇒ ⇒ **变异验证**：去掉该连接 ⇒ ⇒ 判据 MUST 转红
  （⇒ ⇒ **先确认编译成功**）

#### Scenario: 中间级被覆盖后 MUST NOT 再被 `connect_ready` 覆盖

- **WHEN** 中间级的 `pop_stream.ready` 已被接到下一级的容量信号
- **THEN** 再调该中间级的 `connect_ready` MUST **抛异常**
  （⇒ ⇒ 这是**正确**的多驱动检测 ⇒ ⇒ **不是**误抛）
- **AND** ⇒ ⇒ **调用方 MUST NOT** 对已覆盖的中间级调 `connect_ready`
