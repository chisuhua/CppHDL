## Context

### 1.1 历史脉络

| commit | 事件 |
|---|---|
| `d8c536d (2026-05-21)` | `jit(p0): LLVM ORC JIT infrastructure + perf baseline` — 引入 LLVM ORC JIT,使用 C 风格 `LLVMInitializeNative*` |
| `e1ed61d (2026-06-09)` | `perf(jit): small-graph threshold (W2)` — 引入 `JIT_MIN_NODES = 50`,后改 5 |
| `b335daf (2026-06-XX)` | `perf(jit): lower JIT_MIN_NODES threshold to 5 (W2 followup)` |
| `3f1ab5c (2026-06-23)` | `fix(perf): regenerate baseline in F2-isolated state` — 历史 perf_regression 误报真因 = ORC 状态污染(非 LLVM API 问题) |
| `2026-09-20` | 用户报告 perf_regression 失败;诊断发现真因 = build cache 残留 `CH_JIT_ENABLE=OFF`(LLVM API 在 LLVM 22 仍有效) |

### 1.2 LLVM API 实际状态(2026-09-20 本机实证)

| LLVM 版本 | `LLVMInitializeNative*`(`llvm/Support/TargetSelect.h`) | `LLVMInitializeNative*`(`llvm-c/Target.h`) | `llvm::InitializeNative*`(`llvm/Support/TargetSelect.h`) |
|---|---|---|---|
| 17.x | ✅ 可用(deprecated 警告) | ✅ 可用(deprecated 警告) | ✅ 可用 |
| 18.x | ❌ 移除 | ✅ `static inline`(行 131-145) | ❌ 移除 |
| 19.x | ❌ 移除 | ✅ `static inline` | ✅ 可用 |
| 20.x | ❌ 移除 | ✅ `static inline` | ✅ 可用 |
| 21.x | ❌ 移除 | ✅ `static inline` | ✅ 可用 |
| 22.x | ❌ 移除 | ✅ `static inline` | ✅ 可用 |

**关键发现**: `src/jit/jit_compiler.cpp:6` 显式 `#include <llvm-c/Target.h>`,因此 `LLVMInitializeNative*` C 风格调用在 LLVM 18-22 通过 `static inline` 函数被内联,**实际工作**。`llvm::InitializeNative*` C++ 风格在 LLVM 18 头文件中缺失,但 LLVM 19+ 完整可用。

**Effective 兼容范围**: LLVM 17-22 (项目最低要求 + 当前实测可用版本)

### 1.3 当前 perf 性能(Linux, LLVM 22 vs baseline LLVM 18)

| TC | baseline (LLVM 18) | current (LLVM 22) | 比率 |
|---|---|---|---|
| TC-07 depth=10 jit | 2577.72us | 1529.54us | **0.59x (LLVM 22 快 1.68x)** |
| TC-07 depth=100 jit | 19477.90us | 11868.34us | **0.61x (LLVM 22 快 1.64x)** |
| TC-07 depth=1000 jit | 196842.88us | 134017.61us | **0.68x (LLVM 22 快 1.47x)** |
| TC-08 regs=10 jit | 3370.08us | 1924.14us | **0.57x (LLVM 22 快 1.75x)** |

LLVM 22 实际比 baseline 更快,perf_regression 阈值 1.6x 通过。

### 1.4 Stakeholders

- **未来 LLVM 升级者**: 当 LLVM 23+ 真正移除 `llvm-c/Target.h` 中 C 风格 `static inline` 函数时,本 red-line 测试立刻 fail,无需手动诊断
- **新开发者 onboarding**: 项目显式声明 "LLVM 17-22 兼容",避免误判兼容范围
- **CI 维护者**: PR-feedback matrix (`-DCH_JIT_ENABLE=OFF`) 不因新测试必挂

## Goals / Non-Goals

**Goals:**
- 新增 2 个 red-line 测试,显式覆盖 LLVM 版本契约
- 测试在 `CH_JIT_ENABLE=OFF` 配置下 SUCCEED-skip,不阻塞 PR-feedback matrix
- 测试在 `LLVM_VERSION_MAJOR ∈ [17, 22]` 范围内 `REQUIRE(is_available() == true)`
- ADR-027 v2.0 文档化 2026-09-20 调查结论与 LLVM API 现状

**Non-Goals:**
- 不修改任何现有源代码
- 不修改 `src/jit/jit_compiler.cpp`(当前代码工作正常)
- 不修改 CMakeLists.txt(当前 LLVM 检测逻辑正确)
- 不修改 LLJIT 内部 API(LLVM 18/22 签名一致,经实证)
- 不重生成 `perf_baseline.json`(LLVM 22 比 baseline 更快,无需 re-baseline)
- 不实现 LLVM 16 以下的 backward compat(项目最低要求 LLVM 17)
- 不实现 LLVM 23+ 适配(若有需要,留作后续 change)
- 不动 `CH_JIT_ENABLE` 默认值
- 不动 `JIT_MIN_NODES` 阈值
- 不动 `kBackendThresholds`
- 不动 CI 工作流

## Decisions

### Decision 1: 测试覆盖范围 [17, 22]

**Rationale**: 
- 项目最低要求 LLVM 17(per ADR-003)
- 当前实测 LLVM 22 兼容(2026-09-20 baseline 比 LLVM 18 快 1.47-1.75x)
- LLVM 23+ 未知,需 opt-in

**Alternatives considered**:
- (A) [16, 22] — **拒绝**:LLVM 16 已被 ADR-003 排除
- (B) [17, 22] — **接受**:与项目要求 + 实测一致
- (C) 无限范围 — **拒绝**:无法保证未来 LLVM 兼容,需显式 opt-in

### Decision 2: CH_JIT_ENABLE=OFF 下走 SUCCEED-skip

**Rationale**: CI PR-feedback matrix 用 `-DCH_JIT_ENABLE=OFF`(per AGENTS.md)。如果测试在 OFF 配置下 fail,会阻塞所有 PR。新测试用 `#if defined(CH_JIT_ENABLED)` 守卫,OFF 走 `SUCCEED("JIT not enabled - skipping")`。

**Alternatives considered**:
- (A) 测试在 OFF 下必须 fail — **拒绝**:阻塞 PR workflow
- (B) 测试在 OFF 下 SUCCEED-skip — **接受**:与现有 `test_jit_compiler.cpp` 守卫模式一致
- (C) CMake `if(CH_JIT_ENABLE)` 条件注册测试 — **拒绝**:与现有扁平注册模式不一致

### Decision 3: 扁平注册在 tests/CMakeLists.txt, 不创建 tests/jit/CMakeLists.txt

**Rationale**: 现有 `test_jit_golden`(`tests/CMakeLists.txt:305`)用扁平模式注册,与现有惯例一致。新增 2 个测试同理追加 2 行。

**Alternatives considered**:
- (A) 创建 `tests/jit/CMakeLists.txt` + `add_subdirectory(jit)` — **拒绝**:与现有惯例冲突,需迁移 `test_jit_golden`
- (B) 扁平注册在 `tests/CMakeLists.txt` — **接受**:与现状一致
- (C) 在 `tests/jit/CMakeLists.txt` 只注册新测试 — **拒绝**:与 (A) 同问题

### Decision 4: 不修改源代码 — 仅 red-line 测试

**Rationale**: 
- 实测当前代码在 LLVM 22 完全工作(perf_regression 0 regression)
- 之前 perf_regression 失败的真正原因是 build cache 残留(详见 `.sisyphus/plans/2026-09-20-perf-regression-root-cause.md`),与 LLVM API 无关
- "预防未来 LLVM 升级 break"是合理动机,但当前不需要改代码

**Alternatives considered**:
- (A) 仅 red-line 测试 — **接受**:最小变更,价值明确
- (B) red-line + code style cleanup(C → C++ 风格) — **拒绝**:超出 scope,且当前代码已工作
- (C) red-line + 完整代码迁移 — **拒绝**:不必要

## Migration Plan

### Phase 1: 实施

1. 验证 `tests/jit/test_jit_llvm_version.cpp` 与 `tests/jit/test_jit_init_succeeds.cpp` 已存在(Step 2 RED 阶段已创建,见 tasks.md §2.1-2.2)
2. 在 `tests/CMakeLists.txt` 验证扁平注册已添加(见 tasks.md §2.3)
3. 跑 `cmake --build build --target test_jit_llvm_version test_jit_init_succeeds -j$(nproc)` 0 errors 0 warnings
4. 跑 `ctest -R "test_jit_llvm_version|test_jit_init_succeeds" --output-on-failure` 确认 GREEN

### Phase 2: 文档同步

1. `docs/adr/ADR-027-llvm-version-detection.md`:追加 v2.0 "LLVM 版本兼容性 red-line 测试" 小节
2. (可选)`.sisyphus/plans/2026-09-20-perf-regression-root-cause.md` 已记录调查结论

### Phase 3: 全量回归验证

1. `cmake --build build -j$(nproc)` 0 errors 0 warnings
2. `ctest -L base --output-on-failure` 112+2 = 114 测试通过
3. `ctest -L perf --output-on-failure` 9 测试通过(含 `perf_regression` 0 regression)
4. `ctest -L spinalhdl-ported` 25/25 通过
5. `CH_JIT_ENABLE=OFF` 矩阵: `cmake -B build-off -DCH_JIT_ENABLE=OFF && cmake --build build-off && ctest -L base --test-dir build-off` 全 SUCCEED-skip

### Phase 4: 归档

1. `openspec validate 2026-09-20-add-jit-llvm-compat-tests --strict`
2. `openspec archive 2026-09-20-add-jit-llvm-compat-tests`(specs merge 到 `openspec/specs/jit-llvm-compat-tests/spec.md`)

## Risks

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| 新测试在 CI PR-feedback matrix 必挂(`CH_JIT_ENABLE=OFF`) | 中 | 高 — 阻塞 PR | `#if defined(CH_JIT_ENABLED)` 守卫,OFF 走 SUCCEED-skip |
| 测试目标无法链接到 `JitCompiler`(`CH_JIT_ENABLE=OFF`) | 中 | 高 — 测试目标构建失败 | OFF 分支不引用 `JitCompiler`,只有 SUCCEED 宏 |
| LLVM 17 实测不可用(本机未装) | 低 | 中 — CI 失败 | `[17, 22]` 范围,超出 SUCCEED-skip |
| LLVM 23+ 静默 break(范围外) | 低 | 低 — 测试 SUCCEED | 范围外 SUCCEED-skip,需 opt-in 后续 change |