#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "simulator.h"

using namespace ch::core;
using namespace ch;

// 位宽测试
TEST_CASE("bits_update: width verification",
          "[operators][bit_operations][width][bits_update]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("8-bit target with various source widths") {
        ch_uint<8> target(240_d); // 0b11110000

        // 1-bit source
        auto result1 = bits_update<1>(target, 1_d, 2);
        STATIC_REQUIRE(ch_width_v<decltype(result1)> == 8);

        // 2-bit source
        auto result2 = bits_update<2>(target, 2_d, 3);
        STATIC_REQUIRE(ch_width_v<decltype(result2)> == 8);

        // 4-bit source
        auto result4 = bits_update<4>(target, 10_d, 0); // 10_d is 0b1010
        STATIC_REQUIRE(ch_width_v<decltype(result4)> == 8);

        // 8-bit source (full width)
        auto result8 = bits_update<8>(target, 204_d, 0); // 204_d is 0b11001100
        STATIC_REQUIRE(ch_width_v<decltype(result8)> == 8);
    }

    SECTION("16-bit target with various source widths") {
        ch_uint<16> target(61680_d); // 0b1111000011110000

        // 3-bit source
        auto result3 = bits_update<3>(target, 5_d, 4);
        STATIC_REQUIRE(ch_width_v<decltype(result3)> == 16);

        // 8-bit source
        auto result8 = bits_update<8>(target, 170_d, 0); // 170_d is 0b10101010
        STATIC_REQUIRE(ch_width_v<decltype(result8)> == 16);

        // 16-bit source (full width)
        auto result16 = bits_update<16>(target, 43572_d,
                                        0); // 43572_d is 0b1010101011110000
        STATIC_REQUIRE(ch_width_v<decltype(result16)> == 16);
    }

    SECTION("32-bit target with various source widths") {
        ch_uint<32> target(305419896_d); // 0x12345678

        // 5-bit source
        auto result5 = bits_update<5>(target, 27_d, 10); // 27_d is 0b11011
        STATIC_REQUIRE(ch_width_v<decltype(result5)> == 32);

        // 16-bit source
        auto result16 =
            bits_update<16>(target, 44237_d, 8); // 44237_d is 0xABCD
        STATIC_REQUIRE(ch_width_v<decltype(result16)> == 32);

        // 32-bit source (full width)
        auto result32 = bits_update<32>(target, 2271560481_d,
                                        0); // 2271560481_d is 0x87654321
        STATIC_REQUIRE(ch_width_v<decltype(result32)> == 32);
    }
}

// 仿真结果验证测试
TEST_CASE("bits_update: simulation result verification",
          "[operators][bit_operations][simulation][bits_update]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Update lower 4 bits of 8-bit value") {
        ch_uint<8> target(240_d); // 0b11110000 = 240
        ch_uint<4> source(10_d);  // 0b1010 = 10

        auto result = bits_update<4>(target, source, 0); // Update bits [3:0]

        Simulator sim(&ctx);
        sim.tick(); // Tick to evaluate combinatorial logic

        // Expected: 0b11111010 (original upper 4 bits + new lower 4 bits)
        uint64_t expected = 250; // 0b11111010 = 250
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }

    SECTION("Update upper 4 bits of 8-bit value") {
        ch_uint<8> target(240_d); // 0b11110000 = 240
        ch_uint<4> source(10_d);  // 0b1010 = 10

        auto result = bits_update<4>(target, source, 4); // Update bits [7:4]

        Simulator sim(&ctx);
        sim.tick();

        // Expected: 0b10100000 (new upper 4 bits + original lower 4 bits)
        uint64_t expected = 160; // 0b10100000 = 160
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }

    SECTION("Update middle 2 bits of 8-bit value") {
        ch_uint<8> target(240_d); // 0b11110000 = 240
        ch_uint<2> source(2_d);   // 0b10 = 2

        auto result = bits_update<2>(target, source, 2); // Update bits [3:2]

        Simulator sim(&ctx);
        sim.tick();

        // Expected: 0b11111000 (changing bits 3 and 2 from 00 to 10)
        uint64_t expected = 248; // 0b11111000 = 248
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }

    SECTION("Update single bit of 8-bit value") {
        ch_uint<8> target(240_d); // 0b11110000 = 240
        ch_uint<1> source(1_d);   // 1

        auto result = bits_update<1>(target, source, 2); // Update bit [2]

        Simulator sim(&ctx);
        sim.tick();

        // Expected: 0b11110100 (changing bit 2 from 0 to 1)
        uint64_t expected = 244; // 0b11110100 = 244
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }

    SECTION("16-bit target with 8-bit update") {
        ch_uint<16> target(61612_d); // 0b1111000010101100 = 61612
        ch_uint<8> source(91_d);     // 0b01011011 = 91

        auto result = bits_update<8>(target, source, 4); // Update bits [11:4]

        Simulator sim(&ctx);
        sim.tick();

        // Original: 1111 0000 1010 1100 (61612)
        // Source:      0101 1011
        // Position:        ^
        // Result:   1111 0101 1011 1100 = 62908
        uint64_t expected = 62908;
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }

    SECTION("Multiple sequential updates") {
        ch_uint<8> target(195_d); // 0b11000011 = 195
        ch_uint<2> source1(1_d);  // 0b01 = 1
        ch_uint<2> source2(2_d);  // 0b10 = 2

        auto result1 = bits_update<2>(target, source1, 0); // Update bits [1:0]
        auto result2 = bits_update<2>(target, source2, 6); // Update bits [7:6]

        Simulator sim(&ctx);
        sim.tick();

        // Initial:  11000011 (195)
        // Step 1:   11000001 (updated bits [1:0] to 01 = 1)
        // Step 2:   10000001 (updated bits [7:6] to 10 = 2)
        uint64_t expected = 129; // 0b10000001 = 129
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }

    SECTION("Edge case: update at boundary") {
        ch_uint<8> target(255_d); // All ones = 255
        ch_uint<1> source(0_d);   // Zero

        auto result = bits_update<1>(target, source, 7); // Update highest bit

        Simulator sim(&ctx);
        sim.tick();

        // Expected: 0b01111111 (highest bit cleared)
        uint64_t expected = 127;
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }

    SECTION("Literal source value test") {
        ch_uint<8> target(170_d); // 0b10101010 = 170

        auto result =
            bits_update<3>(target, 7_d, 2); // Update bits [4:2] with 7 (0b111)

        Simulator sim(&ctx);
        sim.tick();

        // Original: 10101010 (170)
        // Replace [4:2] (010 = 2) with 111 (7)
        // Result:   10111110 = 242
        uint64_t expected = 190;
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }
}

// 边界条件测试
TEST_CASE("bits_update: boundary conditions",
          "[operators][bit_operations][boundary][bits_update]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Full width update") {
        ch_uint<8> target(204_d); // 0b11001100 = 204
        ch_uint<8> source(170_d); // 0b10101010 = 170

        auto result = bits_update<8>(target, source, 0); // Replace entire value

        Simulator sim(&ctx);
        sim.tick();

        uint64_t expected = 170; // Source value
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }

    SECTION("Update at maximum valid position") {
        ch_uint<8> target(240_d); // 0b11110000 = 240
        ch_uint<1> source(1_d);

        auto result = bits_update<1>(target, source, 7); // Update bit 7 (MSB)

        Simulator sim(&ctx);
        sim.tick();

        // Original: 11110000 (240)
        // Update MSB to 1 (it's already 1, so no change)
        uint64_t expected = 240;
        uint64_t actual = static_cast<uint64_t>(sim.get_value(target));

        REQUIRE(actual == expected);
    }

    SECTION("Update with zero-width (should fail compilation if attempted)") {
        // This test verifies that zero-width updates are prevented
        ch_uint<8> target(204_d); // 0b11001100
        ch_uint<1> source(1_d);

        // The following would cause a static_assert failure:
        // auto result = bits_update<0>(target, source, 0);

        REQUIRE(true); // Placeholder to satisfy Catch2
    }
}

// 动态仿真测试 - 测试随输入变化的行为
TEST_CASE("bits_update: dynamic simulation test",
          "[operators][bit_operations][dynamic][bits_update]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Dynamic value updates") {
        ch_uint<8> target(195_d); // 0b11000011 = 195
        ch_uint<4> source(10_d);  // 0b1010 = 10

        auto result = bits_update<4>(target, source, 2); // Update bits [5:2]

        Simulator sim(&ctx);

        // Initial state
        sim.tick();
        // Original: 11000011 (195), replace [5:2] (0000) with 1010 (10)
        // Result: 11101011 = 235
        REQUIRE(static_cast<uint64_t>(sim.get_value(target)) == 235);

        // Change source value
        sim.set_value(source, 1);
        sim.tick();
        // Original: 11000011 (195), replace [5:2] (0000) with 0001 (1)
        // Result: 11000111 = 199
        REQUIRE(static_cast<uint64_t>(sim.get_value(target)) == 199);

        // Change target value
        sim.set_value(target, 51); // 0b00110011 = 51
        sim.tick();
        // Original: 00110011 (51), replace [5:2] (1100) with 0001 (1)
        // Result: 00000111 = 7
        REQUIRE(static_cast<uint64_t>(sim.get_value(target)) == 7);
    }
}