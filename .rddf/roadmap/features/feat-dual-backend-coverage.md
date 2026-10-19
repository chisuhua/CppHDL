---
id: feat-dual-backend-coverage
kind: feature
status: active
phase_refs: [phase-4, phase-5]
主题: 单元测试统一跑 interpreter + verilator 双后端（5 阶段推进）
---

## 概述

把 CppHDL 的单元测试从"仅 interpreter 路径"升级为"interpreter + verilator 双后端可验证"。核心动机：interpreter（`src/ast/instr_*.cpp::eval()`）与 verilator codegen（`src/codegen_verilog.cpp` → verilator → dlopen Vtop）是两套独立实现路径，shared-pass 测试仅覆盖 ~7% design surface。本 feature 通过 `DUAL_BACKEND_TEST` macro + `add_dual_backend_test` CMake function 让测试自动跑两遍并逐周期比对端口值。

capability spec 已建：`openspec/specs/dual-backend-test-coverage/spec.md`（16 requirements，`validate --specs --strict` 通过）。

| 子项 | change | 状态 |
|---|---|---|
| Phase 1: macro + 8-10 基础 test + 3 专项 fixture | `verilator-parity-all-tests` | ✅ Archived 2026-10-19 |
| Phase 2: cache 迁移 | `dual-backend-cache-tests` | ✅ Archived 2026-10-19 |
| Phase 3: chlib（bundle/stream/fifo/axi4） | `axi-spi-behavior-tests`（partial: SPI only） | 🟡 部分 ✅（2026-10-19；剩 I2c/Pwm/Uart/Timer/Gpio/Dma 6 个 peripherals） |
| Phase 4: RV32I integration | `dual-backend-rv32i-pipeline-behavior-tests`（Batch 1 only） | 🟡 部分 ✅（2026-10-19 commit bbf536d；Batch 2 IfIdPipelineReg clk-spike + Batch 3 BranchPredictor follow-up） |
| Phase 5: 全量覆盖 | `dual-backend-full-coverage` | ⏳ 依赖 cache-stub-physical-removal（ADR-037 deadline 2027-04-17） |

## 跨阶段拆分

### phase-4
- ✅ `dual-backend-test-coverage` capability spec 落地（2026-10-19）
- ✅ Phase 1 实施：`tests/dual_backend_fixture.h` macro + `add_dual_backend_test` CMake function + 5 base test 迁移 + 3 专项 fixture + ci.yml `pr-verilator-fast` job（commit 5f634f2, archive d6954ef）

### phase-5
- ✅ Phase 2: cache 双后端 Sync variant 9 dual pairs（test_cache_sync, archive 2026-10-19 commit aed33be）
- 🟡 Phase 3: chlib partial（SPI only；剩 6 peripherals follow-up）
- 🟡 Phase 4: RV32I partial（Batch 1 5 component classes 13 dual pairs archived；Batch 2/3 follow-up）
- ⏳ Phase 5: 全量覆盖 + 触发 cache-stub 退役

## 验收标准
- [x] capability spec 通过 `openspec validate --specs --strict`（16 requirements，2026-10-20）
- [x] Phase 1 ship（commit 5f634f2, archive d6954ef）
- [x] Phase 2 ship（commit aed33be, archive 2026-10-19）
- [x] Phase 3 partial ship（axi-spi-behavior 7 dual pairs, archive 2026-10-19）
- [x] Phase 4 partial ship（rv32i-pipeline-behavior 13 dual pairs, commit bbf536d, archive 2026-10-19）
- [ ] `ctest -L verilator_e2e` ≥ 100 test（全量目标，当前 21）
- [ ] 单元测试 verilator 覆盖 7% → 100%（Phase 3 续 + Phase 5）
- [x] `ctest -LE perf` 零回归（156/156 PASS）+ CTEST_TOTAL 167 同步