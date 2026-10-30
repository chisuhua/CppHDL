# Design: chlib-guard-contribution-priority-2026-10

> 收口「共享守卫贡献」的两个**契约缺口**。本文件承载**推导 / 逐拍证据 /
> 变异清单 / 实施步骤**；**过程日志不进 ADR**。

## 1. 缺口 ①：ADR-050 的贡献优先级被「组合器抽取」**静默翻转**

### 1.1 机制（源码逐行）

**抽取前**（ADR-050 的内联 `compose()`）：

```cpp
ch_uint<W> acc = acc0;
for (size_t i = 0; i < N; ++i) {          // ← **正向**状态序
    for (const auto &c : contributions_) {
        if (c.target != target) continue;
        if (static_cast<size_t>(c.state) != i) continue;
        acc = select(in_s[i], ch_uint<W>(c.value), acc);   // ← **后写的在外层**
    }
}
return acc;
```

⇒ 链的形态 = `select(g_{N-1}, v_{N-1}, select(g_{N-2}, … select(g_0, v_0, tail)))`
⇒ ⇒ **g_{N-1} 最外层** ⇒ ⇒ **高编号 state 优先**。

**抽取后**（共享组合器 `compose_guard_chain`）：

```cpp
ch_uint<W> acc = tail;
for (auto it = contribs.rbegin(); it != contribs.rend(); ++it) {   // ← **反向**
    acc = select(it->guard, ch_uint<W>(it->value), acc);           // ← **先收集的在外层**
}
```

而 `contribs` 是按**正向**状态序收集的 ⇒ ⇒ 链的形态 =
`select(g_0, v_0, select(g_1, … select(g_{N-1}, v_{N-1}, tail)))`
⇒ ⇒ **g_0 最外层** ⇒ ⇒ **低编号 state 优先**。

⇒ ⇒⇒ **优先级翻转了。**

### 1.2 证据（生成 Verilog 的逐字节 diff，2026-10-30 实测）

方法：把 `include/chlib/state_machine.h` 回退到 `102f39b^`（抽取前），对**真实
dogfood**（`examples/spinalhdl-ported/uart/uart_tx.cpp` 的 `UartTxTop`，用
`#define main` 改名后原样 include ⇒ 不是复刻）各生成一次 Verilog，比对。

⇒ 观测到的 `is_in(S)` 比较**创建顺序**（= 链的内→外顺序）：

| | 比较的字面量序列 | 同一 target 多状态贡献时谁优先 |
|---|---|---|
| **抽取前** | `3, 2, 1, 0` | **高编号** state |
| **抽取后** | `0, 1, 2, 3` | **低编号** state |

⇒ 证据文件：`/tmp/opencode/vg_before.v` vs `vg_after.v`（451 行，各）。

⚠️ **关键观察**：mux 链的**嵌套结构本身逐字相同**
（`mux_select_3 = __eq_eq ? … : mux_select_2` 两版一致）⇒ ⇒ 变的**只是**
「哪个 state 被放在最外层」⇒ ⇒ ⇒ **这正是「链序」这一个维度**。

### 1.3 为什么全绿（⇒ 「绿」完全掩盖了它）

ADR-050 的守卫是**状态守卫** `is_in(S)` = `state_reg == ch_uint<STATE_BITS>(S)`
⇒ ⇒ 同一寄存器对**互不相同**的枚举字面量作比较 ⇒ ⇒ **两两互斥是构造保证**
⇒ ⇒ 同一时刻至多一个守卫为真 ⇒ ⇒ 无论链序如何，结果**逐位相同**
⇒ ⇒ ⇒ **运行期不可观测** ⇒ ⇒ ⇒ 既有金标（`uart_tx` 的 `busy` 高 3906 拍 +
`0xA5` 位序；`complex_e2e` 的 `0x96` MSB-first + 状态序列 + `bit_count` 复位）
**100% 绿**。

⇒ ⇒⇒ **推论（比「金标仍绿」更强）**：「链序对 ADR-050 侧不可观测」**可先验
证明**（由互斥性），**不**是**从「金标仍绿」推出**的 —— 「仍绿」是**确认**
而非**证据来源**。

⇒ ⇒⇒ **推论 2**：正因为不可观测 ⇒ ⇒ **金标钉不住链序** ⇒ ⇒ ⇒ **只有结构级
断言**能钉住 ⇒ ⇒ 而本 change 之前**没有任何结构级断言** ⇒ ⇒ ⇒ **该属性零覆盖**。

### 1.4 口径裁决（用户，2026-10-30）

**保留新行为**（=「先匹配者胜」= 低编号 state 优先）。

**理由**：
1. `_if` 语句块与表达式形式 `multi_if` / `if_then` **已经是**该惯例 ⇒ ⇒ 新行为
   与它们**一致**；
2. 若回退旧行为 ⇒ ⇒ 共享组合器的「先匹配者胜」文档对 ADR-050 侧就是**错的**
   ⇒ ⇒ 与 D10「一处实现、两处使用、语义**不漂移**」的初衷**矛盾**；
3. 旧行为（高编号 state 优先）是**偶然反转**（内联循环的副作用）⇒ ⇒ 新行为是
   **修正**而非**破坏**。

⇒ ⇒ **代价**：这不是「纯重构」⇒ ⇒ ADR-051 D10 的该句 MUST 订正 ⇒ ⇒ 且
`chlib-state-machine-semantics` MUST 补上**优先级条款**（该 capability 从未
写过优先级 ⇒ ⇒ 翻转**不违反任何契约** ⇒ ⇒ 无处登记）。

### 1.5 为什么不是「加一条运行期用例就能钉住」

⚠️ **运行期不可观测**（§1.3）⇒ ⇒ 加多少运行期用例都**抓不到**链序翻转
⇒ ⇒ MUST 断言**链的结构**。

**可行的结构断言**（本 change 采用）：

```
compose_guard_chain(contribs, tail) 产出的节点是 mux（mux_select）
⇒ 断言：该 mux 的**条件**端（true 分支侧）对应**第一个**贡献的守卫
⇒ 即：翻转 `rbegin→rend` 为 `begin→end` ⇒ 该断言**转红**
```

⇒ ⇒ 该断言**只依赖节点结构**，**不依赖**运行期求值 ⇒ ⇒ 可在**单测**里做。

## 2. 缺口 ②：`chlib-conditional-semantics` 的两处**无契约载体**

### 2.1 析构兜底语义

ADR-051 **SF4** 裁决过：「`~conditional_block()` 在 `!finalized_` 时调 `end()`
⇒ `end()` 可能抛（输出缺默认）⇒ ⇒ 析构函数 MUST `noexcept(false)` +
**展开期不抛**」。实现于 `include/chlib/if_stmt.h` ⇒ ⇒ 但主 spec **零**提及
⇒ ⇒ 未来改析构语义**不需要任何 spec delta** ⇒ ⇒ 「把静默变受控」的目的在这条
上落空。

⇒ ⇒ 本 change **ADDED ×1**（析构 MUST 兼容抛出路径）。

### 2.2 表达式形式的**接受面限制**

主 spec 声明 `multi_if` / `if_then` 是**唯一权威表面**，却**不记录**该表面的
**可用域**：

| # | 限制 | 实测错误 |
|---|---|---|
| **E1** | 不能直接赋给 `ch_out` | `multi_if has no member named 'impl'` |
| **E2** | 分支值须同类型 T | `multi_if<ch_uint<9>>` 与 `ch_uint<8>` 不匹配 |
| **E3** | 条件不能是 `ch_in<ch_bool>` 端口 | `no matching function for call to 'if_then(ch_in<ch_bool>&, ch_uint<8>)'` |

⇒ ⇒ 这三条只活在 `include/chlib/AGENTS.md` 的 anti-patterns 段
⇒ ⇒ **「权威表面」的可用域无契约** ⇒ ⇒ 后续修复者**无从判断**哪些限制是
**契约**、哪些是**偶然** ⇒ ⇒ 且若后续 change 用「no spec changes」归档 ⇒ ⇒
**契约被静默丢掉**（critic SF3 指出）。

⇒ ⇒ 本 change **MODIFIED ×1**（权威表面条款追加接受面限制的**已知限制 NOTE**）
+ **ADDED ×1**（析构兜底）。

⚠️ 本 change **不修** E1/E2/E3 的**实现** ⇒ ⇒ 只**钉住其契约归属** ⇒ ⇒ 修复
是独立 change（oracle 建议：E3 + E1(chlib 侧) + `if_demo` 归位**同批**）。

## 3. 实施步骤

| # | 动作 | 面 |
|---|---|---|
| 1 | **ADR-051** D10 的「纯重构」句**订正** + §5.4 的 M6 条目补「链序运行期不可观测 ⇒ 需结构级断言」+ §7 追加**实施期实测**记录 | 文档（**决策未变**，仅订正事实表述） |
| 2 | `include/chlib/guard_contribution.h` 头注释：明确「先匹配者胜」的**方向**（先收集的贡献在最外层）+ 记录「抽取前后的方向差异」+ 指向本 change | 注释（零行为变更） |
| 3 | **结构级断言**（新文件 `tests/chlib/test_guard_contribution.cpp`）：断言最外层 mux 的守卫 = 第一个贡献的守卫 | 测试 |
| 4 | openspec delta 合并（归档时）⇒ `chlib-state-machine-semantics` ADDED ×1 · `chlib-conditional-semantics` ADDED ×2 + MODIFIED ×1 | 契约 |
| 5 | 簿记：improvement 草案 + roadmap 行 | planner |

## 4. 变异清单（每条**先确认编译成功** ⇒ 否则跑旧二进制 ⇒ 假绿）

| # | 注入 | 期望转红 |
|---|---|---|
| **M1** | `compose_guard_chain` 的 `rbegin→rend` ⇒ `begin→end` | **结构级断言**转红（⇒ 证明链序**真的**被钉住） |
| **M2** | 同上 | **运行期**用例**仍绿**（⇒ 证明「运行期不可观测」这条事实）—— 这是**反向**判据（期望**不红**） |

⚠️ **M2 是反向判据**：它验证的是「金标/运行期用例**看不见**链序」⇒ ⇒
若 M2 **转红**，说明存在运行期可观测的路径 ⇒ ⇒ 本 change 的「必须用结构级
断言」结论被推翻 ⇒ ⇒ MUST 重新评估。

⚠️ 变异工具链**必须**满足（oracle 建议 1 + critic SF2，本 change 实施时携带）：
① `ctest` 一律加 `--no-tests=error`（堵「模式匹配 0 个测试 ⇒ 空转仍绿」）；
② fail-closed 校验「被观测的 target ⊆ `build_targets`」；
③ 还原后与备份**逐字节**比对（而非自读自比）。

⇒ ⇒ 三者**已入仓**为 `scripts/lib/mutation_harness.py`（仓内**唯一**的变异
harness）⇒ ⇒ spec 与**变异脚本**归档在本 change 的 `mutations/` 目录 ⇒ ⇒
**产生器可复算**（审稿能复核「变异 2/2 全被抓」）。

⚠️ **两条实施期的实测修正**（入仓时才发现）：

1. **硬化 ③ 的「与 HEAD 比对」不能是硬失败** —— 变异目标文件**常常**正带着本次
   change 的**未提交**改动 ⇒ ⇒ 硬失败 ⇒ harness 对未提交文件**完全不可用**
   ⇒ ⇒ 降级为**提示**（`::notice:` 报「基线 = 工作树」）⇒ ⇒ 真正的 fail-closed
   是**还原后逐字节校验**（保持硬失败）。
2. **按行号注入 ⇒ 行号会漂移** —— 目标文件一改注释，spec 的行号就失效 ⇒ ⇒
   harness **拒绝**在错误的行注入（fail-closed）⇒ ⇒ 这是**有意的**权衡：
   「行号漂移 ⇒ 变异跑不起来」优于「行号漂移 ⇒ 变异跑到别处却报绿」。
