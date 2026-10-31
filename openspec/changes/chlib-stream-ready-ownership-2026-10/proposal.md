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

- `stream_fifo` 改为**延迟构造**：
  - 构造期**不产出**任何字段（结果对象**不完整**）；
  - 新增 `stream_fifo_connect_ready(result)`：调用方设置好
    `result.pop_stream.ready` **之后**调用 ⇒ ⇒ **此时**才建内部 FIFO
    ⇒ ⇒ 读使能**直接**取该 ready 的**同一节点** ⇒ ⇒ **无代理节点**；
  - 结果类型 MUST 携带**输入 stream**（否则 `connect_ready` 无从建 FIFO）；
- 迁移**活跃**调用点：`stream.h` 的 `stream_queue` 别名 · `stream_builder.h` 的
  `StreamBuilder::queue<DEPTH>()` · `samples/spinalhdl_stream_example.cpp`；
- **归位** 2 个 disabled sample（`stream_component_example` /
  `stream_handshake_example`）+ 修**6 处** `ch_in` 直写；
- `stream_mux_connect_ready` 的连接手段由 `operator=` 改为 **`<<=`**
  （ADR-045 决策 0-B 的连接手段要求）；
- **护栏**：① 构造期纪律 grep-lint（禁硬编码常量 ready）② 调用点护栏
  （调 `stream_fifo` 之后 MUST 有对应 `*_connect_ready`）。

## Acceptance

- [ ] `stream_fifo` 的读使能 **MUST NOT** 硬编码常量 ⇒ grep-lint MUST 抓到；
- [ ] **`full` MUST 可达** ⇒ 反压场景可被**断言**；
- [ ] 「未调用 `connect_ready` 就读结果」MUST **可诊断失败**
      ⇒ ⇒ MUST NOT 退化为「读到 0 / 常量」；
- [ ] ready 链 MUST 是**同一节点** ⇒ ⇒ MUST NOT 有代理节点（**E1 回归锁**）；
- [ ] ready 链的连接手段 MUST 是 `<<=`（MUST NOT `operator=`）；
- [ ] **6 处 `ch_in` 静默失效** MUST 全部修复（逐处核实端口方向）；
- [ ] **2 个 disabled sample MUST 归位** ⇒ samples 护栏数量守恒；
- [ ] **变异**：删掉 `connect_ready` 的调用 ⇒ MUST 转红（判别力）；
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
