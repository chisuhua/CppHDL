---
id: feat-riscv-mini-rtl-debt
kind: feature
status: active
phase_refs: [phase-5]
主题: riscv-mini 实测挖出的两个 RTL 缺陷收口（cache wrapper 分支从不发射 · CLINT mtime 不自增）
---

## 概述

`riscv-mini-test-triage-2026-10`（C1）把 riscv-mini 的测试从「不可证伪」改成「可证伪」时，
实测挖出**两个真实 RTL 缺口**。C1 的职责是测试整治 ⇒ 当时拆成两份 improvement 草案，
但 **roadmap 一直没给它们归属**（2026-10-26 审计发现：`.rddf/roadmap/**` 零提及）。
本 feature 是它们的归属地。

| 缺陷 | improvement 草案 | 2026-10-26 复核 |
|---|---|---|
| 两个 cache wrapper 的分支逻辑**从不发射** | `.rddf/improvements/riscv-mini-cache-wrapper-dead-branch-2026-10.md`（P2） | ✅ 仍在：`include/cpu/cache/d_cache_write_buffer.h:97` 的 `if (io().req == ch_bool(true) && io().we == ch_bool(true))` 在建图期求值 ⇒ 分支体内 `io().ready <<=` **不发射**；`i_cache_prefetch.h` 同类 |
| CLINT `mtime` 不自增 | `.rddf/improvements/clint-mtime-autoincrement-2026-10.md`（P2） | ✅ 仍在：`include/cpu/riscv/clint.h` 的 `mtime` 只能由软件经写端口写入 ⇒ 只构成静态比较器 |

> 两者均为 **P2**（不阻塞 parity / ISA 主线），因此排在新 feature `feat-rv32i-isa-completeness` 之后。

## 排期时必须知道的两件事

1. **路径已因 C2 变化**：wrapper 已从 `examples/riscv-mini/src/` 迁到 `include/cpu/cache/`
   （improvement 草案写的旧路径已订正）。
2. **修好会转红两个既有断言**：`examples/riscv-mini/tests/test_cache_smoke.cpp:82-83` 当前
   **钉住缺陷现状**（`ready == false` / `axi_valid == false`，注释里明说「缺陷修好后会转红」）
   ⇒ 该 change MUST 同步改成**真实行为断言**，不得靠删除断言了事。

## 候选 change 分解

| # | 候选 change | 范围 | 前置 | 验收 |
|---|---|---|---|---|
| ① | `riscv-mini-cache-wrapper-live-logic-2026-10` | 用 `select()` / `chlib/if_stmt.h` 的 `_if()` 替换建图期 C++ `if`；`ICachePrefetch` 把 `addr`/`req` 真正接到内层 cache；`DCacheWriteBuffer` 的 buffer 换节点数组（ADR-039 口径） | C2 ✅ 已归档 | `req && we ⇒ ready==1`、`axi_ready ⇒ axi_valid==1`、`axi_addr`/`axi_wdata` 反映被写数据；`test_cache_smoke` 无「缺陷快照」断言 |
| ② | `clint-mtime-autoincrement-2026-10` | `CLINT::tick()` 递增 `mtime`（或按 `mtimecmp` 语义补自由运行计数），恢复文件头注释所述契约 | 无 | 连续 N 拍后 `mtime == N`；写端口仍可覆盖；`test_interrupt_flow` 仍绿 |

## 反例

- **不要**为了「不转红」而保留「未驱动 ⇒ 读 0」的断言 —— 那是**把缺陷写进规格**。
- **不要**把 cache wrapper 的修复扩大到 cache 微架构（scope 只到 wrapper 层的信号发射与接线）。
- **不要**给 CLINT 加「自动递增」却同时保留「软件任意写」而不写清优先级 —— 会得到
  一个两道驱动源的静默冲突；MUST 在 change 里明确「写端口是覆盖还是重载」。

## 验收标准

- [ ] `include/cpu/cache/{i_cache_prefetch,d_cache_write_buffer}.h` 内**零**建图期 C++ `if`
      控制 RTL 发射（grep 可查：`if (io()` 形态）
- [ ] `test_cache_smoke` 的断言全部为可证伪的行为断言（变异即转红）
- [ ] CLINT `mtime` 随时间递增且写端口语义明确
