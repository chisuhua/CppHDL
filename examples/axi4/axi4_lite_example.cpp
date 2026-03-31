/**
 * AXI4-Lite Example - 简化版
 * 
 * 演示 AXI4-Lite 从设备的基本功能
 */

#include "ch.hpp"
#include "axi4/axi4_lite.h"
#include "axi4/axi4_lite_slave.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace axi4;

// ============================================================================
// 顶层模块
// ============================================================================

class AxiLiteTop : public ch::Component {
public:
    __io(
        // AXI4-Lite 从设备接口
        ch_in<ch_uint<32>> awaddr;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;
        
        ch_in<ch_uint<32>> wdata;
        ch_in<ch_uint<4>> wstrb;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;
        
        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;
        
        ch_in<ch_uint<32>> araddr;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;
        
        ch_out<ch_uint<32>> rdata;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;
    )
    
    AxiLiteTop(ch::Component* parent = nullptr, const std::string& name = "axi_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // AXI4-Lite 从设备 (4 个寄存器)
        ch::ch_module<AxiLiteSlave<32, 32, 4>> slave{"slave"};
        
        // 连接
        slave.io().awaddr <<= io().awaddr;
        slave.io().awprot <<= io().awprot;
        slave.io().awvalid <<= io().awvalid;
        io().awready <<= slave.io().awready;
        
        slave.io().wdata <<= io().wdata;
        slave.io().wstrb <<= io().wstrb;
        slave.io().wvalid <<= io().wvalid;
        io().wready <<= slave.io().wready;
        
        io().bresp <<= slave.io().bresp;
        io().bvalid <<= slave.io().bvalid;
        slave.io().bready <<= io().bready;
        
        slave.io().araddr <<= io().araddr;
        slave.io().arprot <<= io().arprot;
        slave.io().arvalid <<= io().arvalid;
        io().arready <<= slave.io().arready;
        
        io().rdata <<= slave.io().rdata;
        io().rresp <<= slave.io().rresp;
        io().rvalid <<= slave.io().rvalid;
        slave.io().rready <<= io().rready;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL - AXI4-Lite Example" << std::endl;
    std::cout << "==========================" << std::endl;
    
    ch::ch_device<AxiLiteTop> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化 AXI 信号
    sim.set_input_value(top_device.instance().io().awvalid, false);
    sim.set_input_value(top_device.instance().io().wvalid, false);
    sim.set_input_value(top_device.instance().io().arvalid, false);
    sim.set_input_value(top_device.instance().io().bready, true);
    sim.set_input_value(top_device.instance().io().rready, true);
    
    std::cout << "\n=== Test 1: Write Register 0 ===" << std::endl;
    
    // 写地址通道
    sim.set_input_value(top_device.instance().io().awaddr, 0_d);
    sim.set_input_value(top_device.instance().io().awprot, 0_d);
    sim.set_input_value(top_device.instance().io().awvalid, true);
    sim.tick();
    
    auto awready = sim.get_port_value(top_device.instance().io().awready);
    std::cout << "AWREADY = " << static_cast<uint64_t>(awready) << std::endl;
    
    // 写数据通道
    sim.set_input_value(top_device.instance().io().wdata, 305419896_d);  // 0x12345678
    sim.set_input_value(top_device.instance().io().wstrb, 0xF_d);
    sim.set_input_value(top_device.instance().io().wvalid, true);
    sim.tick();
    
    auto wready = sim.get_port_value(top_device.instance().io().wready);
    auto bvalid = sim.get_port_value(top_device.instance().io().bvalid);
    std::cout << "WREADY = " << static_cast<uint64_t>(wready) << std::endl;
    std::cout << "BVALID = " << static_cast<uint64_t>(bvalid) << std::endl;
    
    std::cout << "Write complete!" << std::endl;
    
    std::cout << "\n=== Test 2: Read Register 0 ===" << std::endl;
    
    // 读地址通道
    sim.set_input_value(top_device.instance().io().awvalid, false);
    sim.set_input_value(top_device.instance().io().wvalid, false);
    sim.set_input_value(top_device.instance().io().araddr, 0_d);
    sim.set_input_value(top_device.instance().io().arprot, 0_d);
    sim.set_input_value(top_device.instance().io().arvalid, true);
    sim.tick();
    
    auto arready = sim.get_port_value(top_device.instance().io().arready);
    auto rvalid = sim.get_port_value(top_device.instance().io().rvalid);
    auto rdata = sim.get_port_value(top_device.instance().io().rdata);
    
    std::cout << "ARREADY = " << static_cast<uint64_t>(arready) << std::endl;
    std::cout << "RVALID = " << static_cast<uint64_t>(rvalid) << std::endl;
    std::cout << "RDATA = 0x" << std::hex << static_cast<uint64_t>(rdata) << std::dec << std::endl;
    
    if (static_cast<uint64_t>(rdata) == 305419896) {
        std::cout << "✅ PASS: Data matches (0x12345678)!" << std::endl;
    } else {
        std::cout << "❌ FAIL: Expected 0x12345678" << std::endl;
    }
    
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("axi4_lite_example.v", top_device.context());
    std::cout << "Verilog generated: axi4_lite_example.v" << std::endl;
    
    std::cout << "\n✅ AXI4-Lite test completed!" << std::endl;
    return 0;
}
