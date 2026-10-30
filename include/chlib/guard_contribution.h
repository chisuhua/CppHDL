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
 * ⇒ ⇒ 与 `chlib::multi_if`（表达式形式）的「从后往前构链」**同序** ⇒
 * ⇒ 语句块形式与表达式形式**优先级顺序等价**。
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
