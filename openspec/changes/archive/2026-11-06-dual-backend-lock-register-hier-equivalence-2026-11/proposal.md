# Proposal: 双后端覆盖口径 · 把「层次化逐拍等价」套件登记为契约锁定条目
# （`dual-backend-lock-register-hier-equivalence-2026-11`）

## Why

`scripts/verify_dual_backend_parity.sh` 的交叉核验要求：带 `verilator_e2e` 标签的 ctest 条目
⇒ ⇒ MUST 满足「**背后有**双后端 parity」**或**「属 capability `dual-backend-test-coverage`
的**契约锁定清单**」⇒ ⇒ 否则判 `unclassified-labeled-entry` 违规
（理由：旧口径会把**单后端**条目误算成「双后端覆盖」⇒ 正是该护栏要消灭的漏洞）。

T3-005（`hierarchical-rtl-equivalence-2026-11`）新增 `tests/test_hierarchy_equivalence.cpp`
—— 它比较的是**两份 Verilog 产物**（扁平 vs 层次化）⇒ ⇒ **不**属于 `DUAL_BACKEND_TEST`
的 parity 形态 ⇒ ⇒ 属**单后端（verilator-only）**条目 ⇒ ⇒ MUST 登记进契约锁定清单。

⚠️ **它 MUST 带 `verilator_e2e` 标签**：CI 的 `pr-verilator-fast` job 用
`ctest -L verilator_e2e -LE perf` 跑**真 verilator** ⇒ 不带标签 ⇒ 该套件只在
`BUILD_VERILATOR=OFF` 的通用 job 里跑 ⇒ ⇒ 那里 verilator 不存在 ⇒ ⇒ **恒跳过**
⇒ ⇒ 等价性证据永远不会被真正执行。

## What Changes

1. capability `dual-backend-test-coverage` 的 Requirement
   「现有 9 个 verilator 测试 MUST 保留原状」⇒ 正文**追加**一段登记：
   `tests/test_hierarchy_equivalence.cpp`（层次化 ≡ 扁平的**逐拍等价**套件）
   ⇒ MUST 保留原状（MUST NOT 迁移到 macro）。
   ⚠️ **标题不动**（「9 个」指的是 macro 的 **reference implementation** 数量 ⇒ 仍为 9
   ⇒ ⇒ 标题继续为真；本段登记的是**另一个**单后端条目）。

## 不做（Non-Goals）

- **不**改那 9 个 reference implementation 的任何内容。
- **不**改护栏脚本（`scripts/verify_dual_backend_parity.sh` / `scripts/lib/dual_backend_parity.py`）
  ⇒ ⇒ 本 change 只改**登记**，不改**判据**。

## Acceptance

- [ ] `bash scripts/verify_dual_backend_parity.sh` 退出码 **0**
      （新增条目不再被判 `unclassified-labeled-entry`）
- [ ] `VERILATOR_E2E_TOTAL` 锚点与 `ctest -N -L verilator_e2e` 一致
- [ ] `openspec validate --specs --strict` 通过

## Capabilities

| capability | delta |
|---|---|
| `dual-backend-test-coverage` | **MODIFIED ×1**（「现有 9 个 verilator 测试 MUST 保留原状」正文追加登记段） |
