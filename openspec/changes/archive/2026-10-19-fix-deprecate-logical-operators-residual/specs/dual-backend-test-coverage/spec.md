# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

## ADDED Requirements

### Requirement: deprecate-hdl-logical-operators drift MUST 闭合 by follow-up change

本 archive 的 change（`fix-deprecate-logical-operators-residual`）是 archived `deprecate-hdl-logical-operators-redo`（2026-10-16）的 follow-up。**任意文件含 `ch_bool &&` / `ch_bool ||` / ch_uint 混合 `&&`/`||` 残留 MUST 由独立 follow-up change 闭合**，禁止 redo archive 后再出现 silent drift。

#### Scenario: file-level audit MUST 检测

- **WHEN** reviewer 或 CI 扫描 archived `deprecate-hdl-logical-operators-redo` 后**任意 commit**
- **THEN** MUST `grep -rnE "(ch_bool|ch_uint)\b.*(\|\||\&\&)" include/ examples/ samples/ tests/ src/` 输出 0 行
- **AND** 任何命中 MUST 由独立 change（不在 redo scope）显式处理（修复 / 删除 / 标 explicit dead code）

#### Scenario: dead code 与 live code 分类

- **WHEN** grep 命中某个文件
- **THEN** 该文件 MUST 归类为 (a) active TU 引用 → 必修 (b) shadow dead code → 必 sed-rewrite 闭合 archive 声明 (c) truly dead code → 标 explicit deprecated 且 roadmap 立 follow-up
- **AND** 不得 (b)/(c) 类隐藏漂移导致 archived proposal 声明与代码实际状态不符