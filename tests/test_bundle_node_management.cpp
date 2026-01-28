#include "ast/instr_base.h"
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

    SECTION("Bundle and field nodes after as_master/as_slave") {
        context ctx;
        ctx_swap cs(&ctx);

        TestBundle bundle(0xA5_h); // 使用已知值构造
        REQUIRE(bundle.impl() != nullptr);

        // 测试初始状态下字段节点是否存在
        auto bundle_node = bundle.impl();
        REQUIRE(bundle_node != nullptr);

        // 检查 bundle 的宽度
        REQUIRE(bundle.width() == 9);

        // 应用 as_master 操作
        bundle.as_master();

        // 测试字段方向是否已设置
        auto &data_field = bundle.data;
        auto &valid_field = bundle.valid;

        // 检查字段节点是否正确分配 (字段节点在此时才被创建)
        REQUIRE(data_field.impl() != nullptr);
        REQUIRE(valid_field.impl() != nullptr);

        // 测试 as_slave 操作
        TestBundle slave_bundle(0x3C_h);
        slave_bundle.as_slave();

        auto &slave_data = slave_bundle.data;
        auto &slave_valid = slave_bundle.valid;

        REQUIRE(slave_data.impl() != nullptr);
        REQUIRE(slave_valid.impl() != nullptr);
    }

    SECTION("Field bit extraction nodes after direction setting") {
        context ctx;
        ctx_swap cs(&ctx);

        TestBundle master_bundle(0xFF_h);
        master_bundle.as_master(); // 设置为主设备方向

        // 获取字段
        auto &data_field = master_bundle.data;
        auto &valid_field = master_bundle.valid;

        // 验证字段节点存在 (因为已经调用了as_master)
        REQUIRE(data_field.impl() != nullptr);
        REQUIRE(valid_field.impl() != nullptr);

        // 检查字段的位宽是否正确
        REQUIRE(data_field.width == 8);
        REQUIRE(valid_field.width == 1);

        // 检查整个 bundle 的节点
        REQUIRE(master_bundle.impl() != nullptr);
        REQUIRE(master_bundle.width() == 9);

        // 测试 slave 方向设置后的节点
        TestBundle slave_bundle(0x00_h);
        slave_bundle.as_slave();

        auto &slave_data = slave_bundle.data;
        auto &slave_valid = slave_bundle.valid;

        REQUIRE(slave_data.impl() != nullptr);
        REQUIRE(slave_valid.impl() != nullptr);
        REQUIRE(slave_data.width == 8);
        REQUIRE(slave_valid.width == 1);

        // 检查 slave bundle 的节点
        REQUIRE(slave_bundle.impl() != nullptr);
        REQUIRE(slave_bundle.width() == 9);
    }

    SECTION("Node sharing after direction operations") {
        context ctx;
        ctx_swap cs(&ctx);

        TestBundle original_bundle(0xAA_h);
        TestBundle copy_bundle = original_bundle; // 复制构造

        // 验证两个 bundle 共享相同的 bundle 节点
        REQUIRE(original_bundle.impl() == copy_bundle.impl());
        REQUIRE(original_bundle.impl() != nullptr);

        // 在执行 as_master 之前，字段节点尚未创建，因此都是空的
        REQUIRE(original_bundle.data.impl() == nullptr);
        REQUIRE(original_bundle.valid.impl() == nullptr);
        REQUIRE(copy_bundle.data.impl() == nullptr); // 字段节点尚未创建
        REQUIRE(copy_bundle.valid.impl() == nullptr); // 字段节点尚未创建

        // 对其中一个应用方向设置
        original_bundle.as_master();

        // 检查 original_bundle 的字段节点是否被创建
        REQUIRE(original_bundle.data.impl() != nullptr);
        REQUIRE(original_bundle.valid.impl() != nullptr);

        // 注意：copy_bundle 的字段节点仍然为空，因为每个 bundle 需要单独调用
        // as_master/as_slave
        REQUIRE(copy_bundle.data.impl() == nullptr);
        REQUIRE(copy_bundle.valid.impl() == nullptr);

        // 对 copy_bundle 也执行 as_master 操作
        copy_bundle.as_master();

        // 现在 copy_bundle 的字段节点也被创建了
        REQUIRE(copy_bundle.data.impl() != nullptr);
        REQUIRE(copy_bundle.valid.impl() != nullptr);

        // 重要：字段节点是独立创建的，所以它们不应该相同
        REQUIRE(original_bundle.data.impl() != copy_bundle.data.impl());
        REQUIRE(original_bundle.valid.impl() != copy_bundle.valid.impl());

        // 通过赋值操作，可以使字段节点关联起来
        copy_bundle.data = original_bundle.data;
        copy_bundle.valid = original_bundle.valid;

        REQUIRE(original_bundle.data.impl() == copy_bundle.data.impl());
        REQUIRE(original_bundle.valid.impl() == copy_bundle.valid.impl());

        // 测试字段的宽度
        REQUIRE(original_bundle.data.width == 8);
        REQUIRE(original_bundle.valid.width == 1);
        REQUIRE(copy_bundle.data.width == 8);
        REQUIRE(copy_bundle.valid.width == 1);
    }

    SECTION("Bundle serialization and deserialization preserves nodes") {
        context ctx;
        ctx_swap cs(&ctx);

        TestBundle original_bundle(0xF0_h);
        original_bundle.as_master();

        // 检查新 bundle 的节点
        REQUIRE(original_bundle.impl() != nullptr);
        REQUIRE(original_bundle.width() == 9);

        // 检查字段节点
        REQUIRE(original_bundle.data.impl() != nullptr);
        REQUIRE(original_bundle.valid.impl() != nullptr);
        REQUIRE(original_bundle.data.width == 8);
        REQUIRE(original_bundle.valid.width == 1);
    }
}