# 提案：未驱动字段的**只读诊断** API（B7）

- **架构依据**：`.rddf/improvements/framework-undriven-field-diagnostic-2026-10.md`
  + `.rddf/checks/architect-decisions-pending-2026-11.md` 的 **B7 裁决**
  ——「**只读诊断 API**（增量 · **不改既有语义**）⇒ ⇒ ⇒ 「所有护栏受益」**须配
  可机械核验的判据**（哪些护栏/测试 MUST 使用它）⇒ ⇒ ⇒ **选此项**」
- **规格影响**：**新 capability** `core-node-driver-visibility`（ADDED ×1，
  1 条 Requirement + 5 个 Scenario）⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：小-中（`core/` 的**新增只读 API** + 两个 helper 改用它 +
  1 个新测试文件 + 2 条护栏判据更新）
- **回滚路径**：删新增 API（**零行为影响**）⇒ ⇒ **极易回滚**

## Why

框架对「读一个**未被驱动**的字段 / 代理 / 端口」返回 **0** 且**不报错**
⇒ ⇒ **静默**（本仓第 3 个实例，前两个是 `ch_in` 的 `operator=` 与未驱动代理节点）。

⚠️ **实测的静默机制**（本次读代码确认）：`ch_uint::operator<<=` 在目标**没有节点**时
⇒ ⇒ **另建**一个 assign 节点并**只绑到本地副本** ⇒ ⇒ 调用方**看不到**任何效果；
在目标**有节点**时 ⇒ ⇒ `set_src(0, src)` ⇒ ⇒ **传播**给调用方。
⇒ ⇒ 「驱动不了」与「驱动了但没人在意」在**症状**上**不可区分**（都读 0）。

## What Changes

| 文件 | 改动 |
|---|---|
| `include/core/lnodeimpl.h` | **新增** `ch::core::drive_state`（**五态**枚举）+ `diagnose_drive(const lnodeimpl*)` + 三个便捷谓词（`node_is_driven` / `node_is_literal` / `node_exists`）⇒ **只读**，不改任何读取语义 |
| `include/chlib/stream.h` | `detail::drive_master_ready` 改用 `diagnose_drive(...)` ⇒ 判据**只有一处** |
| `include/chlib/stream_pipeline.h` | `detail::drive_upstream_ready` 同上 ⇒ ⇒ **两份实现不再漂移** |
| `scripts/lib/stream_ready_ownership.py` | **R11 / R13 追加**：helper MUST 用 `diagnose_drive(` 且 MUST NOT **自己重新实现**（出现 `type_lit` / `num_srcs()` ⇒ 红）⇒ ⇒ 「所有护栏受益」的可机械核验形态 |
| `tests/test_node_driver_diagnosis.cpp`（**新**） | **21 断言 / 2 用例**：五态逐一钉住 + 与 helper 的联动（`literal` / `driven` ⇒ 响亮拒绝；`undriven` ⇒ 允许） |
| `tests/CMakeLists.txt` | `add_catch_test(test_node_driver_diagnosis)` |
| 计数锚点 | `CTEST_TOTAL` 209 → **210** · non-perf 198 → **199** · `tests/test_*.cpp` 121 → **122**（verilator_e2e / parity **不变**） |

## Non-Goals

- **不改**任何**读取**语义（读未驱动字段仍返回 0）⇒ ⇒ 零回归；
- **不**把诊断接进 `Simulator` 的读路径（那是「方向 1」，爆炸半径 = 全框架）；
- **不**回答「这个设计会不会跑起来」（框架**不跟踪**「是否连到顶层」⇒ 已知边界）。

## Verification（前瞻 · 可勾选）

- [ ] 五种状态逐一有可执行判据（`no_node` / `literal` / `undriven` /
      `driven_by_literal` / `driven`）
- [ ] 两个 helper MUST 用 `diagnose_drive(` 且 MUST NOT 重新实现（R11 / R13 判）
- [ ] **变异 M1**（`undriven` → `driven`）⇒ 诊断判据转红
- [ ] **变异 M2**（`no_node` → `driven`）⇒ 诊断判据转红
- [ ] `ctest -LE perf` 零回归 · 计数锚点同步
