// tests/test_popcount.cpp
#include "catch_amalgamated.hpp"
#include <string>
#include <typeinfo>

#include "core/bool.h"
#include "core/uint.h"
#include "core/io.h"
#include "core/context.h"
#include "core/operators.h"

using namespace ch::core;


TEST_CASE("Popcount operation on various ch_uint types", "[popcount]") {
    SECTION("popcount of ch_uint<1>") {
        auto ctx = context("test_ctx");
        auto value = ch_uint<1>(0b1);
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
        auto input_port = ch::input<8>("input");
        auto result = popcount(input_port);
        
        // 对于8位输入端口，最多有8个1，需要4位表示（0-8）
        REQUIRE(ch_width_v<decltype(result)> == 4);
    }

    SECTION("popcount of output port") {
        auto ctx = context("test_ctx");
        auto output_port = ch::output<8>("output");
        auto result = popcount(output_port);
        
        // 对于8位输出端口，最多有8个1，需要4位表示（0-8）
        REQUIRE(ch_width_v<decltype(result)> == 4);
    }
}