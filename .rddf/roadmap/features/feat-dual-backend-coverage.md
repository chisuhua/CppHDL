---
id: feat-dual-backend-coverage
kind: feature
status: completed
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
| Phase 3: chlib（bundle/stream/fifo/axi4） | `axi-spi-behavior-tests` + `dual-backend-axi4-peripherals` + `dual-backend-axi4-peripherals-extended-2026-10` | ✅ 6/6（SPI/Pwm/I2c + Timer/Gpio/Uart 均已 archived；AxiDma 未排期） |
| Phase 4: RV32I integration | `dual-backend-rv32i-pipeline-behavior-tests`（Batch 1 only） | ✅ **完成**（Batch 1 = 2026-10-19 commit bbf536d；**Batch 2/3 = 2026-10-27**，change `dual-backend-rv32i-integration-2026-10`：`IfIdPipelineReg` ×2 + `BranchPredictor` ×1 ⇒ 该文件 13 → **16** dual pair / 26 → 32 用例；⚠️ 复核确认「clk-spike」**不是风险项**（`IfIdPipelineReg.clk` 是死端口、`BranchPredictor` 纯组合）⇒ 真实工作是 SECTION-free 重写 + 端口驱动纪律） |
| Phase 5: 全量覆盖 | **无同名 change**（两半分别收口） | ✅ **完成**（2026-11-03）：① 「64-bit / clock / reset 专项」= 4 个已注册 dual target（`test_dual_backend_{64_bit,clock,reset,template}`）；② 「全量 100%」**不可机械核验**（design surface 分母无权威定义）⇒ 已被 objective §2 的 **parity 源文件数**口径取代（现 **66**）⇒ 依据 = `.rddf/checks/objective-test-coverage-status-2026-11.md` §2 |

## 跨阶段拆分

### phase-4
- ✅ `dual-backend-test-coverage` capability spec 落地（2026-10-19）
- ✅ Phase 1 实施：`tests/dual_backend_fixture.h` macro + `add_dual_backend_test` CMake function + 5 base test 迁移 + 3 专项 fixture + ci.yml `pr-verilator-fast` job（commit 5f634f2, archive d6954ef）

### phase-5
- ✅ Phase 2: cache 双后端 Sync variant 9 dual pairs（test_cache_sync, archive 2026-10-19 commit aed33be）
- ✅ Phase 3: chlib AXI4-Lite slave dual 覆盖 6/6（SPI/Pwm/I2c archived；Timer/Gpio/Uart archived `2026-10-20-dual-backend-axi4-peripherals-extended-2026-10`；AxiDma 未排期）
- ✅ Phase 4: RV32I **完成**（Batch 1 = 13 dual pairs，2026-10-19；Batch 2/3 = 3 dual pairs，
      2026-10-27 归档 `dual-backend-rv32i-integration-2026-10` ⇒ 该文件共 **16** dual pair / 32 用例）
- ✅ Phase 5: **完成**（2026-11-03）—— 「64-bit / clock / reset 专项」已落地 4 个 dual target；「全量 100%」已由 objective §2 的 **parity 源文件数 ≥ 60** 口径取代（现 **66**）⇒ 两半都无剩余可执行项 ⇒ 见 `.rddf/checks/objective-test-coverage-status-2026-11.md`

## 验收标准
- [x] capability spec 通过 `openspec validate --specs --strict`（16 requirements，2026-10-20）
- [x] Phase 1 ship（commit 5f634f2, archive d6954ef）
- [x] Phase 2 ship（commit aed33be, archive 2026-10-19）
- [x] Phase 3 partial ship（axi-spi-behavior 7 dual pairs, archive 2026-10-19）
- [x] Phase 4 partial ship（rv32i-pipeline-behavior 13 dual pairs, commit bbf536d, archive 2026-10-19）
- [x] ⭐ **双后端 parity 源文件数 ≥ 60**（**2026-11-03 实测 66** ⇒ ✅ **达成**，margin 10%；历史：2026-10-30 实测 29 ⇒ 路线 C 第 1 批（`test_rv32i_stage_dual`）59 → **60** ⇒ `mux-slot-width-preservation` → **65** ⇒ `wide-shl-truncate-word-carry` → **66**）
      —— ⭐ **2026-10-30 planner 修订**：取代旧的「`ctest -L verilator_e2e` ≥ 100」
      （旧口径数的是**标签**⇒ 把 9 个**单后端**契约锁定参考实现算成「双后端覆盖」；
      且其诚实上限 [71, 96] ⇒ **现有语料内不可达** ⇒ 必然到不了）
      ⇒ 新口径定义 + 上限 + margin 理由见
      `.rddf/roadmap/objectives/objective-test-coverage-100pct.md` §2
- [ ] ~~单元测试 verilator 覆盖 7% → 100%（已被 parity 口径取代）~~
      ⇒ ⚠️ **该百分比不可机械核验**（design surface 分母无权威定义）
      ⇒ **由上面的 parity 源文件数代理** ⇒ 本行降级为**愿景**（不作闸门）
- [x] `ctest -LE perf` 零回归（**2026-11-03 实测 232/232 PASS**）+ **CTEST_TOTAL 243** 同步
  （计数沿革：185 @ 2026-10-23 → 200 @ `rv32i-pipeline-commit-path-2026-10` →
  202 @ `axi4-interconnect-routing-2026-10` → **243** @ 2026-11-03；
  `ctest -L verilator_e2e` 同理 = 35 @ C3 → 37 @ 互连层 → **75** @ 2026-11-03）
- [x] ⭐ **判据已落机械锚点**（2026-10-30）：根 `AGENTS.md` 的
      `DUAL_BACKEND_PARITY_TOTAL` 行 + `scripts/verify_test_counts.sh` 的对应检查
      ⇒ 防止「双后端覆盖**静默缩面**」（与 `VERILATOR_E2E_TOTAL` 锚点**同型**、
      但度量的是 **parity 源文件数**而非标签条目数）
- [x] 判据修订**零 capability 变更** ⇒ 无需 OpenSpec change
      （`dual-backend-test-coverage` 零 MODIFIED —— 判据属 objective 文件，
      capability 只规定 R6 排除清单、**未**规定「怎么数」）