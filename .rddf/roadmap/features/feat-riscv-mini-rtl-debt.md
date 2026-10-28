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
| 两个 cache wrapper 的分支逻辑**从不发射** | `.rddf/improvements/riscv-mini-cache-wrapper-live-logic-2026-10.md`（P2） | ✅ **已修复**（2026-10-28，change `riscv-mini-cache-wrapper-live-logic-2026-10` **已归档**）⇒ 3 处建图期 C++ `if` 全部清除 + 10 个 `ch_out` 全部无条件驱动 + 内层 cache 的 5 个输入全部显式驱动 |
| CLINT `mtime` 不自增 | `.rddf/improvements/clint-mtime-autoincrement-2026-10.md`（P2） | ✅ 仍在：`include/cpu/riscv/clint.h` 的 `mtime` 只能由软件经写端口写入 ⇒ 只构成静态比较器 |

> 两者均为 **P2**（不阻塞 parity / ISA 主线），因此排在新 feature `feat-rv32i-isa-completeness` 之后。

## 排期时必须知道的两件事

1. **路径已因 C2 变化**：wrapper 已从 `examples/riscv-mini/src/` 迁到 `include/cpu/cache/`
   （improvement 草案写的旧路径已订正）。
2. ~~**修好会转红两个既有断言**~~ —— ✅ **2026-10-28 已处置**（同 commit）：
   `examples/riscv-mini/tests/test_cache_smoke.cpp` 的两条「钉住缺陷现状」断言
   （`ready == false` / `axi_valid == false`）**已反转** ⇒ 反转方式是
   「**同一 TEST_CASE 内依次改变输入 ⇒ 逐个输出断言跟随自己的输入变化**」
   ⇒ **不是**靠删除断言了事，也**不是**改成 `== true`（后者只证明「有驱动」）。

## 候选 change 分解

| # | 候选 change | 范围 | 前置 | 验收 |
|---|---|---|---|---|
| ① | `riscv-mini-cache-wrapper-live-logic-2026-10` | ⚠️ **裁决改为「无条件组合赋值」**（**不是** `select()`/`_if()` —— 后两者都是「条件块」形态，仍需判断「是否发射」）⇒ 把条件落到**输出端口的取值**里；`ICachePrefetch` 把 `addr`/`req`/`axi_ready`/`axi_data`/`axi_resp_valid` 真正接到内层 cache + 转发内层 AXI 主端；`DCacheWriteBuffer` 补齐 `axi_addr`/`axi_wdata`/`axi_we` 的驱动 | C2 ✅ 已归档 | ✅ **2026-10-28 已归档**：`req && we ⇒ ready==1`、`axi_ready ⇒ axi_valid==1`、`axi_addr`/`axi_wdata` 反映被写数据；`test_cache_smoke` 无「缺陷快照」断言 |
| ② | `clint-mtime-autoincrement-2026-10` | `CLINT::tick()` 递增 `mtime`（或按 `mtimecmp` 语义补自由运行计数），恢复文件头注释所述契约 | 无 | 连续 N 拍后 `mtime == N`；写端口仍可覆盖；`test_interrupt_flow` 仍绿 |

## 反例

- **不要**为了「不转红」而保留「未驱动 ⇒ 读 0」的断言 —— 那是**把缺陷写进规格**。
- **不要**把 cache wrapper 的修复扩大到 cache 微架构（scope 只到 wrapper 层的信号发射与接线）。
- **不要**给 CLINT 加「自动递增」却同时保留「软件任意写」而不写清优先级 —— 会得到
  一个两道驱动源的静默冲突；MUST 在 change 里明确「写端口是覆盖还是重载」。

## 验收标准

- [x] `include/cpu/cache/{i_cache_prefetch,d_cache_write_buffer}.h` 内**零**建图期 C++ `if`
      控制 RTL 发射（grep 可查：`if (io()` 形态）
      —— ✅ **2026-10-28**（change `riscv-mini-cache-wrapper-live-logic-2026-10` **已归档**）：
      改动后两个 wrapper 的 C++ `if` 数量 = **0** ⇒ grep **恒空** ⇒
      护栏从「抓现状」变成「**防复发**」
      ⇒ ⚠️ **护栏的已知边界**：grep **不区分代码与注释** ⇒ 注释**不得**逐字引用
      那个形态（已登记进主 spec 的 Scenario NOTE）
- [x] `test_cache_smoke` 的断言全部为可证伪的行为断言（变异即转红）
      —— ✅ **2026-10-28**（同一 change）：原「钉住缺陷现状」的 2 条断言
      （`ready == false` / `axi_valid == false`）**已反转** ⇒ 反转方式是
      「**同一 TEST_CASE 内依次改变输入 ⇒ 逐个输出断言跟随自己的输入变化**」
      （**不是**改成 `== true`——后者只证明「有驱动」）
      ⇒ 现为 **8 个用例 / 58 断言**、**双后端**（interpreter + verilator）、
      含**内层 cache 的 fill → hit** 路径 ⇒ **变异 5/5 全转红**
- [ ] CLINT `mtime` 随时间递增且写端口语义明确
      —— ⛔ **仍未立项** —— **ADR-049 前置**（`mtime` 从静态比较器变自由运行
      计数器 = **契约变更**，而 **CLINT 不在任何 capability 覆盖内** ⇒ 归属本身
      待裁决 ⇒ delta 写不出 ⇒ **不得**先登记 change 再标 `skip_specs`
      ⇒ 决策门 D1–D5 已写入 `.rddf/improvements/clint-mtime-autoincrement-2026-10.md` §6

> **① 的实施结果（2026-10-28）**：change `riscv-mini-cache-wrapper-live-logic-2026-10`
> 已实施并归档。要点：
> ① **修掉 3 处建图期 C++ `if`**（`i_cache_prefetch.h` 2 处 + `d_cache_write_buffer.h`
>    3 处）—— ⚠️ 根因：`ch_bool` 有 `explicit operator bool()` ⇒ C++ `if` 拿到的是
>    「**当前节点是否非零**」，而 `describe()` 在**建图期**执行 ⇒ **分支体从不发射**；
> ② ⚠️ **不止是 `if`**：只把 `if` 换掉会让 `axi_addr`/`axi_wdata` **仍然未驱动**
>    （驱动是**注释掉的**）且 `axi_we` **从未被驱动过** ⇒ 会发出「地址 0、无写使能」
>    的 AXI 请求 ⇒ **比「恒未驱动」更危险** ⇒ 一并补齐（**10 个 `ch_out` 全部无条件驱动**）；
> ③ **`ICachePrefetch` 的 AXI 主端改为转发内层**（内层 `ICacheSync` **本身**就是
>    完整 AXI 主端 ⇒ 不在 wrapper 层重推导 ⇒ AXI 的「VALID 不得等 READY」纪律
>    天然成立）⇒ 内层 cache 的 **5 个输入端口全部显式驱动**（此前全部未驱动
>    ⇒ `miss` 恒为冷 miss ⇒ hit/miss 不可区分）；
> ④ **顺带消掉 3 条既有 warning**（2 处 `&&` deprecated + 1 处未用变量）
>    ⇒ 两个改动文件**零 warning**；
> ⑤ ⚠️ **计划期草案有 3 处前提不成立**（已在 `design.md` §8 逐条订正）：
>    「MUST 用 `select(...)` 表达」**过窄**（布尔 AND 用 `&`；框架已弃用 `&&`）/
>    M4 的「断开驱动」**转红不确定**（未定义值）⇒ 改为「驱动改成常量 0」/
>    「根 `AGENTS.md` 有 `verilator_e2e` 计数锚点」**不成立**（只有历史 NOTES 条目）
>    ⇒ **新建** `- VERILATOR_E2E_TOTAL: 38` 锚点 + 护栏第 2 条检查
>    （⚠️ 只改标签**不改变** `CTEST_TOTAL` ⇒ 需要**独立**锚点防双后端覆盖静默缩面）；
> ⑥ 计数：`CTEST_TOTAL` **202 不变** / non-perf **191 不变** /
>    `EXAMPLES_TEST_SOURCES` **17 不变** / `ctest -L verilator_e2e` **37 → 38**。
