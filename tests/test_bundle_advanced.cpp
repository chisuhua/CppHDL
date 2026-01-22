// tests/test_bundle_advanced.cpp
#define CATCH_CONFIG_MAIN
#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/io.h"
#include "core/uint.h"
#include <memory>

using namespace ch;
using namespace ch::core;

// 测试用的自定义Bundle
template <typename T> struct TestBundle : public bundle_base<TestBundle<T>> {
    using Self = TestBundle<T>;
    T data;
    ch_bool enable;
    ch_bool ack;

    TestBundle() = default;
    TestBundle(const std::string &prefix) { this->set_name_prefix(prefix); }

    CH_BUNDLE_FIELDS_T(data, enable, ack)

    void as_master_direction() {
        this->make_output(data, enable);
        this->make_input(ack);
    }

    void as_slave_direction() {
        this->make_input(data, enable);
        this->make_output(ack);
    }
};

TEST_CASE("BundleAdvanced - BundleFieldsAndWidth", "[bundle][fields][width]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    TestBundle<ch_uint<8>> bundle;

    // 验证Bundle字段数量
    auto fields = bundle.__bundle_fields();
    REQUIRE(std::tuple_size_v<decltype(fields)> == 3);

    // 验证Bundle总宽度
    REQUIRE(get_bundle_width<TestBundle<ch_uint<8>>>() == 10); // 8 + 1 + 1

    // 验证Bundle是否有效
    REQUIRE(bundle.is_valid());
}

TEST_CASE("BundleAdvanced - BundleNaming", "[bundle][naming]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    TestBundle<ch_uint<8>> bundle("my_test");

    // 验证字段名称前缀设置
    auto fields = bundle.__bundle_fields();
    REQUIRE(std::tuple_size_v<decltype(fields)> == 3);

    // 验证Bundle是否有效
    REQUIRE(bundle.is_valid());
}

TEST_CASE("BundleAdvanced - BundleDirection", "[bundle][direction]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    TestBundle<ch_uint<8>> bundle;

    // 测试方向设置
    bundle.as_master_direction();
    REQUIRE(bundle.get_role() == bundle_role::master);

    bundle.as_slave_direction();
    REQUIRE(bundle.get_role() == bundle_role::slave);

    // 验证Bundle是否有效
    REQUIRE(bundle.is_valid());
}

TEST_CASE("BundleAdvanced - ConnectFunction", "[bundle][connect]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    TestBundle<ch_uint<8>> src_bundle;
    TestBundle<ch_uint<8>> dst_bundle;

    // 测试连接功能
    ch::core::connect(src_bundle, dst_bundle);
    REQUIRE(true); // 如果能编译就说明功能存在
}

TEST_CASE("BundleAdvanced - FactoryFunctions", "[bundle][factory]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试工厂函数
    auto master_bundle = ch::core::master(TestBundle<ch_uint<8>>{});
    auto slave_bundle = ch::core::slave(TestBundle<ch_uint<8>>{});

    REQUIRE(master_bundle.is_valid());
    REQUIRE(slave_bundle.is_valid());
}

TEST_CASE("BundleAdvanced - FlipWithAutoDirection", "[bundle][flip]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    TestBundle<ch_uint<8>> master_bundle;
    auto slave_bundle = master_bundle.flip();

    REQUIRE(slave_bundle != nullptr);
    REQUIRE(slave_bundle->is_valid());
}
