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
  | **现状（2026-11-02 实测）** | **37** | 36 个 macro 文件 + 1 个 hand-crafted（`scripts/verify_dual_backend_parity.sh`） |
  | **路线 A 剩余（可转换的既有文件）** | **12** | 2026-11-02 **重跑** plan §2.1 的权威筛选（候选 38）后逐个裁决：🔒 契约锁定 7 · ⛔ 价值判据排除 3 · → 路线 B 3 |
  | **路线 A 终点** | **49** | 37 + 12 |
  | **阈值** | **≥ 60** | 见下方「诚实上限的重述」 |

  #### ⭐ **诚实上限的重述**（**2026-11-02 · 用户裁决 · P1**）

  ⚠️ **旧定义的失效（实测）**：旧定义的「诚实保守上限 = 现状 + 可转换的既有条目数」
  ⇒ ⇒ 而那个「可转换条目数」依赖 2026-10-28 的盘点，它**漏排除**了
  ① 8 个「`_behavior` 兄弟已存在」（转它 = 重复计数）② 4 个「无 runtime 语义」
  ⇒ ⇒ ⇒ **实测真值 = 37 + 12 = 49 < 60**
  ⇒ ⇒ ⇒ ⇒ **⇒ 阈值 60 违反了本 objective 自己写下的不变量「阈值 ≤ 诚实保守上限」**
  ⇒ ⇒ ⇒ ⇒ **⇒ 这不是「还差几个文件」的工程量问题 ⇒ ⇒ 是判据与自己的不变量矛盾**

  ✅ **重述（用户 2026-11-02 裁决，采纳第三条出路）**：

  > **「诚实保守上限」的构成 = 「既有可转条目数」+ 「新增有意义的 dual 文件数」**
  > （objective §5 的**轴 ③**已明文允许「新增有意义的 dual 文件」是 Phase 5 的正题）

  ⇒ ⇒ ⇒ **⇒ 「阈值 60 ≤ 诚实上限」这条不变量【并未被违反】**
  ⇒ ⇒ ⇒ ⇒ **⇒ 被违反的是「诚实上限」的【定义】（只算既有转换）⇒ ⇒ 定义重述即可**
  ⇒ ⇒ ⇒ ⇒ **⇒ 比「降阈值」或「背书硬凑」都更贴近不变量本身**

  ⚠️⚠️ **重述的诚实代价（如实登记）**：一旦「诚实上限」含**新增**文件，它**不再是
  一个有限数字** ⇒ ⇒ ⇒ **数字可达性这条护栏随之失效** ⇒ ⇒ ⇒ 「防硬凑」的责任
  **转移**到别处 ⇒ ⇒ ⇒ ⇒ 三条现存机制承担：
  ① **每批 proposal MUST 逐条论证「为什么这个语义值得双后端覆盖」**
  （critic SHOULD_FIX #2：「补足到 60」的动机表述本身有诱导凑数的嫌疑 ⇒ MUST 改写）；
  ② **计数锚点的机械核验**（`scripts/verify_dual_backend_parity.sh` 的两个锚点）
  防「静默缩面」；
  ③ **转换类 MUST 有等价性要求**（转换要求与原文**逐位等价**）/ **新增类 MUST 有
  手推期望** ⇒ ⇒ 两类的验收标准**不可混用**（防「伪装成转换」）。

  ⚠️ **口径边界（如实声明）**：本口径数的是**源文件**（当前与 ctest 条目 **1:1**，
  例外：`test_cache_smoke.cpp` → 条目 `riscv_mini_cache_smoke`）⇒ **不是** Catch2
  **用例**数（当前 macro 调用共 **210** 次 ⇒ 用例级 parity 面更宽 ⇒ 该数字作为
  **参考量**记录，不作闸门）。
- ~~单元测试 verilator 覆盖 7% → 100%~~ ⇒ ⚠️ **该百分比不可机械核验**
  （design surface 的分母无权威定义）⇒ **由上面的 parity 源文件数代理**；
  本行保留仅作**愿景**陈述，**不**作闸门。
- 完成判据（done-when）：
  - [x] `dual-backend-test-coverage` capability spec 落地 ✅（已达成）
  - [ ] Phase 1-5 of `verilator-parity-all-tests` 全部 ship（**Phase 1–3 已 ship；4–5 未完成**）
  - [ ] ⭐ **双后端 parity 源文件数 ≥ 60**（**2026-11-02 实测 37** ⇒ 未达成；
        口径定义见上 ⇒ 取代旧的「`ctest -L verilator_e2e` ≥ 100」）
  - [x] `ctest -LE perf` 零回归（**2026-10-26 实测 191/191**）+ CTEST_TOTAL 同步（**202**）
  - [x] 判据修订已落**机械锚点**（根 `AGENTS.md` 的 `DUAL_BACKEND_PARITY_TOTAL` 行
        + `scripts/verify_test_counts.sh` 的对应检查）⇒ 防止「静默缩面」

### 2.1bis ⭐ **第二版「诚实上限」实测台账**（2026-11-02 · 路线 B 第 1–5 批之后）

⚠️ **重述之后仍有一个更根本的问题没被回答**：「新增有意义的 dual 文件」在本仓
**实际语料**里到底还有多少？⇒ ⇒ **实测**回答它 ⇒ ⇒ **完整台账**见
`.rddf/checks/p1-honest-ceiling-restatement-2026-11.md`（**owner = planner**，
build 只交数字 + 交证据）。

实测结论（摘要）：

| 量 | 值 |
|---|---|
| 现状（2026-11-02 实测，post `dual-backend-parity-route-a-mem-comb-read`） | **49** |
| 路线 A 剩余（**12**，逐个复核全部成立） | +12 |
| 路线 B 剩余（**1–3**；第 6 批已清掉 `pipeline` 1 个 ⇒ 第 5 批记录的 2–4 减 1） | +1~3 |
| **实测可达上限** | **58 ~ 60**（阈值 60 ⇒ **落在边缘**） |

> ⚠️ 上表是**实测台账**（owner = planner 的判据 + build 交的数字），
> 每次路线 A/B 批次归档后**只更新「现状」与由它推出的算术**，不动判据条款。

> 🔴 **2026-11-02（build 交付的实测发现，判据仍归 planner）**：D 批第 2 个文件
> （`test_mem_comb_read`）逐条核对后**只有 2 个用例**带来新覆盖 —— 其 aread/sread
> **时序对已被** `tests/test_mem_read_timing_dual.cpp` 以**同一判别形态**（寄存器驱动
> 地址）+ 同一组期望值覆盖 ⇒ ⇒ 「路线 A 剩余 **12**」里**至少有 2 个是重复计数**
> ⇒ ⇒ 上表算术已**不自洽**（49 + 12 + 1~3 = 62~64，而上限写 **58~60**）⇒
> ⇒ **需要 planner 重算可达上限**（build 不擅自改判据，只交数字与证据：
> `.rddf/checks/handoff-2026-11-02-parity-route-ab.md` + 本批次 change 的 proposal
> 「可转性」表）。

⚠️ **第一版估算为什么乐观**：路线 B 的「既有文件」池**已见底**
（`test_stream_operators` 全结构性 · `test_lnode_entry_set` / `test_multithread` /
`test_phase4_integration` 无顶层端口 · `test_axi_i2c` 已有 `_behavior` 兄弟）
⇒ ⇒ 真正的新写目标（`combinational` / `converter` / `arithmetic` / `logic` /
`sequential`）**恰好只有 5 个** ⇒ ⇒ 本会话已清掉 4 个。
⚠️ **hand-crafted 白名单已穷尽**（`examples/riscv-mini/tests/` 的 17 个文件逐个查
⇒ ⇒ 只有 1 个真 dual + 1 个 macro ⇒ ⇒ **「扩白名单」机械上不可行**）。

⚠️ **「双后端覆盖的边际价值已被实证」**：路线 B 第 1–6 批共挖出 **5 个真缺陷/分歧**
（`priority_encoder` 注释与实现相反 · `add_with_carry`/`sub_with_borrow` 的 `cin`
被忽略 · `edge_detector` 的 `pos`/`neg` 恒 0 · `johnson_counter` 恒 0 ·
**`sread` 的禁读拍两后端语义分歧**〔第 6 批，已独立立项
`sread-disable-cycle-divergence-2026-11`〔**已修**〕）⇒ ⇒
⇒ **全部是「既有测试恰好用建图期常量 ⇒ 从未暴露」型** ⇒ ⇒ ⇒ **dual 覆盖不只是
「一致性检查」，而是【缺陷发现机制】** ⇒ ⇒ **这个价值不取决于计数**。

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
  - [ ] ⭐ **双后端 parity 源文件数 ≥ 60**（**2026-11-02 实测 37**；⚠️ 取代旧判据
        「`ctest -L verilator_e2e` ≥ 100」—— 后者的诚实上限 [71, 96] < 100 ⇒
        **现有语料内不可达**；新口径的定义、现状、上限与 margin 理由见 §2
        ⇒ ⇒ **2026-11-02 P1 裁决**：「诚实上限」的构成**重述**为
        「既有可转 + **新增有意义 dual 文件**」⇒ ⇒ 不变量**未被违反**
        ⇒ ⇒ 阈值 **60 保持不变** ⇒ ⇒ 路线 A 终点 49 ⇒ ⇒ **差额 11 MUST 由
        路线 B（新增有意义 dual 文件）补足** ⇒ ⇒ ⇒ ⇒ **⇒ 阈值从「可达性
        赌注」变成「工作量目标」⇒ ⇒ 防硬凑的责任转移（见 §2 的三条机制）**)
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
| sprint-2026-11 | revise | ⭐ **P1 裁决：「诚实保守上限」的构成重述**（阈值 **60 保持不变**） | **采纳第三条出路**：把「诚实上限」的构成从「既有可转条目数」改为「既有可转条目数 + **新增有意义的 dual 文件数**」（objective §5 **轴 ③** 已明文允许）⇒ ⇒ **「阈值 ≤ 诚实上限」这条不变量并未被违反** ⇒ ⇒ 被违反的是「诚实上限」的**定义** ⇒ ⇒ **定义重述即可** ⇒ ⇒ **比「降阈值」（倒退）或「背书硬凑」（违反不变量）都更贴近不变量本身** ⇒ ⇒ **capability 零 MODIFIED**（判据口径未变）⇒ ⇒ **不需要 OpenSpec change** | ① 实测：旧定义的「诚实上限」= 37 + 12（路线 A 剩余）= **49 < 60** ⇒ ⇒ **违反不变量**；② 但 §5 **轴 ③** 已把「新增有意义的 dual 文件」列为 Phase 5 的正题 ⇒ ⇒ ⇒ **把它计入上限即自洽**；③ ⚠️ **诚实代价**：上限不再有限 ⇒ ⇒ 「数字可达性」这条护栏失效 ⇒ ⇒ 「防硬凑」责任转移到 §2 新增的三条机制（逐条价值论证 · 计数锚点机械核验 · 转换类等价性 vs 新增类手推期望**不可混用**）⇒ ⇒ ⇒ ⇒ ⇒ **如实登记，不假装仍有数字护栏** |
| sprint-2026-11 | revise | ⭐ **P2 裁决：CI 墙钟的三个杠杆**（`pr-verilator-fast` · `timeout-minutes: 45`） | **采纳杠杆 ①（缓存 `~/.cache/cpphdl/verilator`）+ 杠杆 ③（缓存 `build/verilator-build` + `build/verilator-install`）**；**杠杆 ②（`ctest -I` 分片）经实测订正后采纳** ⇒ ⇒ ⇒ ⇒ **⇒ 三者覆盖三段互不重叠的成本**（③ = verilator 自身构建 **10–30 min → 增量实测 2 s** · ① = 逐 DUT 的 **2.1 s/pair** · ② = 串行测试阶段的**横切**）⇒ ⇒ ⚠️ **合并后的总墙钟【尚未在 CI 实测】** ⇒ ⇒ **MUST 由下一个 PR 的 `pr-verilator-fast` step summary 实测回填，不得在此处宣称具体分钟数** ⇒ ⇒ ⚠️⚠️ **且杠杆 ② 的本机实测是【负结果】**：两片各 23 个、各 **361/362 s** ⇒ ⇒ 与不分片同量级 ⇒ ⇒ 因为 ① **单个** `test_axi_interconnect_4x4` = **281–284 s**（占一片 **78%**）② **新增一个测试会让后续序号奇偶整体翻转** ⇒ 两片**都**跑到那个重测试 ⇒ ⇒ ⇒ **⇒ 按序号分片既不均衡也不稳定** ⇒ ⇒ **分片只保留「为将来容量预留 + 覆盖护栏」的价值，不宣称墙钟收益** ⇒ ⇒ ⇒ ⇒ **⇒ 但 build 侧实测发现 plan §6.1.3 的杠杆 ② 原写法【是错的】⇒ ⇒ 必须换机制** | ① **实测打脸（语法层面）**：plan §6.1.3 写的「`ctest -I 0,2` / `-I 1,2` 把 verilator_e2e 套件跨 job 分片」⇒ ⇒ **`-I` 的前两个位置参数是【区间 start,end】，不是「起点、步长」** ⇒ ⇒ 实测 `-I 0,2` 与 `-I 1,2` 各只选 **2** 个测试、且**完全相同**（区间嵌套）⇒ ⇒ ⇒ **44/46 个测试会静默不跑而 CI 仍绿** ⇒ ⇒ ⇒ **正确的分片语法 = `-I <start>,,<stride>`**（第三位是 stride、end 省略）⇒ ⇒ 实测 **`-I 1,,2` = 23 · `-I 2,,2` = 23 · 交集 0 · 并集 46** ⇒ ⇒ ⇒ ⇒ **⇒ 落 workflow 内的护栏 `Verify shard partition covers all labelled tests`**（判据三条：非空 / 两片不相交 / 并集 == 全集）⇒ ⇒ **负向对照实测**：`-I 0,2 / -I 1,2` ⇒ RED（overlap=2）· `-I 1,,3 / -I 2,,3` ⇒ RED（16+15≠46）· 两片相同 ⇒ RED（overlap=23）⇒ ⇒ ⚠️ **护栏自身的首个版本是错的**：ctest 输出**列对齐**（`Test  #15` 两个空格 vs `Test #113` 一个空格）⇒ 正则 `Test #` 漏掉**全部两位数** ⇒ 算出 `full=20=10+10` ⇒ **绿着但错** ⇒ 已改 `Test +#?[0-9]+` 并复验 `full=46`；⚠️ **另**：`-I` 是在 **`-L` 过滤之后**的列表上生效（实测 `-I 1,12,2`→6 · `1,24,2`→12 · `1,46,2`→23，与过滤后总数**线性**对应）⇒ ⇒ ⇒ **⇒ 不另建 `scripts/ctest_shard.sh`**（`-I <start>,,<stride>` + workflow 内护栏已足够；脚本会多一处**需自证**的活件）；② 杠杆 ① **安全**（缓存 key = SHA-1(verilog 源 + 模板 + flags + verilator 版本) ⇒ **完全内容寻址** ⇒ 外层 CI key 可粗粒度 ⇒ 不会错配）⇒ ⇒ 且它给的是**逐 DUT 增量**（改一个测试文件只失效那个 DUT ⇒ **优于粗粒度分片**）；③ ⚠️ **实测发现真正的「大象」是 verilator 自身的冷构建（10–30 min）** ⇒ ⇒ 杠杆 ①/② 都**不覆盖**它 ⇒ ⇒ ⇒ ⇒ **⇒ 补杠杆 ③（缓存 `verilator-build` + `verilator-install` ⇒ ExternalProject 因 `BUILD_ALWAYS TRUE` 仍会跑一步，但 make 是增量的 ⇒ 10–30 min → ~1–3 min）** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 且 `all` 目标确实依赖 `verilator`（实测 `build/CMakeFiles/Makefile2`：`all: CMakeFiles/verilator.dir/all`）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 缓存 `verilator-build` 是必需的（只缓存 install 不够）** |
