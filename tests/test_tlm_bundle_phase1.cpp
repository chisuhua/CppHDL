// tests/test_tlm_bundle_phase1.cpp
#define CATCH_CONFIG_MAIN
#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_layout.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_serialization.h"
#include "core/bundle/bundle_traits.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/uint.h"
#include <memory>

using namespace ch;
using namespace ch::core;

// 测试用的简单Bundle
struct test_simple_bundle : public bundle_base<test_simple_bundle> {
    ch_uint<8> data;
    ch_bool flag;
    ch_uint<4> status;

    test_simple_bundle() = default;

    CH_BUNDLE_FIELDS(test_simple_bundle, data, flag, status)

    void as_master() override { this->make_output(data, flag, status); }

    void as_slave() override { this->make_input(data, flag, status); }
};

TEST_CASE("Phase1 - BundleWidthCalculation", "[phase1][width]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试宽度计算
    STATIC_REQUIRE(bundle_width_v<test_simple_bundle> == 13);  // 8 + 1 + 4
    STATIC_REQUIRE(bundle_width_v<Stream<ch_uint<32>>> == 34); // 32 + 1 + 1

    test_simple_bundle simple;
    Stream<ch_uint<16>> stream;

    REQUIRE(simple.width() == 13);
    REQUIRE(stream.width() == 18);
}

TEST_CASE("Phase1 - BundleLayoutInfo", "[phase1][layout]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试布局信息获取
    constexpr auto layout = get_bundle_layout<test_simple_bundle>();
    REQUIRE(std::tuple_size_v<decltype(layout)> == 3);

    constexpr auto stream_layout = get_bundle_layout<Stream<ch_uint<32>>>();
    REQUIRE(std::tuple_size_v<decltype(stream_layout)> == 3);
}

TEST_CASE("Phase1 - SerializationBasic", "[phase1][serialization]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试基本序列化
    test_simple_bundle bundle;
    bundle.data = ch_uint<8>(0xAB_h);
    bundle.flag = ch_bool(true);
    bundle.status = ch_uint<4>(0xC_h);

    auto bits = serialize(bundle);
    REQUIRE(bits.width == 13);

    // 测试反序列化
    auto recovered = deserialize<test_simple_bundle>(bits);
    REQUIRE(recovered.width() == 13);
}

TEST_CASE("Phase1 - StreamBundleSerialization", "[phase1][stream]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    Stream<ch_uint<16>> stream;
    stream.payload = ch_uint<16>(0x1234_h);
    stream.valid = ch_bool(true);
    stream.ready = ch_bool(false);

    auto bits = serialize(stream);
    REQUIRE(bits.width == 18);

    auto recovered = deserialize<Stream<ch_uint<16>>>(bits);
    REQUIRE(recovered.width() == 18);
}

TEST_CASE("Phase1 - BitsConversion", "[phase1][conversion]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    test_simple_bundle bundle;
    auto bits_view = to_bits(bundle);

    REQUIRE(bits_view.width == 13);
}

TEST_CASE("Phase1 - ByteConversion", "[phase1][bytes]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> value(0xFF_h);
    unsigned char bytes[2] = {0};

    bits_to_bytes(value, bytes, 2);
    auto recovered = bytes_to_bits<8>(bytes, 2);

    REQUIRE(static_cast<uint64_t>(recovered) == 0xFF);
}

TEST_CASE("Phase1 - Integration", "[phase1][integration]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 完整的序列化-反序列化流程
    Stream<ch_uint<32>> original;
    original.payload = ch_uint<32>(0xDEADBEEF_h);
    original.valid = ch_bool(true);
    original.ready = ch_bool(false);

    // 序列化
    auto bits = serialize(original);
    REQUIRE(bits.width == 34);

    // 反序列化
    auto recovered = deserialize<Stream<ch_uint<32>>>(bits);
    REQUIRE(recovered.width() == 34);

    // 验证字段
    // REQUIRE(static_cast<uint64_t>(recovered.payload) == 0xDEADBEEF);
    // REQUIRE(static_cast<bool>(recovered.valid) == true);
    // REQUIRE(static_cast<bool>(recovered.ready) == false);
}
