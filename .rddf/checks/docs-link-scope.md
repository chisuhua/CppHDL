# 文档链接护栏 —— 分级登记表（**派生 · 禁手改**）

> ⚠️ **本文件由 `scripts/lib/docs_link_scope.py --render` 生成** ⇒ ⇒ **手改无效**
> （下次 `--render` 会覆盖 ⇒ 分级漂移会被 C5 抓）。
> 分级规则见 `docs/DOCUMENT-ARCHITECTURE.md` §9.1（治理口径正本）。

**统计**：扫描 483 篇 · L1 **62** 篇 · L2/L3 421 篇

## 显式登记的例外（无法由前缀机械判定）

| 文件 | 级别 |
|---|---|
| `docs/CHANGELOG.md` | L2 |
| `docs/architecture/2026-03-30-cpphdl-architecture-gap-analysis.md` | L2 |
| `docs/architecture/2026-03-30-spinalhdl-port-architecture.md` | L2 |
| `docs/architecture/2026-03-30-state-machine-dsl-design.md` | L2 |
| `docs/architecture/2026-10-30-n4-n6-evidence.md` | L3 |
| `docs/architecture/2026-11-16-soc-rtl-product-and-tb.md` | L2 |
| `docs/architecture/PHASE6-ARCHITECTURE.md` | L2 |
| `docs/architecture/SYNC-REPORT.md` | L2 |
| `docs/phase_f_status.md` | L3 |

## L1 活文档（死链 / 行号 / 本机路径 MUST 全绿）

| # | 文件 |
|---|---|
| 1 | `QUICK_REFERENCE.md` |
| 2 | `READING-GUIDE.md` |
| 3 | `README.md` |
| 4 | `docs/DOCUMENT-ARCHITECTURE.md` |
| 5 | `docs/PROJECT-OVERVIEW.md` |
| 6 | `docs/README.md` |
| 7 | `docs/architecture/2026-11-02-hierarchical-rtl-output.md` |
| 8 | `docs/architecture/2026-11-08-hierarchical-parent-side-landing.md` |
| 9 | `docs/architecture/2026-11-17-rtl-verification-single-source.md` |
| 10 | `docs/architecture/2026-11-19-document-governance.md` |
| 11 | `docs/architecture/plans/phase1-tasks.md` |
| 12 | `docs/architecture/rv32i-microarchitecture.md` |
| 13 | `docs/developer_guide/README.md` |
| 14 | `docs/developer_guide/api-reference/T001-simulator-api.md` |
| 15 | `docs/developer_guide/architecture/verilog-codegen-architecture.md` |
| 16 | `docs/developer_guide/openspec-specs-guide.md` |
| 17 | `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` |
| 18 | `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` |
| 19 | `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md` |
| 20 | `docs/developer_guide/patterns/PORT-NAMING-GUIDE.md` |
| 21 | `docs/developer_guide/rv32i-rtl-delivery-and-sim.md` |
| 22 | `docs/developer_guide/tech-reports/README.md` |
| 23 | `docs/developer_guide/tech-reports/T002-verilog-naming-fix-analysis.md` |
| 24 | `docs/developer_guide/tech-reports/bundle-connection-issue.md` |
| 25 | `docs/developer_guide/tech-reports/perf-report-todo.md` |
| 26 | `docs/developer_guide/tech-reports/pipeline-tick-crash-analysis.md` |
| 27 | `docs/developer_guide/tech-reports/riscv-test-framework-adaptation.md` |
| 28 | `docs/developer_guide/tech-reports/test-bundle-connection-known-issue.md` |
| 29 | `docs/developer_guide/verilator-integration.md` |
| 30 | `docs/riscv-mini/freertos_roadmap.md` |
| 31 | `docs/riscv-mini/test-architecture.md` |
| 32 | `docs/simulation/ARCHITECTURE.md` |
| 33 | `docs/simulation/JIT_ARCHITECTURE_DEBT.md` |
| 34 | `docs/simulation/JIT_ROADMAP.md` |
| 35 | `docs/simulation/PERFORMANCE_TESTS.md` |
| 36 | `docs/simulation/PERF_COMPARISON_REPORT.md` |
| 37 | `docs/simulation/ROADMAP.md` |
| 38 | `docs/simulation/SIM_OPT_PLAN.md` |
| 39 | `docs/simulation/TEST_REG_TIMING_BUG.md` |
| 40 | `docs/skills/README.md` |
| 41 | `docs/skills/SIMULATOR-API-GUIDE.md` |
| 42 | `docs/skills/simulator-api-quickref.md` |
| 43 | `docs/usage_guide/04-simulator-api.md` |
| 44 | `docs/usage_guide/05-component-design.md` |
| 45 | `docs/usage_guide/06-bundle-patterns.md` |
| 46 | `docs/usage_guide/09-testbench-guide.md` |
| 47 | `docs/usage_guide/10-verilator-backend.md` |
| 48 | `docs/usage_guide/11-stream-flow-comparison.md` |
| 49 | `docs/usage_guide/README.md` |
| 50 | `examples/spinalhdl-ported/README.md` |
| 51 | `improvement-approved.md` |
| 52 | `improvement-suggestions.md` |
| 53 | `skills/cpphdl-bundle-patterns/SKILL.md` |
| 54 | `skills/cpphdl-component-lifecycle/SKILL.md` |
| 55 | `skills/cpphdl-input-port-assignment/SKILL.md` |
| 56 | `skills/cpphdl-shift-fix/SKILL.md` |
| 57 | `skills/cpphdl-simulator-port-value/SKILL.md` |
| 58 | `tests/README.md` |
| 59 | `tests/benchmark/README.md` |
| 60 | `tests/fixtures/assertion_quality/README.md` |
| 61 | `tests/fixtures/hierarchical_tree_extract/README.md` |
| 62 | `tests/hier_corpus_registry.md` |

## 规则摘要

- **L1 前缀**：`docs/usage_guide/` · `docs/developer_guide/` · `docs/skills/` · `docs/simulation/` · `skills/` · `tests/` · `examples/` · `samples/`
- **L2/L3 前缀（最长优先）**：`docs/adr/` · `docs/archive/` · `docs/plans/` · `docs/reports/` · `docs/freertos/` · `docs/skills/archive/` · `.rddf/` · `openspec/changes/archive/` · `docs/developer_guide/plans/` · `docs/superpowers/`
- **混合目录派生**：`docs/architecture/phases|*review*|README.md` ⇒ L2，其余 `docs/architecture/*.md` ⇒ L1；仓根散落 `*.md` ⇒ L1（`PHASE1-REVISED-PLAN.md` / `perf_results.md` / `Compare.md` ⇒ L3）；`docs/*.md` ⇒ L1
- **out-of-scope（不扫）**：`AGENTS.md` 家族 · `openspec/**` · `.rddf/checks/**` · `external/**`
- **P1 豁免**：`docs/archive/**`（逐字保留区）

