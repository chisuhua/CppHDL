# Tasks：未驱动字段的只读诊断 API（B7）

## C1 —— 框架级只读 API

- [x] `include/core/lnodeimpl.h`：新增 `ch::core::drive_state`（**五态**枚举：
      `no_node` · `literal` · `undriven` · `driven_by_literal` · `driven`）
      + `diagnose_drive(const lnodeimpl*)`（**唯一的判定实现**）
      + 三个便捷谓词（`node_is_driven` / `node_is_literal` / `node_exists`）
- [x] ⚠️ **只读**：MUST NOT 改变任何读取语义（读未驱动字段仍返回 0）

## C2 —— 让两个「响亮拒绝」helper 共用同一判定

- [x] `include/chlib/stream.h` 的 `detail::drive_master_ready` ⇒ 改用
      `ch::core::diagnose_drive(...)`（`switch` 三种状态 ⇒ `CHFATAL_EXCEPTION`）
- [x] `include/chlib/stream_pipeline.h` 的 `detail::drive_upstream_ready` ⇒ 同上
- [x] 护栏 **R11 / R13 追加**：MUST 用 `diagnose_drive(` 且 MUST NOT 重新实现
      （body 里出现 `type_lit` / `num_srcs()` ⇒ 红）⇒ ⇒ 判据只有一处

## C3 —— 判据

- [x] `tests/test_node_driver_diagnosis.cpp`（**新**）：**21 断言 / 2 用例**
      → 五态逐一钉住（含 `node_exists` / `node_is_driven` / `node_is_literal`）
      → 与 helper 的联动：`literal` / `driven` ⇒ **响亮拒绝**；`undriven` ⇒ 允许
- [x] `tests/CMakeLists.txt`：`add_catch_test(test_node_driver_diagnosis)`
- [x] **变异验证**（手动投毒 ⇒ 记录结果）：
      **M1**（`undriven` → `driven`）⇒ 诊断判据 **红**（2 用例 failed）·
      **M2**（`no_node` → `driven`）⇒ 诊断判据 **红**（1 用例 failed）· 还原后复绿
      ⚠️ **实测发现**：`no_node` 与 `driven` 在 **helper 里都导致抛异常**
      ⇒ ⇒ **helper 无法区分二者** ⇒ ⇒ **只有诊断判据能区分** ⇒ ⇒
      ⇒ ⇒ **诊断判据不是冗余的**（这是 M2 期间撞出来的 ⇒ 已写进 spec Scenario）

## C4 —— 计数与文档

- [x] 计数锚点：`CTEST_TOTAL` **210** · non-perf **199** ·
      `tests/test_*.cpp` **122**（verilator_e2e 42 / parity 33·194 **不变**）
- [x] 根 `AGENTS.md` NOTES 条目

## 验收

- [x] `ctest -LE perf` **199/199**（零回归；⚠️ 第一轮因并发进程的坏符号链接导致**陈旧二进制**，已重建后重跑）
- [x] `verify_stream_ready_ownership.sh` PASS + `--self-test` **15/15** 投毒全红
- [x] `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 通过
- [x] `openspec validate node-driver-diagnosis-2026-11 --type change --strict` valid
- [x] `scripts/verify_openspec_delta.sh` 4 条全绿
- [x] 归档：**未**用 `--skip-specs`（新 capability `core-node-driver-visibility` ⇒ ADDED ×1
      ⇒ 1 Requirement / 5 Scenario）
      ⇒ 归档于 `openspec/changes/archive/2026-11-02-node-driver-diagnosis-2026-11/`
      ⇒ ⚠️ archive 生成的 `## Purpose` 是 **TBD** ⇒ ⇒ 已**手写**替换（archive 不合并 Purpose）
