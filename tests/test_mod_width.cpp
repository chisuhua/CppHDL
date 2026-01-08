#include "catch_amalgamated.hpp"
#include "../include/core/operators.h"
#include "../include/core/literal.h"
#include "../include/core/uint.h"

using namespace ch::core;

TEST_CASE("Test mod_op bit width calculation with compile-time literals", "[mod_width]") {
    // 测试当右操作数是字面量3时（值为3，需要2位表示0-2），结果应该只需要2位
    constexpr auto literal_3 = make_literal<3, 2>();
    constexpr unsigned width_result_3 = get_binary_result_width<mod_op, ch_uint<8>, decltype(literal_3)>();
    REQUIRE(width_result_3 == 2);
    
    // 测试当右操作数是字面量1时（值为1，模1的结果只能是0），结果应该只需要1位
    constexpr auto literal_1 = make_literal<1, 1>();
    constexpr unsigned width_result_1 = get_binary_result_width<mod_op, ch_uint<8>, decltype(literal_1)>();
    REQUIRE(width_result_1 == 1);
    
    // 测试当右操作数是字面量8时（值为8，结果范围是0-7，需要3位），结果应该只需要3位
    constexpr auto literal_8 = make_literal<8, 4>();
    constexpr unsigned width_result_8 = get_binary_result_width<mod_op, ch_uint<16>, decltype(literal_8)>();
    REQUIRE(width_result_8 == 3);
    
    // 测试当右操作数是字面量16时（值为16，结果范围是0-15，需要4位），结果应该只需要4位
    constexpr auto literal_16 = make_literal<16, 5>();
    constexpr unsigned width_result_16 = get_binary_result_width<mod_op, ch_uint<32>, decltype(literal_16)>();
    REQUIRE(width_result_16 == 4);
    
    // 测试当右操作数是字面量7时（值为7，结果范围是0-6，需要3位），结果应该只需要3位
    constexpr auto literal_7 = make_literal<7, 3>();
    constexpr unsigned width_result_7 = get_binary_result_width<mod_op, ch_uint<16>, decltype(literal_7)>();
    REQUIRE(width_result_7 == 3);
    
    // 测试当右操作数是字面量100时（值为100，结果范围是0-99，需要7位），结果应该只需要7位
    constexpr auto literal_100 = make_literal<100, 7>();
    constexpr unsigned width_result_100 = get_binary_result_width<mod_op, ch_uint<16>, decltype(literal_100)>();
    REQUIRE(width_result_100 == 7);
    
    SECTION("Testing basic functionality") {
        REQUIRE(width_result_3 == 2);
        REQUIRE(width_result_1 == 1);
        REQUIRE(width_result_8 == 3);
        REQUIRE(width_result_16 == 4);
        REQUIRE(width_result_7 == 3);
        REQUIRE(width_result_100 == 7);
    }
}