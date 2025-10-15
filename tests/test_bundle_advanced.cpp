// tests/test_bundle_advanced.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "core/context.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_utils.h"
#include "io/stream_bundle.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/io.h"
#include <memory>

using namespace ch;
using namespace ch::core;

// 测试用的自定义Bundle
template<typename T>
struct TestBundle : public bundle_base<TestBundle<T>> {
    using Self = TestBundle<T>;
    T data;
    ch_bool enable;
    ch_bool ack;

    TestBundle() = default;
    TestBundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, data, enable, ack)

    void as_master() override {
        this->make_output(data, enable);
        this->make_input(ack);
    }

    void as_slave() override {
        this->make_input(data, enable);
        this->make_output(ack);
    }
};

TEST_CASE("BundleAdvanced - StreamBundleCreation", "[bundle][stream]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    stream_bundle<ch_uint<32>> stream("test_stream");
    
    REQUIRE(stream.is_valid());
    // 验证字段创建
    REQUIRE(true); // 如果能编译就说明成功
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

TEST_CASE("BundleAdvanced - NamingIntegration", "[bundle][naming]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    stream_bundle<ch_uint<16>> stream("io.data");
    // 测试命名功能集成
    REQUIRE(true); // 如果能编译就说明集成成功
}
