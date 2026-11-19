# 归档区索引（`docs/archive/`）

> **2026-11-19 重组**：活文档区从 483 篇压缩到 40 篇，其余 **141 篇历史材料**按类别归档于此。
>
> ⚠️ **归档 ≠ 可用依据**。这里的文档记录的是**当时**的判断 / 计划 / 报告 ⇒
> **只作历史参考与备份**，**不得**作为当前实现依据（判读口径见 [`../DOCUMENT-ARCHITECTURE.md`](../DOCUMENT-ARCHITECTURE.md) §9.1 的 L2/L3）。
>
> **零删除**：本次重组未删除任何文档，全部保留原文（含相对链接按移动前路径书写）。

---

## 1. 分类目录

| 目录 | 篇数 | 内容 | 代表性文档 |
|---|---:|---|---|
| [`phase-plans/`](phase-plans/) | 35 | **Phase 1–6 的实施计划与完成报告**（2026-04 ~ 06） | `PHASE4-PLAN-2026-04-09.md` · `PHASE6-IMPLEMENTATION-PLAN.md` · `T001-completion-report.md` |
| [`phase-reports/`](phase-reports/) | 20 | **验证 / 基准报告**（Phase 1–4 + 2026-10 的 4 份 verification） | `PHASE4-PERFORMANCE-BENCHMARK-V2.2.md` · `verification-*-2026-10.md` |
| [`architecture-history/`](architecture-history/) | 27 | **早期方案快照 + Phase 6 复盘 + `phases/` 规划** | `2026-03-30-*`（3 篇）· `2026-11-*-phase-6-*review*.md`（4 篇）· `phases/`（13 篇） |
| [`tech-reports/`](tech-reports/) | 10 | **技术分析报告**（一次性缺陷分析 / 实施计划） | `T001-simulator-api.md` · `T002-verilog-naming-fix-analysis.md` · `pipeline-tick-crash-analysis.md` |
| [`simulation-plans/`](simulation-plans/) | 7 | **早期仿真性能优化路线图与计划** | `JIT_ROADMAP.md` · `PERFORMANCE_TESTS.md` · `SIM_OPT_PLAN.md` |
| [`superpowers/`](superpowers/) | 7 | 2026-05~09 的 plan/spec 对（JIT 位宽 / Verilator 集成 / SystemVerilog 迁移） | `plans/2026-06-10-verilator-submodule-integration.md` |
| [`freertos-planning/`](freertos-planning/) | 6 | **FreeRTOS 支持方案（未实施）** —— 仓内无任何 FreeRTOS 实现代码 | `01-specification.md` … `05-freertos-porting-guide.md` · `freertos_roadmap.md` |
| [`old-usage-guides/`](old-usage-guides/) | 7 | **已被取代的旧版使用指南**（内容已并入 `usage_guide/`） | `Bundle_UsageGuide.md` · `SIMULATOR-API-GUIDE.md` |
| [`daily-reports/`](daily-reports/) | 13 | 2026 年初的每日设计笔记 | `DESIGN_component.md` · `coding_internal_*.md` |
| [`spinalhdl-comparison/`](spinalhdl-comparison/) | 2 | SpinalHDL Stream 算子实现的早期对比 | `SpinalHDL_Stream_Operators_Implementation.md` |
| [`problem-reports/`](problem-reports/) | 2 | 早期问题报告副本（**活版本在 `tech-reports/`**） | `bundle-connection-issue.md` |
| [`agents-notes/`](agents-notes/) | 1 | 根 `AGENTS.md` 的历史变更流水（2026-11-05 起，逐字保留） | `AGENTS-NOTES-2026-11.md` |
| [`learning/`](learning/) | 1 | 学习小结 | `cpphdl-bundle-io-pattern-summary.md` |
| [`root-scratch/`](root-scratch/) | 3 | 曾散落在仓根/`docs` 根的历史草稿 | `PHASE1-REVISED-PLAN.md` · `Compare.md` · `phase_f_status.md` |

---

## 2. 旧路径 → 新位置（**给按旧路径找文件的人**）

> 根 `AGENTS.md`、`docs/CHANGELOG.md`、`docs/adr/**` 里仍有**历史路径**（按治理口径不改写正文）。
> 按下表换算：

| 旧路径 | 新位置 |
|---|---|
| `docs/plans/<name>.md` | [`phase-plans/<name>.md`](phase-plans/) |
| `docs/reports/<name>.md` | [`phase-reports/<name>.md`](phase-reports/) |
| `docs/superpowers/{plans,specs}/<name>.md` | [`superpowers/{plans,specs}/<name>.md`](superpowers/) |
| `docs/freertos/<name>.md` · `docs/riscv-mini/freertos_roadmap.md` | [`freertos-planning/<name>.md`](freertos-planning/) |
| `docs/developer_guide/tech-reports/<name>.md` | [`tech-reports/<name>.md`](tech-reports/) |
| `docs/developer_guide/api-reference/T001-simulator-api.md` | [`tech-reports/T001-simulator-api.md`](tech-reports/T001-simulator-api.md) |
| `docs/developer_guide/plans/<name>.md` | [`tech-reports/<name>.md`](tech-reports/) |
| `docs/simulation/<name>.md`（除 `ARCHITECTURE.md`） | [`simulation-plans/<name>.md`](simulation-plans/) |
| `docs/architecture/phases/**` | [`architecture-history/phases/**`](architecture-history/phases/) |
| `docs/architecture/{2026-03-30-*,2026-10-30-n4-n6-evidence,2026-11-08-…,2026-11-*-review*,2026-11-16-…,PHASE6-ARCHITECTURE,SYNC-REPORT}.md` | [`architecture-history/`](architecture-history/) |
| `docs/phase_f_status.md` · `PHASE1-REVISED-PLAN.md` · `Compare.md` | [`root-scratch/`](root-scratch/) |

**使用指南重编号**（2026-11-19）：

| 旧 | 新 |
|---|---|
| `usage_guide/04-simulator-api.md` | [`01-simulator-api.md`](../usage_guide/01-simulator-api.md) |
| `usage_guide/05-component-design.md` | [`02-component-design.md`](../usage_guide/02-component-design.md) |
| `usage_guide/06-bundle-patterns.md` | [`03-bundle-patterns.md`](../usage_guide/03-bundle-patterns.md) |
| `usage_guide/09-testbench-guide.md` | [`04-testbench-guide.md`](../usage_guide/04-testbench-guide.md) |
| `usage_guide/10-verilator-backend.md` | [`05-verilator-backend.md`](../usage_guide/05-verilator-backend.md) |
| `usage_guide/11-stream-flow-comparison.md` | [`06-spinalhdl-comparison.md`](../usage_guide/06-spinalhdl-comparison.md) |

**技能目录合并**（2026-11-19）：仓根 `skills/` 5 篇 → [`../skills/`](../skills/)；`docs/skills/SIMULATOR-API-GUIDE.md` → [`old-usage-guides/`](old-usage-guides/)。

---

## 3. 归档纪律

1. **只增不改**：归档文档不进活文档护栏的修复范围；其内部相对链接按**移动前路径**书写（`#anchor` 可能失效）。
2. **新增历史材料**：直接放对应子目录；新类别则在 §1 加一行。
3. **取回**：`git log --follow` 可追溯原路径与完整历史。
4. **哪天真要删**：先确认无活文档引用（`scripts/verify_docs_links.sh` 只扫 L1，归档内引用不拦）⇒ 经用户确认后再删。

---

**维护**: AI Agent · **建档**: 2026-11-19
