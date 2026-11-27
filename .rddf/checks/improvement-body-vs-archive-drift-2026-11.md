---
id: improvement-body-vs-archive-drift-2026-11
kind: ledger
status: closed
created: 2026-11-27
last_revised: 2026-11-27
review_by: 2026-12-27
owner: planner
priority: P2
主题: 提案治理 —— improvement 正文与归档事实的漂移
evidence: unmeasured
---

# 台账 · improvement **正文** 与 **归档事实** 的漂移（8 条 · ✅ **已对账 2026-11-27**）

> ⚠️ **本文件是台账** ⇒ ⇒ `evidence:` 必填（`scripts/verify_ledger_evidence.sh` 的 R1）。
> **本台账 `evidence: unmeasured`** ⇒ ⇒ 因为**每条都要人读正文 + 归档 change 后才能判** ⇒ ⇒
> **不是**派生态 ⇒ ⇒ 不能挂到任何护栏上 ⇒ ⇒ `R1` 判 WARN（不阻塞）⇒ ⇒ **有意为之**。
>
> **判据来源** = 2026-11-27 的治理审查（`rddf status` 待办池清理）。
> **产生方式** = 「B 口径批量回填」时**故意排除**的一组：正文 `**Status**` 首 token **未**判已完，
> 但 `openspec/changes/archive/` 下存在**同名** change 且其 `tasks.md` 已全勾 ⇒ ⇒
> **正文与归档互相矛盾** ⇒ ⇒ **哪边是真的**要人判 ⇒ ⇒ **不能机械回填**。

## 1. 矛盾的两侧各是什么

| 侧 | 含义 | 可机械判？ |
|---|---|---|
| **正文 `- **Status**` 首 token** | 人写的**提案级**状态（「草稿 v2 ⇒ 待 P0 批准门」/「approved（未开工）」） | ✅ 可读，但**语义**要人判 |
| **归档 change 的 `tasks.md`** | 机器写的**交付级**事实（change 已归档 + task 勾选数） | ✅ 可读、可复算 |

⇒ ⇒ 矛盾只有两种可能，**必须逐条判**：
- **① 正文陈旧**（交付已发生、正文没回填）⇒ ⇒ 处置 = 回填 front matter `状态:` + 更新正文 Status 行；
- **② 流程被绕过**（change 在 P0 批准门**之前**就建了并归档）⇒ ⇒ 处置 = **补审**（metis / critic）+ 登记流程违规。

## 2. 逐条清单（8 条）

| # | 提案 | 正文 Status 首 token | 同名归档 change | task 勾选 | 需判 |
|---|---|---|---|---|---|
| 1 | `arch-residual-registry-2026-11` | `已批准`（2026-11-07 · oracle+critic 通过后批准） | `2026-11-08-arch-residual-registry-2026-11` | 34/34 | ① 正文陈旧（批准后次日即归档）？ |
| 2 | `firmware-load-chain-2026-11` | `approved`（裁决已落定 · **未开工** ⇒ 归 B3） | `2026-11-14-firmware-load-chain-2026-11` | 27/27 | ① 正文陈旧（2026-11-13 复核说「未开工」⇒ 2026-11-14 归档）？ |
| 3 | `memory-timing-contract-2026-11` | `draft`（**未开工** ⇒ 归 B3） | `2026-11-14-memory-timing-contract-2026-11` | 35/35 | ① 正文陈旧（front matter `状态依据` 也写「实施未开工」）？ |
| 4 | `hierarchical-port-landing-matrix-2026-11` | `草稿 v2` ⇒ 待 rdd-builder P0 批准门 | `2026-11-08-hierarchical-port-landing-matrix-2026-11` | 28/32（残余 4 项**全为流程步**：`openspec validate` / no-spec 声明 / `rddf feedback` 不需要 / 按路径 commit） | ① 还是 ②？ |
| 5 | `hierarchical-corpus-regression-2026-11` | `草稿 v2`（**待用户拍板**） | `2026-11-07-hierarchical-corpus-regression-2026-11` | 32/32 | ① 还是 ②（**用户拍板是否发生**）？ |
| 6 | `p6-hierarchy-friendliness-diagnosis-2026-11` | `草稿 v3`（设计已拍板 ⇒ 待 rdd-builder P0 走 5 选项批准门） | `2026-11-07-p6-hierarchy-friendliness-diagnosis-2026-11` | 41/41 | ① 还是 ②（P0 5 选项是否走过）？ |
| 7 | `p8-hierarchical-equivalence-public-helper-2026-11` | `草稿 v2`（**待用户拍板**） | `2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11` | 62/62 | ① 还是 ②？ |
| 8 | `port-naming-contract-guard-2026-11` | `草稿 v3`（设计已拍板 ⇒ 待 rdd-builder P0 走 5 选项批准门） | `2026-11-06-port-naming-contract-guard-2026-11` | 42/42 | ① 还是 ②？ |

## 3. 为什么**不在**本轮机械回填

⚠️ 「**同名归档 change 存在**」**不是**可靠的「已交付」信号 —— 本轮已实测出 **3 条反例**：
`rtl-productization-surface-2026-11` / `soc-rtl-product-and-tb-2026-11` / `rv32i-hierarchical-example-2026-11`
⇒ 三者**归档 change 的 task 全勾**、**同名** ⇒ 但提案**仍有残余工作**
⇒ ⇒ 它们的残余已登记进 front matter 的 `残余:` 字段 ⇒ ⇒ **状态保持非终态**。

⇒ ⇒ 因此本台账的 8 条**必须先判「① 正文陈旧」还是「② 流程被绕过」**，才能定处置 ⇒ ⇒ **人工对账**。

## 4. 护栏化条件（✅ 已落地 2026-11-27）

⇒ ⇒ 这 8 条对账完（逐条改判）⇒ ⇒ 才把下面这条**加进** `scripts/verify_improvement_status_truth.sh`：

> **C4（反向矛盾）**：正文 Status 首 token **未**判已完 **且** 同名归档 change 存在且 `tasks.md` 全勾
> ⇒ ⇒ 该提案 MUST 有 `残余:` 字段（声明残余）**或** `状态:` 已是终态词 ⇒ ⇒ 否则红。

✅ **2026-11-27 已实现**（同一轮：对账完 → 加 C4 → 合并即绿）：
- 实现 = `scripts/lib/improvement_status_truth.py::collect()` 的 C4 段（+ `_archived_changes` / `_tasks_all_checked` / `_has_residual`）
- 判别力 = `--self-test` 的 **F9–F14**（投毒红 + `残余:` 放行 + 终态放行 + 3 条边界：非全勾 / 无 tasks.md / 非同名）
- 现状判据 = 本仓 **0 违规**（81 份提案）⇒ 与本节预期一致

## 5. 关联

- 提案池索引 = `.rddf/improvement-suggestions.md`（2026-11-27 由仓根迁入 `.rddf/`）
- 本轮批量回填的判据 = **正文 Status 首 token**（**不**用「同名归档 change」当交付信号 ⇒ 见 §3）
- 新护栏 = `scripts/verify_improvement_status_truth.sh`（C1 front matter 合法性 · C2 终态单调性 · C3 索引链接可达 · **C4 反向矛盾（2026-11-27 补）**）

## 6. ✅ 对账收口（2026-11-27）

**8/8 判定 = ① 正文陈旧**（无一条呈现「② 流程被绕过」）。逐条处置：

| # | 提案 | 判定依据（独立于归档任务数） | 处置 |
|---|---|---|---|
| 1 | `arch-residual-registry-2026-11` | 已登记在 `improvement-approved.md`「已实施」表 · 34/34 | `状态: implemented` + 正文 Status 回填 |
| 2 | `firmware-load-chain-2026-11` | `include/cpu/riscv/rv32i_asm_encode.h` 已存在 · 27/27 | 同上 + 索引行刷新 |
| 3 | `memory-timing-contract-2026-11` | D-TCM 读口已是 `sread`（`include/cpu/pipeline/rv32i_tcm.h:166` · ADR-069）· 35/35 | 同上 + 索引行刷新 |
| 4 | `hierarchical-port-landing-matrix-2026-11` | 28/32 的 4 条**全为 T6 流程步**（validate / no-spec 声明 / 无 feedback / 按路径 commit），proposal 已含 no-spec 声明 | `状态: implemented` + 正文 Status 回填 |
| 5 | `hierarchical-corpus-regression-2026-11` | 32/32 · proposal/review 记录齐全 | 同上 |
| 6 | `p6-hierarchy-friendliness-diagnosis-2026-11` | 41/41 · P0 5 选项批准门在归档前已走 | 同上 |
| 7 | `p8-hierarchical-equivalence-public-helper-2026-11` | 62/62 | 同上 |
| 8 | `port-naming-contract-guard-2026-11` | 42/42 | 同上 |

**§4 的 C4 护栏已实现**（2026-11-27 · 见 §4）：`scripts/lib/improvement_status_truth.py` 的 C4 段
+ `--self-test` 的 F9–F14。本仓现状 = **0 违规** ⇒ 收口结论现在同时由**逐条记录**与**机械护栏**承担。

⚠️ **防止回退**：本轮把 8 条都判成 ① 后，**C4 已落地且绿**（这 8 条现在都有终态词）。
若将来出现「正文未判已完 + 同名归档全勾」**且**无 `残余:` 的新条目 ⇒ C4 会直接红。

## 7. 追加对账（2026-11-27 · 审计 D5/D6 时新发现 · 第 9 条）

| 提案 | 正文/残余 的自述 | 实测 | 判定 | 处置 |
|---|---|---|---|---|
| `rtl-productization-surface-2026-11` | 残余 = 「**D5 + D6 仍未立项**（其独立 change `rtl-productization-coverage-registration-2026-11` 未归档）」 | 同义的独立 change **已存在且已归档**：`openspec/changes/archive/2026-11-17-examples-rtl-sim-registration-hygiene-2026-11/`（tasks **22/22**）= 落选表 TSV 化 + 补登 9 项 + `spec_checks` 列纯化 + 三判据护栏 + CI 接线 ⇒ 护栏 `verify_examples_rtl_sim_coverage.sh` **PASS**（29 == 17 ∪ 12）+ `--self-test` 3 投毒全红 | **① 正文陈旧**（是**名称假设错误**：残余把预测的 change 名当成了唯一合法载体） | `状态: implemented` + 残余改写 + §6 同款 `状态依据:` |

⚠️ **本条的教训**（写入台账，避免同型复发）：残余里**不要**把「预期会用的 change 名」
当作「是否已交付」的判据 —— 交付可能落在**同义但不同名**的 change 上。
判定 MUST 回到**行为/机械判据**（这里是覆盖守恒护栏 + 投毒自检），而不是文件名。
