#include "catch_amalgamated.hpp"
#include "chlib/selector_arbiter.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("SelectorArbiter: priority selector",
          "[selector_arbiter][priority_selector]") {
    auto ctx = std::make_unique<ch::core::context>("test_priority_selector");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Priority selection with multiple requests") {
        ch_uint<8> request(01010010_b); // Requests at positions 1, 4, 6
        PrioritySelectorResult<8> result = priority_selector<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Priority to lower bits, so bit 1 should be granted
        REQUIRE(sim.get_value(result.grant) == 0b00000010); // Bit 1 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Single request") {
        ch_uint<8> request(00001000_b); // Request at position 3
        PrioritySelectorResult<8> result = priority_selector<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0b00001000); // Bit 3 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("No requests") {
        ch_uint<8> request(00000000_b); // No requests
        PrioritySelectorResult<8> result = priority_selector<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0b00000000); // No grants
        REQUIRE(sim.get_value(result.valid) == false);
    }

    SECTION("All requests") {
        ch_uint<8> request(11111111_b); // All requests active
        PrioritySelectorResult<8> result = priority_selector<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) ==
                0b00000001); // Lowest priority (bit 0)
        REQUIRE(sim.get_value(result.valid) == true);
    }
}

TEST_CASE("SelectorArbiter: round robin selector",
          "[selector_arbiter][round_robin_selector]") {
    auto ctx = std::make_unique<ch::core::context>("test_round_robin_selector");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin selection with starting position 0") {
        ch_uint<4> request(0101_b);    // Requests at positions 0 and 2
        ch_uint<4> last_grant(0001_b); // Last granted was position 0, so next
                                       // should start from 1
        PrioritySelectorResult<4> result =
            round_robin_selector<4>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // After position 0 (encoded as 0001_b), next available is position 1, then position 2
        // Position 1 is checked first but not requested, position 2 is checked second and is requested
        REQUIRE(sim.get_value(result.grant) == 0b0100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin selection with starting position 2") {
        ch_uint<4> request(1101_b);    // Requests at positions 0, 2, 3
        ch_uint<4> last_grant(0100_b); // Last granted was position 2, so next
                                       // should start from 3
        PrioritySelectorResult<4> result =
            round_robin_selector<4>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // After position 2 (encoded as 0100_b), next available is position 3
        REQUIRE(sim.get_value(result.grant) == 0b1000); // Bit 3 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin wraparound") {
        ch_uint<4> request(1101_b);    // Requests at positions 0, 2, 3
        ch_uint<4> last_grant(1000_b); // Last granted was position 3, so next
                                       // should start from 0
        PrioritySelectorResult<4> result =
            round_robin_selector<4>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // After position 3 (encoded as 1000_b), wrap to position 0
        REQUIRE(sim.get_value(result.grant) == 0b0001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin with no available requests") {
        ch_uint<4> request(0000_b);    // No requests
        ch_uint<4> last_grant(0001_b); // Last granted was position 0
        PrioritySelectorResult<4> result =
            round_robin_selector<4>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0b0000); // No grants
        REQUIRE(sim.get_value(result.valid) == false);
    }
}

TEST_CASE("SelectorArbiter: round robin arbiter",
          "[selector_arbiter][round_robin_arbiter]") {
    auto ctx = std::make_unique<ch::core::context>("test_round_robin_arbiter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin arbiter with multiple requests") {
        ch_uint<4> request(0101_b);    // Requests at positions 0 and 2
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // First time: should grant position 0 (as internal ptr_reg starts with 0)
        REQUIRE(sim.get_value(result.grant) == 0b0001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with different request pattern") {
        ch_uint<4> request(1100_b);    // Requests at positions 2 and 3
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Should grant position 2 (as internal ptr_reg starts with 0)
        REQUIRE(sim.get_value(result.grant) == 0b0100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with no requests") {
        ch_uint<4> request(0000_b);    // No requests
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0b0000); // No grants
        REQUIRE(sim.get_value(result.valid) == false);
    }

    SECTION("Round robin arbiter with all requests") {
        ch_uint<4> request(1111_b);    // All requests active
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Should grant position 0 (as internal ptr_reg starts with 0)
        REQUIRE(sim.get_value(result.grant) == 0b0001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }
    
    SECTION("Round robin arbiter sequential behavior") {
        ch_uint<4> request(1101_b);    // Requests at positions 0, 2, 3
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // First time: should grant position 0 (as internal ptr_reg starts with 0)
        REQUIRE(sim.get_value(result.grant) == 0b0001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }
}