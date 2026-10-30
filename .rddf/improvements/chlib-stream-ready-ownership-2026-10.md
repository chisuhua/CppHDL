---
name: chlib-stream-ready-ownership-2026-10
priority: P1
source: ADR-045（`docs/adr/ADR-045-chlib-stream-semantics.md` 裁决 1 / 裁决 3 的具名 follow-up）
phase: phase-5
category: chlib
type: refactor
created_at: 2026-10-31
---

# Improvement: stream 组合子的 **ready 所有权**落地（ADR-045 具名 follow-up）

- **Date drafted**: 2026-10-31
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，**决策权归 planner / architect**
  （⚠️ 本草案**只登记实施期实测约束 + 范围界定**，**不**替 architect 裁决）
- **来源**：ADR-045 §1（决策 0-B）**已裁决**「两阶段 ready 所有权」，但明写
  「⏸ **本决策本次不生效** ⇒ 由 follow-up change `chlib-stream-ready-ownership-2026-10`
  落地」⇒ ⇒ **本草案**是该 follow-up 的立项材料。

## 1. Why (背景与动机)

`chlib::stream_fifo` 的**读使能硬编码**为 `true`
（`include/chlib/stream.h:286` `ch_bool pop_ready = true;`）⇒ ⇒ 消费端**恒就绪**
⇒ ⇒ FIFO **永远在排空** ⇒ ⇒ **`full` 不可达** ⇒ ⇒ 「**FIFO 满 ⇒ 上游暂停**」
这个反压场景**在当前 API 下无法演示** ⇒ ⇒ 2 个 disabled sample
（`stream_component_example` / `stream_handshake_example`）的**全部价值**都
依赖它 ⇒ ⇒ 连带 **6 处 `ch_in` 静默失效**
（`stream_component_example.cpp:78,79,80,119` + `stream_handshake_example.cpp:92,131`）。

⚠️ **「硬编码常量 ready」被 ADR-045 裁决 3 明文禁止**
（`ADR-045:160`「组合子 MUST NOT 硬编码常量 ready（`ready = true` 之类）」）。

## 2. What Changes (变更范围)

### 2.1 ADR-045 裁决 1 的**已签批**形态（不改）

- 组合子**不拥有**调用方的入参 ready 字段；
- **构造期**只产出 payload / valid；
- 提供 `<组合子>_connect_ready(result)`，由调用方在**设置好**
  `result.output_streams[i].ready` **之后**调用，把背压接到
  `result.input_stream.ready` / `result.input_streams[i].ready`；
- **连接手段**：ready 链 MUST 用 **`<<=`**（数据流边）
  ⇒ MUST NOT 用 `operator=`（`stream_mux_connect_ready` 正是这么写的 ⇒ 它
  **孤儿化**调用方的 `in.ready` 节点 ⇒ 调用方继续读自己的字段会拿到
  **无驱动的孤儿节点** ⇒ **静默**）。

### 2.2 ⚠️ **实施期实测的三条约束**（ADR-045 **未记载** ⇒ MUST 补裁）

⚠️ **这三条是本草案的核心价值** —— 它们**改变了 ADR-045 裁决 1 的实施形态**。

| # | 实测事实 | 证据 | 后果 |
|---|---|---|---|
| **E1** | 🔴 **「未驱动的代理节点」无法驱动任何输出** —— `ch_out::operator<<=` 要求 **driver 非 null**（`io.h:459`「Invalid port connection: driver node is null」）⇒ 而 `ch_out::operator=` 要求 **`src_lnode` 非 null**（`io.h:119`）⇒ ⇒ **两条路都不通** | 探针 `/tmp/opencode/probe_twostage.cpp` | ⇒ ⇒ **两阶段 MUST 用「代理先有**临时驱动**，阶段 2 再用 `<<=` **覆盖**」** —— 因为 `ch_bool::operator<<=` 在节点已存在时走 `set_src(0, src)`（`bool.tpp:58`）⇒ ⇒ **覆盖**已有驱动 |
| **E2** | ⚠️ **阶段 1 → 阶段 2 之间存在「读临时值」的窗口** —— 代理在阶段 1 持有临时驱动 ⇒ ⇒ 若调用方在此期间**仿真** ⇒ 读到的是**临时值**（不是消费端的 ready） | 同上 | ⇒ ⇒ **契约 MUST 要求**：`connect_ready` MUST 在**任何仿真之前**完成 ⇒ ⇒ **建议加运行期断言**（代理携带「是否已被正式驱动」的标志） |
| **E3** | ✅ **两阶段技术上可行** —— 探针实测：`consumer_ready=0 ⇒ fifo_read_en=0` · `consumer_ready=1 ⇒ fifo_read_en=1` | 同上 | ⇒ ⇒ **不是**「做不到」⇒ ⇒ 是「**需要这两条额外约束**」⇒ ⇒ 裁决方向不变，但**形态 MUST 补进契约** |

### 2.3 范围界定（建议，**待 planner / architect 确认**）

⚠️ ADR-045:190-192 把 follow-up 的范围列为「决策 0-A + 裁决 1/3 全部 MUST 的落地
（两阶段 ready、`<组合子>_connect_ready`、构造期纪律 + grep-lint）」⇒ ⇒
**6 个组合子** + `fwft_fifo` 重写 + ready 链改造 ⇒ ⇒ **建议拆成 4 个 commit**
（⚠️ **每个 commit = 1 项工作**，且**中间态 MUST 自洽可构建**）：

| # | 内容 | 价值 | 依赖 |
|---|---|---|---|
| **T3-a** | **`stream_fifo` 读使能外部化**（两阶段）+ 3 个活跃调用点迁移 + E1/E2/E3 进契约 + 护栏 | 🔴 **最高**：解锁 2 个 disabled sample + **6 处 `ch_in` 静默失效** | — |
| **T3-b** | 其余 **5 个**组合子的 ready 所有权 | 中 | T3-a 的形态被验证 |
| **T3-c** | **`fwft_fifo` 重写 / 改名**（ADR-045:148「单独立项」；capability `chlib-stream-semantics` **已有**「`fwft_fifo` MUST NOT 被当作 FWFT」契约 ⇒ **契约已冻、实现未跟上**） | 中 | 独立 |
| **T3-d** | **构造期纪律 + grep-lint 护栏**（ADR-045 裁决 3） | 中 | T3-a ~ T3-c |

⚠️ **T3-a 与 T3-c 的关系**：`fwft_fifo` 与 `stream_fifo` **都**在 ADR-045 的范围内
⇒ ⇒ 若 T3-a 先落地而 T3-c 后续 ⇒ ⇒ 中间态存在**两个** FIFO 变体 ⇒ ⇒ 可接受
（`fwft_fifo` 现状 = 名字 over-promise，capability **已冻结**「MUST NOT 被当作 FWFT」
⇒ ⇒ 中间态**不违反**任何 in-force 契约）。

## 3. Acceptance (验收标准)

- [ ] `stream_fifo` 的读使能 **MUST NOT** 硬编码常量（ADR-045:160）；
- [ ] `full` **可达** ⇒ ⇒ 反压场景**可演示**（⇒ 2 个 disabled sample 的
      归位条件达成）；
- [ ] **E1/E2/E3 三条实测约束 MUST 写进契约**（capability delta）
      ⇒ ⇒ 尤其「`connect_ready` MUST 在任何仿真之前完成」；
- [ ] `<<=` 连接手段（**MUST NOT** `operator=`）⇒ ⇒ 调用方读自己的入参
      ready 字段**不再**有效（ADR-045「读取规则」）⇒ ⇒ **MUST** 明文；
- [ ] 6 处 `ch_in` 静默失效（`io().X = value` 落在 `ch_in` 上）**全部**修复；
- [ ] 2 个 disabled sample **归位**（从 `samples/disabled_samples.txt` 移除
      + 补进 `CH_SAMPLE_TARGETS`）⇒ ⇒ samples 护栏数量守恒；
- [ ] 构造期纪律的 **grep-lint 护栏**（T3-d）MUST 抓到「组合子硬编码
      `ready = true`」；
- [ ] `ctest -LE perf` 零回归；**计数锚点不变**。

## 4. Capabilities

- **新建** capability `chlib-stream-ready-ownership`（**ADDED ×N**）⇒ ⇒
  归档**禁** `--skip-specs`；
- ⚠️ ADR-045 明写「**未收录**为 capability `chlib-stream-semantics` 的
  in-force MUST（现存组合子代码不满足它们）」⇒ ⇒ ⇒ **不能** MODIFIED 进
  `chlib-stream-semantics` 的既有 Requirement ⇒ ⇒ **新建** capability。

## 5. Impact (影响面)

- **改动文件**（T3-a 估算）：
  - `include/chlib/stream.h`（`stream_fifo` 两阶段 + `stream_fifo_connect_ready`）
  - `include/chlib/stream_builder.h`（`StreamBuilder::queue<DEPTH>()` 的调用点）
  - `include/chlib/stream.h` 的 `stream_queue` 别名（内部调 `stream_fifo`）
  - `samples/spinalhdl_stream_example.cpp`（调用点迁移）
  - `samples/stream_component_example.cpp` / `samples/stream_handshake_example.cpp`
    （归位 + 6 处 `ch_in` 修复）
  - `samples/disabled_samples.txt`（移除 2 行）
  - 护栏（构造期纪律 grep-lint）
- **爆炸半径**：**中** —— ⚠️ **公开 API 变更**（`stream_fifo` 的调用方式）
  ⇒ ⇒ 全部调用点 MUST 审计（实测：3 个活跃 + 2 个禁用）；
- **风险**：**中** —— ⚠️ 「阶段 2 必须在仿真前完成」是**新增的**调用方义务
  ⇒ ⇒ 若调用方违反 ⇒ ⇒ 读到**临时值** ⇒ ⇒ **静默** ⇒ ⇒ MUST 有运行期断言
  ⇒ ⇒ **不是**纯编译期可抓的。
