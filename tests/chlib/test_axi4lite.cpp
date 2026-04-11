#include "catch_amalgamated.hpp"
#include "chlib/axi4lite.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("AXI4-Lite: Signal structs basic check", "[axi4lite][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_axi4lite_basic");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Axi4LiteMaster and Axi4LiteSlave structs compile") {
        ch::Simulator sim(ctx.get());

        Axi4LiteMaster<8, 32> master;
        Axi4LiteSlave<8, 32> slave;

        master.aw.awvalid = ch_bool(false);
        master.aw.awready = ch_bool(false);
        slave.aw.awvalid = ch_bool(false);

        sim.tick();

        REQUIRE(sim.get_value(master.aw.awvalid) == false);
        REQUIRE(sim.get_value(master.aw.awready) == false);
        REQUIRE(sim.get_value(slave.aw.awvalid) == false);
    }
}
