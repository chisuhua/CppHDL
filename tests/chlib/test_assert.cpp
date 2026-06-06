/**
 * @file test_assert.cpp
 * @brief Unit tests for include/chlib/assert.h
 *
 * Exercises the public API of the assert subsystem:
 *   - ch::chlib::AssertChecker Component (ports: condition, enable, failed)
 *   - CH_ASSERT(cond, msg) macro
 *   - ch_assert(cond, msg) macro (alias)
 *
 * Pattern (per tests/AGENTS.md + AGENTS.md):
 *   - Standalone component test uses ch::ch_device<T>
 *   - All TEST_CASEs tagged [chlib][assert]
 *   - IO via sim.set_input_value / sim.get_port_value (never direct io() = ...)
 */

#include "catch_amalgamated.hpp"
#include "chlib/assert.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>

using namespace ch;
using namespace ch::core;
using namespace ch::chlib;

// =============================================================================
// AssertChecker Component — combinational truth-table coverage
// =============================================================================

TEST_CASE("AssertChecker: condition=false with enable=true reports failed",
          "[chlib][assert][checker][fail]") {
    auto ctx = std::make_unique<ch::core::context>("assert_checker_fail");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    ch::ch_device<AssertChecker> checker_device;
    ch::Simulator sim(checker_device.context());

    // Drive inputs: enable asserted, condition violated.
    sim.set_input_value(checker_device.instance().io().enable, true);
    sim.set_input_value(checker_device.instance().io().condition, false);

    sim.tick();

    // Combinational: failed must be 1 because the checker is enabled and
    // the condition is false.
    auto failed = sim.get_port_value(checker_device.instance().io().failed);
    REQUIRE(static_cast<uint64_t>(failed) == 1);
}

TEST_CASE("AssertChecker: condition=true with enable=true reports no failure",
          "[chlib][assert][checker][pass]") {
    auto ctx = std::make_unique<ch::core::context>("assert_checker_pass");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    ch::ch_device<AssertChecker> checker_device;
    ch::Simulator sim(checker_device.context());

    sim.set_input_value(checker_device.instance().io().enable, true);
    sim.set_input_value(checker_device.instance().io().condition, true);

    sim.tick();

    // failed must be 0 — condition holds, checker is silent.
    auto failed = sim.get_port_value(checker_device.instance().io().failed);
    REQUIRE(static_cast<uint64_t>(failed) == 0);
}

TEST_CASE("AssertChecker: enable=false suppresses reporting regardless of condition",
          "[chlib][assert][checker][disabled]") {
    auto ctx = std::make_unique<ch::core::context>("assert_checker_disabled");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    ch::ch_device<AssertChecker> checker_device;
    ch::Simulator sim(checker_device.context());

    // Even with condition=false, a disabled checker must NOT raise failed.
    sim.set_input_value(checker_device.instance().io().enable, false);
    sim.set_input_value(checker_device.instance().io().condition, false);

    sim.tick();

    auto failed = sim.get_port_value(checker_device.instance().io().failed);
    REQUIRE(static_cast<uint64_t>(failed) == 0);
}

TEST_CASE("AssertChecker: input changes propagate on subsequent ticks",
          "[chlib][assert][checker][toggle]") {
    auto ctx = std::make_unique<ch::core::context>("assert_checker_toggle");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    ch::ch_device<AssertChecker> checker_device;
    ch::Simulator sim(checker_device.context());

    // Cycle 1: failing condition.
    sim.set_input_value(checker_device.instance().io().enable, true);
    sim.set_input_value(checker_device.instance().io().condition, false);
    sim.tick();
    auto failed_first = sim.get_port_value(
        checker_device.instance().io().failed);
    REQUIRE(static_cast<uint64_t>(failed_first) == 1);

    // Cycle 2: condition now holds — failed must drop to 0.
    sim.set_input_value(checker_device.instance().io().condition, true);
    sim.tick();
    auto failed_second = sim.get_port_value(
        checker_device.instance().io().failed);
    REQUIRE(static_cast<uint64_t>(failed_second) == 0);
}

// =============================================================================
// CH_ASSERT / ch_assert macros — smoke tests
// =============================================================================
//
// The macros are no-op debug printouts: they expand to a std::cout message
// with file/line info. We exercise them by:
//   1. Redirecting std::cout to an ostringstream and confirming the marker
//      `[CH_ASSERT]` was emitted.
//   2. Re-running under the lowercase alias to confirm it points at the same
//      implementation.

TEST_CASE("CH_ASSERT macro emits diagnostic with [CH_ASSERT] marker",
          "[chlib][assert][macro]") {
    auto ctx = std::make_unique<ch::core::context>("assert_macro_ch");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::ostringstream captured;
    auto* old_cout = std::cout.rdbuf(captured.rdbuf());

    CH_ASSERT(true, "smoke-test message from CH_ASSERT");

    std::cout.rdbuf(old_cout);

    const std::string output = captured.str();
    INFO("captured output: " << output);
    REQUIRE(output.find("[CH_ASSERT]") != std::string::npos);
    REQUIRE(output.find("smoke-test message from CH_ASSERT") != std::string::npos);
    REQUIRE(output.find(__FILE__) != std::string::npos);
}

TEST_CASE("ch_assert macro is an alias of CH_ASSERT",
          "[chlib][assert][macro][alias]") {
    auto ctx = std::make_unique<ch::core::context>("assert_macro_alias");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::ostringstream captured;
    auto* old_cout = std::cout.rdbuf(captured.rdbuf());

    // Use the lowercase alias — must produce the same diagnostic format.
    ch_assert(true, "smoke-test message from ch_assert");

    std::cout.rdbuf(old_cout);

    const std::string output = captured.str();
    INFO("captured output: " << output);
    REQUIRE(output.find("[CH_ASSERT]") != std::string::npos);
    REQUIRE(output.find("smoke-test message from ch_assert") != std::string::npos);
}
