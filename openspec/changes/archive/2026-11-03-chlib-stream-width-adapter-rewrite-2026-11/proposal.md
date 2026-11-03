# 提案：chlib stream 宽度适配器重写（ADR-064 实施）

- **ADR**：`docs/adr/ADR-064-chlib-stream-width-adapter-rewrite.md`（**Accepted** 2026-11-03）
- **立项来源**：`.rddf/improvements/stream-width-adapters-crash-on-simulate-2026-11.md`
- **关联 capability**：`chlib-stream-ready-ownership`（**ADDED ×1**）⇒ 归档**禁** `--skip-specs`
- **行为变更**：是（三个 API 从「一仿真就崩」变可用；输出 +1 拍延迟）

<!-- touched-components: stream_narrow_to_wide, stream_wide_to_narrow, stream_width_adapter -->

## Why

`include/chlib/stream_width_adapter.h` 的**三个对外 API 一仿真就 SIGSEGV**
（独立探针实测 `exit=139`）：`result.ready` 是刚建出来的 `ch_stream` 字段、
**没有任何节点**，却被直接喂给 `select(...)` ⇒ mux 的操作数为空 ⇒
`Simulator::initialize()` 实例化图时崩溃。

⚠️ 成因是**缺失 ADR-054 形态 E 的那一步**（先建默认驱动、再读）。

⚠️ **既有 8 个 TEST_CASE 完全看不见它**：`Simulator` 出现 **0 次** ⇒ 全裸
`context` + 只断 `.width` ⇒ 图从不被实例化 ⇒ 空操作数永不兑现。

⚠️ 且**数据通路也错**（ADR-064 §1.4 实测）：宽字**缺最后一个窄字** +
累积器**从不清零** ⇒ 跨组污染。

## What Changes

| 文件 | 改动 |
|---|---|
| `include/chlib/stream_width_adapter.h` | 重写三个适配器（形态 E + 寄存输出 + 数据通路修正）；**删除** `stream_extend` / `stream_truncate` |
| `tests/chlib/test_stream_width_adapter_dual.cpp`（**新**） | 4 个 `DUAL_BACKEND_TEST_PAIR` / **152 == 2 × 76** 断言 |
| `tests/CMakeLists.txt` | 注册新 dual target |
| `openspec/specs/chlib-stream-ready-ownership/spec.md` | ADDED ×1（1 Requirement + 4 Scenario） |

## 关键裁决（详见 ADR-064）

1. **重写而非删除**：删除的先例（`fwft_fifo` / `stream_arbiter_sequence`）都**有替代**
   ⇒ 宽度适配没有替代（`extend`/`truncate` 只做单拍零扩展，不做多拍累积/拆分）
   ⇒ 删除 = 造能力缺口。
2. **输出改成寄存器（+1 拍）**：这是**可测试性约束** —— 组合输出在本仓
   `set_input → tick → get_port_value` 读模型下**不可观测**（读到沿后组合值，
   而正确值出现在沿前）⇒ 选组合输出就**写不出 parity 判据**。
3. **删除 `stream_extend` / `stream_truncate`**：零消费者 + 零测试 + 静默失效。

## 变异证据（每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 结果 |
|---|---|
| M1 去掉 `result.ready` 默认驱动 | 💥 **CRASH**（崩溃复现） |
| M2 `input.ready <<=` 改回 `operator=` | 🔴 exit=4 |
| M3 累积器不清零 | 🔴 exit=2（⚠️ **首轮恒绿**，见下） |
| M4 输出改成组合 | 🔴 exit=4 |
| M5 宽字不含当前窄字 | 🔴 exit=4 |

### ⚠️ 如实登记：测试数据本身的判别力缺口（M3 首轮恒绿）

首版数据组 1 = `0xA0..0xA3` / 组 2 = `0xB0..0xB3` ⇒ 每个字节都满足
「组 1 **或** 组 2 == 组 2」（`0xA0|0xB0 == 0xB0`）⇒ **按位或掩盖了跨组污染**
⇒ M3 恒绿。改为**位错开**（组 1 高半字节 / 组 2 低半字节）后转红。

⇒ 该约束已写进测试文件注释 + capability 的 Scenario NOTE。

## Verification（已实测回填）

- [x] 三个适配器构造 `Simulator` 不崩溃
- [x] 宽字含最后一个窄字（`0xC3D2E1F0`）+ 组间隔离（`0x3C2D1E0F`）
- [x] 背压：下游 `ready=0` ⇒ 上游 `ready=0` 且输出不变、恢复后不丢
- [x] `wide_to_narrow` 按低字节优先（`0xD4/C3/B2/A1`）
- [x] 输出 `valid`/`payload` 寄存器化，`tick()` 后可观测
- [x] `stream_extend` / `stream_truncate` 已删除
- [x] 变异 5/5 抓（含 1 次「测试数据判别力缺口」的实测修正）
- [x] 既有 8 个结构性用例保留且仍绿（18 断言）
- [x] 全量重建后 `ctest -LE perf` 零回归
- [x] capability ADDED ×1 ⇒ 归档**未**用 `--skip-specs`

## 不在本次范围

- `stream_arbiter_lock`（恒选通道 0，**静默数据损坏**）· `round_robin_arbiter`（不轮转）
  ⇒ 各自独立立项
- `priority_encoder` 注释与实现相反（`include/chlib/converter.h`）⇒ 独立立项
