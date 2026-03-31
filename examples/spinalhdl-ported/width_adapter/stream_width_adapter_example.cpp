/**
 * Stream Width Adapter Example - SpinalHDL Port
 * 
 * 使用移位寄存器模式实现 4x8-bit 到 32-bit 转换
 * 参考 samples/shift_register.cpp 的实现模式
 */

#include "ch.hpp"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// ============================================================================
// 4x8-bit Shift Register to 32-bit
// ============================================================================

class WidthAdapter : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> din;
        ch_in<ch_bool> shift_en;
        ch_out<ch_uint<32>> dout;
        ch_out<ch_bool> full;
    )
    
    WidthAdapter(ch::Component* parent = nullptr, const std::string& name = "width_adapter")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 4 个 8 位移位寄存器
        ch_reg<ch_uint<8>> byte0(0_d);
        ch_reg<ch_uint<8>> byte1(0_d);
        ch_reg<ch_uint<8>> byte2(0_d);
        ch_reg<ch_uint<8>> byte3(0_d);
        
        // 移位逻辑
        byte0->next = select(io().shift_en, io().din, byte0);
        byte1->next = select(io().shift_en, byte0, byte1);
        byte2->next = select(io().shift_en, byte1, byte2);
        byte3->next = select(io().shift_en, byte2, byte3);
        
        // 拼接为 32 位输出
        auto tmp0 = concat(byte1, byte0);
        auto tmp1 = concat(byte3, byte2);
        io().dout = concat(tmp1, tmp0);
        
        // full 信号 - 4 次移位后为高
        ch_reg<ch_uint<2>> counter(0_d);
        counter->next = select(io().shift_en, counter + ch_uint<2>(1_d), counter);
        io().full = select((counter == ch_uint<2>(3_d)), io().shift_en, ch_bool(false));
    }
};

// ============================================================================
// Top Module
// ============================================================================

class Top : public ch::Component {
public:
    __io(
        ch_out<ch_uint<32>> dout;
        ch_out<ch_bool> full;
        ch_in<ch_uint<8>> din;
        ch_in<ch_bool> shift_en;
    )
    
    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<WidthAdapter> adapter{"adapter"};
        
        adapter.io().din <<= io().din;
        adapter.io().shift_en <<= io().shift_en;
        io().dout <<= adapter.io().dout;
        io().full <<= adapter.io().full;
    }
};

// ============================================================================
// Main - Simulation Test
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - Width Adapter Example" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    ch::ch_device<Top> top_device;
    ch::Simulator sim(top_device.context());
    
    sim.set_input_value(top_device.instance().io().din, 0);
    sim.set_input_value(top_device.instance().io().shift_en, false);
    
    std::cout << "\n=== Testing 4x8-bit to 32-bit Conversion ===" << std::endl;
    std::cout << "Input sequence: 0x11, 0x22, 0x33, 0x44" << std::endl;
    std::cout << "Expected output: 0x44332211 (byte3.byte2.byte1.byte0)" << std::endl;
    
    for (int cycle = 0; cycle <= 8; ++cycle) {
        sim.tick();
        
        auto dout_val = sim.get_port_value(top_device.instance().io().dout);
        auto full_val = sim.get_port_value(top_device.instance().io().full);
        
        std::cout << "Cycle " << cycle << ": dout=0x" << std::hex
                  << static_cast<uint64_t>(dout_val)
                  << ", full=" << static_cast<uint64_t>(full_val)
                  << std::dec << std::endl;
        
        // 输入 4 个字节
        if (cycle >= 0 && cycle <= 3) {
            uint8_t data = 0x11 + cycle * 0x11;
            sim.set_input_value(top_device.instance().io().din, data);
            sim.set_input_value(top_device.instance().io().shift_en, true);
        } else {
            sim.set_input_value(top_device.instance().io().shift_en, false);
        }
    }
    
    // 生成 Verilog
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("width_adapter.v", top_device.context());
    std::cout << "Verilog generated: width_adapter.v" << std::endl;
    
    std::cout << "\nWidth adapter test completed!" << std::endl;
    return 0;
}
