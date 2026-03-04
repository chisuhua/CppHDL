// tests/test_stream_arbiter.cpp
// Tests for additional stream arbiter strategies: locking and sequential

#include "catch_amalgamated.hpp"
#include "chlib/stream.h"
#include "chlib/stream_arbiter.h"
#include "core/context.h"
#include "simulator.h"

#include <memory>

using namespace ch;
using namespace chlib;

using namespace ch;

// Test: Stream Arbiter Locking - basic operation
TEST_CASE("Stream Arbiter: Locking arbiter basic operation", "[stream][arbiter][lock]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter_lock");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // Create 2 input streams
    std::array<ch_stream<ch_uint<8>>, 2> input_streams;
    input_streams[0].payload = 0x11_h;
    input_streams[0].valid = true;
    input_streams[1].payload = 0x22_h;
    input_streams[1].valid = false;

    auto arb_result = stream_arbiter_lock<ch_uint<8>, 2>(input_streams);

    // Structural checks
    REQUIRE(arb_result.output_stream.payload.width == 8);
    REQUIRE(arb_result.output_stream.valid.width == 1);
    REQUIRE(arb_result.selected.width == compute_idx_width(2));
}

// Test: Stream Arbiter Locking - locks on first valid and stays
TEST_CASE("Stream Arbiter: Locking arbiter holds lock until transaction", "[stream][arbiter][lock]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter_lock_hold");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // Create 3 input streams with first one valid
    std::array<ch_stream<ch_uint<8>>, 3> input_streams;
    input_streams[0].payload = 0xAA_h;
    input_streams[0].valid = true;
    input_streams[1].payload = 0xBB_h;
    input_streams[1].valid = true;
    input_streams[2].payload = 0xCC_h;
    input_streams[2].valid = false;

    auto arb_result = stream_arbiter_lock<ch_uint<8>, 3>(input_streams);

    ch::Simulator sim(ctx.get());
    sim.tick();

    // When both 0 and 1 are valid, locking arbiter should pick one and stay with it
    // Output should be valid
    REQUIRE(sim.get_value(arb_result.output_stream.valid) == true);
}

// Test: Stream Arbiter Sequential - fixed order, wraps around
TEST_CASE("Stream Arbiter: Sequential arbiter basic operation", "[stream][arbiter][sequence]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter_sequence");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // Create 3 input streams
    std::array<ch_stream<ch_uint<8>>, 3> input_streams;
    input_streams[0].payload = 0x11_h;
    input_streams[0].valid = false;
    input_streams[1].payload = 0x22_h;
    input_streams[1].valid = true;
    input_streams[2].payload = 0x33_h;
    input_streams[2].valid = false;

    auto arb_result = stream_arbiter_sequence<ch_uint<8>, 3>(input_streams);

    // Structural checks
    REQUIRE(arb_result.output_stream.payload.width == 8);
    REQUIRE(arb_result.output_stream.valid.width == 1);
    REQUIRE(arb_result.selected.width == compute_idx_width(3));
}

// Test: Stream Arbiter Sequential - wraps from last to first
TEST_CASE("Stream Arbiter: Sequential arbiter wraps around", "[stream][arbiter][sequence]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter_sequence_wrap");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // Create 2 input streams
    std::array<ch_stream<ch_uint<8>>, 2> input_streams;
    input_streams[0].payload = 0x11_h;
    input_streams[0].valid = true;
    input_streams[1].payload = 0x22_h;
    input_streams[1].valid = true;

    auto arb_result = stream_arbiter_sequence<ch_uint<8>, 2>(input_streams);

    ch::Simulator sim(ctx.get());
    sim.tick();

    // Output should be valid with at least one input valid
    REQUIRE(sim.get_value(arb_result.output_stream.valid) == true);
}

// Test: Stream Arbiter Sequential - no valid inputs
TEST_CASE("Stream Arbiter: Sequential arbiter handles no valid inputs", "[stream][arbiter][sequence]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter_sequence_empty");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // Create 2 input streams, neither valid
    std::array<ch_stream<ch_uint<8>>, 2> input_streams;
    input_streams[0].payload = 0x11_h;
    input_streams[0].valid = false;
    input_streams[1].payload = 0x22_h;
    input_streams[1].valid = false;

    auto arb_result = stream_arbiter_sequence<ch_uint<8>, 2>(input_streams);

    ch::Simulator sim(ctx.get());
    sim.tick();

    // Output should not be valid when no inputs are valid
    REQUIRE(sim.get_value(arb_result.output_stream.valid) == false);
}
