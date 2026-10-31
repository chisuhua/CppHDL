# chlib-stream-ready-ownership Specification

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
`openspec/changes/archive/2026-10-31-chlib-stream-ready-ownership-2026-10/design.md` §9。

⚠️ **实施期补入的两条承重约束**（ADR-054 §3.2 第 1 条 / 第 1-A 条）：
① **默认驱动的源 MUST 活得比结果对象久** ⇒ ⇒ 落地形态 = **结果对象持有源节点**
   ⇒ ⇒ MUST NOT 指向**局部**变量 / **临时**字面量（⇒ ⇒ 临时字面量 ⇒ 驱动未建立
   ⇒ ⇒ `sync_fifo` 拿到**无效操作数** ⇒ ⇒ 💥 **SIGSEGV**）⇒ ⇒ 且目标 MUST 是
   **非 literal** 的节点 ⇒ ⇒ **MUST 用 `<<=`**（MUST NOT `operator=` ——
   重绑定会把字段换成 literal 节点 ⇒ ⇒ 而 literal 节点**不可被 `set_src`**
   ⇒ ⇒ 覆盖**无效**）⇒ ⇒ **两个约束缺一不可**；
② **写使能 MUST 只用 `input_stream.valid`** ⇒ ⇒ MUST NOT 与
   `input_stream.ready` 合取（⇒ ⇒ **先读后写** ⇒ ⇒ `in.ready` 是空 proxy
   ⇒ ⇒ 报 "Invalid operands" ⇒ ⇒ **组合子完全不可用**）。

⇒ ⇒ ⇒ **实施完成且已验证**（2026-10-31，**范围限定**：**fifo 类**）：
`ctest -LE perf` **194/194** · samples **39/39** · examples **28/28** ·
护栏（含 `--self-test`）全 exit 0
⇒ ⇒ 效果判据实测通过（反压能阻止推进 · `full` 可达 · 旧语义逐位保持 ·
多驱动**抛异常**）⇒ ⇒ 详见
`openspec/changes/archive/2026-10-31-chlib-stream-ready-ownership-2026-10/design.md` §14~§15。

⚠️⚠️ **范围限定（2026-10-31 收口订正 · change
`stream-ready-ownership-correction-2026-10`）**：上条的「实施完成且已验证」
**仅覆盖 fifo 类**（`stream_fifo` / `stream_queue`）⇒ ⇒ **转发类**（mux /
demux / arbiter / fork / join / filters）的 ready 链**两端都断**（实测：
构造期 `Invalid operands` ⇒ ⇒ 结果字段是**空节点** ⇒ ⇒ **读即 ABORT**）
⇒ ⇒ 且 `stream_mux_connect_ready` 的 `<<=` 修复在「调用方未建节点」时
**静默无效** ⇒ ⇒ ⇒ **转发类 MUST NOT** 被本 capability 声称「已兑现」
⇒ ⇒ ⇒ 其收口义务**显式移交**
`.rddf/improvements/stream-forward-ready-closure-2026-10.md`（P1 台账）
⇒ ⇒ ⇒ 待其落地时经 delta **加回**。

⚠️ **覆盖生效的四个必要条件**（2026-10-31 机制级订正，实测）：① **目标** MUST
是**已存在**的**非 literal** 节点（⇒ ⇒ `<<=` 在**空**目标上会**另建**节点
⇒ ⇒ 只驱动被 `<<=` 的对象 ⇒ ⇒ **调用方被孤儿化**）② **源** MUST 非空
（⇒ ⇒ 否则 `CHERROR` 只打日志 ⇒ ⇒ **静默**）③ **源** MUST 活得比结果对象久
（⇒ ⇒ 否则 💥 **SIGSEGV**）④ **覆盖 MUST 在 `Simulator::initialize()`
**之前**做**（⇒ ⇒ 改边**不重建指令** ⇒ ⇒ 否则**静默** ⇒ ⇒ ⇒ ⚠️ **判别力
陷阱** ⇒ ⇒ 运行期改 ready 写的断言**恒绿**）。

## Requirements

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

#### Scenario: 转发类 MUST NOT 被读作「已合规」

- **WHEN** 检查本 capability 的**转发类**（mux / demux / arbiter / fork /
      join / filters）的 ready 链实现
- **THEN** MUST 承认其 ready 链**两端都断**（实测：构造期 `Invalid operands`
      ⇒ ⇒ 结果字段是**空节点** ⇒ ⇒ **读即 ABORT**）
- **AND** MUST NOT 被本 capability 声称为「已兑现」
- **AND** 「默认恒就绪」的豁免 MUST NOT 扩展到转发类（⇒ ⇒ 否则本豁免会成为
      「其它组合子也可硬编码常量 ready」的**先例**）
- **AND** 其收口义务 MUST **显式移交**
      `.rddf/improvements/stream-forward-ready-closure-2026-10.md`（P1 台账 §3）
- **AND** 待 P1 落地时 MUST 经 delta **加回**本条 ⇒ ⇒ 届时 MUST 同时补齐
      转发类的**行为判据**与**自动化护栏**（含「注入一处『转发类无条件忽略』
      ⇒ MUST 转红」的自检）

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

#### Scenario: 覆盖生效 MUST 同时满足四个必要条件

- **WHEN** 调用方用 `<组合子>_connect_ready` 覆盖消费端 ready
- **THEN** 覆盖 MUST 生效 ⇒ ⇒ 且 MUST 同时满足：
  ① **目标** MUST 是**已存在**的**非 literal** 节点（⇒ ⇒ `operator=` 重绑定
      ⇒ ⇒ literal 节点**不可被 `set_src`** ⇒ ⇒ 覆盖**无效**；⇒ ⇒ `<<=` 在
      **空节点**上**另建**节点 ⇒ ⇒ 只驱动被 `<<=` 的对象 ⇒ ⇒ **调用方被
      孤儿化** ⇒ ⇒ ⇒ **MUST 用 `<<=`**）
  ② **源** MUST 是**非空**节点（⇒ ⇒ 否则 `CHERROR` 只打日志 ⇒ ⇒ **静默**）
  ③ **源** MUST 活得比结果对象久（⇒ ⇒ 否则 💥 **SIGSEGV**）
  ④ **覆盖 MUST 在 `Simulator::initialize()` **之前**做**（⇒ ⇒ 改边
      **不重建指令** ⇒ ⇒ 否则**静默**）
- **AND** 四类失效（缺持久源 / 缺非空源 / 目标被重绑定成 literal / 覆盖晚于
      `initialize`）MUST NOT 用**单条**判据覆盖 ⇒ ⇒ 其中三类是**静默**的
- **AND** ⚠️ **运行期改 ready ⇒ ⇒ 覆盖「不生效」⇒ ⇒ 任何据此写的断言
      **恒绿** ⇒ ⇒ ⇒ ⇒ **判别力陷阱** ⇒ ⇒ MUST NOT 用它当判据

#### Scenario: 未接背压时行为 MUST 与改动前一致

- **WHEN** 调用方**不**调用 `connect_ready`
- **THEN** 组合子 MUST 表现为**恒就绪**（= **既有旧语义**）
- **AND** 既有调用点 MUST NOT 需要修改即可保持**逐位相同**的行为

#### Scenario: `connect_ready` MUST 能覆盖默认驱动

- **WHEN** 调用方把消费端 ready 接上并调用 `connect_ready`
- **THEN** 读使能 MUST 取消费端 ready 的**值**
- **AND** 读使能 MUST 引用**字段节点本身**（⇒ ⇒ 覆盖 MUST **传播**到内部逻辑）
- **AND** 调用方 MUST 在 `Simulator` 构造**之前**完成该 ready 连接
  （⇒ ⇒ 否则覆盖**不生效** ⇒ ⇒ ⇒ 任何据此写的断言**恒绿**
  ⇒ ⇒ ⇒ ⇒ **判别力陷阱** ⇒ ⇒ MUST NOT 用它当判据）

#### Scenario: 多驱动 MUST 被检出

- **WHEN** 调用方**已自行驱动**该 ready 字段（⇒ ⇒ 与组合子的默认驱动冲突）
- **THEN** `connect_ready` MUST 以**抛异常**的方式失败
- **AND** MUST NOT 只打日志（MUST NOT 静默继续）

### Requirement: fifo 类组合子的**写使能** MUST NOT 依赖调用方的 `in.ready`

`chlib::stream_fifo` 的**写使能** MUST 只用 `input_stream.valid`
⇒ ⇒ MUST NOT 用 `input_stream.valid & input_stream.ready`。

⇒ ⇒ **为什么**：
① **先读后写** —— `in.ready` 是**调用方自己的**信号 ⇒ ⇒ 组合子内部读它时
   调用方**还没**建立 ready 链 ⇒ ⇒ 该字段是**空 proxy** ⇒ ⇒ `&` 拿到
   **null 操作数** ⇒ ⇒ 报 "Invalid operands" ⇒ ⇒ **组合子完全不可用**；
② **语义** —— FIFO 的接收能力由 `result.push_stream.ready`（= `!full`）表达
   ⇒ ⇒ 组合子 MUST NOT 反过来依赖调用方的 `in.ready`；
③ **自保护** —— 写口已用 `will_write = wren & !is_full(count)` 门控
   ⇒ ⇒ 写使能**不需**再与 `ready` 合取。

#### Scenario: 写使能 MUST NOT 读 `in.ready`

- **WHEN** 检查 `chlib::stream_fifo` 的写使能
- **THEN** MUST 是 `input_stream.valid`
- **AND** MUST NOT 含 `input_stream.ready` 的合取
- **AND** 把写使能改回 `valid & ready` 的变异 MUST **转红**

#### Scenario: 调用方 MUST NOT 被要求驱动 `in.ready`

- **WHEN** 调用方只驱动 `payload` / `valid`（**不**驱动 `in.ready`）并调用
      `chlib::stream_fifo`
- **THEN** 组合子 MUST **正常工作**（MUST NOT 报 "Invalid operands"）
- **AND** 调用方 MUST NOT 被要求驱动 `in.ready`

### Requirement: ready 链 MUST 用 `<<=` 连接

ready 链 MUST 用 **`<<=`**（数据流边）连接 ⇒ ⇒ MUST NOT 用 `operator=`
⇒ ⇒ `operator=` 会**孤儿化**调用方的 `in.ready` 节点 ⇒ ⇒ 调用方继续读自己的字段
会拿到一个**无驱动的孤儿节点**（恒 0 / "Value not found"）⇒ ⇒ **静默**。

⇒ ⇒ 调用方 MUST 读 `result.input_stream.ready` / `result.input_streams[i].ready`，
MUST NOT 读自己的入参字段。

⇒ ⇒ ⚠️ 既有 `stream_mux_connect_ready`（`include/chlib/stream.h`）用的是
`operator=` ⇒ ⇒ MUST 改为 `<<=`。

#### Scenario: 覆盖生效的必要条件

- **WHEN** 检查 ready 链的连接手段是否足以让覆盖生效
- **THEN** MUST 认定「用 `<<=`」**不**充分（⇒ ⇒ `<<=` 在**空节点**上会
      **另建**一个 assign 节点 ⇒ ⇒ 只驱动被 `<<=` 的那个对象 ⇒ ⇒ ⇒
      **调用方持有的节点仍是无驱动的孤儿节点**）
- **AND** MUST 认定「目标节点 MUST 已存在」是**必要**条件
- **AND** MUST 引用「fifo 类组合子的 ready 所有权 MUST 是默认值 + 可覆盖」条
      的「四个必要条件」

#### Scenario: 调用方 MUST 先建立字段节点

- **WHEN** 调用方把 stream 传给组合子
- **THEN** MUST 先建立该 stream 的字段节点（`as_master()` / `as_slave()` /
      `create_field_nodes()` 之一）⇒ ⇒ 否则组合子**无法**驱动它的 ready 字段
- **AND** 该前置条件 MUST 被视为**前置条件**，**MUST NOT** 视为实现细节
      （⇒ ⇒ 组合子形参**按值**接收 ⇒ ⇒ 拿到的是**副本**⇒ ⇒ **无法**替
      调用方建节点 ⇒ ⇒ ⇒ 该前置条件**不可由组合子内部消除**）
- **AND** ⚠️ **本前置条件是「当前框架能力」的产物** ⇒ ⇒ MUST 被长期方向取代
      ⇒ ⇒ **ADR-045 决策 0-A**（形参由**按值**改为**按引用**）落地后 ⇒ ⇒
      组合子**能**拿到调用方的**同一个** stream 对象 ⇒ ⇒ ⇒ **该前置条件将
      自动消失**
- **AND** ⇒ ⇒ 故本条 MUST 在 0-A 落地时经 **`MODIFIED` delta 退役**
      （⇒ ⇒ 退役前 MUST 仍**在约** ⇒ ⇒ 因为**当前**实现确实要求它）
- **AND** ⇒ ⇒ MUST NOT 被当作「框架的永久形态」
- **AND** ⇒ ⇒ 退役 MUST 走 `MODIFIED`（保留其余 Scenario）⇒ ⇒ MUST NOT
      用「删除该 Requirement」的方式退役（⇒ ⇒ 那是**契约丢失**，非退役）

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
