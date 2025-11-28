#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "core/context.h"
#include "core/operators.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/reg.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// 创建一个简单的测试组件，用于测试操作结果
class TestOpsComponent : public ch::Component {
public:
    __io(
        ch_out<ch_uint<16>> result_out;
    )

    TestOpsComponent(ch::Component* parent = nullptr, const std::string& name = "test_ops")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type();
    }

    void describe() override {
        // 简单的描述，主要用于测试
    }
};

// 测试各种操作的运行结果正确性
TEST_CASE("Operation Result Correctness", "[operation][result][runtime]") {
    SECTION("Arithmetic Operations") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_uint<8> a(12);  // 0b00001100
        ch_uint<8> b(5);   // 0b00000101
        
        // 加法: 12 + 5 = 17
        auto add_result = a + b;
        device.instance().io().result_out = add_result;
        simulator.eval();
        auto add_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(add_value) == 17);
        
        // 减法: 12 - 5 = 7
        auto sub_result = a - b;
        device.instance().io().result_out = sub_result;
        simulator.eval();
        auto sub_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(sub_value) == 7);
        
        // 乘法: 12 * 5 = 60
        auto mul_result = a * b;
        device.instance().io().result_out = mul_result;
        simulator.eval();
        auto mul_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(mul_value) == 60);
        
        // 负号: -12 = -12 (补码形式)
        auto neg_result = -a;
        device.instance().io().result_out = neg_result;
        simulator.eval();
        auto neg_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<int64_t>(static_cast<uint64_t>(neg_value)) == -12);
    }

    SECTION("Bitwise Operations") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_uint<8> a(12);  // 0b00001100
        ch_uint<8> b(5);   // 0b00000101
        
        // 按位与: 12 & 5 = 0b00000100 = 4
        auto and_result = a & b;
        device.instance().io().result_out = and_result;
        simulator.eval();
        auto and_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(and_value) == 4);
        
        // 按位或: 12 | 5 = 0b00001101 = 13
        auto or_result = a | b;
        device.instance().io().result_out = or_result;
        simulator.eval();
        auto or_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(or_value) == 13);
        
        // 按位异或: 12 ^ 5 = 0b00001001 = 9
        auto xor_result = a ^ b;
        device.instance().io().result_out = xor_result;
        simulator.eval();
        auto xor_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(xor_value) == 9);
        
        // 按位取反: ~12 = 0b11110011 = 243
        auto not_result = ~a;
        device.instance().io().result_out = not_result;
        simulator.eval();
        auto not_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(not_value) == 243);
    }

    SECTION("Comparison Operations") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_uint<8> a(12);
        ch_uint<8> b(5);
        ch_uint<8> c(12);
        
        // 等于: 12 == 12 = true
        auto eq_result = (a == c);
        device.instance().io().result_out = eq_result;
        simulator.eval();
        auto eq_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(eq_value) == 1);
        
        // 不等于: 12 != 5 = true
        auto ne_result = (a != b);
        device.instance().io().result_out = ne_result;
        simulator.eval();
        auto ne_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(ne_value) == 1);
        
        // 大于: 12 > 5 = true
        auto gt_result = (a > b);
        device.instance().io().result_out = gt_result;
        simulator.eval();
        auto gt_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(gt_value) == 1);
        
        // 大于等于: 12 >= 12 = true
        auto ge_result = (a >= c);
        device.instance().io().result_out = ge_result;
        simulator.eval();
        auto ge_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(ge_value) == 1);
        
        // 小于: 5 < 12 = true
        auto lt_result = (b < a);
        device.instance().io().result_out = lt_result;
        simulator.eval();
        auto lt_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(lt_value) == 1);
        
        // 小于等于: 5 <= 12 = true
        auto le_result = (b <= a);
        device.instance().io().result_out = le_result;
        simulator.eval();
        auto le_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(le_value) == 1);
    }

    SECTION("Shift Operations") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_uint<8> a(12);  // 0b00001100
        
        // 左移: 12 << 2 = 0b00110000 = 48
        auto shl_result = a << 2;
        device.instance().io().result_out = shl_result;
        simulator.eval();
        auto shl_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(shl_value) == 48);
        
        // 右移: 12 >> 1 = 0b00000110 = 6
        auto shr_result = a >> 1;
        device.instance().io().result_out = shr_result;
        simulator.eval();
        auto shr_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(shr_value) == 6);
    }

    SECTION("Bit Operations") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_uint<8> a(0b10110101);
        
        // 位提取: bits[6:2] = 0b11010 = 26
        auto bits_result = bits<ch_uint<8>, 6, 2>(a);
        device.instance().io().result_out = bits_result;
        simulator.eval();
        auto bits_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(bits_value) == 26);
    }

    SECTION("Concatenation Operation") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_uint<3> a(0b101);   // 5
        ch_uint<5> b(0b11010); // 26
        
        // 连接: 0b10111010 = 186
        auto concat_result = concat(a, b);
        device.instance().io().result_out = concat_result;
        simulator.eval();
        auto concat_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(concat_value) == 186);
    }

    SECTION("Extension Operations") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_uint<3> a(0b101); // -3 in signed 3-bit
        
        // 零扩展: 0b00000101 = 5
        auto zext_result = zext<ch_uint<3>, 8>(a);
        device.instance().io().result_out = zext_result;
        simulator.eval();
        auto zext_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(zext_value) == 5);
        
        // 符号扩展: 0b11111101 = 253 (补码 -3)
        auto sext_result = sext<ch_uint<3>, 8>(a);
        device.instance().io().result_out = sext_result;
        simulator.eval();
        auto sext_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(sext_value) == 253);
    }

    SECTION("Reduction Operations") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_uint<8> a(0b10110101);
        
        // 与归约: 1 & 0 & 1 & 1 & 0 & 1 & 0 & 1 = 0
        auto and_reduce_result = and_reduce(a);
        device.instance().io().result_out = and_reduce_result;
        simulator.eval();
        auto and_reduce_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(and_reduce_value) == 0);
        
        // 或归约: 1 | 0 | 1 | 1 | 0 | 1 | 0 | 1 = 1
        auto or_reduce_result = or_reduce(a);
        device.instance().io().result_out = or_reduce_result;
        simulator.eval();
        auto or_reduce_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(or_reduce_value) == 1);
        
        // 异或归约: 1 ^ 0 ^ 1 ^ 1 ^ 0 ^ 1 ^ 0 ^ 1 = 1
        auto xor_reduce_result = xor_reduce(a);
        device.instance().io().result_out = xor_reduce_result;
        simulator.eval();
        auto xor_reduce_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(xor_reduce_value) == 1);
    }

    SECTION("Mux Operation") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_bool cond(true);
        ch_uint<8> a(12);
        ch_uint<8> b(5);
        
        // 多路选择器: true ? 12 : 5 = 12
        auto mux_result = select(cond, a, b);
        device.instance().io().result_out = mux_result;
        simulator.eval();
        auto mux_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(mux_value) == 12);
    }
}

// 测试寄存器操作的运行结果正确性
TEST_CASE("Register Operation Results", "[operation][register][runtime]") {
    SECTION("Register Assignment and Operations") {
        // 创建设备和模拟器
        ch_device<TestOpsComponent> device;
        Simulator simulator(device.context());
        
        ch_reg<ch_uint<8>> reg_a(10);
        ch_reg<ch_uint<8>> reg_b(5);
        
        // 寄存器加法: 10 + 5 = 15
        auto reg_add_result = reg_a + reg_b;
        device.instance().io().result_out = reg_add_result;
        simulator.eval();
        auto reg_add_value = simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(reg_add_value) == 15);
    }
}