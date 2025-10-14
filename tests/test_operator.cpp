#include "catch_amalgamated.hpp"
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/core/bundle.h"
#include "../include/core/context.h"
#include "../include/core/uint.h"
#include "../include/core/bool.h"
#include "../include/reg.h"
#include "../include/core/traits.h"
#include "../include/core/literal.h"
#include <type_traits>

using namespace ch::core;
using namespace ch::core::literals;  // 引入硬件字面量

// 测试用的简单组件
class TestComponent : public ch::Component {
public:
    __io(
        ch_out<ch_uint<8>> test_out;
        ch_in<ch_uint<8>> test_in;
        ch::stream<ch_uint<8>> test_stream;
        ch_out<ch_bool> bool_out;
        ch_in<ch_bool> bool_in;
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

// ========== ch_bool 类型测试 ==========

TEST_CASE("ch_bool: basic operations", "[bool][operations][basic]") {
    // 创建测试上下文
    auto ctx = ch::core::context::create("test_bool_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Boolean construction") {
        ch_bool b1(true);
        ch_bool b2(false);
        ch_bool b3(1_b);  // 从字面量构造
        ch_bool b4(0_b);
        
        REQUIRE(b1.impl() != nullptr);
        REQUIRE(b2.impl() != nullptr);
        REQUIRE(b3.impl() != nullptr);
        REQUIRE(b4.impl() != nullptr);
    }
    
    SECTION("Logical NOT operation") {
        ch_bool true_val(true);
        ch_bool false_val(false);
        
        auto not_true = !true_val;   // !true = false
        auto not_false = !false_val; // !false = true
        
        REQUIRE(not_true.impl() != nullptr);
        REQUIRE(not_false.impl() != nullptr);
    }
    
    SECTION("Logical AND operation") {
        ch_bool t(true);
        ch_bool f(false);
        
        auto tt = t && t;  // true && true = true
        auto tf = t && f;  // true && false = false
        auto ft = f && t;  // false && true = false
        auto ff = f && f;  // false && false = false
        
        REQUIRE(tt.impl() != nullptr);
        REQUIRE(tf.impl() != nullptr);
        REQUIRE(ft.impl() != nullptr);
        REQUIRE(ff.impl() != nullptr);
    }
    
    SECTION("Logical OR operation") {
        ch_bool t(true);
        ch_bool f(false);
        
        auto tt = t || t;  // true || true = true
        auto tf = t || f;  // true || false = true
        auto ft = f || t;  // false || true = true
        auto ff = f || f;  // false || false = false
        
        REQUIRE(tt.impl() != nullptr);
        REQUIRE(tf.impl() != nullptr);
        REQUIRE(ft.impl() != nullptr);
        REQUIRE(ff.impl() != nullptr);
    }
    
    SECTION("Boolean comparison operations") {
        ch_bool t(true);
        ch_bool f(false);
        
        auto eq_tt = (t == t);  // true == true = true
        auto eq_tf = (t == f);  // true == false = false
        auto ne_tt = (t != f);  // true != false = true
        auto ne_tf = (t != t);  // true != true = false
        
        REQUIRE(eq_tt.impl() != nullptr);
        REQUIRE(eq_tf.impl() != nullptr);
        REQUIRE(ne_tt.impl() != nullptr);
        REQUIRE(ne_tf.impl() != nullptr);
    }
}

// ========== 混合类型操作测试 ==========

TEST_CASE("Mixed operations: ch_bool and ch_uint", "[mixed][operations][bool_uint]") {
    // 创建测试上下文
    auto ctx = ch::core::context::create("test_mixed_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Boolean operations with uint comparisons") {
        ch_uint<8> a(12);
        ch_uint<8> b(5);
        
        // 比较操作返回布尔值，可以用于逻辑操作
        auto comp_and = (a > b) && (b < a);  // true && true = true
        auto comp_or = (a == b) || (a != b); // false || true = true
        auto comp_not = !(a < b);            // !false = true
        
        REQUIRE(comp_and.impl() != nullptr);
        REQUIRE(comp_or.impl() != nullptr);
        REQUIRE(comp_not.impl() != nullptr);
    }
    
    SECTION("Boolean operations with literals") {
        ch_uint<8> a(12);
        
        // 与字面量的混合操作
        auto lit_and = (a > 5) && (a < 20);     // true && true = true
        auto lit_or = (a == 10) || (a == 12);   // false || true = true
        auto lit_mixed = !(a == 0);             // !false = true
        
        REQUIRE(lit_and.impl() != nullptr);
        REQUIRE(lit_or.impl() != nullptr);
        REQUIRE(lit_mixed.impl() != nullptr);
    }
    
    SECTION("Boolean operations with hardware literals") {
        ch_uint<8> a(12);
        
        // 与硬件字面量的混合操作
        auto hw_lit_and = (a > 0x05_h) && (a < 0x14_h);  // true && true = true
        auto hw_lit_or = (a == 0x0A_h) || (a == 0x0C_h); // false || true = true
        auto hw_lit_not = !(a == 0x00_h);                // !false = true
        
        REQUIRE(hw_lit_and.impl() != nullptr);
        REQUIRE(hw_lit_or.impl() != nullptr);
        REQUIRE(hw_lit_not.impl() != nullptr);
    }
}

// ========== 硬件友好字面量操作数测试 ==========

TEST_CASE("Operations with hardware-friendly literals", "[uint][operations][literals]") {
    // 创建测试上下文
    auto ctx = ch::core::context::create("test_literals_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Binary literals as operands") {
        ch_uint<8> a(1111'0000_b);  // 240
        ch_uint<8> b(0000'1111_b);  // 15
        
        auto and_result = a & b;    // 240 & 15 = 0
        auto or_result = a | b;     // 240 | 15 = 255
        auto add_result = a + b;    // 240 + 15 = 255
        
        REQUIRE(static_cast<uint64_t>(and_result) == 0);
        REQUIRE(static_cast<uint64_t>(or_result) == 255);
        REQUIRE(static_cast<uint64_t>(add_result) == 255);
    }
    
    SECTION("Hexadecimal literals as operands") {
        ch_uint<16> a(0xDEAD_h);    // 57005
        ch_uint<16> b(0xBEEF_h);    // 48879
        
        auto add_result = a + b;    // 57005 + 48879 = 105884
        auto and_result = a & b;    // 0xDEAD & 0xBEEF = 0xBEAD = 48813
        
        REQUIRE(static_cast<uint64_t>(add_result) == 105884);
        REQUIRE(static_cast<uint64_t>(and_result) == 48813);
    }
    
    SECTION("Boolean literals as operands") {
        ch_bool t(1_b);  // true
        ch_bool f(0_b);  // false
        
        auto and_result = t && f;  // true && false = false
        auto or_result = t || f;   // true || false = true
        auto not_result = !t;      // !true = false
        
        REQUIRE(and_result.impl() != nullptr);
        REQUIRE(or_result.impl() != nullptr);
        REQUIRE(not_result.impl() != nullptr);
    }
    
    SECTION("Mixed literal and uint operations") {
        ch_uint<8> regular_val(100);
        auto hex_result = regular_val + 0x14_h;  // 100 + 20 = 120
        auto bin_result = regular_val & 1111'0000_b; // 100 & 240 = 96
        auto bool_result = (regular_val > 0x00_h) && (regular_val < 0xFF_h); // true && true = true
        
        REQUIRE(static_cast<uint64_t>(hex_result) == 120);
        REQUIRE(static_cast<uint64_t>(bin_result) == 96);
        REQUIRE(bool_result.impl() != nullptr);
    }
    
    SECTION("Literal-only expressions") {
        auto literal_add = 1111_b + 0001_b;      // 15 + 1 = 16 (ch_uint<5>)
        auto literal_mul = 0x0F_h * 0x0A_h;      // 15 * 10 = 150 (ch_uint<8>)
        auto literal_bitwise = 0xAA_h & 0x55_h;  // 170 & 85 = 0 (ch_uint<8>)
        auto literal_bool = 1_b && 1_b;          // true && true = true (ch_bool)
        
        REQUIRE(static_cast<uint64_t>(literal_add) == 16);
        REQUIRE(static_cast<uint64_t>(literal_mul) == 150);
        REQUIRE(static_cast<uint64_t>(literal_bitwise) == 0);
        REQUIRE(literal_bool.impl() != nullptr);
    }
}

// ========== 不同操作数类型组合测试 ==========

TEST_CASE("Operations with different operand types", "[uint][operations][mixed]") {
    // 创建测试上下文
    auto ctx = ch::core::context::create("test_mixed_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("ch_uint<N> OP uint64_t literal") {
        ch_uint<8> a(12);
        auto result1 = a + 5;       // ch_uint + uint64_t
        auto result2 = a * 3;       // ch_uint * uint64_t
        auto result3 = a == 12;     // ch_uint == uint64_t (returns ch_bool)
        
        REQUIRE(static_cast<uint64_t>(result1) == 17);
        REQUIRE(static_cast<uint64_t>(result2) == 36);
        REQUIRE(result3.impl() != nullptr);  // 比较结果是ch_bool
    }
    
    SECTION("uint64_t literal OP ch_uint<N>") {
        ch_uint<8> a(5);
        auto result1 = 12 + a;      // uint64_t + ch_uint
        auto result2 = 100 - a;     // uint64_t - ch_uint
        auto result3 = 25 == a;     // uint64_t == ch_uint (returns ch_bool)
        
        REQUIRE(static_cast<uint64_t>(result1) == 17);
        REQUIRE(static_cast<uint64_t>(result2) == 95);
        REQUIRE(result3.impl() != nullptr);  // 比较结果是ch_bool
    }
    
    SECTION("ch_uint<N> OP ch_literal") {
        ch_uint<8> a(12);
        auto result1 = a + 0x05_h;  // ch_uint + ch_literal
        auto result2 = a & 0xF0_b;  // ch_uint & ch_literal
        auto result3 = a < 0x10_h;  // ch_uint < ch_literal (returns ch_bool)
        
        REQUIRE(static_cast<uint64_t>(result1) == 17);
        REQUIRE(static_cast<uint64_t>(result2) == 0);
        REQUIRE(result3.impl() != nullptr);  // 比较结果是ch_bool
    }
    
    SECTION("ch_literal OP ch_uint<N>") {
        ch_uint<8> a(5);
        auto result1 = 0x0C_h + a;  // ch_literal + ch_uint
        auto result2 = 0xFF_h - a;  // ch_literal - ch_uint
        auto result3 = 0x0A_h > a;  // ch_literal > ch_uint (returns ch_bool)
        
        REQUIRE(static_cast<uint64_t>(result1) == 17);
        REQUIRE(static_cast<uint64_t>(result2) == 250);
        REQUIRE(result3.impl() != nullptr);  // 比较结果是ch_bool
    }
    
    SECTION("ch_literal OP ch_literal") {
        auto result1 = 0x0C_h + 0x05_h;  // ch_literal + ch_literal
        auto result2 = 0xF0_b & 0x0F_b;  // ch_literal & ch_literal
        auto result3 = 0x10_h == 0x10_h; // ch_literal == ch_literal (returns ch_bool)
        
        REQUIRE(static_cast<uint64_t>(result1) == 17);
        REQUIRE(static_cast<uint64_t>(result2) == 0);
        REQUIRE(result3.impl() != nullptr);  // 比较结果是ch_bool
    }
    
    SECTION("ch_bool OP ch_bool") {
        ch_bool t(true);
        ch_bool f(false);
        
        auto and_result = t && f;    // true && false = false
        auto or_result = t || f;     // true || false = true
        auto eq_result = (t == t);   // true == true = true
        auto ne_result = (t != f);   // true != false = true
        
        REQUIRE(and_result.impl() != nullptr);
        REQUIRE(or_result.impl() != nullptr);
        REQUIRE(eq_result.impl() != nullptr);
        REQUIRE(ne_result.impl() != nullptr);
    }
    
    SECTION("ch_bool OP bool literal") {
        ch_bool t(true);
        
        auto and_result = t && 1_b;   // true && true = true
        auto or_result = t || 0_b;    // true || false = true
        auto eq_result = (t == 1_b);  // true == true = true
        
        REQUIRE(and_result.impl() != nullptr);
        REQUIRE(or_result.impl() != nullptr);
        REQUIRE(eq_result.impl() != nullptr);
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
        ch_reg<ch_bool> bool_reg(false); // 布尔寄存器
        
        // 测试寄存器的基本操作
        reg_a->next = reg_b + ch_uint<8>(7);  // 5 + 7 = 12
        bool_reg->next = (reg_a > ch_uint<8>(10)); // 12 > 10 = true
        
        REQUIRE(ch_width_v<decltype(reg_a)> == 8);
        REQUIRE(ch_width_v<decltype(reg_b)> == 8);
        REQUIRE(ch_width_v<decltype(bool_reg)> == 1);
    }
    
    SECTION("Register with hardware literals") {
        ch_reg<ch_uint<8>> reg_a(0xFF_b);     // 使用二进制字面量初始化
        ch_reg<ch_uint<16>> reg_b(0xDEAD_h);  // 使用十六进制字面量初始化
        ch_reg<ch_bool> bool_reg(1_b);        // 使用布尔字面量初始化
        
        ch_reg<ch_uint<8>> result(0);
        result->next = reg_a + ch_uint<8>(1);  // 255 + 1 = 256 (会截断到8位)
        
        REQUIRE(ch_width_v<decltype(reg_a)> == 8);
        REQUIRE(ch_width_v<decltype(reg_b)> == 16);
        REQUIRE(ch_width_v<decltype(bool_reg)> == 1);
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
        
        ch_reg<ch_bool> eq_result(false);
        ch_reg<ch_bool> gt_result(false);
        ch_reg<ch_bool> lt_result(false);
        
        eq_result->next = (a == b);
        gt_result->next = (a > b);
        lt_result->next = (a < b);
        
        REQUIRE(ch_width_v<decltype(eq_result)> == 1);
        REQUIRE(ch_width_v<decltype(gt_result)> == 1);
        REQUIRE(ch_width_v<decltype(lt_result)> == 1);
    }
    
    SECTION("Boolean register logical operations") {
        ch_reg<ch_bool> bool_reg1(true);
        ch_reg<ch_bool> bool_reg2(false);
        ch_reg<ch_bool> result_reg(false);
        
        // 测试逻辑操作
        result_reg->next = bool_reg1 && bool_reg2;  // true && false = false
        result_reg->next = bool_reg1 || bool_reg2;  // true || false = true
        result_reg->next = !bool_reg1;              // !true = false
        
        REQUIRE(ch_width_v<decltype(bool_reg1)> == 1);
        REQUIRE(ch_width_v<decltype(bool_reg2)> == 1);
        REQUIRE(ch_width_v<decltype(result_reg)> == 1);
    }
    
    SECTION("Mixed boolean and integer register operations") {
        ch_reg<ch_bool> bool_reg(true);
        ch_reg<ch_uint<8>> uint_reg(5);
        ch_reg<ch_bool> result_reg(false);
        
        // 布尔与整数的混合操作
        result_reg->next = bool_reg && (uint_reg > ch_uint<8>(0));  // true && true = true
        result_reg->next = (uint_reg == ch_uint<8>(5)) || !bool_reg; // true || false = true
        
        REQUIRE(ch_width_v<decltype(bool_reg)> == 1);
        REQUIRE(ch_width_v<decltype(uint_reg)> == 8);
        REQUIRE(ch_width_v<decltype(result_reg)> == 1);
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
    
    SECTION("Boolean output port operations") {
        TestComponent comp;
        comp.create_ports();
        
       
