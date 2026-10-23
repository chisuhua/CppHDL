---
id: phase-4
kind: phase
status: done
phase_refs: []
主题: [dual-backend 测试完整化, cache 子系统重建, 逻辑算子 deprecation 落地]
---

## phase-4 概览

Phase 4 是**当前进行中**的阶段：把 CppHDL 的测试与组件基础设施推向"双后端可验证"状态。核心三条线：
1. **cache 子系统重建** — ADR-037 撞墙事件后，`*_complete.h` 用 `ch_mem` 重写（ADR-039），并拆 Sync/Async 双变体（ADR-041）
2. **dual-backend test 完整化** — 单元测试统一跑 interpreter + verilator，`dual-backend-test-coverage` capability spec 已建，change `verilator-parity-all-tests` proposal/design 已就绪
3. **逻辑算子 deprecation** — ADR-036 已落地 `[[deprecated]]` 标记（35 文件 migration）

| Theme | 覆盖 | 状态 |
|-------|------|------|
| cache 子系统重建 | `include/cpu/cache/{i,d}_cache_{sync,async,complete}.h` | ✅ 已实施（待 verilator e2e 补强） |
| dual-backend test | `tests/dual_backend_fixture.h`（待建）+ 8-10 基础 test 迁移 | 🔄 进行中（Phase 1 of 5） |
| 逻辑算子 deprecation | 8 个 `ch_bool` overload `[[deprecated]]` | ✅ 已实施（移除待 Phase 5） |

## 已实施能力

- **cache-complete-rewrite**（ADR-039）— `*_complete.h` 从 C-style `ch_reg` 数组重写为 `ch_mem`-based（commit `91a0833`），修复 dead-on-arrival 11 build errors
- **cache Sync/Async variant split**（ADR-041）— `ICacheSync` / `DCacheSync`（1-cycle sread，default）与 `ICacheAsync` / `DCacheAsync`（0-cycle aread，TCM-style）；`*_complete.h` 变为兼容 shim（commit `49902b2`）
- **cache 行为测试** — `tests/chlib/test_cache_complete.cpp` 9 个 test_case / 53 assertions（miss-fill / write-through / g1 / g2 / variant equivalence）
- **deprecate-hdl-logical-operators**（ADR-036）— 35 文件 migration + 8 个 `ch_bool` overload `[[deprecated]]`（2026-10-16）
- **dual-backend-test-coverage capability spec** — 13 requirements，`openspec validate --specs --strict` 通过（2026-10-19）

## 进行中工作

- **`verilator-parity-all-tests` change**（Phase 1 of 5）— proposal.md + design.md + specs/ delta 已就绪；等 builder 阶段填 tasks.md 并实施
- **cache 双后端 e2e** — cache-complete-rewrite 的 Verilator codegen 路径尚未 e2e 验证（`dual-backend-cache-tests` Phase 2 目标）

## 架构文档锚点

| 文档 | 与本 phase 关联 |
|------|----------------|
| `docs/adr/ADR-039-cache-complete-rewrite-via-ch-mem.md` | cache 重写路径 |
| `docs/adr/ADR-041-cache-sync-async-variant.md` | cache Sync/Async 拆分 |
| `docs/adr/ADR-036-deprecate-hdl-logical-operators.md` | 逻辑算子 deprecation |
| `docs/adr/ADR-037-cache-stub-removal.md` | stub 物理删除（Phase 5 触发） |
| `openspec/specs/cpu-cache-subsystem/spec.md` | cache 子系统契约 |
| `openspec/specs/dual-backend-test-coverage/spec.md` | dual-backend 契约 |
| `openspec/specs/mem-read-timing/spec.md` | aread/sread 时序契约 |

## 完成条件

- [x] cache-complete-rewrite 落地 + 归档
- [x] ADR-041 Sync/Async 拆分落地
- [x] ADR-036 deprecation 标记落地
- [x] `dual-backend-test-coverage` capability spec 通过 validate
- [x] `verilator-parity-all-tests` Phase 1 ship（macro + 5 base test + 3 专项 fixture + ci.yml
      `pr-verilator-fast`，commit `5f634f2`，archive `d6954ef`）
- [x] cache 双后端 e2e 通过（`tests/chlib/test_cache_sync.cpp`，2026-10-19 archive `aed33be`）
- [x] CTEST_TOTAL 同步 + `verify_test_counts.sh` 绿（**2026-10-23 实测 185**）

## 收口备注（2026-10-23）

Phase 4 的三条主题全部落地（cache 重建 ✅ / deprecation 标记 ✅ / dual-backend 基础设施 + Phase 1-3 ✅）。
**唯一延伸到 phase-5 的残余**是 dual-backend 的全量覆盖（Phase 4 RV32I / Phase 5 全量）—— 该残余
**不是** phase-4 的未完成项，而是 `feat-dual-backend-coverage` 按设计跨阶段（`phase_refs: [phase-4, phase-5]`）
继续推进的部分。故 phase-4 正常收口归档。

> ⚠️ 上表「dual-backend test」行标 🔄 是 **2026-10-19 的旧状态**；2026-10-23 复核：Phase 1-3 均已 archived。
