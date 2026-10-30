# 提案：`multi_if` 的**输出可赋值性**（E1）与**条件可接受性**（E3）

- **来源**：ADR-052 的 Non-Goal #6（明确「**不在本 ADR**」⇒ 归独立 change）
- **规格影响**：capability `chlib-conditional-semantics` ⇒ **MODIFIED ×1**
  （E1 / E3 两个 Scenario **原地订正**）⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**小**（实现面 = `include/chlib/if.h` 一个文件；
  **零** `core/` 改动；爆炸半径 = `if.h` 的使用者（`chlib.h` / `samples/conditional_demo.cpp` /
  `tests/chlib/test_*`））

## Why

**E1**：`multi_if` **不能**直接赋给 `ch_out` ⇒ ⇒ **编译不过**：

```
include/core/io.h:115:39: error: no matching function for call to
  'get_lnode(const chlib::multi_if<ch::core::ch_uint<9> >&)'
```

⇒ ⇒ 根因：`ch_out<T>::operator=(const U &)` 走 `get_lnode(value)` ⇒ 而
`multi_if<T>` **无** `impl()` ⇒ ⇒ **无匹配重载**。

⇒ ⇒ 而 `select` 对同类操作数**可用** ⇒ ⇒ **框架内不一致**。

**E3**：`if_then` 的条件**不能**是 `ch_in<ch_bool>` 端口 ⇒ ⇒ **编译不过**：

```
error: no matching function for call to
  'if_then(ch::core::ch_in<ch::core::ch_bool>&, ch::core::ch_uint<8>&)'
```

⇒ ⇒ 根因：条件形参是 `const ch_bool &` ⇒ ⇒ 端口**不能**绑到它
⇒ ⇒ 而 `_if`（语句块形式）对端口**可用** ⇒ ⇒ **框架内不一致**。

⇒ ⇒ **两条限制此前已被写进 capability**（「权威表面」Requirement 的两个
Scenario，明确写「MUST **编译不过**」）⇒ ⇒ ⇒ 它们是**已登记的限制**，
不是未文档化的行为 ⇒ ⇒ 本次把它们**修掉** ⇒ ⇒ 那两个 Scenario MUST
**原地订正**（⚠️ OpenSpec CLI **拒绝删除** Scenario ⇒ ⇒ 只能原地订正）。

## What Changes

- **E1**：`include/chlib/if.h` **末尾**（`} // namespace chlib` **之后**）加：
  - `namespace ch::core { ch_width_impl<::chlib::multi_if<T>, void> }`
    （⚠️ 特化 MUST 在 `ch::core` ⇒ 在 `chlib` 内部写 `namespace ch::core` **不成立**
    ⇒ ⇒ 必须写在文件末尾）；
  - `namespace chlib { get_lnode(const multi_if<T> &) }` —— ⚠️ **形参 MUST 是
    具体类型** ⇒ ⇒ 它是**类型专用**入口 ⇒ ⇒ **不**违反 ADR-053
    （「MUST NOT 有**无约束的通用**入口」）；
  - ⇒ ⇒ `core/io.h` 的**非限定** `get_lnode(value)` 靠 **ADL** 命中它
    ⇒ ⇒ **`core/` 零改动**。
- **E3**：`if_then` 的条件形参**模板化**（`const C &condition`）+ 经
  `to_operand` 归一化 ⇒ ⇒ 端口 / 硬件类型 / 表达式**都**可用
  ⇒ ⇒ 与 `_if` 的**可表达性对齐**。
  ⇒ ⚠️ 归一化**不新建节点**（`to_operand` 对硬件类型走 `get_lnode`）⇒ ⇒ 零额外
  节点 ⇒ ⇒ 既有行为**逐位不变**。

⇒ ⇒ **契约载体**：capability `chlib-conditional-semantics`（**MODIFIED ×1**）。

## Acceptance

- [ ] `multi_if` **直接赋给 `ch_out`** MUST **编译通过**（**GCC + Clang 各一次**）；
- [ ] `if_then` 的条件为 **`ch_in<ch_bool>` 端口** MUST **编译通过**；
- [ ] 直接赋值 MUST 与「先转 `ch_uint` 再赋值」**逐位等价**（**运行期**断言）；
- [ ] **对照 fixture**（故意用不存在的 API）MUST 编译失败 ⇒ 证明判据有区分度；
- [ ] **fixture 存在性** MUST 有 fail-closed 检查；
- [ ] **变异**：删掉 `get_lnode(multi_if)` ⇒ 转红；条件改回 `const ch_bool &` ⇒ 转红；
- [ ] `ctest -LE perf` 零回归；**计数锚点不变**（用例进**既有** target）。

## Capabilities

- `chlib-conditional-semantics` ⇒ **MODIFIED ×1** ⇒ 归档**禁** `--skip-specs`。

## Impact

- **改动文件**：`include/chlib/if.h`（主体）·
  `tests/chlib/test_multi_if_acceptance.cpp`（1 个 DUT + 1 个用例，**既有** target）·
  `tests/fixtures/multi_if_output/`（2 个 fixture）·
  `scripts/verify_multi_if_output_assignability.sh`（新护栏）+ CI 两处。
- **爆炸半径**：`if.h` 只被 `chlib.h` / `samples/conditional_demo.cpp` /
  `tests/chlib/test_*` 使用 ⇒ ⇒ **小**。
- **风险**：**低** —— E1 是**新增**能力（此前编译不过 ⇒ 无既有行为可破坏）；
  E3 的归一化对硬件类型**逐位等价**（`to_operand` 走 `get_lnode` ⇒ 不新建节点）。
- **计数**：**不变**（用例进**既有** target ⇒ `CTEST_TOTAL` 205 /
  `EXAMPLES_TEST_SOURCES` 17 / `verilator_e2e` 38 / non-perf 194）。

## Non-Goals

- **不**改 `multi_if` 的**宽度**规则（那是 ADR-052 已落地的部分）；
- **不**改 `ch_out::operator=`（`core/` **零改动** ⇒ ⇒ 靠 ADL 解决）；
- **不**处理「块外裸直写覆盖条件合成」（N7 ⇒ 独立 change）。
