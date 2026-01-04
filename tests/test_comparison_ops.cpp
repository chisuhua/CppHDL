#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/uint.h"
#include "core/io.h"

using namespace ch::core;

TEST_CASE("Comparison operations return type verification", "[operators][comparison][return-type]") {
    context ctx("test_context");
    ctx_swap swap(&ctx);

    // 使用字面量创建ch_uint对象
    ch_uint<8> a(100, "a");
    ch_uint<8> b(150, "b");
    ch_uint<8> c(100, "c"); // same as a

    // 测试比较操作
    auto eq_result = a == c;      // Should be true (100 == 100)
    auto ne_result = a != b;      // Should be true (100 != 150)
    auto lt_result = a < b;       // Should be true (100 < 150)
    auto le_result = a <= c;      // Should be true (100 <= 100)
    auto gt_result = b > a;       // Should be true (150 > 100)
    auto ge_result = a >= c;      // Should be true (100 >= 100)

    // 验证返回类型是ch_bool而不是ch_uint<1>
    REQUIRE(std::is_same_v<decltype(eq_result), ch_bool>);
    REQUIRE(std::is_same_v<decltype(ne_result), ch_bool>);
    REQUIRE(std::is_same_v<decltype(lt_result), ch_bool>);
    REQUIRE(std::is_same_v<decltype(le_result), ch_bool>);
    REQUIRE(std::is_same_v<decltype(gt_result), ch_bool>);
    REQUIRE(std::is_same_v<decltype(ge_result), ch_bool>);

    // 验证比较操作结果可以用于逻辑操作
    auto combined = (a == c) && (b > a);  // Should be true && true = true
    REQUIRE(std::is_same_v<decltype(combined), ch_bool>);

    auto combined2 = (a == b) || (b > a); // Should be false || true = true
    REQUIRE(std::is_same_v<decltype(combined2), ch_bool>);
}

TEST_CASE("Comparison operations with literals", "[operators][comparison][literals]") {
    context ctx("test_context");
    ctx_swap swap(&ctx);

    ch_uint<8> a(42, "a");

    // 测试与字面量的比较
    auto eq_lit = a == 42_d;
    auto ne_lit = a != 100_d;
    auto lt_lit = a < 50_d;
    auto gt_lit = a > 20_d;

    // 验证返回类型是ch_bool
    REQUIRE(std::is_same_v<decltype(eq_lit), ch_bool>);
    REQUIRE(std::is_same_v<decltype(ne_lit), ch_bool>);
    REQUIRE(std::is_same_v<decltype(lt_lit), ch_bool>);
    REQUIRE(std::is_same_v<decltype(gt_lit), ch_bool>);
}

TEST_CASE("Logical operations with comparison results", "[operators][logical][comparison]") {
    context ctx("test_context");
    ctx_swap swap(&ctx);

    ch_uint<8> a(10, "a");
    ch_uint<8> b(20, "b");

    // 原来的错误：比较操作返回ch_uint<1>会导致逻辑操作符重载歧义
    // 现在应该可以正常工作，因为比较操作返回ch_bool
    ch_bool result1 = (a == 10_d) && (b == 20_d);  // true && true
    ch_bool result2 = (a == 10_d) || (b == 5_d);   // true || false
    ch_bool result3 = !(a == 20_d);                // !(false) = true

    // 验证类型
    REQUIRE(std::is_same_v<decltype(result1), ch_bool>);
    REQUIRE(std::is_same_v<decltype(result2), ch_bool>);
    REQUIRE(std::is_same_v<decltype(result3), ch_bool>);
}

TEST_CASE("Comparison operations with IO types", "[operators][comparison][io]") {
    context ctx("test_context");
    ctx_swap swap(&ctx);

    // 创建IO类型用于测试
    ch_in<ch_uint<8>> input_a("input_a");
    ch_in<ch_uint<8>> input_b("input_b");
    ch_out<ch_uint<8>> output_c("output_c");

    // 测试IO类型的比较操作
    auto eq_io = input_a == input_b;      // Should return ch_bool
    auto lt_io = input_a < 100_d;         // Should return ch_bool
    auto gt_io = 50_d > input_b;          // Should return ch_bool

    // 验证返回类型是ch_bool
    REQUIRE(std::is_same_v<decltype(eq_io), ch_bool>);
    REQUIRE(std::is_same_v<decltype(lt_io), ch_bool>);
    REQUIRE(std::is_same_v<decltype(gt_io), ch_bool>);

    // 测试IO类型比较结果的逻辑操作
    auto combined_io = (input_a == 50_d) && (input_b < 100_d);
    REQUIRE(std::is_same_v<decltype(combined_io), ch_bool>);

    auto combined_or_io = (input_a != input_b) || (input_a >= 25_d);
    REQUIRE(std::is_same_v<decltype(combined_or_io), ch_bool>);
}