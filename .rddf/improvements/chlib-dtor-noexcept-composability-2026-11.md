---
name: chlib-dtor-noexcept-composability-2026-11
issue_ref: 28
gh_repo: chisuhua/CppHDL
priority: P1
phase: phase-6
category: framework
type: feature
状态: approved
主题: 已知功能缺口清零
依赖: ADR-050, ADR-051, ADR-067
来源: 上游 issue #28（下游 ChipForge 反馈 · 2026-11-08）· **ADR-067（Accepted 2026-11-09 · 用户拍板）** · phase-6 P12（起草）/ P13（architect 裁定 · 已闭口）/ P15（实施）
生成时间: 2026-11-09
evidence: test:tests/test_chlib_composability_api.cpp
---

# Improvement: `chlib` 析构异常规格 vs 可组合性 —— 类型「不能当成员」的框架级缺口

- **Date drafted**: 2026-11-09
- **Drafted by**: planner 角色（build-agent）
- **Status**: ✅ **approved + 已实施并归档**（2026-11-09 · change =
  `openspec/changes/archive/2026-11-09-chlib-dtor-composability-2026-11/`
  ⇒ ⇒ spec delta 已合入主 spec：`chlib-state-machine-semantics` **ADDED ×1** ·
  `chlib-conditional-semantics` **ADDED ×1**（其既有「析构 MUST `noexcept(false)`」**零改动**）
  ⇒ ⇒ 实施 = phase-6 **P15**（✅ 已闭口 · 证据路径见 phase-6 该行））
- **上游 issue**：`https://github.com/chisuhua/CppHDL/issues/28`（2026-11-08 · 下游 ChipForge 的编译破坏反馈 · 原文见文末附录）
- **权威决策**：`docs/adr/ADR-067-dtor-exception-spec-composability.md`（**Accepted** · oracle 1 轮 + critic 2 轮
  ⇒ BLOCKER 闭合 · `mergeable: yes`）⇒ ⇒ 本 improvement **MUST NOT** 改写 ADR-067 的决策内容
  （**契约唯一权威 = ADR-067**）⇒ ⇒ 本文件的 §What Changes 的**选项表**已被 ADR-067 的决策取代
  ⇒ ⇒ 实施时以 **ADR-067 §2 决策 ①–⑨** 为准。

**主题**: 已知功能缺口清零

## 架构依据

### 问题的确切形态（本次独立复现 · 证据可复跑）

复现件：`/tmp/opencode/issue28/`（`repro.cpp` / `variants.cpp` / `variants2.cpp` / `inst.cpp` / `lang.cpp` / `inrepo.cpp` / `optcheck.cpp`）。

`~ch_state_machine() noexcept(false)`（`include/chlib/state_machine.h:506`，来自 `524044f` · **ADR-050 D5**
「未 resolve 的贡献 MUST 在析构期**可捕获**检出」）⇒ **析构非 noexcept 的类型** ⇒ ⇒
**它不能作为「有虚析构的类」的按值成员**（否则派生类的隐式析构被推成 `noexcept(false)`
⇒ override 一个 `noexcept` 虚析构 ⇒ 硬编译错误 `looser exception specification on overriding virtual function`）。

| 组合 | 结果 |
|---|---|
| 基类 `virtual ~Base() noexcept = default;` + 派生**按值**持有 | ❌ 报错 |
| 基类 `virtual ~Base() = default;`（**隐式** noexcept）+ 派生**按值**持有 | ❌ **同样报错** ⇒ 触发面**比 issue 描述更宽**（基类**不必显式**写 `noexcept`） |
| 基类**非虚** `~Base() noexcept` + 派生按值持有 | ✅ 通过 |
| 派生类显式写 `~D() noexcept override {}` / `= default` | ✅ 通过，但成员析构抛出 ⇒ **`std::terminate`** ⇒ 检出降级为 abort |
| `unique_ptr` / `shared_ptr` / 裸指针持有 | ✅ 通过（下游当前处置 ⇒ 同上降级） |
| **`std::optional<Sm>` 持有** | ✅ **通过且无任何诊断** ⇒ ⚠️ `static_assert(std::is_nothrow_destructible_v<std::optional<Sm>>)` **成立** ⇒ ⇒ libstdc++ 把 `T` 放进 union 且 union 析构为用户提供 + noexcept ⇒ `~optional()` 推导为 `noexcept` ⇒ ⇒ **把「编译期撞墙」换成「运行期无诊断 terminate」** ⇒ 比现状**更危险** ⇒ **必须进否决表 + 钉成反向断言** |

### ★ 仓内最强证据：**上游自己的用户会撞**

`include/component.h:62` 的 `virtual ~Component();`（隐式 `noexcept`）⇒ ⇒ **用仓内自己的 `Component` 作基类**、
**按值**持有一个 FSM ⇒ **同样的编译错误**（实测：`inrepo.cpp` ⇒ `note: overridden function is 'virtual ch::Component::~Component() noexcept'`）。

⇒ ⇒ 而仓内的 FSM **全是 describe() 里的局部对象**（`examples/spinalhdl-ported/uart/uart_tx.cpp:71`
`ch_state_machine<UartTxState, 4> sm;`）⇒ ⇒ **「把局部 FSM 提为组件成员 / 把 demo 抽成可复用组件」
是标准重构动作** ⇒ ⇒ 该动作今天**编译不过** ⇒ ⇒ **这不是下游的怪用法**（比引用 issue 更有力）。

### ★ 两处**不同构** ⇒ 必须**分别**裁定

| 位置 | 析构**做什么** | 改 `noexcept` 的后果 |
|---|---|---|
| `include/chlib/state_machine.h:506`（FSM） | **只检查**（未 resolve 的贡献 ⇒ 抛） | 只是**降级检出**（可捕获 ⇒ abort） |
| `include/chlib/if_stmt.h:133`（`conditional_block`） | **做事**（兜底调 `end()` ⇒ **合成候选值并写入目标**） | ⚠️ **`end()` 的「做事」与「抛」是分离的**（`if_stmt.h:179-204`：抛点只有 `validate()` 的「分支体缺失」与 `finalize_targets()` 的「输出缺 `else_` 默认」两处**用法错误检出**）⇒ ⇒ 「可组合形态」**可行** ⇒ ⇒ 丢的是**检出**，不是功能 |

⇒ ⇒ 「同形态 ⇒ 同批改」的前提**不成立** ⇒ ⇒ ADR-067 MUST **分别**裁定两处
（⇒ ⇒ 本 improvement 的「Out of Scope」因此**不含**条件块的语义变更，但**含**它的生命周期契约）。

### capability 层的不对称（决定改动成本）

| capability | 析构异常规格的契约位置 | 改动的代价 |
|---|---|---|
| `chlib-conditional-semantics` | **明文 Requirement**（`spec.md:209-226`「条件块的析构 MUST 兼容抛出路径」⇒ 含「析构函数 MUST 声明为 **`noexcept(false)`**」）+ Scenario「输出缺默认候选时的拒绝在析构路径上可捕获」 | **必须**走 MODIFIED delta（禁 `--skip-specs`） |
| `chlib-state-machine-semantics` | **「析构」出现 0 次** ⇒ 契约只活在 **ADR-050 D5 + 头注释** | 成本**低于**条件块侧（但仍需 ADR + delta 记录新契约） |

⚠️ **条件块侧今天的 Scenario 无测试背书**：`tests/chlib/test_if_stmt.cpp` 里 `.end()` 出现 **14** 次、
**零**处从**析构**路径触发 ⇒ ⇒ `spec.md:223` 的 Scenario **无测试** ⇒ ⇒ **任何**裁定都需要先补这条基线用例
（否则「析构期可捕获」这项能力**当前是否真的存在**都无证据）。

### 仓内已有同形态先例（供选项 E 复用）

`include/chlib/if_stmt.h:109`：`static thread_local std::vector<conditional_block *> block_stack_;`
⇒ ⇒ 仓内**已有**「thread_local 注册表」形态 ⇒ ⇒ 选项 E（elaboration 期扫描）**不是新发明**。
⚠️ **但该先例不可直接复用**（oracle 复核结论）：它是「构造 push / 析构 pop」的**激活栈**形态
⇒ ⇒ 「析构时自注销」会把检出**又放回析构期** ⇒ ⇒ **不能**用来支持「延后到 describe() 收尾扫描」
⇒ ⇒ 且 `Component::build_internal`（`src/component.cpp:318-339`）的 `describe()` 是**最后一步**
⇒ ⇒ **局部 FSM 在 describe() 尾部即销毁** ⇒ ⇒ 「describe() 之后扫描局部 FSM」**不可行**（对象已死）⇒ ⇒ E 的成本含**共享状态** + **core 层钩子**。

## 范围

- **In Scope**：
  1. **架构裁定（ADR-067）**：析构异常规格 × 检出触发点 的取舍 —— 六选项见 §What Changes；
  2. **capability 同步**：`chlib-state-machine-semantics` + `chlib-conditional-semantics`
     （MODIFIED / ADDED delta ⇒ 走 change 流程 ⇒ **禁** `--skip-specs`）；
  3. **可组合性的机械判据**：`tests/test_chlib_composability_api.cpp`（**仓外视角**：只 include 公共头
     + 真编译 + 真链接 + 真运行）+ 变异自检 ⇒ 见 §Acceptance；
  4. **使用约束固化**（精确措辞，见下）：**析构非 `noexcept` 的类型 MUST NOT 作为「有虚析构的类」的
     按值成员**或**基类** ⇒ 落进头注释 + capability；
  5. **上游自己的测试随裁定迁移**（含「忘记 `.end()` ⇒ 析构期可捕获」这条**当前缺失**的基线用例）。
- **Out of Scope**：
  - ❌ **API 移除的跨仓可见性 / CHANGELOG 载体 / Zero-Debt 判决规则边界** ⇒ **已拆为独立 improvement**
    `public-api-remove-visibility-2026-11`（**不需**架构裁定 ⇒ 不被 ADR 审稿门卡住）；
  - ❌ **恢复 `is_built()`**（上游有反向断言 `tests/test_state_machine.cpp:201`
    的 `STATIC_REQUIRE(!has_is_built<Sm>::value)` 锁定该 API 不得存在 ⇒ 恢复 = 改测试 + 动 ADR-050）；
  - ❌ 条件块 / 守卫贡献的**语义**变更（只动**生命周期契约**）；
  - ❌ `std::optional` / `[[no_unique_address]]` 等**替代持有形态**的支持（**进否决表**即可）；
  - ❌ 其它析构异常规格类问题（**全仓 `noexcept(false)` 恰好 2 处** = 本项的两处 ⇒ 范围成立）。

## Why

1. **上游自己的用户会撞**（`ch::Component` 基类 + 按值持有 ⇒ 编译失败）⇒ ⇒ 「局部 FSM 提为成员」
   这个**标准重构**今天不可用 ⇒ ⇒ 缺的不是「迁移指引」，是**类型层面的能力缺失**。
2. **取舍从未被显式裁定**：ADR-050 D5 只在「检出**必须可测**」维度裁定（`noexcept(false)`），
   **从未考虑**「类型可组合性」维度 ⇒ ⇒ 缺口不是「谁粗心」，是**决策维度缺失**。
3. **两处不同构** ⇒ 「一把钥匙」会**静默丢功能**（条件块的兜底合成）⇒ ⇒ 必须分别裁定。
4. **静默降级比显式失败更危险**：`std::optional` 持有 ⇒ 编译期无诊断 + 运行期 terminate ⇒ ⇒
   必须把「什么形态会静默降级」**钉成反向断言**，而不是写在文档里。
5. **展开期 / 静态析构期两道门是既有行为**（`state_machine.h:509` / `if_stmt.h:135` 的
   `std::uncaught_exceptions() == 0` + `in_static_destruction()`）⇒ ⇒ 任何新形态 MUST **复制**这两道门
   ⇒ ⇒ 否则新增一条 `std::terminate` 路径。

## What Changes

> ⚠️ **本段不替 architect 拍板**（对齐 `mem-port-arg-cross-layer-2026-11.md:19` 的「不拍板」口径）
> ⇒ ⇒ 以下选项**穷举两个轴**的组合，**不给倾向、不给 ✅/❌ 评价**。

### 取舍的**两个轴**

| 轴 | 取值 |
|---|---|
| **检出触发点** | ① **类型析构**（现状）· ② **显式 API**（`finalize()` / 作用域守卫）· ③ **elaboration 期扫描**（`describe()` / device 构造收尾统一查）· ④ **从不**（只靠约束与评审） |
| **析构异常规格** | `noexcept`（⇒ 析构期抛 ⇒ `std::terminate`）· `noexcept(false)`（⇒ 可捕获 ⇒ 但类型不可组合） |

### 候选项（**由 architect 选定；两处分别选**）

| 项 | 触发点 | 析构规格 | 可组合 | 检出 | 代价 / 风险 |
|---|---|---|---|---|---|
| **A** | ① | `noexcept(false)` | ❌ | 自动 + 可捕获 | 下游永久走指针持有 ⇒ 检出降级为 abort；「不可组合」无任何提示 |
| **A′** | ① + ④ | `noexcept(false)` | ❌ | 自动 + 可捕获 | 同 A，但**加**「使用约束 + `static_assert` 反向钉住」⇒ 把「不可组合」变成**显式可查**（约束可写进 capability） |
| **B** | ② | `noexcept` | ✅ | **opt-in** + 可捕获（显式路径） | 忘记显式终结 ⇒ 兜底 abort（须判展开期）；检出从「自动」变「opt-in」 |
| **C** | ① 或 ②（按模板实参） | 按实参 | ✅（`false` 特化下） | 取决于实参 | ⚠️ **源码兼容性**：`<S,N,false>` 与 `<S,N>` 是**不同类型** ⇒ 跨函数传参 / 容器 / `template<template<class,size_t> class>` 形态的下游会断；且 `false` 形态下未 resolve 的贡献**静默丢弃** ⇒ 该特化 MUST `CHWARN` |
| **D** | ② | `noexcept`（守卫自身另议） | ✅（若守卫只是**作用域内临时对象**） | **opt-in** + 可捕获 | ⚠️ 守卫的析构若为 `noexcept(false)` ⇒ 它**自己**就是「析构非 noexcept 的类型」⇒ **触犯本项自己的约束** ⇒ 必须显式豁免并写明理由；⚠️ 术语撞车（`guard` 在 chlib 已被 `guard_contribution.h` 占用 ⇒ 建议改名，如 `scope_check()` / `lifecycle_scope()`） |
| **E** | ③ | `noexcept` | ✅ | **自动** + 可捕获 | 需一个**收尾检查点**（`describe()` 结束 / device 构造结束）⇒ 新增框架机制（⚠️ 仓内 `if_stmt.h:109` 的 thread_local 注册表是**激活栈**形态 ⇒ **不可**直接复用 ⇒ 且 `describe()` 之后**局部 FSM 已销毁** ⇒ ⇒ 成本含**共享状态** + **core 层钩子**）；⚠️ 静态对象 / 非 `Component` 上下文里的 FSM **无收尾点** ⇒ 该窗口的检出**静默降级** ⇒ MUST 写死 |

### 已进**否决表**的形态（**不进选项**）

| 形态 | 否决理由（实测） |
|---|---|
| `std::optional<Sm>` 持有 | `~optional()` 推导为 `noexcept` ⇒ **编译期无诊断 + 运行期 terminate** ⇒ 比「编译期撞墙」更危险 |
| `[[no_unique_address]]` | **不参与**隐式析构异常规格推导（只改布局）⇒ 无关 |
| 派生类显式 `~D() noexcept override {}` | 能编译，但成员析构抛出 ⇒ `std::terminate` ⇒ 检出降级为 abort（**下游已知逃生口**，不作为上游方案） |

### 使用约束的**精确措辞**（草案措辞 ⇒ 供 ADR 定稿）

> **析构非 `noexcept` 的类型** MUST NOT 作为「**有虚析构**的类」的**按值成员**或**基类**
> —— 派生类的隐式析构异常规格由子对象推导 ⇒ `noexcept(false)` 子对象 ⇒ 派生类析构 `noexcept(false)`
> ⇒ ⇒ override 一个 `noexcept` 虚析构 ⇒ **编译错误**。

⚠️ **不得**承诺「编译器会说人话」—— 类型系统无法在成员声明处产出**可行动**诊断
⇒ ⇒ 约束的价值在于**文档 + capability + 评审**，不是编译期提示。

### 落地动作（随裁定细化 · 归 builder）

1. `include/chlib/state_machine.h` / `include/chlib/if_stmt.h`：按裁定改析构异常规格 / 加 API / 复制展开期门；
2. 2 份 capability spec：MODIFIED / ADDED delta（**禁** `--skip-specs`）；
3. `tests/test_chlib_composability_api.cpp`（新）：可组合性的机械判据（见 §Acceptance）；
4. `tests/chlib/test_if_stmt.cpp`：补「忘记 `.end()` ⇒ 析构期可捕获」基线用例；
5. `scripts/` + CI：判据的护栏与挂载。

## Acceptance

> ⚠️ **判据原则**：每条 MUST 有**可机械判定**的形式 ⇒ ⇒ **禁**「可行动地失败」「有结论」这类散文。

- [x] **ADR-067 落库**：`docs/adr/ADR-067-dtor-exception-spec-composability.md` · Status = **Accepted**
      （2026-11-09 · 用户拍板）· 四要素齐（元数据 / 选项对比表 ≥ 3 行 / 独立「后果·代价」章 / Verification 章）
      · **oracle → critic 双审 0 BLOCKER**（第 1 轮 1 BLOCKER ⇒ 复审 `CLOSED` / 新 BLOCKER `NONE` / `mergeable: yes`）
      · 审稿记录 = `.rddf/reviews/ADR-067-dtor-composability.md`（oracle 6 条 + critic 9 条 **逐条应答**）。
- [x] **两处分别有结论**：`ch_state_machine` 与 `conditional_block` 在 ADR-067 中**各自**被显式处置
      （决策 ② = FSM 提供可组合形态 · 决策 ③ = `conditional_block` 维持 `noexcept(false)` · 改 / 维持 + 理由）
      ⇒ ⇒ **不得**以「同形态同批改」一笔带过。
- [ ] **可组合性判据（机械）**：`tests/test_chlib_composability_api.cpp`（**仓外视角**：只 include 公共头
      + 真编译 + 真链接 + 真运行）至少含：
      - `static_assert(std::is_nothrow_destructible_v<Dut>)`，其中 `struct Dut : ch::Component { Sm sm_; };`
        （**用仓内 `Component` 作基类** ⇒ 与下游**同形**）；
      - **静默降级 MUST 钉成「落差」而非「取反」**（⚠️ 取反 = 恒假断言 ⇒ 判据永不可达）：
        `static_assert(!std::is_nothrow_destructible_v<Sm>)` **且**
        `static_assert(std::is_nothrow_destructible_v<std::optional<Sm>>)`
      - 视裁定补一条**反向**断言（如选 B/D/E ⇒ `static_assert(std::is_nothrow_destructible_v<Sm>)`）。
- [ ] **判别力自证**：把头里的 `noexcept` 翻回 `noexcept(false)` ⇒ 上面的 fixture **必红** ⇒ 还原复绿
      （仓内惯例：`--self-test` 形态的**原地投毒**）。
- [ ] **基线用例补齐**：`tests/chlib/test_if_stmt.cpp` 新增「作用域内忘记 `.end()` + 输出缺 `else_`
      ⇒ **析构期**抛出**可捕获**异常」用例 ⇒ ⇒ `chlib-conditional-semantics` 的析构 Scenario **首次有测试背书**。
- [ ] **展开期不抛**：新增/保留用例证明「异常展开中 ⇒ 析构不抛」⇒ ⇒ **无新增 `std::terminate` 路径**。
- [ ] **capability delta 合入主 spec**：`bash scripts/verify_openspec_delta.sh` 绿
      + `openspec validate --specs --strict` 绿（⚠️ CLI 须 **1.4.1**，见 `AGENTS.md` COMMANDS）
      + **符号级不变量**：`grep -c 析构 openspec/specs/chlib-state-machine-semantics/spec.md` 由 **0** 变为 **≥ 1**
      ⇒ 且 `chlib-conditional-semantics` 的 `noexcept(false)` MUST 被**显式处置**（改写或保留 + 理由）。
- [ ] **计数锚点同步**：新增 ctest target ⇒ `AGENTS.md` 的 `CTEST_TOTAL`（**253**）同步
      ⇒ `bash scripts/verify_test_counts.sh` 绿。
- [ ] **使用约束已固化**（**词法可查**，不承诺编译期提示）：2 份头注释 + 2 份 capability spec
      MUST 含 §What Changes 的精确措辞 ⇒ ⇒ `grep` 可判。

## Capabilities

- **涉及 capability**：
  - `chlib-state-machine-semantics` —— 视裁定 **MODIFIED**（析构契约）或 **ADDED**（使用约束）；
  - `chlib-conditional-semantics` —— 视裁定 **MODIFIED**（析构 MUST 的收窄 / 维持 + 使用约束）。
- **可能新增公开 API 面**：按裁定可能新增终结 / 作用域守卫 / 收尾扫描机制 ⇒ **不**允许只改头文件 ⇒ MUST 走 ADR + delta。
- **不改** `core/` 的任何公开契约（`ch_reg` 单 next 源 / `ch_out` 单次驱动）⇒ 与 ADR-050 D8 同型边界。
- **不新增 capability**（本项不引入新语义域 ⇒ 归**既有** capability 的 MODIFIED / ADDED）。

## Impact

- **改动面**：`include/chlib/state_machine.h` · `include/chlib/if_stmt.h` · 2 份 capability spec ·
  `tests/test_chlib_composability_api.cpp`（新）· `tests/chlib/test_if_stmt.cpp` · `tests/test_state_machine.cpp`
  · 新增护栏脚本 + CI 挂载。
- **下游迁移成本**（按选项差异极大 ⇒ **这正是要裁定的**）：
  - 选 B/D/E ⇒ 下游**按值持有**重新可行 ⇒ 可回退 `unique_ptr` ⇒ **检出恢复可捕获**；
  - 选 C ⇒ 下游改一个模板实参即可 ⇒ 迁移成本最低 ⇒ ⚠️ 但**源码兼容性**断（不同类型）；
  - 选 A/A′ ⇒ 下游维持指针持有 ⇒ **检出永久降级为 abort** ⇒ 与当前实况一致（零迁移）但能力永久受损。
- **风险**：
  - ⚠️ 析构异常规格是**类型级**属性 ⇒ 改它**传染**到所有把该类型当成员的类 ⇒ ⇒ 必须有 fixture + 变异自检兜底；
  - ⚠️ 条件块侧改 `noexcept` ⇒ **降级兜底合成**（功能）⇒ ⇒ 必须单独裁定 + 单独测试；
  - ⚠️ 展开期门缺失 ⇒ **新增 `std::terminate` 路径**（用户 `describe()` 因别的异常展开时）；
  - ⚠️ 静态析构期门（`in_static_destruction()`）**当前永不生效**（ADR-050:166 自陈 3 个调用点全被注释）
    ⇒ ⇒ 「保持既有守卫」**无法用行为测试验证** ⇒ ⇒ 或按 `tests/test_component_io_const_and_node_name_policy.cpp:69-70`
    的做法**手工置位**测它，或在验收里明写「该门为 dormant · 本轮只做形态保持」；
  - ⚠️ `ch_state_machine` 有**用户声明的析构** ⇒ **无隐式移动构造**（拷贝仍在 ⇒ 放进 `vector` 会**拷贝**
    并复制 `contributions_`）⇒ ★ **ADR-067 决策 ⑨ 已裁定「不删拷贝」**（避免对下游再增一处源码破坏）
    ⇒ ⇒ 本项 MUST 只要求「头注释显式警告（MUST NOT 按值传参 / 放进容器 / 显式拷贝）」
    ⇒ ⇒ 「删拷贝」= **独立后续项**（理由 = 「浅拷贝节点句柄 = 语义地雷」，比「防假阳性」更硬）。
- **不做 / 延后的代价**（如实登记）：下游编译破坏**持续存在** ⇒ 下游永久走指针持有 ⇒
  ADR-050 D5 的「未 resolve 贡献」检出在下游**永久降级为 abort** ⇒ 且「把局部 FSM 提为成员」这个
  **标准重构**在上游**始终不可用** ⇒ ⇒ 上游的 `Component` + FSM 组合**自身**缺一块能力。
- **下游诉求的状态**（如实登记）：下游的 `is_built()` 诉求**紧迫性已解除**（已改断言 `entry_state()` +
  `state_out().impl() != nullptr`）⇒ ⇒ 本项**只承接**「析构可组合性」这一半。

## 上游 issue 原文（附录 · 引用自 https://github.com/chisuhua/CppHDL/issues/28 · 2026-11-08）

**title**: `[下游反馈] ch_state_machine 的 is_built() 移除与 noexcept(false) 析构破坏跨仓下游编译`

**要点摘录**（原文见 issue）：

- ① `is_built()` 被 `c7b0b28` 移除，理由「零消费者」⇒ 该判定**对 CppHDL 自身仓库成立**，但跨仓消费者存在
  ⇒ 建议：恢复等价状态查询，或**至少**在移除清单 / CHANGELOG 标注「跨仓消费者需自行迁移」。
  ⇒ ⇒ **本项已拆出**（见 `public-api-remove-visibility-2026-11`）。
- ② `~ch_state_machine() noexcept(false)` 与「按值持有 + 继承 noexcept 析构基类」**语言层面不兼容**
  ⇒ 建议：把检出从析构路径移出（显式 `finalize()` / `validate()`，析构保持 `noexcept`），或提供 `contrib` 的 RAII 作用域要求。
  ⇒ ⇒ **本项承接这一半**（选项 B / D / E 即其变体）。
- 下游处置：`unique_ptr` 懒构造（与下游其余 3 处 FSM 插件一致）⇒ 上游检出走 `std::terminate` ⇒ 若上游采纳建议 ②，下游可改回按值持有。
- 影响面：上游 `include/chlib/state_machine.h`；下游 `include/cf/plugin/multi_cycle_fsm.h`（框架级 FSM 模板 · 被 3 个 CH_MEM 插件继承）。

<!-- 引用自 https://github.com/chisuhua/CppHDL/issues/28 (2026-11-09) -->
