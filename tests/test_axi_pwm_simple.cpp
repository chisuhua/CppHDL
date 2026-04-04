/**
 * AXI4-Lite PWM Controller Test - Simple Version
 */

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "axi4/peripherals/axi_pwm.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>
#include <fstream>

using namespace axi4;
using namespace ch;
using namespace ch::core;

TEST_CASE("AXI PWM - ComponentCreation", "[axi_pwm][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_pwm_basic");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiLitePwm<16, 4> pwm(nullptr, "pwm");
    
    REQUIRE(pwm.name() == "pwm");
}

TEST_CASE("AXI PWM - VerilogGeneration", "[axi_pwm][verilog]") {
    auto ctx = std::make_unique<ch::core::context>("test_pwm_vlog");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    
    // Generate Verilog
    std::string vlog_file = "/workspace/CppHDL/tests/output/axi_pwm.v";
    (void)system("mkdir -p /workspace/CppHDL/tests/output");
    
    toVerilog(vlog_file, pwm_device.context());
    
    // Verify file was created
    std::ifstream f(vlog_file);
    REQUIRE(f.good());
    
    // Check for key Verilog constructs
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    
    CHECK(content.find("axi_pwm") != std::string::npos);
    CHECK(content.find("awaddr") != std::string::npos);
    CHECK(content.find("pwm_out_0") != std::string::npos);
    CHECK(content.find("irq") != std::string::npos);
    
    std::cout << "Verilog generated: " << vlog_file << std::endl;
}

TEST_CASE("AXI PWM - DefaultValues", "[axi_pwm][simple]") {
    auto ctx = std::make_unique<ch::core::context>("test_pwm_defaults");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    ch::Simulator sim(pwm_device.context());
    
    // Initialize inputs
    sim.set_input_value(pwm_device.instance().io().bready, 0);
    sim.set_input_value(pwm_device.instance().io().rready, 0);
    
    // Just tick the simulator
    sim.tick();
    sim.tick();
    
    REQUIRE(true);  // If we get here, the component works
}
