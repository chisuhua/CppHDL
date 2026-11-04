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

「是否成层」MUST 由**声明**决定，MUST NOT 依赖装配点的位置（⇒ 允许在**无 active parent**
的位置装配）；但被装配的组件 MUST **登记进组件树**（`ch_device` / `ch_module` / `add_child`）
⇒ ⇒ 否则 emitter MUST **响亮拒绝**（MUST NOT 静默并入顶层 module）。

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
- **THEN** 层次化 emitter MUST **抛异常**并给出 `add_child` 修法
- **AND** MUST NOT 静默把它并入顶层 module（⇒ 否则整块逻辑消失）

#### Scenario: 已登记但从未 build 的组件 MUST 被拒绝

> ⚠️ **权威载体**：本条与 capability `component-ownership-and-naming` 的「未 build 的子组件 MUST 被识别」是同一条义务的**两侧**（那边判「框架能识别」，这边判「emitter 必须拒绝」）⇒ ⇒ 改一处 MUST 同步另一处。

- **WHEN** 一个组件被登记进组件树，但**从未** `build()`
      （⇒ ⇒ 它**有节点、无端口** ⇒ ⇒ 端口只在 build 期创建）
- **THEN** 层次化 emitter MUST **抛异常**并点名该组件
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
