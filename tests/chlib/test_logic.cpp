#include "catch_amalgamated.hpp"
#include "chlib/logic.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <array>
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Logic: basic AND gate function", "[logic][and]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_and");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple AND operation") {
        ch_uint<4> a(12_d);
        ch_uint<4> b(10_d);
        ch_uint<4> result = and_gate<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 8_d);
    }

    SECTION("AND with zero") {
        ch_uint<4> a(15_d);
        ch_uint<4> b(0_d);
        ch_uint<4> result = and_gate<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0);
    }
}

TEST_CASE("Logic: basic OR gate function", "[logic][or]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_or");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple OR operation") {
        ch_uint<4> a(12_d);
        ch_uint<4> b(10_d);
        ch_uint<4> result = or_gate<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 14_d);
    }

    SECTION("OR with zero") {
        ch_uint<4> a(15_d);
        ch_uint<4> b(0_d);
        ch_uint<4> result = or_gate<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 15_d);
    }
}

TEST_CASE("Logic: basic NOT gate function", "[logic][not]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_not");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple NOT operation") {
        ch_uint<4> a(12_d);
        ch_uint<4> result = not_gate<4>(a);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 3_d); // NOT 1100 (12) = 0011 (3)
    }
}

TEST_CASE("Logic: basic XOR gate function", "[logic][xor]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_xor");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple XOR operation") {
        ch_uint<4> a(12_d);
        ch_uint<4> b(10_d);
        ch_uint<4> result = xor_gate<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) ==
                6_d); // XOR 1100 (12) and 1010 (10) = 0110 (6)
    }
}

TEST_CASE("Logic: NAND gate function", "[logic][nand]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_nand");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple NAND operation") {
        ch_uint<4> a(12_d);
        ch_uint<4> b(10_d);
        ch_uint<4> result = nand_gate<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 7_d); // NOT of AND result
    }
}

TEST_CASE("Logic: NOR gate function", "[logic][nor]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_nor");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple NOR operation") {
        ch_uint<4> a(12_d);
        ch_uint<4> b(10_d);
        ch_uint<4> result = nor_gate<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 1_d); // NOT of OR result
    }
}

TEST_CASE("Logic: XNOR gate function", "[logic][xnor]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_xnor");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple XNOR operation") {
        ch_uint<4> a(12_d);
        ch_uint<4> b(10_d);
        ch_uint<4> result = xnor_gate<4>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 9_d); // NOT of XOR result
    }
}

TEST_CASE("Logic: multi-input gates", "[logic][multi]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_multi");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Multi-AND operation") {
        ch_uint<4> a(15_d);
        ch_uint<4> b(13_d);
        ch_uint<4> c(11_d);
        ch_uint<4> result = multi_and_gate<4>({a, b, c});

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 9_d);
    }

    SECTION("Multi-OR operation") {
        ch_uint<4> a(12_d);
        ch_uint<4> b(6_d);
        ch_uint<4> c(3_d);
        ch_uint<4> result = multi_or_gate<4>({a, b, c});

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 15_d);
    }

    SECTION("Multi-XOR operation") {
        ch_uint<4> a(12_d);
        ch_uint<4> b(6_d);
        ch_uint<4> c(3_d);
        ch_uint<4> result = multi_xor_gate<4>({a, b, c});

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 9_d);
    }
}

TEST_CASE("Logic: 2-input multiplexer", "[logic][mux2]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_mux2");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Select input 0") {
        ch_uint<4> in0(12_d);
        ch_uint<4> in1(3_d);
        ch_bool sel(false);
        ch_uint<4> result = mux2<4>(in0, in1, sel);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 12_d);
    }

    SECTION("Select input 1") {
        ch_uint<4> in0(12_d);
        ch_uint<4> in1(3_d);
        ch_bool sel(true);
        ch_uint<4> result = mux2<4>(in0, in1, sel);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 3_d);
    }
}

TEST_CASE("Logic: 4-input multiplexer", "[logic][mux4]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_mux4");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Select input 0") {
        ch_uint<4> in0(12_d);
        ch_uint<4> in1(3_d);
        ch_uint<4> in2(10_d);
        ch_uint<4> in3(5_d);
        ch_uint<2> sel(0_d);
        ch_uint<4> result = mux4<4>(in0, in1, in2, in3, sel);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 12_d);
    }

    SECTION("Select input 3") {
        ch_uint<4> in0(12_d);
        ch_uint<4> in1(3_d);
        ch_uint<4> in2(10_d);
        ch_uint<4> in3(5_d);
        ch_uint<2> sel(3_d);
        ch_uint<4> result = mux4<4>(in0, in1, in2, in3, sel);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 5_d);
    }
}

TEST_CASE("Logic: parity generation", "[logic][parity]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_parity");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Even parity input") {
        ch_uint<4> input(12_d); // 2 ones, even parity
        ch_bool parity = parity_gen<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(parity) == false);
    }

    SECTION("Odd parity input") {
        ch_uint<4> input(13_d); // 3 ones, odd parity
        ch_bool parity = parity_gen<4>(input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(parity) == true);
    }
}

TEST_CASE("Logic: parity check", "[logic][parity_check]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_parity_check");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Correct parity") {
        ch_uint<4> input(12_d);          // 2 ones, even parity
        ch_bool expected_parity = false; // expecting even parity
        ch_bool result = parity_check<4>(input, expected_parity);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == true);
    }

    SECTION("Incorrect parity") {
        ch_uint<4> input(13_d);          // 3 ones, odd parity
        ch_bool expected_parity = false; // expecting even parity
        ch_bool result = parity_check<4>(input, expected_parity);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == false);
    }
}

TEST_CASE("Logic: tri-state buffer", "[logic][tristate]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_tristate");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Buffer enabled") {
        ch_uint<4> input(10_d);
        ch_bool enable(true);
        ch_uint<4> result = tri_state_buffer<4>(input, enable);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 10_d);
    }

    SECTION("Buffer disabled") {
        ch_uint<4> input(10_d);
        ch_bool enable(false);
        ch_uint<4> result = tri_state_buffer<4>(input, enable);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result) == 0_d);
    }
}

TEST_CASE("Logic: edge cases", "[logic][edge]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_edge");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Single bit operations") {
        ch_uint<1> a(1_d);
        ch_uint<1> b(0_d);
        auto and_result = and_gate<1>(a, b);
        auto or_result = or_gate<1>(a, b);
        auto xor_result = xor_gate<1>(a, b);
        auto nand_result = nand_gate<1>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(and_result) == 0);
        REQUIRE(sim.get_value(or_result) == 1);
        REQUIRE(sim.get_value(xor_result) == 1);
        REQUIRE(sim.get_value(nand_result) == 1);
    }

    SECTION("All ones input") {
        ch_uint<8> a(0xFF_h);
        ch_uint<8> b(0xFF_h);

        ch_uint<8> result_and = and_gate<8>(a, b);
        ch_uint<8> result_or = or_gate<8>(a, b);
        ch_uint<8> result_xor = xor_gate<8>(a, b);
        ch_uint<8> result_nand = nand_gate<8>(a, b);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result_and) == 0xFF);
        REQUIRE(sim.get_value(result_or) == 0xFF);
        REQUIRE(sim.get_value(result_xor) == 0x00);
        REQUIRE(sim.get_value(result_nand) == 0x00);
    }
}