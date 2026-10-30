# 提案：CLINT 的 `mtime` MUST 是**自由运行计数器**

- **来源**：`.rddf/improvements/clint-mtime-autoincrement-2026-10.md`
  （其决策门已于 2026-10-30 由 **ADR-049** 关闭）
- **规格影响**：**新建** capability `cpu-clint-timer` ⇒ **ADDED ×6**
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**小**（改动面 = 1 个头 + 1 个既有测试文件；
  实例化点 **2** 处 / 消费者 **1** 个文件 ⇒ 未接入 SoC ⇒ 爆炸半径极小）

## Why

`include/cpu/riscv/clint.h` 的 `mtime` 在 RISC-V 规范里是**自由运行的机器定时器**
（软件靠读它做时基），但当前实现里它**只能由软件经写端口显式写入**：

```cpp
mtime_lo->next = select(write_mtime_lo, io().write_data, mtime_lo);   // 纯保持
mtime_hi->next = select(write_mtime_hi, io().write_data, mtime_hi);
```

⇒ ⇒ 无任何递增源 ⇒ `mtime` 在整个仿真期间**恒为软件写的那个值** ⇒
`mtip = (mtime >= mtimecmp)` 退化为「**软件写过的比较值**的比较」⇒ ⇒
**软件不写就不产生定时器中断** ⇒ ⇒ 组件**名不副实**（叫 CLINT / timer，
实际是静态比较器）。

⇒ 该缺口由 change `riscv-mini-test-triage-2026-10` 实测挖出（修
`test_interrupt_flow` 的断言时）⇒ 该 change 已把 `clint.h` 的文件头注释改成
**如实描述**（不再声称「每个 `tick()` 递增」），并删掉了那个**空实现**的 `tick()`
与 2 个恒返 0 的占位 getter ⇒ ⇒ **缺陷被登记但未修**。

⚠️ **归属缺口**：实测 `grep -rn "clint" openspec/specs/*/spec.md` 仅在
`verilator-rv32i-e2e` 里有一句「本路径不触发」⇒ ⇒ **没有任何 capability 规定
CLINT 的语义** ⇒ ⇒ 本次是**契约变更** + **新建 capability**。

## What Changes

- `mtime` 改为 **64 位自由运行计数器**（每个递增拍 +1，无符号回绕）；
- 新增**编译期常量** `TIMER_DIVIDER`（默认 **1** = 每拍递增 1）⇒ 递增频率
  MUST 可配置但 MUST NOT 是运行期端口；
- **写优先**：软件写 `mtime` 的那一拍取写值、另一半保持、**本拍不递增**；
  写后的值成为**下一次**递增的起点（= 校时语义）；
- `mtime` 复位值 **0**（`mtimecmp` 保持全 1）⇒ 复位后**不**触发中断；
- 64 位读**允许撕裂** ⇒ **不**新增端口（`mtimecmp` 语义不变）；
- 内部表示采用 **ADR-049 D6 的形态 B**（单一 64 位 `ch_reg` + `bits_update<32>`）
  —— 进位**自动正确** ⇒ 少一类 bug；代价是「保高半换低半」需 `bits_update<32>`；
- `Clint` 加**模板参数** `TIMER_DIVIDER`（默认 1）⇒ ⚠️ 现有 2 处实例化点
  MUST 由 `Clint` 改为 `Clint<>`（**纯类型名**变化 ⇒ 零行为影响）。

⇒ ⇒ **契约载体**：**新建** capability `cpu-clint-timer`（ADDED ×6）。

## Acceptance

- [ ] `TIMER_DIVIDER = 1` 时连续 N 拍后 `mtime` 逐拍 +1（期望值**手推**）；
- [ ] **跨 32 位边界**：初值 `0xFFFFFFFF` ⇒ **一拍**后 MUST 为 `0x100000000`
      （**判别性**用例）；
- [ ] `TIMER_DIVIDER = N > 1` 时**第 N 拍**发生第一次递增（分频相位）；
- [ ] 分频相位**不**被软件写复位；
- [ ] 写拍**恰落在递增边界** ⇒ 该次递增**被丢弃**且相位**不变**（**判别性**）；
- [ ] 写的那一拍：被写半 = 写值、另一半**保持**、**本拍不递增**；
- [ ] 写后的**下一拍**从写值继续递增（写 50 ⇒ 下一拍 51）；
- [ ] 复位后 `mtime = 0`、`mtimecmp` = 全 1 ⇒ `mtip` MUST 为 0；
- [ ] `mtime >= mtimecmp` ⇒ `mtip` 为 1（既有语义**逐项不变**）；
- [ ] `test_interrupt_flow` 的期望值**重新推导**并仍绿；
- [ ] **变异 6/6** 全被抓（每条**先确认编译成功**）；
- [ ] `ctest -LE perf` 零回归；**计数锚点不变**（新用例进**既有** target）；
- [ ] `clint.h` 与 `include/cpu/AGENTS.md` 的注释**与实际行为一致**；
- [ ] 归档（**禁** `--skip-specs`）⇒ 归档后 MUST 补写主 spec 的 `## Purpose`。

## Capabilities

- **新建** `cpu-clint-timer` ⇒ **ADDED ×6** ⇒ 归档**禁** `--skip-specs`。

## Impact

- **改动文件**：
  - `include/cpu/riscv/clint.h`（**主体**）；
  - `examples/riscv-mini/tests/test_interrupt_flow.cpp`（期望值重推 + 判别性用例）；
  - `include/cpu/AGENTS.md`（契约同步）；
  - `openspec/specs/cpu-clint-timer/spec.md`（归档时合并）。
- **爆炸半径**：**极小** —— CLINT 的仓内实例化点**仅** 2 处（都在
  `test_interrupt_flow.cpp`）、消费者**仅**该文件；**未**接入 SoC
  （`rv32i_pipeline_top.h` 里**没有** CLINT 实例）⇒ ⇒ 无外部依赖面。
- **风险**：**低-中** —— `mtime` 从「恒定」变「会动」⇒ 任何依赖 `mtime` 稳定的
  断言都会变红 ⇒ `test_interrupt_flow` 的期望值 MUST 重新推导。
- **回滚路径**：`revert` 实施 commit 即可 ⇒ 旧语义 = 「静态比较器」⇒
  **无迁移负担**（无持久化格式、无数据兼容面；仅 2 处类型名 `Clint` ⇒ `Clint<>`）。

## Non-Goals

- **不**把 CLINT 接入 SoC（独立立项）；
- **不**提供 64 位原子读 / 撕裂检测（D5 ⇒ 已知边界）；
- **不**变更 `mtimecmp` 的语义（保持 R/W、复位全 1）；
- **不**变更 `Clint` 的**命名空间**（`ch::chlib`）⇒ ADR-033 的命名空间治理另议；
- **不**新增运行期配置端口（ADR-049 §3 S3 已否决）。
