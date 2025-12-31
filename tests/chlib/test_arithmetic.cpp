#include "catch_amalgamated.hpp"
#include "chlib/arithmetic.h"
#include "chlib/switch.h"
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

    SECTION("Add with carry in") {
        ch_uint<4> a(7_d);
        ch_uint<4> b(8_d);
        ch_bool carry_in(true);

        AddWithCarryResult<4> result = add_with_carry<4>(a, b, carry_in);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.sum) ==
                0); // 7+8+1=16, wraps to 0 in 4-bit
        REQUIRE(sim.get_value(result.carry_out) == true);
    }

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

TEST_CASE("Arithmetic: min function", "[arithmetic][min]") {
    auto ctx = std::make_unique<ch::core::context>("test_min");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic min operation") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        ch_uint<4> result = min<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 3);
    }

    SECTION("Min when first value is smaller") {
        ch_uint<4> a(2_d);
        ch_uint<4> b(7_d);
        ch_uint<4> result = min<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 2);
    }

    SECTION("Min with equal values") {
        ch_uint<4> a(6_d);
        ch_uint<4> b(6_d);
        ch_uint<4> result = min<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 6);
    }

    SECTION("Min with multiple values (3 args)") {
        ch_uint<4> a(8_d);
        ch_uint<4> b(3_d);
        ch_uint<4> c(5_d);
        ch_uint<4> result = min<4>(a, b, c);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 3);
    }

    SECTION("Min with multiple values (4 args)") {
        ch_uint<4> a(10_d);
        ch_uint<4> b(15_d);
        ch_uint<4> c(2_d);
        ch_uint<4> d(7_d);
        ch_uint<4> result = min<4>(a, b, c, d);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 2);
    }
}

TEST_CASE("Arithmetic: max function", "[arithmetic][max]") {
    auto ctx = std::make_unique<ch::core::context>("test_max");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic max operation") {
        ch_uint<4> a(5_d);
        ch_uint<4> b(3_d);
        ch_uint<4> result = max<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 5);
    }

    SECTION("Max when second value is smaller") {
        ch_uint<4> a(7_d);
        ch_uint<4> b(2_d);
        ch_uint<4> result = max<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 7);
    }

    SECTION("Max with equal values") {
        ch_uint<4> a(6_d);
        ch_uint<4> b(6_d);
        ch_uint<4> result = max<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 6);
    }

    SECTION("Max with multiple values (3 args)") {
        ch_uint<4> a(8_d);
        ch_uint<4> b(3_d);
        ch_uint<4> c(12_d);
        ch_uint<4> result = max<4>(a, b, c);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 12);
    }

    SECTION("Max with multiple values (4 args)") {
        ch_uint<4> a(10_d);
        ch_uint<4> b(15_d);
        ch_uint<4> c(2_d);
        ch_uint<4> d(7_d);
        ch_uint<4> result = max<4>(a, b, c, d);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 15);
    }
}

TEST_CASE("Arithmetic: switch function", "[arithmetic][switch]") {
    auto ctx = std::make_unique<ch::core::context>("test_switch");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Switch with one case") {
        ch_uint<4> input(2_d);
        ch_uint<4> result =
            switch_case(input, 2_d, 10_d,
                        0_d); // case 2: return 10, default: return 0

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 10);
    }

    SECTION("Switch with one case - default path") {
        ch_uint<4> input(3_d);
        ch_uint<4> result =
            switch_case(input, 2_d, 10_d,
                        0_d); // case 2: return 10, default: return 0

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }

    SECTION("Switch with multiple cases - first case matches") {
        ch_uint<4> input(0_d);
        ch_uint<4> result = switch_case(input, 0_d, 10_d, 1_d, 12_d, 2_d, 13_d,
                                        0_d); // 3 cases + default

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 10);
    }

    SECTION("Switch with multiple cases - middle case matches") {
        ch_uint<4> input(1_d);
        ch_uint<4> result =
            switch_case(input, 0_d, 10_d, 1_d, 12_d, 2_d, 13_d, 0_d);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 12);
    }

    SECTION("Switch with multiple cases - last case matches") {
        ch_uint<4> input(2_d);
        ch_uint<4> result =
            switch_case(input, 0_d, 10_d, 1_d, 12_d, 2_d, 13_d, 0_d);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 13);
    }

    SECTION("Switch with multiple cases - default path") {
        ch_uint<4> input(5_d);
        ch_uint<4> result =
            switch_case(input, 0_d, 10_d, 1_d, 12_d, 2_d, 13_d, 0_d);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }
}

// TEST_CASE("Arithmetic: switch_pairs function", "[arithmetic][switch]") {
//     auto ctx = std::make_unique<ch::core::context>("test_switch_pairs");
//     ch::core::ctx_swap ctx_swapper(ctx.get());

//     SECTION("Switch with pairs - one case") {
//         ch_uint<4> input(1_d);
//         ch_uint<4> result = switch_pairs(input, case_value(1_d, 14_d), 0_d);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 14);
//     }

//     SECTION("Switch with pairs - multiple cases") {
//         ch_uint<4> input(2_d);
//         ch_uint<4> result =
//             switch_pairs(input, case_value(0_d, 10_d), case_value(1_d, 12_d),
//                          case_value(2_d, 13_d), 0_d);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 13);
//     }

//     SECTION("Switch with pairs - default path") {
//         ch_uint<4> input(3_d);
//         ch_uint<4> result = switch_pairs<4>(input, case_value<4>(0_d, 10_d),
//                                             case_value<4>(1_d, 12_d), 0_d);

//         ch::Simulator sim(ctx.get());
//         sim.tick();

//         REQUIRE(sim.get_value(result) == 0);
//     }
// }

TEST_CASE("Arithmetic: switch_ function", "[arithmetic][switch_recursive]") {
    auto ctx = std::make_unique<ch::core::context>("test_switch_recursive");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Switch recursive with one case") {
        ch_uint<4> input(2_d);
        ch_uint<4> result = switch_(input, 0_d, case_(2_d, 10_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 10);
    }

    SECTION("Switch recursive with one case - default path") {
        ch_uint<4> input(3_d);
        ch_uint<4> result = switch_(input, 0_d, case_(2_d, 10_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }

    SECTION("Switch recursive with multiple cases - first case matches") {
        ch_uint<4> input(0_d);
        ch_uint<4> result = switch_(input, 0_d, case_(0_d, 10_d),
                                    case_(1_d, 12_d), case_(2_d, 13_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 10);
    }

    SECTION("Switch recursive with multiple cases - middle case matches") {
        ch_uint<4> input(1_d);
        ch_uint<4> result = switch_(input, 0_d, case_(0_d, 10_d),
                                    case_(1_d, 12_d), case_(2_d, 13_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 12);
    }

    SECTION("Switch recursive with multiple cases - last case matches") {
        ch_uint<4> input(2_d);
        ch_uint<4> result = switch_(input, 0_d, case_(0_d, 10_d),
                                    case_(1_d, 12_d), case_(2_d, 13_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 13);
    }

    SECTION("Switch recursive with multiple cases - default path") {
        ch_uint<4> input(5_d);
        ch_uint<4> result = switch_(input, 0_d, case_(0_d, 10_d),
                                    case_(1_d, 12_d), case_(2_d, 13_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }
}

TEST_CASE("Arithmetic: switch_parallel function",
          "[arithmetic][switch_parallel]") {
    auto ctx = std::make_unique<ch::core::context>("test_switch_parallel");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Switch parallel with one case") {
        ch_uint<4> input(2_d);
        ch_uint<4> result = switch_parallel(input, 0_d, case_(2_d, 10_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 10);
    }

    SECTION("Switch parallel with one case - default path") {
        ch_uint<4> input(3_d);
        ch_uint<4> result = switch_parallel(input, 0_d, case_(2_d, 10_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }

    SECTION("Switch parallel with multiple cases - first case matches") {
        ch_uint<4> input(0_d);
        ch_uint<4> result = switch_parallel(input, 0_d, case_(0_d, 10_d),
                                            case_(1_d, 12_d), case_(2_d, 13_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 10);
    }

    SECTION("Switch parallel with multiple cases - middle case matches") {
        ch_uint<4> input(1_d);
        ch_uint<4> result = switch_parallel(input, 0_d, case_(0_d, 10_d),
                                            case_(1_d, 12_d), case_(2_d, 13_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 12);
    }

    SECTION("Switch parallel with multiple cases - last case matches") {
        ch_uint<4> input(2_d);
        ch_uint<4> result = switch_parallel(input, 0_d, case_(0_d, 10_d),
                                            case_(1_d, 12_d), case_(2_d, 13_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 13);
    }

    SECTION("Switch parallel with multiple cases - default path") {
        ch_uint<4> input(5_d);
        ch_uint<4> result = switch_parallel(input, 0_d, case_(0_d, 10_d),
                                            case_(1_d, 12_d), case_(2_d, 13_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }
}

TEST_CASE("Arithmetic: switch performance comparison",
          "[arithmetic][switch_performance]") {
    auto ctx = std::make_unique<ch::core::context>("test_switch_performance");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Compare all switch implementations return same result") {
        ch_uint<5> input(1_d);

        // Using switch_case (direct values)
        ch_uint<5> result1 =
            switch_case(input, 0_d, 0_d, 1_d, 10_d, 2_d, 20_d, 9_d);

        // Using switch_pairs (with case_value)
        // ch_uint<4> result2 =
        //     switch_pairs(input, case_value(0_d, 0_d), case_value(1_d, 10_d),
        //                  case_value(2_d, 20_d), 99_d);

        // Using switch_ (with case_ entries)
        // ch_uint<5> result3 = switch_(input, (9_d, case_(0_d, 0_d),
        //                              case_(1_d, 10_d), case_(2_d, 20_d));

        // Using switch_parallel (with case_ entries)
        ch_uint<5> result4 = switch_parallel(
            input, 9_d, case_(0_d, 0_d), case_(1_d, 10_d), case_(2_d, 20_d));

        ch::Simulator sim(ctx.get());
        sim.tick();

        // All implementations should return the same result
        REQUIRE(sim.get_value(result1) == 10);
        // REQUIRE(sim.get_value(result2) == 10);
        // REQUIRE(sim.get_value(result3) == 10);
        REQUIRE(sim.get_value(result4) == 10);
    }
}
