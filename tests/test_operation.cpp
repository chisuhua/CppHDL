// tests/test_operations.cpp
#include "catch_amalgamated.hpp"
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/core/bundle.h"
#include "../include/core/context.h"
#include "../include/core/uint.h"
#include "../include/reg.h"
#include "../include/core/traits.h"
#include <type_traits>

using namespace ch::core;

// 测试用的简单组件
class TestComponent : public ch::Component {
public:
    __io(
        ch_out<ch_uint<8>> test_out;
        ch_in<ch_uint<8>> test_in;
        ch::stream<ch_uint<8>> test_stream;
    )

    TestComponent(ch::Component* parent = nullptr, const std::string& name = "test_component")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type();
    }

    void describe() override {
        // 简单的描述，主要用于测试
    }
};

// ========== ch_uint 类型测试 ==========

TEST_CASE("ch_uint: basic operations", "[uint][operations][basic]") {
    // 创建测试上下文
    auto ctx = ch::core::context::create("test_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Arithmetic operations") {
        ch_uint<8> a(12);
        ch_uint<8> b(5);
        
        auto add_result = a + b;
        auto sub_result = a - b;
        auto mul_result = a * b;
        
        REQUIRE(static_cast<uint64_t>(add_result) == 17);
        REQUIRE(static_cast<uint64_t>(sub_result) == 7);
        REQUIRE(static_cast<uint64_t>(mul_result) == 60);
    }
    
    SECTION("Bitwise operations") {
        ch_uint<8> a(12);  // 0b00001100
        ch_uint<8> b(5);   // 0b00000101
        
        auto and_result = a & b;  // 0b00000100 = 4
        auto or_result = a | b;   // 0b00001101 = 13
        auto xor_result = a ^ b;  // 0b00001001 = 9
        auto not_result = ~a;     // 0b11110011 = 243
        
        REQUIRE(static_cast<uint64_t>(and_result) == 4);
        REQUIRE(static_cast<uint64_t>(or_result) == 13);
        REQUIRE(static_cast<uint64_t>(xor_result) == 9);
        REQUIRE(static_cast<uint64_t>(not_result) == 243);
    }
    
    SECTION("Comparison operations") {
        ch_uint<8> a(12);
        ch_uint<8> b(5);
        ch_uint<8> c(12);
        
        REQUIRE(static_cast<bool>((a == c)) == true);
        REQUIRE(static_cast<bool>((a != b)) == true);
        REQUIRE(static_cast<bool>((a > b)) == true);
        REQUIRE(static_cast<bool>((a >= c)) == true);
        REQUIRE(static_cast<bool>((b < a)) == true);
        REQUIRE(static_cast<bool>((b <= a)) == true);
    }
    
    SECTION("Shift operations") {
        ch_uint<8> a(12);  // 0b00001100
        
        auto shl_result = a << 2;  // 0b00110000 = 48
        auto shr_result = a >> 1;  // 0b00000110 = 6
        
        REQUIRE(static_cast<uint64_t>(shl_result) == 48);
        REQUIRE(static_cast<uint64_t>(shr_result) == 6);
    }
    
    SECTION("Bit selection") {
        ch_uint<8> a(12);  // 0b00001100
        
        auto bit2 = a[2];  // 第2位应该是1
        auto bit0 = a[0];  // 第0位应该是0
        
        REQUIRE(static_cast<uint64_t>(bit2) == 1);
        REQUIRE(static_cast<uint64_t>(bit0) == 0);
    }
}

// ========== ch_reg 类型测试 ==========

TEST_CASE("ch_reg: basic operations", "[reg][operations][basic]") {
    // 创建测试上下文
    auto ctx = ch::core::context::create("test_reg_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Register creation and basic operations") {
        ch_reg<ch_uint<8>> reg_a(0);  // 初始值0
        ch_reg<ch_uint<8>> reg_b(5);  // 初始值5
        
        // 测试寄存器的基本操作
        reg_a->next = reg_b + ch_uint<8>(7);  // 5 + 7 = 12
        
        REQUIRE(ch_width_v<decltype(reg_a)> == 8);
        REQUIRE(ch_width_v<decltype(reg_b)> == 8);
    }
    
    SECTION("Register arithmetic operations") {
        ch_reg<ch_uint<8>> a(12);
        ch_reg<ch_uint<8>> b(5);
        
        ch_reg<ch_uint<9>> add_result(0);
        ch_reg<ch_uint<8>> mul_result(0);
        
        add_result->next = a + b;  // 12 + 5 = 17
        mul_result->next = a * b;  // 12 * 5 = 60
        
        REQUIRE(ch_width_v<decltype(add_result)> == 9);  // 加法结果宽度增加1位
        REQUIRE(ch_width_v<decltype(mul_result)> == 8);
    }
    
    SECTION("Register bitwise operations") {
        ch_reg<ch_uint<8>> a(12);
        ch_reg<ch_uint<8>> b(5);
        
        ch_reg<ch_uint<8>> and_result(0);
        ch_reg<ch_uint<8>> or_result(0);
        ch_reg<ch_uint<8>> xor_result(0);
        ch_reg<ch_uint<8>> not_result(0);
        
        and_result->next = a & b;
        or_result->next = a | b;
        xor_result->next = a ^ b;
        not_result->next = ~a;
        
        REQUIRE(ch_width_v<decltype(and_result)> == 8);
        REQUIRE(ch_width_v<decltype(or_result)> == 8);
        REQUIRE(ch_width_v<decltype(xor_result)> == 8);
        REQUIRE(ch_width_v<decltype(not_result)> == 8);
    }
    
    SECTION("Register comparison operations") {
        ch_reg<ch_uint<8>> a(12);
        ch_reg<ch_uint<8>> b(5);
        
        ch_reg<ch_uint<1>> eq_result(0);
        ch_reg<ch_uint<1>> gt_result(0);
        ch_reg<ch_uint<1>> lt_result(0);
        
        eq_result->next = (a == b);
        gt_result->next = (a > b);
        lt_result->next = (a < b);
        
        REQUIRE(ch_width_v<decltype(eq_result)> == 1);
        REQUIRE(ch_width_v<decltype(gt_result)> == 1);
        REQUIRE(ch_width_v<decltype(lt_result)> == 1);
    }
}

// ========== IO 端口测试 ==========

TEST_CASE("IO ports: basic operations", "[io][operations][basic]") {
    // 创建测试上下文
    auto ctx = ch::core::context::create("test_io_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Output port operations") {
        TestComponent comp;
        comp.create_ports();
        
        ch_uint<8> test_value(42);
        comp.io().test_out = test_value;
        
        REQUIRE(comp.io().test_out.is_valid() == true);
        REQUIRE(comp.io().test_out.name() == "test_out");
    }
    
    SECTION("Input port operations") {
        TestComponent comp;
        comp.create_ports();
        
        REQUIRE(comp.io().test_in.is_valid() == true);
        REQUIRE(comp.io().test_in.name() == "test_in");
    }
    
    SECTION("Stream bundle operations") {
        TestComponent comp;
        comp.create_ports();
        
        ch_uint<8> data_value(100);
        ch_uint<1> bool_value(1);
        
        // 测试 Stream Bundle 的各个字段
        comp.io().test_stream.data = data_value;
        comp.io().test_stream.valid = bool_value;
        // ready 是输入端口，不能赋值测试
        
        REQUIRE(comp.io().test_stream.data.is_valid() == true);
        REQUIRE(comp.io().test_stream.valid.is_valid() == true);
        REQUIRE(comp.io().test_stream.ready.is_valid() == true);
        
        REQUIRE(comp.io().test_stream.data.name() == "test_streamdata");
        REQUIRE(comp.io().test_stream.valid.name() == "test_streamvalid");
        REQUIRE(comp.io().test_stream.ready.name() == "test_streamready");
    }
    
    SECTION("Bundle flip operations") {
        TestComponent comp;
        comp.create_ports();
        
        // 测试 Bundle 翻转功能
        auto flipped_stream = comp.io().test_stream.flip();
        REQUIRE(flipped_stream != nullptr);
        
        // 翻转后的端口方向应该相反
        // 这里可以添加更详细的翻转测试
    }
}

// ========== 综合操作测试 ==========

TEST_CASE("Combined operations: complex expressions", "[combined][operations][complex]") {
    // 创建测试上下文
    auto ctx = ch::core::context::create("test_combined_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Complex arithmetic expressions") {
        ch_uint<8> a(10);
        ch_uint<8> b(3);
        ch_uint<8> c(2);
        
        // 测试复杂表达式: (a + b) * c - (a / b)
        auto complex_result = (a + b) * c - (a / b);
        // (10 + 3) * 2 - (10 / 3) = 13 * 2 - 3 = 26 - 3 = 23
        
        REQUIRE(static_cast<uint64_t>(complex_result) == 23);
    }
    
    SECTION("Complex bitwise expressions") {
        ch_uint<8> a(15);   // 0b00001111
        ch_uint<8> b(10);   // 0b00001010
        ch_uint<8> c(5);    // 0b00000101
        
        // 测试复杂位操作: (a & b) | (a ^ c)
        auto complex_bit_result = (a & b) | (a ^ c);
        // (15 & 10) | (15 ^ 5) = 10 | 10 = 10
        
        REQUIRE(static_cast<uint64_t>(complex_bit_result) == 10);
    }
    
    SECTION("Mixed operations with registers") {
        ch_reg<ch_uint<8>> reg_a(12);
        ch_reg<ch_uint<8>> reg_b(5);
        ch_reg<ch_uint<8>> result(0);
        
        // 复杂表达式: ~(reg_a + reg_b) & 0xF0
        result->next = ~(reg_a + reg_b) & ch_uint<8>(0xF0);
        // ~(12 + 5) & 0xF0 = ~17 & 0xF0 = 238 & 0xF0 = 224
        
        REQUIRE(ch_width_v<decltype(result)> == 8);
    }
}

// ========== 边界条件测试 ==========

TEST_CASE("Edge cases: boundary conditions", "[edge][operations][boundary]") {
    // 创建测试上下文
    auto ctx = ch::core::context::create("test_edge_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Zero values") {
        ch_uint<8> zero(0);
        ch_uint<8> non_zero(5);
        
        auto add_zero = zero + non_zero;
        auto mul_zero = zero * non_zero;
        auto and_zero = zero & non_zero;
        
        REQUIRE(static_cast<uint64_t>(add_zero) == 5);
        REQUIRE(static_cast<uint64_t>(mul_zero) == 0);
        REQUIRE(static_cast<uint64_t>(and_zero) == 0);
    }
    
    SECTION("Maximum values") {
        ch_uint<8> max_val(255);  // 0xFF
        ch_uint<8> one(1);
        
        auto add_overflow = max_val + one;  // 255 + 1 = 256 (9位)
        auto mul_result = max_val * one;    // 255 * 1 = 255
        
        REQUIRE(static_cast<uint64_t>(add_overflow) == 256);
        REQUIRE(static_cast<uint64_t>(mul_result) == 255);
    }
    
    SECTION("Single bit operations") {
        ch_uint<1> bit0(0);
        ch_uint<1> bit1(1);
        
        auto and_result = bit0 & bit1;
        auto or_result = bit0 | bit1;
        auto xor_result = bit0 ^ bit1;
        auto not_result = ~bit0;
        
        REQUIRE(static_cast<uint64_t>(and_result) == 0);
        REQUIRE(static_cast<uint64_t>(or_result) == 1);
        REQUIRE(static_cast<uint64_t>(xor_result) == 1);
        REQUIRE(static_cast<uint64_t>(not_result) == 1);
    }
}

// ========== 性能和类型特性测试 ==========

TEST_CASE("Type traits and performance", "[traits][performance]") {
    SECTION("Width traits") {
        STATIC_REQUIRE(ch_width_v<ch_uint<1>> == 1);
        STATIC_REQUIRE(ch_width_v<ch_uint<8>> == 8);
        STATIC_REQUIRE(ch_width_v<ch_uint<16>> == 16);
        STATIC_REQUIRE(ch_width_v<ch_uint<32>> == 32);
        STATIC_REQUIRE(ch_width_v<ch_uint<64>> == 64);
        
        STATIC_REQUIRE(ch_width_v<ch_reg<ch_uint<8>>> == 8);
        STATIC_REQUIRE(ch_width_v<ch_reg<ch_uint<32>>> == 32);
    }
    
    SECTION("Type consistency") {
        using test_type = ch_uint<8>;
        STATIC_REQUIRE(std::is_same_v<test_type, ch_uint<8>>);
        
        using reg_type = ch_reg<ch_uint<8>>;
        STATIC_REQUIRE(ch_width_v<reg_type> == 8);
    }
}
