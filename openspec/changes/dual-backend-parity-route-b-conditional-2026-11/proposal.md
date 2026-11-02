# 提案：路线 B 第 2 批 —— 条件语义的两条**唯一实现点**进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **H 批**（路线 B）
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 显式声明 `skip_specs: true`）
- **规模**：2 个新测试文件 + 2 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

ADR-051 把「条件 / 状态守卫」抽象成**一处实现、两处使用**的两个组件。
本批覆盖的正是这两个**唯一实现点**在 **codegen 路径**上的行为：

| 文件 | 覆盖的组件 | 为什么值得双后端 |
|---|---|---|
| `tests/chlib/test_if_stmt_dual.cpp` | `chlib::conditional_block`（`_if` 语句块 DSL） | 该 DSL 是仓内**唯一**的条件赋值入口；且它是本仓「**假绿**」病的**原型病例** —— ADR-051 记录的实现缺陷（`end()` 先置 `finalized_` 再遍历分支体 ⇒ **条件被彻底忽略**）曾被**既有测试钉住**（原注释明写「pins down that current behavior」）⇒ 绿灯承载的是缺陷。⇒ 「条件真的参与了求值」的判据是该病种的**疫苗** |
| `tests/chlib/test_guard_contribution_dual.cpp` | `chlib::compose_guard_chain` | 它是仓内**唯一**一处「多条 (守卫, 值) 合成一条 `select` 链」的代码 ⇒ **「优先级方向（先匹配者胜）」的唯一定义点**。⚠️ 而那条契约在 ADR-050 的状态守卫侧是**运行期不可观测**的（`is_in(S)` 两两互斥 ⇒ 链序怎变结果都相同）⇒ 只能靠**结构级**断言钉住 ⇒ **结构断言验的是图，不是行为** |

⚠️ 两者合起来才覆盖完整面：`_if` 是**语法层**，`compose_guard_chain` 是**合成器**。

## 为什么**不是**转换（如实说明）

- `tests/chlib/test_if_stmt.cpp`（10 用例）：**4** 个是**构图期**判据（输出无
  `else_` 被拒绝 · `validate` 抛异常 ×2 · `end()` 遍历所有分支体的捕获语义）
  ⇒ 对两后端是**同一段 C++ 代码** ⇒ dual 化**零增量价值**。
- `tests/chlib/test_guard_contribution.cpp`（4 用例）：**全部是结构级**断言
  （最外层 mux 的守卫 = 第一个贡献）⇒ 它**验的是图**，且**不跑仿真**
  ⇒ 没有可 dual 的运行期语义。

⇒ 本 change 是**新增**（路线 B）：两个原文件 **MUST NOT 修改**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_if_stmt_dual.cpp`（**新**） | **4** 个 pair：`then/else_` 两路 · 先匹配者胜 + 无 `else_` **保持** · 嵌套内层优先 · **输出端口**的显式默认尾部 |
| `tests/chlib/test_guard_contribution_dual.cpp`（**新**） | **1** 个 pair：**非互斥**守卫 ⇒ 链序**运行期可观测** ⇒ `g0=1,g1=1` MUST 取 `g0` 的值 |
| `tests/CMakeLists.txt` | 2 条 `add_dual_backend_test` |
| 计数锚点 | `CTEST_TOTAL` 215 → **217** · `VERILATOR_E2E_TOTAL` 47 → **49** · `DUAL_BACKEND_PARITY_TOTAL` 38 → **40** · `PAIRS` 212 → **217** · non-perf 204 → **206** · `tests/chlib/test_*.cpp` 33 → **35** |

## 期望值（**手推**，禁抄仿真输出）

`_if` 的语义（ADR-051）：`select(cond, v_then, v_else)`；无 `else_` ⇒ **保持**；
「先匹配者胜」⇒ 先收集的守卫在链的**外层**。

`compose_guard_chain` 的语义（头文件）：`acc = select(g0, v0, select(g1, v1, tail))`
⇒ `g0` 命中时 `v0` 胜 ⇒ **链序运行期可观测**（守卫**非互斥**时）。

⚠️ **实测记录（写进代码注释）**：
- `ch_in<ch_bool>` **不能**绑到 `const ch_bool&`（ADR-051 的 E3）；
- 用 `ch_reg<ch_bool>` 中转会让守卫**多两拍**延迟 ⇒ 金标退化成难读的偏移序列
  ⇒ ⇒ 改用 **`port<ch_uint<1>> == ch_uint<1>(1_d)`**（**组合直读、同拍**）；
- `port<ch_uint<N>>` **没有** `bits<>` 成员（实测编译失败）。

## Verification（前瞻 · 可勾选）

- [ ] **N2′**：每个文件「断言数(with verilator) == **2 ×** 断言数(interpreter-only)」
      ⇒ `26/13` · `8/4`
- [ ] 期望值**全部手推**（文件头逐拍写明）
- [ ] 两个原文件 **MUST NOT 修改**
- [ ] 变异证明：M1 绕过 `conditional_assign`（条件被忽略）⇒ 红 ·
      M2 反转 `compose_guard_chain` 的链序 ⇒ 红
- [ ] `ctest -LE perf` 零回归
- [ ] 计数锚点同步且两个护栏双绿
