#include "catch_amalgamated.hpp"
#include "chlib/memory.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Memory: Basic context test", "[memory][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_mem_basic");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Context is created and valid") {
        ch::Simulator sim(ctx.get());

        REQUIRE(ctx != nullptr);
        REQUIRE(ctx->name() == "test_mem_basic");

        sim.tick();
    }
}
