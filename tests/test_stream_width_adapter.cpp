/**
 * @file test_stream_width_adapter.cpp
 * @brief 位宽适配器结构测试
 * 
 * 验证 stream_narrow_to_wide 和 stream_wide_to_narrow 函数返回正确的流类型
 */

#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "chlib/stream_width_adapter.h"
#include "chlib/stream.h"
#include "core/context.h"
#include "simulator.h"

using namespace ch::core;
using namespace chlib;

TEST_CASE("Stream Width Adapter: narrow_to_wide 8→32", "[stream][width_adapter][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_narrow_to_wide_struct");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> narrow_input;
    auto result = stream_narrow_to_wide<ch_uint<32>, ch_uint<8>>(narrow_input);

    REQUIRE(result.payload.width == 32);
    REQUIRE(result.valid.width == 1);
    REQUIRE(result.ready.width == 1);
}

TEST_CASE("Stream Width Adapter: wide_to_narrow 32→8", "[stream][width_adapter][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_wide_to_narrow_struct");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<32>> wide_input;
    auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<32>>(wide_input);

    REQUIRE(result.payload.width == 8);
    REQUIRE(result.valid.width == 1);
    REQUIRE(result.ready.width == 1);
}

TEST_CASE("Stream Width Adapter: 8→16 ratio", "[stream][width_adapter][ratio]") {
    auto ctx = std::make_unique<ch::core::context>("test_8_to_16");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> narrow_input;
    auto result = stream_narrow_to_wide<ch_uint<16>, ch_uint<8>>(narrow_input);

    REQUIRE(result.payload.width == 16);
}

TEST_CASE("Stream Width Adapter: 16→8 ratio", "[stream][width_adapter][ratio]") {
    auto ctx = std::make_unique<ch::core::context>("test_16_to_8");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<16>> wide_input;
    auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<16>>(wide_input);

    REQUIRE(result.payload.width == 8);
}

TEST_CASE("Stream Width Adapter: 4→32 ratio", "[stream][width_adapter][ratio]") {
    auto ctx = std::make_unique<ch::core::context>("test_4_to_32");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<4>> narrow_input;
    auto result = stream_narrow_to_wide<ch_uint<32>, ch_uint<4>>(narrow_input);

    REQUIRE(result.payload.width == 32);
}

TEST_CASE("Stream Width Adapter: 64→8 ratio", "[stream][width_adapter][ratio]") {
    auto ctx = std::make_unique<ch::core::context>("test_64_to_8");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<64>> wide_input;
    auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<64>>(wide_input);

    REQUIRE(result.payload.width == 8);
}
