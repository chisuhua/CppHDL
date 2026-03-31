/**
 * Sigma-Delta DAC Example - SpinalHDL Port
 * 
 * 功能:
 * - 一阶 Sigma-Delta 调制器
 * - 将多比特输入转换为 1 比特输出
 * - 可配置输入位宽
 * - 过采样率可配置
 * 
 * Sigma-Delta 原理:
 * - 累加器不断累加输入值
 * - 当累加器溢出时输出 1，否则输出 0
 * - 输出的平均值等于输入值
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
// Sigma-Delta DAC 模块
// ============================================================================

template <unsigned INPUT_WIDTH = 8>
class SigmaDeltaDAC : public ch::Component {
public:
    __io(
        // 数据输入
        ch_in<ch_uint<INPUT_WIDTH>> data_in;  // 输入数据
        
        // 调制器输出
        ch_out<ch_bool> dac_out;  // 1 比特 DAC 输出
        
        // 调试输出
        ch_out<ch_uint<INPUT_WIDTH+1>> accumulator;  // 累加器值
    )
    
    SigmaDeltaDAC(ch::Component* parent = nullptr, const std::string& name = "sd_dac")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        using namespace ch::core;
        
        // 累加器 (INPUT_WIDTH + 1 位，允许溢出)
        ch_reg<ch_uint<INPUT_WIDTH+1>> acc(0_d);
        
        // 累加器更新：acc = acc + data_in
        // 使用 select 实现加法，避免类型转换问题
        auto data_in_val = io().data_in;
        auto new_acc = acc + data_in_val;
        acc->next = new_acc;
        
        io().accumulator = acc;
        
        // DAC 输出：累加器最高位为 1 时输出 1
        auto msb = bits<INPUT_WIDTH, INPUT_WIDTH>(acc);
        io().dac_out = (msb != ch_bool(false));
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class SigmaDeltaTop : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> data_in;
        ch_out<ch_bool> dac_out;
        ch_out<ch_uint<9>> accumulator;
    )
    
    SigmaDeltaTop(ch::Component* parent = nullptr, const std::string& name = "sd_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<SigmaDeltaDAC<8>> dac{"dac"};
        
        dac.io().data_in <<= io().data_in;
        io().dac_out <<= dac.io().dac_out;
        io().accumulator <<= dac.io().accumulator;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - Sigma-Delta DAC" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "8-bit input, 1-bit output" << std::endl;
    
    ch::ch_device<SigmaDeltaTop> top_device;
    ch::Simulator sim(top_device.context());
    
    // 测试 1: 输入 50% 占空比 (128/256)
    std::cout << "\n=== Test 1: 50% Duty Cycle (input=128) ===" << std::endl;
    sim.set_input_value(top_device.instance().io().data_in, 128_d);
    
    unsigned ones_count = 0;
    for (int cycle = 0; cycle < 64; ++cycle) {
        sim.tick();
        
        auto dac_out = sim.get_port_value(top_device.instance().io().dac_out);
        auto acc = sim.get_port_value(top_device.instance().io().accumulator);
        
        if (static_cast<uint64_t>(dac_out)) {
            ones_count++;
        }
        
        if (cycle < 20) {
            std::cout << "Cyc " << cycle << ": dac_out=" << static_cast<uint64_t>(dac_out)
                      << ", acc=" << static_cast<uint64_t>(acc) << std::endl;
        }
    }
    
    double duty_cycle = (double)ones_count / 64.0 * 100.0;
    std::cout << "Ones: " << ones_count << "/64 (" << duty_cycle << "%)" << std::endl;
    
    // 测试 2: 输入 25% 占空比 (64/256)
    std::cout << "\n=== Test 2: 25% Duty Cycle (input=64) ===" << std::endl;
    sim.set_input_value(top_device.instance().io().data_in, 64_d);
    
    ones_count = 0;
    for (int cycle = 0; cycle < 64; ++cycle) {
        sim.tick();
        
        auto dac_out = sim.get_port_value(top_device.instance().io().dac_out);
        
        if (static_cast<uint64_t>(dac_out)) {
            ones_count++;
        }
        
        if (cycle < 20) {
            std::cout << "Cyc " << cycle << ": dac_out=" << static_cast<uint64_t>(dac_out) << std::endl;
        }
    }
    
    duty_cycle = (double)ones_count / 64.0 * 100.0;
    std::cout << "Ones: " << ones_count << "/64 (" << duty_cycle << "%)" << std::endl;
    
    // 测试 3: 输入 75% 占空比 (192/256)
    std::cout << "\n=== Test 3: 75% Duty Cycle (input=192) ===" << std::endl;
    sim.set_input_value(top_device.instance().io().data_in, 192_d);
    
    ones_count = 0;
    for (int cycle = 0; cycle < 64; ++cycle) {
        sim.tick();
        
        auto dac_out = sim.get_port_value(top_device.instance().io().dac_out);
        
        if (static_cast<uint64_t>(dac_out)) {
            ones_count++;
        }
        
        if (cycle < 20) {
            std::cout << "Cyc " << cycle << ": dac_out=" << static_cast<uint64_t>(dac_out) << std::endl;
        }
    }
    
    duty_cycle = (double)ones_count / 64.0 * 100.0;
    std::cout << "Ones: " << ones_count << "/64 (" << duty_cycle << "%)" << std::endl;
    
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("sigma_delta_dac.v", top_device.context());
    std::cout << "Verilog generated: sigma_delta_dac.v" << std::endl;
    
    std::cout << "\n✅ Sigma-Delta DAC test completed!" << std::endl;
    return 0;
}
