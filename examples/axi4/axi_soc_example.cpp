/**
 * AXI4-Lite SoC Integration Example
 * 
 * 集成 AXI4 矩阵 + GPIO + UART + Timer
 * 
 * 系统架构:
 * ```
 *                  ┌─────────────────┐
 *                  │  AXI Matrix 2x2 │
 *                  │                 │
 *   Master 0 ──────┤                 ├───▶ Slave 0 (GPIO @ 0x0000_0000)
 *   (Test Master)  │                 │
 *   Master 1 ──────┤                 ├───▶ Slave 1 (UART/Timer @ 0x8000_0000)
 *   (Reserved)     │                 │
 *                  └─────────────────┘
 * ```
 */

#include "ch.hpp"
#include "axi4/axi4_lite_slave.h"
#include "axi4/axi4_lite_matrix.h"
#include "axi4/peripherals/axi_gpio.h"
#include "axi4/peripherals/axi_uart.h"
#include "axi4/peripherals/axi_timer.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace axi4;

// ============================================================================
// 测试主设备 (简化版)
// ============================================================================

class AxiLiteTestMaster : public ch::Component {
public:
    __io(
        ch_in<ch_bool> start;
        ch_in<ch_bool> write;
        ch_in<ch_uint<32>> addr;
        ch_in<ch_uint<32>> wdata;
        ch_out<ch_uint<32>> rdata;
        ch_out<ch_bool> done;
        
        // AXI 主设备接口 (简化)
        ch_out<ch_uint<32>> awaddr;
        ch_out<ch_bool> awvalid;
        ch_in<ch_bool> awready;
        
        ch_out<ch_uint<32>> wdata_axi;
        ch_out<ch_bool> wvalid;
        ch_in<ch_bool> wready;
        
        ch_in<ch_bool> bvalid;
        ch_out<ch_bool> bready;
        
        ch_out<ch_uint<32>> araddr;
        ch_out<ch_bool> arvalid;
        ch_in<ch_bool> arready;
        
        ch_in<ch_uint<32>> rdata_axi;
        ch_in<ch_bool> rvalid;
        ch_out<ch_bool> rready;
    )
    
    AxiLiteTestMaster(ch::Component* parent = nullptr, const std::string& name = "test_master")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch_reg<ch_bool> state(ch_bool(false));
        
        // 写事务
        auto write_start = io().start && io().write && (!state);
        io().awvalid = write_start;
        io().awaddr = io().addr;
        io().wvalid = write_start && io().awready;
        io().wdata_axi = io().wdata;
        
        auto write_done = io().bvalid && io().bready;
        state->next = select(write_start, ch_bool(true), select(write_done, ch_bool(false), state));
        
        io().bready = io().bvalid && state;
        io().done = write_done;
        
        // 读事务
        auto read_start = io().start && (!io().write) && (!state);
        io().arvalid = read_start;
        io().araddr = io().addr;
        
        auto read_done = io().rvalid && io().rready;
        io().rready = io().rvalid && state;
        io().rdata = io().rdata_axi;
        
        state->next = select(read_start, ch_bool(true), select(read_done, ch_bool(false), state));
        
        io().done = read_done;
    }
};

// ============================================================================
// 顶层 SoC
// ============================================================================

class AxiSocTop : public ch::Component {
public:
    __io(
        ch_in<ch_bool> start;
        ch_in<ch_bool> write;
        ch_in<ch_uint<32>> addr;
        ch_in<ch_uint<32>> wdata;
        ch_out<ch_uint<32>> rdata;
        ch_out<ch_bool> done;
        
        // GPIO 物理接口
        ch_out<ch_uint<8>> gpio_out;
        ch_in<ch_uint<8>> gpio_in;
        
        // UART 物理接口
        ch_out<ch_bool> uart_tx;
        ch_in<ch_bool> uart_rx;
        
        // Timer IRQ
        ch_in<ch_bool> timer_irq;
    )
    
    AxiSocTop(ch::Component* parent = nullptr, const std::string& name = "axi_soc_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 测试主设备
        ch::ch_module<AxiLiteTestMaster> master{"master"};
        
        // AXI4 矩阵
        ch::ch_module<AxiLiteMatrix2x2<32, 32>> matrix{"matrix"};
        
        // 外设：GPIO (从设备 0)
        ch::ch_module<AxiLiteGpio<8>> gpio{"gpio"};
        
        // 外设：UART (从设备 1)
        ch::ch_module<AxiLiteUart<16>> uart{"uart"};
        
        // 连接主设备到矩阵
        matrix.io().m0_awaddr <<= master.io().awaddr;
        matrix.io().m0_awprot <<= ch_uint<2>(0_d);
        matrix.io().m0_awvalid <<= master.io().awvalid;
        master.io().awready <<= matrix.io().m0_awready;
        
        matrix.io().m0_wdata <<= master.io().wdata_axi;
        matrix.io().m0_wstrb <<= ch_uint<4>(0xF_d);
        matrix.io().m0_wvalid <<= master.io().wvalid;
        master.io().wready <<= matrix.io().m0_wready;
        
        master.io().bresp <<= matrix.io().m0_bresp;
        master.io().bvalid <<= matrix.io().m0_bvalid;
        matrix.io().m0_bready <<= master.io().bready;
        
        matrix.io().m0_araddr <<= master.io().araddr;
        matrix.io().m0_arprot <<= ch_uint<2>(0_d);
        matrix.io().m0_arvalid <<= master.io().arvalid;
        master.io().arready <<= matrix.io().m0_arready;
        
        master.io().rdata_axi <<= matrix.io().m0_rdata;
        master.io().rresp <<= matrix.io().m0_rresp;
        master.io().rvalid <<= matrix.io().m0_rvalid;
        matrix.io().m0_rready <<= master.io().rready;
        
        // 连接矩阵到 GPIO (从设备 0)
        gpio.io().awaddr <<= matrix.io().s0_awaddr;
        gpio.io().awprot <<= matrix.io().s0_awprot;
        gpio.io().awvalid <<= matrix.io().s0_awvalid;
        matrix.io().s0_awready <<= gpio.io().awready;
        
        gpio.io().wdata <<= matrix.io().s0_wdata;
        gpio.io().wstrb <<= matrix.io().s0_wstrb;
        gpio.io().wvalid <<= matrix.io().s0_wvalid;
        matrix.io().s0_wready <<= gpio.io().wready;
        
        matrix.io().s0_bresp <<= gpio.io().bresp;
        matrix.io().s0_bvalid <<= gpio.io().bvalid;
        gpio.io().bready <<= matrix.io().s0_bready;
        
        gpio.io().araddr <<= matrix.io().s0_araddr;
        gpio.io().arprot <<= matrix.io().s0_arprot;
        gpio.io().arvalid <<= matrix.io().s0_arvalid;
        matrix.io().s0_arready <<= gpio.io().arready;
        
        matrix.io().s0_rdata <<= gpio.io().rdata;
        matrix.io().s0_rresp <<= gpio.io().rresp;
        matrix.io().s0_rvalid <<= gpio.io().rvalid;
        gpio.io().rready <<= matrix.io().s0_rready;
        
        // GPIO 物理接口
        io().gpio_out <<= gpio.io().gpio_out;
        gpio.io().gpio_in <<= io().gpio_in;
        
        // UART 物理接口
        io().uart_tx <<= uart.io().uart_tx;
        uart.io().uart_rx <<= io().uart_rx;
        
        // 主设备控制接口
        master.io().start <<= io().start;
        master.io().write <<= io().write;
        master.io().addr <<= io().addr;
        master.io().wdata <<= io().wdata;
        io().rdata <<= master.io().rdata;
        io().done <<= master.io().done;
    }
};

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
    sim.set_input_value(top_device.instance().io().gpio_in, 0_d);
    sim.set_input_value(top_device.instance().io().uart_rx, true);
    
    std::cout << "\n=== Test 1: Write GPIO DATA_OUT (addr=0x00) ===" << std::endl;
    
    // 写 GPIO DATA_OUT 寄存器 (地址 0x00)
    sim.set_input_value(top_device.instance().io().addr, 0_d);
    sim.set_input_value(top_device.instance().io().wdata, 0xAB_d);
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
    for (int i = 0; i < 10; ++i) {
        sim.tick();
        auto done = sim.get_port_value(top_device.instance().io().done);
        auto rdata = sim.get_port_value(top_device.instance().io().rdata);
        
        if (static_cast<uint64_t>(done)) {
            std::cout << "Read complete! DATA_IN = 0x" << std::hex << static_cast<uint64_t>(rdata) << std::dec << std::endl;
            break;
        }
    }
    
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("axi_soc_example.v", top_device.context());
    std::cout << "Verilog generated: axi_soc_example.v" << std::endl;
    
    std::cout << "\n✅ AXI4-Lite SoC integration test completed!" << std::endl;
    return 0;
}
