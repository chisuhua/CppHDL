# Design: fix-bundle-reflection

## Context

ADR-014 分析了 Bundle 反射系统的问题：
1. **10 字段限制**：反射系统限制为 10 个字段
2. ~~add_user 实现缺失~~（已延期 - 见 ADR-014 P2）

## Goals / Non-Goals

**Goals:**
- 分析 10 字段限制的根因并修复
- ~~实现 add_user 功能~~（延后）

**Non-Goals:**
- 不改变 Bundle 基本设计
- 不破坏现有 Bundle 用户
- 不修复 add_user（按 ADR-014 P2 分类）

## Decisions

### Bug A: 10 字段限制

**根因**: `CH_GET_NTH_ARG` 宏（bundle_meta.h:15）使用 X-macro 技巧，仅支持 10 个参数。

当传入 23 个参数时（`HazardUnitBundle`），第 11 个参数是字段名而非宏名，
导致字段 11+ 静默丢失。

**修复方案**: 扩展 `CH_BUNDLE_FIELDS_T_11` 到 `CH_BUNDLE_FIELDS_T_20` 宏，
并更新 `CH_GET_NTH_ARG` 参数槽。

### Bug B: add_user 缺失（已取消）

按 ADR-014 P2 分类，此项为"增强，非紧急修复"，从本 change 范围移除。

## Risks / Trade-offs

- **风险**: 低。宏扩展不改变核心逻辑，仅增加容量
- **验证**: 需要编译测试 + 大 Bundle (>10 字段) 测试