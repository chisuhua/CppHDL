# Capability: rtl-hierarchical-output

## MODIFIED Requirements

### Requirement: 层级粒度 MUST 由显式声明决定（默认 inline）

组件是否成为一个独立 Verilog module，MUST 由**显式声明**决定；未声明者 MUST **inline**
（不产生 module / 实例化 / 隐式端口）。其端口的落点 MUST 按「**最近的成层祖先 module**」
分两格：**该祖先 = RTL 顶层** ⇒ 走**顶层端口面契约**（`default_clock` → `default_reset` →
**根组件自有 `__io`**：无 driver 的 `ch_in` ⇒ 顶层 `input`；**根自有的** `ch_out` ⇒ 顶层
`output`；**后代 `ch_out` MUST NOT 上顶层**（降为体内 `logic`））；**该祖先 = 非顶层成层
module** ⇒ `ch_out` 是该 module **体内的普通信号**（**不**上 RTL 顶）。

⇒ ⇒ **端口表收缩发生在两处**：① **顶层端口面**（框架 clock/reset + 根组件自有 `__io` ·
**与粒度无关**）② **成层子树的非根 module**（由本条定）。

⇒ ⇒ MUST NOT 由框架按端口数 / 节点数**自动推断** ⇒ ⇒ 粒度是用户的领域决策。

#### Scenario: 未声明的组件 MUST 被 inline

- **WHEN** 一个组件未声明为「成层」且被用作子组件
- **THEN** 层次化产物 MUST NOT 为它生成 module 或实例化语句
- **AND** 其端口落点 MUST 按「最近的成层祖先」分两格：祖先 = RTL 顶层 ⇒ **仅根组件自有的**
      `ch_out` 成为顶层 `output`；**后代** `ch_out`（**含已被父层消费的**）MUST 降为体内
      `logic`（MUST NOT 上 RTL 顶）；祖先 = 非顶层成层 module ⇒ `ch_out` 是该 module
      体内的普通信号（MUST NOT 上 RTL 顶）

#### Scenario: 声明 MUST 可在类级与实例级表达

- **WHEN** 组件在**类级**声明为「成层」
- **THEN** 它的**每个**实例 MUST 成层，除非该实例被**显式**覆盖为 inline
- **AND** 当一个未声明的组件被**显式**声明为成层时，该实例 MUST 成层

### Requirement: 不属于任何组件的节点 MUST 归顶层 module

不属于任何组件实例的节点（`owner == nullptr`）MUST 归**顶层 module** ⇒ ⇒ MUST NOT 被拒绝。
其处置 MUST 按**两类**区分：

| 子类 | 条款 |
|---|---|
| `default_clock` / `default_reset`（context 级） | MUST 是**顶层端口** · MUST NOT 触发 `CHWARN` |
| 其余 `owner == nullptr`（**用户直接创建**） | MUST 归**顶层 module 的体内**（**MUST NOT** 是顶层端口）· MUST **保留 `CHWARN` 点名** · MUST NOT 被拒绝 |

⇒ ⇒ **归属空洞必须可观测**：用户直接创建的节点 MUST 在 `HierarchyEmitResult.warnings`
里被点名（MUST NOT 只能靠抓 stderr 才可观测）。

#### Scenario: 含寄存器的设计 MUST 仍能层次化

- **WHEN** 一个设计含 `ch_reg`（因而依赖 context 级 `default_clock` / `default_reset`）
- **THEN** 层次化产物 MUST 正常产出
- **AND** `default_clock` / `default_reset` MUST 作为**顶层 module** 的端口出现
- **AND** `default_clock` / `default_reset` MUST **NOT** 触发 `CHWARN`
      （按设计不归属任何组件 ⇒ 告警会让每个含寄存器的设计恒响）
- **AND** 只有「用户在 `main()` 里不经组件建的节点」MUST 触发 `CHWARN`

#### Scenario: 用户直接创建的无归属节点 MUST 是顶层体内的信号

- **WHEN** 一个设计在 `main()` 里创建了不属于任何组件实例的节点（`owner == nullptr`，
      **且**不是 `default_clock` / `default_reset`）
- **THEN** 该节点 MUST **NOT** 出现在顶层 module 的**端口表**里
- **AND** `HierarchyEmitResult.warnings` MUST **非空**且**点名**该节点
- **AND** 发射 MUST **成功**（`ok == true`）⇒ ⇒ MUST NOT 因该节点被拒绝

### Requirement: 成层且非根的 module 端口 MUST 覆盖自身 `ch_in` / `ch_out`，且每个端口可追溯

- **MODIFIED**：拒绝的可观测形态按 ADR-058 §2.3 ⑤ = **错误结果**（`ok = false` +
  `error` 点名 + `files` 为空），与「发射器内 MUST NOT 依赖异常」口径一致。
- **MODIFIED（ADR-066 决策 ③ · 2026-11-08）**：「造网」义务**收窄**为
  「父侧消费点**不是**父 `output` 端口」时 ⇒ ⇒ 消费点**就是**父 `output` 端口时
  MUST NOT 强制造网 ⇒ ⇒ 唯一权威 = `docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md`
  决策 ③。

**成层且非根**的 module，其端口集合 MUST **⊇** 它自己的 `ch_in` ∪ `ch_out`
⇒ ⇒ **是否已被父层驱动不影响它是否成为端口**（emitter MUST 忽略 `inputimpl::driver()`）。
且每个端口 MUST 可追溯到**三类**来源之一：**自身端口** · **隐式端口** ·
**时序基础设施**（clock / reset）。

⇒ ⇒ **RTL 顶层 module MUST 走顶层端口面契约**（框架 `default_clock` / `default_reset` +
**根组件自有 `__io`**），MUST NOT 套用本条。
⇒ ⇒ ⚠️ 根的**自有 IO 段**（即 `__io` 声明的那些端口 · **不含**框架 `default_clock` /
`default_reset`）MUST **==** 自身 `ch_in` ∪ `ch_out`（**==** 而非 **⊇** ⇒ ⇒ 故它仍不套用本条）。
⇒ ⇒ **自驱动**：成层组件自己驱动自己的 `ch_in` ⇒ MUST **返回 `ok = false`** 并点名该端口。

⇒ ⇒ `PortProvenanceOrigin` 的**顶层合法取值 MUST 只有** `Self` / `ClockReset`
（`Bubbled` MUST NOT 存在：悬空后代输入恒被拒绝 ⇒ 无冒泡端口；
`InlineFolded` MUST NOT 存在：后代 `ch_out` MUST NOT 出现在**任何** module 的端口表里
（**含根**）⇒ 无 inline 折叠端口）。

#### Scenario: 已被父层驱动的 `ch_in` MUST 仍是 module 端口

- **WHEN** 一个成层组件的 `ch_in` 由父层表达式驱动
- **THEN** 该 `ch_in` MUST 出现在该组件的 module 端口表里
- **AND** 子 module 内 MUST **NOT** 出现 `assign <该输入> = ...`
- **AND** 父层的驱动 MUST 表现为**实例端口连接**

#### Scenario: 父层消费子输出 MUST 有连接网

- ⚠️ **收窄（ADR-066 决策 ③ · 2026-11-08）**：本条的「必须造网」义务**仅在「消费点不是父
  `output` 端口」时**成立 ⇒ ⇒ 消费点**就是**父 `output` 端口（直通）时 MUST NOT 强制造网
  ⇒ ⇒ **唯一权威 = `docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md` 决策 ③**
- **WHEN** 父 module 消费一个成层子组件的 `ch_out`
- **THEN** 若**消费点不是**父 module 的 `output` 端口（父内运算 / 父内 reg / 子 input 的表达式）
      ⇒ 父 module MUST 为该端口声明一根 `logic` 网 · MUST 由 `.port(net)` 驱动
      · MUST NOT 用 `assign` 接表达式
- **AND** 若**消费点就是**父 module 的 `output` 端口（直通）
      ⇒ MUST NOT 强制造网（子 output MAY 直接作为该 `.port(...)` 的实参）
- **AND** ⚠️ 该「必须造网」的要求 MUST NOT 施加到**接 input 的** `.port(...)`（可为表达式）

#### Scenario: 父侧带运算 MUST 合成父侧网且操作数 MUST 读该网（新增 · ADR-066 决策 ①）

- **WHEN** 父层对成层子组件的 `ch_out` 的使用**带运算 / 表达式**（消费点不是父 `output` 端口）
- **THEN** 发射 MUST 成功（`ok = true`）且 MUST 产出产物
- **AND** 父 module **作用域内** MUST 声明一根 `logic` 网，子 output MUST 以 `.port(net)` 连接该网
- **AND** 父侧运算的操作数 MUST 读**该网**（⇒ 网名 MUST = **驱动端点的 ctx 全局名**；
      MUST NOT 用被驱动节点自体名 ⇒ 否则自指 + 双驱动）
- **AND** 消费点是父 module 的 **`input` 端口**时 MUST **被拒绝**（物理不可行）
      ⇒ ⇒ **ADR-066 决策 ① 排除项**
- **AND** 该形态 MUST 与扁平产物**逐拍等价**

#### Scenario: 父侧运算消费子非端口端点 MUST 读父侧落点网名（新增 · ADR-066 决策 ②）

- **WHEN** 父层对子组件的**非端口端点**（`ch_reg` / bundle 字段 / op 节点）的使用带运算
- **THEN** 父侧表达式的标识符 MUST 是**父 module 作用域内可解析**的名字
      （= 父侧落点网名）
- **AND** MUST NOT 引用**仅在子 module 内声明**的名字（判据 = 该标识符在**父 module
      作用域内不可解析**），也 MUST NOT 直接引用子 module 的**端口名**
      （⚠️ 子侧合成端口名 = `__x_<n>`（ADR-060 §2.2 v1.8）⇒ ⇒ **端口名 ≠ 网名**）
      ⇒ ⇒ ⚠️ **措辞订正（Q6 · 2026-11-08）**：旧措辞「MUST NOT 引用子 module 的
      **内部信号名**」与「落点网名 = 驱动端点的 ctx 全局名」**自相矛盾**（两者是**同一串**
      ⇒ ⇒ 逐字执行会把**正确实现**判成违规）⇒ ⇒ 判据 MUST 用「**仅在**子 module 内声明」
      （= 父作用域可解析）
- **AND** 产物 MUST 通过 ADR-060 §2.5 的 lint 门（零 `%Error`）

#### Scenario: 自驱动 ch_in MUST 被拒绝

- **WHEN** 一个成层组件在自己的体内驱动自己的 `ch_in`
- **THEN** emitter MUST **返回 `ok = false`**，`error` MUST 点名该 `ch_in`
- **AND** `files` MUST 为空
- **AND** MUST NOT 在子 module 内对该 `input` 发 `assign`

#### Scenario: RTL 顶层 module MUST 走扁平规则

- ⚠️ **Scenario 标题**保留原样（「走扁平规则」⇒ ⇒ 标题与新口径不再逐字相符 ⇒ ⇒ **保留是为可追溯性**
  ⇒ ⇒ **口径以本 Scenario 正文 + `docs/adr/ADR-062-hierarchical-unit-interface-typing.md` §5 为准**）
- ⚠️ **MODIFIED（ADR-062 v1.7 · 2026-11-15 · 据实施期实测追认）**：本 Scenario 原写
  「顶层端口表 MUST 与扁平产物**逐字逐序相同**」⇒ ⇒ 该句**对任何部分成层设计结构性不可满足**
  （成层子组件的 `ch_out` 在扁平侧恒为顶层端口、在层次化侧恒不是 ⇒ = 观测盲区）
  ⇒ ⇒ 收窄为两段 ⇒ ⇒ **唯一权威 = `docs/adr/ADR-062-hierarchical-unit-interface-typing.md` §5**
- **WHEN** 对比一个设计的层次化产物与扁平产物的**顶层端口表**
- **THEN** **根 `io()` 端口段** MUST **逐字逐序**相同
      （clock → reset → 无 driver 的 `input` → **根组件自身**的 `ch_out`
      ⇒ ⇒ ⚠️ **不含** inline 后代的 `ch_out` —— 那些**不上**顶层）
- **AND** 层次化侧**其余**顶层端口 MUST 在扁平侧**逐字存在**（集合 **⊆**）
- **AND** 扁平侧 MAY 多出**成层子组件**的 `ch_out`（它们在层次化顶层**不可观测**
      ⇒ = `include/hierarchy_equivalence.h` 明写的「观测盲区」）
- **AND** **段归属 MUST 以 `PortProvenanceOrigin` 为准**（顶层**只允许** `Self` /
      `ClockReset` 两个取值）⇒ ⇒ **MUST NOT** 以**名称前缀**推断

#### Scenario: 非根 module MUST NOT 抬 inline 后代的 `ch_out`

- **WHEN** 一个**非顶层**成层 module 内含被 inline 的子组件
- **THEN** 该子组件的 `ch_out` MUST 是该 module 体内的普通信号
- **AND** MUST NOT 出现在该 module 的端口表里

#### Scenario: 未被消费的子 output 端口 MUST 以空连接出现

- **WHEN** 一个成层子 module 的某个 `ch_out` 端口**未被父层消费**
- **THEN** 该端口 MUST 仍然是该子 module 的 `output` 端口（⇒ 端口表**不收缩**）
- **AND** 父 module 的实例化语句 MUST 以**空连接**（`.port()`）接上它
- **AND** 该产物 MUST 通过 **ADR-060 §2.5 的 lint 门**（退出码 + 无 `%Error`；
      ⚠️ 空连接在默认集下本就合法 ⇒ MUST NOT 靠放宽门来通过）
- **AND** ⚠️ **子 input 端口 MUST NOT** 用空连接糊过去 ⇒ ⇒ 未驱动的**后代**子 input MUST
  依「悬空后代子输入 MUST 被响亮拒绝」条被**响亮拒绝**（`ok = false` + 点名）
  ⇒ ⇒ MUST NOT 冒泡成顶层 input（⇒ ⇒ 否则其值是未定义值 X）

### Requirement: 跨层端口策略 MUST 是单一权威的线性枚举，且各取值的失败语义 MUST 明确

跨层非端口边的处理策略 MUST 由**唯一**的线性枚举 `PortPolicy` 承载
（`SynthesizeWarn` / `SynthesizeFatal` / `Reject`）
⇒ ⇒ MUST NOT 存在第二份同义枚举副本
⇒ ⇒ 「合成」MUST 是**纯内存 phase**、**先于任何文件 I/O**
⇒ ⇒ 悬空输入的策略 MUST 由 `UndrivenPolicy`（**仅** `Reject`）承载
⇒ ⇒ `Bubble` 取值 MUST **NOT** 存在（后代悬空输入恒被**响亮拒绝** ⇒ 无「冒泡」档可承载）。

⚠️ **诊断的计数与列举单位 MUST 是「边」（需合成的跨 module 边），MUST NOT 是
「合成物」（1 条边常合成 k 个端口 + 1 根网）** ⇒ ⇒ 否则「首行计数」与「实际列出的
条目数」**必然**不等（用户可见的错报）。

⚠️ **每条边级诊断 MUST 点名该边**（形如 `from <node> (<module>) -> to <node> (<module>)`；
`(<module>)` = 该 module 在层次树里的**实例路径**）⇒ MUST NOT 只报合成物名
（用户据此无法定位是哪条跨 module 边）。

#### Scenario: SynthesizeWarn 是默认策略

- **WHEN** 未显式指定策略
- **THEN** 隐式端口 MUST 被**合成**并产出产物
- **AND** MUST 对**每一条**被合成的边产出一条诊断：点名该边**与**它合成出的产物名
  （端口名 / 网名）
- **AND** 该诊断 MUST **同时**出现在 `result.warnings`（公开字段）与 `CHWARN` 日志中
  ⇒ ⇒ 「合成了几条边」MUST NOT 只能靠抓 stderr 才可观测
- **AND** MUST 在产物内为**发生合成**的端口写注释

#### Scenario: 未发生合成时产物 MUST 不含合成注释

- **WHEN** 设计内**没有**任何跨 module 边被合成
- **THEN** 产物 MUST NOT 含任何合成端口注释
- **AND** 「默认（全 inline）产物与扁平产物逐字节一致」这条判据 MUST 仍然成立

#### Scenario: SynthesizeFatal 先合成完再失败

- **WHEN** 策略为 `SynthesizeFatal` 且设计含**至少两条**可合成的边
- **THEN** 发射 MUST **失败**且 MUST NOT 产出产物文件
- **AND** 失败诊断 MUST 列出**全部**被合成的边与合成端口名（不止第一条）
- **AND** 列出的**条目数** MUST == **边数**，且 MUST == 诊断首行给出的计数
- **AND** MUST NOT 留下临时文件
- **AND** MUST NOT 触碰既有同名文件

#### Scenario: Reject 只报第一条

- **WHEN** 策略为 `Reject` 且设计含可合成的边
- **THEN** 发射 MUST 失败且 MUST NOT 产出产物文件
- **AND** 诊断 MUST 报出**第一条需合成的跨 module 边**（**边级**记录，不是合成物名）
- **AND** 诊断 MUST NOT 含**合成端口名 / 产物名**（本档不合成 ⇒ ⇒ 无从列出）
- **AND** 诊断首行给出的「**列出的**条目数」MUST == 实际列出的条目数（= 1）；
  可合成边的**总数** MAY 另行给出 ⇒ 诊断 MUST NOT 把总数冒充成「列出的条目数」

## REMOVED Requirements

### Requirement: 悬空子输入 MUST 向上冒泡为顶层输入

**Reason**: 冒泡是「静默降格」的一种 —— 它把一个未驱动的**后代**输入提升为**顶层端口**，
从而把「用户漏接线」变成「看起来合法的新顶层端口」。产品面要求可机器消费的端口面
（声称的端口表 == 产物端口表）⇒ ⇒ 冒泡端口既不可预测（编号随 eval-list 序漂移）又
无语义（顶层接线者不知道它属于哪个组件）⇒ ⇒ 改为**响亮拒绝**（`ok = false` + 点名），
并把 `UndrivenPolicy::Bubble` 取值一并退役（唯一取值 = `Reject`）。

**Migration**: 含未驱动**后代**输入的设计不再能发射 ⇒ ⇒ 调用方 MUST 二选一：
① 由父层接上驱动源（真实的跨层连线）② 明确 tie-off 常量（那是**有**内部 driver 的输入，
不触发本条）。**根自己**的悬空 `ch_in` 不受影响（仍 MUST 是顶层端口）。
`HierarchyEmitOptions::undriven_policy` 的默认值由 `Bubble` 改为 `Reject`。

## ADDED Requirements

### Requirement: 悬空后代子输入 MUST 被响亮拒绝，MUST NOT 静默降格为顶层输入或体内未驱动 wire

**根自己**的悬空 `ch_in` ⇒ ⇒ MUST **仍是顶层端口**（与扁平产物一致）。
**后代**（**含** inline 折叠进根的组件）的悬空 `ch_in` ⇒ ⇒ 层次化发射 MUST **失败**：

- ① 发射 MUST 返回 `ok = false`；
- ② `error` MUST 点名**全部**悬空后代输入（端口名 + module + 层级）；
- ③ `files` MUST 为空（失败路径 MUST NOT 产出任何文件）；
- ④ MUST NOT **静默降格**为：**顶层端口** / 体内**未驱动 wire** / **tie-off**。

⇒ ⇒ 三种静默降格形态**都是**「读 X 或静默改行为」⇒ ⇒ 与仓内「响亮拒绝」纪律相反。
⇒ ⇒ `UndrivenPolicy` 的**唯一取值 MUST 是 `Reject`** ⇒ ⇒ 本条正文 MUST NOT 留
「冒泡是否可选」的余地。

#### Scenario: 根自己的悬空 `ch_in` MUST 仍是顶层端口

- **WHEN** 根组件自己声明了一个 `ch_in`，而它**没有**内部 driver
- **THEN** 它 MUST 出现在顶层 module 的端口表里（`input`）
- **AND** 发射 MUST **成功**（`ok == true`）
- **AND** MUST NOT 因它触发拒绝

#### Scenario: 后代悬空 `ch_in` MUST 被拒绝

- **WHEN** 一个**成层**子组件的 `ch_in` 没有驱动源
- **THEN** 发射 MUST 返回 `ok == false`
- **AND** `error` MUST 点名该端口 · 其模块 · 其层级
- **AND** `files` MUST 为空
- **AND** MUST NOT 把它提升为顶层 `input`

#### Scenario: inline 折叠进根的后代悬空 `ch_in` MUST 同样被拒绝

- **WHEN** 一个**被 inline**（折叠进根 module）的子组件含未驱动的 `ch_in`
- **THEN** 发射 MUST 返回 `ok == false`（该输入**不属于根自有 IO** ⇒ ⇒ 与根自己的悬空
      `ch_in` 不同）
- **AND** `error` MUST 点名该端口 · 该子组件的 module 路径 · 其层级
- **AND** `files` MUST 为空
- **AND** MUST NOT 把它当顶层 `input` 产出（⇒ ⇒ 否则「拒绝合法设计」的镜像错误：
      把**非法**设计当**合法**）

#### Scenario: 拒绝 MUST 列出全部悬空后代输入

- **WHEN** 设计里有**两个**成层实例各含悬空 `ch_in`
- **THEN** `error` MUST 同时点名**两个**端口（只报第一条 ⇒ 判据转红）
- **AND** 根 module 自己的悬空 `ch_in` MUST NOT 触发拒绝 ⇒ ⇒ 产物 MUST 仍被产出
