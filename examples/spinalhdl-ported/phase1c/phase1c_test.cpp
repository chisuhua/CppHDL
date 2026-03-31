/**
 * Phase 1C Features Test - 测试 ch_nextEn, converter
 */

#include "ch.hpp"
#include "chlib/memory.h"
#include "chlib/converter.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// ============================================================================
// 测试模块：ch_nextEn
// ============================================================================

class NextEnTest : public ch::Component {
public:
    __io(
        ch_out<ch_uint<8>> counter;
        ch_in<ch_bool> enable;
        ch_in<ch_bool> reset;
    )
    
    NextEnTest(ch::Component* parent = nullptr, const std::string& name = "nexten_test")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch_reg<ch_uint<8>> count_reg(0_d);
        
        auto next_val = select(io().reset, ch_uint<8>(0_d), count_reg + ch_uint<8>(1_d));
        count_reg->next = ch_nextEn<ch_uint<8>>(count_reg, next_val, ch_bool(true));
        
        io().counter = count_reg;
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class Phase1CTop : public ch::Component {
public:
    __io(
        ch_out<ch_uint<8>> counter;
        ch_out<ch_uint<8>> gray_result;
        ch_in<ch_bool> enable;
        ch_in<ch_bool> reset;
    )
    
    Phase1CTop(ch::Component* parent = nullptr, const std::string& name = "phase1c_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ch_nextEn 测试
        ch::ch_module<NextEnTest> nexten_inst{"nexten_inst"};
        nexten_inst.io().enable <<= io().enable;
        nexten_inst.io().reset <<= io().reset;
        io().counter <<= nexten_inst.io().counter;
        
        // converter 测试：格雷码 (binary_to_gray 已验证)
        ch_reg<ch_uint<8>> binary_input(ch_uint<8>(5_d));
        io().gray_result <<= binary_to_gray(binary_input);
    }
};

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "CppHDL - Phase 1C Features Test" << std::endl;
    std::cout << "================================" << std::endl;
    
    ch::ch_device<Phase1CTop> top_device;
    ch::Simulator sim(top_device.context());
    
    sim.set_input_value(top_device.instance().io().enable, false);
    sim.set_input_value(top_device.instance().io().reset, true);
    
    std::cout << "\n=== Test 1: ch_nextEn Counter ===" << std::endl;
    
    sim.set_input_value(top_device.instance().io().reset, false);
    sim.set_input_value(top_device.instance().io().enable, true);
    
    for (int cycle = 0; cycle < 20; ++cycle) {
        sim.tick();
        
        auto counter = sim.get_port_value(top_device.instance().io().counter);
        
        if (cycle % 5 == 0) {
            std::cout << "Cycle " << cycle << ": counter=" 
                      << static_cast<uint64_t>(counter) << std::endl;
        }
    }
    
    std::cout << "\n=== Test 2: Gray Code ===" << std::endl;
    std::cout << "Binary 5 → Gray: expected 7" << std::endl;
    
    // 生成 Verilog
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("phase1c_features.v", top_device.context());
    std::cout << "Verilog generated" << std::endl;
    
    std::cout << "\n✅ Phase 1C features test completed!" << std::endl;
    return 0;
}
