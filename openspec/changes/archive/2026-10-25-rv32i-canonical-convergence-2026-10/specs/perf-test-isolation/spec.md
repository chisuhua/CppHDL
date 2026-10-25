# Capability: perf-test-isolation

## ADDED Requirements

### Requirement: perf baseline 重生成 MUST 守恒（行集 / 锚点 / 变松披露）

- **WHEN** `tests/benchmark/perf_baseline.json` 被重生成
- **THEN** 新 baseline 的 `(test_name, params, backend)` 行集 MUST 等于基准的行集
- **AND** 基准里 `status=PASS` 且 `median_us > 0` 的锚点 MUST 仍存在**且仍 `> 0`**
  （`perf_regression` 的判据是 `if (base <= 0.0) continue;` ⇒ 锚点降级为 0
  = 该行的未来回归**永久不可见**）
- **AND** 比基准更慢的行 MUST 在 baseline 的 `note` 字段里被**点名**
  （披露的形式、数值、阈值来源、逃逸口禁令见下方 Scenario）
- **AND** 该守恒检查 MUST 由 `scripts/verify_perf_baseline_anchors.sh` 执行
- **AND** 该脚本 MUST 自带 `--self-test`（断言每条判据各自有判别力）

#### Scenario: 锚点被降级为 0 MUST 被抓

- **WHEN** 基准里某行 `status=PASS` 且 `median_us > 0`，而新 baseline 把该行
  降级为 `median_us = 0`（SKIPPED）
- **THEN** 守恒检查 MUST 报违规
- **AND** 违规 MUST 让脚本以**非零**退出码结束（只打印不算 —— CI 见到的是退出码）

#### Scenario: 行消失 MUST 被抓

- **WHEN** 新 baseline 缺少基准里的某一行
- **THEN** 守恒检查 MUST 报违规

#### Scenario: 变松未披露 MUST 被抓

- **WHEN** 某行比基准更慢（超过该 backend 的回归阈值）**且** 未在 `note` 的披露表里
  以 `<test> <params> <backend> old= new= ratio=` 的**行**被点名
- **THEN** 守恒检查 MUST 报违规
- **AND** 判定 MUST **不**接受「三个 token 在 `note` 里各自出现过」
  （`note` 里别的句子会提到同名 token ⇒ 那样判会让该行恒被判为已披露
  ⇒ 披露表漏写它也不会被发现）
- **AND** 披露行里的 `old=` / `new=` / `ratio=` 数值 MUST 与实际计算值一致
  （数值不符 ⇒ 报违规：披露的价值在于「披露了**多大**的变松」）
- **AND** 「更慢」的判据阈值 MUST 从**权威源**（`perf_regression.cpp` 的
  per-backend 阈值常量）读出 ⇒ MUST NOT 用脚本内的**硬编码副本**
  （副本会漂移，且漂移方向可能是**不安全**的：有人放宽 C++ 侧阈值，
  脚本仍按旧值判 ⇒ 放过未披露的变松）
- **AND** 权威源不可读 / 不可解析时 MUST **fail-closed**（报违规）⇒ MUST NOT
  回落到脚本内置的默认值（回落是 **fail-open**：权威源被改写时护栏会静默用
  旧阈值判 ⇒ 恰好复现本条想消除的漂移）
- **AND** 权威阈值表 MUST 覆盖 baseline 里出现的**所有** backend
  （否则那些行的「变松」永远不会被披露）

#### Scenario: 「恢复」标记 MUST NOT 成为降级逃逸口

- **WHEN** 新 baseline 的某行带「恢复」标记（`note_restored`）**且**其
  `median_us` 被降级为 0
- **THEN** 守恒检查 MUST 报违规（标记不是许可证）
- **AND** 带该标记的行 MUST 断言其 `median_us` 与基准**逐字相等**
  （不等 ⇒ 报违规：那不是「恢复」，是偷偷改值）

#### Scenario: 守恒检查自身的失效 MUST 被断言钉住

- **WHEN** 守恒检查的实现被改坏（例如违规只打印不返回非零退出码，或披露判定退回
  子串匹配）
- **THEN** `--self-test` MUST 失败
- **AND** CI MUST 在跑守恒检查**之前**先跑 `--self-test`
  （未测试的闸门 = 形同虚设）

#### Scenario: 基准 ref 解析不到 MUST 显式告警

- **WHEN** 基准 ref（如 CI 的 base branch）无法解析
- **THEN** 守恒检查 MUST 跳过（不阻断 PR）
- **AND** MUST 打印**显式告警**（不得静默跳过 ⇒ 闸门会长期形同虚设）
