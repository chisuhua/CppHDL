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

TEST_CASE("AXI PWM - Register Read/Write", "[axi_pwm][register]") {
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    ch::Simulator sim(pwm_device.context());
    
    // 初始化
    sim.set_input_value(pwm_device.instance().io().bready, 0);
    sim.set_input_value(pwm_device.instance().io().rready, 0);
    sim.tick();  // Initial tick to stabilize state machine
    sim.tick();  // Second tick for stability
    
    // 测试周期寄存器
    axi_lite_write(sim, pwm_device, 0x00, 1023);
    uint32_t val = axi_lite_read(sim, pwm_device, 0x00);
    CHECK(val == 1023);
    
    // 测试占空比寄存器
    axi_lite_write(sim, pwm_device, 0x10, 256);
    val = axi_lite_read(sim, pwm_device, 0x10);
    CHECK(val == 256);
    
    axi_lite_write(sim, pwm_device, 0x14, 512);
    val = axi_lite_read(sim, pwm_device, 0x14);
    CHECK(val == 512);
    
    axi_lite_write(sim, pwm_device, 0x18, 768);
    val = axi_lite_read(sim, pwm_device, 0x18);
    CHECK(val == 768);
    
    axi_lite_write(sim, pwm_device, 0x1C, 1000);
    val = axi_lite_read(sim, pwm_device, 0x1C);
    CHECK(val == 1000);
    
    // 测试使能寄存器
    axi_lite_write(sim, pwm_device, 0x20, 0xF);  // 使能所有通道
    val = axi_lite_read(sim, pwm_device, 0x20);
    CHECK(val == 0xF);
    
    // 测试中断寄存器
    axi_lite_write(sim, pwm_device, 0x24, 0x1);
    val = axi_lite_read(sim, pwm_device, 0x24);
    CHECK(val == 0x1);
}

TEST_CASE("AXI PWM - Counter Logic", "[axi_pwm][counter]") {
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    ch::Simulator sim(pwm_device.context());
    
    sim.set_input_value(pwm_device.instance().io().bready, 0);
    sim.set_input_value(pwm_device.instance().io().rready, 0);
    sim.tick();
    
    // 设置周期为 10
    axi_lite_write(sim, pwm_device, 0x00, 10);
    
    // 使能通道 0
    axi_lite_write(sim, pwm_device, 0x20, 0x1);
    
    // 设置占空比为 5 (50%)
    axi_lite_write(sim, pwm_device, 0x10, 5);
    
    // 运行多个周期，验证计数器行为
    int high_count = 0;
    int low_count = 0;
    
    for (int i = 0; i < 100; i++) {
        sim.tick();
        auto pwm_val = static_cast<uint64_t>(sim.get_port_value(pwm_device.instance().io().pwm_out_0));
        if (pwm_val) {
            high_count++;
        } else {
            low_count++;
        }
    }
    
    // 100 个周期中，应该大约有 50% 高电平
    CHECK(high_count == 50);
    CHECK(low_count == 50);
}

TEST_CASE("AXI PWM - Duty Cycle Control", "[axi_pwm][duty]") {
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    ch::Simulator sim(pwm_device.context());
    
    sim.set_input_value(pwm_device.instance().io().bready, 0);
    sim.set_input_value(pwm_device.instance().io().rready, 0);
    sim.tick();
    
    // 设置周期为 100
    axi_lite_write(sim, pwm_device, 0x00, 100);
    axi_lite_write(sim, pwm_device, 0x20, 0x1);  // 使能通道 0
    
    struct TestCase {
        uint32_t duty;
        float expected_percent;
    };
    
    TestCase tests[] = {
        {0, 0.0f},      // 0% 占空比
        {25, 25.0f},    // 25% 占空比
        {50, 50.0f},    // 50% 占空比
        {75, 75.0f},    // 75% 占空比
        {100, 100.0f},  // 100% 占空比
    };
    
    for (const auto& tc : tests) {
        axi_lite_write(sim, pwm_device, 0x10, tc.duty);
        
        int high_count = 0;
        // 运行一个完整周期 (101 个时钟：0-100)
        for (int i = 0; i < 101; i++) {
            sim.tick();
            auto pwm_val = static_cast<uint64_t>(sim.get_port_value(pwm_device.instance().io().pwm_out_0));
            if (pwm_val) {
                high_count++;
            }
        }
        
        float actual_percent = (high_count / 101.0f) * 100.0f;
        
        // 允许±5% 的误差
        float diff = std::abs(actual_percent - tc.expected_percent);
        CHECK(diff < 5.0f);
    }
}

TEST_CASE("AXI PWM - Enable Control", "[axi_pwm][enable]") {
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    ch::Simulator sim(pwm_device.context());
    
    sim.set_input_value(pwm_device.instance().io().bready, 0);
    sim.set_input_value(pwm_device.instance().io().rready, 0);
    sim.tick();
    
    // 设置周期和占空比
    axi_lite_write(sim, pwm_device, 0x00, 100);
    axi_lite_write(sim, pwm_device, 0x10, 50);
    
    // 禁用通道
    axi_lite_write(sim, pwm_device, 0x20, 0x0);
    
    for (int i = 0; i < 20; i++) {
        sim.tick();
        auto pwm_val = static_cast<uint64_t>(sim.get_port_value(pwm_device.instance().io().pwm_out_0));
        CHECK(pwm_val == 0);
    }
    
    // 使能通道
    axi_lite_write(sim, pwm_device, 0x20, 0x1);
    
    bool saw_high = false;
    bool saw_low = false;
    for (int i = 0; i < 150; i++) {
        sim.tick();
        auto pwm_val = static_cast<uint64_t>(sim.get_port_value(pwm_device.instance().io().pwm_out_0));
        if (pwm_val) saw_high = true;
        else saw_low = true;
    }
    
    CHECK(saw_high);
    CHECK(saw_low);
}

TEST_CASE("AXI PWM - Multi-Channel Independent", "[axi_pwm][multichannel]") {
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    ch::Simulator sim(pwm_device.context());
    
    sim.set_input_value(pwm_device.instance().io().bready, 0);
    sim.set_input_value(pwm_device.instance().io().rready, 0);
    sim.tick();
    
    // 设置周期
    axi_lite_write(sim, pwm_device, 0x00, 100);
    
    // 设置不同占空比
    axi_lite_write(sim, pwm_device, 0x10, 25);   // CH0: 25%
    axi_lite_write(sim, pwm_device, 0x14, 50);   // CH1: 50%
    axi_lite_write(sim, pwm_device, 0x18, 75);   // CH2: 75%
    axi_lite_write(sim, pwm_device, 0x1C, 100);  // CH3: 100%
    
    // 使能所有通道
    axi_lite_write(sim, pwm_device, 0x20, 0xF);
    
    // 统计各通道高电平计数
    int high[4] = {0, 0, 0, 0};
    
    for (int i = 0; i < 101; i++) {
        sim.tick();
        for (int ch = 0; ch < 4; ch++) {
            bool pwm_val = false;
            switch (ch) {
                case 0: pwm_val = static_cast<uint64_t>(sim.get_port_value(pwm_device.instance().io().pwm_out_0)); break;
                case 1: pwm_val = static_cast<uint64_t>(sim.get_port_value(pwm_device.instance().io().pwm_out_1)); break;
                case 2: pwm_val = static_cast<uint64_t>(sim.get_port_value(pwm_device.instance().io().pwm_out_2)); break;
                case 3: pwm_val = static_cast<uint64_t>(sim.get_port_value(pwm_device.instance().io().pwm_out_3)); break;
            }
            if (pwm_val) high[ch]++;
        }
    }
    
    // 验证各通道占空比 (允许±3 误差)
    CHECK(((high[0] >= 22) && (high[0] <= 28)));  // 25%
    CHECK(((high[1] >= 47) && (high[1] <= 53)));  // 50%
    CHECK(((high[2] >= 72) && (high[2] <= 78)));  // 75%
    CHECK(((high[3] >= 98) && (high[3] <= 101))); // 100%
}

TEST_CASE("AXI PWM - Interrupt Generation", "[axi_pwm][interrupt]") {
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    ch::Simulator sim(pwm_device.context());
    
    sim.set_input_value(pwm_device.instance().io().bready, 0);
    sim.set_input_value(pwm_device.instance().io().rready, 0);
    sim.tick();
    
    // 设置小周期以便快速测试
    axi_lite_write(sim, pwm_device, 0x00, 5);
    
    // 清除中断标志
    axi_lite_write(sim, pwm_device, 0x24, 0x0);
    
    // 运行并检测中断
    int interrupt_count = 0;
    for (int i = 0; i < 100; i++) {
        sim.tick();
        auto irq_val = static_cast<uint64_t>(sim.get_port_value(pwm_device.instance().io().irq));
        if (irq_val) {
            interrupt_count++;
        }
    }
    
    // 周期为 5，100 个时钟应该有大约 20 次溢出
    CHECK(interrupt_count >= 18);
    CHECK(interrupt_count <= 22);
    
    // 验证中断标志寄存器
    uint32_t int_status = axi_lite_read(sim, pwm_device, 0x24);
    CHECK((int_status & 0x1) == 0x1);
}

TEST_CASE("AXI PWM - Verilog Generation", "[axi_pwm][verilog]") {
    auto ctx = std::make_unique<ch::core::context>("test_pwm_verilog");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    
    // Generate Verilog
    std::string vlog_file = "/workspace/CppHDL/tests/output/axi_pwm.v";
    
    // Create output directory if needed
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
    CHECK(content.find("wdata") != std::string::npos);
    CHECK(content.find("araddr") != std::string::npos);
    CHECK(content.find("rdata") != std::string::npos);
    CHECK(content.find("pwm_out_0") != std::string::npos);
    CHECK(content.find("pwm_out_1") != std::string::npos);
    CHECK(content.find("pwm_out_2") != std::string::npos);
    CHECK(content.find("pwm_out_3") != std::string::npos);
    CHECK(content.find("irq") != std::string::npos);
    
    std::cout << "Verilog generated: " << vlog_file << std::endl;
}

TEST_CASE("AXI PWM - Default Values", "[axi_pwm][default]") {
    ch::ch_device<AxiLitePwm<16, 4>> pwm_device;
    ch::Simulator sim(pwm_device.context());
    
    sim.set_input_value(pwm_device.instance().io().bready, 0);
    sim.set_input_value(pwm_device.instance().io().rready, 0);
    sim.tick();
    
    // 读取默认值
    uint32_t period = axi_lite_read(sim, pwm_device, 0x00);
    CHECK(period == 255);  // 默认周期 255
    
    uint32_t duty0 = axi_lite_read(sim, pwm_device, 0x10);
    CHECK(duty0 == 128);  // 默认 50% 占空比
    
    uint32_t enable = axi_lite_read(sim, pwm_device, 0x20);
    CHECK(enable == 0);  // 默认禁用
}
