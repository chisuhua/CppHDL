---
id: phase-1
kind: phase
status: done
phase_refs: []
主题: [核心抽象与算子基础设施]
---

## phase-1 概览

Phase 1 覆盖 CppHDL 的**核心抽象层**：`ch_bool` / `ch_uint<N>` 类型系统、算子聚合（operators.h 拆分前身）、`context` / `ctx_swap` 生命周期、`lnode` 逻辑节点、IO 端口（`ch_in` / `ch_out`）与解释器求值路径。这些是后续所有 Phase 的地基，已全部落地并稳定。

| Theme | 覆盖 | 状态 |
|-------|------|------|
| 核心类型与算子 | `include/core/operators*.h`, `include/core/io*.h` | ✅ 已实施 |
| Context 与求值 | `include/core/context.h`, `src/simulator.cpp` | ✅ 已实施 |
| 逻辑节点 | `include/lnode/`, `src/core/lnodeimpl*` | ✅ 已实施 |

## 已实施能力

- **核心类型系统** — `ch_bool` / `ch_uint<N>` / `ch_int<N>`，SpinalHDL 命名对齐（ADR-013）
- **算子聚合拆分** — `operators.h` 933 行 → 5 个 per-category 文件（`operators_{width,arith,logic,compare,shift}.h`），C-class 重构 Phase 3
- **IO 端口双 API** — `ch_in<T>` / `ch_out<T>` + `<<=` 语义（ADR-012），io.h 拆为 4 个 per-category 文件
- **仿真求值顺序** — `eval_combinational → eval → eval_combinational` 3-eval 模型（ADR-009）
- **连接语义** — `<<=` 单驱动约束（ADR-010）
- **拓扑排序单一 IR** — ADR-015
- **命名空间隔离** — ADR-033

## 架构文档锚点

| 文档 | 与本 phase 关联 |
|------|----------------|
| `docs/adr/ADR-009-simulation-evaluation-order.md` | 3-eval 求值顺序 |
| `docs/adr/ADR-010-connection-semantics.md` | `<<=` 连接语义 |
| `docs/adr/ADR-012-io-port-dual-api.md` | IO 端口双 API |
| `docs/adr/ADR-013-type-hierarchy-design.md` | 类型层次 |
| `docs/adr/ADR-015-topological-sort-single-ir.md` | 拓扑排序 |

## 完成条件

- [x] 核心类型 + 算子拆分落地
- [x] Context / Simulator / 解释器路径稳定
- [x] 基础测试覆盖（`test_basic` / `test_operator*` / `test_arithmetic` 等）
- [x] ADR-009/010/012/013/015 全部 Accepted
