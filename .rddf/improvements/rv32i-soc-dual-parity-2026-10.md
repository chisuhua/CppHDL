---
name: rv32i-soc-dual-parity-2026-10
priority: P0
source: objective-rv32i-e2e §10（2026-10-23 roadmap 重梳理）
phase: phase-5
category: testing
type: feature
created_at: 2026-10-23
---

# Improvement: riscv-mini SoC 双路 parity（interpreter + verilator）+ JIT 回落行为显式断言

- **Date drafted**: 2026-10-23
- **Drafted by**: build-agent (planner role)
- **Status**: 📋 Handoff document — 5-segment draft，等待 rdd-builder 启动
- **Objective**: `objective-rv32i-e2e`（C3）
- **强前置**：C1 `riscv-mini-test-triage-2026-10` + C2 `rv32i-canonical-convergence-2026-10` archive

## 1. Why (背景与动机)

`objective-rv32i-e2e` 的旗舰目标。当前 RV32I 只走到「3 条无 hazard 指令的 verilator trace parity」
（`tests/test_verilator_complex_e2e.cpp`）。真正的 SoC 级 parity 未跑通。2026-10-23 定义修订
（用户裁决）：

> 「三路 parity」不可能原生成立 —— `src/jit/jit_ir_gen_lnode_state.cpp:58-60` 对 `type_mem_read_port`
> 返回 `UNSUPPORTED_OP`，RV32I 必有 imem ⇒ JIT 必然整体回落 interpreter。故目标改为
> **「双路 parity（interpreter + verilator）+ JIT 回落行为显式断言」**。

**Why now**：C1 建立了绿色基线、C2 收敛出单一实现 ⇒ 本 change 是 e2e parity 的收口。真实 SoC
（pipeline + TCM + cache wrapper）从未在双后端下逐周期比对过；cache codegen 的 verilator 路径从未 e2e。

## 2. What Changes (变更范围)

### C3-a SoC 级 DUT（单一实现）
- DUT = `Rv32iPipelineTop`（pipeline + `InstrTCM` + `DataTCM`；post-C2 单源化后的唯一实现）。
- 固件：≥20 指令序列，**覆盖三类路径**：ALU-only（已有）+ load/store + branch（新增）。
- imem MUST 用 `sread`（保 `pc_lag` 对齐，verilator-e2e-coverage §R1 invariant 3）。

### C3-b 双路 trace parity
- 用 `DUAL_BACKEND_TEST_PAIR`（`tests/dual_backend_fixture.h`）跑 interpreter + verilator 两后端，
  逐周期比对顶层 `ch_out`（观测点 MUST 走顶层端口，verilator 不能看深层内部 —— 与
  `tests/test_stream_fifo_dual.cpp` DUT 同约束）。
- 断言 commit 序列 / 关键寄存器在每个 tick 一致。

### C3-c JIT 回落行为显式断言
- 对同一 DUT `set_jit_enabled(true)`，断言：
  ① JIT 编译返回 `UNSUPPORTED_OP`（因 `type_mem_read_port`）；
  ② Simulator 自动回落 interpreter；
  ③ 回落后的 trace 与显式 `set_jit_enabled(false)` 的 interpreter 结果逐周期一致。
- 这把「第三路」从隐式（悄悄回落）变成**显式契约**：parity 的观测面是「回落行为正确」而非「JIT 生成了 RV32I」。

### C3-d 新 capability / spec delta
- 新增 capability `rv32i-soc-parity`（或并入 `verilator-rv32i-e2e` MODIFIED）：双路 parity + JIT 回落断言。
- `dual-backend-test-coverage` MODIFIED（RV32I SoC 级 parity 计入覆盖矩阵）。

## 3. Acceptance (验收标准)

### 3.1 编译 + 测试 (PHASE GATES 🟢)
- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] 新 parity test 双后端全绿（interpreter + verilator 各 ≥1 TEST_CASE）
- [ ] `ctest -LE perf` 零回归 + CTEST_TOTAL 同步

### 3.2 双路 parity 核心验收
- [ ] ≥20 指令序列（含 load/store/branch）双后端逐周期 commit 序列一致
- [ ] DUT 观测点全走顶层 `ch_out`（verilator 可观测）
- [ ] imem 用 `sread`（非 `aread`）

### 3.3 JIT 回落断言
- [ ] 断言 ①`UNSUPPORTED_OP` 检测 + ②回落 + ③回落结果 == interpreter 三件套全过
- [ ] 日志中「JIT skipped」从隐性变显式（测试断言它出现）

### 3.4 文档同步
- [ ] `objective-rv32i-e2e` §2 done-when 勾选
- [ ] root `AGENTS.md` KNOWN LIMITATIONS 的 JIT 回落条目更新（不再是「将来修」，是「已断言的契约」）

## 4. Capabilities (涉及的能力维度)

| Capability | 影响 | 文档 |
|------------|------|------|
| `verilator-rv32i-e2e` | SoC 级双路 parity + sread 契约 | MODIFIED（加双路 + JIT 回落断言 requirement）|
| `dual-backend-test-coverage` | RV32I SoC 级计入覆盖矩阵 | MODIFIED |
| （新）`rv32i-soc-parity` | 若独立 capability | ADDED |

## 5. Impact (影响面分析)

### 5.1 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| SoC 级 verilator codegen 暴露缓存/AXI 路径新 bug | 🟡 中 | 从 pipeline+TCM（无 cache）起步，cache 集成是 follow-up |
| `Rv32iPipelineTop` 收敛后需重写（C2 改动波及） | 🟡 中 | C2 已冻结 SoC 顶层接口；本 change 只加观测点 |
| ≥20 指令序列构造（含 load/store/branch 的手工编码） | 🟢 低 | 复用 `firmware_loader` 模式或 `test_verilator_complex_e2e` 的 imem 初始化 |
| JIT 回落断言依赖 `JitResult` 可观测 | 🟢 低 | `jit_compiler.cpp:256-264` 已返回 UNSUPPORTED_OP；fixture 需暴露 |

### 5.2 明确不做（Non-Goals）
- 不实现 JIT 的 mem-read lowering（独立 objective `jit-mem-read-port-support`）
- 不做 AXI 互连的 SoC 级 parity（`axi4-interconnect-routing-2026-10` 的事）
- 不做 cache 的 SoC 级 parity（follow-up）
