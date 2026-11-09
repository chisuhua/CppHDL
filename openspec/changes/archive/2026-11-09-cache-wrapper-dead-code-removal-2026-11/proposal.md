# Proposal: cache wrapper 的死代码清零 + `miss` 期间的地址稳定性契约

> **来源**：phase-6 **P9**（`.rddf/roadmap/phases/phase-6.md` 的 P9 行 · 2026-11-09 修订版）
> ⇒ 任务行出口判据 ① ②（删死成员 · 走 P14 登记）· ③（判别性探针 + 契约）
> ④（`include/cpu/AGENTS.md` 债务段 + 行数同步）
>
> **improvement**：无独立 improvement 草案 ⇒ 本 change 是**窗口内**的 P9 实施载体
> （phase-6 的 P3–P5 链要求 improvement 的是**架构裁定**类工作；P9 是**债务清零**类）
> ⇒ ⇒ 本 change 的 **spec 变更 = 1 条 ADDED Requirement**（`cpu-cache-subsystem`）
> ⇒ ⇒ 归档**禁** `--skip-specs`。

## Why

两个 cache wrapper（`include/cpu/cache/i_cache_prefetch.h` 的 `ICachePrefetch` ·
`include/cpu/cache/d_cache_write_buffer.h` 的 `DCacheWriteBuffer`）**名义上**带缓冲 / 预取，
实际是**直通 shim** ⇒ ⇒ 头里留着**零消费者的死数据**：

| 符号 | 位置 | 实测消费者 | 性质 |
|---|---|---|---|
| `WriteBufferEntry` / `buffer[4]` | `d_cache_write_buffer.h` | **0**（仅注释与自身声明） | 死数据（`(void)buffer` 压警告） |
| `WriteBufferConfig` | `d_cache_write_buffer.h` | **0**（只被上条引用） | 死配置面 |
| `prefetch_data` / `prefetch_addr` / `prefetch_valid` | `i_cache_prefetch.h` | **0** | 死数据（`(void)` 压警告） |
| `PrefetchConfig` | `i_cache_prefetch.h` | **0**（只被上条引用） | 死配置面 |

⇒ ⇒ 与根 `AGENTS.md` ZERO-DEBT「**No dead code**：立即删除」冲突（且 `(void)` 抑制
**掩盖**了它）⇒ ⇒ 且这些符号在**公开头**里 ⇒ ⇒ 删除 MUST 走 **P14 登记机制**
（`.rddf/checks/public-api-surface.md` + `.rddf/checks/public-api-removals.tsv`
+ `docs/CHANGELOG.md` + `scripts/verify_public_api_visibility.sh`）。

第三项债务（内层 cache 的 `miss` 与 `axi_addr` **1 拍错位**）此前**只有散文登记**，
**没有可执行判据** ⇒ ⇒ 本 change 用**判别性探针**把它变成**可执行**的契约。

## What Changes

1. **删死数据**（`include/cpu/cache/d_cache_write_buffer.h`）：
   - 删 `struct WriteBufferEntry` + `describe()` 里的 `buffer[4]` 与 `(void)buffer`；
   - 删随之**零引用**的 `struct WriteBufferConfig`；
   - ⚠️ **`__io` 签名零改动** ⇒ wrapper 的**可观测行为完全不变**。
2. **删死数据**（`include/cpu/cache/i_cache_prefetch.h`）：
   - 删 `prefetch_data` / `prefetch_addr` / `prefetch_valid` 三个 `ch_mem` 与 `(void)` 抑制；
   - 删随之**零引用**的 `struct PrefetchConfig`。
3. **P14 登记**（公开符号移除）：登记表移除章 +2 行 · 机器探针 +6 行 ·
   `docs/CHANGELOG.md` 一行 · 护栏锚点 `EXPECTED_REMOVAL_ROWS` / `EXPECTED_PROBES` 同步。
4. **判别性探针 + 契约**（P9 ③）：新增用例
   「`miss` 期间改变 `addr` ⇒ `axi_addr` 跟随**当拍**地址（1 拍错位 ⇒ 调用方 MUST 保持 `addr`）」
   ⇒ ⇒ 并把「**`miss` 期间的地址稳定性**」写成 capability `cpu-cache-subsystem` 的
   **ADDED Requirement**（含正/负两个 Scenario）。
5. **文档同步**：`include/cpu/AGENTS.md` 的「已知债务」段（① ② 改为「已删除」·
   ③ 补「调用方 MUST 在 `miss` 期间保持 `addr`」+ 契约载体）+ 头部
   「30 headers, 6506 lines」的**实测数字**同步。

## Acceptance

- [ ] **A1（死代码清零）** `include/cpu/cache/` 下 **零** `WriteBufferEntry` /
      `WriteBufferConfig` / `PrefetchConfig` / `prefetch_data` / `prefetch_addr` /
      `prefetch_valid` 的**代码**引用（注释不算 ⇒ 判据按**剥注释**口径）。
- [ ] **A2（行为不变）** 两个 wrapper 的 `__io` 签名与**全部可观测输出**逐拍不变
      ⇒ ⇒ `examples/riscv-mini/tests/test_cache_smoke.cpp` 既有 8 用例 / 58 断言**全绿**。
- [ ] **A3（P14 登记）** 登记表移除章 +2 行 · 探针 +6 行 · CHANGELOG 一行 ·
      `scripts/verify_public_api_visibility.sh`（含 `--self-test`）**绿**。
- [ ] **A4（判别性探针）** 新用例**双后端**绿；且它对「`axi_addr` 跟随当拍地址」这条
      **可执行断言** ⇒ ⇒ 若内层 cache 改成跟随 `miss` 所指地址 ⇒ 该用例**转红**。
- [ ] **A5（契约落库）** capability `cpu-cache-subsystem` 拿到 ADDED Requirement
      「`miss` 期间的地址稳定性」⇒ ⇒ 归档**禁** `--skip-specs` ⇒ ⇒
      `scripts/verify_openspec_delta.sh` **绿**。
- [ ] **A6（零回归 + 锚点）** `ctest -LE perf` 全绿 · `scripts/verify_test_counts.sh` 绿
      （本 change **不新增** ctest 条目 ⇒ `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` **不变**）。

## Capabilities

- **MODIFIED**：无；
- **ADDED**：`cpu-cache-subsystem`（1 条 Requirement：`miss` 期间的地址稳定性）。

## 用户可见变更

- **`include/cpu/cache/d_cache_write_buffer.h`**：移除公开类型 `WriteBufferEntry` 与
  公开配置面 `WriteBufferConfig` ⇒ ⇒ **下游若引用这两个符号 MUST 改为自建**（本仓内
  消费者 = **0**）⇒ ⇒ wrapper 的 `__io` 签名与可观测行为**不变**。
- **`include/cpu/cache/i_cache_prefetch.h`**：移除公开成员 `prefetch_data` /
  `prefetch_addr` / `prefetch_valid` 与公开配置面 `PrefetchConfig` ⇒ ⇒ **下游若引用
  MUST 改为自建**（本仓内消费者 = **0**）⇒ ⇒ wrapper 的 `__io` 签名与可观测行为**不变**。
- ⚠️ 本组件**不含**任何预取 / 缓冲状态 ⇒ ⇒ **真预取 / 真 FIFO 写缓冲 = 独立立项**
  （届时重新引入缓冲与配置面）。
