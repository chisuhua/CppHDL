// tests/test_bundle_serialization.cpp
#define CATCH_CONFIG_MAIN
#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_protocol.h"
#include "core/bundle/bundle_serialization.h"
#include "core/bundle/bundle_traits.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/uint.h"
#include <memory>

using namespace ch;
using namespace ch::core;

// 简单的测试Bundle
struct test_simple_bundle : public bundle_base<test_simple_bundle> {
    ch_uint<8> data;
    ch_bool flag;

    test_simple_bundle() = default;

    CH_BUNDLE_FIELDS(test_simple_bundle, data, flag)

    void as_master() override { this->make_output(data, flag); }

    void as_slave() override { this->make_input(data, flag); }
};

// 新增序列化API测试
TEST_CASE("BundleSerialization - SerializationAPI",
          "[bundle][serialization][api]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    test_simple_bundle bundle;

    // 测试序列化API
    auto bits = ch::core::serialize(bundle);
    REQUIRE(bits.width == 9);

    // 测试反序列化API
    auto deserialized = ch::core::deserialize<test_simple_bundle>(bits);
    REQUIRE(deserialized.width() == 9);
}

TEST_CASE("BundleSerialization - WidthCalculation",
          "[bundle][serialization][width]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 使用统一的宽度计算函数
    STATIC_REQUIRE(bundle_width_v<test_simple_bundle> == 9);   // 8 + 1
    STATIC_REQUIRE(bundle_width_v<Stream<ch_uint<32>>> == 34); // 32 + 1 + 1

    std::cout << "Simple bundle width: "
              << bundle_width_v<test_simple_bundle> << std::endl;
    std::cout << "Stream bundle width: "
              << bundle_width_v<Stream<ch_uint<32>>> << std::endl;
}

TEST_CASE("BundleSerialization - NestedBundleWidth",
          "[bundle][serialization][nested]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 创建嵌套结构进行测试
    struct nested_test : public bundle_base<nested_test> {
        test_simple_bundle inner;
        ch_uint<4> extra;

        nested_test() = default;

        CH_BUNDLE_FIELDS(nested_test, inner, extra)

        void as_master() override { this->make_output(inner, extra); }

        void as_slave() override { this->make_input(inner, extra); }
    };

    nested_test nested;

    // 验证嵌套Bundle宽度计算
    REQUIRE(nested.width() == 13); // 9 (inner) + 4 (extra)
    STATIC_REQUIRE(bundle_width_v<nested_test> == 13);
}

TEST_CASE("BundleSerialization - BitsView", "[bundle][serialization][view]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    test_simple_bundle bundle;
    auto bits_view = ch::core::to_bits(bundle);

    REQUIRE(bits_view.width == 9);
}

TEST_CASE("BundleSerialization - TypeTraits",
          "[bundle][serialization][traits]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    STATIC_REQUIRE(is_bundle_v<test_simple_bundle> == true);
    STATIC_REQUIRE(is_bundle_v<ch_uint<8>> == false);

    REQUIRE(get_bundle_width<test_simple_bundle>() > 0);
}

TEST_CASE("BundleSerialization - StreamBundleWidth",
          "[bundle][serialization][stream]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    Stream<ch_uint<8>> stream8;
    Stream<ch_uint<16>> stream16;
    Stream<ch_uint<32>> stream32;

    REQUIRE(stream8.width() == 10);  // 8 + 1 + 1
    REQUIRE(stream16.width() == 18); // 16 + 1 + 1
    REQUIRE(stream32.width() == 34); // 32 + 1 + 1
}

TEST_CASE("BundleSerialization - ToBitsConversion",
          "[bundle][serialization][conversion]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    test_simple_bundle bundle;

    // 测试位转换方法存在性（编译期检查）
    REQUIRE(bundle.width() == 9);

    // 测试新的序列化接口
    auto serialized = serialize(bundle);
    REQUIRE(serialized.width == 9);

    // 测试反序列化
    auto deserialized = deserialize<test_simple_bundle>(serialized);

    // 注意：实际的to_bits()实现需要访问底层节点，
    // 这里只是验证API存在性
}

TEST_CASE("BundleSerialization - FieldWidthCalculation",
          "[bundle][serialization][field]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试不同字段类型的宽度计算
    STATIC_REQUIRE(get_field_width<ch_uint<1>>() == 1);
    STATIC_REQUIRE(get_field_width<ch_uint<8>>() == 8);
    STATIC_REQUIRE(get_field_width<ch_uint<16>>() == 16);
    STATIC_REQUIRE(get_field_width<ch_uint<32>>() == 32);
    STATIC_REQUIRE(get_field_width<ch_bool>() == 1);
}

TEST_CASE("BundleSerialization - ProtocolIntegration",
          "[bundle][serialization][protocol]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    Stream<ch_uint<32>> stream;

    // 验证序列化与协议验证的集成
    REQUIRE(is_handshake_protocol_v<Stream<ch_uint<32>>> == true);
    REQUIRE(stream.width() == 34);

    // 宽度应该与字段总和一致
    STATIC_REQUIRE(get_field_width<ch_uint<32>>() + get_field_width<ch_bool>() +
                       get_field_width<ch_bool>() ==
                   34);
}