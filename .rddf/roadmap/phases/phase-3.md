---
id: phase-3
kind: phase
status: done
phase_refs: []
主题: [Verilator 后端集成, Verilog codegen 完整化, 性能对比框架]
---

## phase-3 概览

Phase 3 覆盖 **Verilator 仿真后端**（ADR-035）：`src/codegen_verilog.cpp` 生成 SystemVerilog → verilator 编译 → dlopen Vtop → QData accessor 驱动的三路对比框架（interpreter / JIT / Verilator）。本阶段把 CppHDL 从"仅 C++ 仿真"升级为"可直接对接工业级 Verilator 验证流"。已 GA（v2.1 e2e 验证通过）。

| Theme | 覆盖 | 状态 |
|-------|------|------|
| Verilator 后端 | `include/core/verilator_backend.h`, `src/core/verilator_backend.cpp` | ✅ 已实施 |
| Verilog codegen | `src/codegen_verilog.cpp` | ✅ 已实施 |
| 性能对比框架 | `tests/benchmark/perf_main.cpp` | ✅ 已实施 |

## 已实施能力

- **Verilator backend 整体设计** — dlopen / 7-symbol 解析 / SHA-1 cache / 3-eval clock toggle / port access table（ADR-035）
- **codegen 字面量 mask** — `get_literal_str` 按目标宽度 mask，修复 "Too many digits" verilator 语法错误
- **高阶 verilator e2e 覆盖** — 9 个专项测试 × 11 fixture（counter / AXI4-Lite / bundle / stream / sync_fifo / axi4_full / rv32i_pipeline / spi_fsm）
- **三路 perf 对比** — interpreter / JIT / Verilator，subprocess 隔离（`perf-test-isolation` spec）
- **verilator-harness-absolute 修复** — 子进程 cwd 继承问题（perf-test-isolation spec §R1-3）
- **64-bit port cap** — QData/UData accessor 物理上限，超宽端口 `CHERROR`
- **hierarchical input driver** — 子模块 `ch_in` 展平修复（verilator-hier-input spec）
- **default_reset pulse** — initialize 时 pulse reset 加载 INIT（verilator-reg-init spec）

## 架构文档锚点

| 文档 | 与本 phase 关联 |
|------|----------------|
| `docs/adr/ADR-035-verilator-backend.md` | Verilator 后端整体设计 |
| `docs/developer_guide/verilator-integration.md` | 安装与 PATH 集成 |
| `openspec/specs/verilator-backend-e2e/spec.md` | dlopen / cache / 3-eval 契约 |
| `openspec/specs/verilator-e2e-coverage/spec.md` | 9 个专项 e2e 测试契约 |
| `openspec/specs/perf-test-isolation/spec.md` | 三路 perf 隔离契约 |

## 完成条件

- [x] Verilator backend Phase 1-4.1 GA
- [x] v2.1 e2e 验证通过（2026-09-21）
- [x] 9 个专项 verilator e2e 测试落地
- [x] 三路 perf 对比框架成熟（TC-07/08 verilator PASS）
- [x] ADR-035 Accepted
