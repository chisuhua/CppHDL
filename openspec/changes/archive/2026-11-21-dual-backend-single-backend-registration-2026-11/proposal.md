# Proposal: 把 `tests/test_verif_history_primitives.cpp` 登记为「契约锁定（非 macro 形态）」条目

- **状态**: **立项（2026-11-21 · builder 实施期发现）** · 待归档
- **触发**: 护栏 `scripts/verify_dual_backend_parity.sh` **红**（`unclassified-labeled-entry`）
- **架构依据**: capability `dual-backend-test-coverage` 的
  **Requirement「现有 9 个 verilator 测试 MUST 保留原状」**（契约锁定清单 = 非 macro 形态的
  `verilator_e2e` 条目登记处）

---

## Why

`tests/test_verif_history_primitives.cpp`（ADR-076 的 `.verif` 历史/序列原语判别力套件）
带 `verilator_e2e` 标签 ⇒ ⇒ 它必须登记进 capability 的**契约锁定清单**（该清单 = 非
`DUAL_BACKEND_TEST` macro 形态的 `verilator_e2e` 条目之登记处），否则护栏判红：

```
❌ unclassified-labeled-entry: test_verif_history_primitives：带 `verilator_e2e` 标签但
   **背后没有**双后端 parity，且**不在** capability 的契约锁定清单里
```

⇒ ⇒ **这不是「测试写得不好」**，而是**登记漏项**：该套件与已登记的
`tests/test_verif_wait_level.cpp`（2026-11-20 登记）是**完全同型**（同一份 `.verif` ⇒
IR 解释器 vs 渲染 TB 真 verilator 实跑 ⇒ 逐条 `(check_id, PASS/FAIL)` 运行期比对）⇒ ⇒
后者已登记、前者未登记 ⇒ ⇒ **登记面漏了一项**。

⚠️ **为什么不是「把测试改成 macro 形态」**：`DUAL_BACKEND_TEST_PAIR` 的两个 half 是
「**同一个 DUT 行为**跑两个 eval backend（interpreter / verilator）」；本套件的两个 half 是
「**两份独立载体**（IR 解释器 / 渲染出的 TB）跑**同一份验证源**」⇒ ⇒ 形态**不同** ⇒ ⇒
MUST NOT 迁移到 macro ⇒ ⇒ 只能走**登记**通道。

## What Changes

- `openspec/specs/dual-backend-test-coverage/spec.md` 的
  **Requirement「现有 9 个 verilator 测试 MUST 保留原状」** 追加一段
  **dated 登记**（形状 = 2026-11-20 的 `test_verif_wait_level.cpp` 登记段的同型）⇒ ⇒
  **Requirement 标题（「9 个」）不变**（它指 macro 的 reference implementation 数量）。
- **锚点零变更**：`DUAL_BACKEND_PARITY_TOTAL`（66）/ `DUAL_BACKEND_PARITY_PAIRS`（286）/
  `VERILATOR_E2E_TOTAL`（80）**都不动** ⇒ ⇒ 因为该套件**不写** `DUAL_BACKEND_TEST_PAIR`
  调用，且它**本来就**已被计入 `VERILATOR_E2E_TOTAL`（标签已在）⇒ ⇒ 修的只是**分类**。

## Acceptance Criteria

- [ ] **AC1** `bash scripts/verify_dual_backend_parity.sh` **exit 0**（`unclassified-labeled-entry` 消失）
- [ ] **AC2** `bash scripts/verify_dual_backend_parity.sh --self-test` **exit 0**（自检不被本次登记破坏）
- [ ] **AC3** 三个锚点**逐字不变**（`DUAL_BACKEND_PARITY_TOTAL: 66` · `DUAL_BACKEND_PARITY_PAIRS: 286`
      · `VERILATOR_E2E_TOTAL: 80`）⇒ ⇒ `verify_test_counts.sh` PASS
- [ ] **AC4** `openspec validate --strict` exit 0 · `verify_openspec_delta.sh` PASS ·
      `verify_openspec_purpose.sh` PASS
- [ ] **AC5** 归档走**正常合并**通道（**不得** `--skip-specs`）⇒ ⇒ 主 spec 的该 Requirement
      与 delta **逐字一致**

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| capability `dual-backend-test-coverage` 的**契约锁定清单**（Requirement「现有 9 个 verilator 测试 MUST 保留原状」） | 登记 9 个 reference impl + `test_hierarchy_equivalence.cpp` + `test_public_equivalence_api.cpp` + `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` + `tests/test_verif_wait_level.cpp` | **+ `tests/test_verif_history_primitives.cpp`**（ADR-076 的 `.verif` 历史/序列原语套件 · 形状与 `test_verif_wait_level.cpp` 同型）|
| `bash scripts/verify_dual_backend_parity.sh` | **红**（`unclassified-labeled-entry` ⇒ 该套件带 `verilator_e2e` 标签却不在清单里） | **绿**（`--self-test` 12/12）|
| 三个锚点（`DUAL_BACKEND_PARITY_TOTAL` / `_PAIRS` / `VERILATOR_E2E_TOTAL`） | 66 / 286 / 80 | **66 / 286 / 80（不变）** ⇒ ⇒ 修的是**分类**，不是覆盖面 |
| Requirement 标题（「9 个」） | 9 | **9（不变）** ⇒ ⇒ 它指 macro 的 reference implementation 数量，本段登记的是**再一个非 macro 条目** |
