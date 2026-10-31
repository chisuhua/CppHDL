# Change: stream-forward-ready-closure-p1a-2026-11

## Why

🔴 **主 spec `chlib-stream-ready-ownership` 的 R1 明文说**：转发类（mux / demux /
arbiter / fork / join / filters）的 ready 链**两端都断** ⇒ ⇒ 「**转发类 MUST NOT
被本 capability 声称为『已兑现』**」⇒ ⇒ 其收口义务**显式移交** P1 台账。

⇒ ⇒ **本 change = P1-a**（台账 §2.2-1 的三个子集之第 1 个）⇒ ⇒ **把转发类的
ready 链从「死路」变成「活路」** ⇒ ⇒ ⇒ **履行**那条移交的义务。

⚠️ **实测确认「修复不是行为变更」**（探针 ④）：转发类现状 = **上游 ready 恒为 0**
⇒ ⇒ 调用方永远推不动 ⇒ ⇒ ⇒ **死路** ⇒ ⇒ ⇒ ⇒ **零破坏**承诺成立。

## What Changes

### 转发类组合子的 ready 所有权 ⇒ 形态 E（默认值 + 可覆盖）

- **mux** · **arbiter×4**（`priority_arbiter` / `round_robin_arbiter` /
  `lock` / `sequence`）· **join** · **filters×6** · **combine_with**
  ⇒ ⇒ 全部改为：
  ① 组合子**立即**建立「默认恒就绪」的 ready 节点（源由结果对象持有）；
  ② 组合子用 **`<<=`** 驱动上游（**MUST NOT** `operator=`）；
  ③ **MUST NOT** 有 `out_ready = true` 这类局部常量；
- **`StreamBuilder::queue<>()`** ⇒ ⇒ 补**上游回接**（⇒ ⇒ 修「**静默丢字**」：
  链长 ≥ 2 时 fifo1 弹出的 word 会被 fifo2 的写口自保护**静默吞掉**）；
- **`connect_ready`** ⇒ ⇒ 语义**重新定义**为「幂等覆盖消费端 ready」
  （⇒ ⇒ 与 fifo 类**同型** ⇒ ⇒ 心智模型统一；⇒ ⇒ 仓内已有 11 个调用点 ⇒ ⇒
  **不可删**）。

### 契约侧

- **MODIFIED** `chlib-stream-ready-ownership` 的 **R1** ⇒ ⇒ 「转发类 MUST NOT
  被读作『已合规』」Scenario 的**正文原地订正**（⚠️ **标题 MUST NOT 改**
  ⇒ ⇒ `demux` / `fork` 仍**未**收口 ⇒ ⇒ 该 Scenario **继续在约**）；
- **ADDED** `chlib-stream-ready-ownership` 的「转发类组合子的 ready 所有权 MUST
  是**默认值 + 可覆盖**」（含 5 个 Scenario：上游真收到反压 · 未覆盖时恒就绪 ·
  `connect_ready` 幂等 · MUST NOT 硬编码常量 ready · 死路转活路）。

### ADR 修订（**architect 权限 · 用户已批准代改**）

⚠️ **ADR-054 §3.2 第 2 条**（「mux / demux / arbiter 类 MUST 保持决策 0-B 的
**两阶段**形态」）与本 change 的**单阶段**形态**冲突**
⇒ ⇒ ⇒ **用户于 2026-11-01 裁决「A｜单阶段（形态 E）」并批准代改**
⇒ ⇒ ⇒ ⇒ ⇒ **加注「⚠️ 角色越权执行（用户已批准）」**。

## 为什么不做什么（Non-Goals）

- **`demux`**（P1-b）⇒ ⇒ 方向修正是**行为变更** ⇒ ⇒ 独立 ADR；
- **`fork`**（P1-c）⇒ ⇒ 同步语义**从未工作过** ⇒ ⇒ 等于**新功能** ⇒ ⇒ 判据从零建；
- **`fwft_fifo`** 重写/改名（ADR-045:148）；
- **「读未驱动字段诊断」**（框架层通用缺口 ⇒ ⇒ 独立立项）；
- **不动 `.rddf/roadmap/**` 的 feature `status` / objective 判据**（planner 职责）。

## Impact

- **改动文件**：`include/chlib/stream.h` · `include/chlib/stream_arbiter.h` ·
  `include/chlib/stream_builder.h` · `tests/chlib/test_stream.cpp` ·
  `tests/test_stream_builder.cpp` · `scripts/lib/stream_ready_ownership.py` ·
  `scripts/verify_stream_ready_ownership.sh` ·
  `docs/adr/ADR-054-stream-ready-ownership-form.md`
- **爆炸半径**：**中** —— 转发类组合子的 ready 语义**逐个改** ⇒ ⇒ 既有调用点
  （含 `samples/` 与 `examples/`）MUST 复核
- **风险**：**中** —— ① **公开 API 变更**（结果 struct 加 `default_ready_source`
  成员）② `connect_ready` 语义变化（幂等覆盖）③ **行为变化**（从「死路」变
  「活路」⇒ ⇒ 上游 ready 从 0 变 1 ⇒ ⇒ **依赖「上游恒不 ready」的既有断言**
  MUST 复核
- **回滚路径**：`revert` 实施 commit ⇒ ⇒ 无持久化面

## 权限说明

⚠️ **ADR-054 §3.2 第 2 条的修订**属 **architect 权限**（critic 判据：「实施方读完
新文本后被允许/被要求做的事是否变了」⇒ ⇒ 变 ⇒ ⇒ 决策内容变更）
⇒ ⇒ **用户于 2026-11-01 明确批准**由 builder 代改
⇒ ⇒ 本 change 落笔时**加注**「⚠️ 角色越权执行（用户已批准）」。
⇒ ⇒ 其余（capability delta · 实现 · 测试 · 护栏）属 build 权限。

## 评审记录

- 本 change 走**完整的 critic 评审**（**新 change** ⇒ ⇒ **不占用**上一 change
  的 2 轮预算）。
