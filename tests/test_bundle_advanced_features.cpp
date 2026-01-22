// tests/test_bundle_advanced_features.cpp
#define CATCH_CONFIG_MAIN
#include "bundle/common_bundles.h"
#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_operations.h"
#include "core/bundle/bundle_protocol.h"
#include "core/bundle/bundle_traits.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/uint.h"
#include <memory>

using namespace ch;
using namespace ch::core;

TEST_CASE("BundleAdvanced - CommonBundles", "[bundle][common]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试FIFO Bundle
    fifo_bundle<ch_uint<32>> fifo("fifo");
    fifo.as_master();
    REQUIRE(fifo.is_valid());

    // 测试中断Bundle
    interrupt_bundle irq("irq");
    irq.as_slave();
    REQUIRE(irq.is_valid());

    // 测试配置Bundle
    config_bundle<8, 32> config("config");
    config.as_slave();
    REQUIRE(config.is_valid());
}

TEST_CASE("BundleAdvanced - ProtocolValidation", "[bundle][protocol]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    Stream<ch_uint<32>> stream;

    // 验证HandShake协议
    STATIC_REQUIRE(is_handshake_protocol_v<Stream<ch_uint<32>>> == true);
    STATIC_REQUIRE(is_handshake_protocol_v<ch_uint<32>> == false);

    // 验证字段检查
    STATIC_REQUIRE(
        has_field_named_v<Stream<ch_uint<32>>, structural_string{"payload"}> ==
        true);
    STATIC_REQUIRE(
        has_field_named_v<Stream<ch_uint<32>>, structural_string{"valid"}> ==
        true);
    STATIC_REQUIRE(
        has_field_named_v<Stream<ch_uint<32>>, structural_string{"ready"}> ==
        true);
    STATIC_REQUIRE(has_field_named_v<Stream<ch_uint<32>>,
                                     structural_string{"nonexistent"}> ==
                   false);
}
/*
TEST_CASE("BundleAdvanced - BundleOperations", "[bundle][operations]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试Bundle连接 (简化版)
    Stream<ch_uint<16>> stream1;
    Stream<ch_uint<16>> stream2;

    auto concat_bundle = ch::core::bundle_cat(stream1, stream2);
    REQUIRE(concat_bundle.is_valid());
}
*/

TEST_CASE("BundleAdvanced - TypeTraits", "[bundle][traits]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    Stream<ch_uint<8>> stream;
    ch_uint<8> regular_type;

    // 测试类型特征
    STATIC_REQUIRE(is_bundle_v<Stream<ch_uint<8>>> == true);
    STATIC_REQUIRE(is_bundle_v<ch_uint<8>> == false);

    // 测试字段计数
    STATIC_REQUIRE(bundle_field_count_v<Stream<ch_uint<8>>> == 3);
    STATIC_REQUIRE(bundle_field_count_v<fifo_bundle<ch_uint<32>>> == 5);
}

TEST_CASE("BundleAdvanced - NamingIntegration", "[bundle][naming]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试常用Bundle的命名
    fifo_bundle<ch_uint<16>> fifo("top.fifo");
    interrupt_bundle irq("top.irq");
    config_bundle<8, 32> config("top.config");

    fifo.as_master();
    irq.as_master();
    config.as_master();
    REQUIRE(fifo.is_valid());
    REQUIRE(irq.is_valid());
    REQUIRE(config.is_valid());
}

TEST_CASE("BundleAdvanced - ProtocolCheckFunction",
          "[bundle][protocol][function]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    Stream<ch_uint<32>> stream;

    // 测试协议验证函数 (编译期检查)
    validate_handshake_protocol<decltype(stream)>();
    REQUIRE(true); // 如果能编译就说明通过验证
}
