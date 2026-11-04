# Capability: component-ownership-and-naming

## Purpose

定义 CppHDL **组件实例的节点归属**与**实例路径命名**契约：节点 MUST 归属到「真正拥有它的
那个组件实例」，组件实例路径 MUST 无重复前缀 ⇒ ⇒ 任何按归属切分的工具（层次化 RTL 输出、
层次化 trace、可读性报告）MUST NOT 把子组件的状态划进父组件。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

### Requirement: 成层组件的模块类型名 MUST 按来源优先级确定

**成层**组件的模块类型名 MUST 按下列**优先级**确定 ⇒ ⇒ **MUST NOT** 有其它来源：

1. **类级** `CH_HIERARCHY_LAYER("name")` ⇒ ⇒ MUST 用该名字（省略**编译期不可表达**）；
2. **实例级** `ch_layer<T>` · `T` **已**声明成层 ⇒ ⇒ MUST **继承** `T` 的类级名字；
3. **实例级** `ch_layer<T>` · `T` **未**声明成层 ⇒ ⇒ MUST 在**实例处**给名（省略**编译期不可表达**）；
4. **`Granularity::ForceLayer`** 强制成层（用户**未**声明）⇒ ⇒ MAY 回退
   `sanitize(typeid(*this).name())` ⇒ ⇒ **MUST 有 `CHWARN`**。

#### Scenario: 类级声明 MUST 给出模块类型名

- **WHEN** 一个组件声明为「成层」
- **THEN** 该声明 MUST **同时**给出模块类型名（⇒ ⇒ 缺名**编译期即不可表达**）
- **AND** 该名字 MUST 在**同一编译器 / ABI 内**可复算

#### Scenario: 实例级提升 helper 时 MUST 在实例处给出模块类型名

- **WHEN** 一个**未**声明成层的组件被**实例级**提升为成层
- **THEN** 该实例 MUST 携带模块类型名（⇒ ⇒ 缺名**编译期即不可表达**）
- **AND** 若该组件类型**已**声明成层 ⇒ ⇒ 该实例 MUST **继承**类型的类级名字

#### Scenario: emitter 强制成层时 MUST 告警

- **WHEN** emitter 以 `Granularity::ForceLayer` 把一个**用户未声明成层**的组件强制成层
- **THEN** 其模块类型名 MAY 回退到 `sanitize(typeid(*this).name())`
- **AND** MUST 有 `CHWARN` 点名该组件（⇒ ⇒ 平台替用户决定粒度 ⇒ ⇒ MUST 可观测）

