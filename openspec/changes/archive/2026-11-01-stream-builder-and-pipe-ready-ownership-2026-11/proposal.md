# 提案：`chlib` 链式组合的 ready 归属（builder 链内闭包 + pipe 族）

- **架构依据**：**ADR-055**（`docs/adr/ADR-055-chain-intermediate-ready-ownership.md`）
  ⇒ **不修订** ADR-054（已签批的决策内容**一字不改**）
- **规格影响**：`chlib-stream-ready-ownership` 的 **ADDED ×1**
  （新 Requirement + 8 个 Scenario）⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：中等（2 个头文件 + 1 个新 dual 测试文件 + 1 条新护栏判据 R10）
- **关联台账**：`.rddf/improvements/stream-builder-chain-silent-word-loss-2026-11.md`
  · `.rddf/improvements/chlib-naming-honesty-audit-2026-11.md`
  · `.rddf/checks/decision-request-B1-builder-backpressure-2026-11.md`

## Why

### 1. `StreamBuilder` 链是**死路**（实测，2026-11-01）

```
E10 builder 链（**正确接线**：`as_master()` + `<<=`）：
  t   source.ready   out.valid   out.payload
  0   0              1           10
  ...
  9   0              1           19
⇒ ⇒ `source.ready` **恒 0** ⇒ ⇒ **上游遵守 ready 协议 ⇒ ⇒ 永远推不动**
```

**根因**：`stream_fifo` **不驱动**入参 `ready`（写使能只用 `input_stream.valid`，
内部 `sync_fifo` 自保护）⇒ ⇒ builder 链里**没人驱动** `source.ready`
⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」在 builder API 下**不可演示**。

⚠️ **原台账标题的「静默丢字」已被实测推翻**（10 个 word 全部无损通过）
⇒ ⇒ **真缺陷只有这一条**（上游收不到反压）。

### 2. `stream_pipeline.h` 的 pipe 族**只在常量接线下活着**（A4 实测）

| 事实 | 证据 |
|---|---|
| 3 个 pipe（`m2s` / `s2m` / `half`）**都不驱动**上游 ready | `input_stream.ready = result.input.ready`（**`operator=`** ⇒ 节点重绑定 ⇒ 调用方被孤儿化） |
| 正确接线下 **零数据通过** | A4 判决性对比：46/14 vs 23/14 test cases |
| **不在**护栏 R8 的扫描面 | R8 只扫 `stream.h` + `stream_arbiter.h` ⇒ ⇒ pipe 族**不在任何 ready 契约的覆盖范围内** |
| `output.ready` **从未被驱动** | 消费端无处施加反压 ⇒ `input.ready` 引用一个无驱动的节点 |

## What Changes

| 文件 | 改动 |
|---|---|
| `include/chlib/stream_builder.h` | 加私有成员 `pending_ready_`（= **下一级要驱动**的节点）+ `pending_is_source_` + `adopt_stage_output()` + `drive_pending_ready()`（**三处响亮拒绝**）；`queue<>()` 调用回接；过滤类只登记输出（**不动**入参 ready ⇒ 否则会抹掉 `\| cond` 项）⇒ **公开 API 零变更** |
| `include/chlib/stream_pipeline.h` | 3 个 pipe：上游 ready 用 **`<<=`**（MUST NOT `operator=`）；`output.ready` 建成**带默认驱动、可被消费端覆盖**的字段（ADR-054 形态 E）；`input.ready` 由**下游** ready 派生（`m2s` 组合 / `s2m`·`half` 寄存） |
| `tests/test_stream_builder_backpressure_dual.cpp`（**新**） | 2 个 `DUAL_BACKEND_TEST_PAIR`（两级 / 一级链）⇒ **判别性断言 = 空链时 `in_ready == 1`**（修复前恒 0 ⇒ 必红）+ 零丢字金标 |
| `tests/test_stream_builder.cpp` | 既有反压用例的**接线订正**（`as_master()` + `<<=`）+ 2 个「响亮拒绝」用例（缺 `as_master()` / ready 被喂 literal） |
| `tests/test_stream_pipeline.cpp` | 接线订正 + 行为用例（正确接线下**数据真的通过** / 上游 ready 有驱动 / 消费端反压生效） |
| `scripts/lib/stream_ready_ownership.py` | **新增 R10**（builder 链内闭包不变式：存在性 / `<<=` / 3 处响亮拒绝）+ **R8 扫描面扩到 `stream_pipeline.h`** + 3 种新投毒 |
| `scripts/verify_stream_ready_ownership.sh` | 自检按**投毒目标**选判别力二进制（`builder-*` ⇒ 另一 target） |
| `AGENTS.md`（计数锚点） | `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_*` / non-perf |

## Capabilities

`chlib-stream-ready-ownership` **ADDED ×1**：

> **链内中间级的读使能 MUST 由「下一级的容量信号」驱动**
>
> 8 个 Scenario：读使能来源 / 适用面显式列出 / MUST NOT 被读作「fifo 类的例外」
> / 判据 MUST 可机械核验 / pipe 族纳入 / 上游反压 MUST 真的到达调用方 /
> MUST NOT 出现组合环 / 中间级被覆盖后 MUST NOT 再被 `connect_ready` 覆盖。

⚠️ **为什么用 ADDED 而不是 MODIFIED**（见 ADR-055 §2.5 的订正）：`MODIFIED`
是**整条 Requirement 替换** ⇒ 既有那条的正文 + **7 个 Scenario** MUST 逐字
保留 ⇒ 在其上追加反而增加「静默覆盖 / 丢 Scenario」的风险。

## Non-Goals（**明确不做**）

- `stream_fifo` 本身不改（「写使能只用 `valid`」「入参 ready 不被依赖」两条
  既有契约保持）⇒ 链的上游反压是 **builder 层**的责任；
- `demux` 的 ready 方向（B2）· `fork` 的同步语义（B3）⇒ 独立 change；
- `fwft_fifo` 的物理删除（B4）· `sequence` 的删除（B5）⇒ 独立小 change；
- 未驱动字段诊断（B7）⇒ 独立 change。

## Verification（前瞻 · 可勾选）

- [ ] 空链时 `in_ready == 1`（修复前恒 0 ⇒ **判别性**）
- [ ] 推满后 `in_ready == 0`；弹空后回到 1（双臂判据，防「恒 0 假绿」）
- [ ] 零丢字：按 ready 协议推入 N 词 ⇒ 末端序列逐项一致
- [ ] 链长 1 / 2 ⇒ 反压曲线一致
- [ ] 缺 `as_master()` / ready 被喂 literal ⇒ **建图期抛 `std::runtime_error`**
- [ ] pipe 族：正确接线下数据通过 + 上游 ready 有驱动 + 消费端反压生效
- [ ] 护栏：R10 存在；R8 扫描面含 `stream_pipeline.h`；投毒全部转红
- [ ] `ctest -LE perf` 零回归 · 计数锚点同步
