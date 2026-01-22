// tests/test_nested_bundle.cpp
#define CATCH_CONFIG_MAIN
#include "bundle/axi_bundle.h"
#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_traits.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/uint.h"
#include <memory>

using namespace ch;
using namespace ch::core;

TEST_CASE("NestedBundle - BundleTraits", "[bundle][traits]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试类型特征
    STATIC_REQUIRE(is_bundle_v<Stream<ch_uint<8>>> == true);
    STATIC_REQUIRE(is_bundle_v<ch_uint<8>> == false);
    STATIC_REQUIRE(is_bundle_v<ch_bool> == false);

    // 测试字段计数
    STATIC_REQUIRE(bundle_field_count_v<Stream<ch_uint<8>>> == 3);
}

TEST_CASE("NestedBundle - SimpleNested", "[bundle][nested]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 创建嵌套的Stream Bundle
    struct NestedTest : public bundle_base<NestedTest> {
        using Self = NestedTest;
        Stream<ch_uint<16>> inner_stream;
        ch_bool status;

        NestedTest() = default;
        NestedTest(const std::string &prefix) { this->set_name_prefix(prefix); }

        CH_BUNDLE_FIELDS_T(inner_stream, status)

        void as_master_direction() { this->make_output(inner_stream, status); }

        void as_slave_direction() { this->make_input(inner_stream, status); }
    };

    NestedTest nested("test.nested");

    REQUIRE(nested.is_valid());
    REQUIRE(is_bundle_v<NestedTest> == true);
}

TEST_CASE("NestedBundle - AXIBundleCreation", "[bundle][axi]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试AXI通道Bundle
    axi_addr_channel<32> addr_chan("axi.aw");
    axi_write_data_channel<32> data_chan("axi.w");
    axi_write_resp_channel resp_chan("axi.b");

    REQUIRE(addr_chan.is_valid());
    REQUIRE(data_chan.is_valid());
    REQUIRE(resp_chan.is_valid());
}

TEST_CASE("NestedBundle - FullAXIWrite", "[bundle][axi][nested]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试完整的AXI写通道 (嵌套Bundle)
    axi_write_channel<32, 32> axi_write("axi.write");

    REQUIRE(axi_write.is_valid());
    REQUIRE(is_bundle_v<axi_write_channel<32, 32>> == true);

    // 验证嵌套字段数量
    STATIC_REQUIRE(bundle_field_count_v<axi_write_channel<32, 32>> == 3);
}

TEST_CASE("NestedBundle - FlipNested", "[bundle][nested][flip]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    axi_write_channel<32, 32> master_axi("master.axi");
    auto slave_axi = master_axi.flip();

    REQUIRE(slave_axi != nullptr);
    REQUIRE(slave_axi->is_valid());
}

TEST_CASE("NestedBundle - ConnectNested", "[bundle][nested][connect]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    axi_write_channel<32, 32> src_axi;
    axi_write_channel<32, 32> dst_axi;

    // 尝试连接
    dst_axi <<= src_axi;

    REQUIRE(dst_axi.is_valid());
}
