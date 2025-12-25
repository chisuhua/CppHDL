#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/uint.h"
#include "simulator.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// 测试字面量作为左操作数的左移操作
TEST_CASE("Literal Left Shift Operations", "[literal][shift][operations]") {
    context ctx("literal_shift_test");
    ctx_swap swap(&ctx);

    SECTION("Literal left shift with different widths") {
        // 测试不同位宽的字面量左移操作
        ch_uint<8> shift_amount1(2_d);
        ch_uint<4> shift_amount2(3_d);

        // 字面量左移，右操作数是变量
        auto result1 = 1_d << shift_amount1; // 1 左移 2 位 = 4
        auto result2 = 3_d << shift_amount2; // 3 左移 3 位 = 24

        REQUIRE(result1.impl() != nullptr);
        REQUIRE(result2.impl() != nullptr);

        // 验证结果位宽（现在左移操作保持左操作数的位宽）
        STATIC_REQUIRE(ch_width_v<decltype(result1)> ==
                       1); // 1_d是1位，结果保持1位
        STATIC_REQUIRE(ch_width_v<decltype(result2)> ==
                       2); // 3_d是2位，结果保持2位
    }

    SECTION("Literal left shift runtime value verification") {
        // 为了验证运行时值，我们需要创建一个组件
        struct TestComponent : public ch::Component {
            __io(ch_in<ch_uint<8>> shift_val; ch_out<ch_uint<16>> result_out;)

                TestComponent(
                    ch::Component *parent = nullptr,
                    const std::string &name = "literal_shift_test_comp")
                : ch::Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type(); }

            void describe() override {
                // 使用一个固定位宽的值作为左操作数，然后左移
                // 使用 ch_uint<16> 确保有足够的位宽来容纳结果
                ch_uint<16> base_val(5);
                io().result_out = shl<16>(base_val, io().shift_val);
            }
        };

        ch_device<TestComponent> device;
        Simulator simulator(device.context());

        // 测试不同的移位值
        simulator.set_port_value(device.instance().io().shift_val, 1);
        simulator.tick();
        auto result1 =
            simulator.get_port_value(device.instance().io().result_out);
        CHECK(static_cast<uint64_t>(result1) == 10); // 5 << 1 = 10

        simulator.set_port_value(device.instance().io().shift_val, 2);
        simulator.tick();
        auto result2 =
            simulator.get_port_value(device.instance().io().result_out);
        CHECK(static_cast<uint64_t>(result2) == 20); // 5 << 2 = 20

        simulator.set_port_value(device.instance().io().shift_val, 3);
        simulator.tick();
        auto result3 =
            simulator.get_port_value(device.instance().io().result_out);
        CHECK(static_cast<uint64_t>(result3) == 40); // 5 << 3 = 40
    }

    SECTION("Literal left shift compile-time width verification") {
        // 验证不同字面量的位宽
        auto lit1 = 1_d;   // 1位宽 (2^0=1)
        auto lit7 = 7_d;   // 3位宽 (2^2 < 7 <= 2^3-1)
        auto lit15 = 15_d; // 4位宽 (2^3 < 15 <= 2^4-1)

        ch_uint<2> shift2(2_d);
        ch_uint<3> shift3(3_d);
        ch_uint<4> shift4(4_d);

        // 左移操作结果的位宽应为左操作数的位宽（保持不变）
        auto result1 = lit1 << shift2;   // 1位宽字面量左移
        auto result7 = lit7 << shift3;   // 3位宽字面量左移
        auto result15 = lit15 << shift4; // 4位宽字面量左移

        // 验证结果的位宽（保持左操作数的位宽）
        STATIC_REQUIRE(ch_width_v<decltype(result1)> == 1);  // 保持1位
        STATIC_REQUIRE(ch_width_v<decltype(result7)> == 3);  // 保持3位
        STATIC_REQUIRE(ch_width_v<decltype(result15)> == 4); // 保持4位

        REQUIRE(ch_width_v<decltype(lit1)> == 1);
        REQUIRE(ch_width_v<decltype(lit7)> == 3);
        REQUIRE(ch_width_v<decltype(lit15)> == 4);
    }

    SECTION("Literal left shift with literal shift amount") {
        // 测试两个字面量之间的左移操作
        auto result = 3_d << 2_d; // 3 左移 2 位 = 12

        // 这种操作应该能够正确编译
        REQUIRE(result.impl() != nullptr);

        // 创建一个组件来测试运行时值
        struct LiteralTestComponent : public ch::Component {
            __io(ch_out<ch_uint<16>> result_out;)

                LiteralTestComponent(
                    ch::Component *parent = nullptr,
                    const std::string &name = "literal_literal_test")
                : ch::Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type(); }

            void describe() override {
                // 使用 ch_uint<8> 作为中间类型，以确保有足够的位宽
                ch_uint<8> left_val(3_d);
                ch_uint<8> right_val(2_d);
                io().result_out = left_val << right_val;
            }
        };

        ch_device<LiteralTestComponent> device;
        Simulator simulator(device.context());
        simulator.tick();

        auto result_val =
            simulator.get_port_value(device.instance().io().result_out);
        CHECK(static_cast<uint64_t>(result_val) == 12); // 3 << 2 = 12
    }

    SECTION("Left shift with max width template parameter") {
        // 测试带最大位宽参数的左移操作
        ch_uint<4> shift_val(2_d); // 移位2位

        // 使用最大位宽为8的左移操作
        auto result8 = shl<8>(5_d, shift_val); // 5_d是3位宽
        // 结果应该是8位宽，因为最大位宽8大于左操作数位宽3
        STATIC_REQUIRE(ch_width_v<decltype(result8)> == 8);

        // 使用最大位宽为2的左移操作
        auto result2 = shl<2>(3_d, shift_val); // 3_d是2位宽
        // 结果应该是2位宽，因为最大位宽2等于左操作数位宽2
        STATIC_REQUIRE(ch_width_v<decltype(result2)> == 2);

        // 创建组件来测试运行时值
        struct MaxWidthTestComponent : public ch::Component {
            __io(ch_in<ch_uint<8>> shift_val; ch_out<ch_uint<16>> result_out8;
                 ch_out<ch_uint<16>> result_out16;)

                MaxWidthTestComponent(
                    ch::Component *parent = nullptr,
                    const std::string &name = "max_width_test")
                : ch::Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type(); }

            void describe() override {
                // 测试不同的最大位宽
                ch_uint<8> extended_val(3_d); // 3_d扩展到8位宽
                io().result_out8 = shl<8>(extended_val, io().shift_val);
                io().result_out16 = shl<16>(extended_val, io().shift_val);
            }
        };

        ch_device<MaxWidthTestComponent> device;
        Simulator simulator(device.context());

        // 设置移位值为2
        simulator.set_port_value(device.instance().io().shift_val, 2);
        simulator.tick();

        auto result_out8 =
            simulator.get_port_value(device.instance().io().result_out8);
        auto result_out16 =
            simulator.get_port_value(device.instance().io().result_out16);

        // 两者应该有相同的值，但位宽不同
        CHECK(static_cast<uint64_t>(result_out8) ==
              static_cast<uint64_t>(result_out16));
        CHECK(static_cast<uint64_t>(result_out8) == 12); // 3 << 2 = 12
    }

    SECTION("Left shift with explicit result width template parameter") {
        // 测试带结果位宽参数的左移操作
        ch_uint<4> shift_val(2_d); // 移位2位

        // 使用结果位宽为8的左移操作
        auto result8 = shl<8>(5_d, shift_val); // 5_d是3位宽
        // 结果应该是8位宽，这是明确指定的
        STATIC_REQUIRE(ch_width_v<decltype(result8)> == 8);

        // 使用结果位宽为4的左移操作
        auto result4 = shl<4>(3_d, shift_val); // 3_d是2位宽
        // 结果应该是4位宽，这是明确指定的
        STATIC_REQUIRE(ch_width_v<decltype(result4)> == 4);

        // 创建组件来测试运行时值
        struct ExplicitWidthTestComponent : public ch::Component {
            __io(ch_in<ch_uint<8>> shift_val; ch_out<ch_uint<16>> result_out8;
                 ch_out<ch_uint<16>> result_out12;)

                ExplicitWidthTestComponent(
                    ch::Component *parent = nullptr,
                    const std::string &name = "explicit_width_test")
                : ch::Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type(); }

            void describe() override {
                // 测试不同的结果位宽
                ch_uint<8> extended_val(3_d); // 3_d扩展到8位宽
                io().result_out8 = shl<8>(extended_val, io().shift_val);
                io().result_out12 = shl<12>(extended_val, io().shift_val);
            }
        };

        ch_device<ExplicitWidthTestComponent> device;
        Simulator simulator(device.context());

        // 设置移位值为2
        simulator.set_port_value(device.instance().io().shift_val, 2);
        simulator.tick();

        auto result_out8 =
            simulator.get_port_value(device.instance().io().result_out8);
        auto result_out12 =
            simulator.get_port_value(device.instance().io().result_out12);

        // 两者应该有相同的值，但位宽不同
        CHECK(static_cast<uint64_t>(result_out8) ==
              static_cast<uint64_t>(result_out12));
        CHECK(static_cast<uint64_t>(result_out8) == 12); // 3 << 2 = 12
    }
}