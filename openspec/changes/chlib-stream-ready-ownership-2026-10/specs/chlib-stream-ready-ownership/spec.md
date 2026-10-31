# Capability: chlib-stream-ready-ownership

## Purpose

定义 `chlib` 的 **stream 组合子**对 **ready（背压）信号**的**所有权契约**：
组合子 MUST NOT **无条件**忽略消费端的 ready（⇒ ⇒ MUST 存在路径使 `ready == 0`
阻止推进）；ready 链 MUST 用 `<<=`（数据流边）连接；调用方 MUST 读**结果对象**的
ready 字段。

**为什么需要**（ADR-045 §1 决策 0-B + **ADR-054**）：`stream_fifo` 的读使能
硬编码为 `true` ⇒ ⇒ 消费端恒就绪 ⇒ ⇒ FIFO 永远在排空 ⇒ ⇒ **`full` 不可达**
⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」这个反压场景**在当前 API 下无法演示** ⇒ ⇒
2 个 disabled sample 的**全部价值**依赖它 ⇒ ⇒ 连带 **6 处 `ch_in` 静默失效**。
⇒ ⇒ 且 ADR-045:160 **明文禁止**「组合子硬编码常量 ready」⇒ ⇒ 现状**违反**已签批 ADR。

⚠️ **形态说明**（ADR-054 §3，**2026-10-31 修订**）：本 capability 采用**形态 E
（默认值 + 可覆盖）**—— 组合子**立即**建立内部逻辑（行为**逐位不变** ⇒ ⇒ 存量
调用点**不需迁移**），读使能接一个**带默认驱动**的字段（默认 = **既有旧语义**
「恒就绪」），`<组合子>_connect_ready(result)` 把消费端 ready **覆盖**到该字段。
⚠️ **形态 B（延迟构造）已被实施期证伪否决**（既有测试红：读未驱动字段实测得 **0**
而非报错，且一处 **SIGABRT**）⇒ ⇒ 详见
`openspec/changes/chlib-stream-ready-ownership-2026-10/design.md` §9。

## ADDED Requirements

### Requirement: 组合子 MUST NOT **无条件**忽略消费端的 ready

`chlib` 的 stream 组合子 MUST NOT **无条件**忽略消费端的 ready 信号
⇒ ⇒ MUST 存在一条路径，使 `ready == 0` 时组合子的推进被**阻止**。

⇒ ⇒ 「消费端恒就绪 ⇒ 组合子永远推进」的形态 MUST **不可达**。

⇒ ⇒ **判据 MUST 是效果判据**（行为可观测），MUST NOT 是**词法**判据
（⇒ ⇒ 否则「默认恒就绪」这种**合法**实现也会被误判）。

⇒ ⇒ ⚠️ **「默认恒就绪」的豁免 MUST 限定**于 **fifo 类**（读使能是**内部逻辑的
输入**且默认值 = **既有旧语义**）⇒ ⇒ **转发类**（mux / demux / arbiter）
**无条件**忽略消费端 ready ⇒ ⇒ **仍属违规**（⇒ ⇒ 否则本豁免会成为
「其它组合子也可硬编码常量 ready」的**先例**）。

⇒ ⇒ MUST 有**自动化护栏**检出「无条件忽略」（⇒ ⇒ 以**行为**判据实现）。

#### Scenario: 消费端不就绪 MUST 能阻止推进

- **WHEN** 消费端的 ready 为 **0**
- **THEN** 组合子 MUST **不**推进（⇒ ⇒ 例如 FIFO **不**弹出元素）
- **AND** 该行为 MUST 可被**运行期**断言

#### Scenario: 「无条件忽略」MUST 被护栏检出

- **WHEN** 检查组合子实现是否**无条件**忽略消费端 ready
- **THEN** MUST 被**自动化护栏**检出
- **AND** 该护栏 MUST 以**行为 / 结构**判据实现（⇒ ⇒ **MUST NOT** 以
      「源码含常量 `true`」这类**词法**判据实现 ⇒ ⇒ 否则「合法的默认恒就绪」
      会被误判 ⇒ ⇒ 判别力归零）
- **AND** 该护栏 MUST 有**自检**（⇒ ⇒ 注入一处「无条件忽略」⇒ MUST 转红）

### Requirement: fifo 类组合子的 ready 所有权 MUST 是**默认值 + 可覆盖**

fifo 类组合子（读使能是**内部逻辑的输入**）MUST：

1. **立即**建立内部逻辑 ⇒ ⇒ **既有行为逐位不变** ⇒ ⇒ 存量调用点 MUST NOT
   被迫迁移；
2. 读使能 MUST 接一个**带默认驱动**的字段 —— 该默认 MUST 等于**既有旧语义**
   （恒就绪）⇒ ⇒ 未接背压时的行为与改动前**完全一致**；
3. MUST 提供 `<组合子>_connect_ready(result)`，把消费端 ready **覆盖**到该字段。

⇒ ⇒ ⚠️ **读使能 MUST 引用「字段节点本身」**（结果对象的 ready 字段的 proxy），
MUST NOT 引用**局部**节点 ⇒ ⇒ 否则覆盖**不传播 ⇒ 静默失效**。

⇒ ⇒ ⚠️ **多驱动守卫**：`connect_ready` MUST 断言「当前驱动仍是默认节点」，
否则 MUST **抛异常**（⚠️ MUST NOT 只打日志）。

#### Scenario: 未接背压时行为 MUST 与改动前一致

- **WHEN** 调用方**不**调用 `connect_ready`
- **THEN** 组合子 MUST 表现为**恒就绪**（= **既有旧语义**）
- **AND** 既有调用点 MUST NOT 需要修改即可保持**逐位相同**的行为

#### Scenario: `connect_ready` MUST 能覆盖默认驱动

- **WHEN** 调用方把消费端 ready 接上并调用 `connect_ready`
- **THEN** 读使能 MUST 取消费端 ready 的**值**
- **AND** 读使能 MUST 引用**字段节点本身**（⇒ ⇒ 覆盖 MUST **传播**到内部逻辑）

#### Scenario: 多驱动 MUST 被检出

- **WHEN** 调用方**已自行驱动**该 ready 字段（⇒ ⇒ 与组合子的默认驱动冲突）
- **THEN** `connect_ready` MUST 以**抛异常**的方式失败
- **AND** MUST NOT 只打日志（MUST NOT 静默继续）

### Requirement: ready 链 MUST 用 `<<=` 连接

ready 链 MUST 用 **`<<=`**（数据流边）连接 ⇒ ⇒ MUST NOT 用 `operator=`
⇒ ⇒ `operator=` 会**孤儿化**调用方的 `in.ready` 节点 ⇒ ⇒ 调用方继续读自己的字段
会拿到一个**无驱动的孤儿节点**（恒 0 / "Value not found"）⇒ ⇒ **静默**。

⇒ ⇒ 调用方 MUST 读 `result.input_stream.ready` / `result.input_streams[i].ready`，
MUST NOT 读自己的入参字段。

⇒ ⇒ ⚠️ 既有 `stream_mux_connect_ready`（`include/chlib/stream.h`）用的是
`operator=` ⇒ ⇒ MUST 改为 `<<=`。

#### Scenario: 调用方 MUST 读结果对象的 ready 字段

- **WHEN** 调用方需要知道「组合子是否可推进」
- **THEN** MUST 读 `result.input_stream.ready` / `result.input_streams[i].ready`
- **AND** MUST NOT 读自己的入参 ready 字段（⇒ ⇒ 那会读到**无驱动的孤儿节点**）
- **AND** ready 链的连接手段 MUST 是 `<<=`（⇒ ⇒ 静态护栏 MUST 检出 `operator=` 形态）

### Requirement: 「未连接 ⇒ 响亮失败」是**长期方向**（不在本能力范围内）

若调用方在**未**完成 ready 连接的情况下读取结果对象 ⇒ ⇒ **理想**是产生
**可诊断**的失败 ⇒ ⇒ 但该能力依赖**框架层**的「读未驱动字段的**诊断**」机制
⇒ ⇒ 那是**框架层通用能力缺口** ⇒ ⇒ **独立立项** ⇒ ⇒ **不**在本 capability
范围内。

⇒ ⇒ **本 capability 的可落地替代约束**：组合子 MUST NOT **无条件**忽略消费端
ready ⇒ ⇒ MUST 存在路径使 `ready == 0` 阻止推进 ⇒ ⇒ **效果判据**。

⇒ ⇒ ⚠️ 「读未驱动字段返回 0 且不报错」是**框架层**的已知行为
⇒ ⇒ 它**不**由本 capability 约束 ⇒ ⇒ 不得据此断言组合子「响亮拒绝」。

#### Scenario: 本能力 MUST NOT 承诺「响亮失败」

- **WHEN** 检查本 capability 的 MUST 集合
- **THEN** MUST **不**包含「未连接 MUST 产生可诊断失败」这一条
- **AND** MUST 包含其**可落地替代**（「MUST NOT 无条件忽略消费端 ready」）
