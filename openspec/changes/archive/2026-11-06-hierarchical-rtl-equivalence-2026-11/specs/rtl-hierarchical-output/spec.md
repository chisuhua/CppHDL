# Capability: rtl-hierarchical-output

## Purpose

定义 CppHDL **层次化 RTL 输出**的契约：哪些组件成为独立 Verilog module（**层级粒度由用户
显式声明**，默认 inline）、模块边界如何划分（= 所有权边界）、命名权威在哪（= ctx 级全局表）、
跨层连线如何处理（隐式端口 / 悬空输入），以及与**扁平输出**的**逐拍等价性**如何判定。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`（先例：`axi-interconnect-routing`）。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

> ⚠️ **本 delta 的措辞纪律**：`## MODIFIED` 的正文在归档时会**并入主 spec**
> ⇒ ⇒ 注记里 MUST NOT 出现被本 change 淘汰的字面量（全量告警开关 · 旧布尔策略名 ·
> 「MUST 抛」样式）—— 否则归档门（见 tasks 的 V1 两条 grep）会**在归档后自己转红**。

## ADDED Requirements

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

## MODIFIED Requirements

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

### Requirement: 跨层非端口边 MUST 按方向合成隐式端口（默认告警）

- **MODIFIED**：⚠️ `PortPolicy::Reject` 与 `UndrivenPolicy::Reject` **同名** ⇒ ⇒ 本条
  引用时 MUST 写**全限定名**（`UndrivenPolicy::Reject` 管的是**悬空 `ch_in`**）。

跨模块但**不落在端口上**的边（bundle 成员跨层 / 父层改子层状态 / 子端口由父层表达式驱动）
MUST 按**边方向**合成隐式端口（父→子 = 子侧 input；子→父 = 子侧 output）⇒ ⇒
默认（`PortPolicy::SynthesizeWarn`）MUST 有 `CHWARN` 与生成文件内注释；
**`PortPolicy::SynthesizeFatal`** MUST **先合成完再失败**（诊断含**全部**被合成的边与端口名）；
**`PortPolicy::Reject`** MUST **首错即返**（同样 `ok = false` + `error` 点名）。

#### Scenario: 子→父方向 MUST 合成子侧 output

- **WHEN** 父层的输出由**子组件的成员寄存器**驱动
- **THEN** 层次化 MUST 在**子侧**合成一个 **output** 隐式端口
- **AND** 父层 MUST 通过该实例的输出取数

#### Scenario: 隐式端口的实现 MUST NOT 改写 IR

- **WHEN** 一个跨模块边需要「把驱动搬进子模块」
- **THEN** 实现 MUST 只在**发射期**做虚拟重定向
- **AND** MUST NOT 调用 `set_src` / `set_next` 改写 `lnodeimpl`
      （⇒ 否则 `toVerilog()` 产物、解释器行为、两次生成的确定性**全部受影响**）

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

- **WHEN** 父 module 消费一个成层子组件的 `ch_out`
- **THEN** 父 module MUST 为该端口声明一根 `logic` 网
- **AND** 由实例化语句的 `.port(net)` 驱动（MUST NOT 用 `assign` 接表达式）
- **AND** ⚠️ 该「必须造网」的要求 MUST NOT 施加到**接 input 的** `.port(...)`（可为表达式）

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
