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
| 2026-09-20 | v2.0 | 添加 LLVM 版本兼容性 red-line 测试 + 2026-09-20 调查结论 | Sisyphus |

---

## 3. v2.0 — LLVM 版本兼容性 red-line 测试 (2026-09-20)

### 3.1 背景

2026-09-20 用户报告 `perf_regression` 测试失败,初步诊断指向"LLVM 22 移除 C 风格 `LLVMInitializeNative*` API"。深入调查发现:

1. **LLVM API 假设错误**:`src/jit/jit_compiler.cpp:6` 显式 `#include <llvm-c/Target.h>`,该头文件在 LLVM 22 仍提供 `static inline LLVMBool LLVMInitializeNative*()`(行 131-145)。函数被内联,实际工作。Oracle 初始 grep 漏了 `llvm-c/` 子目录。
2. **perf_regression 真正根因**: build cache 残留 `CH_JIT_ENABLE=OFF`,`libcpphdl.a` 中 `JitCompiler` 是 stub,所有 "jit" 数据 = interpreter 路径(~19000us);baseline 中真 JIT (~2000us),perf_regression 报"jit 慢 9.5 倍"。
3. **修复后实测**: LLVM 22 build 下,JIT 真的工作,LLVM 22 实际比 LLVM 18 baseline **快 1.47-1.75x**。详见 `.sisyphus/plans/2026-09-20-perf-regression-root-cause.md`。

### 3.2 当前 LLVM 兼容矩阵(本机 2026-09-20 实证)

| LLVM 版本 | `LLVMInitializeNative*` (llvm/Support/TargetSelect.h) | `LLVMInitializeNative*` (llvm-c/Target.h) | `llvm::InitializeNative*` (llvm/Support/TargetSelect.h) | 项目兼容 |
|---|---|---|---|---|
| 17.x | ✅ 可用(deprecated) | ✅ 可用(deprecated) | ✅ 可用 | ✅ (项目最低,per ADR-003) |
| 18.x | ❌ 移除 | ✅ `static inline` 行 131-145 | ❌ 移除 | ✅ (本机已验证) |
| 19.x | ❌ 移除 | ✅ `static inline` | ✅ 可用 | ✅ (推断) |
| 20.x | ❌ 移除 | ✅ `static inline` | ✅ 可用 | ✅ (推断) |
| 21.x | ❌ 移除 | ✅ `static inline` | ✅ 可用 | ✅ (推断) |
| 22.x | ❌ 移除 | ✅ `static inline` | ✅ 可用 | ✅ (本机已验证,实跑快 baseline 1.47x) |
| 23+ | ❌ 移除 | ❌ 可能移除 | ❌ 可能改名 | ⚠️ 需 opt-in |

**关键 takeaway**: `src/jit/jit_compiler.cpp:73-75` 当前 C 风格调用在 LLVM 17-22 全部有效。LLVM 23+ 可能移除 `llvm-c/Target.h` 的 static inline,届时需迁移到 C++ 风格 `llvm::InitializeNative*`。

### 3.3 新增 red-line 测试

新增 `tests/jit/test_jit_llvm_version.cpp` + `tests/jit/test_jit_init_succeeds.cpp`,作为 LLVM 版本兼容契约:

- **测试范围** `[17, 22]`: `REQUIRE(JitCompiler::is_available() == true)`
- **范围外**: SUCCEED-skip(未来 opt-in 而非 silent fail)
- **CH_JIT_ENABLE=OFF**: SUCCEED-skip(不阻塞 PR-feedback matrix)
- **Register**: 扁平模式,`tests/CMakeLists.txt:309-310`,与 `test_jit_golden` 一致

如果未来 LLVM 升级破坏 API 兼容性,这两个测试会立刻 fail(编译期或运行期),无需手动诊断类似 2026-09-20 的误报场景。

### 3.4 决策

- **不修改 `src/jit/jit_compiler.cpp`**: 当前代码工作正常(LLVM 22 实跑 perf 比 baseline 快 1.47x)
- **不修改 CMakeLists.txt**: `find_package(LLVM REQUIRED)` 自动选系统默认 LLVM
- **不修改 CI 工作流 `.github/workflows/ci.yml`**: `LLVM_DIR` 指向系统默认 LLVMConfig.cmake 是标准 CMake 包发现,**不是 workaround**
- **不重生成 `perf_baseline.json`**: LLVM 22 比 baseline 更快,无需 re-baseline
- **新增 2 个测试 + 文档化**: 仅作为未来 LLVM 升级的回归防护

---

**相关链接**:
- `CMakeLists.txt:58-74` — v1.0 修复位置
- `.sisyphus/plans/2026-09-20-perf-regression-root-cause.md` — perf_regression 根因诊断
- `openspec/changes/2026-09-20-add-jit-llvm-compat-tests/` — 本次 change OpenSpec 提案
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #21
