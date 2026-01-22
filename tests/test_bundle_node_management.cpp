#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_meta.h" // 添加bundle_meta
#include "core/bundle/bundle_serialization.h"
#include "core/bundle/bundle_traits.h" // 添加bundle_traits
#include "core/context.h"
#include "core/io.h"
#include "core/uint.h"
#include "simulator.h" // 添加Simulator的头文件

using namespace ch::core;

// 定义一个简单的Bundle用于测试
struct TestBundle : public bundle_base<TestBundle> {
    using Self = TestBundle;
    ch_uint<8> data;
    ch_bool valid;

    TestBundle() = default;

    CH_BUNDLE_FIELDS_T(data, valid)

    void as_master_direction() { this->make_output(data, valid); }

    void as_slave_direction() { this->make_input(data, valid); }
};

// 一个独立的测试函数，验证is_bundle_safe是否能正确检测is_bundle1
template <typename T> void test_is_bundle_safe_trait() {
    static_assert(is_bundle_v<T> == true,
                  "is_bundle_v should detect is_bundle1 as true");
    std::cout << "is_bundle_v<T>: " << is_bundle_v<T> << std::endl;
}

TEST_CASE("TestBundle node management") {
    SECTION("Default construction creates valid node") {
        context ctx;
        ctx_swap cs(&ctx);

        TestBundle bundle;
        REQUIRE(bundle.impl() == nullptr);
        REQUIRE(bundle_width_v<TestBundle> == 9); // 8 + 1 bits
        REQUIRE(bundle.width() == 9);
    }

    SECTION("Literal construction with ch_literal_impl") {
        context ctx;
        ctx_swap cs(&ctx);

        TestBundle bundle1(0x55_h); // 使用字面值构造
        REQUIRE(bundle1.impl() != nullptr);
        REQUIRE(bundle1.width() == 9);
    }

    SECTION("Copy construction shares node") {
        context ctx;
        ctx_swap cs(&ctx);

        TestBundle bundle1(0x55_h);
        TestBundle bundle2 = bundle1; // Copy construction
        REQUIRE(bundle1.impl() != nullptr);
        REQUIRE(bundle1.impl() == bundle2.impl()); // Same node shared
        REQUIRE(bundle2.width() == 9);
    }

    SECTION("Assignment shares node") {
        context ctx;
        ctx_swap cs(&ctx);

        TestBundle bundle1(0x55_h);
        TestBundle bundle2;
        bundle2 = bundle1; // Assignment
        REQUIRE(bundle1.impl() != nullptr);
        REQUIRE(bundle1.impl() == bundle2.impl()); // Same node shared
        REQUIRE(bundle2.width() == 9);
    }
}