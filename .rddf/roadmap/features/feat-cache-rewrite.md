---
id: feat-cache-rewrite
kind: feature
status: active
phase_refs: [phase-4, phase-5]
主题: cache 子系统从 C-style 数组重写为 ch_mem（ADR-039）+ Sync/Async 双变体（ADR-041）+ stub 物理退役（ADR-037）
---

## 概述

CppHDL 的 L1 cache 子系统经历了一次死而复生的重建：原 `*_complete.h` 用 C-style `ch_reg<...>[N]` 数组 + 运行时 `ch_uint<7>` 索引，导致 11 个 pre-existing compile error（dead-on-arrival）。撞墙事件后按 ADR-039 重写为 `ch_mem`-based，并按 ADR-041 拆分为 Sync（1-cycle sread）/ Async（0-cycle aread）双变体。本 feature 串联三个 ADR 与 stub 物理退役。

| 子项 | ADR | 状态 |
|---|---|---|
| `*_complete.h` ch_mem 重写 | ADR-039 | ✅ 已实施（commit `91a0833`） |
| Sync/Async variant split | ADR-041 | ✅ 已实施（commit `49902b2`） |
| cache 行为测试 9 test_case | — | ✅ 已实施（commit `1221819` + `bb286a2`） |
| cache 双后端 e2e | dual-backend-test-coverage | ⏳ Phase 5 |
| stub 物理删除 | ADR-037 | ⏳ Phase 5（deadline 2027-04-17） |

## 跨阶段拆分

### phase-4
- ✅ ADR-039 ch_mem 重写（`include/cpu/cache/{i,d}_cache_complete.h`）
- ✅ ADR-041 Sync/Async 拆分（`{i,d}_cache_{sync,async}.h` + `_complete.h` 兼容 shim）
- ✅ `tests/chlib/test_cache_complete.cpp` 9 test_case / 53 assertions
- ⏳ cache 双后端 e2e（`dual-backend-cache-tests`，Phase 2 of dual-backend）

### phase-5
- ⏳ `dual-backend-cache-tests` 落地（Sync 走 parity / Async 走 golden-value）
- ✅ cache-stub 物理删除（`git rm include/cpu/cache/{i,d}_cache_stub.h`，2026-10-20，variant b）
- ✅ 重写 `test_cache_pipeline_integration.cpp`（脱离 stub，5 TEST_CASE 全改 `ICacheSync` / `DCacheSync` 真实时序）
- ✅ 3 个 stub 消费者改而非删（`test_cache_simple.cpp` / `test_phase4_integration.cpp` 保留 smoke 锚点，计数不变）

## 验收标准

- [x] ADR-039 ch_mem 重写完成（含 R2 spike + R3 hard-gate）
- [x] ADR-041 Sync/Async variant split 完成
- [x] cache 行为测试全绿
- [ ] cache 双后端 e2e 通过（Sync parity + Async golden-value）
- [ ] cache stub 物理删除（ADR-037，deadline 2027-04-17）
