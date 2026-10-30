/**
 * @file guard_contribution.h
 * @brief 「守卫贡献」组合器 —— 把同一目标的候选值合成**一条** `select` 链
 *
 * ⚠️ **为什么存在**（ADR-051 D10）：
 *   仓内有**两个**「条件 / 状态守卫」机制 ⇒
 *   ① `ch_state_machine::contrib`（**状态**守卫，ADR-050）；
 *   ② `_if` 语句块的条件赋值（**条件**守卫，ADR-051）。
 *   二者是**同一个机制**（累积 `(守卫, 值)` ⇒ 合成**一条** `select` 链）
 *   ⇒ ⇒ 抽取**共享**组合器 ⇒ **一处实现、两处使用** ⇒ ⇒ 两份语义**不会漂移**。
 *
 * ⚠️ **边界**：本文件只提供**内部**组合器 ⇒ **不**泛化 ADR-050 的 `contrib`
 *   为公开的通用 API ⇒ ⇒ `core/` 与 `ch_reg` / `ch_out` 的公开契约**零变化**。
 *
 * ## 语义（**先匹配者胜**）
 *
 * `contribs` MUST 按**优先级顺序**排列（靠前者优先）。合成方式：
 *
 * ```
 * acc = tail;
 * for (i = n-1 .. 0)  acc = select(guards[i], values[i], acc);
 * ```
 *
 * ⇒ 结果 = `select(g0, v0, select(g1, v1, … tail))` ⇒ **g0 命中时 v0 胜**
 * ⇒ ⇒ **先收集**的贡献位于链的**外层**（先被求值并胜出）
 * ⇒ ⇒ 与 `chlib::multi_if`（表达式形式）的「从后往前构链」**同序** ⇒
 * ⇒ 语句块形式与表达式形式**优先级顺序等价**。
 *
 * ⚠️ **方向 MUST 显式钉住**（change `chlib-guard-contribution-priority-2026-10`）：
 * 本组合器的**方向**（先收集的在外层）是**契约的一部分** ⇒ ⇒
 * - **`_if` 侧**：条件守卫**不互斥** ⇒ ⇒ 链序**运行期可观测** ⇒ ⇒ 既有运行期
 *   用例（`tests/chlib/test_if_stmt.cpp`）**钉得住**它；
 * - **状态守卫侧**（ADR-050 的 `ch_state_machine::contrib`）：守卫是
 *   `is_in(S)` ⇒ ⇒ **两两互斥是构造保证** ⇒ ⇒ 链序**运行期不可观测** ⇒ ⇒
 *   **运行期金标钉不住它** ⇒ ⇒ MUST 由**结构级**断言钉住
 *   （`tests/chlib/test_guard_contribution.cpp`：断言最外层 mux 的守卫 =
 *   **第一个**贡献的守卫）。
 *
 * ⚠️ **抽取前的方向差异**（**如实登记**）：ADR-050 的 `contrib` 在被抽到本组合器
 * （ADR-051 D10）**之前**，其内联 `compose()` 用**正向**状态序迭代
 * （`acc = select(g_i, v_i, acc)`）⇒ ⇒ **后写的 state 在外层** ⇒ ⇒
 * **高编号** state 优先；抽取后改为**先收集的在外层** ⇒ ⇒ **低编号** state 优先
 * ⇒ ⇒ ⇒ **优先级被静默翻转了**（⇒ **运行期不可观测** ⇒ ⇒ 既有金标 100% 绿
 * ⇒ ⇒ ⇒ 「绿」完全掩盖了它）。
 * ⇒ ⇒ **口径裁决**（2026-10-30）：**保留新行为**（=「先匹配者胜」= 与 `_if` /
 * `multi_if` 一致）⇒ ⇒ 旧行为属**偶然反转** ⇒ ⇒ 但「抽取是**纯重构**，不是行为
 * 变更」这句表述**不成立** ⇒ ⇒ 已在 ADR-051 D10 订正，并给 capability
 * `chlib-state-machine-semantics` **补上优先级条款**（该 capability 从未写过
 * 优先级 ⇒ ⇒ 翻转**不违反任何契约** ⇒ ⇒ 无处登记）。
 *
 * ## `tail`（默认尾部）
 *
 * - **寄存器**：`tail` = 寄存器的**当前值** ⇒ 未匹配时**保持**；
 * - **输出端口**：`tail` = **显式默认**（输出**不能**「保持」⇒ 那是组合环）。
 */

#pragma once

#include "core/bool.h"
#include "core/lnodeimpl.h"
#include "core/operators.h"
#include "core/uint.h"
#include <vector>

namespace chlib {

using namespace ch::core;

/// 一条「守卫贡献」= (守卫, 值)
struct guard_contribution {
    ch_bool guard;     // 守卫（状态守卫 = is_in(S)；条件守卫 = 分支条件）
    lnodeimpl *value;  // 候选值（**eager** 捕获的节点）
};

/// 把同一 target 的全部守卫贡献合成**一条** `select` 链（**先匹配者胜**）。
///
/// @param contribs MUST 按**优先级顺序**（靠前者优先）排列
/// @param tail     默认尾部（寄存器 = 当前值；输出 = 显式默认）
template <unsigned W>
ch_uint<W> compose_guard_chain(const std::vector<guard_contribution> &contribs,
                                const ch_uint<W> &tail) {
    ch_uint<W> acc = tail;
    // ⚠️ **从后往前**合成 ⇒ 前面的守卫**优先**（先匹配者胜）。
    for (auto it = contribs.rbegin(); it != contribs.rend(); ++it) {
        acc = select(it->guard, ch_uint<W>(it->value), acc);
    }
    return acc;
}

} // namespace chlib
