---
id: feat-rv32i-integration
kind: feature
status: active
phase_refs: [phase-5]
主题: RV32I 5-stage 流水线 + riscv-mini SOC 端到端集成（双路 parity + JIT 回落断言）
---

## 概述

把 CppHDL 的 RV32I 5-stage 流水线（`include/cpu/pipeline/rv32i_pipeline.h`）与 riscv-mini SOC
（`examples/riscv-mini/`）推进到端到端可用。

> ⚠️ **2026-10-23 定义修订**：原目标「interpreter + verilator + JIT 三路 trace parity」**不可能原生成立** ——
> 实测 `src/jit/jit_ir_gen_lnode_state.cpp:58-60` 对 `type_mem_read_port` 返回 `UNSUPPORTED_OP`，而 RV32I 必有
> imem（`ch_mem` 读端口）⇒ 整个设计 JIT 必然回落 interpreter（`jit_compiler.cpp:256-264` 注释即为此设计）。
> 故目标改为「**双路 parity（interpreter + verilator）+ JIT 回落行为显式断言**」；JIT-native 三路作为
> 独立 objective `jit-mem-read-port-support` 排期（非本 feature 范围）。

> ⚠️ **2026-10-23 收敛前提**：本 feature 的 e2e 目标**依赖** `feat-riscv-mini-convergence`
> （当前 `include/cpu/pipeline/` 与 `examples/riscv-mini/src/` 是**两套不同架构**，各自被不同测试覆盖 ⇒
> 没有「单一 SoC」可供 parity）。收敛必须先落地。

| 子项 | 状态 |
|---|---|
| RV32I pipeline 编译期修复 | ✅ 已实施 |
| RV32I 短指令 trace parity（`test_verilator_complex_e2e`，3 条无 hazard 指令） | ✅ 已实施（commit `8facced`） |
| `i_cache_prefetch.h` wrapper SIGSEGV 修复 | ✅ 已实施（2026-10-20 change `fix-riscv-mini-cache-prefetch-sigsegv`，archive `2026-10-19-fix-riscv-mini-cache-prefetch-sigsegv`） |
| RV32I microarchitecture dual-backend behavior parity（Batch 1，13 dual pairs / 5 component classes） | ✅ 已实施（2026-10-19 commit `bbf536d`） |
| RV32I 单源化收敛（`feat-riscv-mini-convergence`） | ✅ 已归档（C2，2026-10-25） |
| **RV32I 流水线 commit/valid 通路修复**（change `rv32i-pipeline-commit-path-2026-10`） | ✅ **已归档**（2026-10-26）—— 观测面（**32 端口**）+ **17 个流水线缺陷**（含 load-use 多拍停顿 A1-a + flush 多杀一拍） |
| riscv-mini SOC 双路 parity + JIT 回落断言 | ✅ **已归档**（2026-10-26，change `rv32i-soc-dual-parity-2026-10`）—— 200 拍 × **27** 个输出观测值**逐拍全等**（无掩码）+ 3 个 store 的 ISA golden + 写回序列断言 + **JIT 回落契约**（原因可归因：`unsupported node width: mem (…, 131072 bits > 64)`） |

## 跨阶段拆分

### phase-5
- ✅ 单独立项修复 `examples/riscv-mini/src/i_cache_prefetch.h` wrapper SIGSEGV（`ch_reg + C-style 数组`
      → `ch_mem` + `ch::ch_module<ICacheSync>` 替换 `Component ctor + parent`；change
      `fix-riscv-mini-cache-prefetch-sigsegv`，2026-10-20 archive）
- ✅ `i_cache_prefetch.h` 实例化 `ICacheSync`（非 stub，3 PASS instantiate）
- 🟡 `dual-backend-rv32i-integration` partial（Batch 1 5 component classes 13 dual pairs archived；
      Batch 2 `IfIdPipelineReg` ×2 clk-spike + Batch 3 `BranchPredictor` ×1 follow-up）
- ⏳ **前置**：`feat-riscv-mini-convergence`（C2）—— 收敛成单一 SoC 实现
- ⏳ riscv-mini SOC **双路** trace parity（interpreter + verilator，≥20 指令含 load/store/branch）
- ⏳ JIT 回落行为显式断言（`UNSUPPORTED_OP` 检测 + 回落结果 == interpreter）

## 验收标准
- [x] RV32I pipeline 编译期修复
- [x] RV32I 3 条无 hazard 指令 trace parity（commit `8facced`）
- [x] `i_cache_prefetch.h` wrapper SIGSEGV 修复（2026-10-20 change `fix-riscv-mini-cache-prefetch-sigsegv`）
- [x] RV32I microarchitecture dual-backend behavior parity partial（commit `bbf536d`）
- [x] riscv-mini SOC **双路**（interpreter + verilator）trace parity 通过（≥20 指令序列）— 2026-10-26 C3 归档（200 拍 × **27** 个输出观测值逐拍全等，无掩码）
- [x] JIT 回落行为显式断言通过（含 `ch_mem` 读端口的设计 → JIT 回落 → 回落结果与 interpreter 一致）— 2026-10-26 C3 的 T6（归因实测：`unsupported node width: mem (…, 131072 bits > 64)`）
- [ ] `dual-backend-rv32i-integration` change archived
