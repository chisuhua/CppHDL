---
name: stream-builder-chain-silent-word-loss-2026-11
priority: P1
source: P1-a 实施期实测（探针 `/tmp/opencode/p1/p1a_probe11.cpp` + `dbg_builder.cpp`，2026-11-01）
phase: phase-5
category: chlib
type: bugfix
created_at: 2026-11-01
---

# Improvement: `StreamBuilder` 的链式 `queue<>()` **静默丢字** + **上游反压丢失**

- **Date drafted**: 2026-11-01
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，**决策权归 planner / architect**
  （⚠️ 本草案**只登记实测事实 + 范围界定**，**不**替 architect 裁决）
- **来源**：change `stream-forward-ready-closure-p1a-2026-11` 的实施期探针
  ⇒ ⇒ 该 change 的 builder 子项**已移出**（本项独立立项）
- ⚠️ **关联**：capability `chlib-stream-ready-ownership` · ADR-054 §3.2 第 1 条
  · `.rddf/improvements/stream-forward-ready-closure-2026-10.md` §2.2-2

## 1. Why (背景与动机)

🔴 **实测确认：builder 的链式 `queue<>()` 会丢数据，且丢得「看不见」**。

```
链长 2：第 1 拍后 out.payload=0x00 out.valid=0
         第 10 拍后 out.payload=0x00 out.valid=0（期望 5A 1）  ❌
链长 1（对照组）：第 10 拍后 out.payload=0x00 out.valid=0（期望 5A 1）  ❌
```

⚠️ **但** 单独探针显示**第 1 拍**时 `out.payload=0x5A` `out.valid=1` ✔
⇒ ⇒ ⇒ **数据先出现后消失** ⇒ ⇒ ⇒ ⇒ **⇒ 「静默丢字」成立**
（被下游吞掉）⇒ ⇒ ⇒ ⇒ ⚠️ **且链长 1 也丢**（⇒ ⇒ 原台账的「链长 ≥ 2 才丢」
**不准确** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 已订正**）。

### 🔴 两条**独立**的缺陷

**缺陷 1｜上游反压丢失**：调用方 `in.ready`（`as_master()` 后）
**有节点**但 **`srcs == 0`** ⇒ ⇒ ⇒ **上游永远收不到反压**
⇒ ⇒ ⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」在 builder 链里**不可演示**。

**缺陷 2｜数据被吞**：`stream_fifo` 的 `sync_fifo` **写口自保护**
（`include/chlib/fifo.h:74` `will_write = wren & !is_full(count)`）
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ FIFO 没有「写口 ready」输出**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 下游 FIFO 无法告诉上游「我还能收」**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 上游回接在当前 fifo 语义下「无路可走」**。

⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **两者叠加** = **静默丢数据**（上游以为能推 ⇒ ⇒ 推了
⇒ ⇒ ⇒ 数据被下游的自保护吞掉 ⇒ ⇒ ⇒ **无任何报错**）。

### ⚠️ 为什么长期无人发现

⚠️ `tests/test_stream_builder.cpp` 的既有用例**只做结构断言**（链能建起来）
⇒ ⇒ ⇒ **零行为覆盖** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **「假绿」**。
⚠️ 且 `samples/` 与 `examples/` 里的 builder 用例**不检查 payload 序列**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 数据丢了也没人看见**。

## 2. What Changes (变更范围)

### 2.1 🔴 **候选修法**（**待 architect 裁决** —— 三者都有代价）

- **A｜`build()` 延迟连接**（**推荐方向**）：builder 保存链上**所有** FIFO 的
  结果对象（⇒ ⇒ **加私有成员**）⇒ ⇒ 在 `build()` 里把**中间级**的
  `pop_stream.ready` 接到**下一级**的 `push_stream.ready`（= `!full`）
  ⇒ ⇒ ⇒ ⚠️ **代价 1**：builder 的链是**逐步**建立的 ⇒ ⇒ 第 1 次 `queue()`
  时**不知道**后面还有几级 ⇒ ⇒ ⇒ **连接 MUST 延迟**；
  ⇒ ⇒ ⇒ ⚠️ **代价 2**：中间级的 `pop_stream.ready` 被接到 `!full` ⇒ ⇒ ⇒
  **消费端失去覆盖权** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 与 ADR-054 §3.2 第 1 条（读使能 =
  消费端的 ready，可覆盖）存在张力** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 需 architect 裁决**；
- **B｜给 `sync_fifo` 加「写口 ready」输出**：⇒ ⇒ 从**根**上解决「下游无法
  告诉上游能否收」⇒ ⇒ ⇒ ⚠️ **这是 `chlib/fifo.h` 的公开 API 变更**
  ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 需独立 ADR**（且影响 `fwft_fifo` / `async_fifo`）
  ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 影响面最大**；
- **C｜只修缺陷 1（上游反压）**：让 builder 把**最后一级**的 `pop_stream.ready`
  接到**调用方**可见的位置 ⇒ ⇒ ⇒ ⚠️ **但**缺陷 2（丢字）**仍在**
  ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 不接受**（半修 = 假绿）。

### 2.2 ⚠️ **本项 MUST 与 ADR-054 的关系**

⚠️ ADR-054 §3.2 第 1 条规定 fifo 类的读使能 = **消费端的 ready**（**可覆盖**）
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 修法 A 会让「中间级」的读使能**不再可覆盖**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 必须由 architect 明确**：
「中间级」是否属于 ADR-054 所说的「fifo 类」⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 若属于 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 修法 A 与之冲突**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 若不属于（视为「链内实现细节」）⇒ ⇒ ⇒
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 修法 A 可行** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这是必须先裁决的点**。

## 3. Acceptance (验收判据 —— MUST 可勾选)

- [ ] **效果判据**（**MUST NOT** 词法）：`queue<4>()` 推入 `0x5A` ⇒ ⇒
      **10 拍后** `out.payload == 0x5A` 且 `out.valid == 1`
      （⇒ ⇒ **手推**：FIFO 深度 4 ⇒ ⇒ 1 拍写入 ⇒ ⇒ ≤4 拍读出）
- [ ] **效果判据**：链长 **2**（`queue<4>().queue<4>()`）⇒ ⇒ 同样**零丢字**
      ⇒ ⇒ 且**逐拍**的 payload 序列 MUST 与链长 1 **一致**
      （⇒ ⇒ 这是「链长无关性」的判据）
- [ ] **效果判据**：调用方 `in.ready` 的 **`num_srcs() > 0`**（⇒ ⇒ 上游
      **真**收到反压）⇒ ⇒ 且「下游满 ⇒ ⇒ 上游 ready 落 0」可被**断言**
- [ ] **变异**：把 `build()` 的延迟连接去掉 ⇒ ⇒ **零丢字**用例 MUST **转红**
      （⇒ ⇒ ⚠️ **先确认编译成功**）
- [ ] **变异**：把中间级连接从 `<<=` 改 `operator=` ⇒ ⇒ MUST **转红**
- [ ] ⚠️ **穷举**：builder 的**全部**链式方法（`queue` / `haltWhen` /
      `continueWhen` / `map` / `throwWhen` / `takeWhen`）⇒ ⇒ 每个都 MUST
      参与「零丢字」判据（⚠️ 「只查已知符号 ⇒ 漏掉同族入口」是本仓登记的
      护栏盲区）
- [ ] `ctest -LE perf` 零回归 · **计数锚点不变**

## 4. Capabilities (契约影响)

- **可能** MODIFIED `chlib-stream-ready-ownership`（若「中间级」属该能力
  的「fifo 类」⇒ ⇒ 需加条款）
- **可能** MODIFIED `chlib-stream-semantics`（fifo 语义）
- ⚠️ 若选修法 B ⇒ ⇒ **必须**新建 ADR（`chlib/fifo.h` 的公开 API 变更）

## 5. Impact (影响面)

- **改动文件**：`include/chlib/stream_builder.h`（私有成员 + `build()` 语义）
  · 可能 `include/chlib/fifo.h`（修法 B）· `tests/test_stream_builder.cpp`
  · 可能 `samples/`（builder 用例）· 护栏脚本
- **爆炸半径**：**中** —— builder 的**公开行为**变化（从「丢字」变「不丢字」
  ⇒ ⇒ ⇒ **既有断言可能依赖旧行为** ⇒ ⇒ ⇒ MUST 逐项复核）
- **风险**：**中** —— ① 与 ADR-054 的张力（**需裁决**）② `build()` 语义变化
  ③ 「零丢字」会**暴露**既有的其它缺陷（⇒ ⇒ 好事，但会增加本项的修复面）
- **回滚路径**：`revert` 实施 commit ⇒ ⇒ 无持久化面
