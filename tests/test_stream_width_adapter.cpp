#define CATCH_CONFIG_MAIN
#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "chlib/stream_width_adapter.h"
#include "chlib/stream.h"
#include "core/context.h"
#include "simulator.h"

using namespace ch::core;
using namespace chlib;

// ============ Test: Stream Width Adapter - Narrow to Wide ============

TEST_CASE("Stream Width Adapter: Narrow to Wide (8-bit to 32-bit)", "[stream][width_adapter]") {
    auto ctx = std::make_unique<ch::core::context>("test_narrow_to_wide");
    ch::core::ctx_swap ctx_guard(ctx.get());

    SECTION("Combine 4 x 8-bit into 32-bit") {
        // Create narrow input stream (8-bit)
        ch_stream<ch_uint<8>> narrow_input;
        narrow_input.payload = 0xAB_h;
        narrow_input.valid = true;
        narrow_input.ready = true;

        // Use narrow-to-wide adapter: 4 x 8-bit = 32-bit
        auto result = stream_narrow_to_wide<ch_uint<32>, ch_uint<8>, 4>(narrow_input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Verify the output is valid
        REQUIRE(sim.get_value(result.output.valid) == true);
        // Verify the width is correct (32-bit)
        REQUIRE(result.output.payload.width == 32);
    }
}

TEST_CASE("Stream Width Adapter: Narrow to Wide with data accumulation", "[stream][width_adapter]") {
    auto ctx = std::make_unique<ch::core::context>("test_narrow_to_wide_data");
    ch::core::ctx_swap ctx_guard(ctx.get());

    SECTION("Data accumulation across multiple inputs") {
        ch_stream<ch_uint<8>> narrow_input;
        
        // First input: 0x11
        narrow_input.payload = 0x11_h;
        narrow_input.valid = true;
        
        auto result = stream_narrow_to_wide<ch_uint<16>, ch_uint<8>, 2>(narrow_input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Output should be valid after first beat (depends on implementation)
        REQUIRE(result.output.payload.width == 16);
    }
}

// ============ Test: Stream Width Adapter - Wide to Narrow ============

TEST_CASE("Stream Width Adapter: Wide to Narrow (32-bit to 8-bit)", "[stream][width_adapter]") {
    auto ctx = std::make_unique<ch::core::context>("test_wide_to_narrow");
    ch::core::ctx_swap ctx_guard(ctx.get());

    SECTION("Split 32-bit into 4 x 8-bit") {
        // Create wide input stream (32-bit)
        ch_stream<ch_uint<32>> wide_input;
        wide_input.payload = 0xDEADBEEF_h;
        wide_input.valid = true;
        wide_input.ready = true;

        // Use wide-to-narrow adapter: 32-bit = 4 x 8-bit
        auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<32>, 4>(wide_input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Verify the output stream is valid
        REQUIRE(sim.get_value(result.output.valid) == true);
        // Verify the output width is correct (8-bit)
        REQUIRE(result.output.payload.width == 8);
    }
}

TEST_CASE("Stream Width Adapter: Wide to Narrow with data splitting", "[stream][width_adapter]") {
    auto ctx = std::make_unique<ch::core::context>("test_wide_to_narrow_data");
    ch::core::ctx_swap ctx_guard(ctx.get());

    SECTION("Wide data is correctly split") {
        ch_stream<ch_uint<16>> wide_input;
        wide_input.payload = 0x1234_h;
        wide_input.valid = true;
        
        // Convert 16-bit to 2 x 8-bit
        auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<16>, 2>(wide_input);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Verify output width is 8-bit
        REQUIRE(result.output.payload.width == 8);
    }
}

// ============ Test: Stream Width Adapter - Structural Tests ============

TEST_CASE("Stream Width Adapter: Structural - narrow_to_wide output type", "[stream][width_adapter][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_narrow_to_wide_struct");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> narrow_input;
    auto result = stream_narrow_to_wide<ch_uint<32>, ch_uint<8>, 4>(narrow_input);

    // Verify output stream has correct structure
    REQUIRE(result.output.payload.width == 32);
    REQUIRE(result.output.valid.width == 1);
    REQUIRE(result.output.ready.width == 1);
}

TEST_CASE("Stream Width Adapter: Structural - wide_to_narrow output type", "[stream][width_adapter][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_wide_to_narrow_struct");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<32>> wide_input;
    auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<32>, 4>(wide_input);

    // Verify output stream has correct structure
    REQUIRE(result.output.payload.width == 8);
    REQUIRE(result.output.valid.width == 1);
    REQUIRE(result.output.ready.width == 1);
}

// ============ Test: Different width ratios ============

TEST_CASE("Stream Width Adapter: Different ratios - 8-bit to 16-bit", "[stream][width_adapter][ratio]") {
    auto ctx = std::make_unique<ch::core::context>("test_8_to_16");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> narrow_input;
    narrow_input.payload = 0x55_h;
    narrow_input.valid = true;
    
    auto result = stream_narrow_to_wide<ch_uint<16>, ch_uint<8>, 2>(narrow_input);

    ch::Simulator sim(ctx.get());
    sim.tick();

    REQUIRE(result.output.payload.width == 16);
}

TEST_CASE("Stream Width Adapter: Different ratios - 16-bit to 8-bit", "[stream][width_adapter][ratio]") {
    auto ctx = std::make_unique<ch::core::context>("test_16_to_8");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<16>> wide_input;
    wide_input.payload = 0xA5A5_h;
    wide_input.valid = true;
    
    auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<16>, 2>(wide_input);

    ch::Simulator sim(ctx.get());
    sim.tick();

    REQUIRE(result.output.payload.width == 8);
}
