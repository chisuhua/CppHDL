# Tasks: `chlib` 析构异常规格 vs 类型可组合性（ADR-067 落地）

> **判据来源** = ADR-067 §5（Verification）+ improvement 的 Acceptance
> ⇒ ⇒ 每条 MUST 有**可机械判定**的形式 ⇒ ⇒ 禁散文式判据。
> ⚠️ **执行纪律**：改了 `include/**` 之后，跑 `ctest` 前 MUST 先**全量重建**
> （`ctest` **不会**自动重新构建 ⇒ 只重建被点名的 target ⇒ 其余 target 跑的是**旧二进制**
> ⇒ ⇒ **假绿**）。

## 0. 立项与基线

- [x] **T0** 立项 change `openspec/changes/2026-11-09-chlib-dtor-composability-2026-11/`
      （`proposal.md` · `design.md` · `tasks.md` · `specs/{chlib-state-machine-semantics,chlib-conditional-semantics}/spec.md`）
- [x] **T0.1** 记录**基线**：`CTEST_TOTAL` = **253**（改前）· `git rev-parse HEAD` 记入本文件

## 1. 契约层（ADR-067 决策 ②③④）

- [x] **T1** `include/chlib/state_machine.h`：
      - [x] **T1.1** 新增 `enum class dtor_detection { automatic, explicit_only }`（**带诚实性注释**）
      - [x] **T1.2** `ch_state_machine` 新增第三模板实参 `Detection`（默认 `automatic`）
      - [x] **T1.3** 新增 `static constexpr bool kDetectAtDtor`
      - [x] **T1.4** 析构按形态分派：`automatic` ⇒ `noexcept(false)` + 可捕获异常（**逐字保留** ADR-050 D5）；
            `explicit_only` ⇒ `noexcept` + `CHERROR` 诊断 + `std::terminate()`
      - [x] **T1.5** 析构**照抄**既有两道门（`std::uncaught_exceptions() == 0` · `!in_static_destruction()`）
      - [x] **T1.6** 新增 `check_unresolved()`（**只读** ⇒ 抛可捕获异常）
      - [x] **T1.7** 新增私有 `has_unresolved_contributions()`（**只读**）
      - [x] **T1.8** 头注释新增「③ 析构形态与可组合性」段（形态表 · **选择指引** · **使用契约** · **拷贝警告**）
- [x] **T2** `include/chlib/if_stmt.h`：头注释新增**使用契约**（决策 ③/④）⇒ 两处都有明文

## 2. 机械判据（ADR-067 决策 ⑤）

- [x] **T3** `tests/test_chlib_composability_api.cpp`（**新** · 仓外视角 · **只** include 公共头）：
      - [x] **T3.1** 编译期 4 条 `static_assert`（类型身份 / 默认仍可抛 / 可组合 noexcept / **`optional` 合取落差**）
      - [x] **T3.2** ★ 与下游**同形**的 `ComposableDut : ch::Component` + 按值持有 + `noexcept` 断言
      - [x] **T3.3** 运行期：显式检查**可捕获** + resolve 后**不抛**
      - [x] **T3.4** 运行期：默认形态析构**仍抛可捕获异常**（ADR-050 D5 未被削弱）
      - [x] **T3.5** 运行期：**展开期不终止**（不新增「双异常 ⇒ terminate」路径）
      - [x] **T3.6** 子进程：析构兜底 ⇒ **SIGABRT**（父进程 `readlink` 解析 exe 路径 ⇒ 两种 wait status 形态都判）
- [x] **T4** `tests/CMakeLists.txt`：注册 `add_catch_test(test_chlib_composability_api ...)` ⇒ **新增 1 个 ctest target**

## 3. 静态护栏

- [x] **T5** `scripts/verify_chlib_dtor_composability.sh`（**新** · R1 fixture 编译 / R2 API 面 /
      R3 两份头注释的契约 / R4 capability 契约（主 spec ∨ pending delta））
- [x] **T5.1** `--self-test`：M1（破坏可组合形态 ⇒ R1 红）· M2（抹掉 `if_stmt.h` 契约 ⇒ R3 红）·
      M3（改名 `check_unresolved` ⇒ R2 红）⇒ **原地投毒 ⇒ 必红 ⇒ 还原复绿**
- [x] **T5.2** 主路径绿 + `--self-test` 绿

## 4. 判别力自证（**每条先确认仍能编译** ⇒ 否则跑的是旧二进制）

- [x] **T6** M1（`noexcept(kDetectAtDtor ? false : true)` → `noexcept(false)`）⇒ fixture **必红**
      （实测 **3 处**红：2 × `static_assert` + 1 × 真实「looser exception specification」编译错误）⇒ 还原复绿
- [x] **T6.1** M2 / M3 由 `scripts/verify_chlib_dtor_composability.sh --self-test` 承担

## 5. 契约同步与 CI

- [x] **T7** 2 份 capability delta（**ADDED**）⇒ `specs/`（**禁** `--skip-specs`）
- [x] **T7.1** `bash scripts/verify_openspec_delta.sh` 绿
- [x] **T8** `AGENTS.md` 的 `CTEST_TOTAL` **253 → 254** ⇒ `bash scripts/verify_test_counts.sh` 绿
- [x] **T9** `.github/workflows/ci.yml`：护栏 + `--self-test` 两步挂进 guard 段

## 6. 全量验证

- [x] **T10** **全量重建**（`cmake --build build -j$(nproc)`）⇒ 0 error / 0 warning（新文件）
- [x] **T10.1** `ctest -LE perf --output-on-failure` ⇒ **254 / 254** 绿（基线 253 + 新增 1）
- [x] **T10.2** `bash scripts/verify_test_counts.sh` 绿
- [x] **T10.3** `bash scripts/verify_chlib_dtor_composability.sh` 绿 + `--self-test` 绿
- [x] **T10.4** `bash scripts/verify_fsm_next_wiring.sh` 绿（默认形态零改动的旁证）
- [x] **T10.5** `bash scripts/verify_roadmap_fragments.sh` 绿

## 7. 归档

- [x] **T11** 合并 spec delta ⇒ 主 spec（`chlib-state-machine-semantics` **ADDED** ×1 Requirement ·
      `chlib-conditional-semantics` **ADDED** ×1 Requirement）⇒ ⇒ **禁** `--skip-specs`
- [x] **T11.1** `openspec validate --specs --strict` 绿（⚠️ CLI 须 **1.4.1**）
- [x] **T12** 回填 `phase-6.md` 的 **P15**（✅ + 证据路径）· 回填 improvement ② 的 Status
- [x] **T13** 归档（`openspec/changes/archive/2026-11-09-chlib-dtor-composability-2026-11/`）
