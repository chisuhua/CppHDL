# 审稿记录：架构台账 §7「已知残留」的**残留收口同步**（2026-11-07）

> **性质**：architect 角色的「**只同步主题文档**」⇒ ⇒ ⇒ 审查者 = **oracle** 1 轮
> （★ **NOT** critic —— 按 §审查配对：只同步主题文档 / 改措辞 ⇒ **只 oracle**）
> ⇒ ⇒ ⇒ **决策内容零变更**（2026-11-05 §11.1 **A4** 仍然成立 ⇒ ⇒ 本次**只**更新**当前态快照**）

## 1. 范围与触发

| 项 | 内容 |
|---|---|
| **触发** | 用户口令「切 architect 同步台账」（`role.json`：`current_role: architect` · `updated_by: user`） |
| **输入** | `.rddf/checks/decision-request-hierarchical-ledger-residuals-2026-11.md`（builder 提交的裁决请求单 · 口径 **X1** 已由用户 2026-11-07 拍板） |
| **改动对象** | `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 的 **4 行状态格**（行 **1** / **1b** / **5** / **11**）+ §7 前言**一条修订注记** + 行 1 / 5 / 11 的**描述列**删除线标注 |
| **NOT 改动** | 任何 ADR 的决策正文 · `openspec/**` · `scripts/**` · `.rddf/roadmap/**`（★ 均属别的域） |
| **可写性依据** | `docs/architecture/**` ∈ `.rddf/config/arch-docs.json` 的 `arch_paths` |

## 2. 判据来源

| # | 判据 | 来源 |
|---|---|---|
| 1 | 残留 1 / 1b / 5 / 11 的**当前态**必须与仓内事实一致（归档名 / capability / ADDED 条数 / 护栏名 / 记录路径 / 数字） | 仓内可复算（§3） |
| 2 | 行 1b 的闭口口径 MUST = **X1 双层**（T0 产物级 + T1 行为级） | 用户 2026-11-07 拍板 · 已登记于 change `proposal.md §3.6` + `tasks.md §B` + capability `hierarchical-corpus-regression` 的 `## Purpose` |
| 3 | **决策内容零变更** | 2026-11-05 §11.1 **A4**（「P6 / P8 二者均独立立项」+ §11.2 形态约束唯一权威） |
| 4 | 状态格里的每个指针 MUST **可解析**（无死引用） | 本仓「如实登记」纪律 |

## 3. oracle 审稿结论 + 逐条处置

oracle 1 轮（`ses_e4be1ad2fffeyCPzn8qxEjYKR8`）· **每条 finding 逐条应答**：

| # | finding | 判定 | 处置 |
|---|---|---|---|
| 1 | **事实准确性**：行 1 / 1b / 5 / 11 引用的归档名 / capability / ADDED 条数 / 数字 / 护栏名 / 记录路径 | ✅ **成立**（逐条核对归档 delta 的 Requirement 计数、`scripts/verify_hierarchical_p8.sh` 的公共面白名单、登记表的 10 行 DUT、change `proposal.md` 的 132/10/122） | 无需改动 |
| 2 | **断链 ①**：行 1b backlog ② 的「登记 = 归档 change `tasks.md` §B」**指错位置** —— §B 是 `ImmConsistencyDut` 订正记录（其 backlog 是**项 ①** C1 无样本）；122 叶子 T2 批次的真实登记处是 `tests/hier_corpus_registry.md` §「已知边界（只登记，不清理）」+ 硬化 change `proposal.md` §「不做」 | ✅ **成立** | ✅ **已修**：指针改指上述两处 |
| 3 | **断链 ②**：前言注记末尾的「记录 = `.rddf/reviews/hierarchical-ledger-residuals-sync-2026-11.md`」指向**不存在的文件** | ✅ **成立** | ✅ **已修**：本文件即该载体（§4 的跟踪载体也指向它） |
| 4 | **误导风险**：行 1 / 5 / 11 的**描述列**仍是现在时的症状陈述，而状态列写「✅ 已收口」⇒ 只扫描述列的读者会得出相反结论 | ✅ **成立**（本表既有惯例：行 **4 / 12 / 13** 收口时对旧描述加 `~~删除线~~`） | ✅ **已修**：行 1 / 5 / 11 描述列套用同一惯例 |
| 5 | **前言一致性**：注记声称的改动范围与 diff 逐条相符；瑕疵 = 未提及行 11 的跨域工作项 + 「归属列不变」措辞会被读成「表结构没变」 | ✅ **成立** | ✅ **已修**：前言收紧为「未闭口工作的**归属**不变（★ 本次**不**重新指派归属）」；跨域工作项在行 11 状态格 + §4 显式登记 |
| 6 | **越界检查**：diff 只触碰 architect 域文件；「归 builder 补登记」是**边界处理**（architect MUST NOT 改 `scripts/`）⇒ 但**该工作项无跟踪载体** ⇒ builder 可能永远看不到，或把「归 builder」误读为免 change 流程的预授权 | ✅ **成立** | ✅ **已修**：行 11 状态格补「跟踪载体 = 本文件 §4」+「MUST 走正常 change 流程（`.rddf/improvements/` → planner → builder）⇒ **NOT** 凭本行散文直接改 `scripts/`」 |
| 7 | **口径一致性**：行 1b 新描述与 capability `## Purpose` + 归档 change 的 What Changes / tasks §A **同一口径**（9+1+3 / 10 登记 / 122 叶子 backlog / C1 ⏠ 全对得上） | ✅ **成立** | 无需改动 |

**未采纳的替代方案**（oracle 已排除，此处登记以免后人重提）：architect 直接改探针 `RESIDUAL_EVIDENCE`（⇒ 越界，护栏脚本归 builder）· 本轮顺手落实决策请求单的**选项 B**（登记表 + 台账派生 + 探针升护栏 ⇒ 超出本次口令范围，留作独立裁决）· 重写行 11 描述列为「已交付」陈述（⇒ 行 11 描述是 A4 裁决的权威登记本体 ⇒ 决策零变更禁止改写 ⇒ 删除线是更轻的合规手段）。

## 4. 交给 **builder** 的工作项（跟踪载体 · 本节即载体）

| 项 | 内容 |
|---|---|
| **工作项** | 把残留 **11** 补进 `scripts/probe_arch_ledger_residuals.sh` 的 `RESIDUAL_EVIDENCE` 登记（当前只登记行 **1** / **1b** / **5**）⇒ ⇒ 否则**行 11 的陈旧不会被探针自动检出** |
| **证据路径（候选）** | `openspec/changes/archive/2026-11-07-p6-hierarchy-friendliness-diagnosis-2026-11` · `openspec/changes/archive/2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11`（★ 行 11 是**两个**立项 ⇒ ⇒ 登记格式需决定：一条登记两个路径，还是把行 11 拆成 11a / 11b） |
| **流程约束** | MUST 走**正常 change 流程**（`.rddf/improvements/` → planner → builder）⇒ ⇒ **NOT** 凭台账一格散文直接改 `scripts/` |
| **归域** | **builder**（护栏脚本域）· architect 已在本台账行 11 状态格登记该指派 |

## 5. 范围外遗留（**只记录，本次不清理**）

| # | 位置 | 事实 | 归域 |
|---|---|---|---|
| 1 | `.rddf/roadmap/features/feat-hierarchical-rtl-output.md` 的 **P8** 行 | 归档名写成 `2026-11-07-p8-hierarchical-equivalence-public-helper2026-11`（★ **缺连字符** ⇒ ⇒ 按字面找不到该目录） | **planner**（`.rddf/roadmap/**`）⇒ architect MUST NOT 改 |
| 2 | `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` 的「2026-11-06 更新」注记 | 仍写「⇒ ⇒ **待 planner 立项**」⇒ ⇒ P6 / P8 均已收口 ⇒ ⇒ 该注记**已过时** | **guide 维护范畴**（非 architect 的 `arch_paths`） |
| 3 | `.rddf/roadmap/features/feat-hierarchical-rtl-output.md` 的**残留 1b** 行描述 | 仍写「层次化路径在全仓既有语料上**零覆盖**」⇒ ⇒ 与 X1 双层口径 + 台账新描述**不一致** | **planner**（`.rddf/roadmap/**`） |

## 6. 遗留清扫（本次范围）

| 检查项 | 结果 |
|---|---|
| TODO / FIXME / 半截实现 | ✅ 无（台账本次改动无占位符、无半截句） |
| 临时调试日志 / 调试残留 | ✅ 无 |
| 被 skip 的测试 / 未替换的 mock | ✅ 不适用（本次**零**代码改动） |
| 死引用（指向不存在的路径 / 目录） | ✅ **已清零**（oracle finding 2 / 3 两条均已修）⇒ 复验见 §7 |
| 悬空指针（本文件被台账引用） | ✅ **已消解**（本文件即 §7 复验对象之一） |

## 7. 复验命令与结果

```bash
bash scripts/probe_arch_ledger_residuals.sh   # 3 行 OK · 0 STALE
bash scripts/verify_ledger_evidence.sh        # 台账 evidence 核验
bash scripts/verify_roadmap_fragments.sh      # AUTO 块守恒
```

| 复验项 | 结果 |
|---|---|
| `probe_arch_ledger_residuals.sh` | ✅ 3 行 OK · **0** 陈旧 |
| `verify_ledger_evidence.sh` | ✅ PASS（68 个台账 · 0 项待补） |
| `verify_roadmap_fragments.sh` | ✅ PASS（AUTO 块守恒 · 9 个 feature） |
| 台账 §7 表格结构 | ✅ 列数一致（无 pipe 数异常行） |

## 8. 已用轮次

| 角色 | 轮次 | 结论 |
|---|---|---|
| **oracle** | **1** | 5 项成立（2 条断链 + 2 处一致性 + 1 处跟踪载体缺失）⇒ **全部已处置**；2 项判定成立无需改动 |
| **critic** | **0** | ★ **不适用**（只同步主题文档 ⇒ 按 §审查配对**只 oracle** ⇒ 不占 critic 的 2 轮预算） |
