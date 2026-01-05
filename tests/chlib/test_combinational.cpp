#include "catch_amalgamated.hpp"
#include "chlib/bitwise.h"
#include "chlib/combinational.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <array>
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Combinational: priority encoder function",
          "[combinational][priority_encoder]") {
    auto ctx = std::make_unique<ch::core::context>("test_priority_encoder");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Single bit set") {
        ch_uint<8> input(0x10_h);
        ch_uint<3> result = priority_encoder<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 4);
    }

    SECTION("Multiple bits set - priority from high to lower") {
        ch_uint<8> input(0x32_h);
        ch_uint<3> result = priority_encoder<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 5);
    }

    SECTION("No bits set") {
        ch_uint<8> input(0x00_h);
        ch_uint<3> result = priority_encoder<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }
}

TEST_CASE("Combinational: binary encoder function",
          "[combinational][binary_encoder]") {
    auto ctx = std::make_unique<ch::core::context>("test_binary_encoder");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Single bit set") {
        ch_uint<8> input(0x20_h);
        ch_uint<3> result = binary_encoder<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 5);
    }
}

TEST_CASE("Combinational: binary decoder function",
          "[combinational][binary_decoder]") {
    auto ctx = std::make_unique<ch::core::context>("test_binary_decoder");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Decode index 3") {
        ch_uint<3> input(3_d);
        ch_uint<8> result = binary_decoder<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0x08_h);
    }

    SECTION("Decode index 0") {
        ch_uint<3> input(0_d);
        ch_uint<8> result = binary_decoder<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0x01_h);
    }
}

TEST_CASE("Combinational: demux function", "[combinational][demux]") {
    auto ctx = std::make_unique<ch::core::context>("test_demux");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Demux to position 2") {
        ch_uint<4> input(1010_b);
        ch_uint<2> sel(2_d);
        auto outputs = demux<4, 4>(input, sel);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(outputs[0]) == 0);
        REQUIRE(sim.get_value(outputs[1]) == 0);
        REQUIRE(sim.get_value(outputs[2]) == 1010_b);
        REQUIRE(sim.get_value(outputs[3]) == 0);
    }
}

TEST_CASE("Combinational: parity generation", "[combinational][parity]") {
    auto ctx = std::make_unique<ch::core::context>("test_parity");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Odd parity with even number of 1s") {
        ch_uint<4> input(1100_b);
        ch_bool result = odd_parity_gen<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == true);
    }

    SECTION("Odd parity with odd number of 1s") {
        ch_uint<4> input(1101_b);
        ch_bool result = odd_parity_gen<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == false);
    }

    SECTION("Even parity with even number of 1s") {
        ch_uint<4> input(1100_b);
        ch_bool result = even_parity_gen<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == false);
    }

    SECTION("Even parity with odd number of 1s") {
        ch_uint<4> input(1101_b);
        ch_bool result = even_parity_gen<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == true);
    }
}

TEST_CASE("Combinational: full adder function", "[combinational][full_adder]") {
    auto ctx = std::make_unique<ch::core::context>("test_full_adder");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("0 + 0 + 0") {
        FullAdderResult result = full_adder(false, false, false);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == false);
        REQUIRE(sim.get_value(result.carry_out) == false);
    }

    SECTION("1 + 1 + 1") {
        FullAdderResult result = full_adder(true, true, true);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == true);
        REQUIRE(sim.get_value(result.carry_out) == true);
    }

    SECTION("1 + 0 + 1") {
        FullAdderResult result = full_adder(true, false, true);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == false);
        REQUIRE(sim.get_value(result.carry_out) == true);
    }
}

TEST_CASE("Combinational: ripple carry adder function",
          "[combinational][ripple_adder]") {
    auto ctx = std::make_unique<ch::core::context>("test_ripple_adder");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple addition without carry") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        auto result = ripple_carry_adder<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == 8);
        REQUIRE(sim.get_value(result.carry_out) == false);
    }

    SECTION("Addition with carry") {
        ch_uint<4> a(10_d);
        ch_uint<4> b(7_d);
        RippleCarryAdderResult<4> result = ripple_carry_adder<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == 1);
        REQUIRE(sim.get_value(result.carry_out) == true);
    }

    SECTION("Addition with initial carry") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        RippleCarryAdderResult<4> result = ripple_carry_adder<4>(a, b, true);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == 9);
        REQUIRE(sim.get_value(result.carry_out) == false);
    }
}

TEST_CASE("Combinational: comparator function", "[combinational][comparator]") {
    auto ctx = std::make_unique<ch::core::context>("test_comparator");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("A > B") {
        ch_uint<4> a(8_d);
        ch_uint<4> b(5_d);
        ComparatorResult<4> result = comparator<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.greater) == true);
        REQUIRE(sim.get_value(result.equal) == false);
        REQUIRE(sim.get_value(result.less) == false);
    }

    SECTION("A < B") {
        ch_uint<4> a(3_d);
        ch_uint<4> b(7_d);
        ComparatorResult<4> result = comparator<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.greater) == false);
        REQUIRE(sim.get_value(result.equal) == false);
        REQUIRE(sim.get_value(result.less) == true);
    }

    SECTION("A == B") {
        ch_uint<4> a(6_d);
        ch_uint<4> b(6_d);
        ComparatorResult<4> result = comparator<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.greater) == false);
        REQUIRE(sim.get_value(result.equal) == true);
        REQUIRE(sim.get_value(result.less) == false);
    }
}

TEST_CASE("Combinational: multiplexer function", "[combinational][mux]") {
    auto ctx = std::make_unique<ch::core::context>("test_mux");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("8-to-1 mux selection") {
        std::array<ch_uint<4>, 8> inputs = {1_d, 2_d, 3_d, 4_d,
                                            5_d, 6_d, 7_d, 8_d};
        ch_uint<3> sel(5_d);
        ch_uint<4> result = multiplexer<4, 8>(inputs, sel);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 6);
    }

    SECTION("16-to-1 mux selection") {
        ch_uint<4> result = mux16to1<4>(
            1_d, 2_d, 3_d, 4_d, 5_d, 6_d, 7_d, 8_d, 9_d, 10_d, 11_d, 12_d, 13_d,
            14_d, 15_d, 0_d, // Changed 16_d to 0_d to fit in 4 bits
            10_d);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 11);
    }
}

TEST_CASE("Combinational: equals and range check functions",
          "[combinational][comparison]") {
    auto ctx = std::make_unique<ch::core::context>("test_comparison");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Equals check") {
        ch_uint<4> input(7_d);
        ch_bool result = equals<4>(input, 7);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == true);
    }

    SECTION("Equals check - not equal") {
        ch_uint<4> input(7_d);
        ch_bool result = equals<4>(input, 5);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == false);
    }

    SECTION("Range check - in range") {
        ch_uint<4> input(7_d);
        ch_bool result = in_range<4>(input, 5, 10);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == true);
    }

    SECTION("Range check - out of range") {
        ch_uint<4> input(12_d);
        ch_bool result = in_range<4>(input, 5, 10);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == false);
    }
}

TEST_CASE("Combinational: leading zero detector",
          "[combinational][leading_zero]") {
    auto ctx = std::make_unique<ch::core::context>("test_leading_zero");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Leading zeros in 8-bit value") {
        ch_uint<8> input(0x12_h);
        ch_uint<4> result = leading_zero_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 3); // 3 leading zeros
    }

    SECTION("All zeros") {
        ch_uint<8> input(0x00_h);
        ch_uint<4> result = leading_zero_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 8); // 8 leading zeros
    }

    SECTION("No leading zeros") {
        ch_uint<8> input(0xFF_h);
        ch_uint<4> result = leading_zero_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0); // 0 leading zeros
    }

    SECTION("Single bit set at MSB") {
        ch_uint<8> input(0x80_h);
        ch_uint<4> result = leading_zero_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0); // 0 leading zeros
    }

    SECTION("Single bit set") {
        ch_uint<8> input(0x01_h);
        ch_uint<4> result = leading_zero_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 7); // 7 leading zeros
    }
}

TEST_CASE("Combinational: leading one detector",
          "[combinational][leading_one]") {
    auto ctx = std::make_unique<ch::core::context>("test_leading_one");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Leading ones in 8-bit value") {
        ch_uint<8> input(0xEA_h);
        ch_uint<4> result = leading_one_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 3); // 3 leading ones
    }

    SECTION("All ones") {
        ch_uint<8> input(0xFF_h);
        ch_uint<4> result = leading_one_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 8); // 8 leading ones
    }

    SECTION("No leading ones") {
        ch_uint<8> input(0x00_h);
        ch_uint<4> result = leading_one_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0); // 0 leading ones
    }

    SECTION("Single zero at MSB") {
        ch_uint<8> input(0x7F_h);
        ch_uint<4> result = leading_one_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0); // 0 leading ones
    }

    SECTION("Single one at LSB") {
        ch_uint<8> input(0x01_h);
        ch_uint<4> result = leading_one_detector<8>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0); // 0 leading ones
    }
}

TEST_CASE("Combinational: edge cases", "[combinational][edge]") {
    auto ctx = std::make_unique<ch::core::context>("test_edge");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Maximum values comparison") {
        ch_uint<4> a(15_d);
        ch_uint<4> b(15_d);
        ComparatorResult<4> result = comparator<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.greater) == false);
        REQUIRE(sim.get_value(result.equal) == true);
        REQUIRE(sim.get_value(result.less) == false);
    }

    SECTION("Zero values addition") {
        ch_uint<4> a(0_d);
        ch_uint<4> b(0_d);
        RippleCarryAdderResult<4> result = ripple_carry_adder<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == 0);
        REQUIRE(sim.get_value(result.carry_out) == false);
    }
}