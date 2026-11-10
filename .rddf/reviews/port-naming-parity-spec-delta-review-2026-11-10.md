# 复核记录：端口命名的两份 spec ADDED delta（2026-11-10 · architect）

> **复核对象**：`.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 的
> **R1-a**（→ `rtl-hierarchical-output`）与 **R1-b**（→ `component-ownership-and-naming`）
> **复核者**：architect 角色 · **轮次**：1（本批 delta 的首轮 · 非 ADR 级）
> **判据面**：`prompts/critic.md` 的「arch — BLOCKER / SHOULD_FIX」摘要逐条自检
> （BLOCKER 四类 · SHOULD_FIX 六类）⇒ ⇒ **本记录是自检记录，不是 critic 输出**

---

## 0. ⚠️ 审查者启用情况（如实登记）

| 审查者 | 是否启用 | 原因 |
|---|---|---|
| **oracle** | ❌ **未启用独立复轮** | ⚠️ **未启用 oracle 审查（原因：delta 文本本身 = oracle 建议 1/2/3 的直译 + 用户 2026-11-10 拍板 ⇒ ⇒ 再起一轮 = 自我复审，无新信息）** ⇒ ⇒ **已用内置自检清单代替**（§1–§3） |
| **critic** | ⏸ **待用户批准**（第 3 轮 · 预算 2/2 已用满） | 见 `.rddf/roadmap/phase-6-rtl-productization.md` §6 ⑦⑧ |

---

## 1. R1-a（`rtl-hierarchical-output` · ADDED 1 Requirement / 2 Scenario）逐条自检

### BLOCKER 四类

| # | 判据 | 结论 |
|---|---|---|
| 1 | **与已签批 ADR / spec 冲突且未走取代流程** | ✅ **不冲突**。既有 `spec.md:309-316` 的 Scenario 定的是**端口集合**（clock → reset → 无 driver 的 input → **全部** `ch_out`）；R1-a 定的是**每个端口的元信息可解析性** ⇒ ⇒ 两者**正交**（集合 vs 元信息）⇒ ⇒ ⇒ 且 R1-a 是**追加**，**未触碰** `:309-316` |
| 2 | **决策缺备选方案 / 取舍理由 / 回滚路径** | ⚠️ **N/A（这是 Requirement 不是 ADR）** ⇒ ⇒ 但「SHOULD 而非 MUST」的**取舍理由**已在 §A.3 / §裁决记录 ② 写明（三条：结构上不可满足 / 扁平单一命名空间 / 真实缺陷是「名字无语义且不稳定」）⇒ ⇒ **有据** |
| 3 | **文档内对同一决策自相矛盾** | ✅ **无**。MUST 用 `cpp_member`；SHOULD 才谈字面相等 ⇒ ⇒ 二者**不重叠**（MUST 不要求字面相等） |
| 4 | **把未决问题写成已决** | 🟡 **一处需登记（不是 BLOCKER，是依赖）**：R1-a 引用的 `cpp_member` / `is_top_io` **由 `rtl-emit-portability` 承载，而该 capability 尚未实施**（`rtl-emit-product-tooling` 的 C4 仍在 draft）⇒ ⇒ ⇒ **契约先于实现** ⇒ ⇒ ⇒ 本仓**有先例**（台账 §7 行 15「契约已定 · 实现未达」）⇒ ⇒ ⇒ **接受，但 MUST 在台账/草案里如实登记** ⇒ ⇒ 该依赖已由 `rtl-emit-product-tooling` 的 C4 AC 跟踪 ⇒ ⇒ ⇒ **不新增台账行**（避免行数膨胀） |

### SHOULD_FIX 六类

| # | 判据 | 结论 |
|---|---|---|
| 1 | 决策陈述被稀释（正文超长 / 决策+后果+备选不足一半） | ✅ 两段 Requirement，正文短 |
| 2 | 过程日志混入决策文档 | ✅ 无 |
| 3 | 缺独立「后果 / 代价」章 | N/A（spec Requirement 无此章） |
| 4 | Verification 只写实测日志而无前瞻判据 | ✅ 两个 Scenario 都是**前瞻判据**（WHEN/THEN） |
| 5 | 元数据头膨胀 | N/A |
| 6 | 一篇承载多个独立决策点 | ✅ **一个**决策（作用范围 = T1 / 字面同名 = 条件化 SHOULD）⇒ ⇒ 两个 Scenario 是**同一决策的两面** |

⇒ ⇒ **R1-a 结论：APPROVE**（无 BLOCKER；4 号依赖已登记去向）。

---

## 2. R1-b（`component-ownership-and-naming` · ADDED 1 Requirement / 2 Scenario）逐条自检

### BLOCKER 四类

| # | 判据 | 结论 |
|---|---|---|
| 1 | **与已签批 ADR / spec 冲突** | ✅ **不冲突**。既有 `:62`「其 Verilog 名 MUST 含该组件的路径段」与 R1-b「子路径 MUST 由父路径 + `.` + 局部名构成」**同向**（都要求路径段存在）⇒ ⇒ ⇒ R1-b **正是**修那处口径不一致 |
| 2 | 缺取舍理由 | ✅ 理由在 `node-name-prefix-consistency` §A.2（方向 (i) 撞 `:62` ⇒ 取 (ii)） |
| 3 | 自相矛盾 | ✅ 无 |
| 4 | 未决写成已决 | ✅ 无（方向 (i) 已登记为**未决残差** = 台账行 21） |

### SHOULD_FIX 六类 + **一处实缺陷**

| # | 判据 | 结论 |
|---|---|---|
| 1 | 决策陈述被稀释 | ✅ |
| 2 | 过程日志混入 | ✅ |
| 3 | 缺「后果 / 代价」章 | N/A |
| 4 | Verification 只写实测日志 | ✅ Scenario 为前瞻判据 |
| 5 | 元数据头膨胀 | N/A |
| 6 | 多个独立决策点 | ✅ 一个 |
| **7** | 🔴 **契约完整性**：**原 R1-b 只写了「有父」的情形** | 🔴 **实缺陷**（architect 复核发现）：**根组件（`parent == nullptr`）的 `name()` == `local_name()` 未被覆盖** ⇒ ⇒ ⇒ 契约不完整 ⇒ ⇒ 且「根组件的子节点走哪套口径」也悬空 ⇒ ⇒ ⇒ **已补** `Scenario: 无父的组件实例路径等于其局部名`（含「其子节点 MUST 以 `<local_name> + "."` 为前缀」） |

⇒ ⇒ **R1-b 结论：APPROVE_WITH_FIX**（1 处已修 ⇒ ⇒ 修后无 BLOCKER）。

---

## 3. 对「不可违反的 MUST」的专项检查（本项的核心风险）

本批 delta **必须**避免 sister 草案自己定的纪律所禁止的形态：
「**每条 MUST 必须有可构造的违反样例**」⇒ ⇒ 写不出样例的 MUST = 不可满足 / 不可验证 ⇒ ⇒ 禁写。

| 条款 | 可违反样例 | 判定 |
|---|---|---|
| R1-a MUST「每个端口 MUST 能被解析到一个 C++ 成员名」 | 某端口不出现在 manifest ⇒ ⇒ 违反 | ✅ 可构造 |
| R1-a MUST「MUST NOT 依赖端口名与成员名的字面相等」 | 某下游工具按字面相等匹配 ⇒ ⇒ 违反（**行为可观测**） | ✅ 可构造 |
| R1-a SHOULD「逐字相同」 | 不满足条件 ④ 的成员名 ⇒ ⇒ SHOULD 不适用（⇒ **不是**违反） | ✅ SHOULD 不制造常态违约 |
| R1-a MUST NOT「同名兄弟 / 子组件端口 / 关键字 / 数字开头 MUST NOT 被承诺逐字相同」 | 文档或工具承诺它 ⇒ ⇒ 违反 | ✅ 可构造 |
| R1-b MUST「`name()` == `<P> + "." + <L>`」 | 删掉 `component.h:274` 之外**再**加特判 ⇒ ⇒ 违反 | ✅ 可构造 |
| R1-b MUST「MUST NOT 因 `<P>` 的字面值剥离路径段」 | 把 `\|\| parent_path == "unnamed"` 加回去 ⇒ ⇒ **立即违反** | ✅ 可构造（**这就是本项的判据**） |
| R1-b MUST「无父 ⇒ `name()` == `local_name()`」 | 给根也拼前缀 ⇒ ⇒ 违反 | ✅ 可构造 |
| 「「无名」是**普通名字**，不是「无路径段」标记」 | ⚠️ **它是陈述句，不带 MUST** ⇒ ⇒ 不可违反 ⇒ ⇒ **正确地未写成 MUST** | ✅ |

⇒ ⇒ **专项检查通过**：无不可违反的 MUST。

---

## 4. 遗留与去向

| # | 事项 | 去向 |
|---|---|---|
| 1 | `cpp_member` / `is_top_io` 的**实现**尚未落地（`rtl-emit-portability` 仍是 draft）⇒ ⇒ 契约先于实现 | 由 `rtl-emit-product-tooling` 的 **C4** AC 跟踪 ⇒ ⇒ 本仓**有先例**（台账行 15「契约已定 · 实现未达」）⇒ ⇒ **不新增台账行** |
| 2 | 方向 (i)（让 `lnodeimpl` 也剥哨兵 ⇒ 撞 `:62`）未决 | 台账 §7 **行 21**（★ 已落地 · 双向 1:1 PASS / 22 行） |
| 3 | 两份 delta 的**落地形态** = openspec **change**（根 `AGENTS.md §OPENSPEC WORKFLOW`：Spec 变更 MUST 走 change 流程）⇒ ⇒ 归档时由 CLI 合并 | planner（change 目录 + proposal）/ builder（delta spec 文件）⇒ ⇒ **architect 只复核内容**（本记录即复核） |
| 4 | critic 第 3 轮 | **待用户批准**（预算 2/2 已用满） |

---

## 5. 复核结论

| delta | 结论 |
|---|---|
| **R1-a** → `rtl-hierarchical-output` | ✅ **APPROVE**（1 处依赖已登记去向） |
| **R1-b** → `component-ownership-and-naming` | ✅ **APPROVE_WITH_FIX**（已补「无父」Scenario ⇒ ⇒ 修后无 BLOCKER） |

⇒ ⇒ **两份 delta 内容可进入 openspec change 流程**。
⇒ ⇒ ⚠️ **本记录不是 critic 输出** ⇒ ⇒ critic 第 3 轮仍需用户批准后才跑（届时 scope 只含「上轮 BLOCKER 闭合 + 新 diff」）。
