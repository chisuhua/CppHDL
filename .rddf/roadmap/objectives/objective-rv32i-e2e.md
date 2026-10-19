---
id: objective-rv32i-e2e
status: active
created: 2026-10-19
last_revised: 2026-10-19
review_by: 2027-01-17
owner: rdd-planner
priority: P0
manual_deps: []
supersedes: null
theme: RV32I 完整 e2e 在 interpreter + verilator + JIT 三路 trace parity 通过
---

# Objective: objective-rv32i-e2e

## 1. 驱动诊断（Why now）

RV32I 5-stage 流水线（`include/cpu/pipeline/rv32i_pipeline.h`）是 CppHDL 的旗舰设计，但当前只走到"编译期修复完成 + 3 条无 hazard 指令的 verilator trace parity"。真正的端到端集成（`examples/riscv-mini/` SOC，含 i_cache / d_cache / AXI 互连）尚未在任何后端跑通：`i_cache_prefetch.h` wrapper instantiate 触发 SIGSEGV（root AGENTS.md §KNOWN LIMITATIONS），且 cache 子系统的 verilator codegen 路径从未 e2e 验证。这个 objective 是 Phase 5 的旗舰目标——它是唯一能同时验证 dual-backend 基础设施、cache 子系统、RV32I 流水线三者的端到端场景。

适用场景：RV32I 顶层集成 + 三路 trace parity 验证；不适用场景：单模块单元测试（已有 dual-backend Phase 1 覆盖）。

## 2. 目标愿景 + 完成判据

- riscv-mini SOC 能在 interpreter / verilator / JIT 三后端跑同一指令序列产出相同 commit 序列
- RV32I 短指令 + load/store + branch 三类路径三路 parity 通过
- `i_cache_prefetch.h` / `d_cache_write_buffer.h` wrapper 无 SIGSEGV
- 完成判据（done-when）：
  - [ ] `examples/riscv-mini/src/i_cache_prefetch.h` 实例化 `ICacheSync` 通过（无 SIGSEGV）
  - [ ] riscv-mini SOC 三路 trace parity 通过（≥ 20 指令序列）
  - [ ] `dual-backend-rv32i-integration` change archived

## 3. 架构依据

- **ADR-035** — Verilator 后端整体设计（64-bit port cap / clock toggle / reset pulse）
- **ADR-039** — cache complete rewrite via ch_mem（RV32I L1 cache 依赖）
- **ADR-041** — cache Sync/Async variant（RV32I L1 用 Sync）
- `openspec/specs/verilator-rv32i-e2e/spec.md` — RV32I + Verilator e2e 契约
- `openspec/specs/dual-backend-test-coverage/spec.md` — dual-backend 契约（Phase 4 续接）
- root `AGENTS.md §KNOWN LIMITATIONS` — i_cache_prefetch SIGSEGV 记录

## 5. 反例

- 不要在 objective §6 写执行指令（违反 planner/builder 边界）
- 不要把 RV32I e2e 与 cache-stub 退役捆绑成单 change（修复与退役是两个独立关注点，违反 scope discipline）
- 不要假设 wrapper SIGSEGV 是 dual-backend 的 bug——它是 wrapper 内部 `ch_reg + C-style 数组` 架构 bug，必须先单独立项修

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
- `objective-test-coverage-100pct` — RV32I e2e 是 dual-backend 全量覆盖的子集，需先有 macro 基础设施
- `objective-cache-stub-retired` — cache stub 退役后 RV32I 才能用真实 cache（或反之，两者可并行但共享 cache 子系统）

### 9.2 Go / No-Go Decision Gate
- **Go 判据**（满足全部 3 条 → 转 completed）：
  - [ ] `i_cache_prefetch.h` wrapper SIGSEGV 修复（真实实例化 ICacheSync 通过）
  - [ ] riscv-mini SOC 三路 trace parity 通过（interpreter + verilator + JIT）
  - [ ] 1 个真实指令序列（含 load/store/branch）端到端验证
- **No-Go 判据**（任一成立 → defer）：
  - verilator codegen 对 RV32I 完整设计暴露无法在合理工期内修复的 bug
  - i_cache_prefetch wrapper 修复需要重写 riscv-mini 架构（超出本 objective scope）

### 9.3 跨 objective 影响
- 受此 objective 影响的下游：无（旗舰目标）
- 受上游 objective 影响：`objective-test-coverage-100pct`（macro 基础设施）+ `objective-cache-stub-retired`（cache 子系统）

## 10. next_sprint_candidates
- sprint-2026-10: 无（Phase 4 仍在 dual-backend Phase 1，RV32I 集成是 Phase 5 目标）
- sprint-2026-11+: `i_cache_prefetch.h` SIGSEGV 修复单独立项 → `dual-backend-rv32i-integration`

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建（roadmap 首次初始化） | 激活为 Phase 5 旗舰目标 | Phase 4 dual-backend 基础设施就绪后即可启动 |
