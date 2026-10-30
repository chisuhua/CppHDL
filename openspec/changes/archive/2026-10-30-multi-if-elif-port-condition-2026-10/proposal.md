# 提案：`multi_if::elif` 的条件 MUST **也**能是端口（E3 的延伸）

- **来源**：`samples/if_demo.cpp` 归位时的**实测编译错误**
- **规格影响**：capability `chlib-conditional-semantics` ⇒ **MODIFIED ×1**
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**小**（`include/chlib/if.h` 两处形参 + 归一化 ⇒ **零** `core/` 改动）

## Why

⚠️ **实测编译错误**（2026-10-30，归位 `if_demo` 时撞上）：

```
samples/if_demo.cpp:79:29: error: no matching function for call to
  'chlib::multi_if<ch::core::ch_uint<8> >::elif(
     ch::core::ch_in<ch::core::ch_bool>&, ch::core::ch_uint<8>)'
```

⇒ ⇒ **根因**：`multi_if::elif` 的条件形参**仍是** `const ch_bool &`
⇒ ⇒ 而 A3（E3）只把 **`if_then` 的**首分支**条件模板化
⇒ ⇒ ⇒ 「`if_then` 的条件可以是端口、`.elif` 的不行」
= **同一能力面**上的**框架内不一致**。

⇒ ⇒ **这不只是「sample 写错了」** —— 它暴露了 A3 的**修复不完整**
⇒ ⇒ 而 A3 的**护栏也没抓到**（R3 只检查 `if_then` 的条件模板化）
⇒ ⇒ ⇒ **又一次「护栏自身有盲区」实例**（本仓已有 5 个同类先例）。

## What Changes

- `multi_if` 的 **ctor** 与 **`elif`** 的条件形参：`const ch_bool &`
  ⇒ `const C &condition` + 经 **`to_operand`** 归一化
  ⇒ ⇒ 端口 / 硬件类型 / 表达式**都**可用；
- ⚠️ 归一化**不新建节点**（`to_operand` 对硬件类型走 `get_lnode` ⇒ 取现有节点）
  ⇒ ⇒ 零额外节点 ⇒ ⇒ 既有行为**逐位不变**；
- ⚠️ **`if_then` 与 `multi_if` 的归一化是幂等的**（两次都走 `to_operand`
  ⇒ 第二次取现有节点）⇒ ⇒ 无副作用；
- **护栏 R3 补强**：`elif` + ctor 的**正 + 负**检查 + **原地投毒**的
  self-test ⇒ ⇒ 「投毒副本不会被 include ⇒ 判据恒绿」这个坑被避开。

⇒ ⇒ **契约载体**：capability `chlib-conditional-semantics`（**MODIFIED ×1**）。

## Acceptance

- [ ] `if_then<ch_uint<8>>(port, v).elif(port, v2).else_(v3)` MUST **编译通过**；
- [ ] `multi_if<ch_uint<8>>(port, v).elif(port, v2).else_(v3)` MUST **编译通过**；
- [ ] **运行期**语义**逐位不变**（既有 `multi_if` 用例全绿）；
- [ ] 护栏 R3 MUST 覆盖 `elif` + ctor（**正 + 负**）；
- [ ] **self-test** 投毒 `elif` 的条件 ⇒ MUST 转红 ⇒ ⇒ 还原后 MUST 再确认绿；
- [ ] `ctest -LE perf` 零回归；**计数锚点不变**。

## Capabilities

- `chlib-conditional-semantics` ⇒ **MODIFIED ×1** ⇒ 归档**禁** `--skip-specs`。

## Impact

- **改动文件**：`include/chlib/if.h`（主体）·
  `tests/fixtures/multi_if_output/pos_output_assignability.cpp`（补 2 种组合）·
  `scripts/verify_multi_if_output_assignability.sh`（R3 补强 + self-test）。
- **爆炸半径**：`if.h` 的使用者（`chlib.h` / `samples/conditional_demo.cpp` /
  `samples/if_demo.cpp` / `tests/chlib/test_*`）⇒ ⇒ **小**。
- **风险**：**低** —— 归一化对硬件类型**逐位等价**（`to_operand` 走 `get_lnode`
  ⇒ 不新建节点）⇒ ⇒ 既有行为零变化。

## Non-Goals

- **不**改 `multi_if` 的**宽度**规则（ADR-052 已落地）；
- **不**改 `core/`（**零**改动）；
- **不**处理「块外裸直写覆盖条件合成」（N7 ⇒ 独立 change）。
