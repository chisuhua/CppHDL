# Capability: rtl-hierarchical-output

## Purpose

定义 CppHDL **层次化 RTL 输出**的契约：哪些组件成为独立 Verilog module（**层级粒度由用户
显式声明**，默认 inline）、模块边界如何划分（= 所有权边界）、命名权威在哪（= ctx 级全局表）、
跨层连线如何处理（隐式端口 / 悬空输入），以及与**扁平输出**的**逐拍等价性**如何判定。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`（先例：`axi-interconnect-routing`）。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

### Requirement: 命名权威 MUST 是 ctx 级全局表

层次化 emitter MUST 复用既有的 **ctx 级全局命名表**（同名节点全局加 `_N` 后缀）
⇒ ⇒ MUST NOT 发明 per-module 局部命名 ⇒ ⇒ 顶层端口名 MUST 与扁平产物**逐字相同**。

#### Scenario: 顶层端口名 MUST 与扁平产物逐字相同

- **WHEN** 同一设计分别用扁平与层次化两种方式生成
- **THEN** 两者顶层端口的名字 MUST **逐字相同**
- **AND** 层次化 MUST NOT 引入任何新的改名
- **AND** 顶层端口名 MUST NOT 带任何框架前缀（`__if_` / `__x_`）
      （⇒ 合成端口名只允许出现在**非顶层** module 的端口上；`__undriven_` 前缀**已作废**，
      冒泡端口 MUST 用 ctx 级全局名）

