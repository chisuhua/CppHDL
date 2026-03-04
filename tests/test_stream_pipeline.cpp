#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "bundle/stream_bundle.h"
#include "core/context.h"
#include "simulator.h"

#include "chlib/stream_pipeline.h"
#include "bundle/stream_bundle_member_inlines.h"

using namespace ch::core;
using namespace chlib;

TEST_CASE("Stream Pipeline: stream_m2s_pipe basic operation", "[stream][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_m2s_pipe");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream M2S pipe - basic 1-cycle delay") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x42_h;
        input_stream.valid = true;
        input_stream.ready = true;

        auto pipe_result = stream_m2s_pipe(input_stream);

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x42);
    }
}

TEST_CASE("Stream Pipeline: stream_m2s_pipe structural tests", "[stream][pipeline][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_m2s_pipe_structural");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream M2S pipe result structure") {
        ch_stream<ch_uint<8>> input_stream;
        
        auto pipe_result = stream_m2s_pipe(input_stream);

        REQUIRE(pipe_result.output.payload.width == 8);
        REQUIRE(pipe_result.output.valid.width == 1);
        REQUIRE(pipe_result.output.ready.width == 1);
        
        REQUIRE(pipe_result.input.payload.width == 8);
        REQUIRE(pipe_result.input.valid.width == 1);
        REQUIRE(pipe_result.input.ready.width == 1);
    }

    SECTION("Stream M2S pipe with different payload widths") {
        ch_stream<ch_uint<16>> input16;
        auto result16 = stream_m2s_pipe(input16);
        REQUIRE(result16.output.payload.width == 16);

        ch_stream<ch_uint<32>> input32;
        auto result32 = stream_m2s_pipe(input32);
        REQUIRE(result32.output.payload.width == 32);
    }
}


TEST_CASE("Stream Pipeline: stream_s2m_pipe basic operation", "[stream][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_s2m_pipe");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream S2M pipe - 0-cycle payload, 1-cycle ready") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x42_h;
        input_stream.valid = true;
        input_stream.ready = true;

        auto pipe_result = stream_s2m_pipe(input_stream);

        ch::Simulator sim(ctx.get());
        
        // Payload should be available immediately (0-cycle delay)
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x42);
        
        // Valid should also be immediately available
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
    }
}

TEST_CASE("Stream Pipeline: stream_s2m_pipe structural tests", "[stream][pipeline][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_s2m_pipe_structural");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream S2M pipe result structure") {
        ch_stream<ch_uint<8>> input_stream;
        
        auto pipe_result = stream_s2m_pipe(input_stream);

        REQUIRE(pipe_result.output.payload.width == 8);
        REQUIRE(pipe_result.output.valid.width == 1);
        REQUIRE(pipe_result.output.ready.width == 1);
        
        REQUIRE(pipe_result.input.payload.width == 8);
        REQUIRE(pipe_result.input.valid.width == 1);
        REQUIRE(pipe_result.input.ready.width == 1);
    }

    SECTION("Stream S2M pipe with different payload widths") {
        ch_stream<ch_uint<16>> input16;
        auto result16 = stream_s2m_pipe(input16);
        REQUIRE(result16.output.payload.width == 16);

        ch_stream<ch_uint<32>> input32;
        auto result32 = stream_s2m_pipe(input32);
        REQUIRE(result32.output.payload.width == 32);
    }
}

TEST_CASE("Stream Pipeline: stream_half_pipe basic operation", "[stream][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_half_pipe");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Half pipe - all signals registered (bandwidth halved)") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x42_h;
        input_stream.valid = true;
        input_stream.ready = true;

        auto pipe_result = stream_half_pipe(input_stream);

        ch::Simulator sim(ctx.get());
        
        // First tick: toggle starts at 0, so data not accepted yet
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.valid) == false);
        
        // Second tick: toggle now high, data is accepted
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x42);
    }
}

TEST_CASE("Stream Pipeline: stream_half_pipe structural tests", "[stream][pipeline][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_half_pipe_structural");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Half pipe result structure") {
        ch_stream<ch_uint<8>> input_stream;
        
        auto pipe_result = stream_half_pipe(input_stream);

        REQUIRE(pipe_result.output.payload.width == 8);
        REQUIRE(pipe_result.output.valid.width == 1);
        REQUIRE(pipe_result.output.ready.width == 1);
        
        REQUIRE(pipe_result.input.payload.width == 8);
        REQUIRE(pipe_result.input.valid.width == 1);
        REQUIRE(pipe_result.input.ready.width == 1);
    }

    SECTION("Stream Half pipe with different payload widths") {
        ch_stream<ch_uint<16>> input16;
        auto result16 = stream_half_pipe(input16);
        REQUIRE(result16.output.payload.width == 16);

        ch_stream<ch_uint<32>> input32;
        auto result32 = stream_half_pipe(input32);
        REQUIRE(result32.output.payload.width == 32);
    }
}


TEST_CASE("Stream Pipeline: ch_stream member functions - m2sPipe", "[stream][pipeline][member]") {
    auto ctx = std::make_unique<ch::core::context>("test_m2sPipe_member");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("ch_stream.m2sPipe() member function") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x42_h;
        input_stream.valid = true;
        input_stream.ready = true;

        auto pipe_result = input_stream.m2sPipe();

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x42);
    }
}

TEST_CASE("Stream Pipeline: ch_stream member functions - stage", "[stream][pipeline][member]") {
    auto ctx = std::make_unique<ch::core::context>("test_stage_member");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("ch_stream.stage() member function (alias for m2sPipe)") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0xAB_h;
        input_stream.valid = true;
        input_stream.ready = true;

        auto pipe_result = input_stream.stage();

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0xAB);
    }
}

TEST_CASE("Stream Pipeline: ch_stream member functions - s2mPipe", "[stream][pipeline][member]") {
    auto ctx = std::make_unique<ch::core::context>("test_s2mPipe_member");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("ch_stream.s2mPipe() member function") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x55_h;
        input_stream.valid = true;
        input_stream.ready = true;

        auto pipe_result = input_stream.s2mPipe();

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x55);
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
    }
}

TEST_CASE("Stream Pipeline: ch_stream member functions - halfPipe", "[stream][pipeline][member]") {
    auto ctx = std::make_unique<ch::core::context>("test_halfPipe_member");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("ch_stream.halfPipe() member function") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x77_h;
        input_stream.valid = true;
        input_stream.ready = true;

        auto pipe_result = input_stream.halfPipe();

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.valid) == false);
        
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x77);
    }
}
