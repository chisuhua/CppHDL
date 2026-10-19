---
id: feat-rv32i-integration
kind: feature
status: active
phase_refs: [phase-5]
主题: RV32I 5-stage 流水线 + riscv-mini SOC 端到端集成（三路 trace parity）
---

## 概述

把 CppHDL 的 RV32I 5-stage 流水线（`include/cpu/pipeline/rv32i_pipeline.h`）与 riscv-mini SOC（`examples/riscv-mini/`）推进到端到端可用：interpreter + verilator + JIT 三路 trace parity 通过。当前 `rv32i_pipeline` 编译期修复完成，但 runtime 依赖 `ch_device` wrapper；`i_cache_prefetch.h` wrapper instantiate 触发 SIGSEGV（root AGENTS.md §KNOWN LIMITATIONS）—— 2026-10-20 已修复（commit b987bb7 fix-riscv-mini-cache-prefetch-sigsegv，3 PASS instantiate）。

| 子项 | 状态 |
|---|---|
| RV32I pipeline 编译期修复 | ✅ 已实施 |
| RV32I 短指令 trace parity（`test_verilator_complex_e2e`） | ✅ 已实施（3 条无 hazard 指令，commit 8facced） |
| `i_cache_prefetch.h` wrapper SIGSEGV 修复 | ✅ 已实施（2026-10-19 commit b987bb7 fix-riscv-mini-cache-prefetch-sigsegv） |
| RV32I microarchitecture dual-backend behavior parity（Batch 1） | ✅ 已实施（2026-10-19 commit bbf536d，13 dual pairs 5 component classes） |
| riscv-mini SOC 完整 e2e | ⏳ Phase 5 |

## 跨阶段拆分

### phase-5
- ✅ 单独立项修复 `examples/riscv-mini/src/i_cache_prefetch.h` wrapper SIGSEGV（`ch_reg + C-style 数组` → `ch_mem` + `ch::ch_module<ICacheComplete>` 替换 `Component ctor + parent`，archive 2026-10-19 commit b987bb7）
- ✅ `i_cache_prefetch.h` 实例化 `ICacheSync`（非 stub，3 PASS instantiate）
- 🟡 `dual-backend-rv32i-integration` partial（Batch 1 5 component classes 13 dual pairs archived；Batch 2 IfIdPipelineReg ×2 clk-spike + Batch 3 BranchPredictor ×1 follow-up）
- ⏳ riscv-mini SOC 端到端三路 trace parity（依赖 Batch 2/3）

## 验收标准
- [x] RV32I pipeline 编译期修复
- [x] RV32I 3 条无 hazard 指令 trace parity（commit 8facced）
- [x] `i_cache_prefetch.h` wrapper SIGSEGV 修复（commit b987bb7）
- [x] RV32I microarchitecture dual-backend behavior parity partial（commit bbf536d）
- [ ] riscv-mini SOC 端到端三路（interpreter + verilator + JIT）trace parity