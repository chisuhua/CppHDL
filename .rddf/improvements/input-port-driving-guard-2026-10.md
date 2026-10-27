---
name: input-port-driving-guard-2026-10
priority: P1
source: 2026-10-27 两轮 critic 的反复指向（Batch 2/3 的 M3/M3' + AGENTS.md 登记）
phase: phase-5
category: testing
type: research
created_at: 2026-10-27
---

# Improvement: 「新增输入端口 MUST 被显式驱动」MUST 有自动化护栏（**先做可行性 spike**）

- **Date drafted**: 2026-10-27
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，归属 `feat-dual-backend-coverage` 的**工程纪律**项
- **来源**：2026-10-27 两轮审稿的反复指向 ——
  ① Batch 2/3 的第 1 轮 BLOCKER = 「第 1 拍前只驱动了 6/7 个输入端口」；
  ② M3' 实测**零判别力**（删掉那行驱动仍全绿）⇒ 如实记录「**未驱动端口没有通用的
  变异判别力**」；
  ③ 根 `AGENTS.md §ANTI-PATTERNS` 登记：「⚠️ 当前**无自动化护栏**覆盖这一类
  （`scripts/verify_rst_n_wiring.sh` 只覆盖 `rst_n`）」。

## 1. Why (背景与动机)

未驱动的输入端口读到的是**未定义值**（实测：`strbe` 读**全 1**、`rst_n` 读 0）
⇒ 依赖默认值的测试会「**碰巧**通过」而跨后端行为可能分叉。

⚠️ **但静态分析这一类护栏有明确的前车之鉴**：
`scripts/verify_example_assertion_quality.sh` 的**自研 C 子集分析器**
「**不可能被完全闭合**」⇒ 对抗审稿第 4/5/6 轮的**全部** BLOCKER 都出在它身上
⇒ **误报**是这类护栏的主要失败模式 ⇒ 直接开写护栏的风险很高。

## 2. What Changes (变更范围)

**本 change 只做可行性 spike（不改实现）**，产出「可行性报告」：

- **需要解析什么**：从 C++ 测试源码判定「某组件的某个 `ch_in` 端口是否被驱动」
  ⇒ 表达式解析（`sim.set_input_value(dev.instance().io().x, …)` /
  `io().x <<= …` / fixture 里的统一驱动 helper）⇒ **C++ 表达式的任意性是核心难点**；
- **候选设计**（各自评估成本 / 误报面 / 可测性）：
  1. **显式登记表**：仓库内维护「组件 → 必须驱动的端口列表」+ grep 式检查
     （成本最低、误报最低，但**登记表本身可能过时** ⇒ 需要「登记表 ⊆ 组件实际端口」的
     交叉校验）；
  2. **C++ 解析**（clang-based / libclang）：最准，但引入新依赖 + 解析器自身的坑；
  3. **运行期检查**：在 `Simulator` 侧记录「本 context 内被驱动过的输入端口」，
     在 `tick()` 时报告**从未被驱动**的输入端口 ⇒ **零解析成本**、**零误报**
     （只报「确实没驱动」的），但需要框架层加 API（⇒ 触及公共 API 面 ⇒ 需 ADR）。
- ⚠️ **决策门**：若结论是「无法在可接受误报率内实现」⇒ **记录并关闭**，
  把该义务**留在 `AGENTS.md §ANTI-PATTERNS`**（人工核对 + 代码评审）
  ⇒ **不得**硬造一个误报率高的护栏。

## 3. Acceptance (验收标准)

- [ ] 可行性报告给出 **2–3 个候选设计**的成本 / 误报面 / 可测性对比
- [ ] 明确**推荐方案**与**推荐理由**（含「为什么不是另外两个」）
- [ ] 明确「若实施 ⇒ 需要哪些前置」（ADR？框架层 API？CI 挂载点？）
- [ ] 给出**决策门结论**：实施 / 关闭（并把结论写进根 `AGENTS.md`）
- [ ] **无代码改动**（spike 只出文档）

## 4. Capabilities (涉及的能力维度)

- **不涉及**（spike 不改契约）⇒ 本 change **无 spec 变更** ⇒ 归档用 `--skip-specs`
- 若后续实施 ⇒ 可能涉及 `dual-backend-test-coverage`（端口驱动纪律条款）

## 5. Impact (影响面分析)

- **改动文件**：**无代码改动** ⇒ 只有本 change 目录内的文档
- **计数**：**不变**
- **风险**：**低**（只读）—— 价值在于**避免**开写一个高误报护栏
