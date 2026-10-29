---
id: objective-test-coverage-100pct
status: active
created: 2026-10-19
last_revised: 2026-10-30
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

> ⚠️ **2026-10-30 判据已修订**（planner 裁决，见 §11 台账）：上面 L25–L27 的
> **37** 是**历史快照链**（原样保留）；**当前值 = 38**（2026-10-30 实测
> `ctest -N -L verilator_e2e`），且**度量单位已改** ⇒ 「标签条目数」不再作数
> ⇒ ⇒ 完成判据改为「**双后端 parity 源文件数** ≥ 60」（定义见 §2）。
> ⇒ ⇒ 旧口径「标签条目数 ≥ 100」的**诚实上限是 [71, 96]**
> （盘点：`openspec/changes/archive/2026-10-28-dual-backend-full-coverage-inventory-2026-10/design.md` §7）
> ⇒ ⇒ **在现有语料内不可达**（连乐观上限 96 也差 4）⇒ ⇒ 必须修订。

## 2. 目标愿景 + 完成判据

- 所有可 dual 的单元测试走 `DUAL_BACKEND_TEST` macro（interpreter + verilator 双跑）
- ⭐ **【2026-10-30 修订】双后端 parity 源文件数 ≥ 60**（**取代**旧的
  「`ctest -L verilator_e2e` ≥ 100」）

  ⚠️ **为什么必须改**（planner 裁决，依据 = 2026-10-28 只读盘点的实测）：
  旧口径数的是**带 `verilator_e2e` 标签的 ctest 条目**，而这 38 个条目的**真实构成**是
  **28 个** macro 双后端 + **1 个** hand-crafted 真 dual + **9 个** capability 明文
  「MUST 保留原状」的**单后端** verilator 参考实现 ⇒ ⇒ **按标签计数会把
  「单后端 verilator e2e」算成「双后端覆盖」** ⇒ ⇒ 旧判据**奖励的是「加标签」
  而不是「加 parity」**。且旧判据在现有语料内**不可达**（诚实上限 [71, 96]，
  连乐观上限 96 都 < 100）⇒ ⇒ 继续盲做必然到不了 ⇒ ⇒ 判据与「不可达」二者
  只能改一个 ⇒ ⇒ **改判据**（planner 决策权）。

  📐 **新口径的定义（可机械核验）**：
  **双后端 parity 源文件数** = 仓内测试源文件（`tests/**`、`examples/**`）中满足
  以下**任一**条件的文件数：
  1. **剥注释后**含 `DUAL_BACKEND_TEST_PAIR(` 的**调用**
     （⚠️ 必须是**调用**，不是**提到** —— 10 个契约锁定的参考实现只在**注释**里
     提到该宏 ⇒ 子串匹配会误判）；
  2. ∈ **hand-crafted 真 dual 白名单**（当前 **1** 个：
     `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` —— 它在同一
     `TEST_CASE` 内跑 2×`Simulator` + `InterpreterBackend` + `VerilatorBackend`
     ⇒ 语义上就是双后端 parity，只是没用 macro）。

  | 量 | 值 | 来源 |
  |---|---|---|
  | **现状（2026-10-30 实测）** | **29** | 28 个 macro 文件 + 1 个 hand-crafted |
  | **诚实保守上限** | **62** | 29 + 33 个「可转换」的既有条目（盘点 §7） |
  | **阈值** | **≥ 60** | 留 **2** 个条目余量（吸收条目退役 / hand-crafted 被合并 / R6 新增类别判死） |

  ⚠️ **阈值 MUST ≤ 诚实保守上限**（62）—— 否则就是「硬凑」的判据。
  ⚠️ **口径边界（如实声明）**：本口径数的是**源文件**（当前与 ctest 条目 **1:1**，
  例外：`test_cache_smoke.cpp` → 条目 `riscv_mini_cache_smoke`）⇒ **不是** Catch2
  **用例**数（当前 macro 调用共 **184** 次 ⇒ 用例级 parity 面更宽 ⇒ 该数字作为
  **参考量**记录，不作闸门）。
- ~~单元测试 verilator 覆盖 7% → 100%~~ ⇒ ⚠️ **该百分比不可机械核验**
  （design surface 的分母无权威定义）⇒ **由上面的 parity 源文件数代理**；
  本行保留仅作**愿景**陈述，**不**作闸门。
- 完成判据（done-when）：
  - [x] `dual-backend-test-coverage` capability spec 落地 ✅（已达成）
  - [ ] Phase 1-5 of `verilator-parity-all-tests` 全部 ship（**Phase 1–3 已 ship；4–5 未完成**）
  - [ ] ⭐ **双后端 parity 源文件数 ≥ 60**（**2026-10-30 实测 29** ⇒ 未达成；
        口径定义见上 ⇒ 取代旧的「`ctest -L verilator_e2e` ≥ 100」）
  - [x] `ctest -LE perf` 零回归（**2026-10-26 实测 191/191**）+ CTEST_TOTAL 同步（**202**）
  - [x] 判据修订已落**机械锚点**（根 `AGENTS.md` 的 `DUAL_BACKEND_PARITY_TOTAL` 行
        + `scripts/verify_test_counts.sh` 的对应检查）⇒ 防止「静默缩面」

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
- ⚠️ **不要用「给任意条目加 `verilator_e2e` 标签」的方式凑判据** —— 那是旧口径的
  漏洞（标签 ≠ parity）⇒ 新口径只认 **macro 调用**与 **hand-crafted 白名单**
- ⚠️ **不要把 9 个契约锁定的单后端 verilator 参考实现迁到 macro**
  （capability 明文「MUST 保留原状」）⇒ 它们**永远**不计入新口径

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
无（独立 objective；是 `objective-rv32i-e2e` 与 `objective-cache-stub-retired` 的前置）

### 9.2 Go / No-Go Decision Gate
- **Go 判据**（满足全部 3 条 → 转 completed）：
  - [ ] Phase 1-5 全部 ship + archived（**Phase 1–3 已 ship；4–5 未完成**）
  - [ ] ⭐ **双后端 parity 源文件数 ≥ 60**（**2026-10-30 实测 29**；⚠️ 取代旧判据
        「`ctest -L verilator_e2e` ≥ 100」—— 后者的诚实上限 [71, 96] < 100 ⇒
        **现有语料内不可达**；新口径的定义、现状、上限与 margin 理由见 §2）
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
| sprint-2026-10 | revise | 「`ctest -L verilator_e2e` ≥ 100」的**单位**澄清（= dual **文件数**，非用例数） | 采纳 2026-10-27 roadmap 审查 | `add_dual_backend_test` 一个文件注册一个 ctest 条目 ⇒ 在既有文件里加 pair 不推进本指标（实测 37 = 含 `DUAL_BACKEND_TEST_PAIR` 的 `.cpp` 文件数） | ⇒ ⚠️ **2026-10-30 订正**（见下方新行）：本行的「dual **文件数**」单位陈述**已被证伪** —— **38 是标签条目数**，真 dual 文件数 = **29** ⇒ **以新行为准** |
| sprint-2026-10 | revise | ⭐ **判据修订（planner 裁决）**：完成判据 2 与 Go 判据 2 由「`ctest -L verilator_e2e` ≥ 100（标签条目数）」改为「**双后端 parity 源文件数 ≥ 60**」 | **采纳轴 ②（改度量单位）+ 轴 ①（阈值下调至诚实保守上限之下）**；**否决轴 ④**（放松 R6 排除清单 = 改 capability 契约 ⇒ 本次不做，另立 backlog）；**轴 ③（新增有意义的 dual 文件）仍是 Phase 5 的正题**、只是不再被「≥100 这个数」卡住；**R6 候选 grep 的 65–99 bit 盲区维持现状 + 明文登记为已知盲区**（当前全仓仅 1 处 >64-bit `ch_out` 且在契约锁定文件里 ⇒ 零实际影响）⇒ ⇒ **capability `dual-backend-test-coverage` 零 MODIFIED** ⇒ ⇒ **不需要 OpenSpec change** | ① 旧口径数的是**标签**⇒ 把 9 个**单后端**参考实现算成「双后端覆盖」⇒ 奖励加标签而非加 parity；② 旧判据的诚实上限 [71, 96] ⇒ **现有语料内不可达**（乐观上限 96 也差 4）⇒ 盲做必然到不了；③ 新口径 = macro 调用（剥注释）+ hand-crafted 白名单 ⇒ 现状 29 / 上限 62 / 阈值 60（margin 2） |
