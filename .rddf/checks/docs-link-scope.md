---
# ⚠️ **本文件是台账** ⇒ ⇒ `evidence:` 必填（`scripts/verify_ledger_evidence.sh` 的 R1）
#    ⇒ 指向**派生器自己的护栏**（本表的分级漂移由它抓 ⇒ ⇒ 同一载体）
evidence: probe:scripts/verify_docs_links.sh
---

# 文档链接护栏 —— 分级登记表（**派生 · 禁手改**）

> ⚠️ **本文件由 `scripts/lib/docs_link_scope.py --render` 生成** ⇒ ⇒ **手改无效**
> （下次 `--render` 会覆盖 ⇒ 分级漂移会被 C5 抓）。
> 分级规则见 `docs/DOCUMENT-ARCHITECTURE.md` §9.1（治理口径正本）。

**统计**：扫描 530 篇 · L1 **46** 篇 · L2/L3 484 篇

## 显式登记的例外（无法由前缀机械判定）

| 文件 | 级别 |
|---|---|
| `docs/CHANGELOG.md` | L2 |

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
| 8 | `docs/architecture/2026-11-17-rtl-verification-single-source.md` |
| 9 | `docs/architecture/2026-11-19-document-governance.md` |
| 10 | `docs/architecture/README.md` |
| 11 | `docs/architecture/rv32i-microarchitecture.md` |
| 12 | `docs/developer_guide/README.md` |
| 13 | `docs/developer_guide/architecture/verilog-codegen-architecture.md` |
| 14 | `docs/developer_guide/openspec-specs-guide.md` |
| 15 | `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` |
| 16 | `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` |
| 17 | `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md` |
| 18 | `docs/developer_guide/patterns/PORT-NAMING-GUIDE.md` |
| 19 | `docs/developer_guide/rv32i-rtl-delivery-and-sim.md` |
| 20 | `docs/developer_guide/verilator-integration.md` |
| 21 | `docs/riscv-mini/dual-backend-gap-analysis.md` |
| 22 | `docs/riscv-mini/test-architecture.md` |
| 23 | `docs/simulation/ARCHITECTURE.md` |
| 24 | `docs/skills/README.md` |
| 25 | `docs/skills/cpphdl-bundle-patterns/SKILL.md` |
| 26 | `docs/skills/cpphdl-component-lifecycle/SKILL.md` |
| 27 | `docs/skills/cpphdl-input-port-assignment/SKILL.md` |
| 28 | `docs/skills/cpphdl-shift-fix/SKILL.md` |
| 29 | `docs/skills/cpphdl-simulator-port-value/SKILL.md` |
| 30 | `docs/skills/simulator-api-quickref.md` |
| 31 | `docs/usage_guide/01-simulator-api.md` |
| 32 | `docs/usage_guide/02-component-design.md` |
| 33 | `docs/usage_guide/03-bundle-patterns.md` |
| 34 | `docs/usage_guide/04-testbench-guide.md` |
| 35 | `docs/usage_guide/05-verilator-backend.md` |
| 36 | `docs/usage_guide/06-spinalhdl-comparison.md` |
| 37 | `docs/usage_guide/README.md` |
| 38 | `examples/riscv-mini/RUNNING-ELF.md` |
| 39 | `examples/riscv-mini/tests/riscv-arch-test/PROVENANCE.md` |
| 40 | `examples/riscv-mini/tests/riscv-tests/PROVENANCE.md` |
| 41 | `examples/spinalhdl-ported/README.md` |
| 42 | `tests/README.md` |
| 43 | `tests/benchmark/README.md` |
| 44 | `tests/fixtures/assertion_quality/README.md` |
| 45 | `tests/fixtures/hierarchical_tree_extract/README.md` |
| 46 | `tests/hier_corpus_registry.md` |

## 规则摘要

- **L1 前缀**：`docs/usage_guide/` · `docs/developer_guide/` · `docs/skills/` · `docs/simulation/` · `docs/riscv-mini/` · `docs/architecture/` · `tests/` · `examples/` · `samples/`
- **L2/L3 前缀（最长优先）**：`docs/adr/` · `docs/archive/` · `docs/skills/archive/` · `.rddf/` · `openspec/changes/archive/`
- **混合目录派生**：`docs/architecture/phases|*review*|README.md` ⇒ L2，其余 `docs/architecture/*.md` ⇒ L1；仓根散落 `*.md` ⇒ L1（`PHASE1-REVISED-PLAN.md` / `perf_results.md` / `Compare.md` ⇒ L3）；`docs/*.md` ⇒ L1
- **out-of-scope（不扫）**：`AGENTS.md` 家族 · `openspec/**` · `.rddf/checks/**` · `external/**`
- **P1 豁免**：`docs/archive/**`（逐字保留区）

