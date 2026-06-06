// test_mod_width_simple.cpp
// Simple, focused test for mod_op result-width calculation with the
// supported make_literal<N, W>() factory. This is the minimal "smoke test"
// companion to the more comprehensive test_mod_width.cpp.

#include "catch_amalgamated.hpp"
#include "../include/core/operators.h"
#include "../include/core/literal.h"
#include "../include/core/uint.h"

using namespace ch::core;

TEST_CASE("mod_op width: literal 8 yields 3-bit result", "[mod_width][simple]") {
    // mod 8 -> range 0..7 -> 3 bits
    constexpr auto literal_8 = make_literal<8, 4>();
    constexpr unsigned width_result_8 =
        get_binary_result_width<mod_op, ch_uint<16>, decltype(literal_8)>();
    REQUIRE(width_result_8 == 3);
}

TEST_CASE("mod_op width: literal 16 yields 4-bit result", "[mod_width][simple]") {
    // mod 16 -> range 0..15 -> 4 bits
    constexpr auto literal_16 = make_literal<16, 5>();
    constexpr unsigned width_result_16 =
        get_binary_result_width<mod_op, ch_uint<32>, decltype(literal_16)>();
    REQUIRE(width_result_16 == 4);
}

TEST_CASE("mod_op width: literal 7 yields 3-bit result", "[mod_width][simple]") {
    // mod 7 -> range 0..6 -> 3 bits
    constexpr auto literal_7 = make_literal<7, 3>();
    constexpr unsigned width_result_7 =
        get_binary_result_width<mod_op, ch_uint<16>, decltype(literal_7)>();
    REQUIRE(width_result_7 == 3);
}
