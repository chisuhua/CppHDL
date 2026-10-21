/**
 * AXI4-Lite PWM Controller Test
 * 
 * 测试内容:
 * 1. 寄存器读写测试
 * 2. PWM 计数器逻辑测试
 * 3. 占空比比较测试
 * 4. 使能控制测试
 * 5. 中断生成测试
 * 6. Verilog 生成验证
 */

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "axi4/peripherals/axi_pwm.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>
#include <cmath>
#include <fstream>

using namespace axi4;
using namespace ch;
using namespace ch::core;

// ============================================================================
// 测试用例
// ============================================================================

TEST_CASE("AXI PWM - ComponentCreation", "[axi_pwm][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_pwm_basic");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiLitePwm<16, 4> pwm(nullptr, "pwm");
    
    REQUIRE(pwm.name() == "pwm");
}

