# 提案：收口「共享守卫贡献」的两个契约缺口 —— 优先级条款 + 析构/接受面条款

- **架构依据**：**ADR-051**（`docs/adr/ADR-051-chlib-conditional-semantics.md`）
  的 **D10** 条款 + **§5.4** 的验证条目 ⇒ 实施期实测订正
- **关联 ADR**：**ADR-050**（`docs/adr/ADR-050-chlib-state-machine-semantics.md`，
  D10 的同型机制 + 金标来源）
- **规格影响**：**MODIFIED ×1 + ADDED ×3**
  - `chlib-state-machine-semantics`：**ADDED ×1**（优先级条款）
  - `chlib-conditional-semantics`：**ADDED ×2**（析构兜底 / 接受面限制）+
    **MODIFIED ×1**（权威表面条款追加接受面 NOTE）
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**小**（**零** `core/` 改动；**零**行为变更）

## Why

ADR-051 D10 把 `ch_state_machine::contrib`（**状态**守卫）与 `_if` 的条件贡献
（**条件**守卫）统一到**同一个**共享组合器
（`include/chlib/guard_contribution.h::compose_guard_chain`）⇒ ⇒ 一处实现、
两处使用 ⇒ ⇒ 语义**不会漂移**。

⚠️ **但 D10 的约束句「抽取是**纯重构**，不是行为变更」经实施期实测**证伪** ——
在**一个维度**上：

| | `is_in(S)` 比较的创建顺序（= 链的内→外） | 同一 target 多状态贡献时**谁**优先 |
|---|---|---|
| **抽取前**（旧内联 `compose()`，正向 state 序迭代） | `3, 2, 1, 0` | **高编号** state（后写的在外层） |
| **抽取后**（共享组合器 `rbegin→rend`） | `0, 1, 2, 3` | **低编号** state（先收集的在外层） |

⇒ ⇒⇒ **ADR-050 的贡献优先级被静默翻转了。**

⚠️ **为什么全绿**：ADR-050 的守卫是**状态守卫**（`is_in(S)` =
`state_reg == ch_uint<STATE_BITS>(S)`）⇒ ⇒ **同一时刻至多一个为真**（互斥是
**构造保证**，不是经验观察）⇒ ⇒ 无论链序如何，结果**逐位相同** ⇒ ⇒
**运行期不可观测** ⇒ ⇒ ⇒ **既有金标 100% 绿** ⇒ ⇒ ⇒ **「绿」完全掩盖了它**。

⇒ ⇒⇒ **这正是「链序对 ADR-050 侧不可观测」的另一面**：不可观测 ⇒ 金标钉不住
⇒ ⇒ **只有结构级断言**能钉住它 ⇒ ⇒ 而本 change **当前没有任何结构级断言**
⇒ ⇒ ⇒ **该属性处于零覆盖状态**。

⇒ ⇒⇒ 三个后果必须收口：

1. **契约缺口**：`chlib-state-machine-semantics` 的 **6** 条 Requirement
   **从未写过优先级** ⇒ ⇒ 翻转**不违反任何契约** ⇒ ⇒ 也就**无处登记**；
2. **文档缺口**：ADR-051 D10 的「纯重构」表述**不成立** ⇒ ⇒ 须订正；
3. **覆盖缺口**：链序**无任何用例**钉住 ⇒ ⇒ 将来若引入**非互斥**守卫（或
   `contrib` 语义变化），**无网可捞**。

### 顺带收口（同一批，`chlib-conditional-semantics`）

4. **析构兜底语义无契约载体**：ADR-051 **SF4** 裁决过「`~conditional_block()`
   兜底调 `end()` ⇒ 抛出路径 MUST 兼容析构（`noexcept(false)` + 展开期不抛）」，
   实现于 `include/chlib/if_stmt.h` ⇒ ⇒ 但主 spec **零**提及 ⇒ ⇒ 未来改析构
   语义**不需要任何 spec delta** ⇒ ⇒ 「把静默变受控」的目的在这条上落空；
5. **表达式形式的接受面限制无契约载体**：主 spec 声明 `multi_if` / `if_then`
   是**唯一权威表面**，却**不记录**该表面的**可用域** ——
   ① 不能直接赋给 `ch_out`（E1）② 分支值须同类型（E2）③ 条件不能是
   `ch_in<ch_bool>` 端口（E3）⇒ ⇒ 这三条只活在 `include/chlib/AGENTS.md` 的
   anti-patterns 段 ⇒ ⇒ **「权威表面」的可用域无契约**。

## What Changes

### 范围内

1. **订正 ADR-051 D10**（+ §5.4 + §7 审稿记录）：
   「抽取是**纯重构**，不是行为变更」⇒ ⇒ 改为**如实**表述：链序（= 贡献优先级）
   维度上**是**行为变更（高编号 state 优先 → 低编号 state 优先）⇒ ⇒ 而
   **新行为符合仓内惯例**（`_if` 与 `multi_if` 均为「先匹配者胜」）⇒ ⇒ 旧行为
   属**偶然反转** ⇒ ⇒ 本次抽取**顺带修掉**该潜伏反转。
   ⚠️ **决策未变**（共享组合器 / 一处实现两处使用 / 零 `core/` 改动）⇒ ⇒ 仅
   **订正事实表述**。
2. **订正 ADR-051 §5.4 的 M6 条目**：补「链序**运行期**不可观测 ⇒ 金标钉不住
   ⇒ ⇒ **必须**结构级断言」这条事实。
3. **契约**（`chlib-state-machine-semantics`）：
   - **ADDED ×1**：「状态守卫贡献的优先级 MUST 是**先匹配者胜**（低编号 state
     优先）」+ Scenario「同一 target 的多状态贡献 ⇒ 低编号 state 优先」+
     Scenario「优先级 MUST 有**结构级**断言钉住（运行期不可观测 ⇒ 金标**不**
     足以钉住它）」；
   - **MODIFIED ×1**（若需要）：把「贡献 API 与手写链」那条的边界补上优先级
     口径（⇒ 贡献 API 与手写 `select` 链的**优先级方向** MUST 一致）。
4. **契约**（`chlib-conditional-semantics`）：
   - **ADDED ×1**：「条件块的析构 MUST 兼容抛出路径」（`noexcept(false)` +
     展开期不抛 + 兜底 `end()`）；
   - **MODIFIED ×1**：「权威表面 MUST 唯一」⇒ ⇒ 追加**接受面限制**的已知限制
     NOTE（E1/E2/E3）⇒ ⇒ 修复后改判据。
5. **实现**：
   - `include/chlib/guard_contribution.h`：头注释明确「先匹配者胜」的**方向**
     （先收集的贡献在最外层）+ 记录「抽取前后的方向差异」；
   - **新增结构级断言**（`tests/chlib/test_guard_contribution.cpp` 或既有
     `test_if_stmt.cpp` 内）：直接检查 `compose_guard_chain` 产出的链**结构**
     —— 最外层 mux 的守卫 MUST 是**第一个**贡献的守卫 ⇒ ⇒ 钉住链序。
6. **簿记**（planner）：本 change 的 improvement 草案 + roadmap 行。

### 范围外

- **不**回退优先级（新行为=保留 ⇒ 见口径裁决）；
- **不**引入**非互斥**守卫（oracle 已排除：当前**不可构造** ⇒ 写了也测不到真路径）；
- **不**修 E1/E2/E3 的**实现**（那是独立 change；但本 change 的 NOTE 会把它们
  的**契约归属**钉在 `chlib-conditional-semantics` ⇒ ⇒ 避免后续 change 用
  「no spec changes」归档 ⇒ ⇒ **契约被静默丢掉**）；
- **不**动 `core/`；**不**动 `chlib/state_machine.h` 的**行为**。

## Acceptance

- [ ] ADR-051 D10 的「纯重构」表述**已订正**为如实表述（**决策未变**），
      且 §5.4 / §7 有对应的**实施期实测**记录；
- [ ] `chlib-state-machine-semantics` 有一条**明确优先级方向**的 Requirement
      + 至少 2 个 Scenario；
- [ ] `chlib-conditional-semantics` 有**析构兜底**条款 + **接受面限制** NOTE；
- [ ] **结构级断言**存在且**能转红**：把 `compose_guard_chain` 的 `rbegin→rend`
      改成 `begin→end` ⇒ ⇒ 该断言**转红**（⇒ 证明链序**真的**被钉住），
      而**运行期**用例**仍绿**（⇒ 证明「运行期不可观测」这条事实）；
- [ ] 既有金标**逐项不变**（`uart_tx` 的 `busy` 高 **3906** 拍 + `0xA5` 位序；
      `complex_e2e` 的 `0x96` MSB-first 16 位 + 状态序列 + `bit_count` 复位）；
- [ ] `ctest -LE perf` 零回归；全部护栏（含 `--self-test`）exit 0；
- [ ] 计数锚点核对（新增用例进**既有** target ⇒ ctest **条目数**不变）；
- [ ] 归档（**禁** `--skip-specs` ⇒ 两个 capability 的 delta 合并）。

## Capabilities

- **ADDED ×3 + MODIFIED ×1** ⇒ 归档**禁** `--skip-specs`。

## Impact

- **改动文件**：`docs/adr/ADR-051-chlib-conditional-semantics.md` ·
  `include/chlib/guard_contribution.h`（注释）· `tests/chlib/test_if_stmt.cpp`
  或**新**测试文件 · `openspec/specs/**`（归档时合并）· `.rddf/**` 簿记；
- **计数**：**不变**（用例进既有 target ⇒ ctest 条目数不变）；
- **爆炸半径**：**极小**（**零** `core/` 改动 · **零**行为变更）；
- **风险**：**低** —— 唯一的实质动作是**改文档表述** + **加断言** + **加契约**；
  ⚠️ 风险点是「订正已签批 ADR 的措辞」⇒ ⇒ 只订正**事实表述**、**不动决策**
  ⇒ ⇒ 且在 §7 透明标注。

## Non-Goals

- **不**回退优先级（新行为保留）；
- **不**改 `core/`；
- **不**修 E1/E2/E3 的实现；
- **不**引入非互斥守卫。
