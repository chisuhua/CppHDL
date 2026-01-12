#include "catch_amalgamated.hpp"
#include "chlib/selector_arbiter.h"
#include "codegen_dag.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
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

TEST_CASE("SelectorArbiter: round robin arbiter basic tests",
          "[selector_arbiter][round_robin_arbiter][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_round_robin_arbiter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin arbiter with multiple requests") {
        ch_uint<4> request(0101_b); // Requests at positions 0 and 2
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // First time: should grant position 0 (as internal ptr_reg starts with
        // 0)
        REQUIRE(sim.get_value(result.grant) == 0b0001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with different request pattern") {
        ch_uint<4> request(1100_b); // Requests at positions 2 and 3
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Should grant position 2 (as internal ptr_reg starts with 0)
        REQUIRE(sim.get_value(result.grant) == 0b0100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with no requests") {
        ch_uint<4> request(0000_b); // No requests
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0b0000); // No grants
        REQUIRE(sim.get_value(result.valid) == false);
    }

    SECTION("Round robin arbiter with all requests") {
        ch_uint<4> request(1111_b); // All requests active
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Should grant position 0 (as internal ptr_reg starts with 0)
        REQUIRE(sim.get_value(result.grant) == 0b0001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter sequential behavior") {
        ch_uint<4> request(1101_b); // Requests at positions 0, 2, 3
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // First time: should grant position 0 (as internal ptr_reg starts with
        // 0)
        REQUIRE(sim.get_value(result.grant) == 0b0001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }
}

TEST_CASE("SelectorArbiter: round robin arbiter extended tests",
          "[selector_arbiter][round_robin_arbiter][extended]") {
    auto ctx = std::make_unique<ch::core::context>(
        "test_round_robin_arbiter_extended");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin arbiter 8-bit width") {
        ch_uint<8> request(0b01010101); // Alternating pattern
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Should grant position 0 (lowest set bit initially)
        REQUIRE(sim.get_value(result.grant) == 0b00000001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with high bit requests") {
        ch_uint<8> request(0b10000000); // Only highest bit set
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0b10000000); // Bit 7 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with middle bits") {
        ch_uint<8> request(0b00110000); // Bits 4 and 5 set
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0b00010000); // Bit 4 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with single bit at various positions") {
        for (int i = 0; i < 8; i++) {
            ch_uint<8> request(1U << i); // Only bit i set
            RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

            ch::Simulator sim(ctx.get());
            sim.tick();

            REQUIRE(sim.get_value(result.grant) == (1U << i)); // Same bit set
            REQUIRE(sim.get_value(result.valid) == true);
        }
    }
}

TEST_CASE("SelectorArbiter: round robin arbiter boundary tests",
          "[selector_arbiter][round_robin_arbiter][boundary]") {
    auto ctx = std::make_unique<ch::core::context>(
        "test_round_robin_arbiter_boundary");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin arbiter 1-bit width") {
        ch_uint<1> request(1_b); // Only possible request
        RoundRobinArbiterResult<1> result = round_robin_arbiter<1>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 1); // Only possible grant
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter 2-bit width") {
        ch_uint<2> request(11_b); // Both bits set
        RoundRobinArbiterResult<2> result = round_robin_arbiter<2>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) ==
                0b01); // Bit 0 set (first available)
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with all possible 3-bit patterns") {
        for (int pattern = 1; pattern < 8; pattern++) { // Skip 0 (no requests)
            ch_uint<3> request(pattern);
            RoundRobinArbiterResult<3> result = round_robin_arbiter<3>(request);

            ch::Simulator sim(ctx.get());
            sim.tick();

            // First available bit should be granted
            bool found = false;
            for (int i = 0; i < 3; i++) {
                if ((pattern & (1 << i)) && !found) {
                    REQUIRE(sim.get_value(result.grant) == (1 << i));
                    found = true;
                }
            }
            REQUIRE(sim.get_value(result.valid) == true);
        }
    }

    SECTION("Round robin arbiter boundary: max bit set") {
        ch_uint<16> request(0x8000); // Bit 15 set (highest in 16-bit)
        RoundRobinArbiterResult<16> result = round_robin_arbiter<16>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0x8000); // Bit 15 set
        REQUIRE(sim.get_value(result.valid) == true);
    }
}

TEST_CASE("SelectorArbiter: round robin arbiter stress tests",
          "[selector_arbiter][round_robin_arbiter][stress]") {
    auto ctx =
        std::make_unique<ch::core::context>("test_round_robin_arbiter_stress");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin arbiter sequential access pattern") {
        ch_uint<8> request(0b11111111); // All bits set
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());

        // Simulate multiple ticks to check round-robin behavior
        // After granting bit 0, next grant should start from bit 1, etc.
        std::vector<uint8_t> grants;
        for (int i = 0; i < 10; i++) {
            sim.tick();
            uint8_t grant = static_cast<uint8_t>(
                static_cast<uint64_t>(sim.get_value(result.grant)));
            grants.push_back(grant);
        }

        // Check that the first few grants follow round-robin pattern
        REQUIRE(grants[0] == 0x01); // First grant is bit 0
        // Subsequent grants will depend on the internal pointer logic
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with maximum width (64-bit)") {
        ch_uint<64> request(0xFFFFFFFFFFFFFFFFUL); // All 64 bits set
        RoundRobinArbiterResult<64> result = round_robin_arbiter<64>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) ==
                0x0000000000000001UL); // Bit 0 set initially
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with various large widths") {
        // Test 32-bit width
        {
            ch_uint<32> request(0xC0000000UL); // High bits set
            RoundRobinArbiterResult<32> result =
                round_robin_arbiter<32>(request);

            ch::Simulator sim(ctx.get());
            sim.tick();

            // Print input and output values in binary format as per
            // specification
            auto request_val = sim.get_value(request);
            auto grant_val = sim.get_value(result.grant);
            auto valid_val = sim.get_value(result.valid);

            std::cout << "32-bit round robin arbiter:" << std::endl;
            std::cout << "  request: 0b" << to_binary_string(request_val, 32)
                      << std::endl;
            std::cout << "  grant:   0b" << to_binary_string(grant_val, 32)
                      << std::endl;
            std::cout << "  valid:   " << valid_val << std::endl;

            REQUIRE(grant_val == 0x40000000UL); // Bit 30 set (first in pattern)
            REQUIRE(valid_val == true);
        }

        // Test 16-bit alternating
        {
            ch_uint<16> request(0xAAAA); // Alternating 1010 pattern
            RoundRobinArbiterResult<16> result =
                round_robin_arbiter<16>(request);

            ch::Simulator sim(ctx.get());
            sim.tick();

            // Print input and output values in binary format as per
            // specification
            auto request_val = sim.get_value(request);
            auto grant_val = sim.get_value(result.grant);
            auto valid_val = sim.get_value(result.valid);

            std::cout << "16-bit round robin arbiter:" << std::endl;
            std::cout << "  request: 0b" << to_binary_string(request_val, 16)
                      << std::endl;
            std::cout << "  grant:   0b" << to_binary_string(grant_val, 16)
                      << std::endl;
            std::cout << "  valid:   " << valid_val << std::endl;

            REQUIRE(grant_val == 0x0002); // Bit 1 set (first 1 in pattern)
            REQUIRE(valid_val == true);
        }
    }

    SECTION("Round robin arbiter stress: multiple different patterns") {
        std::vector<std::pair<uint64_t, uint64_t>> test_cases = {
            {0x0F0F0F0F0F0F0F0F, 0x0000000000000001}, // Pattern 1: lowest bit
            {0xF0F0F0F0F0F0F0F0, 0x0000000000000010}, // Pattern 2: bit 4
            {0x5555555555555555,
             0x0000000000000001}, // Alternating 1: lowest bit
            {0xAAAAAAAAAAAAAAAA, 0x0000000000000002}, // Alternating 2: bit 1
            {0x00000000FFFFFFFF, 0x0000000000000001}, // Lower half: lowest bit
            {0xFFFFFFFF00000000, 0x0000000100000000}  // Upper half: bit 32
        };

        for (size_t i = 0; i < test_cases.size(); ++i) {
            auto [req_val, expected_grant] = test_cases[i];

            std::string ctx_name =
                "test_rr_arbiter_pattern_" + std::to_string(i);
            auto local_ctx = std::make_unique<ch::core::context>(ctx_name);
            ch::core::ctx_swap local_swapper(local_ctx.get());

            ch_uint<64> request(req_val);
            RoundRobinArbiterResult<64> result =
                round_robin_arbiter<64>(request);

            ch::Simulator sim(local_ctx.get());
            sim.tick();

            uint64_t actual_grant =
                static_cast<uint64_t>(sim.get_value(result.grant));
            bool has_requests = req_val != 0;

            REQUIRE(actual_grant == expected_grant);
            REQUIRE(sim.get_value(result.valid) == has_requests);
        }
    }
}

TEST_CASE("SelectorArbiter: round robin arbiter consistency tests",
          "[selector_arbiter][round_robin_arbiter][consistency]") {
    auto ctx = std::make_unique<ch::core::context>(
        "test_round_robin_arbiter_consistency");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Consistency across multiple ticks with same request") {
        ch_uint<8> request(0b01010101); // Fixed pattern
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());

        // Run multiple ticks and verify consistent starting behavior
        for (int i = 0; i < 5; ++i) {
            sim.tick();
            // First tick should always grant the first available bit (bit 0)
            // For subsequent ticks, the behavior depends on internal state
            if (i == 0) {
                REQUIRE(sim.get_value(result.grant) ==
                        0x01); // Bit 0 set initially
            }
            REQUIRE(sim.get_value(result.valid) == true);
        }
    }

    SECTION("Round robin arbiter with dynamic request changes") {
        ch_uint<8> request;
        request = ch_uint<8>(0xFF); // All bits set initially
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());

        // Test with all bits set
        sim.tick();
        REQUIRE(sim.get_value(result.grant) == 0x01); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Update request to have only high bit set
        sim.set_value(request, 0x80);
        sim.tick();
        REQUIRE(sim.get_value(result.grant) == 0x80); // Bit 7 set
        REQUIRE(sim.get_value(result.valid) == true);

        // Update request to have middle bits set
        sim.set_value(request, 0x3C); // Bits 2,3,4,5 set
        sim.tick();
        REQUIRE(sim.get_value(result.grant) == 0x04); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);
    }
}
