---
name: chlib-guard-contribution-priority-2026-10
priority: P0
source: ADR-051 D10 实施期实测（共享守卫贡献的**优先级**被静默翻转）
phase: phase-5
category: chlib
type: bugfix
created_at: 2026-10-30
---

# Improvement: 共享守卫贡献的**优先级** MUST 有契约 + 有结构级断言

- **Date drafted**: 2026-10-30
- **Drafted by**: build-agent（planner role）
- **Status**: ✅ **已立项**（2026-10-30）⇒ change
  `openspec/changes/chlib-guard-contribution-priority-2026-10/`
- **归属 feature**：**待裁决** —— `feat-dual-backend-coverage` 的主题是「双后端
  覆盖」，与本项**不**直接相关 ⇒ ⇒ 本项暂挂 phase-5（归属待 planner 裁决）
- **上游**：ADR-051（`docs/adr/ADR-051-chlib-conditional-semantics.md`）的
  **D10** 条款 + **§5.4** 验证条目 ⇒ 实施期实测订正

## 1. Why (背景与动机)

ADR-051 D10 把 ADR-050 的 `ch_state_machine::contrib`（**状态**守卫）与 `_if`
的条件贡献（**条件**守卫）统一到**同一个**共享组合器
（`include/chlib/guard_contribution.h::compose_guard_chain`）⇒ ⇒ 一处实现、
两处使用 ⇒ ⇒ 语义**不会漂移**。

⚠️ **但 D10 的约束句「抽取是**纯重构**，不是行为变更」经实施期实测**证伪** ——
在**链序（= 贡献优先级）**这一个维度上：

| | `is_in(S)` 比较的创建顺序（= 链的内→外） | 同一 target 多状态贡献时**谁**优先 |
|---|---|---|
| **抽取前**（旧内联 `compose()`，正向状态序迭代） | `3, 2, 1, 0` | **高编号** state（后写的在外层） |
| **抽取后**（共享组合器 `rbegin→rend`） | `0, 1, 2, 3` | **低编号** state（先收集的在外层） |

⇒ ⇒⇒ **ADR-050 的贡献优先级被静默翻转了。**

⚠️ **为什么全绿**：状态守卫 `is_in(S)` = `state_reg == ch_uint<STATE_BITS>(S)`
⇒ ⇒ 同一寄存器对**互不相同**的枚举字面量作比较 ⇒ ⇒ **互斥是构造保证**
⇒ ⇒ 同一时刻至多一个守卫为真 ⇒ ⇒ 无论链序如何，结果**逐位相同**
⇒ ⇒ ⇒ **运行期不可观测** ⇒ ⇒ ⇒ **既有金标 100% 绿** ⇒ ⇒ ⇒ **「绿」完全
掩盖了它**。

⇒ ⇒⇒ 三个后果必须收口：

1. **契约缺口**：`chlib-state-machine-semantics` 的 **6** 条 Requirement
   **从未写过优先级** ⇒ ⇒ 翻转**不违反任何契约** ⇒ ⇒ **无处登记**；
2. **文档缺口**：ADR-051 D10 的「纯重构」表述**不成立** ⇒ ⇒ 须订正；
3. **覆盖缺口**：链序**无任何用例**钉住 ⇒ ⇒ 该属性**零覆盖**。

### 顺带收口（同一批，`chlib-conditional-semantics`）

4. **析构兜底语义无契约载体**（ADR-051 SF4 的裁决，主 spec 零提及）；
5. **表达式形式的接受面限制无契约载体**（E1/E2/E3 只活在 `include/chlib/AGENTS.md`）。

## 2. What Changes (变更范围)

- **ADR-051 D10 / §5.4 / §7**：订正「纯重构」表述（**决策未变**，仅订正**事实
  表述**）+ 记录「链序运行期不可观测 ⇒ 需结构级断言」；
- **契约**：`chlib-state-machine-semantics` **ADDED ×1**（优先级条款）·
  `chlib-conditional-semantics` **ADDED ×2**（析构兜底）+ **MODIFIED ×1**
  （权威表面追加接受面限制 NOTE）；
- **实现**：`include/chlib/guard_contribution.h` 头注释（**零行为变更**）+
  **结构级断言**（新 `tests/chlib/test_guard_contribution.cpp`）。

⚠️ **口径裁决**（用户，2026-10-30）：优先级**保留新行为**（=「先匹配者胜」
= 低编号 state 优先）⇒ ⇒ 理由：`_if` 与 `multi_if` 已是该惯例 ⇒ 若回退旧行为
⇒ ⇒ 共享组合器的「先匹配者胜」文档对 ADR-050 侧就是**错的** ⇒ ⇒ 与 D10 的
初衷**矛盾** ⇒ ⇒ 旧行为属**偶然反转**。

⚠️ **不在范围**：回退优先级 · 引入非互斥守卫（oracle 已排除：当前**不可
构造**）· 修 E1/E2/E3 的**实现**（独立 change）· 动 `core/`。

## 3. Acceptance (验收标准)

- [ ] ADR-051 D10 的「纯重构」表述**已订正**（**决策未变**），§5.4 / §7 有
      **实施期实测**记录；
- [ ] `chlib-state-machine-semantics` 有**明确优先级方向**的 Requirement
      + ≥2 个 Scenario；
- [ ] `chlib-conditional-semantics` 有**析构兜底**条款 + **接受面限制** NOTE；
- [ ] **结构级断言**存在且**能转红**（翻转 `rbegin→rend` ⇒ 转红）；
- [ ] **反向判据**成立：同一变异下**运行期**用例**仍绿**（⇒ 证明「运行期
      不可观测」这条事实）；
- [ ] 既有金标**逐项不变**；
- [ ] 变异注入器满足**三处硬化**（`--no-tests=error` · target ⊆
      `build_targets` 的 fail-closed 校验 · 还原后与 `git show HEAD:` 比对）；
- [ ] 计数锚点**不变**（用例进**既有** target ⇒ ctest **条目数**不变）。

## 4. Capabilities (涉及的能力维度)

- **ADDED ×3 + MODIFIED ×1** ⇒ 归档**禁** `--skip-specs`；
- 涉及 `chlib-state-machine-semantics` + `chlib-conditional-semantics`
  ⇒ ⇒ **零** `core/` 改动 ⇒ ⇒ `dual-backend-test-coverage` 等**零 MODIFIED**。

## 5. Impact (影响面分析)

- **改动文件**：`docs/adr/ADR-051-chlib-conditional-semantics.md` ·
  `include/chlib/guard_contribution.h`（注释）· **新**
  `tests/chlib/test_guard_contribution.cpp` · `openspec/specs/**`（归档合并）·
  `.rddf/**` 簿记；
- **爆炸半径**：**极小**（**零** `core/` 改动 · **零**行为变更）；
- **计数**：**不变**；
- **风险**：**低** —— 唯一实质动作是**改文档表述** + **加断言** + **加契约**；
  ⚠️ 风险点 = 「订正已签批 ADR 的措辞」⇒ ⇒ 只订正**事实表述**、**不动决策**
  ⇒ ⇒ 且在 §7 透明标注。
