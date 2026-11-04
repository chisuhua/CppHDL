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

⚠️ **覆盖面扩展（2026-11-01 · ADR-055）**：本 capability 另覆盖**链式组合的
链内中间级**（`StreamBuilder` 的 `queue<>()` 链 · `stream_pipeline.h` 的
`m2s` / `s2m` / `half` 三个 pipe）—— 它们的 ready 归属由「**链内闭包不变式**」
约束（中间级的读使能 ← 下一级的容量信号；链尾 ← 消费端 ready）。
⚠️ **链内中间级不是对外暴露的组合子** ⇒ ⇒ ADR-054 §3.2 第 1 条对它们
**不适用**（是**适用面界定**，不是例外，**不立第三类**）。
⇒ ⇒ 落地与证据：`openspec/changes/archive/2026-11-01-stream-builder-and-pipe-ready-ownership-2026-11/`。

⚠️⚠️ **范围限定（2026-10-31 收口订正 · change
`stream-ready-ownership-correction-2026-10`；⚠️ **2026-11-01 更新** · change
`stream-forward-ready-closure-p1a-2026-11`）**：上条的「实施完成且已验证」
**仅覆盖 fifo 类**（`stream_fifo` / `stream_queue`）⇒ ⇒ ⇒
⚠️ **转发类的收口进度（2026-11-01）**：
- ✅ **已收口**（形态 E · 单阶段）：`mux` · `arbiter×4`（`priority` /
  `round_robin` / `lock` / `sequence`）· `join` · `filters×6` ·
  `combine_with` ⇒ ⇒ ⇒ 判别力用例（**调用方持有的** ready 节点**真有驱动**
  且**值跟随**）⇒ ⇒ ⇒ 护栏 R8（**穷举** + **存在性判据**）⇒ ⇒ ⇒
  ⇒ ⇒ ⇒ **8 种原地投毒全部被抓住**（行为 + 静态**双判据**）；
- ❌ **仍未收口**：`demux`（方向修正是**行为变更** ⇒ ⇒ 独立 ADR）·
  `fork`（同步语义**从未工作过** ⇒ ⇒ 等于**新功能**）⇒ ⇒ ⇒ 其义务**显式移交**
  `.rddf/improvements/stream-forward-ready-closure-2026-10.md`（P1 台账）
  ⇒ ⇒ ⇒ 待其落地时经 delta **加回**；
- ⚠️ **另有一项独立的真实缺陷**（⇒ ⇒ **不属于**本 capability）：builder 的
  链式 `queue<>()` **上游反压从未建立**（实测 `source.ready` **恒 0** ⇒ ⇒ ⇒
  上游若遵守 ready 协议 ⇒ ⇒ ⇒ **永远推不动 = 死路**）⇒ ⇒ ⇒
  `.rddf/improvements/stream-builder-chain-silent-word-loss-2026-11.md`。
  ⚠️ **2026-11-01 事实订正**（change
  `chlib-stream-ready-ownership-purpose-correction-2026-11`）：本段原写
  「**静默丢字**」⇒ ⇒ ⇒ **该描述已被实测推翻**（⇒ ⇒ 「弹光后为空」被误读为
  「丢字」）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 上面的「上游反压」才是真缺陷**（⇒ ⇒ 两者**不是**
  ⇒ ⇒ ⇒ 同一件事 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒）。

⇒ ⇒ ⇒ **转发类 MUST NOT** 被本 capability 声称「**全部**已兑现」
⇒ ⇒ ⇒ ⇒ 未收口者仍属「硬编码常量 ready ⇒ ⇒ **仍属违规**」。

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
- **THEN** MUST 承认其 ready 链的**现状**（⚠️ **2026-11-04 原地订正**：
      **转发类已全部收口** —— `mux` · `arbiter` 族（`priority` / `round_robin` /
      `lock`）· `join` · `filters` 族 · `combine_with`
      ⇒ ⇒ **2026-11-01** 落地（**ADR-054 §3.2 第 2 条** · 2026-11-01 用户裁决
      「单阶段 / 形态 E」+ change `stream-forward-ready-closure-p1a-2026-11`）
      ⇒ ⇒ `demux` ⇒ ⇒ **2026-11-01 裁决**（**ADR-056**）·
      `fork` ⇒ ⇒ **2026-11-01 裁决**（**ADR-057**）
      ⇒ ⇒ ⇒ ⇒ **2026-11-02 落地**（change
      `stream-demux-fork-ready-semantics-2026-11`）
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 本 Scenario 原先的**「两端都断」**描述
      **仅对 2026-11-02 之前成立**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **「未收口者」集合现为空**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **本 Scenario 继续在约**
      （作为「转发类 MUST NOT 被读作『已合规』」的**持续义务**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **将来新增**的转发类组合子 MUST 同样受它约束））
- **AND** ⚠️ **2026-11-01 的「未收口者 MUST NOT 被声称『已兑现』」条款已结清**
      （⇒ ⇒ **P1-b（demux）/ P1-c（fork）已于 2026-11-02 收口**
      ⇒ ⇒ ⇒ ⇒ 该台账 `.rddf/improvements/stream-forward-ready-closure-2026-10.md`
      的 Status 已同步订正为「P1-a / P1-b / P1-c 全部收口」
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **本条不再是未决义务**，仅为**移交记录**）
- **AND** 「默认恒就绪」的豁免 MUST NOT 扩展到转发类（⇒ ⇒ 否则本豁免会成为
      「其它组合子也可硬编码常量 ready」的**先例**）
- **AND** ⚠️ **「未收口者的收口义务 MUST 显式移交」改写为条件式**
      （⇒ ⇒ **合并 = 整条 Requirement 替换** ⇒ ⇒ 直接删掉它就等于
      **该义务静默消失** ⇒ ⇒ 故保留其规范效力）：
      **若**将来出现未收口者 ⇒ ⇒ 其收口义务 MUST **显式移交**
      `.rddf/improvements/stream-forward-ready-closure-2026-10.md`（P1 台账 §3）
      ⇒ ⇒ **本次**的移交对象（P1-b / P1-c）**已结清**（2026-11-02）
- **AND** 各自的收口落地时 MUST 经 delta **加回** ⇒ ⇒ 且 MUST 同时补齐
      **行为判据**与**自动化护栏**（含「注入一处『转发类无条件忽略』⇒ MUST
      转红」的自检）⇒ ⇒ ⚠️ **2026-11-02 已按此条执行**
      （`demux` / `fork` 的 delta = change
      `stream-demux-fork-ready-semantics-2026-11` ⇒ 行为判据 = 双后端用例 +
      护栏 = **R13** + 2 种原地投毒）⇒ ⇒ ⇒ ⇒ ⇒ **本条对将来新增的转发类继续有效**
- **AND** ⚠️ **2026-11-04 限定措辞**（**避免假陈述**）：本条 MUST NOT 被读作
      「**转发类全部**无条件忽略 ready」⇒ ⇒ ⇒ **硬编码常量 ready** 的转发类
      （⇒ ⇒ 此处指**上游侧**（输入侧）ready 源；形态 E 的**可覆盖**默认驱动
      （驱**输出侧**的 `default_ready`）**不**属此列 —— 判据与护栏 **R8** 的
      「输出侧 / 输入侧」角色分类同口径；⇒ ⇒ ⚠️ 「**可覆盖**」是**人工判据**
      ⇒ ⇒ **R8 只按角色分类**，判不出可覆盖性）
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **未收口**者（**2026-11-04 起该集合为空**）⇒ ⇒ 其
      ready 链**曾**是死路 ⇒ ⇒ **曾属**违规（⇒ ⇒ 该集合现为空 ⇒ ⇒ 本句为
      **历史陈述**）；
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **已收口**者（mux / arbiter / join / filters /
      combine_with · `demux` · `fork`）⇒ ⇒ **不属『未收口』之列**（⇒ ⇒ 它们的
      ready 链已接通，**不是**死路）
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **但**『已收口』**不**等于『可硬编码常量
      ready』⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **上游侧**
      硬编码常量 ready **对所有转发类**违规（⇒ ⇒ 见本 capability 的 Scenario
      「转发类 MUST NOT 硬编码常量 ready」）
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **判别力缺口（如实
      登记）**：护栏 **R8** 判的是「上游 ready 是否用 `<<=` 驱动」⇒ ⇒ 「未用
      `<<=` / 无驱动」两种形态**可判** ⇒ ⇒ 「**用 `<<=` 但值为常量**」这一形态
      **当前无护栏**⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒

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
  ④ **改边（`<<=` / `set_src`）MUST 在 `Simulator` 构造**之前**完成**
      （⇒ ⇒ **可观测规则**：构造之后改边 ⇒ ⇒ 覆盖**不生效** ⇒ ⇒ 值读到的
      仍是**旧**的 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 运行期 MUST 只改**
      **值**（`sim.set_value`）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **「改值」是测试台的正常用法**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 不构成限制** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **但
      「改边须在构造前」是**易踩的坑** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ MUST 在契约里
      明写 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **确切机制尚未完全确定**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **（实测：改边**不生效** ⇒ ⇒ 且
      `reinitialize()` **不**解决 ⇒ ⇒ 且源是**旧**节点时**同样不生效**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 故 MUST NOT 在契约里断言
      未验证的机制** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 只写可观测规则**））
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

### Requirement: 转发类组合子的 ready 所有权 MUST 是**默认值 + 可覆盖**

转发类组合子（mux / arbiter / join / filters / combine_with）的 ready
所有权 MUST 采用**形态 E（默认值 + 可覆盖）** ⇒ ⇒ 与 fifo 类**同型**
⇒ ⇒ ⇒ 心智模型统一 ⇒ ⇒ **无**两阶段 / 单阶段的分别。

⇒ ⇒ ⚠️ **转发类在收口前的现状 = 「上游恒不 ready = 死路」**（⚠️ **2026-11-04
原地订正**：该陈述记录的是**收口前**的状态 ⇒ ⇒ 收口后（形态 E）未覆盖时
上游 ready = **1**，见 Scenario「未覆盖时 MUST 保持『恒就绪』」）⇒ ⇒ 故本条是
「把死路变成活路」⇒ ⇒ **零破坏**成立（⇒ ⇒ 逐条见 Scenario「转发类现状是死路」）。

⇒ ⇒ ⚠️ **三条实现纪律** ⇒ ⇒ 逐条见 Scenario「转发类的三条实现纪律」。

⇒ ⇒ ⚠️ **消费端表达「不就绪」的两种方式**（**都合法**）⇒ ⇒ 逐条见 Scenario
「消费端表达不就绪的两种方式」。

⇒ ⇒ ⚠️ **`connect_ready` MUST 是幂等的**（⇒ ⇒ 重复调用 MUST NOT 报错）
⇒ ⇒ 逐条见 Scenario「`connect_ready` MUST 是幂等的」。

#### Scenario: 转发类现状是死路

- **WHEN** 检查转发类组合子**未**接消费端 ready 时的行为
- **THEN** MUST 承认**收口前**其上游 ready **恒为 0**（实测：不调
      `connect_ready` ⇒ ⇒ 上游 ready 恒为 0 ⇒ ⇒ 调用方永远推不动）
      ⇒ ⇒ ⚠️ **2026-11-04 原地订正**：**收口后**（形态 E）未覆盖时上游 ready
      = **1**（⇒ ⇒ 见 Scenario「未覆盖时 MUST 保持『恒就绪』」）⇒ ⇒ ⇒ ⇒
      本条记录的是**历史现状**，**不是**现行契约
- **AND** ⇒ ⇒ 本 capability 的转发类条款 MUST 被视为「把死路变成活路」
      ⇒ ⇒ **MUST NOT** 被当作**行为变更**
- **AND** ⇒ ⇒ **零破坏**成立（⇒ ⇒ 未接背压时的行为 = **既有旧语义**
      ⇒ ⇒ **既有旧语义** = 上游 ready 恒为 **1**）

#### Scenario: 转发类的三条实现纪律

- **WHEN** 实现转发类组合子的 ready 所有权
- **THEN** MUST **立即**建立「默认恒就绪」的 ready 节点（⇒ ⇒ 源 MUST 是
      **非 literal** ⇒ ⇒ 可被覆盖）
- **AND** 源的**节点** MUST **持久有效**（⇒ ⇒ MUST NOT 是**临时对象**
      ⇒ ⇒ 临时对象的 `get_lnode` **无效** ⇒ ⇒ 驱动**未建立**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 具名的局部 `ch_bool` 与「结果对象持有成员」两种
      形态**都**满足（框架**无引用计数、无析构** ⇒ ⇒ 节点不随 C++ 对象
      析构而失效）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「持有者是谁」不是判据
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 节点是否持久才是判据**）
- **AND** MUST 用 **`<<=`** 驱动上游的 ready 字段（**MUST NOT** 用
      `operator=`）⇒ ⇒ ⚠️ 因为 `ch_stream` 的**拷贝与原对象共享同一 ready
      节点** ⇒ ⇒ ⇒ `<<=` 能**穿透副本**驱动到**调用方**
- **AND** MUST NOT 有 `input_stream.ready = true` 这类**局部常量**充当
      **上游侧**（输入侧）ready 源（⇒ ⇒ 那是「无条件忽略」）
      ⇒ ⇒ ⚠️ **2026-11-04 原地订正**：形态 E 的**输出侧可覆盖**默认驱动
      （`ch_bool default_ready(true); result.ready <<= default_ready;`）
      **不**属此列 ⇒ ⇒ ⇒ ⇒ ⇒ 它**正是**形态 E 的合法形态
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 判据与护栏 **R8** 的「输出侧 / 输入侧」角色分类同口径
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ 「**可覆盖**」是**人工判据**（⇒ ⇒ **R8 只按角色分类**
      ⇒ ⇒ 判不出可覆盖性）

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
- **THEN** MUST NOT 存在 `input_stream.ready = true` 这类**局部常量**充当
      **上游侧**（输入侧）ready 源（⇒ ⇒ 那是「无条件忽略消费端 ready」）
      ⇒ ⇒ ⚠️ **2026-11-04 原地订正**：**输出侧**的**可覆盖**默认驱动
      （形态 E 的 `default_ready`）**不**属此列 ⇒ ⇒ ⇒ ⇒ ⇒ 护栏 **R8** 的
      「输出侧 / 输入侧」角色分类即按此口径判定
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ 「**可覆盖**」是**人工判据**（**R8 判不出**）
- **AND** MUST 有**自动化护栏**检出（⇒ ⇒ 以**行为 / 结构**判据实现 ⇒ ⇒
      **MUST NOT** 以「源码含常量 `true`」这类**词法**判据实现）
- **AND** 该护栏 MUST 有**自检**（⇒ ⇒ 注入一处「硬编码常量 ready」
      ⇒ MUST 转红）

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

### Requirement: 宽度适配类组合子 MUST 可被仿真且 ready 所有权与转发类同型

- **WHEN** 使用 `chlib` 的 stream 宽度适配组合子
      （`stream_narrow_to_wide` / `stream_wide_to_narrow` / `stream_width_adapter`）
- **THEN** 它们 MUST **构建出可实例化的图**（构造 `Simulator` MUST NOT 崩溃）
- **AND** 其 ready 所有权 MUST 与「转发类组合子的 ready 所有权 MUST 是**默认值 +
      可覆盖**」**完全相同**（形态 E + 节点持久 + 用 `<<=` 驱动上游 ⇒ **不重述**，
      见该 Requirement）
- **AND** 输出 `valid` / `payload` MUST 是**寄存器**（⇒ `tick()` 后即可观测）

#### Scenario: 三个适配器可被仿真

- **WHEN** 用任意合法位宽组合构造三个适配器并创建 `Simulator`
- **THEN** MUST NOT 崩溃（`SIGSEGV`）
- **AND** 上游 `ready` MUST 由**调用方持有的 handle** 观测到（⇒ 调用方 MUST
      先 `as_master()`，否则 `ch_stream::ready` 无节点 ⇒ 组合子的驱动无处置放）
- **NOTE**: 「输出 MUST 是寄存器」是**可测试性约束**，不是实现偏好 ——
      组合输出在本仓 `set_input → tick → get_port_value` 读模型下**不可观测**
      （读到沿后组合值，而正确值出现在沿前）⇒ 写不出有效的 parity 判据
      ⇒ 见 ADR-064 D3 / §1.4。

#### Scenario: 累积型适配器的宽字 MUST 含最后一个窄字且组间隔离

- **WHEN** `stream_narrow_to_wide<ch_uint<32>, ch_uint<8>>` 连续接收
      `0xF0, 0xE1, 0xD2, 0xC3`（下游恒就绪），随后接收 `0x0F, 0x1E, 0x2D, 0x3C`
- **THEN** 第一个宽字 MUST 是 `0xC3D2E1F0`（低字节先到）
- **AND** 第二个宽字 MUST 是 `0x3C2D1E0F`
- **AND** 第二个宽字 MUST NOT 含第一组的任何位 ⇒ 累积器 MUST 在组完成时**清零**
- **NOTE**: 测试数据 MUST 满足「组 1 字节 **或** 组 2 字节 ≠ 组 2 字节」——
      否则按位或会**掩盖**跨组污染（实测 `0xA0 | 0xB0 == 0xB0` ⇒ 该变异恒绿）。

#### Scenario: 背压 MUST 到达上游且不丢已产生的宽字

- **WHEN** 宽字已装载（`valid=1`）而下游 `ready=0`
- **THEN** 上游 `ready` MUST 为 `0`
- **AND** 输出的 `valid` / `payload` MUST 保持不变（MUST NOT 被新数据覆盖）
- **AND** 下游恢复 `ready=1` 后，未取走的宽字 MUST 仍可被取走（MUST NOT 丢失）

#### Scenario: 拆分型适配器 MUST 按低字节优先输出

- **WHEN** `stream_wide_to_narrow<ch_uint<8>, ch_uint<32>>` 接收 `0xA1B2C3D4`
- **THEN** 依次输出 `0xD4` / `0xC3` / `0xB2` / `0xA1`
- **AND** 输出 `valid` MUST 保持到 `ready`，且输出满时上游 `ready` MUST 为 `0`
