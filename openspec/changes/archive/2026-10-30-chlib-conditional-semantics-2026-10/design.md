# Design: chlib-conditional-semantics-2026-10

> 实施 **ADR-051**（`docs/adr/ADR-051-chlib-conditional-semantics.md`）。
> 本文件承载**推导 / 逐拍证据 / 变异清单 / 实施步骤**；**过程日志不进 ADR**。

## 1. 现状的精确失效机制（源码行号）

```
include/chlib/if_stmt.h:84-100   end()
    86:  finalized_ = true;        ← ① 先置位
    92-97: for (...) body();       ← ② 后遍历 ⇒ 捕获期间 is_active() == false

include/chlib/if_stmt.h:157-166   next_proxy::operator=
    159: if (conditional_block::is_active()) {
    160-162:   /* 空 */            ← ③ 即使 ① 修好，此分支也会**丢弃**候选值
    163-165: } else { reg_ref->next = value; }
```

⇒ ⇒ 需**同时**修 ① 与 ③；只修 ① ⇒ 候选值被丢弃（表现为「赋值丢失」）；
只修 ③ ⇒ 分支永不触发（表现为「条件恒假」）。

## 2. 共享组合器（ADR-051 D10）

新增 `include/chlib/guard_contribution.h`：

```cpp
namespace chlib {

/// 一条守卫贡献：(守卫, 值)
template <typename W> struct guard_contribution {
    ch_bool guard;
    lnodeimpl *value;
};

/// 把同一 target 的全部贡献合成**一条** select 链（先匹配者胜）。
/// `tail` = 默认尾部（寄存器=当前值 / 输出=显式默认）。
template <unsigned W>
ch_uint<W> compose_guard_chain(const std::vector<guard_contribution> &contribs,
                                const ch_uint<W> &tail);

}
```

⇒ **合成顺序**：`acc = tail; for (i = n-1 .. 0) acc = select(c[i].guard, c[i].value, acc);`
⇒ ⇒ **先匹配者胜**（与 `multi_if::operator T()` 的既有「从后往前」构链**同序**）
⇒ ⇒ ADR-050 的 `compose()` 改为调用它 ⇒ ⇒ **金标逐项不变**。

## 3. `_if` 的捕获机制

### 3.1 目标与分支表

```cpp
struct conditional_block {
    struct branch_info { ch_bool condition; std::function<void()> body; bool is_else; };
    struct target_entry {
        lnodeimpl *target;                       // 目标节点（reg 的 proxy / output 的 impl）
        lnodeimpl *reg;                          // 非空 ⇒ 是寄存器
        std::vector<guard_contribution> contribs; // 按**分支顺序**追加
        bool has_default;                        // 是否拿到过 else 候选值
        lnodeimpl *default_value;
    };
    std::vector<branch_info> branches_;
    std::vector<target_entry> targets_;
};
```

### 3.2 捕获点

`conditional_assign(target).next() = value`：
- **不在**任何激活的条件块内 ⇒ 直写 `reg->next = value`（保持既有行为）；
- **在**激活块内 ⇒ **不**写 `next` ⇒ 而是把
  `(current_branch.condition, value_node)` 追加到该 target 的贡献表
  ⇒ ⇒ 写入动作推迟到 `end()`。

### 3.3 `end()` 的合成

```
for each target in targets_:
    if (target.reg):                       // 寄存器
        tail = target.reg 的当前值（proxy）
        if (target.has_default) tail = default_value   // 有 else ⇒ 用 else 候选
        chain = compose_guard_chain(contribs, tail)
        target.reg->next = chain           // ← **唯一**写入点
    else:                                  // 输出端口
        if (!target.has_default) THROW     // ← target 级判据（ADR-051 D3）
        chain = compose_guard_chain(contribs, default_value)
        target.target->set_src(0, chain)   // ← **唯一**驱动点
```

⚠️ **else 候选的处理**：`else_()` 分支的条件恒为 `ch_bool(true)` ⇒ 它在链里
**最后**匹配 ⇒ ⇒ **必须**把它从链里**摘掉**并单独作为 `default_value` ⇒
否则 `select(true, v_else, tail)` 会**吞掉**后续分支（虽然 else 恒真 ⇒ 结果
相同，但**尾部**语义会变：寄存器的「保持」尾巴会被 else 吃掉 ⇒ 未匹配时
`mtime` 类的「保持」失效）⇒ ⇒ **else 候选 MUST 单独作为尾部**。

### 3.4 嵌套 ⇒ 栈

`active_block_` 由「单个指针」改为 **`std::vector<conditional_block*>` 栈**：
- `_if()` ⇒ push；
- `end()` ⇒ pop **自己**（若自己在栈顶）；若不在栈顶（异常路径）⇒ 只标记
  `finalized_`，由析构兜底；
- 析构 ⇒ 若 `!finalized_` ⇒ 调 `end()`（既有行为）⇒ **不**弹别人的栈项。

### 3.5 析构兜底 × 抛出路径（ADR-051 SF4）

`~conditional_block()` 在 `!finalized_` 时调 `end()` ⇒ `end()` 可能抛
（输出缺默认）⇒ ⇒ 析构函数 MUST `noexcept(false)` + **展开期不抛**
（`std::uncaught_exceptions() == 0` 才抛）⇒ ⇒ 继承 ADR-050 实测③ 的结论。

## 4. 静态护栏（ADR-051 D6）

新增 `scripts/verify_if_stmt_wiring.sh` + `scripts/lib/if_stmt_wiring.py`：
- **词法级**：闭包（`_if(...)` / `.then(` / `.else_(` 的 lambda 体）内出现
  `->next =` **且**未经 `conditional_assign` ⇒ 违规；
- **同型**：闭包内 `io().x =` ⇒ 违规；
- **共用**：`scripts/lib/fsm_wiring.py` 的 `strip_comments_and_strings` +
  花括号配平（⚠️ `find_closures` 的入口表是 `on_active(` 专用 ⇒ 新护栏**新增**
  `.then(` / `.else_(` 入口 ⇒ 复用**骨架**而非入口表）；
- 方向：**宁可漏报、不误报**（除 parse-error **fail-closed**）。

## 5. `priority_if` 的移除（ADR-051 D8）

`include/chlib/if.h` 的 `priority_if` / `priority_if_then` 与 `multi_if` 的
`operator T()` **逐行等价** ⇒ ⇒ **物理移除**（零活消费者 ⇒ 无别名期）。

⚠️ `samples/conditional_demo.cpp` / `samples/if_demo.cpp` 引用了它 ⇒
⇒ `conditional_demo` 随本 change 改用 `if_then`；`if_demo` 归**独立归位**
（卡在 `ch_in<ch_bool>` 绑定面）。

## 5.5 实施期实测的 3 条框架约束（**写 DSL / 扩 API 时 MUST 携带**）

⚠️ 这三条**不是** ADR 的裁决内容，而是**实施时踩到**的框架事实 ⇒ ⇒ 必须
留痕，否则后人改回去会再次踩坑。

| # | 实测 | 后果 | 处置 |
|---|---|---|---|
| ① | **`ch_in<ch_bool>` 不能绑到 `const ch_bool&`** | 原签名 `const ch_bool &condition` ⇒ **DSL 对「条件来自输入端口」这个最自然的用例不可用** | 条件参数改为**模板**（接受任意硬件操作数）⇒ `conditional_block` 的 ctor / `elif()` / `_if()` 全部模板化 |
| ② | **`operator->` 只对类类型**对象**生效** | 指针上的 `reg->next` **不成立**（`class ch_reg has no member named 'next'`） | 一律写 `(*reg)->next` |
| ③ | **lambda MUST 按值捕获目标指针** | `conditional_assign(x)` 是**临时对象** ⇒ 捕获 `this` 会**悬垂** ⇒ 实测 `regimpl_node_ is null` | writer/composer/committer 三类回调全部**按值捕获** `reg` / `out` 指针 |

⚠️ ①②③ 中 ① 的**表达式形式对应物仍未修**（见 §5.6 的 E3）。

## 5.6 实施期发现的 3 个**独立**缺陷（**不在**本 change 范围）

⚠️ ADR-051 只裁决**语句块**形式 ⇒ ⇒ 下列缺陷属**表达式形式**（`include/chlib/if.h`）
⇒ ⇒ **独立立项**，本 change **只绕行 + 留痕**。

| # | 缺陷 | 实测 | 本 change 的处置 |
|---|---|---|---|
| **E1** | `multi_if` **不能**直接赋给 `ch_out` | `io().x = if_then(...)` ⇒ `multi_if has no member named 'impl'`（走了 bundle 的 `get_lnode` 重载） | `samples/conditional_demo.cpp` 显式 `ch_uint<8>(...)` 绕行 + 注释留痕 |
| **E2** | `multi_if<T>` 要求**所有分支值同类型 T** | `a + b` 是 **9 位**、`a - b` / `a \| b` 是 **8 位** ⇒ 推导冲突（`multi_if<ch_uint<9>>` 与 `ch_uint<8>` 不匹配） | 同上（每个分支显式收敛宽度） |
| **E3** | `if_then` 的条件**不能**是 `ch_in<ch_bool>` 端口 | `if_then(io().en, v)` ⇒ `no matching function` ⇒ **同 ① 的绑定缺陷** | 本 change **不**改表达式形式 ⇒ 登记待独立立项 |

⚠️ E3 与 `samples/if_demo.cpp` 的归位条件**同源** ⇒ ⇒ 两者可**同批**处理。

## 5.7 计数锚点的**事实订正**

⚠️ 本 change 的 proposal / tasks / improvement 草案**初稿**写的是
「`CTEST_TOTAL` 202 → 203、`EXAMPLES_TEST_SOURCES` 17 → 18」⇒ ⇒ **事实错误**：

- `tests/chlib/test_if_stmt.cpp` 是**既有**、**已注册**的 ctest target ⇒
  一个 Catch2 二进制 = **1** 个 ctest 条目 ⇒ 用例数增加但**条目数不变**
  ⇒ `CTEST_TOTAL` = **202 不变**；
- 本 change 新增的是 **samples/** 下的 sample ⇒ 而 `EXAMPLES_TEST_SOURCES` 的
  口径是 **`examples/**` 下**的测试源文件数 ⇒ ⇒ **17 不变**；
- ⇒ 真值：**202 / 38 / 191 / 17 全部不变**；**只有 samples 注册数变**
  （已注册 35 → 36、禁用 5 → 4）⇒ 该数**不是** CI 锚点，但
  `scripts/run_all_samples.sh` 的 **C2 数量守恒**会校验（实测 36 == 36 ✅）。

## 6. 变异清单（每条**先确认编译成功** ⇒ 否则跑旧二进制 ⇒ 假绿）

| # | 注入 | 期望转红 | 实测 |
|---|---|---|---|
| M1 | `finalized_ = true` 移回遍历**之前** | 「条件为假时不被写」 | ✅ 转红 |
| M2 | 捕获分支改回**空实现**（`if (!blk)` ⇒ `if (true)`） | 「条件为假时不被写」 | ✅ 转红 |
| M3 | `_if` 侧的**贡献收集顺序**反转（`push_back` ⇒ `insert(begin)`） | 多分支用例 | ✅ 转红 |
| M4 | 去掉输出「缺 `else_` ⇒ 抛异常」+ 默认退化为 `0` | 该用例（`REQUIRE_THROWS_AS`）| ✅ 转红 |
| M5 | 嵌套不再委托给外层（`outer_active()` ⇒ `nullptr`） | 嵌套用例 | ✅ 转红 |
| M6 | **共享组合器**的链构造方向反转（`rbegin` ⇒ `begin`） | **`_if` 侧的「先匹配者胜」** | ✅ 转红 |

⚠️ M1 是**关键闸门**：它证明「条件化」真的被测到（否则 M2 之类的恒绿）。
⚠️ M3 与 M6 **故意分层**：M3 动的是 `_if` **自己的**贡献收集顺序（**不**碰共享
组合器）⇒ 证明「贡献收集顺序」被钉住；M6 动的是**共享组合器**内部 ⇒ 证明
**共享的那份实现**确实是「先匹配者胜」。

## 6.1 ⚠️ M6 的**实测订正**：链序对 ADR-050 侧**不可观测**

⚠️ **初稿预测**（「M6 ⇒ ADR-050 的既有金标转红」）⇒ ⇒ **被实测证伪**。

**根因**：ADR-050 的守卫是**状态守卫**（`is_in(S)`）⇒ ⇒ **同一时刻至多一个
为真** ⇒ ⇒ 无论 `select` 链的顺序如何，结果**逐位相同** ⇒ ⇒ 链序在 ADR-050
侧**根本不产生可观测差异**。

⇒ ⇒ **D10「纯重构」的证据 MUST 是**「ADR-050 的既有金标**逐项不变**（绿灯）」
（实测 ✅：`busy` 高 **3906** 拍 + `0xA5` 位序；`0x96` MSB-first 16 位 +
状态序列 + `bit_count` 复位），**不**可能是「链序变异下金标转红」。

⚠️ **对 `_if` 侧**则相反：条件守卫**不互斥**（这正是「先匹配者胜」的意义）
⇒ ⇒ 链序**可观测** ⇒ ⇒ M6 在 `_if` 侧转红 ✔。

⇒ ⇒ **如实登记**：「链序」这个属性，**只有 `_if` 侧的用例钉得住**；ADR-050 侧
**无任何用例**钉它 ⇒ ⇒ 若将来 ADR-050 引入**非互斥**守卫（多个状态可同时
为真），**必须**补一条**顺序敏感**的用例，否则该属性**无覆盖**。

## 6.2 ⚠️ 变异**工具链**的两条实测教训（**比变异本身更重要**）

⚠️ 本次实施中，变异工具链**自身**制造了两次「假红」，若不深究就会被误判成
「实现有缺陷」⇒ ⇒ 记录下来。

### ① 还原源码后**必须重建** ⇒ 否则跑的是**上一条变异**的二进制

**症状**：M6 跑完后（源码已还原、但**未重建**），全量 `ctest` 里 `test_if_stmt`
报「nested 期望 `11` 实得 `13`」⇒ ⇒ 看起来像「嵌套实现有缺陷」。

**根因**：M6 翻转了 `guard_contribution.h` 的链构造方向 ⇒ `test_if_stmt` 的二进制
里就带着这个翻转 ⇒ 还原**源码**不会改变**二进制** ⇒ ⇒ 后续跑的都是**变异体**。

⇒ ⇒ **护栏**：变异工具链 MUST 在**还原之后重建**（本次已加进
`/tmp/opencode/mutate_if.py` 的 `finally` 段）⇒ ⇒ 且还原后 MUST **逐字节校验**
内容一致（fail-closed）。

⇒ ⇒ **一般化**：凡是「改源码 → 跑二进制 → 还原源码」的流程，**还原**不等于
**复原** —— 二进制也是状态。

### ② `build_targets` MUST 覆盖**每一个**被观测的测试

**症状**：M6 的 `expect_red` 是 `test_if_stmt`，但 `build_targets` 只列了
`uart_tx_example` + `test_verilator_complex_e2e` ⇒ ⇒ `test_if_stmt` 的二进制
**没被重建** ⇒ ⇒ 跑的是**上一轮**的正确二进制 ⇒ ⇒ 报「变异后仍绿 ⇒ 无判别力」
⇒ ⇒ **误判**。

⇒ ⇒ **护栏**：`build_targets` MUST 覆盖 `expect_red` + `also_red` + `also_green`
里出现的**全部** target（本次已补齐）。

⇒ ⇒ **一般化**：观测面与重建面**必须**由同一份清单驱动 ⇒ ⇒ 否则「观测」会
悄悄退化成「看旧结果」。
