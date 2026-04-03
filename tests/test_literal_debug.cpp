#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/literal.h"

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
    ch_uint<3> val(4_d);
    std::cout << "ch_uint<3>(4_d) created" << std::endl;
    
    // 检查字面量节点是否正确创建
    REQUIRE(val.impl() != nullptr);
}
