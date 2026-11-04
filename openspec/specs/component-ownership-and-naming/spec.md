# component-ownership-and-naming Specification

## Purpose
定义 CppHDL **组件实例的命名**与**节点归属**契约：

- **命名**：组件实例 MUST 提供**两个互不替代**的命名维度 —— **局部名**
  （`local_name()` = 实例在父下的名字）与**模块类型名**（`module_type_name()` =
  module 定义的类型名）⇒ ⇒ 组件的**全路径名**（`name()`）MUST 由「父路径 + 局部名」
  拼成 ⇒ ⇒ **MUST NOT** 重复父路径本身（深度 ≥3 MUST 形如 `top.m.lf`，
  **MUST NOT** `top.top.m.lf`）。
- **归属**：节点 MUST 归属到「**真正拥有它的那个组件实例**」⇒ ⇒ 任何按归属切分的工具
  （层次化 RTL 输出、层次化 trace、可读性报告）MUST NOT 把子组件的状态划进父组件。

**为什么需要**（实测 · 2026-11-04）：① `build_hierarchical_name()` 用
`parent->hierarchical_name()` 拼子路径，而后者**又**把父路径拼到「**已经是全路径**的
`name_`」前面 ⇒ ⇒ **父路径被拼两次** ⇒ ⇒ 深度 3 实测 `top.top.m.lf`；且两个函数
**互相喂**全路径 ⇒ ⇒ 谁也拿不到正确的局部名。② 成员形态的 `ch_reg` / `ch_mem` / bundle
字段归属**父**组件、成员 `ch_module` 挂**祖父** ⇒ ⇒ 按归属切模块会把**子模块的状态**
划进父模块 ⇒ ⇒ 产出的 RTL **看起来对、实际错**，且**无任何告警**。

⚠️ **边界**：本 capability 只管**名字与归属**⇒ ⇒ **不**管粒度（成层 / inline 的判定）、
端口策略、产物结构 ⇒ ⇒ 那些各有其 capability。

## Requirements

### Requirement: 组件实例路径 MUST 无重复前缀

组件实例的路径 MUST 由「父路径 + 局部名」拼成，且 MUST NOT 重复父路径本身
⇒ ⇒ 深度 ≥ 3 的实例路径 MUST NOT 出现重复段。

#### Scenario: 深度三层 MUST 无重复前缀

- **WHEN** 一个三层组件树（Top → Mid → Leaf）
- **THEN** Leaf 的实例路径 MUST 形如 `top.m.lf`
- **AND** MUST NOT 出现 `top.top.m.lf` 这类重复前缀

### Requirement: 组件实例 MUST 提供局部名与模块类型名两个命名维度

组件实例 MUST 提供**两个互不替代**的命名维度：
**局部名**（`local_name()` = 实例在父下的局部名）与**模块类型名**（`module_type_name()` = module 定义的类型名）
⇒ ⇒ 组件的**全路径名**（`name()`）MUST 由「父路径 + 局部名」拼成 ⇒ ⇒ **MUST NOT** 重复父路径本身。

#### Scenario: 局部名与模块类型名 MUST 可分别读取

- **WHEN** 一个组件实例被读取其命名维度
- **THEN** `local_name()` MUST 返回**局部**名（不含父路径）
- **AND** `module_type_name()` MUST 返回**类型**名（同一类型的多实例 MUST 返回**相同**值
      ⇒ ⇒ 因为它们**各发一份** module ⇒ ⇒ module 名由 `<module_type_name>_<序号>` 派生
      ⇒ ⇒ **序号 = 该组内实例按「实例路径」排序**的次序（⇒ ⇒ 确定性 ⇒ ⇒ 否则
      「同一设计两次生成字节相同」不成立））
