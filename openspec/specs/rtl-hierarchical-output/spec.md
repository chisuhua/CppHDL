# rtl-hierarchical-output Specification

## Purpose
定义 CppHDL **层次化 RTL 输出**的契约 —— 一份设计产出**多个** Verilog module
（而非单个扁平 `module top`）时所必须成立的性质：

- **粒度**：哪些组件成为独立 module MUST 由**用户显式声明**决定（默认 **inline**）
  ⇒ ⇒ MUST NOT 由框架按端口数 / 节点数自动推断（粒度是用户的领域决策）。
- **边界**：module 边界 = **所有权边界**（节点归属哪个组件 ⇒ 它就属于哪个 module）。
- **命名**：命名权威 MUST 是 **ctx 级全局表** ⇒ ⇒ 顶层端口名 MUST 与扁平产物**逐字相同**
  （MUST NOT 发明 per-module 局部命名）。
- **跨层连线**：跨层的非端口边 MUST 按**方向**合成隐式端口（默认告警）；悬空子输入 MUST
  向上**冒泡**为顶层输入。
- **等价性**：层次化产物 MUST 与扁平产物**等价**，且 MUST 可**机械判定**
  （默认全 inline ⇒ 逐字节一致；成层时 ⇒ 映射表上的观测集合逐拍全等）。
- **接口与策略**：单元接口 MUST 限于 `ch_uint` / `ch_bool`（+ 框架供给的 clock/reset）；
  跨层端口策略与**粒度**各 MUST 只有**一份**线性枚举权威（MUST NOT 出现第二份副本）。
- **错误载体**：失败 MUST 由**结果结构体**承载（`ok=false` + `error` 点名 module / 端口）
  ⇒ ⇒ 发射器内 MUST NOT 抛；分析层的前置校验若抛，MUST 由入口统一转换。
- **产物级护栏**：`scripts/verify_hierarchical_rtl.sh` 的 **R1–R7**（lint 门 = ADR-060 §2.5）
  MUST 能独立于发射器判定产物（lint / pin 名 / 宽度 / 确定性 / 拒绝 / 树同构 / 路径分离）。

**为什么需要**（ADR-058 / 060 / 061 / 062）：现状唯一 RTL 出口 `toVerilog()` 固定输出
**一个** `module top` ⇒ ⇒ 想要层次化只能「每层手写 wrapper + 手写顶层例化」
⇒ ⇒ 手工量随层次深度线性增长，且每处手工连线都是一次可错的机会。

⚠️ **边界**：本 capability 只管**产物结构、端口、命名与等价性判定** ⇒ ⇒ **不**管节点归属
与实例命名本身（那是 `component-ownership-and-naming`）· **不**管仿真语义。

## Requirements

### Requirement: 层级粒度 MUST 由显式声明决定（默认 inline）

组件是否成为一个独立 Verilog module，MUST 由**显式声明**决定；未声明者 MUST **inline**
（不产生 module / 实例化 / 隐式端口）。其端口的落点 MUST 按「**最近的成层祖先 module**」
分两格：**该祖先 = RTL 顶层** ⇒ 照扁平规则（无 driver 的 `ch_in` ⇒ 顶层 `input`；
**任意** `ch_out` ⇒ 顶层 `output`）；**该祖先 = 非顶层成层 module** ⇒ `ch_out` 是该
module **体内的普通信号**（**不**上 RTL 顶）。

⇒ ⇒ **端口表收缩（无人消费的子输出不上 RTL 顶）MUST 只发生在成层子树**。

⇒ ⇒ MUST NOT 由框架按端口数 / 节点数**自动推断** ⇒ ⇒ 粒度是用户的领域决策。

#### Scenario: 未声明的组件 MUST 被 inline

- **WHEN** 一个组件未声明为「成层」且被用作子组件
- **THEN** 层次化产物 MUST NOT 为它生成 module 或实例化语句
- **AND** 其端口落点 MUST 按「最近的成层祖先」分两格：祖先 = RTL 顶层 ⇒ 照扁平规则
      （任意 `ch_out` ⇒ 顶层 `output`，**包括已被父层消费的** ⇒ MUST NOT 降成纯内部 wire）；
      祖先 = 非顶层成层 module ⇒ `ch_out` 是该 module 体内的普通信号（MUST NOT 上 RTL 顶）

#### Scenario: 声明 MUST 可在类级与实例级表达

- **WHEN** 组件在**类级**声明为「成层」
- **THEN** 它的**每个**实例 MUST 成层，除非该实例被**显式**覆盖为 inline
- **AND** 当一个未声明的组件被**显式**声明为成层时，该实例 MUST 成层

### Requirement: 成层判定 MUST 与装配点解耦，但组件 MUST 登记进组件树

- **MODIFIED**：拒绝的可观测形态 = **错误结果**（ADR-058 §2.3 ⑤ ⇒ 对外唯一载体是
  `HierarchyEmitResult`）⇒ ⇒ 发射器内 MUST NOT 依赖异常（会落进 `print()` 的
  `catch(...)` ⇒ 静默失败）⇒ ⇒ 本条的拒绝判据 MUST NOT 表述为「向调用方抛出」
  （与实现及本 capability 的 Purpose 不符）。

「是否成层」MUST 由**声明**决定，MUST NOT 依赖装配点的位置（⇒ 允许在**无 active parent**
的位置装配）；但被装配的组件 MUST **登记进组件树**（`ch_device` / `ch_module` / `add_child`）
⇒ ⇒ 否则 emitter MUST **以错误结果拒绝**（`ok = false` + `error` 点名；
MUST NOT 静默并入顶层 module）。

#### Scenario: 无 active parent 处的装配 MUST 仍能成层

- **WHEN** 一个声明为成层的组件在没有 active parent 的位置被装配
- **THEN** 它 MUST 按其声明成层，且其节点的归属 MUST 是它自己
- **AND** 它的**名字** MUST 由装配方**自己算成全路径**（`父->name() + "." + 局部名`）
      后传给构造函数 ⇒ ⇒ MUST NOT 依赖 `add_child` 补前缀
      （⇒ `add_child` 只 `push_back`、不碰 `name_`，`name_` 由 **ctor 实参逐字**决定）
- **AND** `add_child` MUST **先于** `build(ctx)`（⇒ 让 `parent_` 在建节点前就位），
      且装配方 MUST 用 `add_child` 返回的 `shared_ptr` 续存

#### Scenario: 未登记进组件树的组件 MUST 被拒绝

- **WHEN** 一个组件被 `make_unique` + `build(ctx)` 装配，但**没有** `add_child`
      （⇒ 它的节点 `owner` 不在 `top` 的子树里）
- **THEN** 层次化 emitter MUST **返回 `ok = false`**，`error` MUST 给出 `add_child` 修法
- **AND** `files` MUST 为空（失败路径不产生任何文件）
- **AND** MUST NOT 静默把它并入顶层 module（⇒ 否则整块逻辑消失）

#### Scenario: 已登记但从未 build 的组件 MUST 被拒绝

> ⚠️ **权威载体**：本条与 capability `component-ownership-and-naming` 的「未 build 的子组件 MUST 被识别」是同一条义务的**两侧**（那边判「框架能识别」，这边判「emitter 必须拒绝」）⇒ ⇒ 改一处 MUST 同步另一处。

- **WHEN** 一个组件被登记进组件树，但**从未** `build()`
      （⇒ ⇒ 它**有节点、无端口** ⇒ ⇒ 端口只在 build 期创建）
- **THEN** 层次化 emitter MUST **返回 `ok = false`**，`error` MUST 点名该组件
- **AND** MUST NOT 发出一个**无端口**的 module
- **AND** MUST NOT 把它的节点当作空气（⇒ ⇒ 那会让它的逻辑**静默消失**）

### Requirement: 不属于任何组件的节点 MUST 归顶层 module

不属于任何组件实例的节点（`owner == nullptr`：context 级 `default_clock` / `default_reset`，
以及用户不经组件直接创建的节点）MUST 归**顶层 module** ⇒ ⇒ MUST NOT 被拒绝
⇒ ⇒ 且 MUST 有 `CHWARN` 点名（归属空洞必须可观测）。

#### Scenario: 含寄存器的设计 MUST 仍能层次化

- **WHEN** 一个设计含 `ch_reg`（因而依赖 context 级 `default_clock` / `default_reset`）
- **THEN** 层次化产物 MUST 正常产出
- **AND** `default_clock` / `default_reset` MUST 作为**顶层 module** 的端口出现
- **AND** `default_clock` / `default_reset` MUST **NOT** 触发 `CHWARN`
      （按设计不归属任何组件 ⇒ 告警会让每个含寄存器的设计恒响）
- **AND** 只有「用户在 `main()` 里不经组件建的节点」MUST 触发 `CHWARN`

### Requirement: 成层组件 MUST 形成独立 module，且 module 树等价于成层组件子树

层次化产物的 module 树 MUST 与「**被声明为成层的**」组件子树**一一对应**
（节点数、父子关系、实例名）⇒ ⇒ 被 inline 的组件 MUST NOT 出现在 module 树里。

#### Scenario: 三层设计的 module 数量

- **WHEN** 一个三层设计里三个组件**全部**被声明为成层
- **THEN** 产物 MUST 包含 **3 个 module** 与 **2 条实例化语句**
- **AND** MUST NOT 存在任何跨 module 边界的 `assign`

#### Scenario: 计数对但树形错 MUST 不通过

- **WHEN** module 数量与预期相同、但父子关系与组件子树不一致
- **THEN** 该产物 MUST 判定为**不通过**（结构同构判据）

### Requirement: 跨层非端口边 MUST 按方向合成隐式端口（默认告警）

- **MODIFIED**：⚠️ `PortPolicy::Reject` 与 `UndrivenPolicy::Reject` **同名** ⇒ ⇒ 本条
  引用时 MUST 写**全限定名**（`UndrivenPolicy::Reject` 管的是**悬空 `ch_in`**）。

跨模块但**不落在端口上**的边（bundle 成员跨层 / 父层改子层状态 / 子端口由父层表达式驱动）
MUST 按**边方向**合成隐式端口（父→子 = 子侧 input；子→父 = 子侧 output）⇒ ⇒
默认（`PortPolicy::SynthesizeWarn`）MUST 有 `CHWARN` 与生成文件内注释；
**`PortPolicy::SynthesizeFatal`** MUST **先合成完再失败**（诊断含**全部**被合成的边与端口名）；
**`PortPolicy::Reject`** MUST **首错即返**（同样 `ok = false` + `error` 点名）。

⚠️ **跨模块 `ch_mem` 的闸门 MUST 按三桶判定**（本条新增 ⇒ 取代原「跨模块存储器 MUST
响亮拒绝」的**单一**表述）⇒ ⇒ 判定 MUST 用**节点级归属比对**
（**持有 module** = `ch_mem` 本体的 inline 折叠后归属 · **使用 module** = mem 端口节点自身的
折叠后归属）⇒ ⇒

- **(i)** 持有 module ≠ 使用 module（= **mem 本体跨 module**）⇒ ⇒ MUST **响亮拒绝**
  （⇒ ⇒ 本条的「跨模块存储器 MUST 响亮拒绝」scenario 即**这一桶**）；
- **(ii)** 持有 module == 使用 module（= mem 本体与使用者同 module）**且** 跨 module src
  **仅**落在 `addr` / `en` / `wdata` 三个实参槽 **且** 该 src 的 owner 与使用 module **相邻**
  ⇒ ⇒ **MUST 支持**（⇒ ⇒ 见 ADDED 的「mem 端口实参跨层」条）；
- **(iii)** 持有 module == 使用 module 但 src owner **非相邻**（孙 / 兄弟）⇒ ⇒ MUST 响亮拒绝。

⇒ ⇒ 三桶 MUST **互斥且穷举** ⇒ ⇒ (ii) MUST 带**相邻性**约束（否则「mem 本体同 module ·
实参来自孙」的形态会同时满足 (ii) 与 (iii)）。

⚠️ **诊断 MUST 按桶给出正确归因** ⇒ ⇒ (i) 桶 MUST 说「mem 本体与使用者 MUST 同 module」
⇒ ⇒ **(iii) 桶 MUST NOT 说该句**（mem 本体**已在**同层 ⇒ ⇒ 说它会把用户引向**错误**改法）
⇒ ⇒ 两桶 MUST **分开**判（MUST NOT 一刀切删除 (i) 桶的归因）。

#### Scenario: 子→父方向 MUST 合成子侧 output

- **WHEN** 父层的输出由**子组件的成员寄存器**驱动
- **THEN** 层次化 MUST 在**子侧**合成一个 **output** 隐式端口
- **AND** 父层 MUST 通过该实例的输出取数

#### Scenario: 隐式端口的实现 MUST NOT 改写 IR

- **WHEN** 一个跨模块边需要「把驱动搬进子模块」
- **THEN** 实现 MUST 只在**发射期**做虚拟重定向
- **AND** MUST NOT 调用 `set_src` / `set_next` 改写 `lnodeimpl`
      （⇒ ⇒ 否则 `toVerilog()` 产物、解释器行为、两次生成的确定性**全部受影响**）

#### Scenario: 跨模块存储器 MUST 响亮拒绝

- **WHEN** 父层直接读取**子组件**持有的 `ch_mem`
- **THEN** 层次化 MUST **返回 `ok = false`**，`error` MUST 给出「存储器与使用者同层」的两条替代路径
- **AND** `files` MUST 为空（MUST NOT 留下跨模块存储器引用的产物）
- **AND** MUST NOT 静默产出跨模块的存储器引用

#### Scenario: 驱动源在更上层时 MUST 逐级合成 pass-through 输入端口

- **WHEN** 一个成层 module 的输入端口，其驱动节点的归属落在**更上层**的 module
      （含「不属于任何组件」⇒ 归顶层）
- **THEN** MUST **沿祖先链逐级**合成同名的 **input** pass-through 端口（每级一个）
- **AND** MUST NOT 拒绝（⇒ ⇒ 否则父 module 引用不到顶层网 ⇒ ⇒ 该输入**无源**）
- **AND** MUST NOT 把该输入直接接**表达式**（⇒ ⇒ 跨 module 的 `assign` 是非法 Verilog）

> ⚠️ **本条是「按方向分列」矩阵的第三轴**（ADR-060 §2.1 v1.4）⇒ ⇒
> **缺了它 ⇒ ⇒ 驱动源在顶层的输入会在中间层丢失 ⇒ ⇒ 行为分叉且无告警**。

### Requirement: 悬空子输入 MUST 向上冒泡为顶层输入

- **MODIFIED**：`UndrivenPolicy::Reject` 的适用范围**已收窄**（ADR-060 §2.4.2）：只对
  「inline 折叠后 module 归属为**非根成层 module**」的悬空 `ch_in` 生效 ⇒ ⇒ **根自己的**
  与**折叠进根**的悬空 `ch_in` 在扁平里本就是顶层 input ⇒ ⇒ **仍 MUST 产出**。

无驱动源的子模块输入端口 MUST 逐级向上冒泡，最终成为**顶层 input**
（与扁平产物行为一致）⇒ ⇒ 默认（`UndrivenPolicy::Bubble`）MUST 有 `CHWARN` 点名路径
与端口旁注释；**`UndrivenPolicy::Reject`** MUST **不产出**（`ok = false` + `files` 为空）
且 `error` MUST 列出**全部**悬空输入（端口名 + module + 层级）。

#### Scenario: 悬空输入在两份产物中都可驱动

- **WHEN** 一个子模块的输入端口未被驱动
- **THEN** 扁平与层次化两份产物 MUST 都把它暴露为**顶层可驱动输入**
- **AND** 层次化侧该端口名 MUST **与扁平产物逐字相同**（= 该输入节点的 ctx 级全局名）
      ⇒ ⇒ MUST NOT 加任何框架前缀
- **AND** 该端口旁 MUST 有注释标明它来自哪个组件的未驱动输入

#### Scenario: `Reject` MUST 列出全部悬空输入且不误拒根自己的端口

- **WHEN** `undriven_policy = UndrivenPolicy::Reject`，设计里有**两个**实例含悬空 `ch_in`
- **THEN** `error` MUST 同时点名**两个**端口（只报第一条 ⇒ 判据转红）
- **AND** 根 module 自己的 `ch_in` 与 inline 折叠进根的悬空 `ch_in` MUST NOT 触发拒绝
      ⇒ ⇒ 产物 MUST 仍被产出

### Requirement: 时序模块 MUST 有 clock/reset 端口

- **MODIFIED**：lint 门 = **ADR-060 §2.5**（不启用全量告警开关 · 捕获退出码 · stderr 无
  `%Error` · `lint_off` 白名单 · **禁 `-Wno-fatal`** ⇒ 它让 warning 不影响退出码 ⇒ 门失效）。

**根 module** MUST **无条件**发射 `default_clock` / `default_reset`
（⇒ 与扁平产物一致：`context::init()` 恒定创建这对节点，扁平 `print_header` 无条件发射）。
**非根** module MUST **把 clock 与 reset 分开判**（⇒ 一元 `has_seq` 不够）。
`default_clock` ⇒ 含 `ch_reg` ∨ mem 写端口 ∨ `sread`；
`default_reset` ⇒ 含 `ch_reg` ∨ `sread`（⇒ mem 写端口的 `always_ff` **没有 reset 分支**）。

#### Scenario: 纯组合设计的扁平与全 inline 产物端口数一致

- **WHEN** 一个设计**不含**任何 `ch_reg` / `ch_mem`
- **THEN** 扁平与全 inline 层次化产物 MUST **都**含 `default_clock` / `default_reset` 两个端口
- **AND** 两份产物 MUST 逐字节相同

#### Scenario: 只有 RAM 写端口的模块 MUST 有时钟

- **WHEN** 一个成层 module 的体内只含 `ch_mem` 的**写端口**（无 `ch_reg`、无 `sread`）
- **THEN** 该 module MUST 发射 clock 端口
- **AND** 它 MUST **NOT** 发射 reset 端口（⇒ mem 写端口的 `always_ff` 没有 reset 分支）
- **AND** 产物 MUST 通过 **ADR-060 §2.5 的 lint 门**（MUST NOT 引用未声明信号）

### Requirement: 成层且非根的 module 端口 MUST 覆盖自身 `ch_in` / `ch_out`，且每个端口可追溯

- **MODIFIED**：拒绝的可观测形态按 ADR-058 §2.3 ⑤ = **错误结果**（`ok = false` +
  `error` 点名 + `files` 为空），与「发射器内 MUST NOT 依赖异常」口径一致。
- **MODIFIED（ADR-066 决策 ③ · 2026-11-08）**：「造网」义务**收窄**为
  「父侧消费点**不是**父 `output` 端口」时 ⇒ ⇒ 消费点**就是**父 `output` 端口时
  MUST NOT 强制造网 ⇒ ⇒ 唯一权威 = `docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md`
  决策 ③。

**成层且非根**的 module，其端口集合 MUST **⊇** 它自己的 `ch_in` ∪ `ch_out`
⇒ ⇒ **是否已被父层驱动不影响它是否成为端口**（emitter MUST 忽略 `inputimpl::driver()`）。
且每个端口 MUST 可追溯到五类来源之一：自身端口 · inline 后代的 `ch_out`（**仅根允许**）·
隐式端口 · 冒泡端口 · **时序基础设施**（clock/reset）。

⇒ ⇒ **RTL 顶层 module MUST 走扁平规则**，MUST NOT 套用本条。
⇒ ⇒ **自驱动**：成层组件自己驱动自己的 `ch_in` ⇒ MUST **返回 `ok = false`** 并点名该端口。

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

- **WHEN** 一个设计里 RTL 顶层 module 的下方有被 inline 的子组件
- **THEN** 顶层端口表 MUST 是 clock → reset → 无 driver 的 `input` → **全部** `ch_out`
      （⇒ 含 inline 后代的 `ch_out`）
- **AND** 该端口表 MUST 与扁平产物**逐字逐序相同**

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
- **AND** ⚠️ **子 input 端口 MUST NOT** 用空连接糊过去 ⇒ ⇒ 未驱动的子 input MUST
  **冒泡**成顶层 input 并由父层接上（⇒ ⇒ 否则其值是未定义值 X）

### Requirement: 单元体内 MUST NOT 引用「非自己、非祖先」的端口节点

一个组件的体内节点 MUST NOT 以某个端口节点为源，而该端口节点的 `owner`
**既不是它自己、也不是它的祖先** ⇒ ⇒ 这是「**值经 host 侧通道（键值总线）进来**」的
可机械判据（⇒ 那种通道**不产生端口、也不构成接口声明**）。

#### Scenario: 只从 host 侧键值通道读的单元 MUST 被判定为接口不完整

- **WHEN** 一个单元的输入**只**从 host 侧键值通道读取，**没有**对应的 `ch_in` 端口
- **THEN** 上述判据 MUST **转红**
- **AND** MUST NOT 改用「module 的端口数 > 0」当判据
      （⇒ 输出方向必须先有 `ch_out` 作源 ⇒ 端口数可能 > 0 ⇒ 该判据**没有判别力**）

### Requirement: 层次化单元的接口类型 MUST 限于白名单

一个**成层**单元的接口端口类型 MUST 限于 `ch_uint<N>` / `ch_bool`
⇒ ⇒ MUST NOT 出现 `ch_mem` · `ch_reg` · bundle / `ch_stream` · clock / reset · `inout`
（⇒ ⇒ 理由分别是：数组无端口形态 · 状态不是接口 · 成员不是端口 · 由框架供给以免重复端口 · 单方向模型）。

⇒ ⇒ **数组字段**（`ch_out<T> x[N]`）MUST 允许，但端口名 MUST 是**确定性派生**（`x_<i>`）
⇒ ⇒ 否则「同一设计两次生成字节相同」不成立。

⇒ ⇒ ⚠️ **义务方**：本条约束**上游**（emitter / 接口声明方）；
「接口字段名与顺序 MUST 稳定」（ADR-062 **N-3**）**不在**本条范围 ⇒ ⇒ 但 N-3 MUST 另补载体。

#### Scenario: 白名单外的接口类型 MUST 被拒绝

- **WHEN** 一个成层单元的接口端口类型是 `ch_mem` / `ch_reg` / bundle / `ch_stream` / `inout`
- **THEN** emitter MUST **响亮拒绝**并点名该端口
- **AND** MUST NOT 静默产出（⇒ ⇒ 「数组无端口形态」⇒ ⇒ 发射器**无法**表达它）

### Requirement: 层次化产物 MUST 是单文件多 module，且顶层 module 名可配置

层次化产物 MUST 是**一个文件**、内含**多个 module**（子在前、根在后）
⇒ ⇒ `verilator --top-module <root_module>` MUST 能直接吃
⇒ ⇒ 顶层 module 名 MUST 由选项 `top_module_name` 决定（默认 `top`）
⇒ ⇒ 层次化产物的**路径** MUST NOT 与扁平产物同名（同名不同物 ⇒ ⇒ 消解手段 = 路径由调用方显式指定）。

#### Scenario: 单文件承载全部 module

- **WHEN** 一个三层设计被发射
- **THEN** 产物 MUST 是**一个**文件
- **AND** 该文件 MUST 含 **3 个 module** 定义
- **AND** MUST NOT 产生按 module 拆分的多个文件

#### Scenario: 顶层 module 名可配置

- **WHEN** 调用方把 `top_module_name` 设为 `my_top`
- **THEN** 顶层 module MUST 名为 `my_top`
- **AND** 其余 module 名 MUST NOT 受影响

#### Scenario: 层次化产物路径 MUST NOT 与扁平产物同名

- **WHEN** 层次化产物的路径与某个既有扁平产物的路径相同
- **THEN** 该情形 MUST 被**护栏**判为违规（护栏承担此约束 ⇒ ⇒ API 侧无自卫）

### Requirement: 跨层端口策略 MUST 是单一权威的线性枚举，且各取值的失败语义 MUST 明确

跨层非端口边的处理策略 MUST 由**唯一**的线性枚举 `PortPolicy` 承载
（`SynthesizeWarn` / `SynthesizeFatal` / `Reject`）
⇒ ⇒ MUST NOT 存在第二份同义枚举副本
⇒ ⇒ 「合成」MUST 是**纯内存 phase**、**先于任何文件 I/O**
⇒ ⇒ 悬空输入的策略 MUST 由 `UndrivenPolicy`（`Bubble` / `Reject`）承载。

#### Scenario: SynthesizeWarn 是默认策略

- **WHEN** 未显式指定策略
- **THEN** 隐式端口 MUST 被**合成**并产出产物
- **AND** MUST 对**每一条**被合成的边打 `CHWARN`（点名该边与合成出的端口名）
- **AND** MUST 在产物内为**发生合成**的端口写注释

#### Scenario: 未发生合成时产物 MUST 不含合成注释

- **WHEN** 设计内**没有**任何跨 module 边被合成
- **THEN** 产物 MUST NOT 含任何合成端口注释
- **AND** 「默认（全 inline）产物与扁平产物逐字节一致」这条判据 MUST 仍然成立

#### Scenario: SynthesizeFatal 先合成完再失败

- **WHEN** 策略为 `SynthesizeFatal` 且设计含**至少两条**越界边
- **THEN** 发射 MUST **失败**且 MUST NOT 产出产物文件
- **AND** 失败诊断 MUST 列出**全部**被合成的边与合成端口名（不止第一条）
- **AND** MUST NOT 留下临时文件
- **AND** MUST NOT 触碰既有同名文件

#### Scenario: Reject 只报第一条

- **WHEN** 策略为 `Reject` 且设计含越界边
- **THEN** 发射 MUST 失败且 MUST NOT 产出产物文件
- **AND** 诊断 MUST 报出**第一条**越界边

### Requirement: 层级粒度 MUST 由全局唯一权威决定，且作用域 MUST 覆盖构造与发射

层级粒度（`RespectDeclarations` / `ForceInline` / `ForceLayer`）的**唯一权威** MUST 是**全局**开关
⇒ ⇒ 选项结构 MUST NOT 携带 `granularity` 字段
⇒ ⇒ 粒度的**唯一入口** MUST 是 RAII 作用域（`Component::GranularityScope`）
⇒ ⇒ 非作用域的裸设置入口 MUST NOT 存在
⇒ ⇒ 粒度的判定优先级 MUST 是「全局 > 实例级 > 类级声明」
⇒ ⇒ 作用域 MUST **同时覆盖**组件**构造**与**发射**两段。

#### Scenario: 粒度只包构造时 MUST 回退到 RespectDeclarations

- **WHEN** 作用域只覆盖组件构造、而发射发生在作用域之外
- **THEN** 产物 MUST 等于 `RespectDeclarations` 形态
- **AND** MUST NOT 等于 `ForceInline` 形态

#### Scenario: 作用域覆盖两段时 ForceLayer MUST 成层

- **WHEN** 作用域同时覆盖构造与发射、且粒度为 `ForceLayer`
- **THEN** 产物 MUST 把每个组件都发射成独立 module

#### Scenario: 选项结构 MUST NOT 携带粒度字段

- **WHEN** 检查层次化发射的选项结构
- **THEN** 它 MUST NOT 含 `granularity` 字段
- **AND** 粒度 MUST 只能经 RAII 作用域设置

### Requirement: 层次化 MUST 有独立的公开入口，既有路径 MUST 逐字节不变

层次化输出 MUST 走**独立的新入口**（新函数 + 新选项结构 + 新错误类型）⇒ ⇒
既有 `toVerilog()` 的**签名**与**行为** MUST **逐字节不变**（⇒ ⇒ 既有调用点与 Verilator backend
**零风险**）⇒ ⇒ Verilator backend / Simulator / JIT 的**既有入口**一律不动。

⇒ ⇒ **错误模型**（MUST）：
① 所有校验 MUST 在**第一次 I/O 之前**完成（⇒ ⇒ 否则留下**半套产物**）；
② 写出 MUST 走「**临时目录 → 成功后 rename**」（⇒ ⇒ 失败**不留下**任何新文件）；
③ 校验 MUST NOT 落在任何 `catch(...)` 作用域内（⇒ ⇒ 否则响亮失败会被**静默吞掉**）；
④ 静态析构期 MUST 另有**返回码**兜底。

⇒ ⇒ 选项 MUST 用**线性枚举**表达（MUST NOT 用布尔开关组合 ⇒ ⇒ 组合语义重叠 ⇒ ⇒ 未定义态留给实现者）。

#### Scenario: 既有扁平入口 MUST 不受影响

- **WHEN** 同一个设计用既有 `toVerilog()` 生成
- **THEN** 产物 MUST 与引入层次化能力**之前逐字节相同**
- **AND** 该入口的签名 MUST NOT 变化（⇒ ⇒ 既有调用点零改动）

#### Scenario: 校验失败 MUST 不留下半套产物

- **WHEN** 层次化生成在**写出之前**就因校验失败
- **THEN** MUST **不产生任何**新文件
- **AND** 错误 MUST **可被调用方观测**（⇒ ⇒ 返回错误类型，MUST NOT 只打日志）

#### Scenario: 写出过程中失败 MUST 不留下半套产物

- **WHEN** 层次化生成已开始写出，但在**完成之前**失败
- **THEN** 输出路径 MUST **不留下**任何新文件（⇒ ⇒ 半套产物会让 `cmp` / lint 读到它）
- **AND** 临时目录 MUST 被清理
- **AND** 既有的同名文件 MUST NOT 被破坏（⇒ ⇒ 写出 MUST 走「**临时目录 → 成功后 rename**」）

#### Scenario: 静态析构期的失败 MUST 以返回码呈现

- **WHEN** 层次化入口在**静态析构期**被调用且校验失败
- **THEN** 失败 MUST 以**返回码 / 错误类型**呈现
- **AND** MUST NOT 依赖抛异常（⇒ ⇒ 该期抛异常会 `std::terminate`；既有宏在该期**不抛**）
- **AND** 退出路径 MUST **不泄漏**资源

### Requirement: 默认（全 inline）产物 MUST 与扁平产物逐字节一致

当**没有任何**组件被声明为成层时，层次化产物 MUST 与扁平产物**逐字节一致**
（同名、同序、同端口表）⇒ ⇒ **允许差异 = 空集**（可执行判据：`cmp` 退出码 0）。

#### Scenario: 全 inline 时零差异

- **WHEN** 一个设计未声明任何成层组件
- **THEN** 层次化产物与扁平产物 MUST **逐字节相同**（`cmp` 退出码 0 ⇒ 无任何允许差异）
- **AND** 该产物 MUST NOT 包含任何实例化语句

### Requirement: 层次化产物 MUST 与扁平产物等价（可机械判定）

等价性 MUST 由以下五条同时判定，且每条 MUST **可机械执行**：

① **映射表**被产出，且是**四元组**（`node_id` → 设计级信号名 → 扁平位置 → 层次化位置集合）；
② **输入端口集合相等**（名字 + 宽度逐项配对）；
③ **输出端口集合等于精确期望集** = 扁平输出集 − {`ch_out` 的最近成层祖先 ≠ 根 的全部节点}；
④ 显式观测集合 `O`（**非空**且**至少一个随激励变化**）上 `flat[O] ≡ hier[O]` **逐拍全等**；
   `O` MUST ⊆ **该次运行粒度下可见**的 `ch_out` 集（可见 ⇔ 映射表 `hier_top_port_name` 非空）；
⑤ **观测盲区 MUST 显式声明**：等价性只覆盖**顶层可见行为**；盲区 MUST 以**对偶断言**机械
   登记（未观测者 `hier_top_port_name` 为空），「可见但未观测」者 MUST 进**显式豁免清单**。

#### Scenario: 映射表 MUST 由 emitter 同次产出

- **WHEN** 一次 `toVerilogHierarchical()` 调用完成
- **THEN** 映射表 MUST 由该次调用**产出**（载体 = `HierarchyEmitResult` 的公开字段 ·
      **value-only** ⇒ MUST NOT 带 `lnodeimpl*`）⇒ MUST NOT 由测试自建
- **AND** `hier_top_port_name` **允许为空**（= 该信号不在层次化顶层），
      「层次化位置集合」覆盖子 module 端口与体内线网
- **AND** `node_id` 的有效性域 = **同一 ctx** ⇒ 扁平与层次化产物 MUST 由同一个 `ch_device`
      产生 ⇒ harness MUST 断言「两次 emit 共享同一 ctx」
- **AND** ⚠️ 该断言 MUST **可失败**（MUST NOT 是恒真表达式；MUST NOT 用「同一对象与自身比较」
      的形式 ⇒ ⇒ 恒真 ⇒ 零判别力）⇒ ⇒ 断言 MUST 比较**两次发射各自记录的来源 ctx**

#### Scenario: 漏发端口 MUST 被抓到

- **WHEN** 层次化产物少发了某个输入端口
- **THEN** 判据 ② MUST **转红**
- **AND** MUST NOT 用「`hier.top_ports ⊆ flat.top_ports`」代替 ②（⇒ 丢端口正是子集方向）

#### Scenario: 判据 MUST 有判别力

- **WHEN** 故意破坏层次化产物（删一条连线 / 改一个隐式端口方向）
- **THEN** 等价性测试 MUST **转红** ⇒ ⇒ 还原后 MUST 复绿

#### Scenario: 观测盲区 MUST 不被当成「已全覆盖」

- **WHEN** 一个 `ch_out` 的最近成层祖先**不是**根（⇒ 它不进层次化顶层端口表）
- **THEN** 该信号 MUST 出现在映射表的「层次化位置集合」里（`hier_top_port_name` 为空）
- **AND** 判据 MUST **显式声明**它不在观测面内（MUST NOT 声称「全部输出已比对」）
- **AND** 输入方向**无盲区**（顶层 input 集合两侧 MUST 相等 ⇒ 激励可完整施加）

#### Scenario: 单粒度比较时 `O` MUST 覆盖全部可见输出

- **WHEN** 一次等价性比较只涉及**一种**粒度
- **THEN** `O` MUST 覆盖该粒度下**全部可见**的 `ch_out`
      （可见 = 映射表里 `hier_top_port_name` 非空者）⇒ ⇒ 未被观测的 `ch_out` MUST 全部
      满足「`hier_top_port_name` 为空」⇒ ⇒ 盲区即「该粒度下不可见者」
- **AND** 该覆盖 MUST 有**独立于 `O` 手写清单**的锚点交叉校验
      （映射表的可见集计数 vs DUT 端口清单 + 该产物实际存在的 clock/reset 条目）
      ⇒ ⇒ 防止「`O` 与 DUT 清单同时漏掉同一个 `ch_out`」而恒绿

#### Scenario: 跨粒度比较时 `O` MUST 取公共子集并显式登记豁免

- **WHEN** 一次等价性比较涉及**两种粒度**（同一设计分别以不同粒度发射后比较）
- **THEN** `O` MUST ⊆ **两种粒度下都可见**的 `ch_out` 集（公共子集）
      ⇒ ⇒ 否则某粒度下读不到的一侧会被误判成分歧
- **AND** 「在某粒度下可见、但本次未观测」的信号 MUST 出现在**显式豁免清单**里
- **AND** 该次比较 MUST NOT 声称「全部可见输出已比对」

### Requirement: mem 端口实参跨层 MUST 支持且走父侧落点网

`ch_mem` 读 / 写端口的 `addr` / `en` / `wdata` **实参槽**取自成层子组件的 `ch_out` 时
（= 上条的 **(ii)** 桶）⇒ ⇒ 层次化 MUST 支持该形态 ⇒ ⇒

- **落点网**：MUST **复用**「父侧消费子 `ch_out`」的既有落点网机制（父 module 声明一根
  `logic` 网 + 子 output 以 `.port(net)` 连接 + 父侧读该网）⇒ ⇒ MUST NOT 为 mem 端口合成
  **专用端口**（mem 端口**不是**一根信号）⇒ ⇒ MUST NOT 引入 mem 专用的第二套命名；
- **命名**：落点网的网名 MUST = **驱动端点的 ctx 全局名** ⇒ ⇒ mem 端口读到的名字与网名
  **恒等** ⇒ ⇒ MUST NOT 为 mem 实参新增「重定向表查表」（恒等映射 ⇒ 死代码）；
- **宽度**：MUST NOT 让父 module 出现 **implicit wire**（1-bit ⇒ 宽度截断 ⇒ 值错）⇒ ⇒
  产物内 mem 实参引用的名字 MUST 在**父 module 作用域**有**显式 `logic` 声明**；
- **`en` 槽**：`enable` 缺省（无实参）⇒ MUST 取 `1'b1`（恒使能）；`enable` **有实参**但
  **取不到名字**⇒ ⇒ MUST **fail-closed**（诊断点名）⇒ ⇒ MUST NOT 回退 `1'b1`
  （⇒ ⇒ 读侧恒使能 / 写侧恒写 ⇒ ⇒ **值错且 lint 绿**）。

#### Scenario: (ii) 形态 MUST 发射成功且父侧有显式声明

- **WHEN** mem 本体与使用者同 module，且 `addr` 取自成层子组件的 `ch_out`
- **THEN** 层次化 MUST 返回 `ok = true`
- **AND** 父 module 作用域 MUST 有该实参名的**显式 `logic` 声明**（⇒ ⇒ 非 implicit wire）
- **AND** mem 端口 MUST **读**该网

#### Scenario: 非相邻的 mem 实参 MUST 响亮拒绝（**(iii) 桶**）

- **WHEN** mem 本体与使用者同 module，但 `addr` / `en` / `wdata` 取自**孙组件**或**兄弟**的 `ch_out`
- **THEN** 层次化 MUST **返回 `ok = false`**
- **AND** `error` MUST 点名该跨层实参与两端 module
- **AND** `error` MUST NOT 含「存储器与使用者同层」这类**错误**归因（mem 本体**已在**同层）

#### Scenario: (ii) 形态的读值 MUST 是确定值

- **WHEN** mem 的初值表已知，且实参地址逐拍变化
- **THEN** 层次化产物在**逐拍**仿真下 MUST 读出与**手推**一致的值
- **AND** MUST NOT 以「非 0」作为判据（⇒ ⇒ 错值只要非 0 即过）

#### Scenario: 写侧的 `wdata` / `en` 同样 MUST 跨层生效

- **WHEN** mem 写端口的 `addr` / `wdata` / `en` **三个**实参槽全部取自成层子组件的 `ch_out`
- **THEN** 层次化 MUST 发射出**读**这三根父侧网的写端口
- **AND** 逐拍仿真 MUST 观察到写入的值（⇒ ⇒ `en` 未接上 ⇒ 写恒不发生 ⇒ ⇒ 必被抓住）

#### Scenario: `en` 实参有值但取不到名字 MUST fail-closed

- **WHEN** mem 端口的 `enable` 槽有实参，但该实参在当前 module 作用域取不到名字
- **THEN** 发射 MUST 诊断点名该 mem 端口与该事实
- **AND** MUST NOT 回退 `1'b1`（⇒ ⇒ 否则恒使能 ⇒ ⇒ 值错且 lint 绿）
