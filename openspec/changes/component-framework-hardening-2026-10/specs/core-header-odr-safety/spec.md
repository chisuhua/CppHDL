# Capability: core-header-odr-safety

## Purpose

定义 CppHDL **公共头**的 ODR（one-definition rule）安全契约：★ 任何公共头 MUST 可被
**2+ 翻译单元**同时 include 而**不产生链接期重复定义** ⇒ ★ 否则下游一旦出现
multi-source 目标即链接失败（★ 而失败点远离真正的原因）。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`（先例：`axi-interconnect-routing`）。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

### Requirement: 公共头 MUST 可被 2+ 翻译单元同时 include

任何被公共聚合头（★ 例：`ch.hpp`、`module.h`）间接或直接 include 的头文件，MUST NOT
在头内定义**非 inline** 的**外部链接**符号（★ 自由函数 / 非模板的非 inline 定义）
⇒ ★ 否则 `#pragma once` 只保护**同一 TU 内**，★ **跨 TU 仍是两份定义** ⇒ 链接期
`multiple definition`。

⇒ ⇒ 该契约 MUST 由**一个 multi-source 编译 fixture**核验（★ 至少两个 .cpp 各自
include 同一公共头 ⇒ **链接必须成功**）。

#### Scenario: 2+ 翻译单元 include 同一公共头 MUST 链接成功

- **WHEN** 一个构建目标含**至少两个** .cpp，★ 且它们各自 include 同一公共聚合头
- **THEN** **链接 MUST 成功**（⇒ MUST NOT 出现 `multiple definition`）
- **AND** 该目标 MUST 产出一个可执行文件

#### Scenario: 头内自由函数 MUST NOT 是非 inline 的外部链接符号

- **WHEN** 一个公共头内定义了一个命名空间作用域的自由函数
- **THEN** 该定义 MUST 是 `inline`（★ 或模板 / 类内隐式 inline）
- **AND** 若该头被移除 `inline` ⇒ 上面的链接 fixture MUST **失败**
      （⇒ 构成**判别性对照**，★ 而非「恒绿」的护栏）
