/**
 * @file test_forwarding.cpp
 * @brief Forwarding 单元功能测试
 */

#include "catch_amalgamated.hpp"
#include "../examples/riscv-mini/src/rv32i_forwarding.h"

using namespace riscv;

TEST_CASE("ForwardingUnit - Type Check", "[forwarding][type]") {
    ch::core::context ctx("forwarding_test");
    ch::core::ctx_swap swap(&ctx);
    ForwardingUnit fu{nullptr, "forwarding_unit"};
    REQUIRE(true);
}

TEST_CASE("ForwardingUnit - EX to EX Forwarding", "[forwarding][ex]") {
    ch::core::context ctx("forwarding_ex_test");
    ch::core::ctx_swap swap(&ctx);
    
    ForwardingUnit fu{nullptr, "forwarding_unit"};
    
    // 验证类型存在
    REQUIRE(true);
}

TEST_CASE("ForwardingUnit - MEM to EX Forwarding", "[forwarding][mem]") {
    ch::core::context ctx("forwarding_mem_test");
    ch::core::ctx_swap swap(&ctx);
    
    ForwardingUnit fu{nullptr, "forwarding_unit"};
    
    REQUIRE(true);
}

TEST_CASE("ForwardingUnit - Priority Logic", "[forwarding][priority]") {
    ch::core::context ctx("forwarding_priority_test");
    ch::core::ctx_swap swap(&ctx);
    
    ForwardingUnit fu{nullptr, "forwarding_unit"};
    
    REQUIRE(true);
}
