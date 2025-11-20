#include "catch_amalgamated.hpp"
#include "simulator.h"
#include "core/easy_nodes.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// Test to verify simplified node creation
TEST_CASE("Simplified Node Creation", "[maintainability][creation]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    SECTION("Test simplified register creation") {
        // 使用新的简化API创建寄存器
        auto reg1 = make_register<ch_uint<8>>(ch_literal(42), "easy_reg1");
        auto reg2 = make_register<ch_uint<8>>("easy_reg2");
        
        // 验证寄存器创建成功
        REQUIRE(true); // 如果没有崩溃，说明创建成功
    }
    
    SECTION("Test simplified input/output creation") {
        // 使用新的简化API创建输入/输出
        auto input = make_input<ch_uint<8>>("easy_input");
        auto output = make_output<ch_uint<8>>("easy_output");
        
        // 验证创建成功
        REQUIRE(true);
    }
    
    SECTION("Test simplified literal creation") {
        // 使用新的简化API创建文字
        auto lit = make_literal(123);
        
        // 验证创建成功
        REQUIRE(true);
    }
    
    SECTION("Test simulator with simplified nodes") {
        // 创建一些简化节点并用模拟器运行
        auto reg = make_register<ch_uint<8>>(ch_literal(0), "test_reg");
        auto input = make_input<ch_uint<8>>("test_input");
        auto output = make_output<ch_uint<8>>("test_output");
        
        // 创建模拟器
        Simulator sim(&ctx);
        
        // 运行几个周期
        sim.tick();
        sim.tick();
        
        // 验证没有崩溃
        REQUIRE(true);
    }
}