#include "catch_amalgamated.hpp"
#include "chlib/arithmetic_advance.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include "utils/format_utils.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Arithmetic Advance: carry lookahead adder",
          "[arithmetic_advance][cla]") {
    auto ctx = std::make_unique<ch::core::context>("test_cla");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple addition without carry") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        CLAResult<4> result = carry_lookahead_adder<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        auto a_val = sim.get_value(a);
        auto b_val = sim.get_value(b);
        auto sum_val = sim.get_value(result.sum);
        auto carry_val = sim.get_value(result.carry_out);

        std::cout << "Input: a=0b" << to_binary_string(a_val, 4) << ", b=0b"
                  << to_binary_string(b_val, 4) << std::endl;
        std::cout << "Output: sum=0b" << to_binary_string(sum_val, 4)
                  << ", carray=" << carry_val << std::endl;

        REQUIRE(sum_val == 8);
        REQUIRE(carry_val == false);
    }

    SECTION("Addition with carry") {
        ch_uint<4> a(10_d);
        ch_uint<4> b(7_d);
        CLAResult<4> result = carry_lookahead_adder<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == 1);
        REQUIRE(sim.get_value(result.carry_out) == true);
    }

    SECTION("Addition with initial carry") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        CLAResult<4> result = carry_lookahead_adder<4>(a, b, true);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == 9);
        REQUIRE(sim.get_value(result.carry_out) == false);
    }
}

TEST_CASE("Arithmetic Advance: carry save adder", "[arithmetic_advance][csa]") {
    auto ctx = std::make_unique<ch::core::context>("test_csa");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Three operand addition") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        ch_uint<4> c(2_d);
        CSAResult<4> result = carry_save_adder<4>(a, b, c);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Expected: 5 + 3 + 2 = 10 (0b1010)
        // Sum: XOR of all inputs bit by bit
        // Carry: AND of any two inputs, shifted left
        REQUIRE(sim.get_value(result.sum) == (5 ^ 3 ^ 2)); // 5^3^2 = 4
        REQUIRE(sim.get_value(result.carry) == ((5 & 3) | (3 & 2) | (5 & 2))
                                                   << 1); // Carry bits shifted
    }
}

TEST_CASE("Arithmetic Advance: wallace tree multiplier",
          "[arithmetic_advance][wallace]") {
    auto ctx = std::make_unique<ch::core::context>("test_wallace");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple multiplication") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        ch_uint<8> result = wallace_tree_multiplier<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 15); // 5 * 3 = 15
    }

    SECTION("Multiplication by zero") {
        ch_uint<4> a(7_d);
        ch_uint<4> b(0_d);
        ch_uint<8> result = wallace_tree_multiplier<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0); // 7 * 0 = 0
    }

    SECTION("Multiplication by one") {
        ch_uint<4> a(9_d);
        ch_uint<4> b(1_d);
        ch_uint<8> result = wallace_tree_multiplier<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 9); // 9 * 1 = 9
    }
}

TEST_CASE("Arithmetic Advance: booth multiplier",
          "[arithmetic_advance][booth]") {
    auto ctx = std::make_unique<ch::core::context>("test_booth");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple multiplication") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        ch_uint<8> result = booth_multiplier<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 15); // 5 * 3 = 15
    }

    SECTION("Multiplication by zero") {
        ch_uint<4> a(7_d);
        ch_uint<4> b(0_d);
        ch_uint<8> result = booth_multiplier<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0); // 7 * 0 = 0
    }
}

TEST_CASE("Arithmetic Advance: non-restoring divider",
          "[arithmetic_advance][divider]") {
    auto ctx = std::make_unique<ch::core::context>("test_divider");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple division") {
        ch_uint<4> dividend(12_d);
        ch_uint<4> divisor(3_d);
        DividerResult<4> result = non_restoring_divider<4>(dividend, divisor);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.quotient) == 4);  // 12 / 3 = 4
        REQUIRE(sim.get_value(result.remainder) == 0); // 12 % 3 = 0
    }

    SECTION("Division with remainder") {
        ch_uint<4> dividend(13_d);
        ch_uint<4> divisor(3_d);
        DividerResult<4> result = non_restoring_divider<4>(dividend, divisor);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.quotient) == 4);  // 13 / 3 = 4
        REQUIRE(sim.get_value(result.remainder) == 1); // 13 % 3 = 1
    }

    SECTION("Division by zero") {
        ch_uint<4> dividend(10_d);
        ch_uint<4> divisor(0_d);
        DividerResult<4> result = non_restoring_divider<4>(dividend, divisor);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.quotient) == 15); // Max value for 4-bit
        REQUIRE(sim.get_value(result.remainder) == 0); // Remainder is 0
    }
}

// TEST_CASE("Arithmetic Advance: square root calculator",
//           "[arithmetic_advance][sqrt]") {
//     auto ctx = std::make_unique<ch::core::context>("test_sqrt");
//     ch::core::ctx_swap ctx_swapper(ctx.get());

//     SECTION("Perfect square") {
//         ch_uint<8> input(16_d);
//         ch_uint<4> result = square_root_calculator<8>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 4); // sqrt(16) = 4
//     }

//     SECTION("Non-perfect square") {
//         ch_uint<8> input(10_d);
//         ch_uint<4> result = square_root_calculator<8>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 3); // floor(sqrt(10)) = 3
//     }

//     SECTION("Square root of 0") {
//         ch_uint<8> input(0_d);
//         ch_uint<4> result = square_root_calculator<8>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 0); // sqrt(0) = 0
//     }

//     SECTION("Square root of 1") {
//         ch_uint<8> input(1_d);
//         ch_uint<4> result = square_root_calculator<8>(input);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 1); // sqrt(1) = 1
//     }
// }

TEST_CASE("Arithmetic Advance: fixed point arithmetic",
          "[arithmetic_advance][fixed_point]") {
    auto ctx = std::make_unique<ch::core::context>("test_fixed_point");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Fixed point addition (Q8.4 format)") {
        ch_uint<8> a(00010000_b); // 1.0 in Q4.4 format
        ch_uint<8> b(00001000_b); // 0.5 in Q4.4 format
        FixedPointResult<8, 4> result = fixed_point_adder<8, 4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 1.0 + 0.5 = 1.5, which is 0b00011000 (1.5 * 2^4 = 24)
        REQUIRE(sim.get_value(result.result) == 24);
    }

    SECTION("Fixed point multiplication (Q8.4 format)") {
        ch_uint<8> a(00010000_b); // 1.0 in Q4.4 format
        ch_uint<8> b(00001000_b); // 0.5 in Q4.4 format
        FixedPointResult<8, 4> result = fixed_point_multiplier<8, 4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 1.0 * 0.5 = 0.5, which is 0b00001000 (0.5 * 2^4 = 8)
        REQUIRE(sim.get_value(result.result) == 8);
    }

    SECTION("Fixed point division (Q8.4 format)") {
        ch_uint<8> a(00010000_b); // 1.0 in Q4.4 format
        ch_uint<8> b(00001000_b); // 0.5 in Q4.4 format
        FixedPointResult<8, 4> result = fixed_point_divider<8, 4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 1.0 / 0.5 = 2.0, which is 0b00100000 (2.0 * 2^4 = 32)
        REQUIRE(sim.get_value(result.result) == 32);
    }
}