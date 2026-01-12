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

TEST_CASE("SelectorArbiter: priority selector basic tests",
          "[selector_arbiter][priority_selector][basic]") {
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

TEST_CASE("SelectorArbiter: priority selector extended tests",
          "[selector_arbiter][priority_selector][extended]") {
    auto ctx =
        std::make_unique<ch::core::context>("test_priority_selector_extended");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("All positions except one tested") {
        ch_uint<16> request(
            1111111111111101_b); // All bits set except position 1
        PrioritySelectorResult<16> result = priority_selector<16>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Priority to lower bits, so bit 0 should be granted (since bit 1 is
        // not set)
        REQUIRE(sim.get_value(result.grant) == 0b0000000000000001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("High position request") {
        ch_uint<16> request(0100000000000000_b); // Only bit 14 set
        PrioritySelectorResult<16> result = priority_selector<16>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) ==
                0b0100000000000000); // Bit 14 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Alternating pattern") {
        ch_uint<12> request(010101010101_b); // Alternating: 0,2,4,6,8,10 set
        PrioritySelectorResult<12> result = priority_selector<12>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Priority to lower bits, so bit 0 should be granted
        REQUIRE(sim.get_value(result.grant) == 0b000000000001); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    // SECTION("Large width with high priority request") {
    //     ch_uint<64> request(0); // Initialize to 0
    //     // Set a few high-order bits
    //     request = 0x8000000000000000UL; // Bit 63 set
    //     // Also set some lower bits to test priority
    //     request = request | 0x0000000000000014UL; // Bits 2 and 4 set
    //     PrioritySelectorResult<64> result = priority_selector<64>(request);

    //     ch::Simulator sim(ctx.get());
    //     sim.tick();

    //     // Priority to lower bits, so bit 2 should be granted
    //     REQUIRE(sim.get_value(result.grant) ==
    //             0x0000000000000004UL); // Bit 2 set
    //     REQUIRE(sim.get_value(result.valid) == true);
    // }

    SECTION("Edge case: 1-bit width") {
        ch_uint<1> request(1_b); // Only possible request
        PrioritySelectorResult<1> result = priority_selector<1>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 1); // Only possible grant
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Edge case: 2-bit width, both set") {
        ch_uint<2> request(11_b); // Both bits set
        PrioritySelectorResult<2> result = priority_selector<2>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Priority to lower bits, so bit 0 should be granted
        REQUIRE(sim.get_value(result.grant) == 0b01); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Sequential testing of different widths") {
        // Test 4-bit width
        ch_uint<4> request4(1011_b); // Bits 0, 1, 3 set
        PrioritySelectorResult<4> result4 = priority_selector<4>(request4);

        // Test 12-bit width with same pattern
        ch_uint<12> request12(000000101100_b); // Bits 2, 3, 5 set
        PrioritySelectorResult<12> result12 = priority_selector<12>(request12);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Print input and output values in binary format as per specification
        auto request4_val = sim.get_value(request4);
        auto grant4_val = sim.get_value(result4.grant);
        auto valid4_val = sim.get_value(result4.valid);

        std::cout << "4-bit priority selector:" << std::endl;
        std::cout << "  request: 0b" << to_binary_string(request4_val, 4)
                  << std::endl;
        std::cout << "  grant:   0b" << to_binary_string(grant4_val, 4)
                  << std::endl;
        std::cout << "  valid:   " << valid4_val << std::endl;

        REQUIRE(grant4_val == 0b0001); // Bit 0 set
        REQUIRE(valid4_val == true);

        // sim.reset();
        // sim.tick();

        // Print input and output values in binary format as per specification
        auto request12_val = sim.get_value(request12);
        auto grant12_val = sim.get_value(result12.grant);
        auto valid12_val = sim.get_value(result12.valid);

        std::cout << "12-bit priority selector:" << std::endl;
        std::cout << "  request: 0b" << to_binary_string(request12_val, 12)
                  << std::endl;
        std::cout << "  grant:   0b" << to_binary_string(grant12_val, 12)
                  << std::endl;
        std::cout << "  valid:   " << valid12_val << std::endl;

        REQUIRE(grant12_val == 0b000000000100); // Bit 2 set
        REQUIRE(valid12_val == true);
    }
}

TEST_CASE("SelectorArbiter: priority selector stress tests",
          "[selector_arbiter][priority_selector][stress]") {
    auto ctx =
        std::make_unique<ch::core::context>("test_priority_selector_stress");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Maximum width priority selector") {
        ch_uint<64> request(0xFFFFFFFFFFFFFFFFUL); // All 64 bits set
        PrioritySelectorResult<64> result = priority_selector<64>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) ==
                0x0000000000000001UL); // Bit 0 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Alternating stress test") {
        for (int width = 2; width <= 16; width++) {
            std::string ctx_name =
                "test_priority_width_" + std::to_string(width);
            auto local_ctx = std::make_unique<ch::core::context>(ctx_name);
            ch::core::ctx_swap local_swapper(local_ctx.get());

            // Create request with alternating pattern
            uint64_t req_val = 0;
            for (int i = 0; i < width; i += 2) {
                req_val |= (1UL << i);
            }

            auto request = ch_uint<16>(req_val);
            PrioritySelectorResult<16> result = priority_selector<16>(request);

            ch::Simulator sim(local_ctx.get());
            sim.tick();

            // Should always grant the lowest bit that's set
            REQUIRE(sim.get_value(result.grant) == 0x0001);
            REQUIRE(sim.get_value(result.valid) == true);
        }
    }

    SECTION("Priority selector with various bit patterns") {
        // Test different bit patterns
        std::vector<std::pair<uint64_t, uint64_t>> test_cases = {
            {0x0F0F0F0F0F0F0F0F, 0x0000000000000001}, // Pattern 1
            {0xF0F0F0F0F0F0F0F0, 0x0000000000000010}, // Pattern 2
            {0x5555555555555555, 0x0000000000000001}, // Alternating 1
            {0xAAAAAAAAAAAAAAAA, 0x0000000000000002}, // Alternating 2
            {0x00000000FFFFFFFF, 0x0000000000000001}, // Lower half
            {0xFFFFFFFF00000000, 0x0000000100000000}  // Upper half
        };

        for (size_t i = 0; i < test_cases.size(); ++i) {
            auto [req_val, expected_grant] = test_cases[i];

            std::string ctx_name = "test_priority_pattern_" + std::to_string(i);
            auto local_ctx = std::make_unique<ch::core::context>(ctx_name);
            ch::core::ctx_swap local_swapper(local_ctx.get());

            ch_uint<64> request(req_val);
            PrioritySelectorResult<64> result = priority_selector<64>(request);

            ch::Simulator sim(local_ctx.get());
            sim.tick();

            REQUIRE(sim.get_value(result.grant) == expected_grant);
            REQUIRE(sim.get_value(result.valid) == true);
        }
    }
}

TEST_CASE("SelectorArbiter: priority selector consistency tests",
          "[selector_arbiter][priority_selector][consistency]") {
    auto ctx = std::make_unique<ch::core::context>("test_priority_consistency");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Consistency across multiple ticks") {
        ch_uint<8> request(01010101_b); // Bits 0, 2, 4, 6 set
        PrioritySelectorResult<8> result = priority_selector<8>(request);

        ch::Simulator sim(ctx.get());

        // Run multiple ticks and verify consistent results
        for (int i = 0; i < 5; ++i) {
            sim.tick();
            REQUIRE(sim.get_value(result.grant) ==
                    0b00000001); // Bit 0 should always be granted
            REQUIRE(sim.get_value(result.valid) == true);
        }
    }

    SECTION("Priority selector with dynamic values") {
        ch_uint<8> request;
        request = 0xFF_h; // All bits set initially
        PrioritySelectorResult<8> result = priority_selector<8>(request);

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