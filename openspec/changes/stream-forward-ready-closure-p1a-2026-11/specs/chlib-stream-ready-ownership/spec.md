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
`openspec/changes/archive/2026-10-31-chlib-stream-ready-ownership-2026-10/design.md` §14~§15.

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

## MODIFIED Requirements

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
- **THEN** MUST 承认其 ready 链的**现状**（⚠️ **2026-11-01 原地订正**：
      change `stream-forward-ready-closure-p1a-2026-11` 落地后 ⇒ ⇒ **转发类
      （mux / arbiter / join / filters / combine_with）已收口**
      ⇒ ⇒ ⇒ ⇒ 本 Scenario 的**「两端都断」**描述**仅对未收口者成立**
      ⇒ ⇒ ⇒ ⇒ ⇒ **demux / fork**（独立 change `P1-b` / `P1-c`）**仍**是
      **两端都断** ⇒ ⇒ ⇒ ⇒ ⇒ **本 Scenario 继续在约** ⇒ ⇒ 直至它们落地）
- **AND** ⚠️ **仍**未收口者（`demux` / `fork`）MUST NOT 被本 capability
      声称为「已兑现」
- **AND** 「默认恒就绪」的豁免 MUST NOT 扩展到转发类（⇒ ⇒ 否则本豁免会成为
      「其它组合子也可硬编码常量 ready」的**先例**）
- **AND** 未收口者的收口义务 MUST **显式移交**
      `.rddf/improvements/stream-forward-ready-closure-2026-10.md`（P1 台账 §3）
- **AND** 各自的收口落地时 MUST 经 delta **加回** ⇒ ⇒ 且 MUST 同时补齐
      **行为判据**与**自动化护栏**（含「注入一处『转发类无条件忽略』⇒ MUST
      转红」的自检）

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

## ADDED Requirements

### Requirement: 转发类组合子的 ready 所有权 MUST 是**默认值 + 可覆盖**

转发类组合子（mux / arbiter / join / filters / combine_with）的 ready
所有权 MUST 采用**形态 E（默认值 + 可覆盖）** ⇒ ⇒ 与 fifo 类**同型**
⇒ ⇒ ⇒ 心智模型统一 ⇒ ⇒ **无**两阶段 / 单阶段的分别。

⇒ ⇒ ⚠️ **转发类现状 = 「上游恒不 ready = 死路」** ⇒ ⇒ 故本条是「把死路变成
活路」⇒ ⇒ **零破坏**成立（⇒ ⇒ 逐条见 Scenario「转发类现状是死路」）。

⇒ ⇒ ⚠️ **三条实现纪律** ⇒ ⇒ 逐条见 Scenario「转发类的三条实现纪律」。

⇒ ⇒ ⚠️ **消费端表达「不就绪」的两种方式**（**都合法**）⇒ ⇒ 逐条见 Scenario
「消费端表达不就绪的两种方式」。

⇒ ⇒ ⚠️ **`connect_ready` MUST 是幂等的**（⇒ ⇒ 重复调用 MUST NOT 报错）
⇒ ⇒ 逐条见 Scenario「`connect_ready` MUST 是幂等的」。

#### Scenario: 转发类现状是死路

- **WHEN** 检查转发类组合子**未**接消费端 ready 时的行为
- **THEN** MUST 承认其上游 ready **恒为 0**（实测：不调 `connect_ready` ⇒ ⇒
      上游 ready 恒为 0 ⇒ ⇒ 调用方永远推不动）
- **AND** ⇒ ⇒ 本 capability 的转发类条款 MUST 被视为「把死路变成活路」
      ⇒ ⇒ **MUST NOT** 被当作**行为变更**
- **AND** ⇒ ⇒ **零破坏**成立（⇒ ⇒ 未接背压时的行为 = **既有旧语义**）

#### Scenario: 转发类的三条实现纪律

- **WHEN** 实现转发类组合子的 ready 所有权
- **THEN** MUST **立即**建立「默认恒就绪」的 ready 节点（源 MUST 由**结果对象
      持有** ⇒ ⇒ 非 literal ⇒ ⇒ 可被覆盖）
- **AND** MUST 用 **`<<=`** 驱动上游的 ready 字段（**MUST NOT** 用
      `operator=`）⇒ ⇒ ⚠️ 因为 `ch_stream` 的**拷贝与原对象共享同一 ready
      节点** ⇒ ⇒ ⇒ `<<=` 能**穿透副本**驱动到**调用方**
- **AND** MUST NOT 有 `out_ready = true` 这类**局部常量**充当 ready 源
      （⇒ ⇒ 那是「无条件忽略」）

#### Scenario: 消费端表达不就绪的两种方式

- **WHEN** 消费端需要表达「不就绪」
- **THEN** MAY 直接 `<<=` 覆盖 `result.output_stream.ready`
      （⇒ ⇒ **单阶段** ⇒ ⇒ **不需要** `connect_ready`）
- **AND** MAY 调用 `<组合子>_connect_ready(result, consumer_ready)`
      （⇒ ⇒ 由它把消费端 ready **覆盖**到该字段 ⇒ ⇒ ⚠️ 与 fifo 类的
      `connect_ready` **语义一致** ⇒ ⇒ 心智模型统一）
- **AND** ⇒ ⇒ 两种方式 MUST **都**能达成「上游被阻塞」

#### Scenario: 转发类的上游 MUST 真的收到反压

- **WHEN** 消费端的 ready 为 **0**
- **THEN** **调用方持有的** ready 节点 MUST **有驱动**（`num_srcs() > 0`）
      ⇒ ⇒ 且其**值** MUST 为 **0**（⇒ ⇒ 上游被**真正**阻塞）
- **AND** 该行为 MUST 可被**运行期**断言（⇒ ⇒ **结构**判据 + **值**判据）
- **AND** ⚠️ **变异判别力**：把 `<<=` 改回 `operator=` ⇒ ⇒ MUST 转红
      （⇒ ⇒ `operator=` 只重绑定副本 ⇒ ⇒ 调用方节点**仍无驱动**）

#### Scenario: 未覆盖时 MUST 保持「恒就绪」（零破坏）

- **WHEN** 调用方**不**覆盖 `result.output_stream.ready`（也**不**调
      `connect_ready`）
- **THEN** 上游 ready MUST 为 **1**（= **既有旧语义**「恒就绪」）
- **AND** 既有调用点 MUST NOT 需要修改即可保持**逐位相同**的行为

#### Scenario: `connect_ready` MUST 是幂等的

- **WHEN** 调用方**重复**调用 `<组合子>_connect_ready(result, consumer_ready)`
- **THEN** MUST **不**抛异常 ⇒ ⇒ 且最后一个覆盖 MUST 生效
- **AND** MUST NOT 静默接受**空** ready 字段（⇒ ⇒ MUST 抛异常）

#### Scenario: 转发类 MUST NOT 硬编码常量 ready

- **WHEN** 检查转发类组合子的实现
- **THEN** MUST NOT 存在 `out_ready = true` 这类**局部常量**充当 ready 源
      （⇒ ⇒ 那是「无条件忽略消费端 ready」）
- **AND** MUST 有**自动化护栏**检出（⇒ ⇒ 以**行为 / 结构**判据实现 ⇒ ⇒
      **MUST NOT** 以「源码含常量 `true`」这类**词法**判据实现）
- **AND** 该护栏 MUST 有**自检**（⇒ ⇒ 注入一处「硬编码常量 ready」
      ⇒ MUST 转红）
