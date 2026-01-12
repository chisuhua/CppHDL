#include "catch_amalgamated.hpp"
#include "chlib/selector_arbiter.h"
#include "codegen_dag.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <bitset>
#include <memory>

using namespace ch::core;
using namespace chlib;

// Helper function to convert uint value to binary string representation
template <typename T> std::string to_binary_string(T value, size_t width) {
    std::bitset<64> bs(static_cast<uint64_t>(value));
    std::string result = bs.to_string();
    // Return only the requested width bits
    return result.substr(64 - width, width);
}

TEST_CASE("SelectorArbiter: round robin selector extended tests",
          "[selector_arbiter][round_robin_selector][extended]") {
    auto ctx = std::make_unique<ch::core::context>(
        "test_round_robin_selector_extended");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Alternating requests test") {
        ch_uint<4> request(0101_b, "request"); // Requests at positions 0 and 2
        ch_uint<4> last_grant(0001_b,
                              "last_grant"); // Last granted was position 0

        PrioritySelectorResult<4> result =
            round_robin_selector<4>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Get values before printing
        auto request_val = sim.get_value(request);
        auto last_grant_val = sim.get_value(last_grant);
        auto grant_val = sim.get_value(result.grant);
        auto valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 4)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 4)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 4)
                  << ", valid=" << valid_val << std::endl;

        // After position 0, should go to position 1 but it's not requested,
        // so position 2 should be granted
        REQUIRE(sim.get_value(result.grant) == 0b0100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Now continue with same selector but different values
        sim.set_value(request, 0101_b);    // Same requests
        sim.set_value(last_grant, 0100_b); // Last granted was position 2
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 4)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 4)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 4)
                  << ", valid=" << valid_val << std::endl;

        sim.toVCD("1.vcd");

        // After position 2, should go to position 3 but it's not requested,
        // so wrap around to position 0
        REQUIRE(sim.get_value(result.grant) == 0b0001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("All positions requested test") {
        ch_uint<4> request(1111_b, "request"); // All positions requested
        ch_uint<4> last_grant(0001_b,
                              "last_grant"); // Last granted was position 0

        PrioritySelectorResult<4> result =
            round_robin_selector<4>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Get values before printing
        auto request_val = sim.get_value(request);
        auto last_grant_val = sim.get_value(last_grant);
        auto grant_val = sim.get_value(result.grant);
        auto valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 4)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 4)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 4)
                  << ", valid=" << valid_val << std::endl;

        // After position 0, should go to position 1
        REQUIRE(sim.get_value(result.grant) == 0b0010); // Bit 1 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Continue with same selector but different values
        sim.set_value(request, 1111_b);    // All positions still requested
        sim.set_value(last_grant, 0010_b); // Last granted was position 1
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 4)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 4)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 4)
                  << ", valid=" << valid_val << std::endl;

        // After position 1, should go to position 2
        REQUIRE(sim.get_value(result.grant) == 0b0100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Continue with same selector but different values
        sim.set_value(request, 1111_b);    // All positions still requested
        sim.set_value(last_grant, 0100_b); // Last granted was position 2
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 4)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 4)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 4)
                  << ", valid=" << valid_val << std::endl;

        // After position 2, should go to position 3
        REQUIRE(sim.get_value(result.grant) == 0b1000); // Bit 3 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Sparse requests test") {
        ch_uint<8> request(01010000_b,
                           "request"); // Requests at positions 4 and 6
        ch_uint<8> last_grant(00010000_b,
                              "last_grant"); // Last granted was position 4

        PrioritySelectorResult<8> result =
            round_robin_selector<8>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Get values before printing
        auto request_val = sim.get_value(request);
        auto last_grant_val = sim.get_value(last_grant);
        auto grant_val = sim.get_value(result.grant);
        auto valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 8)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 8)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 8)
                  << ", valid=" << valid_val << std::endl;

        // After position 4, should go to position 5 but it's not requested,
        // so position 6 should be granted
        REQUIRE(sim.get_value(result.grant) == 0b01000000); // Bit 6 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Continue with same selector but different values
        sim.set_value(request, 01010000_b);    // Same requests
        sim.set_value(last_grant, 01000000_b); // Last granted was position 6
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 8)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 8)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 8)
                  << ", valid=" << valid_val << std::endl;

        // After position 6, should go to position 7 but it's not requested,
        // wrap around to position 0 but it's not requested,
        // continue to position 1 but it's not requested,
        // continue to position 2 but it's not requested,
        // continue to position 3 but it's not requested,
        // continue to position 4 which is requested
        REQUIRE(sim.get_value(result.grant) == 0b00010000); // Bit 4 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Another iteration with all positions requested
        sim.set_value(request, 11111111_b);    // All positions requested now
        sim.set_value(last_grant, 00010000_b); // Last granted was position 4
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 8)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 8)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 8)
                  << ", valid=" << valid_val << std::endl;

        // After position 4, should go to position 5
        REQUIRE(sim.get_value(result.grant) == 0b00100000); // Bit 5 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Consecutive requests test") {
        ch_uint<6> request(001110_b,
                           "request"); // Requests at positions 1, 2, 3
        ch_uint<6> last_grant(000010_b,
                              "last_grant"); // Last granted was position 1

        PrioritySelectorResult<6> result =
            round_robin_selector<6>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Get values before printing
        auto request_val = sim.get_value(request);
        auto last_grant_val = sim.get_value(last_grant);
        auto grant_val = sim.get_value(result.grant);
        auto valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 6)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 6)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 6)
                  << ", valid=" << valid_val << std::endl;

        // After position 1, should go to position 2
        REQUIRE(sim.get_value(result.grant) == 0b000100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Continue with same selector but different values
        sim.set_value(request, 001110_b);    // Same requests
        sim.set_value(last_grant, 000100_b); // Last granted was position 2
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 6)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 6)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 6)
                  << ", valid=" << valid_val << std::endl;

        // After position 2, should go to position 3
        REQUIRE(sim.get_value(result.grant) == 0b001000); // Bit 3 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Now change to different consecutive requests
        sim.set_value(request, 110000_b);    // Requests at positions 4, 5
        sim.set_value(last_grant, 001000_b); // Last granted was position 3
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 6)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 6)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 6)
                  << ", valid=" << valid_val << std::endl;

        // After position 3, should go to position 4 which is requested
        REQUIRE(sim.get_value(result.grant) == 0b010000); // Bit 4 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Single request in various positions test") {
        ch_uint<4> request(0001_b, "request"); // Request only at position 0
        ch_uint<4> last_grant(1000_b,
                              "last_grant"); // Last granted was position 3

        PrioritySelectorResult<4> result =
            round_robin_selector<4>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Get values before printing
        auto request_val = sim.get_value(request);
        auto last_grant_val = sim.get_value(last_grant);
        auto grant_val = sim.get_value(result.grant);
        auto valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 4)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 4)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 4)
                  << ", valid=" << valid_val << std::endl;

        // After position 3, should go to position 0 and it is requested
        REQUIRE(sim.get_value(result.grant) == 0b0001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Continue with same selector but different values
        sim.set_value(request, 1000_b);    // Request only at position 3
        sim.set_value(last_grant, 0100_b); // Last granted was position 2
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 4)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 4)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 4)
                  << ", valid=" << valid_val << std::endl;

        // After position 2, should go to position 3 and it is requested
        REQUIRE(sim.get_value(result.grant) == 0b1000); // Bit 3 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Test with request at position 1
        sim.set_value(request, 0010_b);    // Request only at position 1
        sim.set_value(last_grant, 1000_b); // Last granted was position 3
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 4)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 4)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 4)
                  << ", valid=" << valid_val << std::endl;

        // After position 3, should go to position 0 but not requested, then
        // position 1 which is requested
        REQUIRE(sim.get_value(result.grant) == 0b0010); // Bit 1 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Complex sequence with mixed patterns") {
        ch_uint<5> request(10101_b, "request"); // Requests at positions 0, 2, 4
        ch_uint<5> last_grant(00001_b,
                              "last_grant"); // Last granted was position 0

        PrioritySelectorResult<5> result =
            round_robin_selector<5>(request, last_grant);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Get values before printing
        auto request_val = sim.get_value(request);
        auto last_grant_val = sim.get_value(last_grant);
        auto grant_val = sim.get_value(result.grant);
        auto valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 5)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 5)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 5)
                  << ", valid=" << valid_val << std::endl;

        // After position 0, position 1 is not requested, position 2 is
        // requested
        REQUIRE(sim.get_value(result.grant) == 0b00100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Continue the sequence
        sim.set_value(request, 10101_b);    // Same request pattern
        sim.set_value(last_grant, 00100_b); // Last granted was position 2
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 5)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 5)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 5)
                  << ", valid=" << valid_val << std::endl;

        // After position 2, positions 3 is not requested, position 4 is
        // requested
        REQUIRE(sim.get_value(result.grant) == 0b10000); // Bit 4 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Continue the sequence - wraparound
        sim.set_value(request, 10101_b);    // Same request pattern
        sim.set_value(last_grant, 10000_b); // Last granted was position 4
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 5)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 5)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 5)
                  << ", valid=" << valid_val << std::endl;

        // After position 4, wrap around: position 0 is requested
        REQUIRE(sim.get_value(result.grant) == 0b00001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Change to different pattern
        sim.set_value(request, 01010_b);    // Requests at positions 1, 3
        sim.set_value(last_grant, 00001_b); // Last granted was position 0
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 5)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 5)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 5)
                  << ", valid=" << valid_val << std::endl;

        // After position 0, position 1 is requested
        REQUIRE(sim.get_value(result.grant) == 0b00010); // Bit 1 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Continue with same pattern
        sim.set_value(request, 01010_b);    // Requests at positions 1, 3
        sim.set_value(last_grant, 00010_b); // Last granted was position 1
        sim.tick();

        // Get values before printing
        request_val = sim.get_value(request);
        last_grant_val = sim.get_value(last_grant);
        grant_val = sim.get_value(result.grant);
        valid_val = sim.get_value(result.valid);

        // Print inputs and outputs in binary
        std::cout << "Input: request=0b" << to_binary_string(request_val, 5)
                  << ", last_grant=0b" << to_binary_string(last_grant_val, 5)
                  << std::endl;
        std::cout << "Output: grant=0b" << to_binary_string(grant_val, 5)
                  << ", valid=" << valid_val << std::endl;

        // After position 1, position 2 is not requested, position 3 is
        // requested
        REQUIRE(sim.get_value(result.grant) == 0b01000); // Bit 3 set
        REQUIRE(sim.get_value(result.valid) == true);
    }
}

TEST_CASE("SelectorArbiter: round robin selector",
          "[selector_arbiter][round_robin_selector]") {
    auto ctx = std::make_unique<ch::core::context>("test_round_robin_selector");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin selection with starting position 0") {
        ch_uint<4> request(0101_b, "request"); // Requests at positions 0 and 2
        ch_uint<4> last_grant(0001_b,
                              "last_grand"); // Last granted was position 0, so
                                             // next should start from 1
        PrioritySelectorResult<4> result =
            round_robin_selector<4>(request, last_grant);

        ch::Simulator sim(ctx.get(), "trace.ini");
        sim.tick();
        std::cout << "Grand(0101 0001): " << sim.get_value(result.grant)
                  << std::endl;

        sim.set_value(request, 0100_b);
        sim.set_value(last_grant, 0100_b);
        sim.tick();
        std::cout << "Grand(0101 0100): " << sim.get_value(result.grant)
                  << std::endl;

        sim.set_value(request, 1111_b);
        sim.set_value(last_grant, 0001_b);
        sim.tick();
        std::cout << "Grand(1111 0001): " << sim.get_value(result.grant)
                  << std::endl;

        sim.set_value(request, 1111_b);
        sim.set_value(last_grant, 0010_b);
        sim.tick();
        std::cout << "Grand(1111 0001): " << sim.get_value(result.grant)
                  << std::endl;

        sim.set_value(request, 1111_b);
        sim.set_value(last_grant, 0100_b);
        sim.tick();
        std::cout << "Grand(1111 0001): " << sim.get_value(result.grant)
                  << std::endl;

        sim.set_value(request, 1111_b);
        sim.set_value(last_grant, 1000_b);
        sim.tick();
        std::cout << "Grand(1111 1000): " << sim.get_value(result.grant)
                  << std::endl;

        sim.toVCD("rrs_trace.vcd");
        ch::toDAG("rrs_trace.dot", ctx.get(), sim);

        // After position 0 (encoded as 0001_b), next available is position 1,
        // then position 2 Position 1 is checked first but not requested,
        // position 2 is checked second and is requested
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
