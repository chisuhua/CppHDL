/**
 * AXI4-Lite SoC Integration Example
 * 
 * 集成 AXI4 矩阵 + GPIO + UART
 * （**4 个 IP**：Test Master + Matrix 2x2 + GPIO + UART）
 * 
 * 系统架构:
 * ```
 *                  ┌─────────────────┐
 *                  │  AXI Matrix 2x2 │
 *                  │                 │
 *   Master 0 ──────┤                 ├───▶ Slave 0 (GPIO @ 0x0000_0000)
 *   (Test Master)  │                 │
 *   Master 1 ──────┤                 ├───▶ Slave 1 (UART @ 0x8000_0000)
 *   (Reserved)     │                 │
 *                  └─────────────────┘
 * ```
 */

#include "ch.hpp"
#include "axi_soc_example_top.h"
#include "axi4/axi4_lite_slave.h"
#include "axi4/axi4_lite_matrix.h"
#include "axi4/peripherals/axi_gpio.h"
#include "axi4/peripherals/axi_uart.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>
#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace axi4;
using axi4::example::AxiLiteTestMaster;
using axi4::example::AxiSocTop;

// ============================================================================
// 测试主设备 (简化版)
// ============================================================================


// ============================================================================
// 顶层 SoC
// ============================================================================


// ============================================================================
// 主函数 - 系统验证
// ============================================================================

int main() {
    std::cout << "CppHDL - AXI4-Lite SoC Integration" << std::endl;
    std::cout << "===================================" << std::endl;
    
    ch::ch_device<AxiSocTop> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化
    sim.set_input_value(top_device.instance().io().start, false);
    sim.set_input_value(top_device.instance().io().write, false);
    // 释放从设备复位（低有效）
    sim.set_input_value(top_device.instance().io().rst_n, true);
    // GPIO 输入给一个非零值 —— 读回 DATA_IN 才能证明读通道真的取到了数据
    sim.set_input_value(top_device.instance().io().gpio_in, 0x5a_h);
    sim.set_input_value(top_device.instance().io().uart_rx, true);
    
    std::cout << "\n=== Test 1: Write GPIO DATA_OUT (addr=0x00) ===" << std::endl;
    
    // 写 GPIO DATA_OUT 寄存器 (地址 0x00)
    sim.set_input_value(top_device.instance().io().addr, 0_d);
    sim.set_input_value(top_device.instance().io().wdata, 0xAB_h);
    sim.set_input_value(top_device.instance().io().write, true);
    sim.set_input_value(top_device.instance().io().start, true);
    sim.tick();
    sim.set_input_value(top_device.instance().io().start, false);
    
    // 等待完成
    for (int i = 0; i < 10; ++i) {
        sim.tick();
        auto done = sim.get_port_value(top_device.instance().io().done);
        if (static_cast<uint64_t>(done)) {
            std::cout << "Write complete!" << std::endl;
            break;
        }
    }
    
    // 等 master 从写状态回到 idle（done 与 idle 差一拍 —— master 的状态在
    // 响应握手后的**下一个**时钟沿才回到 idle）
    sim.tick();

    auto gpio_out = sim.get_port_value(top_device.instance().io().gpio_out);
    std::cout << "GPIO_OUT = 0x" << std::hex << static_cast<uint64_t>(gpio_out) << std::dec << std::endl;
    
    std::cout << "\n=== Test 2: Read GPIO DATA_IN (addr=0x04) ===" << std::endl;
    
    // 读 GPIO DATA_IN 寄存器 (地址 0x04)
    sim.set_input_value(top_device.instance().io().addr, 4_d);
    sim.set_input_value(top_device.instance().io().write, false);
    sim.set_input_value(top_device.instance().io().start, true);
    sim.tick();
    sim.set_input_value(top_device.instance().io().start, false);
    
    // 等待完成
    bool read_ok = false;
    for (int i = 0; i < 10; ++i) {
        sim.tick();
        auto done = sim.get_port_value(top_device.instance().io().done);
        auto rdata = sim.get_port_value(top_device.instance().io().rdata);
        
        if (static_cast<uint64_t>(done)) {
            std::cout << "Read complete! DATA_IN = 0x" << std::hex << static_cast<uint64_t>(rdata) << std::dec << std::endl;
            read_ok = true;
            break;
        }
    }
    if (!read_ok) {
        // 读事务未完成 —— 属真实失败（不再是"已知限制"），直接以非零码退出，
        // 避免 example 静默通过。
        std::cerr << "ERROR: AXI4-Lite read transaction did not complete"
                  << std::endl;
        return 1;
    }

    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("axi_soc_example.v", top_device.context());
    std::cout << "Verilog generated: axi_soc_example.v" << std::endl;
    
    std::cout << "\n✅ AXI4-Lite SoC integration test completed!" << std::endl;
    return 0;
}
