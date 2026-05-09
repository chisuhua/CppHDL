# ADR-030: Include 防护风格统一

**状态**: ✅ 已采纳（已执行）
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户

---

## 1. 背景

议题 #24 审查 include 防护风格不一致问题。AGENTS.md 要求全部使用 `#pragma once`。

## 2. 现状与变更

### 转换前

| 风格 | 文件数 | 说明 |
|------|--------|------|
| `#pragma once` | 53 | ✅ |
| `#ifndef` / `#define` / `#endif` | 84 | ❌ |

### 转换后

| 风格 | 文件数 |
|------|--------|
| `#pragma once` | **137** |
| 合法保留的 `#ifndef` | 2（`axi4_full.h`, `axi_interconnect_4x4.h` — 非头文件守卫，用于 `AXI4_AXIRESP_DEFINED` 枚举多重包含保护）|

### 处理方式

| 模式 | 处理 | 数量 |
|------|------|------|
| `#ifndef GUARD_H` / `#define GUARD_H` + 末尾 `#endif` | 自动替换为 `#pragma once` | 81 |
| 注释块后 `#ifndef` | 手动修复（`bundle.h`） | 1 |
| `#ifndef CONDITIONAL_DEFINE` | 保留（非守卫用途） | 2 |
| `#ifdef` / `#ifndef` 条件编译 | 保留（logger.h 等） | N/A |

---

## 3. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：统一全部 84 个 `#ifndef` 守卫为 `#pragma once` | Sisyphus |

---

**相关链接**:
- `include/` — 所有 .h 文件
- `include/AGENTS.md` — 已要求 `#pragma once`
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #24
