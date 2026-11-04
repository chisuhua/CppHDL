# Proposal: 层次化 RTL 输出 · 第 2 段 —— 归属真源（`hierarchical-rtl-ownership-fix-2026-11`）

## Why

### 缺陷（实测 · ADR-059 §1.1）

| 形态 | 实测 | 根因 |
|---|---|---|
| 成员的 `ch_reg` 归**父**组件 | `top.m.r <- parent=top.m`（r 实属 Leaf） | 建节点时 `Component::current()` 还是父组件（成员构造早于 `build()`） |
| 成员 `ch_module` 挂**祖父** | `top.children = 4`，含两个同名 `top.leaf` | 同上 |
| 成员 `ch_module` **直接消失** | `context()==nullptr` ⇒ `module.h:49-53` 早退 ⇒ 后续 `io()` ⇒ **SIGSEGV** | 构造期 `ctx_` 为 `nullptr`（`src/component.cpp:13`） |

⇒ ⇒ 按「节点归属」切模块 ⇒ ⇒ **会把子模块的状态划进父模块** ⇒ ⇒ 产出的 RTL
**看起来对、实际错**，且**无任何告警** ⇒ ⇒ ⇒ **先修归属，再写 emitter**
（ADR-058 §2.5 把这条定为硬前置）。

⚠️ **归属不是纯命名属性**：`src/simulator_init.cpp:95` 用 `!node->get_parent()`
把节点分到 `input_instr_list_` / `combinational_instr_list_`；`include/core/io.h` 的
4 处方向 `CHREQUIRE` 也用 `get_parent()` ⇒ ⇒ ⇒ **归属真源变更 ⇒ 仿真分流可能变**
⇒ ⇒ ⇒ 「仿真逐位不变」是**待验证项**，不是可假定的结论（ADR-059 D7）。

## What Changes

1. **作用域栈**（ADR-059 §2.1）：`Component::Component()` 压入 CONSTRUCT 作用域、
   **构造函数出口**弹出（配对 ⇒ 无泄漏 ⇒ **无需**析构兜底）；`build_internal()` 压入
   BUILD 作用域、出口弹出；`create_child` 路径**不进入**任何作用域。
2. **`Component::node_owner()`**：节点归属的**唯一真源**（作用域栈顶，空则回落 `current_`）。
3. **`ch_module` 的 parent** 取「最内层作用域的组件」，且 `build` context 取
   **环境 context**（D2′）⇒ ⇒ 成员形态**可用**且**不需要延迟构造**。
4. **跨 ctx 守卫**（D2″）：基准**分两支** —— ① 构造期**有** env ctx ⇒ `build(ctx)` MUST
   等于它；② 构造期**无** env ctx（D2′ 明确允许）⇒ ⇒ 以**首次** `build(ctx)` 绑定的 ctx
   为基准，其后换 ctx ⇒ `CHFATAL_EXCEPTION`。
5. **`ch_module` 无 active parent ⇒ `CHFATAL_EXCEPTION`**（D8；**MUST NOT** 只打日志）。
6. **组件树登记双向完整**（D9/D10）：`add_child` MUST 补绑 `parent_`；归属判定 MUST
   **只向下走 `children()`**（MUST NOT 用 parent 链）⇒ ⇒ 成员子组件**不被误杀**。
7. **`io.h` 的 4 处方向守卫与归属同源**（D7）：守卫判的是
   `lnodeimpl::current_component()` ⇒ ⇒ 把它改为返回 `Component::node_owner()`
   ⇒ ⇒ 守卫**零文本改动**却与归属**同源**（MUST NOT 各写一套判定）。

## 不做（Non-Goals）

- **不**改粒度语义（`ch_layer` / `ch_inline` / `is_layer()`）⇒ ⇒ 那是 **T3-①c**。
- **不**改命名（`local_name()` / `module_type_name()` / 合并宏）⇒ ⇒ 那是 **T3-①a**。
- **不**写 emitter ⇒ ⇒ 那是 T3-②/③/④。
- **不**改 `toVerilog()` 的语义（端口筛选 / 算子映射 / `always_ff` / mem 一律不动）。
- **不**「事后归属规则」（`parent_ == nullptr` ⇒ 归顶层）⇒ ⇒ 归属**只由作用域栈决定**。

## Acceptance

- [x] Leaf 的 `ch_reg` 成员 `get_parent()` MUST 是 Leaf；成员形态 `ch_module` MUST 挂到
      **直接父组件**且可 `io()`（不早退）
- [x] **顶层 `ch_device<Pipeline>` MUST 可实例化且不 SIGSEGV**
      （⇒ `examples/removed_examples.txt:26` 归位条件解除）
- [x] 作用域**配平**：任意构造/析构序列后栈 MUST 为空；MUST 有「故意 `create_child`
      不 build」的用例（栈不得被污染）；构造函数抛出时 MUST 由 RAII 自动回退
- [x] **归属 ↔ 语义分流**：`type_input` 节点在 `input_instr_list_` /
      `combinational_instr_list_` 的归属 MUST 与变更前**逐项一致**
- [x] `include/core/io.h` 的 4 处方向守卫：判据 = **「改前改后均无触发」**
      （ADR-059 D7 **v2.2**）⇒ ⇒ **MUST NOT** 用「触发面逐项一致」
- [x] 跨 ctx 守卫基准**分两支**（见 What 4）；env ctx 为空时 MUST **NOT** 硬失败
- [x] `ch_module` 无 active parent ⇒ `CHFATAL_EXCEPTION`；
      **根组件 / 手工装配 MUST NOT 被本条拒绝**
- [x] 组件树登记**双向完整**：成员子组件 `parent()` MUST 非空
      （⚠️ 「归属判定 MUST 只向下走 `children()`」是**实现约束**，其实现体在 **T3-② 分析层**
      ⇒ ⇒ 本 change 只保证该约束成立的**前提**：`children()` 与 `parent()` 双向完整）
- [x] 归属断言 MUST 用 `CHFATAL_EXCEPTION`（**MUST NOT** 用只打日志的 `CHREQUIRE`）
- [x] **零回归**：`ctest -LE perf` / `run_all_samples.sh` / `run_all_ported_tests.sh` 全绿；
      计数锚点同步

## Capabilities

| capability | delta |
|---|---|
| `component-ownership-and-naming` | **ADDED ×5**（R1 归属由构造作用域唯一决定 · R5 build ctx 显式来源 · R6 跨 ctx 再装配被拒 · R7 `ch_module` 无 parent 响亮失败 · R8 组件树登记双向完整） |

⇒ 归档**禁** `--skip-specs`。

---

## 实施记录（**判据 → 证据**）

| Acceptance | 证据 |
|---|---|
| Leaf 的 `ch_reg` 归属 Leaf · 成员 `ch_module` 挂直接父 | `tests/test_component_ownership.cpp` 的 `[ownership][component]`（`top.mid.leaf` + `top.mid.leaf._reg`） |
| **顶层 `ch_device<Pipeline>` 可实例化且不 SIGSEGV** | 同上 `[ownership][component][pipeline]`：`ch::ch_device<riscv::Rv32iPipeline>`（7 个成员 `ch_module` 全部挂直接父 + 各拥有节点 > 0） |
| 作用域配平（含 `create_child` 不 build / 构造抛异常） | 同上 `[ownership][component][scope]`（3 个 SECTION） |
| 归属 ↔ 语义分流 | 同上 `[ownership][component][simulator]`（`type_input` 无新增无归属节点） |
| `io.h` 4 处守卫「改前改后均无触发」 | 同上 `[ownership][component][guards]`（4 个条件逐条成立） |
| 跨 ctx 守卫两支 · env ctx 为空不硬失败 | 同上 `[ownership][component][ctx]`（2 个 SECTION + defer 用例） |
| `ch_module` 无 active parent ⇒ `CHFATAL_EXCEPTION` | 同上 `[ownership][component][module]`（`REQUIRE_THROWS_AS`） |
| 组件树登记双向完整 | `[ownership][component]` + `[ownership][component][pipeline]`（`children()` 与 `parent()` 双向） |
| 归属断言 MUST 用 `CHFATAL_EXCEPTION` | `src/component.cpp` / `include/module.h` 的 3 处 `CHFATAL_EXCEPTION`（`grep -c CHFATAL_EXCEPTION`） |
| 零回归 | `ctest -LE perf` 234/234 · samples 39/39 · examples 28/28 |

**变异 6/6 全抓**（每条先确认可编译）：① `lnodeimpl` 回落 `current()` ② `ch_module` parent 回落
`current()` ③ 去掉 `rebind_children_parents()` ④ 分支②守卫移到 `built_` 早退之后
⑤ 去掉分支①守卫 ⑥ 去掉 D10 抑制 ⇒ 全部 `RED`，还原后复绿。

⚠️ **三处实施期订正**（ADR 推导不完整、决策未变）见 `design.md` §4。
