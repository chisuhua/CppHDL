---
id: feat-dual-backend-coverage
kind: feature
status: active
phase_refs: [phase-4, phase-5]
主题: 单元测试统一跑 interpreter + verilator 双后端（5 阶段推进）
---

## 概述

把 CppHDL 的单元测试从"仅 interpreter 路径"升级为"interpreter + verilator 双后端可验证"。核心动机：interpreter（`src/ast/instr_*.cpp::eval()`）与 verilator codegen（`src/codegen_verilog.cpp` → verilator → dlopen Vtop）是两套独立实现路径，shared-pass 测试仅覆盖 ~7% design surface。本 feature 通过 `DUAL_BACKEND_TEST` macro + `add_dual_backend_test` CMake function 让测试自动跑两遍并逐周期比对端口值。

capability spec 已建：`openspec/specs/dual-backend-test-coverage/spec.md`（13 requirements，`validate --specs --strict` 通过）。

| 子项 | change | 状态 |
|---|---|---|
| Phase 1: macro + 8-10 基础 test + 3 专项 fixture | `verilator-parity-all-tests`（proposal/design 就绪） | ⏳ Phase 4 |
| Phase 2: cache 迁移 | `dual-backend-cache-tests` | ⏳ Phase 5 |
| Phase 3: chlib（bundle/stream/fifo/axi4） | `dual-backend-chlib-tests` | ⏳ Phase 5 |
| Phase 4: RV32I integration | `dual-backend-rv32i-integration` | ⏳ Phase 5 |
| Phase 5: 全量覆盖 | `dual-backend-full-coverage` | ⏳ Phase 5 |

## 跨阶段拆分

### phase-4
- ✅ `dual-backend-test-coverage` capability spec 落地（2026-10-19）
- ✅ `verilator-parity-all-tests` proposal.md + design.md + specs/ delta 就绪
- ⏳ Phase 1 实施：`tests/dual_backend_fixture.h` macro + `add_dual_backend_test` CMake function + 8-10 基础 test 迁移 + 3 专项 fixture + ci.yml `pr-verilator-fast` job

### phase-5
- ⏳ Phase 2: cache 双后端（Sync parity / Async golden-value）
- ⏳ Phase 3: chlib ~50 test 迁移
- ⏳ Phase 4: RV32I integration（阻塞于 i_cache_prefetch SIGSEGV）
- ⏳ Phase 5: 全量覆盖 + 触发 cache-stub 退役

## 验收标准

- [x] capability spec 通过 `openspec validate --specs --strict`
- [ ] Phase 1（macro + 8-10 test + 3 fixture + ci.yml）ship
- [ ] `ctest -L verilator_e2e` ≥ 100 test（全量目标）
- [ ] 单元测试 verilator 覆盖 7% → 100%
- [ ] `ctest -LE perf` 零回归 + CTEST_TOTAL 同步
