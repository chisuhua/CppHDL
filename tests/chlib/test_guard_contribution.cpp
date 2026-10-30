/**
 * @file test_guard_contribution.cpp
 * @brief 「守卫贡献」共享组合器（`include/chlib/guard_contribution.h`）的
 *        **结构级**断言
 *
 * ⚠️ **为什么需要结构级断言**（change
 *    `chlib-guard-contribution-priority-2026-10`）：
 *   共享组合器 `compose_guard_chain` 把若干 `(守卫, 值)` 合成为**一条**
 *   `select` 链，语义为「**先匹配者胜**」⇒ ⇒ **先收集**的贡献 MUST 位于链的
 *   **外层**（先被求值并胜出）。
 *
 *   ⚠️ **这条性质运行期【不可观测】**：状态守卫 `is_in(S)` =
 *   `state_reg == ch_uint<STATE_BITS>(S)` ⇒ 同一寄存器对互不相同的枚举字面量
 *   作比较 ⇒ ⇒ **两两互斥是构造保证** ⇒ ⇒ 同一时刻至多一个守卫为真 ⇒ ⇒
 *   无论链序如何，结果**逐位相同**。
 *
 *   ⇒ ⇒⇒ **既有金标 100% 绿**却**看不见**链序 ⇒ ⇒⇒ 链序 MUST 由
 *   **结构级**断言钉住（本文件）⇒ ⇒⇒ 否则该属性**零覆盖**。
 *
 * ## 判别力（变异 M1）
 *
 *   把 `compose_guard_chain` 的 `rbegin→rend` 改成 `begin→end`
 *   ⇒ ⇒ 本文件的断言**转红**（最外层 mux 的守卫变成**最后一个**贡献的守卫）
 *   ⇒ ⇒ 而**运行期**用例**仍绿**（⇒ 证明「运行期不可观测」这条事实）。
 *
 * ## 断言手法
 *
 *   `compose_guard_chain` 返回的节点是 `muximpl`（`type_mux`）⇒ ⇒
 *   `condition()` = 该 `select` 的守卫端、`true_value()` = 贡献值、
 *   `false_value()` = **内层链**（⇒ 递归可取出整条链的守卫序列）。
 */

#include "catch_amalgamated.hpp"
#include "chlib/guard_contribution.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/lnodeimpl.h"
#include "core/uint.h"
#include <memory>
#include <vector>

using namespace ch::core;
using namespace chlib;

namespace {

/// RAII helper: 建一个 context + ctx_swap（建节点需要活跃 context）
struct test_ctx {
    std::unique_ptr<ch::core::context> ctx_;
    ch::core::ctx_swap swap_;

    explicit test_ctx(const char *name)
        : ctx_(std::make_unique<ch::core::context>(name)), swap_(ctx_.get()) {}
};

/// 取出 `select` 链的**守卫序列**（从**最外层**到**最内层**）。
///
/// ⚠️ `select(cond, v, acc)` 的 `false_value()` 就是**内层链** ⇒ ⇒ 沿
///    `false_value()` 下降即可。
std::vector<lnodeimpl *> collect_guard_chain(lnodeimpl *node) {
    std::vector<lnodeimpl *> out;
    while (node && node->type() == lnodetype::type_mux) {
        out.push_back(node->src(0));  // condition()
        node = node->src(2);          // false_value() = 内层链
    }
    return out;
}

} // namespace

// ===========================================================================
// 结构级断言：链的构造顺序 = 「先匹配者胜」
// ===========================================================================

TEST_CASE("guard_contribution: the FIRST contribution MUST be the outermost mux",
          "[chlib][guard_contribution][adr051]") {
    test_ctx ctx("test_guard_chain_order");

    // 三个**可区分**的守卫（各自是独立的字面量节点 ⇒ 指针可区分）
    ch_bool g0(true, "g0");
    ch_bool g1(false, "g1");
    ch_bool g2(true, "g2");

    std::vector<guard_contribution> contribs = {
        {g0, ch_uint<8>(11_d).impl()},
        {g1, ch_uint<8>(22_d).impl()},
        {g2, ch_uint<8>(33_d).impl()},
    };

    ch_uint<8> tail(99_d);
    ch_uint<8> chain = compose_guard_chain<8>(contribs, tail);

    auto guards = collect_guard_chain(chain.impl());
    REQUIRE(guards.size() == 3u);

    // ⚠️ **核心判据**：最外层 mux 的守卫 MUST 是**第一个**贡献的守卫
    REQUIRE(guards[0] == g0.impl());
    REQUIRE(guards[1] == g1.impl());
    REQUIRE(guards[2] == g2.impl());
}

TEST_CASE("guard_contribution: a single contribution yields a one-deep chain",
          "[chlib][guard_contribution][adr051]") {
    test_ctx ctx("test_guard_chain_single");

    ch_bool g0(true, "g0");
    std::vector<guard_contribution> contribs = {
        {g0, ch_uint<8>(7_d).impl()},
    };

    ch_uint<8> tail(0_d);
    ch_uint<8> chain = compose_guard_chain<8>(contribs, tail);

    auto guards = collect_guard_chain(chain.impl());
    REQUIRE(guards.size() == 1u);
    REQUIRE(guards[0] == g0.impl());
}

TEST_CASE("guard_contribution: NO contributions yields the tail itself",
          "[chlib][guard_contribution][adr051]") {
    test_ctx ctx("test_guard_chain_empty");

    std::vector<guard_contribution> contribs;   // 空
    ch_uint<8> tail(42_d);
    ch_uint<8> chain = compose_guard_chain<8>(contribs, tail);

    // 空贡献 ⇒ **不是** mux（直接是尾部）
    REQUIRE(chain.impl()->type() != lnodetype::type_mux);
}

TEST_CASE("guard_contribution: the tail MUST be the innermost fallback",
          "[chlib][guard_contribution][adr051]") {
    test_ctx ctx("test_guard_chain_tail");

    ch_bool g0(true, "g0");
    std::vector<guard_contribution> contribs = {
        {g0, ch_uint<8>(7_d).impl()},
    };

    ch_uint<8> tail(42_d);
    ch_uint<8> chain = compose_guard_chain<8>(contribs, tail);

    // 最内层（false 分支链的末端）MUST 是**尾部**
    auto guards = collect_guard_chain(chain.impl());
    REQUIRE(guards.size() == 1u);
    // 尾部的节点 = 链的 false 分支（只有一个 mux ⇒ false 分支就是 tail）
    REQUIRE(chain.impl()->src(2) == tail.impl());
}
