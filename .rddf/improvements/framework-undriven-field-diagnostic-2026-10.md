---
name: framework-undriven-field-diagnostic-2026-10
priority: P1
source: `chlib-stream-ready-ownership-2026-10` 的实施期证伪（design.md §9.1）
phase: phase-5
category: core
type: feature
created_at: 2026-10-31
---

# Improvement: 框架层「**读未驱动字段**」的**诊断**能力

- **Date drafted**: 2026-10-31
- **Drafted by**: build-agent（builder role）
- **Status**: 📋 5-segment draft，**决策权归 planner / architect**
  （⚠️ 本草案**只登记实测事实与范围界定**，**不**替 architect 裁决方案）
- **来源**：change `chlib-stream-ready-ownership-2026-10` 实施期**证伪**
  （`design.md` §9.1）⇒ ⇒ critic 判 **REJECT** 时明确建议「**独立立项**」
  （耦合进 stream 改动会把 chlib 改动变成 `core/` 语义变更 ⇒ 越界）。

## 1. Why (背景与动机)

🔴 **实测事实**（2026-10-31，实施形态 B 时撞出）：

```cpp
// 框架对「读一个**未被驱动**的字段/代理」的处理
auto r = stream_fifo<...>(in);   // 形态 B：结果对象此时**不完整**
// ⇒ 不调 connect_ready ⇒ r.pop_stream.payload / r.full / r.empty 全**未驱动**
// ⇒ 读取它们 ⇒ 得到 **0**（实测：tests/chlib/test_stream_fifo_dual.cpp:145
//   期望 out_empty == 1 ⇒ 实测得 0）
// ⇒ 且 tests/chlib/test_stream.cpp:470 在该形态下**直接 SIGABRT**
```

⇒ ⇒ **框架对「读未驱动字段」返回 0，且不报错** ⇒ ⇒ **静默**。

⚠️ 这直接击穿了本仓的总原则「**要么语义正确、要么响亮拒绝**」
（ADR-052 的 D 总原则）⇒ ⇒ 「静默失效」是本仓反复出现的病症（`ch_in` 的
`operator=`、未驱动代理节点、当前这一条 ⇒ ⇒ **第 3 个实例**）。

⚠️ **这不是 stream 专属**：任何「**未驱动**的 bundle 字段 / 代理节点 / 输出端口」
在读取时都可能拿到 0 ⇒ ⇒ ⇒ 该缺口是**框架层通用**的 ⇒ ⇒ **必须独立立项**
（⇒ ⇒ 改的是 `core/` 的读取语义 ⇒ ⇒ 爆炸半径 = **全框架**的代理 / 字段读路径）。

## 2. What Changes (候选范围 —— **待 architect 裁决**)

⚠️ **本草案不选方案** ⇒ ⇒ 只界定**问题边界**与**候选方向** ⇒ ⇒ 方案选择须先有 ADR。

**问题边界**（三者之一或组合）：
1. **诊断式**：读未驱动字段时**报错 / 抛异常**（而非返回 0）⇒ ⇒ 需一个
   「该节点当前**无驱动**」的**可判定**判据 ⇒ ⚠️ 难点：框架当前**不跟踪**
   「某节点是否已被驱动」（`set_src` / `set_driver` 之后是否**永远**有源？
   ⇒ ⇒ 有源但**未连到任何顶层输入**的节点**算不算**「未驱动」？）；
2. **标注式**：提供**显式**的「未驱动」查询 API（`impl()->has_driver()` 之类）
   ⇒ ⇒ 由**调用方 / 护栏**在合适时机断言 ⇒ ⇒ 框架**不改**默认读取语义
   ⇒ ⇒ **爆炸半径最小**；
3. **编译期式**：让「未驱动即不可构造」成为**类型/编译期**约束 ⇒ ⇒ ⚠️ 需
   改变 bundle 字段与代理的类型系统 ⇒ ⇒ **爆炸半径最大**。

⚠️ **已知约束**（来自实测）：
- ⚠️ 「有源但未连到顶层」的节点**无法**与「完全无源」区分（现状）⇒ ⇒ 方向 1/3
  需要**新增跟踪机制**（⇒ ⇒ 改 `core/`）；
- ⚠️ 方向 2（标注式）**不改变**任何既有行为 ⇒ ⇒ 对现有 194 个测试**零回归**
  ⇒ ⇒ **成本最低、风险最小**；
- ⚠️ 任何方案 MUST 保住本仓既有约定：`CHERROR` / `CHFATAL` **只打日志不抛**
  ⇒ ⇒ 若要「响亮」⇒ ⇒ MUST 用 **`CHFATAL_EXCEPTION`**（ADR-050 已踩过此坑）。

## 3. Acceptance (验收标准)

- [ ] 有一个**可执行**判据证明「读未驱动字段**不再静默返回 0**」
      （或「诊断 API 能正确报告未驱动」）；
- [ ] 该判据 MUST 有**变异**证明判别力（M：让「未驱动」看起来像「已驱动」⇒ 转红）；
- [ ] 若选方向 2/3：`ctest -LE perf` **零回归**（现有 194 个测试全绿）；
- [ ] 契约载体：capability **归属** MUST 先经 architect 裁决
      （候选：`core-node-driver-visibility` 之类的新 capability）
      ⇒ ⇒ 归档**禁** `--skip-specs`；
- [ ] ⚠️ **若**最终选方向 1/3 ⇒ ⇒ MUST 先评估对 `core/` 读取语义的影响面
      ⇒ ⇒ 并同步 `include/core/AGENTS.md` 与根 `AGENTS.md` 的契约段。

## 4. Capabilities

- **候选**新建 capability（**名字待 architect 定**）⇒ ⇒ 归档**禁** `--skip-specs`；
- ⚠️ **不** MODIFIED 既有 capability（除非 architect 判定应改某条既有 MUST）。

## 5. Impact (影响面)

- **改动文件**（方向 2，最小面）：`include/core/` 的节点实现（新增查询）
  + `include/core/AGENTS.md` + 可能一个新头；
- **改动文件**（方向 1/3）：`core/` 的**读取语义** ⇒ ⇒ 波及**全框架**的
  代理 / 字段读路径 ⇒ ⇒ **高风险**；
- **爆炸半径**：方向 2 = **小**（新增 API，不改语义）；方向 1/3 = **大**；
- **回滚路径**：方向 2 = 删新增 API（零行为影响）⇒ ⇒ **极易回滚**；
- ⚠️ **与 stream change 的关系**：**解耦** ⇒ ⇒ `chlib-stream-ready-ownership`
  采用**形态 E**（默认值 + 可覆盖 ⇒ 不依赖本草案）⇒ ⇒ 两者**可并行**。
