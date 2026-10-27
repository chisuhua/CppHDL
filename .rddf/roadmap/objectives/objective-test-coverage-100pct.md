---
id: objective-test-coverage-100pct
status: active
created: 2026-10-19
last_revised: 2026-10-26
review_by: 2027-01-17
owner: rdd-planner
priority: P1
manual_deps: []
supersedes: null
theme: 单元测试 100% 双后端（interpreter + verilator）覆盖
---

# Objective: objective-test-coverage-100pct

## 1. 驱动诊断（Why now）

CppHDL 的 ~150 个 `add_catch_test` 单元测试仅走 interpreter 路径，而 verilator codegen 是与 interpreter 并行的独立实现路径。shared-pass 测试只覆盖 ~7% design surface。历史痛点反复证明 interpreter pass ≠ verilator pass（ADR-035 issue #25、strengthen-verilator-e2e-coverage 5 天补 7 fixture、cache-complete-rewrite 的 codegen 路径完全未 e2e 验证）。本 objective 把 dual-backend 覆盖推进到全量，使 codegen 回归在 test-time 暴露而非 nightly。

适用场景：所有 device+port 形态单元测试；不适用场景：纯编译期 / 裸 context / aread-based / >64-bit 端口测试（见 capability spec 排除清单）。

> ⚠️ **2026-10-26 状态更新**：本 objective 的 **Phase 1–3 已 ship**
> （`verilator-parity-all-tests` + `dual-backend-cache-tests` + Batch 1）；
> **Phase 4（RV32I 残余）/ Phase 5（全量）仍未完成**。
> ⇒ `ctest -L verilator_e2e` = **37**（2026-10-26 实测；34 @ 2026-10-23 →
> 35（`test_rv32i_soc_dual_parity`，C3）→ 37（`test_axi_interconnect_4x4` +
> `test_axi_lite_matrix_2x2`，`axi4-interconnect-routing-2026-10`））。
> ⇒ 全量目标（≥ 100）**未达成** ⇒ 本 objective 保持 `active`。

## 2. 目标愿景 + 完成判据

- 所有可 dual 的单元测试走 `DUAL_BACKEND_TEST` macro（interpreter + verilator 双跑）
- `ctest -L verilator_e2e` ≥ 100 test
  ⚠️ **单位澄清（2026-10-27 审查）**：该指标的实现是 `add_dual_backend_test()` =
  `add_catch_test(TEST_NAME …)` + `set_tests_properties(LABELS "base;verilator_e2e")`
  ⇒ **一个文件 = 一个 ctest 条目**，`ctest -N -L verilator_e2e` 数的是**文件/二进制**，
  不是 Catch2 用例数（实测 **37** = 含 `DUAL_BACKEND_TEST_PAIR` 的 `.cpp` 文件数）。
  ⇒ 「≥ 100」= **100 个 dual 文件**。⚠️ 因此「在既有文件里加 dual pair」**不会**推进本指标
  （它只增加覆盖广度，不增加文件数）⇒ Phase 5 的全量推进 MUST 以**新文件**为主，
  或在判据里显式改成「用例数」口径（那是改判据 ⇒ planner 决策门，见 plan T7）
- 单元测试 verilator 覆盖 7% → 100%
- 完成判据（done-when）：
  - [x] `dual-backend-test-coverage` capability spec 落地 ✅（已达成）
  - [ ] Phase 1-5 of `verilator-parity-all-tests` 全部 ship（**Phase 1–3 已 ship；4–5 未完成**）
  - [ ] `ctest -L verilator_e2e` ≥ 100 test（**2026-10-26 实测 37** ⇒ 未达成）
  - [x] `ctest -LE perf` 零回归（**2026-10-26 实测 191/191**）+ CTEST_TOTAL 同步（**202**）

## 3. 架构依据

- **ADR-035** — Verilator 后端（64-bit cap / clock toggle / reset pulse 差异点）
- **ADR-041** — cache Sync/Async（Async 走 golden-value 而非 parity）
- `openspec/specs/dual-backend-test-coverage/spec.md` — 本 objective 的 capability 契约
- `openspec/specs/verilator-e2e-coverage/spec.md` — 现有 9 个 verilator test 契约
- `openspec/specs/mem-read-timing/spec.md` — aread/sread 时序（排除清单依据）
- `.rddf/improvements/verilator-parity-all-tests.md` — 原始需求 draft

## 5. 反例

- 不要把"所有单元测试"字面理解为 100%——~130 个是编译期 / 裸 context / aread / >64-bit，写进排除清单而非强行 dual
- 不要在 macro 里复用单一 device（node_id 是 context-local，必须双 device）
- 不要跳过 `set_jit_enabled(false)`（默认走 JIT 而非 interpreter，parity 失败无法归因）
- 不要用全局 `RESOURCE_LOCK` 之外的并发策略（现有锁已存在，避免同 SHA-1 cache 冲突）

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
无（独立 objective；是 `objective-rv32i-e2e` 与 `objective-cache-stub-retired` 的前置）

### 9.2 Go / No-Go Decision Gate
- **Go 判据**（满足全部 3 条 → 转 completed）：
  - [ ] Phase 1-5 全部 ship + archived（**Phase 1–3 已 ship；4–5 未完成**）
  - [ ] `ctest -L verilator_e2e` ≥ 100 test（**2026-10-27 复核仍 37**；⚠️ 单位 = **dual 文件数**，
        不是 Catch2 用例数 —— 见 §2 的单位澄清）
  - [x] CTEST_TOTAL 同步（**202**）+ `verify_test_counts.sh` 绿
- **No-Go 判据**（任一成立 → defer）：
  - PR feedback 墙钟持续 > 45 min（需降级 Phase 2-5）
  - verilator codegen 暴露无法在合理工期内修复的系统性 bug

### 9.3 跨 objective 影响
- 受此 objective 影响的下游：`objective-rv32i-e2e`（macro 基础设施）+ `objective-cache-stub-retired`（Phase 2 cache 迁移）
- 受上游 objective 影响：无

## 10. next_sprint_candidates
- sprint-2026-10: `verilator-parity-all-tests`（Phase 1 of 5）— macro + 8-10 test + 3 fixture + ci.yml
- sprint-2026-11: `dual-backend-cache-tests`（Phase 2）
- sprint-2026-12+: Phase 3-5

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建（roadmap 首次初始化） | 激活为 P1 | Phase 4 核心目标 |
| sprint-2026-10 | go-decision | capability spec 落地（2026-10-19） | Go（Phase 1 启动） | `dual-backend-test-coverage` spec 13 requirements 通过 validate |
| sprint-2026-10 | revise | 「`ctest -L verilator_e2e` ≥ 100」的**单位**澄清（= dual **文件数**，非用例数） | 采纳 2026-10-27 roadmap 审查 | `add_dual_backend_test` 一个文件注册一个 ctest 条目 ⇒ 在既有文件里加 pair 不推进本指标（实测 37 = 含 `DUAL_BACKEND_TEST_PAIR` 的 `.cpp` 文件数） |
