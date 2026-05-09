# ADR-027: LLVM 版本硬编码修复

**状态**: ✅ 已采纳（已执行）
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户

---

## 1. 背景

议题 #21 修复 CMake 中 LLVM 版本（LLVM-18）的硬编码问题。

### 1.1 问题

```cmake
# 旧代码 (CMakeLists.txt:62-73)
find_package(LLVM QUIET)           # 静默检测——找不到也不报错
...
set(LLVM_LIBS LLVM-18)             # 硬编码版本号——锁定为 LLVM-18
...
message(WARNING "LLVM not found")  # 静默降级——JIT 被静默禁用
set(CH_JIT_ENABLE OFF CACHE BOOL "" FORCE)
```

**三个问题**：
1. `LLVM-18` 硬编码 → 系统 LLVM-17/19 时链接失败
2. `find_package(LLVM QUIET)` → 找不到 LLVM 时不报错
3. `WARNING` + 静默 OFF → 用户不知道 JIT 被禁用的原因

### 1.2 修复

| 变更 | 旧 | 新 |
|------|----|----|
| 版本号 | `LLVM-18` | `LLVM-${LLVM_VERSION_MAJOR}` |
| 查找模式 | `QUIET` | `REQUIRED` |
| 失败处理 | `WARNING` + 静默 OFF | `FATAL_ERROR` |

```cmake
# 新代码
find_package(LLVM REQUIRED)
...
set(LLVM_LIBS "LLVM-${LLVM_VERSION_MAJOR}")
...
message(FATAL_ERROR "CH_JIT_ENABLE=ON but LLVM not found.")
```

---

## 2. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：修复 LLVM 版本硬编码和静默降级 | Sisyphus |

---

**相关链接**:
- `CMakeLists.txt:58-74` — 修复位置
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #21
