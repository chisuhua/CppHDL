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
// 测试辅助函数
// ============================================================================

/**
 * AXI4-Lite 写操作
 */
template <typename T>
void axi_lite_write(ch::Simulator& sim, T& device, uint32_t addr, uint32_t data) {
    // 设置地址
    sim.set_port_value(device.instance().io().awaddr, addr);
    sim.set_port_value(device.instance().io().awprot, 0);
    sim.set_port_value(device.instance().io().awvalid, 1);
    
    // 等待握手
    while (!static_cast<uint64_t>(sim.get_port_value(device.instance().io().awready))) {
        sim.tick();
    }
    sim.tick();  // Extra tick for signal propagation
    sim.tick();  // Second tick for stability
    sim.set_port_value(device.instance().io().awvalid, 0);
    
    // 设置数据
    sim.set_port_value(device.instance().io().wdata, data);
    sim.set_port_value(device.instance().io().wstrb, 0xF);
    sim.set_port_value(device.instance().io().wvalid, 1);
    
    // 等待写响应
    while (!static_cast<uint64_t>(sim.get_port_value(device.instance().io().wready))) {
        sim.tick();
    }
    sim.tick();  // Extra tick for signal propagation
    sim.tick();  // Second tick for stability
    sim.set_port_value(device.instance().io().wvalid, 0);
    
    // 等待 B 响应
    while (!static_cast<uint64_t>(sim.get_port_value(device.instance().io().bvalid))) {
        sim.tick();
    }
    sim.tick();  // Extra tick for signal propagation
    sim.set_port_value(device.instance().io().bready, 1);
    sim.tick();
    sim.set_port_value(device.instance().io().bready, 0);
}

/**
 * AXI4-Lite 读操作
 */
template <typename T>
uint32_t axi_lite_read(ch::Simulator& sim, T& device, uint32_t addr) {
    // 设置地址
    sim.set_port_value(device.instance().io().araddr, addr);
    sim.set_port_value(device.instance().io().arprot, 0);
    sim.set_port_value(device.instance().io().arvalid, 1);
    
    // 等待握手
    while (!static_cast<uint64_t>(sim.get_port_value(device.instance().io().arready))) {
        sim.tick();
    }
    sim.tick();  // Extra tick for signal propagation
    sim.tick();  // Second tick for stability
    sim.set_port_value(device.instance().io().arvalid, 0);
    
    // 等待读数据
    while (!static_cast<uint64_t>(sim.get_port_value(device.instance().io().rvalid))) {
        sim.tick();
    }
    sim.tick();  // Extra tick for stable data
    
    uint32_t rdata = static_cast<uint64_t>(sim.get_port_value(device.instance().io().rdata));
    sim.set_port_value(device.instance().io().rready, 1);
    sim.tick();
    sim.set_port_value(device.instance().io().rready, 0);
    
    return rdata;
}

// ============================================================================
// 测试用例
// ============================================================================

TEST_CASE("AXI PWM - ComponentCreation", "[axi_pwm][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_pwm_basic");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiLitePwm<16, 4> pwm(nullptr, "pwm");
    
    REQUIRE(pwm.name() == "pwm");
}

