# Capability: rtl-hierarchical-output

## Purpose

定义 CppHDL **层次化 RTL 输出**的契约：哪些组件成为独立 Verilog module（**层级粒度由用户
显式声明**，默认 inline）、模块边界如何划分（= 所有权边界）、命名权威在哪（= ctx 级全局表）、
跨层连线如何处理（隐式端口 / 悬空输入），以及与**扁平输出**的**逐拍等价性**如何判定。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`（先例：`axi-interconnect-routing`）。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

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

