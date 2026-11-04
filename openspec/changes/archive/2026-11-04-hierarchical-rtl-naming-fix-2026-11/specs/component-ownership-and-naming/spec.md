# Capability: component-ownership-and-naming

## Purpose

定义 CppHDL **组件实例的节点归属**与**实例路径命名**契约：节点 MUST 归属到「真正拥有它的
那个组件实例」，组件实例路径 MUST 无重复前缀 ⇒ ⇒ 任何按归属切分的工具（层次化 RTL 输出、
层次化 trace、可读性报告）MUST NOT 把子组件的状态划进父组件。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

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

