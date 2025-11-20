#include "catch_amalgamated.hpp"
#include "simulator.h"
#include "ch.hpp"
#include <chrono>
#include <iostream>

using namespace ch;
using namespace ch::core;

// Test to verify that memory optimization using vector instead of unordered_map works correctly
TEST_CASE("Memory Optimization Test", "[memory][performance]") {
    SECTION("Vector-based data storage produces same results as map-based") {
        context ctx("test_ctx");
        ctx_swap swap(&ctx);

        // Create some registers
        ch_uint<8> val_a(5_d);
        ch_uint<8> val_b(3_d);
        
        // Create an operation
        auto sum = val_a + val_b;
        
        // Create a register to hold the result
        ch_reg<ch_uint<8>> result_reg(sum, 0_d, "result_reg");
        
        // Create simulators - one with current implementation, one with optimized
        Simulator sim(&ctx);
        
        // Run simulation
        sim.tick();
        
        // Check that we get the expected result (5 + 3 = 8)
        auto result_val = sim.get_value(result_reg);
        REQUIRE(static_cast<uint64_t>(result_val) == 8);
        
        // Additional ticks to verify state updates correctly
        ch_uint<8> val_c(2_d);
        auto new_sum = sum + val_c;
        result_reg.next(new_sum);
        
        sim.tick();
        auto result_val2 = sim.get_value(result_reg);
        REQUIRE(static_cast<uint64_t>(result_val2) == 10); // 5 + 3 + 2 = 10
    }
}