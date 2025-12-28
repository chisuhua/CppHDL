#include "catch_amalgamated.hpp"
#include "chlib/logic.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>
#include <array>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Logic: basic AND gate function", "[logic][and]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_and");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple AND operation") {
        ch_uint<4> a(0b1100_d);
        ch_uint<4> b(0b1010_d);
        ch_uint<4> result = and_gate<4>(a, b);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1000_d);
    }
    
    SECTION("AND with zero") {
        ch_uint<4> a(0b1111_d);
        ch_uint<4> b(0b0000_d);
        ch_uint<4> result = and_gate<4>(a, b);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0);
    }
}

TEST_CASE("Logic: basic OR gate function", "[logic][or]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_or");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple OR operation") {
        ch_uint<4> a(0b1100_d);
        ch_uint<4> b(0b1010_d);
        ch_uint<4> result = or_gate<4>(a, b);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1110_d);
    }
    
    SECTION("OR with zero") {
        ch_uint<4> a(0b1111_d);
        ch_uint<4> b(0b0000_d);
        ch_uint<4> result = or_gate<4>(a, b);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1111_d);
    }
}

TEST_CASE("Logic: basic NOT gate function", "[logic][not]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_not");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple NOT operation") {
        ch_uint<4> a(0b1100_d);
        ch_uint<4> result = not_gate<4>(a);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b0011_d);
    }
}

TEST_CASE("Logic: basic XOR gate function", "[logic][xor]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_xor");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple XOR operation") {
        ch_uint<4> a(0b1100_d);
        ch_uint<4> b(0b1010_d);
        ch_uint<4> result = xor_gate<4>(a, b);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b0110_d);
    }
}

TEST_CASE("Logic: NAND gate function", "[logic][nand]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_nand");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple NAND operation") {
        ch_uint<4> a(0b1100_d);
        ch_uint<4> b(0b1010_d);
        ch_uint<4> result = nand_gate<4>(a, b);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b0111_d);  // NOT of AND result
    }
}

TEST_CASE("Logic: NOR gate function", "[logic][nor]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_nor");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple NOR operation") {
        ch_uint<4> a(0b1100_d);
        ch_uint<4> b(0b1010_d);
        ch_uint<4> result = nor_gate<4>(a, b);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b0001_d);  // NOT of OR result
    }
}

TEST_CASE("Logic: XNOR gate function", "[logic][xnor]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_xnor");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Simple XNOR operation") {
        ch_uint<4> a(0b1100_d);
        ch_uint<4> b(0b1010_d);
        ch_uint<4> result = xnor_gate<4>(a, b);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1001_d);  // NOT of XOR result
    }
}

TEST_CASE("Logic: multi-input gates", "[logic][multi]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_multi");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Multi-AND operation") {
        ch_uint<4> a(0b1111_d);
        ch_uint<4> b(0b1101_d);
        ch_uint<4> c(0b1011_d);
        ch_uint<4> result = multi_and_gate<4>({a, b, c});
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1001_d);
    }
    
    SECTION("Multi-OR operation") {
        ch_uint<4> a(0b1100_d);
        ch_uint<4> b(0b0110_d);
        ch_uint<4> c(0b0011_d);
        ch_uint<4> result = multi_or_gate<4>({a, b, c});
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1111_d);
    }
    
    SECTION("Multi-XOR operation") {
        ch_uint<4> a(0b1100_d);
        ch_uint<4> b(0b0110_d);
        ch_uint<4> c(0b0011_d);
        ch_uint<4> result = multi_xor_gate<4>({a, b, c});
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1001_d);
    }
}

TEST_CASE("Logic: 2-input multiplexer", "[logic][mux2]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_mux2");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Select input 0") {
        ch_uint<4> in0(0b1100_d);
        ch_uint<4> in1(0b0011_d);
        ch_bool sel(false);
        ch_uint<4> result = mux2<4>(in0, in1, sel);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1100_d);
    }
    
    SECTION("Select input 1") {
        ch_uint<4> in0(0b1100_d);
        ch_uint<4> in1(0b0011_d);
        ch_bool sel(true);
        ch_uint<4> result = mux2<4>(in0, in1, sel);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b0011_d);
    }
}

TEST_CASE("Logic: 4-input multiplexer", "[logic][mux4]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_mux4");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Select input 0") {
        ch_uint<4> in0(0b1100_d);
        ch_uint<4> in1(0b0011_d);
        ch_uint<4> in2(0b1010_d);
        ch_uint<4> in3(0b0101_d);
        ch_uint<2> sel(0b00_d);
        ch_uint<4> result = mux4<4>(in0, in1, in2, in3, sel);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1100_d);
    }
    
    SECTION("Select input 3") {
        ch_uint<4> in0(0b1100_d);
        ch_uint<4> in1(0b0011_d);
        ch_uint<4> in2(0b1010_d);
        ch_uint<4> in3(0b0101_d);
        ch_uint<2> sel(0b11_d);
        ch_uint<4> result = mux4<4>(in0, in1, in2, in3, sel);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b0101_d);
    }
}

TEST_CASE("Logic: parity generation", "[logic][parity]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_parity");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Even parity input") {
        ch_uint<4> input(0b1100_d);  // 2 ones, even parity
        ch_bool parity = parity_gen<4>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(parity) == false);
    }
    
    SECTION("Odd parity input") {
        ch_uint<4> input(0b1101_d);  // 3 ones, odd parity
        ch_bool parity = parity_gen<4>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(parity) == true);
    }
}

TEST_CASE("Logic: parity check", "[logic][parity_check]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_parity_check");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Correct parity") {
        ch_uint<4> input(0b1100_d);  // 2 ones, even parity
        ch_bool expected_parity = false;  // expecting even parity
        ch_bool result = parity_check<4>(input, expected_parity);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == true);
    }
    
    SECTION("Incorrect parity") {
        ch_uint<4> input(0b1101_d);  // 3 ones, odd parity
        ch_bool expected_parity = false;  // expecting even parity
        ch_bool result = parity_check<4>(input, expected_parity);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == false);
    }
}

TEST_CASE("Logic: tri-state buffer", "[logic][tristate]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_tristate");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Buffer enabled") {
        ch_uint<4> input(0b1010_d);
        ch_bool enable(true);
        ch_uint<4> result = tri_state_buffer<4>(input, enable);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b1010_d);
    }
    
    SECTION("Buffer disabled") {
        ch_uint<4> input(0b1010_d);
        ch_bool enable(false);
        ch_uint<4> result = tri_state_buffer<4>(input, enable);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result) == 0b0000_d);
    }
}

TEST_CASE("Logic: edge cases", "[logic][edge]") {
    auto ctx = std::make_unique<ch::core::context>("test_logic_edge");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Single bit operations") {
        ch_uint<1> a(1_d);
        ch_uint<1> b(0_d);
        
        REQUIRE(simulator.get_value(and_gate<1>(a, b)) == 0);
        REQUIRE(simulator.get_value(or_gate<1>(a, b)) == 1);
        REQUIRE(simulator.get_value(xor_gate<1>(a, b)) == 1);
        REQUIRE(simulator.get_value(nand_gate<1>(a, b)) == 1);
    }
    
    SECTION("All ones input") {
        ch_uint<8> a(0xFF_d);
        ch_uint<8> b(0xFF_d);
        
        ch_uint<8> result_and = and_gate<8>(a, b);
        ch_uint<8> result_or = or_gate<8>(a, b);
        ch_uint<8> result_xor = xor_gate<8>(a, b);
        ch_uint<8> result_nand = nand_gate<8>(a, b);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result_and) == 0xFF);
        REQUIRE(simulator.get_value(result_or) == 0xFF);
        REQUIRE(simulator.get_value(result_xor) == 0x00);
        REQUIRE(simulator.get_value(result_nand) == 0x00);
    }
}