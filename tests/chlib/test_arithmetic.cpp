#include "catch_amalgamated.hpp"
#include "chlib/arithmetic.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Arithmetic: basic add function", "[arithmetic][add]") {
    auto ctx = std::make_unique<ch::core::context>("test_arithmetic");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple addition") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        ch_uint<4> result = add<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 8);
    }

    SECTION("Addition with carry") {
        ch_uint<4> a(10_d);
        ch_uint<4> b(7_d);
        ch_uint<4> result = add<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 17 % 16); // 4-bit wraparound
    }
}

TEST_CASE("Arithmetic: add with carry function", "[arithmetic][add_carry]") {
    auto ctx = std::make_unique<ch::core::context>("test_add_carry");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Add with no carry in") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        ch_bool carry_in(false);

        AddWithCarryResult<4> result = add_with_carry<4>(a, b, carry_in);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) == 8);
        REQUIRE(sim.get_value(result.carry_out) == false);
    }

    //     SECTION("Add with carry in") {
    //         ch_uint<4> a(7_d);
    //         ch_uint<4> b(8_d);
    //         ch_bool carry_in(true);

    //         AddWithCarryResult<4> result = add_with_carry<4>(a, b, carry_in);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result.sum) ==
    //                 0); // 7+8+1=16, wraps to 0 in 4-bit
    //         REQUIRE(sim.get_value(result.carry_out) == true);
    //     }
    // }

    // TEST_CASE("Arithmetic: basic subtract function",
    // "[arithmetic][subtract]") {
    //     auto ctx = std::make_unique<ch::core::context>("test_subtract");
    //     ch::core::ctx_swap ctx_swapper(ctx.get());

    //     SECTION("Simple subtraction") {
    //         ch_uint<4> a(8_d);
    //         ch_uint<4> b(3_d);
    //         ch_uint<4> result = subtract<4>(a, b);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result) == 5);
    //     }

    //     SECTION("Subtraction with borrow") {
    //         ch_uint<4> a(3_d);
    //         ch_uint<4> b(5_d);
    //         ch_uint<4> result = subtract<4>(a, b);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result) == 14); // 3-5 = -2, wraps to 14 in
    //         4-bit
    //     }
    // }

    // TEST_CASE("Arithmetic: sub with borrow function",
    // "[arithmetic][sub_borrow]") {
    //     auto ctx = std::make_unique<ch::core::context>("test_sub_borrow");
    //     ch::core::ctx_swap ctx_swapper(ctx.get());

    //     SECTION("Subtract with no borrow") {
    //         ch_uint<4> a(8_d);
    //         ch_uint<4> b(3_d);
    //         ch_bool borrow_in(false);

    //         SubWithBorrowResult<4> result = sub_with_borrow<4>(a, b,
    //         borrow_in);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result.diff) == 5);
    //         REQUIRE(sim.get_value(result.borrow_out) == false);
    //     }

    //     SECTION("Subtract with borrow") {
    //         ch_uint<4> a(3_d);
    //         ch_uint<4> b(5_d);
    //         ch_bool borrow_in(true);

    //         SubWithBorrowResult<4> result = sub_with_borrow<4>(a, b,
    //         borrow_in);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result.diff) ==
    //                 13); // 3-5-1 = -3, wraps to 13 in 4-bit
    //         REQUIRE(sim.get_value(result.borrow_out) == true);
    //     }
    // }

    // TEST_CASE("Arithmetic: multiply function", "[arithmetic][multiply]") {
    //     auto ctx = std::make_unique<ch::core::context>("test_multiply");
    //     ch::core::ctx_swap ctx_swapper(ctx.get());

    //     SECTION("Simple multiplication") {
    //         ch_uint<4> a(5_d);
    //         ch_uint<4> b(3_d);
    //         ch_uint<8> result = multiply<4>(a, b);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result) == 15);
    //     }

    //     SECTION("Larger multiplication") {
    //         ch_uint<4> a(10_d);
    //         ch_uint<4> b(12_d);
    //         ch_uint<8> result = multiply<4>(a, b);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result) == 120);
    //     }
    // }

    // TEST_CASE("Arithmetic: compare function", "[arithmetic][compare]") {
    //     auto ctx = std::make_unique<ch::core::context>("test_compare");
    //     ch::core::ctx_swap ctx_swapper(ctx.get());

    //     SECTION("Equal comparison") {
    //         ch_uint<4> a(5_d);
    //         ch_uint<4> b(5_d);
    //         ComparisonResult<4> result = compare<4>(a, b);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result.equal) == true);
    //         REQUIRE(sim.get_value(result.not_equal) == false);
    //         REQUIRE(sim.get_value(result.greater) == false);
    //         REQUIRE(sim.get_value(result.less) == false);
    //     }

    //     SECTION("Greater than comparison") {
    //         ch_uint<4> a(7_d);
    //         ch_uint<4> b(5_d);
    //         ComparisonResult<4> result = compare<4>(a, b);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result.equal) == false);
    //         REQUIRE(sim.get_value(result.greater) == true);
    //         REQUIRE(sim.get_value(result.less) == false);
    //     }

    //     SECTION("Less than comparison") {
    //         ch_uint<4> a(3_d);
    //         ch_uint<4> b(8_d);
    //         ComparisonResult<4> result = compare<4>(a, b);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result.equal) == false);
    //         REQUIRE(sim.get_value(result.greater) == false);
    //         REQUIRE(sim.get_value(result.less) == true);
    //     }
    // }

    // TEST_CASE("Arithmetic: abs function", "[arithmetic][abs]") {
    //     auto ctx = std::make_unique<ch::core::context>("test_abs");
    //     ch::core::ctx_swap ctx_swapper(ctx.get());

    //     SECTION("Positive number") {
    //         ch_uint<4> a(5_d); // 0101 in binary
    //         ch_uint<4> result = abs<4>(a);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result) == 5);
    //     }

    //     SECTION("Negative number (2's complement)") {
    //         // For 4-bit, 1011 represents -5 in 2's complement
    //         ch_uint<4> a(1011_b); // -5 in 2's complement
    //         ch_uint<4> result = abs<4>(a);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result) == 5);
    //     }
    // }

    // TEST_CASE("Arithmetic: shift functions", "[arithmetic][shift]") {
    //     auto ctx = std::make_unique<ch::core::context>("test_shift");
    //     ch::core::ctx_swap ctx_swapper(ctx.get());

    //     SECTION("Left shift") {
    //         ch_uint<8> a(00000101_b); // 5
    //         ch_uint<8> result = left_shift<8>(a, 2);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result) == 00010100_b); // 20
    //     }

    //     SECTION("Logical right shift") {
    //         ch_uint<8> a(01010000_b); // 80
    //         ch_uint<8> result = logical_right_shift<8>(a, 3);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result) == 00001010_b); // 10
    //     }

    //     SECTION("Arithmetic right shift with positive number") {
    //         ch_uint<8> a(01010000_b); // 80 (positive)
    //         ch_uint<8> result = arithmetic_right_shift<8>(a, 2);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(result) == 00010100_b); // 20
    //     }

    //     SECTION("Arithmetic right shift with negative number") {
    //         // 11111011 is -5 in 8-bit 2's complement
    //         ch_uint<8> a(11111011_b); // -5 in 2's complement
    //         ch_uint<8> result = arithmetic_right_shift<8>(a, 1);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         // Should preserve sign bit: 11111101 which is -3 in 2's
    //         complement REQUIRE(sim.get_value(result) == 11111101_b); // -3
    //     }
    // }

    // TEST_CASE("Arithmetic: edge cases", "[arithmetic][edge]") {
    //     auto ctx = std::make_unique<ch::core::context>("test_edge");
    //     ch::core::ctx_swap ctx_swapper(ctx.get());

    //     SECTION("Maximum values addition") {
    //         ch_uint<4> a(15_d); // 0xF
    //         ch_uint<4> b(15_d); // 0xF
    //         ch_uint<4> result = add<4>(a, b);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         // 15 + 15 = 30, but in 4-bit it wraps to 14 (30 % 16)
    //         REQUIRE(sim.get_value(result) == 14);
    //     }

    //     SECTION("Zero operations") {
    //         ch_uint<4> a(0_d);
    //         ch_uint<4> b(5_d);

    //         ch_uint<4> add_result = add<4>(a, b);
    //         ch_uint<4> sub_result = subtract<4>(b, a);

    //         ch::Simulator sim(ctx.get());
    //         sim.tick();

    //         REQUIRE(sim.get_value(add_result) == 5);
    //         REQUIRE(sim.get_value(sub_result) == 5);
    //     }
}