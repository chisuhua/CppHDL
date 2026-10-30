# Tasks: clint-mtime-autoincrement-2026-10

> 实施 **ADR-049**（`docs/adr/ADR-049-clint-mtime-semantics.md`）。
> ⚠️ 全部期望值 MUST **手推**（禁抄仿真输出）；全部变异 MUST **先确认编译成功**
> （否则跑的是旧二进制 ⇒ 假绿）。

## 1. 立项（planner 侧，**零**代码改动）

- [x] 1.1 improvement 草案的决策门已关闭（ADR-049，2026-10-30）
- [x] 1.2 change 目录 + `proposal.md` + `design.md` + `tasks.md`
- [x] 1.3 capability delta：`cpu-clint-timer`（**ADDED ×6**）⇒ 归档**禁** `--skip-specs`

## 2. 实现（builder）

- [x] 2.1 `Clint` 加模板参数 `TIMER_DIVIDER`（默认 1）⇒ ⚠️ 2 处实例化点
      `Clint` ⇒ `Clint<>`（**纯类型名**）
- [x] 2.2 `mtime` 改**单一 64 位 `ch_reg`**（形态 B）+ 复位值 0
- [x] 2.3 分频计数器 `d`（复位 0）+ 递增条件 `d == TIMER_DIVIDER - 1`
      ⇒ ⚠️ **必须**用 `select`（禁 C++ 三元 —— 建图期恒假）
- [x] 2.4 写路径：`bits_update<32>`（低半 ⇒ lsb 0 / 高半 ⇒ lsb 32）
      ⇒ ⚠️ **必须用 64 位变量接收**（返回 64 位 ⇒ 32 位接收会**静默截断**）
- [x] 2.5 优先级：写 lo > 写 hi > 递增 > 保持
- [x] 2.6 读路径：`bits<31,0>` / `bits<63,32>` 取两半；`mtip` 比较语义**不变**
- [x] 2.7 文件头注释 + `include/cpu/AGENTS.md` **与实际行为一致**

## 3. 验证（每条必须能**转红**）

- [x] 3.1 **V1** 递增：`TIMER_DIVIDER = 1` ⇒ 连续 N 拍后 `mtime == N`
- [x] 3.2 **V2** **跨 32 位边界**：初值 `0xFFFFFFFF` ⇒ 一拍后 `0x100000000`
      （**判别性** —— 抓「进位用 C++ 三元 ⇒ 恒 0」）
- [x] 3.3 **V3** 分频相位：`TIMER_DIVIDER = N` ⇒ **第 N 拍**第一次递增
- [x] 3.4 **V4** 分频相位**不**被写复位
- [x] 3.5 **V5** 写拍落在递增边界 ⇒ 该次递增**被丢弃**且相位不变
      （**判别性** —— 区分「写复位分频器」与「写暂停分频器」）
- [x] 3.6 **V6** 写优先：被写半 = 写值 · 另一半**保持** · 本拍**不递增**
- [x] 3.7 **V7** 写后下一拍从写值继续
- [x] 3.8 **V8** 复位后 `mtip == 0`；`mtime >= mtimecmp` ⇒ `mtip == 1`
- [x] 3.9 **V9** `test_interrupt_flow` 期望值重推并仍绿（⚠️ MUST 消除
      「靠余量才通过」的时序脆弱性）
- [x] 3.10 **变异 6/6**（M1–M6，见 `design.md` §4）

## 4. 回归与计数

- [x] 4.1 `ctest -LE perf` 零回归（`test_interrupt_flow` 绿）
- [x] 4.2 `run_all_samples.sh` / `run_all_ported_tests.sh` 零回归
- [x] 4.3 **计数锚点不变**（新用例进**既有** target ⇒ `CTEST_TOTAL` 205 /
      `EXAMPLES_TEST_SOURCES` 17 / `verilator_e2e` 38 / non-perf 194）
- [x] 4.4 全部护栏（含 `--self-test`）exit 0

## 5. 归档

- [x] 5.1 归档（**禁** `--skip-specs`）⇒ 合并 `cpu-clint-timer` 的 ADDED ×6
- [x] 5.2 归档后 MUST **补写**主 spec 的 `## Purpose`（archive **不合并** delta
      的 `## Purpose`）
- [x] 5.3 ADR-049 的「关联 OpenSpec change」更新为**已归档路径**
