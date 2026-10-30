# 提案：把「权威表面」的接受面限制 NOTE 从 Requirement **正文**移进 Scenario

- **来源**：change `chlib-guard-contribution-priority-2026-10` 归档时，
  capability `chlib-conditional-semantics` 的「条件构造的**权威表面** MUST 唯一」
  Requirement 的正文被追加了一段**接受面限制 NOTE**（E1/E2/E3）⇒ ⇒ 该
  Requirement 的正文长度由 **241** 字符涨到 **891** 字符 ⇒ ⇒ 触发了 openspec
  CLI 的「Requirement 正文 > 500 字符」规则。
- **规模 / 风险**：**极小**（**零**行为变更 · **零**代码改动 · 纯规格整理）
- **规格影响**：**MODIFIED ×1** ⇒ 归档**禁** `--skip-specs`

## Why

Requirement 的**正文**应当陈述**行为**；**逐条的形态描述**（错误消息、宽度
对照、签名）属于 **Scenario** ⇒ ⇒ 二者混在一起会让正文变成长而难读的一段
⇒ ⇒ 且 Scenario 本就是「可勾选的验收形态」的载体 ⇒ ⇒ 归属更自然。

⚠️ 本仓 CLI 钉 **1.4.1**（该 >500 字符规则在 1.4.1 下**不存在**）⇒ ⇒ CI
**不**跑 `openspec validate` ⇒ ⇒ 本 change **不**是「CI 必红」的修复 ⇒ ⇒
但把正文写成**可读的**而非**塞满**的，本身就是契约该有的形态 ⇒ ⇒ 值得做。

## What Changes

- **仅** capability `chlib-conditional-semantics` 的「权威表面」Requirement：
  - 正文保留**行为陈述** + 一句「接受面限制 MUST 有明文契约」；
  - **E1 / E2 / E3** 的逐条形态 ⇒ 移进 **3 个 Scenario**（含「修复状态 MUST 可判定」）。

## Acceptance

- [ ] 该 Requirement 的正文 < **500** 字符（实测）；
- [ ] E1 / E2 / E3 三条限制**逐条**可查（每条一个 Scenario）；
- [ ] **零**代码改动；`ctest -LE perf` 零回归；
- [ ] 归档（**禁** `--skip-specs`）。

## Capabilities

- **MODIFIED ×1** ⇒ 归档**禁** `--skip-specs`。

## Impact

- **改动文件**：`openspec/specs/chlib-conditional-semantics/spec.md`（归档时合并）；
- **计数**：**不变**；**爆炸半径**：**零**（**无代码改动**）；**风险**：**极低**。

## Non-Goals

- **不**改任何代码；
- **不**改 E1/E2/E3 的**内容**（只换位置 ⇒ 语义逐字保留）。
