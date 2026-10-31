# 提案：stream 组合子的 **ready 所有权**（ADR-054 形态 B）

- **来源**：ADR-045 §1 决策 0-B 的具名 follow-up（**裁决已由 ADR-054 补齐**）
- **规格影响**：**新建** capability `chlib-stream-ready-ownership`（ADDED ×4）
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**中-大** —— **公开 API 变更**（`stream_fifo` 的结果类型 +
  调用方式）⇒ 波及全部调用点

## Why

`stream_fifo` 的读使能**硬编码**为 `true`（`include/chlib/stream.h:286`）
⇒ ⇒ 消费端恒就绪 ⇒ ⇒ FIFO 永远在排空 ⇒ ⇒ **`full` 不可达**
⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」这个反压场景**在当前 API 下无法演示**
⇒ ⇒ 2 个 disabled sample 的**全部价值**依赖它
⇒ ⇒ 连带 **6 处 `ch_in` 静默失效**（`io().X = value` 落在 `ch_in` 上）。

⇒ ⇒ 且 ADR-045:160 **明文禁止**「组合子硬编码常量 ready」⇒ ⇒ 现状**违反**已签批 ADR。

⇒ ⇒ **ADR-045 决策 0-B 不足**：它对 mux 类足够（ready 是**纯转发**），
但对 fifo 类**不够**（读使能是**内部逻辑的输入** ⇒ 构造期就要用 ⇒ 时序悖论）
⇒ ⇒ **ADR-054** 裁决 fifo 类改用**形态 B（延迟构造）**。

## What Changes

- `stream_fifo` 改为**形态 E（默认值 + 可覆盖）**（ADR-054 §3，2026-10-31 修订）：
  - **立即**建内部 FIFO ⇒ ⇒ **既有行为逐位不变** ⇒ ⇒ **存量调用点不需迁移**；
  - 读使能接一个**带默认驱动**的字段（默认 = **既有旧语义**「恒就绪」）；
  - 新增 `stream_fifo_connect_ready(result)`：把消费端 ready **覆盖**到该字段；
  - 🔴 读使能 MUST 引用**字段节点本身**（MUST NOT 局部节点）⇒ ⇒ 否则覆盖
    **不传播 ⇒ 静默失效**；
  - 🔴 多驱动守卫：`connect_ready` MUST 断言「当前驱动仍是默认节点」，
    否则 MUST **抛异常**（⚠️ `CHERROR` 只打日志不抛）；
- 迁移**活跃**调用点：`stream.h` 的 `stream_queue` 别名 · `stream_builder.h` 的
  `StreamBuilder::queue<DEPTH>()` · `samples/spinalhdl_stream_example.cpp`；
- **归位** 2 个 disabled sample（`stream_component_example` /
  `stream_handshake_example`）+ 修**6 处** `ch_in` 直写；
- `stream_mux_connect_ready` 的连接手段由 `operator=` 改为 **`<<=`**
  （ADR-045 决策 0-B 的连接手段要求）；
- **护栏**：① 构造期纪律 grep-lint（禁硬编码常量 ready）② 调用点护栏
  （调 `stream_fifo` 之后 MUST 有对应 `*_connect_ready`）。

## Acceptance

- [ ] **效果判据**：消费端 ready == 0 ⇒ MUST **阻止推进**（⇒ ⇒ FIFO 不弹出）
      ⇒ ⇒ 反压场景可被**断言**；
- [ ] **`full` MUST 可达**（DEPTH=4 ⇒ 写 4 个后 `full == 1`）
      ⇒ ⇒ **反向压力场景可演示**；
- [ ] **不调 `connect_ready` 时行为 MUST 与改动前逐位一致**
      ⇒ ⇒ **存量调用点 MUST NOT 需要迁移**；
- [ ] `connect_ready` 覆盖 MUST **传播**到内部逻辑 ⇒ ⇒ 读使能 MUST 引用
      **字段节点本身**（MUST NOT 局部节点）⇒ ⇒ **覆盖不传播**的变异 MUST 转红；
- [ ] **多驱动 MUST 被检出** ⇒ ⇒ `connect_ready` MUST **抛异常**
      （⚠️ MUST NOT 只打日志）；
- [ ] ready 链的连接手段 MUST 是 `<<=`（MUST NOT `operator=`）⇒ ⇒ 字段孤儿化的
      变异 MUST 转红；
- [ ] **6 处 `ch_in` 静默失效** MUST 全部修复（逐处核实端口方向）；
- [ ] **2 个 disabled sample MUST 归位** ⇒ samples 护栏数量守恒；
- [ ] **调用点护栏**：调 `stream_fifo` 后 MUST 出现 `connect_ready` **或** 对
      `pop_stream.ready` 的 `<<=` ⇒ ⇒ **或**显式豁免标记（豁免**非静默**）；
- [ ] `ctest -LE perf` 零回归；**计数锚点不变**。

## Capabilities

- **新建** `chlib-stream-ready-ownership` ⇒ **ADDED ×4** ⇒ 归档**禁** `--skip-specs`。

## Impact

- **改动文件**：`include/chlib/stream.h` · `include/chlib/stream_builder.h` ·
  `samples/spinalhdl_stream_example.cpp` ·
  `samples/stream_component_example.cpp` · `samples/stream_handshake_example.cpp` ·
  `samples/disabled_samples.txt` · 护栏脚本（新建/扩展）。
- **爆炸半径**：**中** —— **公开 API 变更** ⇒ 全部调用点 MUST 审计
  （实测：3 个活跃 + 2 个禁用）。
- **风险**：**中** —— ⚠️ 旧调用点**编译仍过**（签名不变）但**行为错**
  ⇒ ⇒ **必须** grep-lint 护栏 + **运行期**判据。
- **回滚路径**：`revert` 实施 commit ⇒ ⇒ 旧语义 = 「静态比较器式」的 FIFO
  ⇒ ⇒ 无持久化面、无数据兼容负担。

## Non-Goals

- **不**做 ADR-045 决策 0-A（入参改引用）的签名变更；
- **不**做 `fwft_fifo` 重写 / 改名（ADR-045:148 单独立项）；
- **不**做其余组合子的 ready 所有权（T3-b）—— 本 change 只做 fifo 类；
- **不**修 `examples/.../fifo/stream_fifo_example.cpp` 的指针门控（ADR-045:197-198）。
