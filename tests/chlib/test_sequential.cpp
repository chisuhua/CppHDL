#include "catch_amalgamated.hpp"
#include "chlib/sequential.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Sequential: register function", "[sequential][register]") {
    auto ctx = std::make_unique<ch::core::context>("test_register");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic register functionality") {
        ch_bool clk = false;
        ch_bool rst = false;
        ch_bool en = true;
        ch_uint<4> d = 5_d;
        
        ch_uint<4> q = register_<4>(d, clk, rst, en, "test_reg");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(q) == 0);  // Initial value
        
        // Update inputs and apply clock pulse
        d = 5_d;
        clk = true;
        sim.tick();
        
        REQUIRE(simulator.get_value(q) == 5);  // Value after clock
    }
    
    SECTION("Register with reset") {
        ch_bool clk = false;
        ch_bool rst = true;  // Reset active
        ch_bool en = true;
        ch_uint<4> d = 5_d;
        
        ch_uint<4> q = register_<4>(d, clk, rst, en, "test_reg_reset");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(q) == 0);  // Reset value
    }
}

TEST_CASE("Sequential: DFF function", "[sequential][dff]") {
    auto ctx = std::make_unique<ch::core::context>("test_dff");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("DFF with enable") {
        ch_bool clk = false;
        ch_bool rst = false;
        ch_bool en = false;  // Disable
        ch_uint<8> d = 0b10101010_d;
        
        ch_uint<8> q = dff_<8>(d, clk, rst, en, "test_dff");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(q) == 0);  // Initial value
        
        // Enable and apply clock
        en = true;
        clk = true;
        sim.tick();
        
        REQUIRE(simulator.get_value(q) == 0b10101010_d);  // New value
    }
}

TEST_CASE("Sequential: binary counter function", "[sequential][binary_counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_binary_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("4-bit binary counter") {
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool en = false;
        
        ch_uint<4> count = binary_counter<4>(clk, rst, en, "test_binary_counter");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Release reset and enable
        rst = false;
        en = true;
        
        // Count from 0 to 15
        for (int i = 0; i < 16; ++i) {
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
            
            REQUIRE(simulator.get_value(count) == i + 1);
        }
        
        // Next clock should wrap around to 0
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(count) == 0);  // Wrapped to 0
    }
}

TEST_CASE("Sequential: BCD counter function", "[sequential][bcd_counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_bcd_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("4-bit BCD counter") {
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool en = false;
        
        BCDCounterResult<4> result = bcd_counter<4>(clk, rst, en, "test_bcd_counter");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Release reset and enable
        rst = false;
        en = true;
        
        // Count from 0 to 9 (BCD range)
        for (int i = 0; i < 10; ++i) {
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
            
            REQUIRE(simulator.get_value(result.count) == i + 1);
            
            // Check carry flag - should be high only when reaching 9 and counting to 0
            if (i == 8) {  // When counter was 9 and will reset to 0
                REQUIRE(simulator.get_value(result.carry) == false);
            }
        }
        
        // Next clock should reset to 0 with carry
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(result.count) == 0);  // Reset to 0
        REQUIRE(simulator.get_value(result.carry) == true);  // Carry generated
    }
}

TEST_CASE("Sequential: counter function", "[sequential][counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Up counter") {
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool en = false;
        ch_bool up_down = true;  // Count up
        
        ch_uint<4> count = counter<4>(clk, rst, en, up_down, "test_counter");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Release reset and enable
        rst = false;
        en = true;
        
        // Count up
        for (int i = 0; i < 5; ++i) {
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
            
            REQUIRE(simulator.get_value(count) == i + 1);
        }
    }
    
    SECTION("Down counter") {
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool en = false;
        ch_bool up_down = false;  // Count down
        
        ch_uint<4> count = counter<4>(clk, rst, en, up_down, "test_counter_down");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Release reset and enable
        rst = false;
        en = true;
        
        // For testing purposes, we'll manually set the counter to max
        // and then count down
        for (int i = 0; i < 5; ++i) {
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
            
            // Since we start from 0 and count down, we wrap around
            // 0 -> 15 -> 14 -> 13 -> 12
            REQUIRE(simulator.get_value(count) == 
                   (i == 0 ? 15 : 15 - i));
        }
    }
}

TEST_CASE("Sequential: ring counter function", "[sequential][ring_counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_ring_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("4-bit ring counter") {
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool en = true;
        
        ch_uint<4> out = ring_counter<4>(clk, rst, en, "test_ring_counter");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Release reset
        rst = false;
        
        // Check initial state
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(out) == 1);  // 0001
        
        // Shift 3 times
        for (int i = 0; i < 3; ++i) {
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
            
            // 0001 -> 0010 -> 0100 -> 1000
            REQUIRE(simulator.get_value(out) == (1 << (i + 1)));
        }
        
        // One more shift to complete the ring
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(out) == 1);  // Back to 0001
    }
}

TEST_CASE("Sequential: shift register function", "[sequential][shift_register]") {
    auto ctx = std::make_unique<ch::core::context>("test_shift_register");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Left shift operation") {
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool en = true;
        ch_bool shift_dir = true;  // Left shift
        ch_bool load = false;
        ch_uint<4> parallel_in = 0b1010_d;
        
        ShiftRegisterResult<4> sreg = shift_register<4>(clk, rst, en, shift_dir, parallel_in, load, "test_shift_reg");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Release reset
        rst = false;
        
        // Load initial value
        load = true;
        clk = true;
        sim.tick();
        load = false;
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(sreg.out) == 0b1010_d);
        
        // Shift left
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        // 1010 shifted left is 0100 (MSB shifted out, 0 shifted in)
        REQUIRE(simulator.get_value(sreg.out) == 0b0100_d);
        REQUIRE(simulator.get_value(sreg.serial_out) == true);  // MSB was 1
    }
    
    SECTION("Right shift operation") {
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool en = true;
        ch_bool shift_dir = false;  // Right shift
        ch_bool load = false;
        ch_uint<4> parallel_in = 0b1100_d;
        
        ShiftRegisterResult<4> sreg = shift_register<4>(clk, rst, en, shift_dir, parallel_in, load, "test_shift_reg_right");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Release reset and load value
        rst = false;
        load = true;
        clk = true;
        sim.tick();
        load = false;
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(sreg.out) == 0b1100_d);
        
        // Shift right
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        // 1100 shifted right is 0110 (LSB shifted out, 0 shifted in)
        REQUIRE(simulator.get_value(sreg.out) == 0b0110_d);
        REQUIRE(simulator.get_value(sreg.serial_out) == false);  // LSB was 0
    }
}

TEST_CASE("Sequential: edge detector function", "[sequential][edge_detector]") {
    auto ctx = std::make_unique<ch::core::context>("test_edge_detector");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Rising edge detection") {
        ch_bool clk = false;
        ch_bool rst = false;
        ch_bool signal = false;
        
        EdgeDetectorResult detector = edge_detector(clk, rst, signal, "test_edge_detector");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(detector.pos_edge) == false);
        REQUIRE(simulator.get_value(detector.neg_edge) == false);
        REQUIRE(simulator.get_value(detector.any_edge) == false);
        
        // Apply rising edge
        signal = true;
        sim.eval();
        REQUIRE(simulator.get_value(detector.pos_edge) == false);
        
        // Clock the edge
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(detector.pos_edge) == true);
        REQUIRE(simulator.get_value(detector.neg_edge) == false);
        REQUIRE(simulator.get_value(detector.any_edge) == true);
    }
    
    SECTION("Falling edge detection") {
        ch_bool clk = false;
        ch_bool rst = false;
        ch_bool signal = true;
        
        EdgeDetectorResult detector = edge_detector(clk, rst, signal, "test_edge_detector_fall");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Apply falling edge
        signal = false;
        sim.eval();
        REQUIRE(simulator.get_value(detector.neg_edge) == false);
        
        // Clock the edge
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(detector.pos_edge) == false);
        REQUIRE(simulator.get_value(detector.neg_edge) == true);
        REQUIRE(simulator.get_value(detector.any_edge) == true);
    }
}

TEST_CASE("Sequential: configurable counter function", "[sequential][config_counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_config_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Up counter mode") {
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool en = false;
        ch_uint<2> mode = 0_d;  // Up counter
        ch_uint<4> max_val = 7_d;  // Max value 7
        
        ConfigurableCounterResult<4> counter = configurable_counter<4>(clk, rst, en, mode, max_val, "test_config_counter");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Release reset and enable
        rst = false;
        en = true;
        
        // Count up to max and wrap
        for (int i = 0; i < 10; ++i) {
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
            
            // Should count 1, 2, 3, 4, 5, 6, 7, 0, 1, 2
            int expected = (i < 8) ? i + 1 : i - 7;
            REQUIRE(simulator.get_value(counter.count) == expected);
        }
    }
    
    SECTION("Down counter mode") {
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool en = false;
        ch_uint<2> mode = 1_d;  // Down counter
        ch_uint<4> max_val = 5_d;  // Max value 5
        
        ConfigurableCounterResult<4> counter = configurable_counter<4>(clk, rst, en, mode, max_val, "test_config_counter_down");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Release reset and enable
        rst = false;
        en = true;
        
        // Count down from max
        for (int i = 0; i < 8; ++i) {
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
            
            // Should count 4, 3, 2, 1, 0, 15, 14, 13
            int expected;
            if (i < 5) {
                expected = 4 - i;
            } else {
                expected = 15 - (i - 5);
            }
            REQUIRE(simulator.get_value(counter.count) == expected);
        }
    }
}

TEST_CASE("Sequential: edge cases", "[sequential][edge]") {
    auto ctx = std::make_unique<ch::core::context>("test_sequential_edge");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Counter overflow behavior") {
        ch_bool clk = false;
        ch_bool rst = false;
        ch_bool en = true;
        ch_bool up_down = true;  // Count up
        
        ch_uint<3> count = counter<3>(clk, rst, en, up_down, "test_counter_overflow");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Set to max value manually by counting up
        for (int i = 0; i < 7; ++i) {
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
        }
        
        // One more increment should wrap around
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(count) == 0);  // Wrapped to 0
    }
}