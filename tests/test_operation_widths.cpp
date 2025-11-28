#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/context.h"
#include "core/operators.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/literal.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// 测试各种操作的目标位宽计算
TEST_CASE("Operation Width Calculation", "[operation][width][compile_time]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Arithmetic Operations") {
        ch_uint<8> a(10);
        ch_uint<6> b(5);

        // 加法: max(M, N) + 1
        auto add_result = a + b;
        STATIC_REQUIRE(ch_width_v<decltype(add_result)> == 9);  // max(8,6) + 1 = 9

        // 减法: max(M, N)
        auto sub_result = a - b;
        STATIC_REQUIRE(ch_width_v<decltype(sub_result)> == 8);  // max(8,6) = 8

        // 乘法: M + N
        auto mul_result = a * b;
        STATIC_REQUIRE(ch_width_v<decltype(mul_result)> == 14); // 8 + 6 = 14

        // 负号: M
        auto neg_result = -a;
        STATIC_REQUIRE(ch_width_v<decltype(neg_result)> == 8);  // 8
    }

    SECTION("Bitwise Operations") {
        ch_uint<8> a(0b10101010);
        ch_uint<6> b(0b110011);

        // 按位与: max(M, N)
        auto and_result = a & b;
        STATIC_REQUIRE(ch_width_v<decltype(and_result)> == 8);  // max(8,6) = 8

        // 按位或: max(M, N)
        auto or_result = a | b;
        STATIC_REQUIRE(ch_width_v<decltype(or_result)> == 8);   // max(8,6) = 8

        // 按位异或: max(M, N)
        auto xor_result = a ^ b;
        STATIC_REQUIRE(ch_width_v<decltype(xor_result)> == 8);  // max(8,6) = 8

        // 按位取反: M
        auto not_result = ~a;
        STATIC_REQUIRE(ch_width_v<decltype(not_result)> == 8);  // 8
    }

    SECTION("Comparison Operations") {
        ch_uint<8> a(10);
        ch_uint<6> b(5);

        // 等于: 1
        auto eq_result = (a == b);
        STATIC_REQUIRE(ch_width_v<decltype(eq_result)> == 1);

        // 不等于: 1
        auto ne_result = (a != b);
        STATIC_REQUIRE(ch_width_v<decltype(ne_result)> == 1);

        // 小于: 1
        auto lt_result = (a < b);
        STATIC_REQUIRE(ch_width_v<decltype(lt_result)> == 1);

        // 小于等于: 1
        auto le_result = (a <= b);
        STATIC_REQUIRE(ch_width_v<decltype(le_result)> == 1);

        // 大于: 1
        auto gt_result = (a > b);
        STATIC_REQUIRE(ch_width_v<decltype(gt_result)> == 1);

        // 大于等于: 1
        auto ge_result = (a >= b);
        STATIC_REQUIRE(ch_width_v<decltype(ge_result)> == 1);
    }

    SECTION("Shift Operations") {
        ch_uint<8> a(10);
        ch_uint<3> shamt(2);

        // 左移: M
        auto shl_result = a << shamt;
        STATIC_REQUIRE(ch_width_v<decltype(shl_result)> == 8);  // 8

        // 右移: M
        auto shr_result = a >> shamt;
        STATIC_REQUIRE(ch_width_v<decltype(shr_result)> == 8);  // 8

        // 算术右移: M
        auto sshr_result = a >> shamt;  // 简化处理，实际应该有符号类型
        STATIC_REQUIRE(ch_width_v<decltype(sshr_result)> == 8); // 8
    }

    SECTION("Bit Operations") {
        ch_uint<8> a(0b10101010);

        // 位提取
        auto bits_result = bits<ch_uint<8>, 6, 2>(a);  // 使用正确的函数名 bits<>()
        STATIC_REQUIRE(ch_width_v<decltype(bits_result)> == 5);  // 6-2+1 = 5
    }

    SECTION("Concatenation Operation") {
        ch_uint<3> a(0b101);
        ch_uint<5> b(0b11010);

        // 连接: M + N
        auto concat_result = concat(a, b);
        STATIC_REQUIRE(ch_width_v<decltype(concat_result)> == 8);  // 3 + 5 = 8
    }

    SECTION("Extension Operations") {
        ch_uint<3> a(0b101);

        // 零扩展
        auto zext_result = zext<ch_uint<3>, 8>(a);
        STATIC_REQUIRE(ch_width_v<decltype(zext_result)> == 8);

        // 符号扩展
        auto sext_result = sext<ch_uint<3>, 8>(a);
        STATIC_REQUIRE(ch_width_v<decltype(sext_result)> == 8);
    }

    SECTION("Reduction Operations") {
        ch_uint<8> a(0b10101010);

        // 与归约: 1
        auto and_reduce_result = and_reduce(a);
        STATIC_REQUIRE(ch_width_v<decltype(and_reduce_result)> == 1);

        // 或归约: 1
        auto or_reduce_result = or_reduce(a);
        STATIC_REQUIRE(ch_width_v<decltype(or_reduce_result)> == 1);

        // 异或归约: 1
        auto xor_reduce_result = xor_reduce(a);
        STATIC_REQUIRE(ch_width_v<decltype(xor_reduce_result)> == 1);
    }

    SECTION("Mux Operation") {
        ch_bool cond(true);
        ch_uint<8> a(10);
        ch_uint<6> b(5);

        // 多路选择器: max(M, N)
        auto mux_result = select(cond, a, b);
        STATIC_REQUIRE(ch_width_v<decltype(mux_result)> == 8);  // max(8,6) = 8
    }
}

// 测试仿真时运行时位宽是否与编译期一致
TEST_CASE("Runtime Width Consistency", "[operation][width][runtime]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Concatenation Runtime Width") {
        ch_uint<3> a(0b101);
        ch_uint<5> b(0b11010);
        
        // 编译期宽度
        constexpr unsigned compile_time_width = ch_width_v<decltype(concat(a, b))>;
        STATIC_REQUIRE(compile_time_width == 8);
        
        // 创建实际节点进行测试
        auto concat_result = concat(a, b);
        
        // 检查运行时节点大小是否与编译期一致
        REQUIRE(concat_result.impl() != nullptr);
        REQUIRE(concat_result.impl()->size() == compile_time_width);
    }

    SECTION("Arithmetic Runtime Width") {
        ch_uint<8> a(10);
        ch_uint<6> b(5);
        
        // 加法测试
        auto add_result = a + b;
        constexpr unsigned add_compile_width = ch_width_v<decltype(add_result)>;
        STATIC_REQUIRE(add_compile_width == 9);
        REQUIRE(add_result.impl() != nullptr);
        REQUIRE(add_result.impl()->size() == add_compile_width);
        
        // 乘法测试
        auto mul_result = a * b;
        constexpr unsigned mul_compile_width = ch_width_v<decltype(mul_result)>;
        STATIC_REQUIRE(mul_compile_width == 14);
        REQUIRE(mul_result.impl() != nullptr);
        REQUIRE(mul_result.impl()->size() == mul_compile_width);
    }

    SECTION("Bit Extraction Runtime Width") {
        ch_uint<8> a(0b10101010);
        
        auto bits_result = bits<ch_uint<8>, 6, 2>(a);
        constexpr unsigned bits_compile_width = ch_width_v<decltype(bits_result)>;
        STATIC_REQUIRE(bits_compile_width == 5);
        REQUIRE(bits_result.impl() != nullptr);
        REQUIRE(bits_result.impl()->size() == bits_compile_width);
    }
}