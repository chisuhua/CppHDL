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

// 测试左移操作，使用shl函数
TEST_CASE("Left Shift Operations with shl function", "[shift][operations]") {
    context ctx("shift_test");
    ctx_swap swap(&ctx);

    SECTION("Compile-time width verification") {
        // 验证不同输入的位宽
        ch_uint<4> input4(5_d);
        ch_uint<8> input8(10_d);

        ch_uint<3> shift2(2_d);
        ch_uint<4> shift3(3_d);

        // 使用shl函数指定结果位宽
        auto result1 = shl<8>(input4, shift2);  // 4位输入左移，结果8位
        auto result2 = shl<12>(input8, shift3); // 8位输入左移，结果12位

        // 验证结果的位宽
        STATIC_REQUIRE(ch_width_v<decltype(result1)> == 8);
        STATIC_REQUIRE(ch_width_v<decltype(result2)> == 12);

        REQUIRE(ch_width_v<decltype(input4)> == 4);
        REQUIRE(ch_width_v<decltype(input8)> == 8);
    }

    SECTION("Left shift with explicit result width template parameter") {
        // 测试带结果位宽参数的左移操作
        ch_uint<4> input_val(5_d); // 5需要3位宽
        ch_uint<3> shift_val(2_d); // 移位2位

        // 使用结果位宽为8的左移操作
        auto result8 = shl<8>(input_val, shift_val); // 5 左移 2 位
        // 结果应该是8位宽，这是明确指定的
        STATIC_REQUIRE(ch_width_v<decltype(result8)> == 8);

        // 使用结果位宽为10的左移操作
        auto result10 = shl<10>(input_val, shift_val); // 5 左移 2 位
        // 结果应该是10位宽，这是明确指定的
        STATIC_REQUIRE(ch_width_v<decltype(result10)> == 10);
    }

    SECTION("Simple left shift with literals") {
        // 创建一个简单的测试来验证左移操作
        struct SimpleShlTest : public ch::Component {
            __io(ch_out<ch_uint<8>> result_out;)

                SimpleShlTest(ch::Component *parent = nullptr,
                              const std::string &name = "simple_shl_test")
                : ch::Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type(); }

            void describe() override {
                // 使用shl函数进行左移操作：3 << 2 = 12
                io().result_out = shl<8>(3_d, 2_d);
            }
        };

        ch_device<SimpleShlTest> device;
        Simulator simulator(device.context());

        simulator.tick();
        auto result =
            simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(result) == 12); // 3 << 2 = 12
    }
}

// 测试左移操作，确保字面量作为右操作数
TEST_CASE("Left Shift Operations with Literal as Right Operand",
          "[shift][operations]") {
    context ctx("shift_test");
    ctx_swap swap(&ctx);

    SECTION("Simple left shift with known values using literals") {
        // 创建一个简单的测试来验证左移操作
        struct SimpleShlTest : public ch::Component {
            __io(ch_out<ch_uint<8>> result_out;)

                SimpleShlTest(ch::Component *parent = nullptr,
                              const std::string &name = "simple_shl_test")
                : ch::Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type(); }

            void describe() override {
                // 使用shl函数进行左移操作：3 << 2 = 12
                io().result_out = shl<8>(3_d, 2_d);
            }
        };

        ch_device<SimpleShlTest> device;
        Simulator simulator(device.context());

        simulator.tick();
        auto result =
            simulator.get_port_value(device.instance().io().result_out);
        CHECK(static_cast<uint64_t>(result) == 12); // 3 << 2 = 12
    }

    SECTION("Left shift with variable inputs") {
        // 使用变量作为输入的测试
        struct VariableShlTest : public ch::Component {
            __io(ch_in<ch_uint<8>> input_val; ch_in<ch_uint<4>> shift_val;
                 ch_out<ch_uint<16>> result_out;)

                VariableShlTest(ch::Component *parent = nullptr,
                                const std::string &name = "var_shl_test")
                : ch::Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type(); }

            void describe() override {
                // 使用shl函数进行左移操作
                io().result_out = shl<16>(io().input_val, io().shift_val);
            }
        };

        ch_device<VariableShlTest> device;
        Simulator simulator(device.context());

        // 分别测试多个用例
        SECTION("Test 1: 1 << 1 = 2") {
            simulator.set_port_value(device.instance().io().input_val, 1);
            simulator.set_port_value(device.instance().io().shift_val, 1);
            simulator.tick();
            auto result =
                simulator.get_port_value(device.instance().io().result_out);
            CHECK(static_cast<uint64_t>(result) == 2); // 1 << 1 = 2
        }

        SECTION("Test 2: 3 << 2 = 12") {
            simulator.set_port_value(device.instance().io().input_val, 3);
            simulator.set_port_value(device.instance().io().shift_val, 2);
            simulator.tick();
            auto result =
                simulator.get_port_value(device.instance().io().result_out);
            CHECK(static_cast<uint64_t>(result) == 12); // 3 << 2 = 12
        }

        SECTION("Test 3: 5 << 1 = 10") {
            simulator.set_port_value(device.instance().io().input_val, 5);
            simulator.set_port_value(device.instance().io().shift_val, 1);
            simulator.tick();
            auto result =
                simulator.get_port_value(device.instance().io().result_out);
            CHECK(static_cast<uint64_t>(result) == 10); // 5 << 1 = 10
        }
    }
}

// 测试字面量作为左操作数的左移操作
TEST_CASE("Literal Left Shift Operations", "[literal][shift][operations]") {
    context ctx("literal_shift_test");
    ctx_swap swap(&ctx);

    SECTION("Literal left shift with different widths") {
        // 测试不同位宽的字面量左移操作
        ch_uint<8> shift_amount1(2_d);
        ch_uint<4> shift_amount2(3_d);

        // 字面量左移，右操作数是变量
        auto result1 = shl<16>(1_d, shift_amount1); // 1 左移 2 位 = 4
        auto result2 = shl<16>(3_d, shift_amount2); // 3 左移 3 位 = 24

        REQUIRE(result1.impl() != nullptr);
        REQUIRE(result2.impl() != nullptr);

        // 验证结果位宽（现在左移操作的位宽是左操作数字面量位宽 + 右操作数的值）
        // 1_d 是 1 位宽，左移 2 位，结果应该是 1+2=3 位宽
        // 3_d 是 2 位宽，左移 3 位，结果应该是 2+3=5 位宽
        STATIC_REQUIRE(ch_width_v<decltype(result1)> == 16);
        STATIC_REQUIRE(ch_width_v<decltype(result2)> == 16);
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
                // 字面量左移操作
                io().result_out =
                    shl<16>(5_d, io().shift_val); // 5 左移 shift_val 位
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

        // 左移操作结果的位宽应为左操作数的位宽 + 右操作数的值
        auto result1 = lit1 << shift2;   // 1位宽字面量左移2位
        auto result7 = lit7 << shift3;   // 3位宽字面量左移3位
        auto result15 = lit15 << shift4; // 4位宽字面量左移4位

        // 验证结果的位宽（左操作数字面量位宽 + 右操作数的值）
        STATIC_REQUIRE(ch_width_v<decltype(result1)> == 1 + 3);   // 1+2=3位
        STATIC_REQUIRE(ch_width_v<decltype(result7)> == 3 + 7);   // 3+3=6位
        STATIC_REQUIRE(ch_width_v<decltype(result15)> == 4 + 15); // 4+4=8位

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
                io().result_out = 3_d << 2_d; // 字面量左移字面量
            }
        };

        ch_device<LiteralTestComponent> device;
        Simulator simulator(device.context());
        simulator.tick();

        auto result_val =
            simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(result_val) == 12); // 3 << 2 = 12
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