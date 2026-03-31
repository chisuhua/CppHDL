/**
 * Assert Example - 断言系统使用示例 (简化版)
 */

#include "ch.hpp"
#include "chlib/assert.h"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// ============================================================================
// 简单计数器
// ============================================================================

template <unsigned WIDTH = 8>
class SimpleCounter : public ch::Component {
public:
    __io(
        ch_out<ch_uint<WIDTH>> counter;
        ch_in<ch_bool> enable;
        ch_in<ch_bool> reset;
    )
    
    SimpleCounter(ch::Component* parent = nullptr, const std::string& name = "counter")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch_reg<ch_uint<WIDTH>> count_reg(0_d);
        
        auto overflow = (count_reg == ch_uint<WIDTH>((1 << WIDTH) - 1));
        count_reg->next = select(io().reset, ch_uint<WIDTH>(0_d),
                                 select(io().enable,
                                        select(overflow, ch_uint<WIDTH>(0_d),
                                               count_reg + ch_uint<WIDTH>(1_d)),
                                        count_reg));
        
        io().counter = count_reg;
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class AssertTop : public ch::Component {
public:
    __io(
        ch_out<ch_uint<8>> counter;
        ch_in<ch_bool> enable;
        ch_in<ch_bool> reset;
    )
    
    AssertTop(ch::Component* parent = nullptr, const std::string& name = "assert_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<SimpleCounter<8>> counter_inst{"counter_inst"};
        
        counter_inst.io().enable <<= io().enable;
        counter_inst.io().reset <<= io().reset;
        io().counter <<= counter_inst.io().counter;
    }
};

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "CppHDL - Assert Example" << std::endl;
    std::cout << "=======================" << std::endl;
    
    // 演示 ch_assert 宏的使用
    ch_assert(true, "This is a compile-time assert demo");
    
    ch::ch_device<AssertTop> top_device;
    ch::Simulator sim(top_device.context());
    
    sim.set_input_value(top_device.instance().io().enable, false);
    sim.set_input_value(top_device.instance().io().reset, true);
    
    std::cout << "\n=== Test 1: Counter with Reset ===" << std::endl;
    
    sim.set_input_value(top_device.instance().io().reset, false);
    sim.set_input_value(top_device.instance().io().enable, true);
    
    for (int cycle = 0; cycle < 50; ++cycle) {
        sim.tick();
        
        auto counter = sim.get_port_value(top_device.instance().io().counter);
        
        if (cycle % 10 == 0) {
            std::cout << "Cycle " << cycle << ": counter=" 
                      << static_cast<uint64_t>(counter) << std::endl;
        }
    }
    
    std::cout << "\n=== Test 2: Reset ===" << std::endl;
    sim.set_input_value(top_device.instance().io().reset, true);
    sim.tick();
    
    auto counter = sim.get_port_value(top_device.instance().io().counter);
    std::cout << "After reset: counter=" << static_cast<uint64_t>(counter) << std::endl;
    
    if (static_cast<uint64_t>(counter) == 0) {
        std::cout << "✅ PASS" << std::endl;
    } else {
        std::cout << "❌ FAIL" << std::endl;
    }
    
    // 生成 Verilog
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("assert_example.v", top_device.context());
    std::cout << "Verilog generated" << std::endl;
    
    std::cout << "\n✅ Assert example completed!" << std::endl;
    return 0;
}
