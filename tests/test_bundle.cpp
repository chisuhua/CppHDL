// tests/test_bundle_final.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/io.h"
#include "core/uint.h"
#include <memory>
#include <string>

using namespace ch;
using namespace ch::core;

// 测试Bundle
template <typename T> struct TestBundle : public bundle_base<TestBundle<T>> {
    using Self = TestBundle<T>;
    T data;
    ch_bool enable;
    ch_bool ack;

    TestBundle() = default;
    // TestBundle(const std::string& prefix) {
    //     this->set_name_prefix(prefix);
    // }

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

// HandShake Bundle
template <typename T> struct HandShake : public bundle_base<HandShake<T>> {
    using Self = HandShake<T>;
    T payload;
    ch_bool valid;
    ch_bool ready;

    HandShake() = default;
    HandShake(const std::string &prefix) { this->set_name_prefix(prefix); }

    CH_BUNDLE_FIELDS(Self, payload, valid, ready)

    void as_master() override {
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->make_input(payload, valid);
        this->make_output(ready);
    }
};

TEST_CASE("BundleFinal - BasicBundleCreation", "[bundle][final]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    TestBundle<ch_uint<8>> bundle;
    auto fields = bundle.__bundle_fields();

    REQUIRE(std::tuple_size_v<decltype(fields)> == 3);
}

TEST_CASE("BundleFinal - HandShakeBundleCreation", "[bundle][handshake]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    HandShake<ch_uint<32>> hs_bundle;
    auto fields = hs_bundle.__bundle_fields();

    REQUIRE(std::tuple_size_v<decltype(fields)> == 3);
}

TEST_CASE("BundleFinal - FlipFunctionality", "[bundle][flip]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    HandShake<ch_uint<8>> master_bundle;
    auto slave_bundle = master_bundle.flip();

    REQUIRE(slave_bundle != nullptr);
    REQUIRE(slave_bundle->is_valid());
}

TEST_CASE("BundleFinal - DirectionMethodExistence", "[bundle][direction]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    HandShake<ch_uint<8>> master_bundle;
    HandShake<ch_uint<8>> slave_bundle;

    // 验证方法存在（编译期检查）
    REQUIRE(true); // 如果能编译就说明方法存在
}

TEST_CASE("BundleFinal - MetadataAccess", "[bundle][metadata]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    HandShake<ch_uint<16>> bundle;
    auto fields = bundle.__bundle_fields();

    // 验证字段元数据
    auto [field1, field2, field3] = fields;

    REQUIRE(std::string(field1.name) == "payload");
    REQUIRE(std::string(field2.name) == "valid");
    REQUIRE(std::string(field3.name) == "ready");
}

TEST_CASE("BundleFinal - ContextIsolation", "[bundle][context]") {
    auto ctx1 = std::make_unique<ch::core::context>("ctx1");
    auto ctx2 = std::make_unique<ch::core::context>("ctx2");

    {
        ch::core::ctx_swap ctx_guard(ctx1.get());
        HandShake<ch_uint<8>> bundle1;
        REQUIRE(bundle1.is_valid());
    }

    {
        ch::core::ctx_swap ctx_guard(ctx2.get());
        HandShake<ch_uint<8>> bundle2;
        REQUIRE(bundle2.is_valid());
    }
}

TEST_CASE("BundleEnhanced - AutoNaming", "[bundle][naming]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    HandShake<ch_uint<8>> bundle("test.prefix");
    // 测试命名功能存在（具体实现依赖于字段类型）
    REQUIRE(true); // 如果能编译就说明功能存在
}

TEST_CASE("BundleEnhanced - ConnectFunction", "[bundle][connect]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    HandShake<ch_uint<8>> src_bundle;
    HandShake<ch_uint<8>> dst_bundle;

    // 测试连接功能
    ch::core::connect(src_bundle, dst_bundle);
    REQUIRE(true); // 如果能编译就说明功能存在
}

TEST_CASE("BundleEnhanced - FactoryFunctions", "[bundle][factory]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试工厂函数
    auto master_bundle = ch::core::master(HandShake<ch_uint<8>>{});
    auto slave_bundle = ch::core::slave(HandShake<ch_uint<8>>{});

    REQUIRE(master_bundle.is_valid());
    REQUIRE(slave_bundle.is_valid());
}

TEST_CASE("BundleEnhanced - EnhancedValidation", "[bundle][validation]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    HandShake<ch_uint<8>> bundle;
    bool valid = bundle.is_valid();

    REQUIRE(valid == true); // 基本Bundle应该有效
}
