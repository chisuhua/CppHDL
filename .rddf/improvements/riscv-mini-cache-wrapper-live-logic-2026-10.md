---
name: riscv-mini-cache-wrapper-live-logic-2026-10
priority: P2
source: riscv-mini-test-triage-2026-10（实测挖出的真实 RTL 缺口）
phase: phase-5
category: cpu
type: bugfix
created_at: 2026-10-24
---

# Improvement: riscv-mini 的两个 cache wrapper 必须真正发射分支逻辑

- **Date drafted**: 2026-10-24
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，已归属 feature `feat-riscv-mini-rtl-debt` ①（2026-10-26 roadmap 审计）；前置 C2 已满足 ⇒ 可立项
  ⚠️ **2026-10-27 改名**：草案原名 `...-dead-branch-2026-10` ⇒ 统一为 `...-live-logic-2026-10`
  （与 `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` 的 T8 命名一致）
  ⇒ **已立项**：`openspec/changes/riscv-mini-cache-wrapper-live-logic-2026-10/`
- **来源**: change `riscv-mini-test-triage-2026-10` 实测挖出（C1 把
  `test_cache_smoke` 的断言从「不可证伪」改成「可证伪」时暴露），
  **原登记在该 change 的「框架缺陷」节** ⇒ 本 draft 承接之，C1 归档时移除该条。

## 1. Why (背景与动机)

`include/cpu/cache/{i_cache_prefetch,d_cache_write_buffer}.h` 用**普通 C++ `if`**
包裹 `ch_bool` 条件来发射 RTL（⚠️ C2 收敛前这两个文件位于 `examples/riscv-mini/src/`，
2026-10-26 已把路径订正为新位置）：

```cpp
if (io().req == ch_bool(true) && io().we == ch_bool(true)) {
    io().ready <<= ch_bool(true);      // ← 这行**从不发射**
}
```

`ch_bool` 有 `explicit operator bool()` ⇒ C++ `if` 拿到的是「**当前节点是否非零**」，
而 `describe()` 是在**建图期**执行的 ⇒ 该条件在建图期为**假** ⇒ **分支内的
`io().xxx <<=` 永远不会被发射**。

⇒ 后果（实测）：
- `DCacheWriteBuffer::ready` / `axi_valid` / `axi_addr` / `axi_wdata` **恒为未驱动**（读到 0）
- `ICachePrefetch::axi_valid` / `axi_addr` 同理
- 且 `ICachePrefetch` **从未把 `addr`/`req` 接到内层 cache** ⇒ `icache.io().addr` 恒 0
  ⇒ `miss` 恒为「冷 miss」

⇒ 即：这两个 wrapper 目前是**功能上死的**（只能证明「不 SIGSEGV」）。

**Why now**：C1 已把 `test_cache_smoke` 的断言改成**可证伪**（`miss == 1` 可被变异证伪），
并把「未驱动 ⇒ 读 0」的两条断言写成**当前事实的快照**（注释里明说「缺陷修好后本断言会转红」）。
C1 的职责是测试整治，不是补 RTL ⇒ 单独立项。

## 2. What Changes (变更范围)

- **改写条件发射**：把 C++ `if` 换成仓库既有的两种正确写法之一
  - `select(cond, a, b)` —— 值选择（仓库主流写法，见 `include/cpu/pipeline/rv32i_pipeline.h`）
  - `chlib/if_stmt.h` 的 `_if(cond)` DSL —— 需要语句块时
- **`ICachePrefetch`**：
  - 把 `io().addr` / `io().req` **接到内层 `ICacheSync`**（当前完全没接）
  - `axi_valid` / `axi_addr` 用 `select` 发射（miss && axi_ready 时）
- **`DCacheWriteBuffer`**：
  - `ready` 用 `select(req && we, true, false)` 发射
  - `axi_valid` / `axi_addr` / `axi_wdata` 用 `select` 发射（`!axi_busy && axi_ready` 时）
  - ⚠️ 当前 `buffer` 是**普通 struct 数组**（`WriteBufferEntry buffer[4]`）且
    `io().axi_addr <<= buffer[oldest].addr;` 被**注释掉** ⇒ 真正发射 AXI 写需要
    把 buffer 换成节点数组（`ch_mem` / `ch_reg[]`，按 ADR-039 的口径）
- **`test_cache_smoke`**：删除「钉住未驱动 ⇒ 读 0」的两条快照断言，
  换成**真实行为断言**（如：写请求被接受 ⇒ `ready == 1`；`axi_ready == 1` ⇒ `axi_valid == 1`）

## 3. Acceptance (验收标准)

- [ ] `ICachePrefetch`：驱动 `addr`/`req` ⇒ 内层 cache 真的按该地址查（`miss` 随命中变化）
- [ ] `DCacheWriteBuffer`：`req && we` ⇒ `ready == 1`；`axi_ready == 1` ⇒ `axi_valid == 1`
      且 `axi_addr` / `axi_wdata` 反映被写的数据
- [ ] `test_cache_smoke` 的断言**全部**是**可证伪**的行为断言（变异即转红）
- [ ] `test_cache_smoke` 中不再有「钉住缺陷现状」的快照断言
- [ ] `include/cpu/cache/**` 与 `examples/riscv-mini/src/**` 无新增编译警告
      （当前这两个 wrapper 有 `deprecated operator&&` 与 `unused variable` 警告）

## 4. Capabilities (涉及的能力维度)

- `cpu-cache-subsystem`（ICacheSync / DCacheSync 的 wrapper 层）
- `rv32i-canonical-home`（若 C2 把 wrapper 收敛进 `include/cpu/**` ⇒ 归该 capability）

## 5. Impact (影响面分析)

- **改动文件**：`include/cpu/cache/{i_cache_prefetch,d_cache_write_buffer}.h`、
  `examples/riscv-mini/tests/test_cache_smoke.cpp`
  （⚠️ 2026-10-26 订正：C2 已把 wrapper 从 `examples/riscv-mini/src/` 迁到 `include/cpu/cache/`）
- **风险**：**中** —— 改动的是 cache wrapper 的实际行为；当前无消费者
  （除 smoke 测试）⇒ 爆炸半径小
- **⚠️ 已知转红点**：`examples/riscv-mini/tests/test_cache_smoke.cpp:82-83` 当前**钉住缺陷现状**
  （`ready == false` / `axi_valid == false`）⇒ 修好即转红，MUST 同步改成真实行为断言
- **与 C2/C3 的关系**：原「强前置 = C2」**已满足**（`rv32i-canonical-convergence-2026-10`
  于 2026-10-25 归档，wrapper 已落在 `include/cpu/cache/`）⇒ 现在可直接立项
- **建议时序**：与 `feat-rv32i-isa-completeness` 并行（互不触碰）；先于把 cache 接入 RV32I 数据通路的任何改动
