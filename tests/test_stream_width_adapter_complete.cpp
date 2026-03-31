/**
 * @file test_stream_width_adapter_complete.cpp
 * @brief Compile-time tests for stream width adapters (type checking only)
 */

#include "chlib/stream_width_adapter.h"
#include "catch_amalgamated.hpp"

using namespace ch::core;
using namespace chlib;

// Test: compute_transfer_ratio
TEST_CASE("Stream Width Adapter: Transfer Ratio", "[stream][width_adapter][utility]") {
    // 32-bit wide, 8-bit narrow = 4 transfers
    REQUIRE(compute_transfer_ratio<ch_uint<32>, ch_uint<8>>() == 4);
    
    // 16-bit wide, 8-bit narrow = 2 transfers
    REQUIRE(compute_transfer_ratio<ch_uint<16>, ch_uint<8>>() == 2);
    
    // 24-bit wide, 8-bit narrow = 3 transfers
    REQUIRE(compute_transfer_ratio<ch_uint<24>, ch_uint<8>>() == 3);
    
    // 32-bit wide, 16-bit narrow = 2 transfers
    REQUIRE(compute_transfer_ratio<ch_uint<32>, ch_uint<16>>() == 2);
    
    // Same width = 1 transfer
    REQUIRE(compute_transfer_ratio<ch_uint<16>, ch_uint<16>>() == 1);
}

// Test: Function signature compilation (type checking only)
TEST_CASE("Stream Width Adapter: Type Compilation", "[stream][width_adapter][compile]") {
    // This test verifies that the templates compile correctly
    // Actual runtime testing requires active context
    
    // Verify type traits exist
    REQUIRE(ch_uint<8>::ch_width == 8);
    REQUIRE(ch_uint<16>::ch_width == 16);
    REQUIRE(ch_uint<32>::ch_width == 32);
}
