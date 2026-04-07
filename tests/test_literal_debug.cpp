#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/literal.h"
#include "core/context.h"

using namespace ch;
using namespace ch::core;

TEST_CASE("Debug literal 4_d", "[debug]") {
    auto lit = 4_d;
    std::cout << "lit.actual_value = " << lit.actual_value << std::endl;
    std::cout << "lit.actual_width = " << lit.actual_width << std::endl;
    
    REQUIRE(lit.actual_value == 4);
    REQUIRE(lit.actual_width == 3);  // bit_width(4) = 3
}

TEST_CASE("Debug ch_uint<3>(4_d)", "[debug]") {
    // 创建上下文，因为字面量创建需要活动的上下文
    auto ctx = std::make_unique<ch::core::context>("test_literal");
    ch::core::ctx_swap swap(ctx.get());
    
    ch_uint<3> val(4_d);
    std::cout << "ch_uint<3>(4_d) created" << std::endl;
    
    // 检查字面量节点是否正确创建
    REQUIRE(val.impl() != nullptr);
}
