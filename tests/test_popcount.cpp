// tests/test_popcount.cpp
#include "catch_amalgamated.hpp"
#include <string>
#include <typeinfo>

#include "codegen_dag.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/operators.h"
#include "core/uint.h"
#include "simulator.h"

using namespace ch::core;

TEST_CASE("Popcount operation on various ch_uint types", "[popcount]") {
    SECTION("popcount of ch_uint<1>") {
        auto ctx = context("test_ctx");
        auto sim1 = Simulator(&ctx);
        auto value = ch_uint<1>(1_b);
        auto result = popcount(value);

        // 对于1位输入，最多有1个1，需要1位表示（0或1）
        REQUIRE(ch_width_v<decltype(result)> == 1);
    }

    SECTION("popcount of ch_uint<8>") {
        auto ctx = context("test_ctx");
        auto value = ch_uint<8>(0b10101010);
        auto result = popcount(value);

        // 对于8位输入，最多有8个1，需要4位表示（0-8）
        REQUIRE(ch_width_v<decltype(result)> == 4);
    }

    SECTION("popcount of ch_uint<16>") {
        auto ctx = context("test_ctx");
        auto value = ch_uint<16>(0b1111111100000000);
        auto result = popcount(value);

        // 对于16位输入，最多有16个1，需要5位表示（0-16）
        REQUIRE(ch_width_v<decltype(result)> == 5);
    }

    SECTION("popcount of ch_uint<32>") {
        auto ctx = context("test_ctx");
        auto value = ch_uint<32>(0xFFFFFFFF);
        auto result = popcount(value);

        // 对于32位输入，最多有32个1，需要6位表示（0-32）
        REQUIRE(ch_width_v<decltype(result)> == 6);
    }

    SECTION("popcount of ch_uint<64>") {
        auto ctx = context("test_ctx");
        auto value = ch_uint<64>(0xFFFFFFFFFFFFFFFF);
        auto result = popcount(value);

        // 对于64位输入，最多有64个1，需要7位表示（0-64）
        REQUIRE(ch_width_v<decltype(result)> == 7);
    }

    SECTION("popcount of input port") {
        auto ctx = context("test_ctx");
        auto input_port = ch_in<ch_uint<8>>("input");
        auto result = popcount(input_port);

        // 对于8位输入端口，最多有8个1，需要4位表示（0-8）
        REQUIRE(ch_width_v<decltype(result)> == 4);
    }

    SECTION("popcount of output port") {
        auto ctx = context("test_ctx");
        auto output_port = ch_out<ch_uint<8>>("output");
        auto result = popcount(output_port);

        // 对于8位输出端口，最多有8个1，需要4位表示（0-8）
        REQUIRE(ch_width_v<decltype(result)> == 4);
    }
}

TEST_CASE("Popcount operation result width calculation", "[popcount][width]") {
    SECTION("Result width for different input widths") {
        // 测试各种输入宽度的结果宽度计算
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<1> == 1);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<2> == 2);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<3> == 2);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<4> == 3);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<7> == 3);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<8> == 4);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<15> == 4);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<16> == 5);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<31> == 5);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<32> == 6);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<63> == 6);
        STATIC_REQUIRE(ch::core::popcount_op::result_width_v<64> == 7);
    }
}

TEST_CASE("Popcount operation computation results", "[popcount][computation]") {
    SECTION("Popcount computation for various values") {
        // 测试具体值的popcount计算结果
        auto ctx = context("test_ctx");
        auto sim1 = Simulator(&ctx);

        // ch_uint<4> 测试
        auto value1 = ch_uint<4>(0000_b); // 0 ones
        auto result1 = popcount(value1);

        auto value2 = ch_uint<4>(1111_b); // 4 ones
        auto result2 = popcount(value2);

        auto value3 = ch_uint<4>(1010_b); // 2 ones
        auto result3 = popcount(value3);

        auto value4 = ch_uint<8>(10101010_b); // 4 ones
        auto result4 = popcount(value4);

        auto value5 = ch_uint<8>(11111111_b); // 8 ones
        auto result5 = popcount(value5);

        auto value6 = ch_uint<3>(111_b); // 3 ones
        auto result6 = popcount(value6);

        REQUIRE(ch_width_v<decltype(result1)> == 3); // log2(4)+1 = 3
        REQUIRE(ch_width_v<decltype(result2)> == 3); // log2(4)+1 = 3
        REQUIRE(ch_width_v<decltype(result3)> == 3); // log2(4)+1 = 3
        REQUIRE(ch_width_v<decltype(result4)> == 4); // log2(8)+1 = 4
        REQUIRE(ch_width_v<decltype(result5)> == 4); // log2(8)+1 = 4
        REQUIRE(ch_width_v<decltype(result6)> == 2); // log2(3)+1 = 2

        sim1.reinitialize();
        sim1.tick();
        REQUIRE(static_cast<uint64_t>(sim1.get_value(result1)) == 0);
        REQUIRE(static_cast<uint64_t>(sim1.get_value(result2)) == 4);
        REQUIRE(static_cast<uint64_t>(sim1.get_value(result3)) == 2);
        // ch_uint<8> 测试
        REQUIRE(static_cast<uint64_t>(sim1.get_value(result4)) == 4);
        REQUIRE(static_cast<uint64_t>(sim1.get_value(result5)) == 8);
        // ch_uint<3> 测试
        REQUIRE(static_cast<uint64_t>(sim1.get_value(result6)) == 3);
    }

    SECTION("Popcount with ports") {
        auto ctx = context("test_ctx");

        // 测试输入端口
        auto input_port = ch_in<ch_uint<6>>("input");
        auto result = popcount(input_port);
        REQUIRE(ch_width_v<decltype(result)> == 3); // log2(6)+1 = 3

        // 测试输出端口
        auto output_port = ch_out<ch_uint<5>>("output");
        auto result2 = popcount(output_port);
        REQUIRE(ch_width_v<decltype(result2)> == 3); // log2(5)+1 = 3
    }
}