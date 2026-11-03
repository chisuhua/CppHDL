---
id: feat-platform-matrix
kind: feature
status: active
phase_refs: [phase-3, phase-5]
主题: CI 平台矩阵扩展（macOS-14 已启用 → Windows-2022）
---

## 概述

CppHDL 的 CI 平台覆盖从 Linux-only 扩展为多平台。macOS-14（clang/arm64）已启用并修复 compile errors；Windows-2022 仍延后（MSVC C++20 module 边界 + JIT 路径 platform 差异）。

| 平台 | 状态 |
|---|---|
| Linux（ubuntu-22.04） | ✅ 已启用 |
| macOS-14（clang/arm64） | ✅ 已启用（compile errors 已修） |
| Windows-2022（MSVC） | ⏳ 延后（C++20 module 边界） |

⚠️ **本 feature 只覆盖「默认构建（`BUILD_VERILATOR=OFF`）」的平台矩阵** ——
**Verilator 后端是显式 Linux-only**（2026-11-03 裁决：`BUILD_VERILATOR=ON` 在
macOS/Windows 上 configure 期 `FATAL_ERROR`；理由 = POSIX `dlopen`/`libVtop.so`
假设 + C++23 `std::expected` 的工具链可用性）⇒ ⇒ **Windows 启用不包含 verilator
后端**，也不得把该 guard 当成待修的缺陷（详见根 `AGENTS.md` KNOWN LIMITATIONS
与 `.rddf/improvements/stdlib-expected-missing-in-libcxx-2026-11.md` §7）。

## 跨阶段拆分

### phase-3
- ✅ macOS-14 启用（clang/arm64 compile errors 修复）
- ✅ JIT 路径 nightly 验证（CH_JIT_ENABLE=ON 默认）

### phase-5
- ⏳ Windows-2022 启用（MSVC C++20 module 边界处理）

## 验收标准

- [x] Linux CI 稳定
- [x] macOS-14 CI 稳定
- [ ] Windows-2022 CI 启用（matrix 单行改动）
