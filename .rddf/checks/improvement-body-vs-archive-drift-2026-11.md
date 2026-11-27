---
id: improvement-body-vs-archive-drift-2026-11
kind: ledger
status: active
created: 2026-11-27
last_revised: 2026-11-27
review_by: 2026-12-27
owner: planner
priority: P2
主题: 提案治理 —— improvement 正文与归档事实的漂移
evidence: unmeasured
---

# 台账 · improvement **正文** 与 **归档事实** 的漂移（8 条 · 待人工对账）

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

## 4. 护栏化条件（backlog）

⇒ ⇒ 这 8 条对账完（逐条改判）⇒ ⇒ 才把下面这条**加进** `scripts/verify_improvement_status_truth.sh`：

> **C4（反向矛盾）**：正文 Status 首 token **未**判已完 **且** 同名归档 change 存在且 `tasks.md` 全勾
> ⇒ ⇒ 该提案 MUST 有 `残余:` 字段（声明残余）**或** `状态:` 已是终态词 ⇒ ⇒ 否则红。

⚠️ **现在不加** ⇒ 因为加了本护栏就是红的（这 8 条尚未对账）⇒ ⇒ **护栏必须合并即绿**。

## 5. 关联

- 提案池索引 = `.rddf/improvement-suggestions.md`（2026-11-27 由仓根迁入 `.rddf/`）
- 本轮批量回填的判据 = **正文 Status 首 token**（**不**用「同名归档 change」当交付信号 ⇒ 见 §3）
- 新护栏 = `scripts/verify_improvement_status_truth.sh`（C1 front matter 合法性 · C2 终态单调性 · C3 索引链接可达）
