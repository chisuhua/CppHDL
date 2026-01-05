#include "catch_amalgamated.hpp"
#include "chlib/converter.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Converter: binary to onehot", "[converter][binary_to_onehot]") {
    auto ctx = std::make_unique<ch::core::context>("test_binary_to_onehot");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Binary 0 to onehot") {
        ch_uint<3> input(0_d);
        ch_uint<8> result = binary_to_onehot<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0b00000001); // Bit 0 set
    }

    SECTION("Binary 3 to onehot") {
        ch_uint<3> input(3_d);
        ch_uint<8> result = binary_to_onehot<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0b00001000); // Bit 3 set
    }

    SECTION("Binary max to onehot") {
        ch_uint<3> input(7_d);
        ch_uint<8> result = binary_to_onehot<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0b10000000); // Bit 7 set
    }
}

TEST_CASE("Converter: onehot to binary", "[converter][onehot_to_binary]") {
    auto ctx = std::make_unique<ch::core::context>("test_onehot_to_binary");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Onehot with bit 0 set") {
        ch_uint<8> input(00000001_b);
        ch_uint<3> result = onehot_to_binary<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }

    SECTION("Onehot with bit 5 set") {
        ch_uint<8> input(00100000_b);
        ch_uint<3> result = onehot_to_binary<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 5);
    }

    SECTION("Onehot with bit 7 set") {
        ch_uint<8> input(10000000_b);
        ch_uint<3> result = onehot_to_binary<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 7);
    }
}

// TEST_CASE("Converter: bcd to binary", "[converter][bcd_to_binary]") {
//     auto ctx = std::make_unique<ch::core::context>("test_bcd_to_binary");
//     ch::core::ctx_swap ctx_swapper(ctx.get());

//     SECTION("BCD 0000 to binary") {
//         ch_uint<4> input(0000_b);
//         ch_uint<4> result = bcd_to_binary<4>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 0);
//     }

//     SECTION("BCD 0101 to binary (5 in decimal)") {
//         ch_uint<4> input(0101_b);
//         ch_uint<4> result = bcd_to_binary<4>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 5);
//     }

//     SECTION("BCD 1001 to binary (9 in decimal)") {
//         ch_uint<4> input(1001_b);
//         ch_uint<4> result = bcd_to_binary<4>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 9);
//     }

//     SECTION("Multi-digit BCD 0010 0101 (25 in decimal)") {
//         ch_uint<8> input(00100101_b); // BCD for 25
//         ch_uint<8> result = bcd_to_binary<8>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 25);
//     }
// }

// TEST_CASE("Converter: binary to bcd", "[converter][binary_to_bcd]") {
//     auto ctx = std::make_unique<ch::core::context>("test_binary_to_bcd");
//     ch::core::ctx_swap ctx_swapper(ctx.get());

//     SECTION("Binary 0 to BCD") {
//         ch_uint<4> input(0_d);
//         ch_uint<4> result = binary_to_bcd<4>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 0);
//     }

//     SECTION("Binary 5 to BCD") {
//         ch_uint<4> input(5_d);
//         ch_uint<4> result = binary_to_bcd<4>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 5);
//     }

//     SECTION("Binary 9 to BCD") {
//         ch_uint<4> input(9_d);
//         ch_uint<4> result = binary_to_bcd<4>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 9);
//     }

//     SECTION("Binary 25 to multi-digit BCD") {
//         ch_uint<8> input(25_d);
//         ch_uint<8> result = binary_to_bcd<8>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         // Expected BCD for 25: lower 4 bits = 5, upper 4 bits = 2
//         // So result should be 0010 0101 = 0x25 = 37 in decimal
//         REQUIRE(sim.get_value(result) == 0x25);
//     }

//     SECTION("Binary 99 to multi-digit BCD") {
//         ch_uint<8> input(99_d);
//         ch_uint<8> result = binary_to_bcd<8>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         // Expected BCD for 99: lower 4 bits = 9, upper 4 bits = 9
//         // So result should be 1001 1001 = 0x99 = 153 in decimal
//         REQUIRE(sim.get_value(result) == 0x99);
//     }
// }

TEST_CASE("Converter: gray to binary", "[converter][gray_to_binary]") {
    auto ctx = std::make_unique<ch::core::context>("test_gray_to_binary");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Gray 0000 to binary") {
        ch_uint<4> input(0000_b);
        ch_uint<4> result = gray_to_binary<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }

    SECTION("Gray 0001 to binary") {
        ch_uint<4> input(0001_b);
        ch_uint<4> result = gray_to_binary<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 1);
    }

    SECTION("Gray 0011 to binary") {
        ch_uint<4> input(0011_b);
        ch_uint<4> result = gray_to_binary<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 2);
    }

    SECTION("Gray 0010 to binary") {
        ch_uint<4> input(0010_b);
        ch_uint<4> result = gray_to_binary<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 3);
    }

    SECTION("Gray 1111 to binary (max 4-bit)") {
        ch_uint<4> input(1111_b);
        ch_uint<4> result = gray_to_binary<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 10);
    }
}

TEST_CASE("Converter: binary to gray", "[converter][binary_to_gray]") {
    auto ctx = std::make_unique<ch::core::context>("test_binary_to_gray");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Binary 0 to gray") {
        ch_uint<4> input(0_d);
        ch_uint<4> result = binary_to_gray<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }

    SECTION("Binary 1 to gray") {
        ch_uint<4> input(1_d);
        ch_uint<4> result = binary_to_gray<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 1);
    }

    SECTION("Binary 2 to gray") {
        ch_uint<4> input(2_d);
        ch_uint<4> result = binary_to_gray<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 3);
    }

    SECTION("Binary 3 to gray") {
        ch_uint<4> input(3_d);
        ch_uint<4> result = binary_to_gray<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 2);
    }

    SECTION("Binary 10 to gray") {
        ch_uint<4> input(10_d);
        ch_uint<4> result = binary_to_gray<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 15);
    }
}