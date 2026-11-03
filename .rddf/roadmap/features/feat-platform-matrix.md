---
id: feat-platform-matrix
kind: feature
status: completed
phase_refs: [phase-3, phase-5]
主题: CI 平台矩阵 = 仅 Ubuntu（macOS/Windows 明确不做 · 2026-11-03 裁决）
---

## 概述

CppHDL 的 CI **只跑 Ubuntu**（2026-11-03 用户裁决：「CI只做ubuntu系统, macos/windows不做」）。

⇒ ⇒ macOS-14 的 leg **已从 `build-and-test` 矩阵中移除**（它此前提供 clang/arm64 的编译覆盖）；
Windows-2022 **不再排期**（MSVC C++20 module 边界 + JIT 路径 platform 差异）。

⚠️ **代价（如实登记）**：仓库现在**没有** clang / arm64 / MSVC 的编译覆盖 ⇒ ⇒
「换个编译器就炸」这类问题**只能在本地或将来的独立立项里发现**。
⇒ ⇒ 想恢复多平台覆盖 = **独立立项**（含各自的已知问题清单），不是把 matrix 行加回来。

| 平台 | 状态 |
|---|---|
| Linux（ubuntu-22.04） | ✅ 已启用（**唯一**受支持平台） |
| macOS-14（clang/arm64） | ❌ 不做（2026-11-03 裁决：CI 只跑 Ubuntu） |
| Windows-2022（MSVC） | ❌ 不做（同上） |

⚠️ **Verilator 后端 = Linux-only**（2026-11-03 裁决：`BUILD_VERILATOR=ON` 在
macOS/Windows 上 configure 期 `FATAL_ERROR`；理由 = POSIX `dlopen`/`libVtop.so`
假设 + C++23 `std::expected` 的工具链可用性）⇒ ⇒ 在「CI 只跑 Ubuntu」的前提下
这条 guard **与 CI 范围一致**，不是待修的缺陷（详见根 `AGENTS.md` KNOWN LIMITATIONS
与 `.rddf/improvements/stdlib-expected-missing-in-libcxx-2026-11.md` §7）。

## 跨阶段拆分

### phase-3
- ✅ macOS-14 曾启用并修复 clang/arm64 的 compile errors（**历史事实**）
  —— 2026-11-03 随「CI 只跑 Ubuntu」裁决**从矩阵移除**
- ✅ JIT 路径 nightly 验证（CH_JIT_ENABLE=ON 默认）

### phase-5
- ❌ Windows-2022 启用（MSVC C++20 module 边界处理）—— **明确不做**（2026-11-03 裁决）

## 验收标准

- [x] Linux CI 稳定
- [x] macOS-14 CI 曾稳定（历史：已启用并修复 compile errors；2026-11-03 起**移除**该 leg）
- [x] Windows-2022 **明确不做**（2026-11-03 裁决：CI 只跑 Ubuntu）
