## Why

`docs/simulation/PERF_COMPARISON_REPORT.md §6 K1` 描述了一个 JIT 状态污染问题:`perf_main` 同进程顺序跑 TC-07 → TC-08 → TC-09 时,父进程 ORC JIT 状态被污染,导致后续 TC 的 JIT 数据失真。F1-F2 fix(见 `openspec/changes/archive/2026-06-19-fix-jit-orc-state-leak` 与 `openspec/changes/archive/2026-06-19-fix-perf-subprocess-isolation`)通过子进程隔离与 JIT 析构修复了部分场景,但**没有添加显式的 LLVM 版本兼容 red-line 测试**。

2026-09-20 一次 `perf_regression` 误报触发了本次调查(详见 `.sisyphus/plans/2026-09-20-perf-regression-root-cause.md`)。诊断结论:

| 阶段 | 观察 |
|------|------|
| 之前 perf_regression 失败时 | build 缓存残留 `CH_JIT_ENABLE=OFF`,`libcpphdl.a` 中 `JitCompiler` 是 stub,所有 "jit" 数据 = interpreter 路径(~19000us);baseline 是真 JIT(~2000us),perf_regression 报 "jit 慢 9.5 倍" |
| 修复(clear cache + `CH_JIT_ENABLE=ON`) | JIT 真的工作,LLVM 22 实际比 LLVM 18 baseline **更快 1.5-1.7x**,perf_regression 0 regression |

**核心观察**:项目历史上没有任何 LLVM 版本兼容性的显式测试。如果未来 LLVM 升级时 `llvm-c/Target.h` 中的 C 风格 `LLVMInitializeNative*` 被**真正移除**(LLVM 23+ 可能在 `llvm-c/` 中也删除),或 `llvm::InitializeNative*` 在新 LLVM 版本改名,本项目会静默 break — 类似本次 perf_regression 误报的现象,但因为 `static inline` 函数被内联而**不会触发编译错误**,更难诊断。

**Red-line 测试的必要性**:当前 `tests/jit/test_jit_compiler.cpp:17-24` 有一个 `JIT Compiler availability` 测试,但它只验证编译器对象存在,未验证**LLVM 版本契约**与**初始化返回值正确性**。本次 change 添加 explicit red-line tests,作为未来 LLVM 升级的回归防护。

## What Changes

- **新增回归测试** `tests/jit/test_jit_llvm_version.cpp`:验证 `JitCompiler::is_available() == true` 在 `LLVM_VERSION_MAJOR ∈ [17, 22]` 范围内,否则 SUCCEED-skip(未来 LLVM 版本需 opt-in)
- **新增初始化冒烟测试** `tests/jit/test_jit_init_succeeds.cpp`:实例化 `JitCompiler` 并断言非 crash,捕捉任何未来 LLVM API 破坏性变更
- **注册**:在 `tests/CMakeLists.txt` 用扁平模式注册(`add_catch_test` 同 `test_jit_golden`),**不**创建 `tests/jit/CMakeLists.txt`(与现有惯例一致)
- **CH_JIT_ENABLE=OFF 兼容**:测试用 `#if defined(CH_JIT_ENABLED)` 守卫,OFF 配置下走 `SUCCEED("JIT not enabled - skipping")` 分支,避免 CI PR-feedback matrix(用 `-DCH_JIT_ENABLE=OFF`,per AGENTS.md)必挂
- **文档同步**:在 `docs/adr/ADR-027-llvm-version-detection.md` 追加 v2.0 "LLVM 版本兼容性 red-line 测试" 小节,记录 2026-09-20 调查结论(`LLVMInitializeNative*` 在 LLVM 22 仍作为 `static inline` 在 `llvm-c/Target.h` 有效)
- **不在本 change scope 内**:
  - **不修改 `src/jit/jit_compiler.cpp`** — 当前代码在 LLVM 17-22 全部正常工作(实证 TC-07 jit median 134017us vs baseline 196842us,LLVM 22 比 baseline 快 1.47x)
  - **不修改 `LLVMInitializeNative*` 调用** — LLVM 22 中 `llvm-c/Target.h:131-145` 仍提供 `static inline LLVMBool LLVMInitializeNative*()`,inline 后与 C++ 风格等价有效
  - **不修改 LLJIT 内部 API 调用** — `LLJIT::lookup`、`addIRModule`、`LLJITBuilder::create`、`Sym->getValue()` 在 LLVM 18/22 签名一致(经 `include/llvm/ExecutionEngine/Orc/LLJIT.h` 对照实证)
  - **不修改 CMakeLists.txt** — 当前 `find_package(LLVM REQUIRED)` 自动选系统默认 LLVM 即可
  - **不修改 `tests/CMakeLists.txt` 现有测试** — 仅添加 2 行 `add_catch_test`
  - **不动 `CH_JIT_ENABLE` 默认值**(默认 ON)
  - **不动 `JIT_MIN_NODES` 阈值**
  - **不动 `kBackendThresholds`**(perf_regression 阈值未受影响)
  - **不重生成 `perf_baseline.json`** — 当前 baseline 反映 LLVM 18 性能,LLVM 22 比 baseline 更快,无需 re-baseline
  - **不动 `.github/workflows/ci.yml`** — CI 的 `LLVM_DIR` 指向系统默认 LLVMConfig.cmake 是标准 CMake 包发现,不是 workaround

## Capabilities

### New Capabilities

- `jit-llvm-compat-tests`: 定义 LLVM 版本兼容性 red-line 测试契约 — `tests/jit/test_jit_llvm_version.cpp` 必须验证 `JitCompiler::is_available() == true` 在 `LLVM_VERSION_MAJOR ∈ [17, 22]`;超出范围 SUCCEED-skip 不 fail;`tests/jit/test_jit_init_succeeds.cpp` 必须验证 JitCompiler 构造后非 crash。本 change 在此 capability 下新增 2-3 条 `SHALL` 级 REQUIREMENT。

### Modified Capabilities

- 无。`openspec/specs/` 下现有的 `chlib-aggregator/`、`bundle-base-integer-ctor/`、`perf-test-isolation/` 与本 change 无关。

## Impact

| 类别 | 影响 |
|------|------|
| 受影响代码 | 仅新增 2 个测试文件 + 1 个 CMake 注册行;**不修改任何现有源代码** |
| CI | 测试在 `ctest -L base` 中自动发现,`CH_JIT_ENABLE=OFF` 配置下 SUCCEED-skip 不 fail |
| 新增测试 | `tests/jit/test_jit_llvm_version.cpp`(LLVM 版本契约)、`tests/jit/test_jit_init_succeeds.cpp`(JitCompiler 构造可用性) |
| 外部 API | 无 breaking change |
| 依赖 | 无新增依赖 |
| 性能 | 中性 — 新测试仅在 ctest 时跑一次,耗时 < 0.1s |
| 风险 | **极低**:仅新增测试,不动核心代码 |
| 回滚 | 直接 `git revert` 即可;测试删除不影响其他功能 |
| 关联 issue | `openspec/changes/archive/2026-06-19-fix-jit-orc-state-leak`、`openspec/changes/archive/2026-06-19-fix-perf-subprocess-isolation`、`.sisyphus/plans/2026-09-20-perf-regression-root-cause.md` |

## Non-Goals

- 不修改 `src/jit/jit_compiler.cpp`(当前代码工作正常,实证 LLVM 22 比 baseline 快 1.47x)
- 不升级 LLJIT 内部 API(LLVM 18/22 签名一致)
- 不修改 CMakeLists.txt 现有 LLVM 检测逻辑
- 不重生成 `perf_baseline.json`(LLVM 22 比 baseline 更快,无需 re-baseline)
- 不修改 CI 工作流(`LLVM_DIR` 设置是标准 CMake 包发现,非 workaround)
- 不实现 LLVM 16 以下的 backward compat(项目最低要求 LLVM 17,per ADR-003)
- 不支持 LLVM 23+(若有进一步破坏性变更,留作后续 change)
- 不重构 `JitCompiler` 为 PIMPL idiom(架构改进,非兼容性测试)
- 不动 `JIT_MIN_NODES` 阈值
- 不动 `kBackendThresholds`
- 不动测试 framework(Catch2 v3.7.0)与 build system(CMake 3.14+)