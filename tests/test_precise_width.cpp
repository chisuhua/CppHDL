// test_precise_width.cpp
#include "catch_amalgamated.hpp"
#include "core/operators.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/context.h"

using namespace ch::core;

TEST_CASE("precise_width: concat operation with exact widths", "[precise_width][concat]") {
    // 创建测试上下文
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // 创建不同宽度的ch_uint
    ch_uint<3> a(0b101, "a");  // 3位
    ch_uint<5> b(0b11010, "b");  // 5位
    
    // 测试拼接操作
    auto result = concat(a, b);  // 应该是8位精确宽度
    
    // 验证结果类型具有精确的8位宽度
    STATIC_REQUIRE(ch_width_v<decltype(result)> == 8);
    
    // 验证类型是ch_uint<8>而不是ch_uint<8>的分组类型
    REQUIRE(std::is_same_v<decltype(result), ch_uint<8>>);
}

TEST_CASE("precise_width: nested concat operations", "[precise_width][concat][nested]") {
    // 创建测试上下文
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // 创建不同宽度的ch_uint
    ch_uint<2> a(0b11, "a");  // 2位
    ch_uint<3> b(0b101, "b");  // 3位
    ch_uint<4> c(0b1110, "c");  // 4位
    
    // 测试嵌套拼接操作
    auto result1 = concat(a, concat(b, c));  // 2 + (3 + 4) = 9位
    auto result2 = concat(concat(a, b), c);  // (2 + 3) + 4 = 9位
    
    // 验证结果类型具有精确的9位宽度
    STATIC_REQUIRE(ch_width_v<decltype(result1)> == 9);
    STATIC_REQUIRE(ch_width_v<decltype(result2)> == 9);
    
    // 验证类型是ch_uint<9>
    REQUIRE(std::is_same_v<decltype(result1), ch_uint<9>>);
    REQUIRE(std::is_same_v<decltype(result2), ch_uint<9>>);
}

TEST_CASE("precise_width: bit operations", "[precise_width][bits]") {
    // 创建测试上下文
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // 创建一个12位的ch_uint
    ch_uint<12> data(0b101101011100, "data");
    
    // 测试位提取操作
    auto slice_7_4 = bits<decltype(data), 7, 4>(data);  // 应该是4位
    
    // 验证结果类型具有精确的4位宽度
    STATIC_REQUIRE(ch_width_v<decltype(slice_7_4)> == 4);
    
    // 验证类型是ch_uint<4>
    REQUIRE(std::is_same_v<decltype(slice_7_4), ch_uint<4>>);
}

TEST_CASE("precise_width: arithmetic operations", "[precise_width][arithmetic]") {
    // 创建测试上下文
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // 创建不同宽度的ch_uint
    ch_uint<7> a(0b1010101, "a");  // 7位
    ch_uint<5> b(0b11010, "b");    // 5位
    
    // 测试加法操作
    auto sum = a + b;  // 结果应该是max(7,5)+1 = 8位
    
    // 验证结果类型具有精确的8位宽度
    STATIC_REQUIRE(ch_width_v<decltype(sum)> == 8);
    
    // 验证类型是ch_uint<8>
    REQUIRE(std::is_same_v<decltype(sum), ch_uint<8>>);
}

TEST_CASE("precise_width: mixed operations", "[precise_width][mixed]") {
    // 创建测试上下文
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // 创建不同宽度的ch_uint
    ch_uint<13> a(0b1011010111001, "a");  // 13位
    ch_uint<9> b(0b110101101, "b");       // 9位
    ch_uint<6> c(0b101101, "c");          // 6位
    
    // 测试复杂混合操作
    auto result = concat(a, b + c);  // 13 + (max(9,6)+1) = 13 + 10 = 23位
    
    // 验证结果类型具有精确的23位宽度
    STATIC_REQUIRE(ch_width_v<decltype(result)> == 23);
    
    // 验证类型是ch_uint<23>
    REQUIRE(std::is_same_v<decltype(result), ch_uint<23>>);
}