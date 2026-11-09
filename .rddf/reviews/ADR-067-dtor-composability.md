# ADR-067 审稿记录 —— `chlib` 析构异常规格 vs 类型可组合性

- 被审对象：`docs/adr/ADR-067-dtor-exception-spec-composability.md`
- `review_profile`：`arch`
- 审稿链路：**oracle 1 轮** → **critic 1 轮**（REJECT · 1 BLOCKER）→ **critic 复审 1 轮**（`CLOSED` · 新 BLOCKER `NONE` · `mergeable: yes`）
- 裁决状态：**Proposed**（双审已过 ⇒ ⇒ **待用户拍板** ⇒ 拍板后转 `Accepted`）
- 输入草案：`.rddf/improvements/chlib-dtor-noexcept-composability-2026-11.md`（planner 域 · metis 1 轮 12 finding 逐条处置）
- 上游 issue：https://github.com/chisuhua/CppHDL/issues/28

---

## 1. oracle 第 1 轮（6 条 finding · **全部核实为真** · 已逐条修正）

| # | finding | 核实 | 处置 |
|---|---|---|---|
| R1 | **事实错误**：「既有 5 处实例化」与仓内实况不符（实测 **8 处**对象创建点 + 3 处 `using` 别名） | ✅ 真 | §2 决策 ② 与 §4 正面后果改为 **8 处**并逐条列出文件行号（`uart_tx.cpp:71` · `test_state_machine.cpp:104,172,235,255` · `test_state_machine_dual.cpp:85,117` · `test_verilator_complex_e2e.cpp:189`） |
| R2 | **决策 ③ 的否决理由不准确**：`if_stmt.h:179-204` 显示 `end()` 的「做事（合成+写入）」与「抛（用法错误检出）」本就是分离的 ⇒ 「可组合形态」**可行** ⇒ 「改 `noexcept` = 静默丢功能」站不住 | ✅ 真 | 决策 ③ 改为「**可行但当前不做**」+ 三条真实理由（无需求 / 固定成本高 / 基线缺失）；§1.3 表格的「改 noexcept 的后果」列同步订正；★ 并登记「防止后人把『不可行』当事实继承」 |
| R3 | **决策对 D 的否决理由第一条字面不成立**（契约只禁「按值成员/基类」，守卫是局部）⇒ 真正的判据 ADR 没写 | ✅ 真 | §1.4 的「互斥」加限定语「**在同一析构函数上**」；§3 的 D 行改为 5 条净判据（成员持有场景**结构性无效** / 嵌套受 `uncaught_exceptions()` 门影响**静默** / 检出不幂等 / 收益被决策 ② 严格覆盖 / `guard` 撞 `guard_contribution.h` 语义） |
| R4 | 决策 ⑦ 的「成本 > 收益」低估了难度 | ⚠️ 部分真 | 补**结构事实**：`Component::build_internal`（`src/component.cpp:318-339`）的 `describe()` 是最后一步 ⇒ **局部 FSM 在 describe() 尾部即销毁** ⇒ 「describe() 之后扫描」**不可行**；并说明 `if_stmt.h:109` 的先例是**激活栈**形态 ⇒ 不可复用 ⇒ 理由由「成本 > 收益」改为「结构上需重设检出时机」 |
| R5 | 「默认形态零变更」的下游成立性 | ✅ 基本成立 | ADR §4 风险已登记唯一硬断点（`template<template<class,size_t> class>` 绑定）；补「按值传参 / 容器持有」的拷贝地雷 |
| R6 | 终止兜底无未识别的误报路径；假阳性窗口比 ADR 描述窄 | ✅ 真 | 决策 ⑨ 精确化：唯一可达窗口 = 「`contrib` 后、`resolve` 前被拷贝」；`Component` 移动构造对**空表**成员的退化拷贝发生在 describe() **之前** ⇒ 不可达；后续项理由改为「浅拷贝节点句柄 = **语义地雷**」 |

★ **oracle 明确确认成立的方向**：默认形态「全员迁移到显式检查」违反 ADR-050 D5 的「自保护优于纪律」；
`std::optional<Sm>` 形态（实测 `is_nothrow_destructible_v` 为真）比「编译期撞墙」更危险；
「elaboration 期扫描不存在便宜变体」；「`conditional_block` 可组合形态 = **可行**（被否决的是 ADR 给它的**理由措辞**）」。

---

## 2. critic 第 1 轮（`REJECT` · 1 BLOCKER · 5 SHOULD_FIX · 3 NIT · `scope_exceeded: false`）

### 2.1 BLOCKER（已闭合并经复审确认）

| # | finding | 处置 | 证据 |
|---|---|---|---|
| **B1** | `static_assert(!std::is_nothrow_destructible_v<std::optional<Sm>>)` 与 ADR §1.2 的实测事实**直接矛盾** ⇒ 该判据**恒假** ⇒ 「绿」永不可达（BLOCKER 第 ③ 类：文档内自相矛盾） | 改为**合取**形态：`static_assert(!is_nothrow_destructible_v<Sm>)` **且** `static_assert(is_nothrow_destructible_v<std::optional<Sm>>)` ⇒ 把「静默降级」钉成「**落差**」而非「取反」；**同一错误已传播到 planner 草案 ⇒ 同改** | 实测：`/tmp/opencode/issue28/fixcheck.cpp`（合取形态）**exit 0**；`/tmp/opencode/issue28/wrongcheck.cpp`（旧写法）**`static assertion failed`** ⇒ ⇒ BLOCKER 成立且已修 |

### 2.2 SHOULD_FIX（5 条 · 全部处置）

| # | finding | 处置 |
|---|---|---|
| S1 | 元数据写两个 capability 均「MODIFIED」，但决策 ③ 明确**维持** `noexcept(false)` MUST ⇒ 实施方无法判断到底改不改 | 元数据改为**操作分解**：`chlib-state-machine-semantics` = **ADDED**（该 spec「析构」0 次 ⇒ 无 requirement 可 MODIFIED）· `chlib-conditional-semantics` = **ADDED**（使用约束）+ 既有 MUST **零改动**（决策 ③ = 维持）⇒ **无 MODIFIED delta** ⇒ 理由记录在决策 ③ + change 的 `design.md` |
| S2 | 决策 ② 未裁定「`catch` 后不 resolve ⇒ 析构兜底是否照常 terminate」⇒ 实施期两种写法都「符合 ADR」 | 决策 ② 语义新增第 4 条：**检出非幂等**（fail-closed 的必然结果）⇒ `catch` 后不 resolve ⇒ 兜底**照常 terminate** ⇒ **MUST NOT** 引入「已检出 ⇒ 豁免」标志位 ⇒ 若 `catch` 后继续 resolve ⇒ 兜底自然不触发（**推荐的恢复路径**） |
| S3 | planner 草案 ② 的 Impact（「若加 API，MUST 顺手 `= delete` 拷贝」）与 ADR 决策 ⑨（**不删**拷贝）**直接冲突** ⇒ builder 照草案做就推翻已签批裁定 | 草案 ② Impact 改为对齐决策 ⑨（不删拷贝 + 头注释警告 + 「删拷贝」列独立后续项） |
| S4 | planner 草案 ① 预设了**不存在的 `ADR-068`** ⇒ 隐性依赖一个无归属、无立项路径的架构工件 | 草案 ① 改为「适用范围已由**本 change** 收窄」+ 显式禁止预置 ADR 编号 + 「若实施中发现需架构裁决 ⇒ **升级**为 ADR 并登记」 |
| S5 | ADR 缺**显式回滚句**（arch 判据 ② 的回滚要素） | §4 新增「**回滚路径**」小节：纯增量（新模板实参带默认值 ⇒ 既有 8 处类型身份不变）⇒ **回滚 = 还原该 change** ⇒ 无数据迁移 / 无序列化变更 / 无 `core/` 契约变更 |

### 2.3 NIT（3 条 · 全部处置）

| # | finding | 处置 |
|---|---|---|
| N1 | 决策 ② 表的可组合形态行只写「显式 `check_unresolved()`」⇒ 读者会以为析构期零检出 | 表内改为「**① 显式 `check_unresolved()`（可捕获）② 析构期 terminate 兜底（不可捕获 · fail-closed）**」 |
| N2 | 「命名由实施期校准」本身无验收锚点 | §5 新增判据「**模板实参名与默认值已固化** ⇒ 落进 capability delta ⇒ `grep` 可判」 |
| N3 | 决策 ⑥ 的「或手工置位测、或 AC 明写 dormant」未指明何时定 | 决策 ⑥ 补注「**由 P15 立项时随 AC 固化**（流程余量，**不是**未决决策）」 |

★ **critic 第 1 轮明确确认无冲突项**：与 ADR-050 D5（默认形态逐字保留）· D8（不触 `core/`）·
ADR-051 的 MUST `noexcept(false)`（决策 ③ **维持**而非削弱）**均一致**；「可行但不做」是对 ADR-051
**未管辖**的新变体的取舍，**不构成矛盾**；focus 3 的「决策是否可执行」结论 = API 形态已具体到可直接立项。

---

## 3. critic 复审（第 2 轮 · **只验闭合**）

| 项 | 结论 |
|---|---|
| BLOCKER 闭合判定 | **`CLOSED`** —— 三处修正逐字核实（`ADR-067:139-141` · `:252-256` · 草案 `:180-182`）⇒ 修正方向与 critic 给出的修复方向**完全同型** ⇒ 恒假断言**已不存在于任何**将冻结进 change AC 的判据中 |
| 新增 diff 是否引入新 BLOCKER | **`NONE`** —— 13 项新增/修订逐项对照四类判据（S1 消除了 MODIFIED 标签的二义 · S2 是对 SHOULD_FIX 的闭合且与 §3 D 行理由 ③ 的观察一致 · S5/S6 为可机械判定的增量 · S11/S12 闭合上一轮的两处冲突点 · 其余为表述层修订） |
| `mergeable` | **`yes`** |

---

## 4. 逐条处置汇总（**无一条「只列不改」**）

- oracle：**6 / 6** 已修正进 ADR（+ 2 条同步进 planner 草案）
- critic：**1 BLOCKER + 5 SHOULD_FIX + 3 NIT = 9 / 9** 已处置
- critic 复审：`CLOSED` · 新 BLOCKER `NONE` · `mergeable: yes`
- **未采纳的 finding**：**无**（⇒ ⇒ 本 ADR 的全部审稿意见均已闭合并留痕）

## 5. 本记录的边界

- 本记录是**审稿过程**的载体 ⇒ **决策正文**在 `docs/adr/ADR-067-dtor-exception-spec-composability.md`
  ⇒ ⇒ 两份**不得**互相改写（正文只留最终结论 + 指针）。
- 复现件：`/tmp/opencode/issue28/`（`repro.cpp` · `variants.cpp` · `variants2.cpp` · `inst.cpp` ·
  `lang.cpp` · `inrepo.cpp` · `optcheck.cpp` · `fixcheck.cpp` · `wrongcheck.cpp`）
  ⚠️ 该目录在**临时区** ⇒ 长期证据以 ADR §1.2 的**文件:行号**引用为准。
