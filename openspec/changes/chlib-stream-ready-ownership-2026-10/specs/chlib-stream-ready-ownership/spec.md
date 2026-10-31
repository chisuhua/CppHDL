# Capability: chlib-stream-ready-ownership

## Purpose

定义 `chlib` 的 **stream 组合子**对 **ready（背压）信号**的**所有权契约**：
组合子 MUST NOT 硬编码常量 ready；ready 链 MUST 用 `<<=`（数据流边）连接；
调用方 MUST 读**结果对象**的 ready 字段；**未完成 ready 连接**时 MUST **响亮拒绝**
（MUST NOT 退化为「读到 0 / 读到常量」）。

**为什么需要**（ADR-045 §1 决策 0-B + **ADR-054**）：`stream_fifo` 的读使能
硬编码为 `true` ⇒ ⇒ 消费端恒就绪 ⇒ ⇒ FIFO 永远在排空 ⇒ ⇒ **`full` 不可达**
⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」这个反压场景**在当前 API 下无法演示** ⇒ ⇒
2 个 disabled sample 的**全部价值**依赖它 ⇒ ⇒ 连带 **6 处 `ch_in` 静默失效**。
⇒ ⇒ 且 ADR-045:160 **明文禁止**「组合子硬编码常量 ready」⇒ ⇒ 现状**违反**已签批 ADR。

## ADDED Requirements

### Requirement: 组合子 MUST NOT 硬编码常量 ready

`chlib` 的 stream 组合子 MUST NOT 用**常量**（`true` / `false`）充当 ready 信号
⇒ ⇒ 消费端的就绪 MUST 能**真实地**参与组合子的推进判定。

⇒ ⇒ 「消费端恒就绪 ⇒ 组合子永远推进」的形态 MUST **不可达**。

⇒ ⇒ MUST 有**自动化护栏**检出硬编码常量 ready（⇒ grep-lint 形态）。

#### Scenario: 硬编码常量 ready MUST 被检出

- **WHEN** 检查组合子实现中 `ready = true` / `pop_ready = true` 之类的**常量** ready
- **THEN** MUST 被**自动化护栏**检出
- **AND** 该护栏 MUST 有**自检**（⇒ 注入一处硬编码 ⇒ MUST 转红）

### Requirement: ready 所有权 MUST 是**两阶段**，且 fifo 类 MUST 用**延迟构造**

组合子 MUST NOT 拥有调用方的入参 ready 字段。它 MUST：

1. 提供 `<组合子>_connect_ready(result)`，由调用方在**设置好**消费端 ready
   **之后**调用；
2. `connect_ready` MUST 在**内部逻辑建立时**使用消费端 ready 的**同一节点**
   ⇒ ⇒ MUST NOT 引入**代理节点**。

⇒ ⇒ **fifo 类**（读使能是**内部逻辑的输入**）MUST 采用**延迟构造**：
构造期 MUST **不产出**任何字段 ⇒ ⇒ `connect_ready` **之后**才建立内部 FIFO
⇒ ⇒ 读使能直接取消费端 ready 的**同一节点** ⇒ ⇒ 无代理节点 ⇒ ⇒ **可移植**。

⇒ ⇒ **mux / demux / arbiter 类**（ready 是**纯转发**）MUST 保持两阶段形态。

⇒ ⇒ ⚠️ **形态 A（代理 + 阶段 2 覆盖）MUST NOT 被采用**（依赖实现细节 +
接受「读到**临时值**」的静默窗口）。

#### Scenario: fifo 类的读使能 MUST 取消费端 ready 的同一节点

- **WHEN** 调用方设置好 `result.pop_stream.ready` 后调用 `stream_fifo_connect_ready(result)`
- **THEN** 内部 FIFO 的读使能 MUST 是该 ready 的**同一节点**
- **AND** MUST NOT 存在**代理节点**（⇒ ⇒ 代理「覆盖」技巧 MUST NOT 被使用）
- **AND** 「未调用 `connect_ready` 就读取结果」MUST NOT 退化为「读到常量 / 读到 0」

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

### Requirement: 未完成 ready 连接 MUST 是**可诊断**的失败

若调用方在**未**调用 `<组合子>_connect_ready(result)` 的情况下读取结果对象
⇒ ⇒ MUST 产生**可诊断**的失败 ⇒ ⇒ MUST NOT 退化为「读到 0 / 读到常量 /
读到临时值」。

⇒ ⇒ 该失败 MUST 可**运行期**检出（⇒ ⇒ 它**不可**是编译期问题 ⇒ ⇒
MUST 有**运行期**判据，而非仅靠文档）。

⇒ ⇒ ⚠️ 由此产生一条**新增的调用方义务**：`connect_ready` MUST 在**任何仿真之前**
完成 ⇒ ⇒ 违反该义务的后果 MUST 是**响亮**的。

#### Scenario: 未调用 `connect_ready` 就读结果 MUST 响亮失败

- **WHEN** 调用方未调用 `connect_ready` 就读取结果对象的字段
- **THEN** MUST 产生**可诊断**的失败
- **AND** MUST NOT 读到 0 / 常量 / 任何**未连接**状态下的值
