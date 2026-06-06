/**
 * @file test_if_stmt.cpp
 * @brief Unit tests for chlib::conditional_block (if_stmt.h) public API.
 *
 * Covers:
 *  - _if(condition).then(body).end() — basic branch with a single body
 *  - _if(...).elif(...).else_().end() — multi-branch chain
 *  - validate() errors: missing body, elif after else_, duplicate else_
 *  - conditional_reg_assign<T>(reg).next() = value — outside/inside if block
 *
 * NOTE on execution semantics: the current implementation of
 * conditional_block::end() iterates over ALL branches and invokes each body
 * (see header comment "执行所有分支以捕获所有赋值"). The tests assert this
 * documented behavior, not a per-condition short-circuit. Bodies that need
 * to honor a condition are expected to gate their own work with select() /
 * when() on the ch_bool condition.
 */

#include "catch_amalgamated.hpp"
#include "chlib/if_stmt.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/reg.h"
#include "core/uint.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

namespace {

// RAII helper: build a context + ctx_swap for a test body. Mirrors the
// pattern used in test_logic.cpp / test_combinational.cpp.
struct test_ctx {
    std::unique_ptr<ch::core::context> ctx_;
    ch::core::ctx_swap swap_;

    explicit test_ctx(const char *name)
        : ctx_(std::make_unique<ch::core::context>(name)), swap_(ctx_.get()) {}

    ch::core::context *get() const { return ctx_.get(); }
};

} // namespace

TEST_CASE("if_stmt: basic if-then with true condition executes body",
          "[chlib][if_stmt]") {
    test_ctx ctx("test_if_stmt_true");

    bool body_executed = false;

    _if(ch_bool(true))
        .then([&body_executed] { body_executed = true; })
        .end();

    REQUIRE(body_executed);
}

TEST_CASE("if_stmt: basic if-then with false condition does not throw",
          "[chlib][if_stmt]") {
    test_ctx ctx("test_if_stmt_false");

    // The block must accept a false condition without throwing.
    // Per the documented semantics in if_stmt.h, end() still walks the
    // branch list and invokes the body — bodies are responsible for
    // short-circuiting themselves via the condition signal.
    bool body_executed = false;

    REQUIRE_NOTHROW(
        _if(ch_bool(false))
            .then([&body_executed] { body_executed = true; })
            .end());

    // Document the current execution semantics explicitly.
    REQUIRE(body_executed);
}

TEST_CASE("if_stmt: if-elif-else chain accepts all branches",
          "[chlib][if_stmt]") {
    test_ctx ctx("test_if_stmt_chain");

    int if_hits = 0;
    int elif_hits = 0;
    int else_hits = 0;

    _if(ch_bool(true))
        .then([&if_hits] { ++if_hits; })
        .elif(ch_bool(false))
        .then([&elif_hits] { ++elif_hits; })
        .else_()
        .then([&else_hits] { ++else_hits; })
        .end();

    // All three bodies are walked by end(); the test confirms none throws
    // and the chain finalizes cleanly.
    REQUIRE(if_hits == 1);
    REQUIRE(elif_hits == 1);
    REQUIRE(else_hits == 1);
}

TEST_CASE("if_stmt: validate throws when then body is missing",
          "[chlib][if_stmt][validate]") {
    test_ctx ctx("test_if_stmt_no_body");

    // _if(c).end() — branch exists, no .then() body supplied.
    // validate() must throw a std::runtime_error.
    bool caught = false;
    try {
        _if(ch_bool(true)).end();
    } catch (const std::runtime_error &) {
        caught = true;
    }
    REQUIRE(caught);
}

TEST_CASE("if_stmt: validate throws when an elif branch has no body",
          "[chlib][if_stmt][validate]") {
    test_ctx ctx("test_if_stmt_elif_no_body");

    // _if(c).then(b).elif(c2).end() — second branch has no body.
    bool caught = false;
    try {
        _if(ch_bool(true))
            .then([] {})
            .elif(ch_bool(false))
            .end();
    } catch (const std::runtime_error &) {
        caught = true;
    }
    REQUIRE(caught);
}

TEST_CASE("if_stmt: elif after else_ throws", "[chlib][if_stmt][validate]") {
    test_ctx ctx("test_if_stmt_elif_after_else");

    bool caught = false;
    try {
        _if(ch_bool(true))
            .then([] {})
            .else_()
            .then([] {})
            .elif(ch_bool(false));
    } catch (const std::runtime_error &) {
        caught = true;
    }
    REQUIRE(caught);
}

TEST_CASE("if_stmt: duplicate else_ throws", "[chlib][if_stmt][validate]") {
    test_ctx ctx("test_if_stmt_double_else");

    bool caught = false;
    try {
        _if(ch_bool(true))
            .then([] {})
            .else_()
            .then([] {})
            .else_();
    } catch (const std::runtime_error &) {
        caught = true;
    }
    REQUIRE(caught);
}

TEST_CASE("if_stmt: end() is idempotent", "[chlib][if_stmt]") {
    test_ctx ctx("test_if_stmt_idempotent_end");

    int hits = 0;

    auto block = _if(ch_bool(true));
    block.then([&hits] { ++hits; });
    block.end();
    // Second end() must be a no-op (finalized_ guard).
    REQUIRE_NOTHROW(block.end());

    REQUIRE(hits == 1);
}

TEST_CASE("if_stmt: then() chained after _if() always works",
          "[chlib][if_stmt]") {
    test_ctx ctx("test_if_stmt_then_chained");

    REQUIRE_NOTHROW(_if(ch_bool(true)).then([] {}).end());
}

TEST_CASE("if_stmt: conditional_reg_assign outside if block drives reg next",
          "[chlib][if_stmt][reg]") {
    test_ctx ctx("test_if_stmt_reg_assign");

    ch_reg<ch_uint<8>> reg(0_d, "if_stmt_reg");

    // Outside an active conditional_block, the assignment must be
    // forwarded to reg->next unconditionally.
    conditional_reg_assign(reg).next() = ch_uint<8>(42_d);

    ch::Simulator sim(ctx.get());
    sim.tick();

    REQUIRE(sim.get_value(reg) == 42_d);
}

TEST_CASE("if_stmt: conditional_reg_assign inside if block forwards to next",
          "[chlib][if_stmt][reg]") {
    test_ctx ctx("test_if_stmt_reg_in_block");

    ch_reg<ch_uint<8>> reg(0_d, "if_stmt_reg_in_block");

    // end() sets finalized_=true BEFORE walking the bodies, so
    // conditional_block::is_active() returns false when the body
    // actually runs. The next_proxy::operator= therefore takes the
    // "direct assign" branch and writes through to reg->next, just
    // like the outside-if case. This test pins down that current
    // (deferred-select-tree) behavior so future changes are noticed.
    _if(ch_bool(true))
        .then([&reg] {
            conditional_reg_assign(reg).next() = ch_uint<8>(7_d);
        })
        .end();

    ch::Simulator sim(ctx.get());
    sim.tick();

    REQUIRE(sim.get_value(reg) == 7_d);
}
