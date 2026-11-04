# Capability: rtl-hierarchical-output

## Purpose

定义 CppHDL **层次化 RTL 输出**的契约：哪些组件成为独立 Verilog module（**层级粒度由用户
显式声明**，默认 inline）、模块边界如何划分（= 所有权边界）、命名权威在哪（= ctx 级全局表）、
跨层连线如何处理（隐式端口 / 悬空输入），以及与**扁平输出**的**逐拍等价性**如何判定。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`（先例：`axi-interconnect-routing`）。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

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

