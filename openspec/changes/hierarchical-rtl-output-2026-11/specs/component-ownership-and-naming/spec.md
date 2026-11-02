# Capability: component-ownership-and-naming

## Purpose

定义 CppHDL **组件实例的节点归属**与**实例路径命名**契约：节点 MUST 归属到「真正拥有它的
那个组件实例」，组件实例路径 MUST 无重复前缀 ⇒ ⇒ 任何按归属切分的工具（层次化 RTL 输出、
层次化 trace、可读性报告）MUST NOT 把子组件的状态划进父组件。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

### Requirement: 节点归属 MUST 由构造作用域唯一决定

节点的归属组件 MUST 由**建节点时所处的构造作用域**唯一决定（最内层作用域的组件）
⇒ ⇒ MUST NOT 依赖事后推断（前缀匹配 / 时间戳 / 遍历修正）⇒ ⇒ 作用域 MUST **配对**
（进入必退出）⇒ ⇒ 栈失衡 MUST 抛异常（MUST NOT 只打日志）。

#### Scenario: 成员节点的归属 MUST 是所属组件

- **WHEN** 一个组件以**成员变量**形式持有 `ch_reg` / `ch_mem` / bundle 字段
- **THEN** 这些节点的归属组件 MUST 是**该组件本身**，而不是它的父组件
- **AND** 其 Verilog 名 MUST 含该组件的路径段

#### Scenario: 构造函数抛出时 MUST 由 RAII 弹栈

- **WHEN** 一个组件的构造函数在**压栈之后**抛出异常
- **THEN** 作用域栈 MUST 由 RAII 护栏**自动回退**到进入前的深度
      （⇒ MUST NOT 残留半帧）
- **AND** 此后新建节点的归属 MUST 仍然正确（⇒ MUST NOT 被那个半构造的组件吸收）

> ⚠️ **为什么不写「栈非空 ⇒ 抛异常」**（v1.1 订正）：配对作用域成立时栈**永远**为空
> ⇒ ⇒ 该断言**不可触发**（⇒ 写进契约就是一条**永远绿**的判据）。可触发的等价形态是
> 「异常路径的栈回退」，故改成上面这条。

### Requirement: 组件实例路径 MUST 无重复前缀

组件实例的路径 MUST 由「父路径 + 局部名」拼成，且 MUST NOT 重复父路径本身
⇒ ⇒ 深度 ≥ 3 的实例路径 MUST NOT 出现重复段。

#### Scenario: 深度三层 MUST 无重复前缀

- **WHEN** 一个三层组件树（Top → Mid → Leaf）
- **THEN** Leaf 的实例路径 MUST 形如 `top.m.lf`
- **AND** MUST NOT 出现 `top.top.m.lf` 这类重复前缀
