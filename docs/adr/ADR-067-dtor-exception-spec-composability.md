# ADR-067: `chlib` 析构异常规格 vs 类型可组合性 —— 「析构期可捕获检出」与「可作按值成员」的**互斥性**与上游裁量

- Status: **Accepted**（2026-11-09 · **用户拍板采纳本裁决** · oracle 1 轮 + critic **2 轮**（第 1 轮 `REJECT` · **1 BLOCKER** ⇒ 第 2 轮复审 `CLOSED` / 新 BLOCKER `NONE` / `mergeable: yes`）⇒ ⇒ 双审已过 · 审稿记录 = `.rddf/reviews/ADR-067-dtor-composability.md`）
- Date: 2026-11-09
- Author: build-agent（architect role）
- Parent: ADR-050（D2 移除集 · **D5 析构期可捕获检出**）· ADR-051（条件块析构契约「析构 MUST `noexcept(false)`」）
- 关联 capability: `chlib-state-machine-semantics`（**ADDED**：析构检出契约 + 可组合形态 + 使用约束
  —— 该 spec 中「析构」现出现 **0** 次 ⇒ ⇒ **无既有 requirement 可 MODIFIED** ⇒ ⇒ 走 ADDED）
  · `chlib-conditional-semantics`（**ADDED**：使用约束；★ 其既有「析构 MUST `noexcept(false)`」
  requirement **零改动**（决策 ③ = 维持）⇒ ⇒ **无 MODIFIED delta** ⇒ ⇒ 理由记录在本 ADR 决策 ③ + change 的 `design.md`）
- 关联 change: `backlog: chlib-dtor-noexcept-composability-2026-11`（**未立项** ⇒ ⇒ **AC 映射暂缺**，立项后回填）
- 关联 issue: https://github.com/chisuhua/CppHDL/issues/28（下游 ChipForge 的编译破坏反馈 · 2026-11-08）
- 姊妹 improvement（**不在本 ADR 范围**）：`.rddf/improvements/public-api-remove-visibility-2026-11`
  —— issue #28 的**第 ① 半**（公开 API 移除的跨仓可见性 · CHANGELOG 载体 · Zero-Debt 判决规则边界）
  ⇒ ⇒ **不需**架构裁定 ⇒ ⇒ 不被本 ADR 的审稿门卡住。
- 前置审查：oracle（架构复核）+ critic（对抗审稿 · `review_profile: arch`）⇒ 记录 = `.rddf/reviews/ADR-067-dtor-composability.md`
- **超长自检**：正文（§1–§4 = `:20-241`）**222** 行 ≤ 250 ⇒ ⇒ **不是**设计规格（是「决策 + 后果 + 判据」）
  ⇒ ⇒ **不拆分**；⚠️ 推导与逐条证据（复现矩阵 / 逐选项核算）⇒ 落在 planner 草案与审稿记录，**不进正文**。

---

## 1. 背景与问题（**只陈述决策所需的事实**）

### 1.1 问题的形状

`include/chlib/state_machine.h:506` 的 `~ch_state_machine() noexcept(false)`（ADR-050 **D5**：
「未 resolve 的贡献 MUST 在析构期**可捕获**检出」）⇒ ⇒ **析构非 `noexcept` 的类型** ⇒ ⇒
**它不能作为「有虚析构的类」的按值成员** ⇒ ⇒ 派生类的隐式析构被推成 `noexcept(false)`
⇒ ⇒ override 一个 `noexcept` 虚析构 ⇒ **硬编译错误**
（`looser exception specification on overriding virtual function`）。

### 1.2 实测事实（本次独立复现 · `/tmp/opencode/issue28/`）

| 事实 | 证据 |
|---|---|
| **触发面比 issue 描述更宽**：基类**不必显式**写 `noexcept`（`virtual ~Base() = default;` 隐式 noexcept 同样触发） | `variants.cpp` |
| 真正的条件 = 「**基类链上有虚析构**」+「派生类**按值**持有」；基类**非虚**析构 ⇒ 不触发 | `variants.cpp` |
| **★ 上游自己的用户会撞**：用仓内 `include/component.h:62` 的 `virtual ~Component();` 作基类 + 按值持有 FSM ⇒ **同样的编译错误** | `inrepo.cpp` |
| 而仓内的 FSM **全是 describe() 里的局部对象**（`examples/spinalhdl-ported/uart/uart_tx.cpp:71`）⇒ ⇒ **「把局部 FSM 提为成员」= 标准重构** ⇒ 今天不可用 | 源码 |
| 派生类显式 `~D() noexcept override {}` / `= default` ⇒ 能编译，但成员析构抛出 ⇒ **`std::terminate`** ⇒ 检出降级为 abort | `lang.cpp` |
| **`std::optional<Sm>` 是静默降级陷阱**：`static_assert(std::is_nothrow_destructible_v<std::optional<Sm>>)` **成立**（libstdc++ 把 `T` 放进 union ⇒ union 析构 noexcept ⇒ `~optional()` 推导 noexcept）⇒ ⇒ **编译期无诊断 + 运行期 terminate** ⇒ 比「编译期撞墙」更危险 | `optcheck.cpp` |
| **继承也会传染**：隐式析构把「potentially throwing」**往上传** ⇒ 作为**基类**同样触发 | C++ 规则 |

### 1.3 ★ 两处**不同构** ⇒ 不能一把钥匙

| 位置 | 析构**做什么** | 改 `noexcept` 的后果 |
|---|---|---|
| `state_machine.h:506`（FSM） | **只检查**（未 resolve 的贡献 ⇒ 抛） | 只是**降级检出**（可捕获 ⇒ terminate） |
| `if_stmt.h:133`（`conditional_block`） | **做事**（兜底调 `end()` ⇒ **合成候选值并写入目标**） | ⚠️ **`end()` 的「做事」与「抛」本就是分离的**（`if_stmt.h:179-204`：抛点只有 `validate()` 的「分支体缺失」与 `finalize_targets()` 的「输出缺 `else_` 默认」两处**用法错误检出**；合成 + 写入是主路径）⇒ ⇒ 「可组合形态」**可行** ⇒ ⇒ 改 `noexcept` 丢的是**检出**，**不是功能** |

⇒ ⇒ 「同形态 ⇒ 同批改」的前提**不成立** ⇒ ⇒ 两处**必须分别裁定**。

### 1.4 问题（**精确表述**）

**不是**「要不要检出」，**也不是**「要不要支持跨仓组合」，而是：

> **在 C++17 的语言规则下，「析构期可捕获检出」与「该类型可作按值成员」二者在【同一析构函数】上互斥**
> —— `noexcept` 析构 ⇒ 析构期抛 ⇒ `std::terminate` ⇒ **不可捕获**；
> 而 `noexcept(false)` 析构 ⇒ 类型不可组合。
> ⇒ ⇒ 上游 MUST 明确**取舍哪一个是默认**，并为**另一个**提供**显式**的形态（若要的话）。
>
> ⚠️ **「互斥」的限定语不可省**：若把「抛」挪到**另一个对象**（如析构可抛的**局部**守卫），
> 则「可组合 + 可捕获」在字面上可同时成立 ⇒ ⇒ 但该形态对本 issue 的**核心场景**
> （**成员持有** ⇒ 析构发生在组件析构期 ⇒ describe() 内的守卫早已死亡）**结构性无效**
> ⇒ ⇒ 详见 §3 的选项 **D** 行。

### 1.5 capability 层的不对称

`chlib-conditional-semantics`（`spec.md:209-226`）**明文**要求「析构函数 MUST 声明为 `noexcept(false)`」
+ Scenario「输出缺默认候选时的拒绝在析构路径上可捕获」⇒ ⇒ 改它 = **MODIFIED delta**；
`chlib-state-machine-semantics` 里「析构」出现 **0 次** ⇒ ⇒ FSM 侧契约只活在 ADR-050 D5 + 头注释。

---

## 2. 决策（**裁决**）

### 决策 ① —— 两处**分别**裁定（FSM 与 `conditional_block` **不共用**钥匙）

依据 §1.3（不同构）⇒ ⇒ 一个形态方案**不得**同时套在两处。

### 决策 ② —— **FSM**：默认形态**零变更** + **显式**提供「可组合形态」

| 形态 | 析构异常规格 | 检出时机 | 可捕获 | 可组合 |
|---|---|---|---|---|
| **默认**（`DetectAtDtor = true`）—— **逐字保留 ADR-050 D5** | `noexcept(false)` | **析构期（自动）** | ✅ | ❌ |
| **可组合形态**（`DetectAtDtor = false`）—— **新** | `noexcept` | **① 显式 `check_unresolved()`**（可捕获）**② 析构期 terminate 兜底**（不可捕获 · fail-closed） | ✅（仅 ①） | ✅ |

- **可组合形态的语义**：
  1. 析构**不抛** ⇒ ⇒ 类型可作按值成员 / 基类（下游的核心诉求）；
  2. **显式** `check_unresolved()` ⇒ 抛**可捕获**异常（⇒ 上游的「失败机制 MUST 可测」在**显式路径**上成立）；
  3. **终止兜底**（fail-closed）：析构时若确有未 resolve 的贡献且**非展开期**
     ⇒ 输出诊断 + `std::terminate()` ⇒ ⇒ **绝不静默**（与「`DetectUnresolved=false` 下静默丢弃贡献」明确对立）。
  4. ★ **检出非幂等（fail-closed 的必然结果）**：`check_unresolved()` 的检出是**只读检查**
     ⇒ 用户 `catch` 后**不 resolve** ⇒ 析构时兜底**照常 terminate**
     ⇒ ⇒ MUST NOT 引入「已检出 ⇒ 豁免」的标志位（那会把「检出」变成「一次性提醒」⇒ ⇒ 退回静默）
     ⇒ ⇒ 若用户 `catch` 后**继续 resolve** ⇒ 兜底自然不触发（状态已消解）⇒ ⇒ **这是推荐的恢复路径**。
- **默认形态零变更** ⇒ ⇒ 既有 **8 处**对象创建点（`examples/spinalhdl-ported/uart/uart_tx.cpp:71` ·
  `tests/test_state_machine.cpp:104,172,235,255` · `tests/test_state_machine_dual.cpp:85,117` ·
  `tests/test_verilator_complex_e2e.cpp:189`；另有 **3 处** `using` 别名）+ 上游既有测试**逐字不变**
  ⇒ ⇒ 与 ADR-050 **D8**（「`core/` 层零改动」）同型边界。
- **命名**：`template <typename StateEnum, size_t N, bool DetectAtDtor = true>`
  ⇒ ⇒ 名字 MUST 表达「**在析构期**检出」这一**时机**（⇒ 勿用 `DetectUnresolved`：可组合形态**也有**检出，只是时机不同）
  ⇒ ⇒ 最终拼写由实施期按仓内命名诚实审计（`.rddf/checks/chlib-naming-honesty-registry.md`）校准。

### 决策 ③ —— **`conditional_block`：维持 `noexcept(false)`**，**当前不提供**可组合形态

依据 §1.3（其析构**兜底合成**是**功能**）⇒ ⇒ **不提供**可组合形态。

⚠️ **否决理由 MUST 如实写**（**不可**写成「不可行」）：由 §1.3 的分离性分析可知
「析构 `noexcept` + 保留兜底合成 + 检出改 terminate 兜底 + 显式检查」的**可组合形态是可行的**
⇒ ⇒ **当前不做的真实理由**是：① **无需求**（无下游消费者提出）⇒ 每条 chlib 公开面扩张都有固定成本；
② **固定成本高**：`chlib-conditional-semantics` 有**明文 MUST**（「析构 MUST `noexcept(false)`」）⇒ 改它必须走
**MODIFIED delta**（禁 `--skip-specs`）；③ **基线缺失**：该 capability 的析构路径 Scenario（`spec.md:223`）
**当前零测试背书**（`tests/chlib/test_if_stmt.cpp` 里 `.end()` 出现 **14** 次、**零处**从析构路径触发）
⇒ ⇒ 在**没有基线**的情况下改析构契约 ⇒ 无法判断是否回归。

⇒ ⇒ **代价（如实登记）**：条件块**仍**不可作为「有虚析构的类」的按值成员 ⇒ ⇒
⇒ 「`conditional_block` 的可组合形态」= **已知限制**（**当前不做** · 可行路径如上）
⇒ ⇒ 若日后确有需求 ⇒ **独立立项**（入口 = §1.3 的分离性分析 + 本决策的理由 ②/③）
⇒ ⇒ ★ **本条登记的目的**：防止后人把「不可行」当**事实**继承（它是**取舍**，不是**限制**）。

### 决策 ④ —— **使用契约**（两处共用 · 措辞精确 · 落 2 份头注释 + 2 份 capability）

> **析构非 `noexcept` 的类型** MUST NOT 作为「**有虚析构**的类」的**按值成员**或**基类**
> —— 派生类的隐式析构异常规格由子对象推导 ⇒ `noexcept(false)` 子对象 ⇒ 派生类析构 `noexcept(false)`
> ⇒ ⇒ override 一个 `noexcept` 虚析构 ⇒ **编译错误**。

⚠️ **不得**承诺「编译器会说人话」—— 类型系统无法在成员声明处产出**可行动**诊断 ⇒ ⇒ 契约的价值在
**文档 + capability + 评审**，**不是**编译期提示。

### 决策 ⑤ —— **可组合性的机械判据**（**禁**散文式判据）

新增 `tests/test_chlib_composability_api.cpp`（**仓外视角**：只 include 公共头 + 真编译 + 真链接 + 真运行）：

- `static_assert(std::is_nothrow_destructible_v<Dut>)`，其中 `struct Dut : ch::Component { Sm sm_; };`
  （**用仓内 `Component` 作基类** ⇒ 与下游**同形**）；
- `static_assert(std::is_same_v<ch_state_machine<S,N>, ch_state_machine<S,N,true>>)`
  ⇒ 钉「默认形态的**类型身份**不变」（⇒ 「默认形态零变更」在**类型层**可判）；
- **静默降级 MUST 钉成「落差」而非「取反」**（⚠️ 取反会写成恒假断言 ⇒ 判据永不可达）：
  `static_assert(!std::is_nothrow_destructible_v<Sm>)` **且**
  `static_assert(std::is_nothrow_destructible_v<std::optional<Sm>>)`
  ⇒ 即「`Sm` 假 ∧ `optional<Sm>` 真」⇒ ⇒ 证明「包装一层就静默变 noexcept」；
- 视裁定补**反向**断言（默认形态 MUST 仍 `noexcept(false)` ⇒ `!is_nothrow_destructible_v<Sm>` 已在上面）；
- **变异自检**：把 `noexcept` 翻回 `noexcept(false)` ⇒ fixture **必红** ⇒ 还原复绿。

### 决策 ⑥ —— **展开期 / 静态析构期门**：新形态 MUST **复制**既有门

`state_machine.h:509` / `if_stmt.h:135` 的 `std::uncaught_exceptions() == 0` 门 MUST 复制到
「终止兜底」路径 ⇒ ⇒ **展开期不触发终止**（⇒ 不新增「双异常 ⇒ terminate」路径）。
⚠️ `in_static_destruction()` 门**当前永不生效**（ADR-050:166 自陈 3 个调用点全被注释）⇒ ⇒
「保持既有守卫」**行为不可测** ⇒ ⇒ 或按 `tests/test_component_io_const_and_node_name_policy.cpp:69-70`
的做法**手工置位**测它，或在 change 的 AC 里明写「该门为 dormant · 本轮只做形态保持」
⇒ ⇒ ★ 该二选一**由 P15 立项时随 AC 固化**（流程余量，**不是**未决决策）。

### 决策 ⑦ —— **不做**：elaboration 期扫描（把检出移到「describe() 收尾」）

**结构事实（先于成本评估）**：`Component::build_internal`（`src/component.cpp:318-339`）=
`create_ports(); describe();` ⇒ **`describe()` 是最后一步** ⇒ ⇒ **局部 FSM 在 describe() 尾部即销毁**
⇒ ⇒ 「describe() **之后**扫描局部 FSM」**不可行**（对象已死）。

⇒ ⇒ 唯一可行路径 = FSM 构造时向 **thread_local 注册表自登记**（持共享态）+ **框架收尾点**遍历
⇒ ⇒ 但「析构时自注销」会把检出**又放回析构期**（仓内唯一同形态先例 `if_stmt.h:109` 的
`block_stack_` 正是「构造 push / 析构 pop」的**激活栈**形态 ⇒ ⇒ **不能**用来支持延后扫描）
⇒ ⇒ 成本 = **共享状态** + **core 层钩子** + **改「检出时机」**（⇒ ADR-050 D5 的「析构时抛」措辞变更
+ capability 改写）+ **静默窗口**（非 `Component` 上下文里的 FSM 无收尾点）

⇒ ⇒ ⇒ **独立立项**（若日后需要）⇒ ⇒ 结论不变，但**理由由「成本 > 收益」改为「结构上需重设检出时机」**。

### 决策 ⑧ —— **不做**：恢复 `is_built()`

上游有**反向断言**（`tests/test_state_machine.cpp:201` 的 `STATIC_REQUIRE(!has_is_built<Sm>::value)`）
锁定该 API 不得存在 ⇒ ⇒ 恢复 = 改测试 + 动 ADR-050 ⇒ ⇒ **不在本 ADR 范围**
⇒ ⇒ 归姊妹 improvement（`public-api-remove-visibility-2026-11`）或**独立决策**。

### 决策 ⑨ —— **拷贝语义**：**不删**拷贝，但 MUST **显式警告**

`ch_state_machine` 有用户声明的析构 ⇒ **无隐式移动构造**（拷贝仍在）⇒ ⇒ 拷贝会**浅拷贝**
`contributions_` / `resolved_targets_` 里的 `lnodeimpl *` ⇒ ⇒ **两个 C++ 对象指向同一批节点句柄**
⇒ ⇒ ① 语义地雷（独立于检出）· ② 检出的**唯一可达假阳性窗口** = 「`contrib` 后、`resolve` 前被拷贝」
（`Component` 移动构造对**空表**成员的退化拷贝发生在 describe() **之前** ⇒ 不可达）
⇒ ⇒ 本 ADR **不删**拷贝（避免对下游再增一处源码破坏）⇒ ⇒ MUST 在头注释写明该警告，且警告范围 MUST 含
「**MUST NOT 按值传参 / 放进容器 / 显式拷贝**」⇒ ⇒ 「删拷贝」的**后续项理由** =
「浅拷贝节点句柄 = **语义地雷**」（比「防检出假阳性」更硬）⇒ ⇒ 独立后续项。

---

## 3. 选项对比（**逐条含理由**）

| 选项 | 检出时机 | 可捕获 | 可组合 | 结论 |
|---|---|---|---|---|
| **A 维持现状 + 文档** | 析构期 | ✅ | ❌ | **否决** —— 「不可组合」对下游**零提示** ⇒ 且下游被迫走指针持有 ⇒ 检出降级为 terminate |
| **A′ 维持现状 + 使用契约** | 析构期 | ✅ | ❌ | **部分采纳**（契约部分 = 决策 ④）⇒ 契约**单独**不足以解决下游 ⇒ 故**并入决策 ②**（默认形态 = A′） |
| **B 显式 `finalize()` + 析构 `noexcept` + 终止兜底** | 显式 | ✅（显式路径） | ✅ | **否决**（作为**默认**）—— 把既有用户的检出从「自动」降为「opt-in」⇒ 违反 ADR-050 D5 的「自保护优于纪律」⇒ ⇒ **其形态被决策 ② 吸收**（可组合形态 = B 的形态，但**只在显式 opt-in 时**） |
| **C 模板参数 opt-out** | 析构期或显式 | 取决于实参 | ✅ | **采纳**（= 决策 ② 的形态）⇒ ⚠️ 单纯 C（`false` 即**无检出**）会**静默丢检查** ⇒ ⇒ 故决策 ② 追加「显式 `check_unresolved()` + 终止兜底」⇒ ⇒ **不是裸 C** |
| **D 作用域守卫对象** | 守卫析构期 | ✅ | ✅（守卫须为局部临时） | **否决** —— ① **对成员持有（本 issue 核心场景）结构性无效**：守卫是 describe() 局部对象 ⇒ 成员 FSM 的析构发生在**组件析构期** ⇒ 守卫早已死亡 ⇒ 仍需 terminate 兜底 ⇒ 退化为「决策 ② + 一个冗余抛出点」；② **嵌套不可靠**：外层守卫抛出 ⇒ `uncaught_exceptions() > 0` ⇒ 内层命中 `state_machine.h:509` 的门 ⇒ **静默**；③ **检出不幂等**：守卫的检出是只读检查 ⇒ 用户 catch 后继续 ⇒ 成员 FSM 析构时 terminate 兜底**必然二次触发**（要幂等需给守卫写权限 ⇒ API 面再增）；④ **收益被决策 ② 严格覆盖**（描述末尾一行 `check_unresolved()`：同纪律负担 / 同可捕获 / 同时机由用户放置）；⑤ `guard` 一词撞 `chlib/guard_contribution.h` 的既有语义 |
| **E elaboration 期扫描** | describe() 收尾 | ✅ | ✅ | **否决**（决策 ⑦）—— core 层钩子 + 共享状态 + 改检出时机 + 静默窗口 ⇒ 成本 > 收益 |
| **`std::optional<Sm>` 持有** | — | — | ✅（无诊断） | **否决**（进否决表）—— `~optional()` 推导 `noexcept` ⇒ 编译期无诊断 + 运行期 terminate |
| **`[[no_unique_address]]`** | — | — | — | **否决**（进否决表）—— **不参与**隐式析构异常规格推导（只改布局） |
| **派生类显式 `~D() noexcept override {}`** | 析构期 | ❌（terminate） | ✅ | **否决**（作为上游方案）—— 它是**下游的逃生口**，上游不必提供；且它把异常变成 abort |

---

## 4. 后果 / 代价

### 正面

- 「可捕获检出」与「可组合」**不再互斥**（对用户而言）⇒ ⇒ 下游可**回到按值持有**且检出**不被降级**（显式检查路径）；
- **默认形态逐字不变** ⇒ ⇒ 既有 **8 处**对象创建点 + 上游既有测试**零改动** ⇒ ⇒ ADR-050 D5 **不被本 ADR 削弱**；
- 两处**分别裁定** ⇒ ⇒ 条件块的**兜底合成功能零损失**；
- 使用契约 + 机械判据 ⇒ ⇒ 「不可组合」从**晦涩的 C++ 报错**变成**文档 + capability + 编译期 fixture** 三重覆盖；
- 可组合形态的「终止兜底」⇒ ⇒ **无静默丢弃**（对比裸 C 的 `false` 形态）。

### 负面 / 代价

- **公开 API 面扩张**：多一个模板实参 + 一个显式方法（`check_unresolved()`）⇒ ⇒ 需 capability delta + 头注释 + 测试；
- **可组合形态的检出是「显式 + terminate」** ⇒ ⇒ 使用者 MUST 知情（⇒ 头注释 + capability MUST 写明）；
- **`conditional_block` 仍不可组合** ⇒ ⇒ **已知限制**（决策 ③）；
- **判据成本**：新增 fixture + 变异自检 + `CTEST_TOTAL` 锚点同步；
- **下游仍需迁移**：下游 MUST 改模板实参（或保持 `unique_ptr`）⇒ ⇒ 与 issue 的「建议 ②」**不完全一致**
  （本 ADR 不承诺「析构保持 `noexcept` 而检出仍在析构期」—— 那在 C++17 下**不可实现**）。

### 风险

- ⚠️ **终止兜底**在「非展开期 + 确有未 resolve」时 ⇒ `std::terminate` ⇒ ⇒ MUST 有诊断输出（否则**不可行动**）；
- ⚠️ 模板实参变更 ⇒ `<S,N>` 与 `<S,N,false>` 是**不同类型** ⇒ ⇒ 跨函数传参 / 容器 / `template<template<class,size_t> class>` 形态的下游**会断** ⇒ ⇒ 迁移指引 MUST 写明；
- ⚠️ 可组合形态下若用户**忘记** `check_unresolved()` ⇒ ⇒ 兜底在**析构时**才触发 ⇒ ⇒ 时机可能**晚于**用户期望 ⇒ ⇒ 头注释 MUST 说明；
- ⚠️ capability delta 触及 `chlib-conditional-semantics` 的**明文 MUST** ⇒ ⇒ 必须 MODIFIED（禁 `--skip-specs`）；
- ⚠️ `in_static_destruction()` 门**当前 dormant**（ADR-050:166 自陈 3 个调用点全被注释）⇒ ⇒ 「保持既有守卫」无行为证据（决策 ⑥）；
- ⚠️ **同一组件内混用两种形态**（describe() 内局部 FSM 用**默认形态** + 成员 FSM 用**可组合形态**）⇒ ⇒ 两套检出语义并存（自动抛 vs 显式抛 + 终止兜底）⇒ ⇒ 头注释 MUST 给**选择指引**（默认形态留给 describe() 内局部；可组合形态留给成员）；
- ⚠️ **拷贝**的可达窗口比「可能假阳性」更窄（**唯一**窗口 = 「`contrib` 后、`resolve` 前被拷贝」；`Component` 移动构造对空表成员的退化拷贝发生在 describe() **之前** ⇒ 不可达）⇒ ⇒ 但「浅拷贝节点句柄」本身是**独立于检出**的语义地雷 ⇒ ⇒ 决策 ⑨ 的头注释警告 MUST 含「MUST NOT 按值传参 / 放进容器 / 显式拷贝」。

### 不在本次范围

- 恢复 `is_built()`（决策 ⑧）· CHANGELOG 载体与 Zero-Debt 判决规则边界（**姊妹 improvement**）·
  发布/tag 策略 · 「删拷贝」（决策 ⑨ 的独立后续项）· elaboration 期扫描（决策 ⑦）·
  条件块的可组合形态（决策 ③ · **已知限制**）。

### 回滚路径

本 ADR 的落地是**纯增量**：新模板实参带**默认值** ⇒ 既有 **8 处**对象创建点的**类型身份不变**
（`std::is_same_v<ch_state_machine<S,N>, ch_state_machine<S,N,true>>`）⇒ 新增 specialization +
2 份 capability 的 **ADDED** requirements + 新测试文件 + 头注释 ⇒ ⇒
**回滚 = 还原该 change 即可**（无数据迁移、无序列化格式变更、无 `core/` 契约变更）⇒ ⇒ 回滚窗口 = 全程。

---

## 5. Verification（**前瞻**验收判据 —— **可勾选**）

- [ ] **默认形态零变更**：既有 `tests/test_state_machine.cpp`（含「未 resolve ⇒ 析构时抛」的既有用例）**逐字不变**且绿；
- [ ] **可组合形态可编译**：`static_assert(std::is_nothrow_destructible_v<Dut>)`（`Dut : ch::Component { Sm sm_; }`）绿；
- [ ] **默认形态仍不可组合**：`static_assert(!std::is_nothrow_destructible_v<Sm>)` 绿（⇒ ⇒ 决策 ② 的「默认零变更」可判）；
- [ ] **静默降级被钉死（合取形态 · 禁取反）**：`static_assert(!std::is_nothrow_destructible_v<Sm>)` **且**
      `static_assert(std::is_nothrow_destructible_v<std::optional<Sm>>)` 同时绿
      （⚠️ **单写 `!is_nothrow_destructible_v<std::optional<Sm>>` 是恒假断言** ⇒ 判据永不可达
      ⇒ ⇒ 该错误已于本 ADR 审稿第 1 轮被 BLOCKER 指出并修正 ⇒ ⇒ **实测**：合取形态编译通过
      / 取反形态 `static assertion failed`）；
- [ ] **默认形态的类型身份不变**：`static_assert(std::is_same_v<ch_state_machine<S,N>, ch_state_machine<S,N,true>>)` 绿；
- [ ] **变异自检**：把 `noexcept` 翻回 `noexcept(false)` ⇒ 上述 fixture **必红** ⇒ 还原复绿；
- [ ] **终止兜底可测**：可组合形态下未 resolve 且非展开期 ⇒ **terminate**（且有诊断输出）⇒ 有用例；
- [ ] **展开期不终止**：异常展开中析构可组合形态 ⇒ **不 terminate** ⇒ 有用例；
- [ ] **显式检查可捕获**：可组合形态下 `check_unresolved()` ⇒ 抛**可捕获**异常 ⇒ 有用例；
- [ ] **使用契约已固化**：2 份头注释 + 2 份 capability spec 含 §2 决策 ④ 的措辞 ⇒ `grep` 可判；
- [ ] **模板实参名与默认值已固化**：最终拼写（`DetectAtDtor` 或实施期按命名诚实审计校准后的名字）
      **与默认值** MUST 落进 capability delta ⇒ ⇒ `grep` 可判（⇒ 否则「命名校准」本身无验收锚点）；
- [ ] **`chlib-conditional-semantics` 的既有 MUST 零改动**：`git diff` 显示
      `openspec/specs/chlib-conditional-semantics/spec.md` 中 `noexcept(false)` 的
      MUST **未被改写**（⇒ 「显式处置 = 维持 + 理由」，而**非** MODIFIED）
      ⇒ ⇒ 理由记录在 ADR-067 决策 ③ + change 的 `design.md` ⇒ ⇒ `grep` 可判；
- [ ] **capability delta 合入主 spec**：`bash scripts/verify_openspec_delta.sh` 绿
      + `openspec validate --specs --strict` 绿（⚠️ CLI 须 **1.4.1**）
      + `chlib-conditional-semantics` 的 `noexcept(false)` MUST 被**显式处置**（维持 + 理由）；
- [ ] **计数锚点同步**：`AGENTS.md` 的 `CTEST_TOTAL`（**253**）同步 ⇒ `scripts/verify_test_counts.sh` 绿。
