#define CATCH_CONFIG_MAIN
#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "chlib/stream_operators.h"
#include "chlib/stream_pipeline.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/uint.h"

using namespace ch::core;

// ============ Test: Direct stream connection operator<<= ============

TEST_CASE("Stream Operators - Direct Connection sink <<= source", "[stream][operators]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Create source and sink streams
    ch_stream<ch_uint<8>> source;
    ch_stream<ch_uint<8>> sink;

    // Set up source as master (valid and payload are outputs)
    source.as_master();
    // Set up sink as slave (valid and payload are inputs)
    sink.as_slave();

    // Connect: sink <<= source (direct connection, 0-cycle delay)
    // This should connect:
    // - sink.valid = source.valid
    // - sink.payload = source.payload
    // - source.ready = sink.ready
    sink <<= source;

    // Verify the connections exist (non-null implementations)
    REQUIRE(sink.valid.impl() != nullptr);
    REQUIRE(sink.payload.impl() != nullptr);
    REQUIRE(source.ready.impl() != nullptr);
}

TEST_CASE("Stream Operators - Direct Connection source >>= sink", "[stream][operators]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Create source and sink streams
    ch_stream<ch_uint<8>> source;
    ch_stream<ch_uint<8>> sink;

    // Set up source as master
    source.as_master();
    // Set up sink as slave
    sink.as_slave();

    // Connect: source >>= sink (reverse direction, same as sink <<= source)
    source >>= sink;

    // Verify the connections exist (non-null implementations)
    REQUIRE(sink.valid.impl() != nullptr);
    REQUIRE(sink.payload.impl() != nullptr);
    REQUIRE(source.ready.impl() != nullptr);
}

TEST_CASE("Stream Operators - Both Directions Equivalent", "[stream][operators]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Test 1: sink <<= source
    {
        ch_stream<ch_uint<8>> source1;
        ch_stream<ch_uint<8>> sink1;
        source1.as_master();
        sink1.as_slave();
        sink1 <<= source1;
        REQUIRE(sink1.valid.impl() != nullptr);
        REQUIRE(sink1.payload.impl() != nullptr);
        REQUIRE(source1.ready.impl() != nullptr);
    }

    // Test 2: source >>= sink (should be equivalent)
    {
        ch_stream<ch_uint<8>> source2;
        ch_stream<ch_uint<8>> sink2;
        source2.as_master();
        sink2.as_slave();
        source2 >>= sink2;
        REQUIRE(sink2.valid.impl() != nullptr);
        REQUIRE(sink2.payload.impl() != nullptr);
        REQUIRE(source2.ready.impl() != nullptr);
    }
}

TEST_CASE("Stream Operators - Different Payload Widths", "[stream][operators][width]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Test with different payload widths
    {
        ch_stream<ch_uint<16>> source;
        ch_stream<ch_uint<16>> sink;
        source.as_master();
        sink.as_slave();
        sink <<= source;
        REQUIRE(sink.valid.impl() != nullptr);
        REQUIRE(sink.payload.impl() != nullptr);
        REQUIRE(source.ready.impl() != nullptr);
    }

    {
        ch_stream<ch_uint<32>> source;
        ch_stream<ch_uint<32>> sink;
        source.as_master();
        sink.as_slave();
        source >>= sink;
        REQUIRE(sink.valid.impl() != nullptr);
        REQUIRE(sink.payload.impl() != nullptr);
        REQUIRE(source.ready.impl() != nullptr);
    }
}


// ============ Test: Pipeline connection operators ============

// Note: C++ doesn't support arbitrary operator sequences like <<=<, so we use
// the pattern: sink <<= pipe_result.output where pipe_result comes from
// stream_m2s_pipe(), stream_s2m_pipe(), or chained pipelines.
// This is equivalent to SpinalHDL's <<=<, <<</<, <<=</< operators.

TEST_CASE("Stream Operators - M2S Pipeline connection", "[stream][operators][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Create source and sink streams
    ch_stream<ch_uint<8>> source;
    ch_stream<ch_uint<8>> sink;

    // Set up source as master, sink as slave
    source.as_master();
    sink.as_slave();

    // Connect with M2S pipeline: sink <<= m2s_pipe(source).output
    // Equivalent to SpinalHDL's sink <<=< source
    auto pipe_result = chlib::stream_m2s_pipe(source);
    sink <<= pipe_result.output;

    // Verify the connections exist (non-null implementations)
    // After m2sPipe, sink should be connected to the pipe output
    REQUIRE(sink.valid.impl() != nullptr);
    REQUIRE(sink.payload.impl() != nullptr);
    REQUIRE(source.ready.impl() != nullptr);
}

TEST_CASE("Stream Operators - M2S Pipeline reverse direction", "[stream][operators][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> source;
    ch_stream<ch_uint<8>> sink;

    source.as_master();
    sink.as_slave();

    // Reverse direction: m2s_pipe(source).output >>= sink
    // Equivalent to SpinalHDL's source >>=> sink
    auto pipe_result = chlib::stream_m2s_pipe(source);
    pipe_result.output >>= sink;

    REQUIRE(sink.valid.impl() != nullptr);
    REQUIRE(sink.payload.impl() != nullptr);
    REQUIRE(source.ready.impl() != nullptr);
}

TEST_CASE("Stream Operators - S2M Pipeline connection", "[stream][operators][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> source;
    ch_stream<ch_uint<8>> sink;

    source.as_master();
    sink.as_slave();

    // Connect with S2M pipeline: sink <<= s2m_pipe(source).output
    // Equivalent to SpinalHDL's sink <<</< source
    auto pipe_result = chlib::stream_s2m_pipe(source);
    sink <<= pipe_result.output;

    REQUIRE(sink.valid.impl() != nullptr);
    REQUIRE(sink.payload.impl() != nullptr);
    REQUIRE(source.ready.impl() != nullptr);
}

TEST_CASE("Stream Operators - S2M Pipeline reverse direction", "[stream][operators][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> source;
    ch_stream<ch_uint<8>> sink;

    source.as_master();
    sink.as_slave();

    // Reverse direction: s2m_pipe(source).output >>= sink
    // Equivalent to SpinalHDL's source >>>/> sink
    auto pipe_result = chlib::stream_s2m_pipe(source);
    pipe_result.output >>= sink;

    REQUIRE(sink.valid.impl() != nullptr);
    REQUIRE(sink.payload.impl() != nullptr);
    REQUIRE(source.ready.impl() != nullptr);
}

TEST_CASE("Stream Operators - Full Pipeline connection", "[stream][operators][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> source;
    ch_stream<ch_uint<8>> sink;

    source.as_master();
    sink.as_slave();

    // Connect with full pipeline (s2m + m2s): sink <<= m2s(s2m(source).output).output
    // Equivalent to SpinalHDL's sink <<=</< source (2-cycle total delay)
    auto s2m_result = chlib::stream_s2m_pipe(source);
    auto m2s_result = chlib::stream_m2s_pipe(s2m_result.output);
    sink <<= m2s_result.output;

    REQUIRE(sink.valid.impl() != nullptr);
    REQUIRE(sink.payload.impl() != nullptr);
    REQUIRE(source.ready.impl() != nullptr);
}

TEST_CASE("Stream Operators - Full Pipeline reverse direction", "[stream][operators][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> source;
    ch_stream<ch_uint<8>> sink;

    source.as_master();
    sink.as_slave();

    // Reverse direction: m2s(s2m(source).output).output >>= sink
    // Equivalent to SpinalHDL's source >>>/-> sink
    auto s2m_result = chlib::stream_s2m_pipe(source);
    auto m2s_result = chlib::stream_m2s_pipe(s2m_result.output);
    m2s_result.output >>= sink;

    REQUIRE(sink.valid.impl() != nullptr);
    REQUIRE(sink.payload.impl() != nullptr);
    REQUIRE(source.ready.impl() != nullptr);
}
