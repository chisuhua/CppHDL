# Change: chlib-stream-semantics-2026-10 — `chlib` stream 组合子的语义契约化

- Status: **实施中（2026-10-22）**
- 立项自：`2991eed`（`fix(samples): restore 9 disabled samples + fix 3 chlib null-operand defects`）的对抗审稿 **REJECT**
- 关联 ADR：`docs/adr/ADR-045-chlib-stream-semantics.md`
- 关联 capability：`chlib-stream-semantics`（**新建**）
- 需 MODIFIED：`verilator-e2e-coverage`（其 Sync FIFO Scenario 的 fixture 归因需订正 + 需加 `chlib::stream_fifo` 的 FWFT parity）

## Why

`2991eed` 在 `include/chlib/stream.h` 修了 3 处「先读后写 ⇒ 空操作数」缺陷，
并新增了一个自称「行为级」的 Catch2 用例。对抗审稿判定 **REJECT**，实测：

| 现象 | 实测 |
|---|---|
| `chlib::stream_fifo` 的数据通路**仍然是坏的** | 推入 0x5A 后 `pop_stream.payload` cycle 0..3 全为 `0x0`，cycle 4 才是 `0x5A` |
| 新增的「行为级」用例对上述缺陷**零信号** | 只断言 `occupancy` / `empty`，**从不断言 `payload`** |
| 「变异验证 ⇒ 真护栏」**自证失效** | 该变异只区分「能跑 vs 崩溃」 |
| 同型缺陷**未修全** | `include/chlib/stream_arbiter.h:92`/`:157`、`include/chlib/stream.h:193-194`/`:395`；`stream_arbiter_lock` 额外**静默返回错值** |

### 根因：ready 的所有权从未被定义

`bundle_base` 拷贝构造**共享字段节点**（`include/core/bundle/bundle_base.h:66-70`），
`operator=` 是**节点重绑定**（`include/lnode/logic_buffer.tpp:21-26`）
⇒ 组合子按值收 `ch_stream` 后，函数体内写 `input_stream.ready = X` **只改本地拷贝的
字段指针** ⇒ **组合子永远无法驱动调用方的 `in.ready`**。

⇒ 组合子只能二选一地绕：① 构造期读未赋值字段（null proxy ⇒ 构造即炸）
② 硬编码常量 ready（`stream.h:274` `pop_ready = true`、`:251`/`:460` `out_ready = true`、
`:344`/`:353` `result.input_stream.ready = input_stream.ready`）。

**⇒ 这不是四个独立 bug，而是同一个根因的四个切面。**
`2991eed` 做的是「在从未被定义的地方打补丁」，而非「定义语义并实现它」。

### 第二个根因：FIFO 的指针与计数脱钩

`include/chlib/fifo.h` 的 `sync_fifo`：`count` 用**带门**的 `will_write`/`will_read`
（`:68-69`），而**写口与双指针**用**裸**信号（`:76`/`:79-81`/`:84-86`）⇒ 静默数据损坏：

- 满 FIFO 再写 ⇒ 写指针回绕 ⇒ **覆写队首**（实测排空得 `[255,2,3,4]` 而非 `[1,2,3,4]`）；
- 空 FIFO 上 `rden=1` ⇒ 读指针自由运行 ⇒ 之后写入的元素**永远读不出来**。

### 为什么必须先写契约

「先改代码、后补契约」正是 `2991eed` 被 REJECT 的同一种形状。
⇒ 本 change 的代码改动 MUST 与 capability delta **同批落地**。

## What Changes

### 1. `sync_fifo` 自保护（`include/chlib/fifo.h`）

写口、写指针、读指针 MUST 全部使用与 `count` **同源**的 `will_write` / `will_read`。
契约从「调用方保证 `rden` 只在非空时拉高」变为「自保护」。

连带：删除 `threshold` 死参数（函数体零引用；唯一实参传递点
`tests/chlib/test_fifo.cpp:120` 需连带清理）。

### 2. `chlib::stream_fifo` 的输出改为 FWFT（`include/chlib/stream.h` + `fifo.h`）

- `sync_fifo` 增加一个 `aread` 读端口，暴露独立字段 `q_async`；
  **`sync_fifo::q` 的 1-cycle `sread` 语义保持不变** ⇒ 既有断言零改动。
- `stream_fifo` 的 `pop_stream.payload` 改用 `q_async` ⇒ payload 与 valid **同拍**。
- `stream_fifo` 增加 `DEPTH` MUST 为 2 的幂的 `static_assert`。
- 修 `include/chlib/stream.h:277-291` 的**错误安全保证注释**（它把 `fifo.h:69`
  说成保护读指针，实际只保护 count）。

### 3. 护栏（行为级 + 判别性）

- `tests/chlib/test_fifo.cpp`：新增 5 组断言（满写不丢数据 / 读那一拍 q 即该元素 /
  count 只统计真实传输 / 排空后再写能读回 / 写 N 读 N 逐个比对）。
- `tests/chlib/test_stream.cpp`：FIFO 段补 `payload` 的**三元组**断言
  （tick 序号 + `valid` + `payload`）。
- `tests/test_stream_fifo_dual.cpp`（**新建**）：`DUAL_BACKEND_TEST_PAIR` 覆盖
  `stream_fifo` 的 payload/valid 逐拍一致（interpreter + verilator）。
- `samples/stream_fifo_example.cpp` / `samples/spinalhdl_stream_example.cpp`：
  补 payload 断言（当前 payload 全错也 exit 0 ⇒ sample 绿没有信号）。

### 4. 契约（capability + ADR）

- 新建 capability `chlib-stream-semantics`（5 条 Requirement）。
  ⚠️ 归档后收口（2026-10-22 对抗审稿）：ADDED 收窄为 **3 条**，
  见同目录 delta 顶部「收口订正」。
- 新建 `docs/adr/ADR-045-chlib-stream-semantics.md`（4 项裁决）。
- MODIFIED `verilator-e2e-coverage`：订正 Sync FIFO Scenario 的 fixture 归因
  （`SyncFifoFixture` **已实现**空/满门控，而它「简化自」的 example **没有** ⇒
  归因不准确）+ 新增 `chlib::stream_fifo` 的 FWFT parity Scenario。

## Non-Goals

1. **决策 0-A**（组合子入参改 `ch_stream<T>&` + `<<=` 写 ready）—— 破坏公开 API
   签名、波及 6 个组合子全部调用点 ⇒ **单独立项**。
2. **打包式 bundle API 的编译期拒绝**（`fifo_bundle` 超宽）—— 与本 change 无关，
   且爆炸半径最大（`tests/test_bundle_master_slave.cpp:432` 编译失败 + `:418`
   运行时护栏重写 + `bundle-master-slave-io` D2 MODIFIED + `pod_bundle_conversion`
   连带修）⇒ **独立立项**。
3. **`examples/spinalhdl-ported/fifo/stream_fifo_example.cpp` 的指针门控修复** ——
   该 example 是**反例**（cycle 5 起假满并永久卡死），修复单独立项。
4. **`fwft_fifo` 的重写 / 改名** —— 实测它**不是** FWFT；本 change 只在 capability
   里写明「勿用」⇒ 单独立项。
5. **`stream_arbiter_lock` 重写 / `stream_arbiter_sequence` 真轮转** —— 单独立项。
6. **`stream_fork` / `stream_join` / `stream_combine_with` / `stream_demux` /
   `when` 族的两阶段 ready 改造** —— 属同一 capability 的后续 Requirement，
   但**工作量与爆炸半径独立** ⇒ 单独立项（`chlib-stream-ready-ownership-2026-10`）。
7. **examples 护栏 C1 扫源文件 + 集合解析正则放宽** —— 独立立项。

## Impact

| 项 | 影响 |
|---|---|
| `include/chlib/fifo.h` | `sync_fifo` 的**公开契约变更**（自保护）+ `q_async` 字段新增 + `threshold` 参数删除 |
| `include/chlib/stream.h` | `stream_fifo` 的 `pop_stream.payload` 时序 1-cycle → 0-cycle（**公开行为变更**） |
| 消费方 | `chlib::stream_fifo` 的唯一生产调用方是 `stream.h:289` 自身；`stream_builder.h:56` 间接消费 |
| 既有测试 | `tests/chlib/test_fifo.cpp` 的 4 个 `sync_fifo` 相关 SECTION **断言不变**（已双实现干跑验证零回归） |
| 既有 sample | `samples/stream_fifo_example.cpp` 的打印序列会变化（它此前 payload 全 0 也 exit 0） |
| CI | `ctest -LE perf` 数量会变（新增 1 个 dual target）⇒ 须同步 `AGENTS.md` + `scripts/verify_test_counts.sh` |
