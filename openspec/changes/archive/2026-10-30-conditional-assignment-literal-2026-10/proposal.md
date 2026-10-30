# 提案：`conditional_assign` MUST 接受**裸 literal**

- **来源**：实测探针（`/tmp/opencode/n7probe/lit.cpp`）—— 照抄框架**自己**的
  头注释示例时**编译不过**
- **规格影响**：capability `chlib-conditional-semantics` ⇒ **MODIFIED ×1**
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**极小**（实现面 = 1 行 + 注释；**零** `core/` 改动）

## Why

`include/chlib/if_stmt.h` 的**头注释**写着：

```cpp
_if(io().en)
    .then([&] { conditional_assign(count).next() = count + 1_d; })
    .else_()
    .then([&] { conditional_assign(count).next() = 0_d; })   // ← 照抄就编译不过
    .end();
```

⇒ ⇒ **实测**：照抄这段示例 ⇒ **编译失败**：

```
include/core/io.h:455:34: error: no matching function for call to
  'get_lnode(const ch::core::ch_literal_impl<170, 8>&)'
include/chlib/if_stmt.h:362:25: required from
  'void chlib::conditional_assignment<T>::assign(const U&)'
```

⇒ ⇒ **根因**：`assign()` 的 else 分支（**块外直写**）写的是
`(*out_) <<= value;` ⇒ 它走 `operator<<=` ⇒ 用 `get_lnode(driver)`
⇒ ⚠️ `ch_literal_impl` **没有** `get_lnode` 重载（`ch_literal_impl` **不是**
`HardwareType` —— 它**无** `impl()`）⇒ ⇒ literal **编译不过**。

⇒ ⇒ ⚠️ **该分支总被实例化**（`if` / `else` 都是模板代码）⇒ ⇒ 即使运行期
`reg_` 非空（走不到 else）⇒ ⇒ 该行**仍会编译**⇒ ⇒ 一旦有 literal 实参
⇒ ⇒ **整个调用点编译失败** ⇒ ⇒ 「能不能编译」与「走哪条分支」**无关**。

⇒ ⇒ **框架内的真实影响**：`tests/chlib/test_if_stmt.cpp` 的**全部**既有 DUT
都写成 `ch_uint<8>(7_d)`（**显式包裹**）⇒ ⇒ 那正是**绕过**缺陷的写法
⇒ ⇒ 缺陷**从未被触发** ⇒ ⇒ 只有照抄文档示例的人会撞上。

## What Changes

- `assign()` 的 else 分支改为经 **`to_operand`** 归一化：
  `(*out_) <<= ch_uint<ch_width_v<T>>(to_operand(value).impl());`
  ⇒ ⇒ `to_operand` 对 hardware / port / **literal** / 算术**四类**都有分支
  ⇒ ⇒ 全部可用；
- ⚠️ `ch_reg` 路径**已经**OK（`next_assignment_proxy::operator=` 内部走
  `to_operand`）⇒ ⇒ 但**必须**由护栏确认**两条路径都在**（只修一条 ⇒ 另一条
  上的 literal 仍编译不过 ⇒ 且因为**总被实例化**，会**掩盖**「已修」的假象）；
- 头注释的示例**保留裸 literal**（并加注说明）⇒ ⇒ 它成为**文档级**的回归点。

⇒ ⇒ **契约载体**：capability `chlib-conditional-semantics`（**MODIFIED ×1**）。

## Acceptance

- [ ] 裸 literal（`0_d` / `0xAA_h`）在 `ch_reg` 与 `ch_out` 两种 target 上
      **都**编译通过（**GCC + Clang 各一次**）；
- [ ] **对照 fixture**（故意用不存在的 API）MUST 编译失败 ⇒ 证明判据有区分度；
- [ ] **fixture 存在性** MUST 有 fail-closed 检查（⚠️ **实测踩中的护栏盲区**：
      文件不存在时编译器报「No such file」⇒ 会被误判为「编译失败」⇒ **假绿**）；
- [ ] 运行期：裸 literal 的条件赋值语义**正确**（条件真取 `then` / 假取 `else_`）；
- [ ] 运行期：块外的裸 literal 直写语义**正确**（直写，不捕获）；
- [ ] **变异**：撤掉归一化 ⇒ 护栏转红；吞掉块外直写 ⇒ 运行期用例转红；
- [ ] `ctest -LE perf` 零回归；**计数锚点不变**（用例进**既有** target）。

## Capabilities

- `chlib-conditional-semantics` ⇒ **MODIFIED ×1** ⇒ 归档**禁** `--skip-specs`。

## Impact

- **改动文件**：`include/chlib/if_stmt.h`（**1 行** + 注释）·
  `tests/chlib/test_if_stmt.cpp`（2 个 DUT + 2 个用例，**既有** target）·
  `tests/fixtures/conditional_assign_literal/`（2 个 fixture）·
  `scripts/verify_conditional_assign_integrity.sh`（新护栏）。
- **爆炸半径**：`if_stmt.h` 仅被 `samples/conditional_demo.cpp` 与
  `tests/chlib/test_if_stmt.cpp` 使用 ⇒ ⇒ **极小**。
- **风险**：**极低** —— 改动是**纯归一化**（`to_operand` 对硬件类型的行为与
  原路径**一致**）⇒ ⇒ 既有行为**零变化**。
- **计数**：**不变**（用例进**既有** target ⇒ `CTEST_TOTAL` 205 /
  `EXAMPLES_TEST_SOURCES` 17 / `verilator_e2e` 38 / non-perf 194）。

## Non-Goals

- **不**处理「块外对**同一 target** 的**裸**直写会覆盖条件合成」那个缺陷
  （**预防性**护栏 ⇒ **独立** change）；
- **不**给 `ch_literal_impl` 加 `get_lnode` 重载（那会给框架**新增**一条
  「literal → lnode」路径 ⇒ 与 `to_operand` 的既有路径**可能分叉** ⇒ ⇒
  归一化到 `to_operand` 更安全）；
- **不**改 `core/`（**零**改动）。
