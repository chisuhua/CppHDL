---
evidence: probe:scripts/verify_guard_reachability.sh
---

# 护栏可达性登记（**唯一真相** · phase-6 R23-B）

> **这张表登记「哪些护栏被 CI 引用、在哪些入口被调用、它是否被 gate 掉」**
> ⇒ ⇒ 机器判据 = `scripts/verify_guard_reachability.sh`（护栏本体）+ `scripts/lib/guard_reachability.py`（派生器）。
> ⚠️ **排除类型 MUST 是「派生」的** —— 护栏会从 `.github/workflows/ci.yml` **重新派生**每个脚本的
> 排除类型并与本表**逐字比对** ⇒ ⇒ **手写的额外排除 ⇒ ⇒ 判红** ⇒ ⇒ 否则「排除集合」
> 就是一个恒绿出口（与 `--allow-running-no-tests` 同族）。
> ⚠️ **棘轮**：本表与 `ci.yml` 的引用集合 MUST **双向 1:1** ⇒ ⇒ 引用集合**只许增**
> （减项需显式批准，形态同 `port-naming-exempt.tsv` 的双向红）。
> ⚠️ **「最近一次 executed+绿」列是**易逝**证据**（CI 日志有保留期）⇒ ⇒ 它**不是**判据
> ⇒ ⇒ 只作留痕；**判据**是前两列 + 派生一致性。

## 判据口径（三条**静态可判**的）

| # | 判据 | 派生规则 | 反向判据 |
|---|---|---|---|
| ① | **引用存在** | `ci.yml` 任一 `run:` 块**命令位**出现的 `*.sh`（含 `bash ` 前缀形态）⇒ ⇒ 去重集合（**每次重算**，禁写死） | 引用但文件**不存在** ⇒ ⇒ 必红 |
| ② | **可执行位** | git **index** mode = `100755`（`git ls-files -s`）⇒ ⇒ 排除「**被 `source`**」的库文件 | 非 100755 且非被 source ⇒ ⇒ 必红 |
| ③ | **gate × 环境对齐** | **逐调用点**判定 ⇒ ⇒ 脚本级类型 = 所有调用点里「最可达」的那个 ⇒ ⇒ **全部**调用点都恒 skipped ⇒ ⇒ 才是 `gate-env-never`（真缺陷） | 全部调用点恒 skipped ⇒ ⇒ 必红 |

## 排除类型词表（**派生**出来的闭集）

| 类型 | 含义 | 是否缺陷 |
|---|---|---|
| `none` | 无 gate ⇒ 总是执行 | — |
| `gate-event-only` | 仅特定事件（`pull_request` / `schedule` / `workflow_dispatch`）可达 | 否（**分类登记**，不是缺陷） |
| `gate-matrix` | 仅特定分片可达 | 否 |
| `gate-unevaluated` | 静态不可判的 gate | 否（**显式不可达类型**，不硬编） |
| `gate-env-never` | **全部**调用点都恒 skipped ⇒ ⇒ 该护栏从不执行 | **是 ⇒ 必红** |

## 纪律（**新增护栏时 MUST 遵守**）

1. **可达性 = 前提声明 + 入口对齐** —— 新护栏 MUST 声明它依赖什么（工具 / 版本 / 标签口径 /
   编译器能力 / 历史深度），且这些前提 MUST 与**引用它的那个 step 的实际环境**逐项对齐
   （工具名要**版本化**、标签口径要**闭合**、编译器要**实测能力**而不是假定、
   脚本要**可执行**、自检要**不依赖真实仓历史**）。
2. **自检 MUST 不依赖网络 / git 历史（`HEAD~1`）/ verilator** —— 否则它自己就是第一个不可达护栏。
3. **写完护栏 ≠ 交付** —— MUST 在 CI 里**真正 executed 且绿**过（`skipped` **不算**）。
4. **排除 MUST 派生** —— 禁手写额外排除；排除集合 MUST 闭合且可枚举。
5. ⚠️ **纪律的落点 = 本文件**（`.rddf/checks/`）⇒ ⇒ **不**写根 `AGENTS.md`
   （⚠️ 根 `AGENTS.md` 已超 WARN 阈值 57,344 B ⇒ ⇒ 新增内容 MUST 进 capability / `.rddf/checks/`）。

## 护栏清单（**由 `scripts/verify_guard_reachability.sh --render` 生成** ⇒ ⇒ 禁手改）

── R0：必需文件齐备（fail-closed）
  [0;32m✅ R0 派生器 + CI 配置 + 登记表 齐备[0m
| 脚本 | 调用入口 | 排除类型 | 最近一次 executed+绿 |
|---|---|---|---|
| `run_all_ported_tests.sh` | `examples-runtime:Run examples runtime guard` | `none` | — |
| `scripts/run_all_samples.sh` | `samples-regression:Run samples regression guard` | `none` | — |
| `scripts/verify_agents_size.sh` | `build-and-test:Verify root AGENTS.md size cap`, `build-and-test:Verify root AGENTS.md size cap guard self-test`, `pr-verilator-fast:Verify root AGENTS.md size cap`, `pr-verilator-fast:Verify root AGENTS.md size cap guard self-test` | `none` | — |
| `scripts/verify_arch_ledger_residuals.sh` | `build-and-test:Verify arch ledger residual registry`, `build-and-test:Verify arch ledger residual registry (self-test)` | `none` | — |
| `scripts/verify_assertion_quality_fixtures.sh` | `build-and-test:Verify assertion quality fixtures (spec)`, `pr-verilator-fast:Verify assertion quality fixtures (spec)` | `none` | — |
| `scripts/verify_chlib_dtor_composability.sh` | `build-and-test:Verify chlib dtor composability (ADR-067)`, `build-and-test:Verify chlib dtor composability (self-test)` | `none` | — |
| `scripts/verify_chlib_naming_honesty.sh` | `build-and-test:Verify chlib naming honesty registry`, `build-and-test:Verify chlib naming honesty registry (self-test)` | `none` | — |
| `scripts/verify_component_naming.sh` | `build-and-test:Verify component naming model (path uniqueness + two dimensions)`, `build-and-test:Verify component naming model (self-test)` | `none` | — |
| `scripts/verify_conditional_assign_integrity.sh` | `build-and-test:Verify conditional_assign integrity`, `build-and-test:Verify conditional_assign integrity (self-test)` | `none` | — |
| `scripts/verify_deprecation_calendar_gate.sh` | `build-and-test:Verify deprecation calendar gate`, `build-and-test:Verify deprecation calendar gate (self-test)` | `none` | — |
| `scripts/verify_deprecation_callsites.sh` | `build-and-test:Verify deprecation callsites (0 new call sites)`, `build-and-test:Verify deprecation callsites (self-test)` | `none` | — |
| `scripts/verify_downstream_consumer.sh` | `build-and-test:Verify downstream consumer (installed artifact)`, `build-and-test:Verify downstream consumer (self-test)` | `none` | — |
| `scripts/verify_dual_backend_parity.sh` | `build-and-test:Verify dual-backend parity coverage count`, `build-and-test:Verify dual-backend parity guard self-test`, `pr-verilator-fast:Verify dual-backend parity coverage count`, `pr-verilator-fast:Verify dual-backend parity guard self-test` | `none` | — |
| `scripts/verify_example_assertion_quality.sh` | `build-and-test:Verify example assertion quality`, `pr-verilator-fast:Verify example assertion quality` | `none` | — |
| `scripts/verify_example_test_wiring.sh` | `build-and-test:Verify example test wiring`, `pr-verilator-fast:Verify example test wiring` | `none` | — |
| `scripts/verify_examples_rtl_sim.sh` | `pr-verilator-fast:Verify examples RTL simulation (independent Verilog TB)` | `gate-event-only` | — |
| `scripts/verify_flat_rtl_emit.sh` | `build-and-test:Verify flat RTL emit products`, `build-and-test:Verify flat RTL emit products (self-test)`, `pr-verilator-fast:Verify flat RTL emit products (verilator required)`, `pr-verilator-fast:Verify flat RTL emit products (self-test)`, `nightly-verilator:Verify flat RTL emit products (verilator required)`, `nightly-verilator:Verify flat RTL emit products (self-test)` | `gate-event-only` | — |
| `scripts/verify_fsm_next_wiring.sh` | `build-and-test:Verify FSM next-state wiring`, `build-and-test:Verify FSM next-state wiring (self-test)`, `pr-verilator-fast:Verify FSM next-state wiring`, `pr-verilator-fast:Verify FSM next-state wiring (self-test)` | `none` | — |
| `scripts/verify_get_lnode_entry_set.sh` | `build-and-test:Verify get_lnode entry set`, `build-and-test:Verify get_lnode entry set (self-test)` | `none` | — |
| `scripts/verify_guard_reachability.sh` | `build-and-test:Verify guard reachability`, `build-and-test:Verify guard reachability (self-test)` | `none` | — |
| `scripts/verify_header_odr.sh` | `build-and-test:Verify public-header ODR safety (aggregate header closure)`, `build-and-test:Verify public-header ODR safety (self-test)` | `none` | — |
| `scripts/verify_header_self_contained.sh` | `build-and-test:Verify header self-containment`, `build-and-test:Verify header self-containment (self-test)` | `none` | — |
| `scripts/verify_hier_corpus_coverage.sh` | `pr-verilator-fast:Verify hierarchical corpus coverage (three-way cross-check)`, `pr-verilator-fast:Verify hierarchical corpus coverage guard self-test` | `gate-event-only` | — |
| `scripts/verify_hierarchical_p8.sh` | `build-and-test:Verify public API surface (P8 guard, R1–R4)`, `build-and-test:Verify public API surface guard self-test` | `none` | — |
| `scripts/verify_hierarchical_port_naming.sh` | `pr-verilator-fast:Verify hierarchical port-naming contract (N0–N3)`, `pr-verilator-fast:Verify hierarchical port-naming guard self-test` | `gate-event-only` | — |
| `scripts/verify_hierarchical_rtl.sh` | `pr-verilator-fast:Verify hierarchical RTL products (R1–R7)`, `pr-verilator-fast:Verify hierarchical RTL guard self-test` | `gate-event-only` | — |
| `scripts/verify_hierarchical_tree_extract.sh` | `pr-verilator-fast:Verify hierarchical module-tree extraction` | `gate-event-only` | — |
| `scripts/verify_if_stmt_wiring.sh` | `build-and-test:Verify conditional-block assignment wiring`, `build-and-test:Verify conditional-block assignment wiring (self-test)`, `pr-verilator-fast:Verify conditional-block assignment wiring`, `pr-verilator-fast:Verify conditional-block assignment wiring (self-test)` | `none` | — |
| `scripts/verify_input_port_additions.sh` | `build-and-test:Verify input-port additions (event-driven, PR only)`, `build-and-test:Verify input-port additions (self-test)` | `none` | — |
| `scripts/verify_input_port_required_registry.sh` | `build-and-test:Verify input-port required registry (D2-A)`, `build-and-test:Verify input-port required registry (self-test)` | `none` | — |
| `scripts/verify_jit_golden_coverage.sh` | `build-and-test:Verify JIT golden coverage floor`, `build-and-test:Verify JIT golden coverage floor (self-test)` | `none` | — |
| `scripts/verify_ledger_evidence.sh` | `build-and-test:Verify ledger evidence (discipline M)`, `build-and-test:Verify ledger evidence (self-test)` | `none` | — |
| `scripts/verify_multi_if_output_assignability.sh` | `build-and-test:Verify multi_if output assignability (E1) and condition (E3)`, `build-and-test:Verify multi_if output assignability (self-test)` | `none` | — |
| `scripts/verify_multi_if_width.sh` | `build-and-test:Verify multi_if acceptance surface`, `build-and-test:Verify multi_if acceptance surface (self-test)` | `none` | — |
| `scripts/verify_no_duplicate_rv32i_headers.sh` | `build-and-test:Verify RV32I single-source convergence`, `build-and-test:Verify RV32I single-source convergence (self-test)`, `pr-verilator-fast:Verify RV32I single-source convergence`, `pr-verilator-fast:Verify RV32I single-source convergence (self-test)` | `none` | — |
| `scripts/verify_node_names_single_source.sh` | `build-and-test:Verify node-names single source (R1–R3)`, `build-and-test:Verify node-names single source guard self-test` | `none` | — |
| `scripts/verify_openspec_delta.sh` | `build-and-test:Verify OpenSpec delta self-check`, `pr-verilator-fast:Verify OpenSpec delta self-check` | `none` | — |
| `scripts/verify_perf_baseline_anchors.sh` | `build-and-test:Verify perf baseline anchor conservation`, `build-and-test:Verify perf baseline anchor conservation`, `pr-verilator-fast:Verify perf baseline anchor conservation`, `pr-verilator-fast:Verify perf baseline anchor conservation` | `none` | — |
| `scripts/verify_port_naming.sh` | `build-and-test:Verify port naming ratchet`, `build-and-test:Verify port naming ratchet (self-test)` | `none` | — |
| `scripts/verify_public_api_visibility.sh` | `build-and-test:Verify public API removal visibility (issue`, `build-and-test:Verify public API removal visibility (self-test)` | `none` | — |
| `scripts/verify_roadmap_fragments.sh` | `build-and-test:Verify roadmap fragments consistency`, `build-and-test:Verify roadmap fragments consistency (self-test)`, `pr-verilator-fast:Verify roadmap fragments consistency`, `pr-verilator-fast:Verify roadmap fragments consistency (self-test)` | `none` | — |
| `scripts/verify_rst_n_wiring.sh` | `build-and-test:Verify rst_n wiring`, `pr-verilator-fast:Verify rst_n wiring` | `none` | — |
| `scripts/verify_stream_domain_closure.sh` | `build-and-test:Verify stream domain closure checklist`, `build-and-test:Verify stream domain closure checklist (self-test)` | `none` | — |
| `scripts/verify_stream_ready_ownership.sh` | `build-and-test:Verify stream ready ownership`, `build-and-test:Verify stream ready ownership (self-test)` | `none` | — |
| `scripts/verify_test_counts.sh` | `build-and-test:Verify AGENTS.md test counts`, `pr-verilator-fast:Verify AGENTS.md test counts` | `none` | — |
