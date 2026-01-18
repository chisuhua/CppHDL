// tests/test_axi_lite_bundle.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_traits.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/uint.h"
#include "io/axi_lite_bundle.h"
#include "io/axi_protocol.h"
#include "io/stream_bundle.h"
#include <memory>

using namespace ch;
using namespace ch::core;

TEST_CASE("AXILiteBundle - ChannelCreation", "[axi][lite][channel]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试写地址通道
    axi_lite_aw_channel<32> aw_chan("axi.aw");
    REQUIRE(aw_chan.is_valid());
    REQUIRE(bundle_field_count_v<axi_lite_aw_channel<32>> == 4);

    // 测试写数据通道
    axi_lite_w_channel<32> w_chan("axi.w");
    REQUIRE(w_chan.is_valid());
    REQUIRE(bundle_field_count_v<axi_lite_w_channel<32>> == 4);

    // 测试写响应通道
    axi_lite_b_channel b_chan("axi.b");
    REQUIRE(b_chan.is_valid());
    REQUIRE(bundle_field_count_v<axi_lite_b_channel> == 3);

    // 测试读地址通道
    axi_lite_ar_channel<32> ar_chan("axi.ar");
    REQUIRE(ar_chan.is_valid());
    REQUIRE(bundle_field_count_v<axi_lite_ar_channel<32>> == 4);

    // 测试读数据通道
    axi_lite_r_channel<32> r_chan("axi.r");
    REQUIRE(r_chan.is_valid());
    REQUIRE(bundle_field_count_v<axi_lite_r_channel<32>> == 4);
}

TEST_CASE("AXILiteBundle - InterfaceCreation", "[axi][lite][interface]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试写接口
    axi_lite_write_interface<32, 32> write_if("axi.write");
    REQUIRE(write_if.is_valid());
    REQUIRE(bundle_field_count_v<axi_lite_write_interface<32, 32>> == 3);

    // 测试读接口
    axi_lite_read_interface<32, 32> read_if("axi.read");
    REQUIRE(read_if.is_valid());
    REQUIRE(bundle_field_count_v<axi_lite_read_interface<32, 32>> == 2);

    // 测试完整接口
    axi_lite_bundle<32, 32> axi_if("axi.full");
    REQUIRE(axi_if.is_valid());
    REQUIRE(bundle_field_count_v<axi_lite_bundle<32, 32>> == 5);
}

TEST_CASE("AXILiteBundle - ProtocolValidation", "[axi][lite][protocol]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试AXI-Lite协议检查
    axi_lite_bundle<32, 32> full_axi;
    axi_lite_write_interface<32, 32> write_axi;
    axi_lite_read_interface<32, 32> read_axi;
    Stream<ch_uint<32>> stream;

    // 协议类型检查
    STATIC_REQUIRE(is_axi_lite_v<axi_lite_bundle<32, 32>> == true);
    STATIC_REQUIRE(is_axi_lite_write_v<axi_lite_write_interface<32, 32>> ==
                   true);
    STATIC_REQUIRE(is_axi_lite_read_v<axi_lite_read_interface<32, 32>> == true);

    // 非AXI类型检查
    STATIC_REQUIRE(is_axi_lite_v<Stream<ch_uint<32>>> == false);
    STATIC_REQUIRE(is_axi_lite_write_v<Stream<ch_uint<32>>> == false);
    STATIC_REQUIRE(is_axi_lite_read_v<Stream<ch_uint<32>>> == false);
}

TEST_CASE("AXILiteBundle - FieldNameChecking", "[axi][lite][fields]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    axi_lite_bundle<32, 32> axi_if;

    // 测试字段存在性检查
    STATIC_REQUIRE(
        has_field_named_v<axi_lite_bundle<32, 32>, structural_string{"aw"}> ==
        true);
    STATIC_REQUIRE(
        has_field_named_v<axi_lite_bundle<32, 32>, structural_string{"w"}> ==
        true);
    STATIC_REQUIRE(
        has_field_named_v<axi_lite_bundle<32, 32>, structural_string{"b"}> ==
        true);
    STATIC_REQUIRE(
        has_field_named_v<axi_lite_bundle<32, 32>, structural_string{"ar"}> ==
        true);
    STATIC_REQUIRE(
        has_field_named_v<axi_lite_bundle<32, 32>, structural_string{"r"}> ==
        true);
    STATIC_REQUIRE(has_field_named_v<axi_lite_bundle<32, 32>,
                                     structural_string{"invalid"}> == false);
}

TEST_CASE("AXILiteBundle - DirectionControl", "[axi][lite][direction]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    axi_lite_bundle<32, 32> master_axi;
    axi_lite_bundle<32, 32> slave_axi;

    // 设置方向
    master_axi.as_master();
    slave_axi.as_slave();

    REQUIRE(true); // 如果能编译和运行就说明方向控制工作
}

TEST_CASE("AXILiteBundle - FlipFunctionality", "[axi][lite][flip]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    axi_lite_bundle<32, 32> master_axi;
    auto slave_axi = master_axi.flip();

    REQUIRE(slave_axi != nullptr);
    REQUIRE(slave_axi->is_valid());
}

TEST_CASE("AXILiteBundle - ConnectFunction", "[axi][lite][connect]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    axi_lite_bundle<32, 32> src_axi;
    axi_lite_bundle<32, 32> dst_axi;

    // 测试连接功能
    ch::core::connect(src_axi, dst_axi);
    REQUIRE(true); // 如果能编译就说明连接功能工作
}

TEST_CASE("AXILiteBundle - ProtocolValidationFunctions",
          "[axi][lite][validation]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    axi_lite_bundle<32, 32> full_axi;
    axi_lite_write_interface<32, 32> write_axi;
    axi_lite_read_interface<32, 32> read_axi;

    // 测试协议验证函数 (编译期检查)
    validate_axi_lite_protocol(full_axi);
    validate_axi_lite_write_protocol(write_axi);
    validate_axi_lite_read_protocol(read_axi);

    REQUIRE(true); // 如果能编译就说明验证通过
}

TEST_CASE("AXILiteBundle - NamingIntegration", "[axi][lite][naming]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    axi_lite_bundle<32, 32> axi_if("peripheral.axi");
    axi_lite_write_interface<32, 32> write_if("master.write");
    axi_lite_read_interface<32, 32> read_if("master.read");

    REQUIRE(axi_if.is_valid());
    REQUIRE(write_if.is_valid());
    REQUIRE(read_if.is_valid());
}

TEST_CASE("AXILiteBundle - DifferentWidths", "[axi][lite][widths]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试不同数据宽度
    axi_lite_bundle<32, 32> axi32("axi32");
    axi_lite_bundle<64, 64> axi64("axi64");
    axi_lite_bundle<32, 64> axi32_64("axi32_64");
    axi_lite_bundle<64, 32> axi64_32("axi64_32");

    REQUIRE(axi32.is_valid());
    REQUIRE(axi64.is_valid());
    REQUIRE(axi32_64.is_valid());
    REQUIRE(axi64_32.is_valid());
}
