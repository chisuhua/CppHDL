#include "catch_amalgamated.hpp"
#include "chlib/bitwise.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Bitwise: leading zero detector", "[bitwise][leading_zero]") {
    auto ctx = std::make_unique<ch::core::context>("test_leading_zero");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Leading zeros in 8-bit value") {
        ch_uint<8> input(0b00010010_d);
        ch_uint<4> result = leading_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 3);  // 3 leading zeros
    }
    
    SECTION("All zeros") {
        ch_uint<8> input(0b00000000_d);
        ch_uint<4> result = leading_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 8);  // 8 leading zeros
    }
    
    SECTION("No leading zeros") {
        ch_uint<8> input(0b11111111_d);
        ch_uint<4> result = leading_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // 0 leading zeros
    }
    
    SECTION("Single bit set at MSB") {
        ch_uint<8> input(0b10000000_d);
        ch_uint<4> result = leading_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // 0 leading zeros
    }
    
    SECTION("Single bit set") {
        ch_uint<8> input(0b00000001_d);
        ch_uint<4> result = leading_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 7);  // 7 leading zeros
    }
}

TEST_CASE("Bitwise: leading one detector", "[bitwise][leading_one]") {
    auto ctx = std::make_unique<ch::core::context>("test_leading_one");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Leading ones in 8-bit value") {
        ch_uint<8> input(0b11101010_d);
        ch_uint<4> result = leading_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 3);  // 3 leading ones
    }
    
    SECTION("All ones") {
        ch_uint<8> input(0b11111111_d);
        ch_uint<4> result = leading_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 8);  // 8 leading ones
    }
    
    SECTION("No leading ones") {
        ch_uint<8> input(0b00000000_d);
        ch_uint<4> result = leading_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // 0 leading ones
    }
    
    SECTION("Single zero at MSB") {
        ch_uint<8> input(0b01111111_d);
        ch_uint<4> result = leading_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // 0 leading ones
    }
    
    SECTION("Single one at LSB") {
        ch_uint<8> input(0b00000001_d);
        ch_uint<4> result = leading_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // 0 leading ones
    }
}

TEST_CASE("Bitwise: trailing zero detector", "[bitwise][trailing_zero]") {
    auto ctx = std::make_unique<ch::core::context>("test_trailing_zero");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Trailing zeros in 8-bit value") {
        ch_uint<8> input(0b10100000_d);
        ch_uint<4> result = trailing_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 5);  // 5 trailing zeros
    }
    
    SECTION("All zeros") {
        ch_uint<8> input(0b00000000_d);
        ch_uint<4> result = trailing_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 8);  // 8 trailing zeros
    }
    
    SECTION("No trailing zeros") {
        ch_uint<8> input(0b11111111_d);
        ch_uint<4> result = trailing_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // 0 trailing zeros
    }
    
    SECTION("Single bit set at LSB") {
        ch_uint<8> input(0b00000001_d);
        ch_uint<4> result = trailing_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // 0 trailing zeros
    }
    
    SECTION("Multiple trailing zeros") {
        ch_uint<8> input(0b11110000_d);
        ch_uint<4> result = trailing_zero_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 4);  // 4 trailing zeros
    }
}

TEST_CASE("Bitwise: trailing one detector", "[bitwise][trailing_one]") {
    auto ctx = std::make_unique<ch::core::context>("test_trailing_one");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Trailing ones in 8-bit value") {
        ch_uint<8> input(0b01011111_d);
        ch_uint<4> result = trailing_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 5);  // 5 trailing ones
    }
    
    SECTION("All ones") {
        ch_uint<8> input(0b11111111_d);
        ch_uint<4> result = trailing_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 8);  // 8 trailing ones
    }
    
    SECTION("No trailing ones") {
        ch_uint<8> input(0b00000000_d);
        ch_uint<4> result = trailing_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // 0 trailing ones
    }
    
    SECTION("Single one at LSB") {
        ch_uint<8> input(0b00000001_d);
        ch_uint<4> result = trailing_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 1);  // 1 trailing one
    }
    
    SECTION("Multiple trailing ones") {
        ch_uint<8> input(0b11110011_d);
        ch_uint<4> result = trailing_one_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 2);  // 2 trailing ones
    }
}

TEST_CASE("Bitwise: population count", "[bitwise][popcount]") {
    auto ctx = std::make_unique<ch::core::context>("test_popcount");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Count of 8-bit value with 3 ones") {
        ch_uint<8> input(0b10101000_d);
        ch_uint<4> result = population_count<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 3);  // 3 ones
    }
    
    SECTION("All zeros") {
        ch_uint<8> input(0b00000000_d);
        ch_uint<4> result = population_count<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // 0 ones
    }
    
    SECTION("All ones") {
        ch_uint<8> input(0b11111111_d);
        ch_uint<4> result = population_count<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 8);  // 8 ones
    }
    
    SECTION("Single one") {
        ch_uint<8> input(0b00010000_d);
        ch_uint<4> result = population_count<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 1);  // 1 one
    }
    
    SECTION("Alternating pattern") {
        ch_uint<8> input(0b10101010_d);
        ch_uint<4> result = population_count<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 4);  // 4 ones
    }
}

TEST_CASE("Bitwise: bit reversal", "[bitwise][bit_reversal]") {
    auto ctx = std::make_unique<ch::core::context>("test_bit_reversal");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Reversal of 8-bit value") {
        ch_uint<8> input(0b11001010_d);  // 0b11001010
        ch_uint<8> result = bit_reversal<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b01010011);  // Reversed bits
    }
    
    SECTION("Reversal of all zeros") {
        ch_uint<8> input(0b00000000_d);
        ch_uint<8> result = bit_reversal<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b00000000);  // Still zeros
    }
    
    SECTION("Reversal of all ones") {
        ch_uint<8> input(0b11111111_d);
        ch_uint<8> result = bit_reversal<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b11111111);  // Still all ones
    }
    
    SECTION("Reversal of single bit") {
        ch_uint<8> input(0b10000000_d);
        ch_uint<8> result = bit_reversal<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b00000001);  // Bit moved to LSB
    }
}

TEST_CASE("Bitwise: bit swap", "[bitwise][bit_swap]") {
    auto ctx = std::make_unique<ch::core::context>("test_bit_swap");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Swap bits in 8-bit value") {
        ch_uint<8> input(0b10110100_d);
        ch_uint<8> result = bit_swap<8>(input, 2, 5);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Original: 10110100
        // bit 2 (0-indexed) is 0, bit 5 is 1
        // After swap: bit 2 should be 1, bit 5 should be 0
        // Result: 10100110 = 0x86 = 134
        REQUIRE(simulator.get_value(result) == 0b10100110);
    }
    
    SECTION("Swap same bit") {
        ch_uint<8> input(0b10110100_d);
        ch_uint<8> result = bit_swap<8>(input, 3, 3);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Swapping same bit should not change value
        REQUIRE(simulator.get_value(result) == 0b10110100);
    }
}

TEST_CASE("Bitwise: first set bit detector", "[bitwise][first_set_bit]") {
    auto ctx = std::make_unique<ch::core::context>("test_first_set_bit");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("First set bit in 8-bit value") {
        ch_uint<8> input(0b00010100_d);
        ch_uint<4> result = first_set_bit_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 2);  // First set bit at position 2
    }
    
    SECTION("All zeros") {
        ch_uint<8> input(0b00000000_d);
        ch_uint<4> result = first_set_bit_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 8);  // No set bit, returns N
    }
    
    SECTION("LSB set") {
        ch_uint<8> input(0b00000001_d);
        ch_uint<4> result = first_set_bit_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);  // LSB at position 0
    }
    
    SECTION("MSB set") {
        ch_uint<8> input(0b10000000_d);
        ch_uint<4> result = first_set_bit_detector<8>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 7);  // MSB at position 7
    }
}

TEST_CASE("Bitwise: bit field extract", "[bitwise][bit_field_extract]") {
    auto ctx = std::make_unique<ch::core::context>("test_bit_field_extract");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Extract middle field from 8-bit value") {
        ch_uint<8> input(0b11010110_d);
        ch_uint<8> result = bit_field_extract<8>(input, 2, 3);  // Extract 3 bits starting from position 2
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Input: 11010110
        // Extracting bits [4:2] -> 101 (binary) = 5 (decimal)
        REQUIRE(simulator.get_value(result) == 5);
    }
    
    SECTION("Extract from LSB") {
        ch_uint<8> input(0b11010110_d);
        ch_uint<8> result = bit_field_extract<8>(input, 0, 3);  // Extract 3 bits starting from position 0
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Input: 11010110
        // Extracting bits [2:0] -> 110 (binary) = 6 (decimal)
        REQUIRE(simulator.get_value(result) == 6);
    }
}

TEST_CASE("Bitwise: bit field insert", "[bitwise][bit_field_insert]") {
    auto ctx = std::make_unique<ch::core::context>("test_bit_field_insert");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Insert field into 8-bit value") {
        ch_uint<8> input(0b11010110_d);
        ch_uint<8> value_to_insert(0b011_d);  // 3 bits: 011
        ch_uint<8> result = bit_field_insert<8>(input, value_to_insert, 2, 3);  // Insert at position 2, width 3
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Input: 11010110
        // Clearing bits [4:2]: 11000110
        // Inserting 011 at positions [4:2]: 11011110
        REQUIRE(simulator.get_value(result) == 0b11011110);
    }
    
    SECTION("Insert with no change") {
        ch_uint<8> input(0b11010110_d);
        ch_uint<8> value_to_insert(0b101_d);  // 3 bits: 101 (same as original at pos 2-4)
        ch_uint<8> result = bit_field_insert<8>(input, value_to_insert, 2, 3);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Should remain unchanged
        REQUIRE(simulator.get_value(result) == 0b11010110);
    }
}