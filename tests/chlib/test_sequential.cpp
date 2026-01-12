#include "catch_amalgamated.hpp"
#include "chlib/sequential.h"
#include "codegen_dag.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <bitset>
#include <memory>

using namespace ch::core;
using namespace chlib;

// Helper function to convert uint value to binary string representation
template <typename T> std::string to_binary_string(T value, size_t width) {
    std::bitset<64> bs(static_cast<uint64_t>(value));
    std::string result = bs.to_string();
    // Return only the requested width bits
    return result.substr(64 - width, width);
}

TEST_CASE("Sequential: register function", "[sequential][register]") {
    auto ctx = std::make_unique<ch::core::context>("test_register");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic register functionality") {
        auto clk = ctx->get_default_clock();
        ch_bool rst = false;
        ch_bool en = true;
        ch_uint<4> d = 5_d;

        ch_uint<4> q = register_<4>(rst, en, d, "test_reg");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(q) == 0); // Initial value

        // First tick with default values
        sim.set_value(rst, 1_b);
        sim.tick();

        toDAG("1.dot", ctx.get(), sim);

        // Get values before printing
        auto clk_val = sim.get_value(clk);
        auto rst_val = sim.get_value(rst);
        auto en_val = sim.get_value(en);
        auto d_val = sim.get_value(d);
        auto q_val = sim.get_value(q);

        // Print inputs and outputs in binary
        std::cout << "After first tick: clk=" << clk_val << ", rst=" << rst_val
                  << ", en=" << en_val << ", d=" << d_val << std::endl;
        std::cout << "Output: q=0b" << to_binary_string(q_val, 4) << std::endl;

        REQUIRE(sim.get_value(q) == 0); // Still initial value after first tick

        // Update inputs using sim.set_value and apply clock pulse
        sim.set_value(rst, 0_b);
        sim.set_value(d, 6_d);
        // sim.set_value(clk, true);
        sim.tick();
        toDAG("2.dot", ctx.get(), sim);

        // Get values before printing
        clk_val = sim.get_value(clk);
        rst_val = sim.get_value(rst);
        en_val = sim.get_value(en);
        d_val = sim.get_value(d);
        q_val = sim.get_value(q);

        // Print inputs and outputs in binary
        std::cout << "Input: clk=" << clk_val << ", rst=" << rst_val
                  << ", en=" << en_val << ", d=" << d_val << std::endl;
        std::cout << "Output: q=0b" << to_binary_string(q_val, 4) << std::endl;

        REQUIRE(sim.get_value(q) == 6); // Value after clock
    }

    SECTION("Register with reset") {
        auto clk = ctx->get_default_clock();
        ch_bool rst = true; // Reset active
        ch_bool en = true;
        ch_uint<4> d = 5_d;

        ch_uint<4> q = register_<4>(rst, en, d, "test_reg_reset");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(q) == 0); // Initial value

        sim.tick();

        // Get values before printing
        auto clk_val = sim.get_value(clk);
        auto rst_val = sim.get_value(rst);
        auto en_val = sim.get_value(en);
        auto d_val = sim.get_value(d);
        auto q_val = sim.get_value(q);

        // Print inputs and outputs in binary
        std::cout << "Input: clk=" << clk_val << ", rst=" << rst_val
                  << ", en=" << en_val << ", d=" << d_val << std::endl;
        std::cout << "Output: q=0b" << to_binary_string(q_val, 4) << std::endl;

        REQUIRE(sim.get_value(q) == 0); // Reset value
    }

    SECTION("Basic register functionality with simplified interface") {
        ch_bool en = true;
        ch_uint<4> d = 5_d;

        ch_uint<4> q = register_<4>(en, d, "test_reg_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(q) == 0); // Initial value

        sim.eval_combinational();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto d_val = sim.get_value(d);
        auto q_val = sim.get_value(q);

        // Print inputs and outputs in binary
        std::cout << "Input: en=" << en_val << ", d=" << d_val << std::endl;
        std::cout << "Output: q=0b" << to_binary_string(q_val, 4) << std::endl;

        REQUIRE(sim.get_value(q) == 0); // Initial value after first tick

        // Update inputs using sim.set_value
        sim.set_value(d, 6_d);
        sim.tick();

        // Get values before printing
        en_val = sim.get_value(en);
        d_val = sim.get_value(d);
        q_val = sim.get_value(q);

        // Print inputs and outputs in binary
        std::cout << "Input: en=" << en_val << ", d=" << d_val << std::endl;
        std::cout << "Output: q=0b" << to_binary_string(q_val, 4) << std::endl;

        REQUIRE(sim.get_value(q) == 6); // Value after tick
    }
}

TEST_CASE("Sequential: DFF function", "[sequential][dff]") {
    auto ctx = std::make_unique<ch::core::context>("test_dff");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("DFF with enable") {
        auto clk = ctx->get_default_clock();
        ch_bool rst = false;
        ch_bool en = false; // Disable
        ch_uint<8> d = 10101010_b;

        ch_uint<8> q = dff_<8>(rst, en, d, "test_dff");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(q) == 0); // Initial value

        sim.tick();

        // Get values before printing
        auto clk_val = sim.get_value(clk);
        auto rst_val = sim.get_value(rst);
        auto en_val = sim.get_value(en);
        auto d_val = sim.get_value(d);
        auto q_val = sim.get_value(q);

        // Print inputs and outputs in binary
        std::cout << "Input: clk=" << clk_val << ", rst=" << rst_val
                  << ", en=" << en_val << ", d=0b" << to_binary_string(d_val, 8)
                  << std::endl;
        std::cout << "Output: q=0b" << to_binary_string(q_val, 8) << std::endl;

        REQUIRE(sim.get_value(q) == 0); // Initial value after first tick

        // Enable and apply clock
        sim.set_value(en, true);
        sim.set_value(clk, true);
        sim.tick();

        // Get values before printing
        clk_val = sim.get_value(clk);
        rst_val = sim.get_value(rst);
        en_val = sim.get_value(en);
        d_val = sim.get_value(d);
        q_val = sim.get_value(q);

        // Print inputs and outputs in binary
        std::cout << "Input: clk=" << clk_val << ", rst=" << rst_val
                  << ", en=" << en_val << ", d=0b" << to_binary_string(d_val, 8)
                  << std::endl;
        std::cout << "Output: q=0b" << to_binary_string(q_val, 8) << std::endl;

        REQUIRE(sim.get_value(q) == 10101010_b); // New value
    }

    SECTION("DFF with simplified interface") {
        ch_bool en = false; // Disable
        ch_uint<8> d = 10101010_b;

        ch_uint<8> q = dff_<8>(en, d, "test_dff_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(q) == 0); // Initial value

        sim.tick();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto d_val = sim.get_value(d);
        auto q_val = sim.get_value(q);

        // Print inputs and outputs in binary
        std::cout << "Input: en=" << en_val << ", d=0b"
                  << to_binary_string(d_val, 8) << std::endl;
        std::cout << "Output: q=0b" << to_binary_string(q_val, 8) << std::endl;

        REQUIRE(sim.get_value(q) == 0); // Initial value after first tick

        // Enable
        sim.set_value(en, true);
        sim.tick();

        // Get values before printing
        en_val = sim.get_value(en);
        d_val = sim.get_value(d);
        q_val = sim.get_value(q);

        // Print inputs and outputs in binary
        std::cout << "Input: en=" << en_val << ", d=0b"
                  << to_binary_string(d_val, 8) << std::endl;
        std::cout << "Output: q=0b" << to_binary_string(q_val, 8) << std::endl;

        REQUIRE(sim.get_value(q) == 10101010_b); // New value
    }
}

TEST_CASE("Sequential: binary counter function",
          "[sequential][binary_counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_binary_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("4-bit binary counter with simplified interface") {
        ch_bool en = false;

        ch_uint<4> count = binary_counter<4>(en, "test_binary_counter_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(count) == 0); // Initial value

        sim.eval_combinational();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto count_val = sim.get_value(count);

        // Print inputs and outputs in binary
        std::cout << "Initial: en=" << en_val << std::endl;
        std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                  << std::endl;

        REQUIRE(sim.get_value(count) == 0); // Initial value after first tick

        // Enable
        sim.set_value(en, true);

        // Count from 0 to 15
        for (int i = 0; i < 15; ++i) {
            sim.tick();

            // Get values before printing
            en_val = sim.get_value(en);
            count_val = sim.get_value(count);

            // Print inputs and outputs in binary
            std::cout << "Step " << i << ": en=" << en_val << std::endl;
            std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                      << std::endl;

            REQUIRE(sim.get_value(count) == i + 1);
        }

        // Next increment should wrap around to 0
        sim.tick();

        // Get values before printing
        en_val = sim.get_value(en);
        count_val = sim.get_value(count);

        // Print inputs and outputs in binary
        std::cout << "Wrap: en=" << en_val << std::endl;
        std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                  << std::endl;

        REQUIRE(sim.get_value(count) == 0); // Wrapped to 0
    }
}

TEST_CASE("Sequential: BCD counter function", "[sequential][bcd_counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_bcd_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("4-bit BCD counter with simplified interface") {
        ch_bool en = false;

        BCDCounterResult<4> result =
            bcd_counter<4>(en, "test_bcd_counter_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(result.count) == 0);     // Initial value
        REQUIRE(sim.get_value(result.carry) == false); // Initial value

        sim.tick();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto count_val = sim.get_value(result.count);
        auto carry_val = sim.get_value(result.carry);

        // Print inputs and outputs in binary
        std::cout << "Initial: en=" << en_val << std::endl;
        std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                  << ", carry=" << carry_val << std::endl;

        REQUIRE(sim.get_value(result.count) ==
                0); // Initial value after first tick
        REQUIRE(sim.get_value(result.carry) ==
                false); // Initial value after first tick

        // Enable
        sim.set_value(en, true);

        // Count from 0 to 9 (BCD range)
        for (int i = 0; i < 9; ++i) {
            sim.tick();

            // Get values before printing
            en_val = sim.get_value(en);
            count_val = sim.get_value(result.count);
            carry_val = sim.get_value(result.carry);

            // Print inputs and outputs in binary
            std::cout << "Step " << i << ": en=" << en_val << std::endl;
            std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                      << ", carry=" << carry_val << std::endl;

            REQUIRE(sim.get_value(result.count) == i + 1);

            // Check carry flag - should be high only when reaching 9 and
            // counting to 0
            if (i == 8) { // When counter was 9 and will reset to 0
                REQUIRE(sim.get_value(result.carry) == true);
            }
        }

        // Next increment should reset to 0 with carry
        sim.tick();

        // Get values before printing
        en_val = sim.get_value(en);
        count_val = sim.get_value(result.count);
        carry_val = sim.get_value(result.carry);

        // Print inputs and outputs in binary
        std::cout << "Reset: en=" << en_val << std::endl;
        std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                  << ", carry=" << carry_val << std::endl;

        REQUIRE(sim.get_value(result.count) == 0);     // Reset to 0
        REQUIRE(sim.get_value(result.carry) == false); // Carry generated
    }
}

TEST_CASE("Sequential: counter function", "[sequential][counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Up counter with simplified interface") {
        ch_bool en = false;
        ch_bool up_down = true; // Count up

        ch_uint<4> count = counter<4>(en, up_down, "test_counter_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(count) == 0); // Initial value

        sim.tick();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto up_down_val = sim.get_value(up_down);
        auto count_val = sim.get_value(count);

        // Print inputs and outputs in binary
        std::cout << "Initial: en=" << en_val << ", up_down=" << up_down_val
                  << std::endl;
        std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                  << std::endl;

        REQUIRE(sim.get_value(count) == 0); // Initial value after first tick

        // Enable
        sim.set_value(en, true);

        // Count up
        for (int i = 0; i < 5; ++i) {
            sim.tick();

            // Get values before printing
            en_val = sim.get_value(en);
            up_down_val = sim.get_value(up_down);
            count_val = sim.get_value(count);

            // Print inputs and outputs in binary
            std::cout << "Step " << i << ": en=" << en_val
                      << ", up_down=" << up_down_val << std::endl;
            std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                      << std::endl;

            REQUIRE(sim.get_value(count) == i + 1);
        }
    }

    SECTION("Down counter with simplified interface") {
        ch_bool en = false;
        ch_bool up_down = false; // Count down

        ch_uint<4> count = counter<4>(en, up_down, "test_counter_down_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(count) == 0); // Initial value

        sim.tick();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto up_down_val = sim.get_value(up_down);
        auto count_val = sim.get_value(count);

        // Print inputs and outputs in binary
        std::cout << "Initial: en=" << en_val << ", up_down=" << up_down_val
                  << std::endl;
        std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                  << std::endl;

        REQUIRE(sim.get_value(count) == 0); // Initial value after first tick

        // Enable
        sim.set_value(en, true);

        // For testing purposes, we'll manually set the counter to max
        // and then count down
        for (int i = 0; i < 5; ++i) {
            sim.tick();

            // Get values before printing
            en_val = sim.get_value(en);
            up_down_val = sim.get_value(up_down);
            count_val = sim.get_value(count);

            // Print inputs and outputs in binary
            std::cout << "Step " << i << ": en=" << en_val
                      << ", up_down=" << up_down_val << std::endl;
            std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                      << std::endl;

            // Since we start from 0 and count down, we wrap around
            // 0 -> 15 -> 14 -> 13 -> 12
            REQUIRE(sim.get_value(count) == (i == 0 ? 15 : 15 - i));
        }
    }
}

TEST_CASE("Sequential: ring counter function", "[sequential][ring_counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_ring_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("4-bit ring counter with simplified interface") {
        ch_bool en = true;

        ch_uint<4> out = ring_counter<4>(en, "test_ring_counter_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(out) == 0); // Initial value

        sim.eval_combinational();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto out_val = sim.get_value(out);

        // Print inputs and outputs in binary
        std::cout << "Initial: en=" << en_val << std::endl;
        std::cout << "Output: out=0b" << to_binary_string(out_val, 4)
                  << std::endl;

        REQUIRE(sim.get_value(out) == 1); // Initial value after first tick

        // Check initial state
        sim.tick();

        // Get values before printing
        en_val = sim.get_value(en);
        out_val = sim.get_value(out);

        // Print inputs and outputs in binary
        std::cout << "After tick: en=" << en_val << std::endl;
        std::cout << "Output: out=0b" << to_binary_string(out_val, 4)
                  << std::endl;

        REQUIRE(sim.get_value(out) == 2); // 0001

        // Shift to 3 times counting
        for (int i = 1; i < 3; ++i) {
            sim.tick();

            // Get values before printing
            en_val = sim.get_value(en);
            out_val = sim.get_value(out);

            // Print inputs and outputs in binary
            std::cout << "Shift " << i << ": en=" << en_val << std::endl;
            std::cout << "Output: out=0b" << to_binary_string(out_val, 4)
                      << std::endl;

            // 0001 -> 0010 -> 0100 -> 1000
            REQUIRE(sim.get_value(out) == (1 << (i + 1)));
        }

        // One more shift to complete the ring
        sim.tick();

        // Get values before printing
        en_val = sim.get_value(en);
        out_val = sim.get_value(out);

        // Print inputs and outputs in binary
        std::cout << "Final shift: en=" << en_val << std::endl;
        std::cout << "Output: out=0b" << to_binary_string(out_val, 4)
                  << std::endl;

        // toDAG("ring.dot", ctx.get(), sim);
        // sim.toVCD("ring.vcd");

        REQUIRE(sim.get_value(out) == 1); // Back to 0001
    }
}

TEST_CASE("Sequential: shift register function",
          "[sequential][shift_register]") {
    auto ctx = std::make_unique<ch::core::context>("test_shift_register");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Left shift operation with simplified interface") {
        ch_bool en = true;
        ch_bool shift_dir = true; // Left shift
        ch_bool load = false;
        ch_uint<4> parallel_in = 1010_b;

        ShiftRegisterResult<4> sreg = shift_register<4>(
            en, shift_dir, parallel_in, load, "test_shift_reg_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(sreg.out) == 0); // Initial value

        sim.tick();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto shift_dir_val = sim.get_value(shift_dir);
        auto load_val = sim.get_value(load);
        auto parallel_in_val = sim.get_value(parallel_in);
        auto out_val = sim.get_value(sreg.out);

        // Print inputs and outputs in binary
        std::cout << "Initial: en=" << en_val << ", shift_dir=" << shift_dir_val
                  << ", load=" << load_val << ", parallel_in=0b"
                  << to_binary_string(parallel_in_val, 4) << std::endl;
        std::cout << "Output: out=0b" << to_binary_string(out_val, 4)
                  << std::endl;

        REQUIRE(sim.get_value(sreg.out) == 0); // Initial value after first tick

        // Load initial value
        sim.set_value(load, true);
        sim.tick();
        sim.set_value(load, false);
        sim.eval_combinational();

        // Get values before printing
        en_val = sim.get_value(en);
        shift_dir_val = sim.get_value(shift_dir);
        load_val = sim.get_value(load);
        parallel_in_val = sim.get_value(parallel_in);
        out_val = sim.get_value(sreg.out);

        // Print inputs and outputs in binary
        std::cout << "After load: en=" << en_val
                  << ", shift_dir=" << shift_dir_val << ", load=" << load_val
                  << ", parallel_in=0b" << to_binary_string(parallel_in_val, 4)
                  << std::endl;
        std::cout << "Output: out=0b" << to_binary_string(out_val, 4)
                  << std::endl;

        REQUIRE(sim.get_value(sreg.out) == 1010_b);

        // Shift left
        sim.tick();

        // Get values before printing
        en_val = sim.get_value(en);
        shift_dir_val = sim.get_value(shift_dir);
        out_val = sim.get_value(sreg.out);

        // Print inputs and outputs in binary
        std::cout << "After shift: en=" << en_val
                  << ", shift_dir=" << shift_dir_val << std::endl;
        std::cout << "Output: out=0b" << to_binary_string(out_val, 4)
                  << std::endl;

        // 1010 shifted left is 0100 (MSB shifted out, 0 shifted in)
        REQUIRE(sim.get_value(sreg.out) == 0100_b);
    }
}

TEST_CASE("Sequential: edge detector function", "[sequential][edge_detector]") {
    auto ctx = std::make_unique<ch::core::context>("test_edge_detector");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Rising edge detection with simplified interface") {
        ch_bool signal = false;

        EdgeDetectorResult detector =
            edge_detector(signal, "test_edge_detector_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(detector.pos_edge) == false); // Initial value
        REQUIRE(sim.get_value(detector.neg_edge) == false); // Initial value
        REQUIRE(sim.get_value(detector.any_edge) == false); // Initial value

        sim.tick();

        // Get values before printing
        auto signal_val = sim.get_value(signal);
        auto pos_edge_val = sim.get_value(detector.pos_edge);
        auto neg_edge_val = sim.get_value(detector.neg_edge);
        auto any_edge_val = sim.get_value(detector.any_edge);

        // Print inputs and outputs in binary
        std::cout << "Initial: signal=" << signal_val << std::endl;
        std::cout << "Output: pos_edge=" << pos_edge_val
                  << ", neg_edge=" << neg_edge_val
                  << ", any_edge=" << any_edge_val << std::endl;

        REQUIRE(sim.get_value(detector.pos_edge) ==
                false); // Initial value after first tick
        REQUIRE(sim.get_value(detector.neg_edge) ==
                false); // Initial value after first tick
        REQUIRE(sim.get_value(detector.any_edge) ==
                false); // Initial value after first tick

        // Apply rising edge
        sim.set_value(signal, true);
        sim.eval_combinational();

        // toDAG("edge.dot", ctx.get(), sim);
        // sim.toVCD("edge.vcd");

        // Get values before printing
        signal_val = sim.get_value(signal);
        pos_edge_val = sim.get_value(detector.pos_edge);
        neg_edge_val = sim.get_value(detector.neg_edge);
        any_edge_val = sim.get_value(detector.any_edge);

        // Print inputs and outputs in binary
        std::cout << "After signal change: signal=" << signal_val << std::endl;
        std::cout << "Output: pos_edge=" << pos_edge_val
                  << ", neg_edge=" << neg_edge_val
                  << ", any_edge=" << any_edge_val << std::endl;

        REQUIRE(sim.get_value(detector.pos_edge) == true);
        REQUIRE(sim.get_value(detector.neg_edge) == false);
        REQUIRE(sim.get_value(detector.any_edge) == true);
    }
}

TEST_CASE("Sequential: configurable counter function",
          "[sequential][config_counter]") {
    auto ctx = std::make_unique<ch::core::context>("test_config_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Up counter mode with simplified interface") {
        ch_bool en = false;
        ch_uint<2> mode = 0_d;    // Up counter
        ch_uint<4> max_val = 7_d; // Max value 7

        ConfigurableCounterResult<4> counter = configurable_counter<4>(
            en, mode, max_val, "test_config_counter_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(counter.count) == 0);        // Initial value
        REQUIRE(sim.get_value(counter.overflow) == false); // Initial value

        sim.tick();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto mode_val = sim.get_value(mode);
        auto max_val_sim = sim.get_value(max_val);
        auto count_val = sim.get_value(counter.count);
        auto overflow_val = sim.get_value(counter.overflow);

        // Print inputs and outputs in binary
        std::cout << "Initial: en=" << en_val << ", mode=" << mode_val
                  << ", max_val=" << max_val_sim << std::endl;
        std::cout << "Output: count=0b" << to_binary_string(count_val, 4)
                  << ", overflow=" << overflow_val << std::endl;

        REQUIRE(sim.get_value(counter.count) ==
                0); // Initial value after first tick
        REQUIRE(sim.get_value(counter.overflow) ==
                false); // Initial value after first tick

        // Enable
        sim.set_value(en, true);

        // Count up to max and wrap
        for (int i = 0; i < 10; ++i) {
            sim.tick();

            // Get values before printing
            en_val = sim.get_value(en);
            mode_val = sim.get_value(mode);
            max_val_sim = sim.get_value(max_val);
            count_val = sim.get_value(counter.count);
            overflow_val = sim.get_value(counter.overflow);

            // Print inputs and outputs in binary
            std::cout << "Step " << i << ": Input - en=" << en_val
                      << ", mode=" << mode_val << ", max_val=" << max_val_sim
                      << std::endl;
            std::cout << "Output - count=0b" << to_binary_string(count_val, 4)
                      << ", overflow=" << overflow_val << std::endl;

            // Should count 1, 2, 3, 4, 5, 6, 7, 0, 1, 2
            int expected = (i < 7) ? i + 1 : i - 7;
            REQUIRE(sim.get_value(counter.count) == expected);
        }
    }
}

TEST_CASE("Sequential: edge cases", "[sequential][edge]") {
    auto ctx = std::make_unique<ch::core::context>("test_sequential_edge");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Counter overflow behavior with simplified interface") {
        ch_bool en = true;
        ch_bool up_down = true; // Count up

        ch_uint<3> count =
            counter<3>(en, up_down, "test_counter_overflow_simple");

        ch::Simulator sim(ctx.get());

        // Initial value before any tick
        REQUIRE(sim.get_value(count) == 0); // Initial value

        sim.eval_combinational();

        // Get values before printing
        auto en_val = sim.get_value(en);
        auto up_down_val = sim.get_value(up_down);
        auto count_val = sim.get_value(count);

        // Print inputs and outputs in binary
        std::cout << "Initial: en=" << en_val << ", up_down=" << up_down_val
                  << std::endl;
        std::cout << "Output: count=0b" << to_binary_string(count_val, 3)
                  << std::endl;

        REQUIRE(sim.get_value(count) == 0); // Initial value after first tick

        // Set to max value manually by counting up
        for (int i = 0; i < 7; ++i) {
            sim.tick();

            // Get values before printing
            en_val = sim.get_value(en);
            up_down_val = sim.get_value(up_down);
            count_val = sim.get_value(count);

            // Print inputs and outputs in binary
            std::cout << "Step " << i << ": en=" << en_val
                      << ", up_down=" << up_down_val << std::endl;
            std::cout << "Output: count=0b" << to_binary_string(count_val, 3)
                      << std::endl;
        }

        // One more increment should wrap around
        sim.tick();

        // Get values before printing
        en_val = sim.get_value(en);
        up_down_val = sim.get_value(up_down);
        count_val = sim.get_value(count);

        // Print inputs and outputs in binary
        std::cout << "Wrap: en=" << en_val << ", up_down=" << up_down_val
                  << std::endl;
        std::cout << "Output: count=0b" << to_binary_string(count_val, 3)
                  << std::endl;

        REQUIRE(sim.get_value(count) == 0); // Wrapped to 0
    }
}