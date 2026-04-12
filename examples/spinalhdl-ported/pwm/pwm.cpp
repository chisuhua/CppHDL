/**
 * PWM (Pulse Width Modulation) Example - SpinalHDL Port
 * 
 * 对应 SpinalHDL PWM 示例：
 * @code{.scala}
 * class PWM extends Component {
 *   val io = new Bundle {
 *     val enable = in Bool()
 *     val value = in UInt(8 bits)  // Duty cycle
 *     val pwm = out Bool()
 *   }
 *   
 *   val counter = Reg(UInt(8 bits)) init(0)
 *   counter := counter + 1
 *   io.pwm = (counter < io.value) & io.enable
 * }
 * @endcode
 * 
 * CppHDL 移植版本 - 使用状态机 + 比较器
 */

#include "chlib/arithmetic.h"
#include "chlib/combinational.h"
#include "ch.hpp"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// ============================================================================
// PWM 模块
// ============================================================================

template <unsigned WIDTH = 8>
class PWM : public ch::Component {
public:
    __io(
        ch_in<ch_bool> enable;
        ch_in<ch_uint<WIDTH>> duty_cycle;  // 占空比 (0-255)
        ch_out<ch_bool> pwm_out;
    )
    
    PWM(ch::Component* parent = nullptr, const std::string& name = "pwm")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 8 位计数器 (0-255)
        ch_reg<ch_uint<WIDTH>> counter(0_d);
        
        // 计数器自增
        counter->next = counter + ch_uint<WIDTH>(1_d);
        
        // 使用 < 运算符直接比较
        // counter < duty_cycle 返回 ch_bool
        auto less_than = (counter < io().duty_cycle);
        
        // PWM 输出：当 counter < duty_cycle 且 enable 为高时输出高
        io().pwm_out = select(io().enable, less_than, ch_bool(false));
    }
};

// ============================================================================
// 顶层模块（带测试激励）
// ============================================================================

class PwmTop : public ch::Component {
public:
    __io(
        ch_out<ch_bool> pwm_out;
        ch_in<ch_bool> enable;
        ch_in<ch_uint<8>> duty_cycle;
    )
    
    PwmTop(ch::Component* parent = nullptr, const std::string& name = "pwm_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<PWM<8>> pwm_inst{"pwm_inst"};
        
        pwm_inst.io().enable <<= io().enable;
        pwm_inst.io().duty_cycle <<= io().duty_cycle;
        io().pwm_out <<= pwm_inst.io().pwm_out;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - PWM Example" << std::endl;
    std::cout << "=================================" << std::endl;
    
    ch::ch_device<PwmTop> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化输入
    sim.set_input_value(top_device.instance().io().enable, true);
    sim.set_input_value(top_device.instance().io().duty_cycle, 128);  // 50% 占空比
    
    std::cout << "\n=== PWM Simulation (50% Duty Cycle) ===" << std::endl;
    std::cout << "duty_cycle = 128 (50%), counter runs 0-255" << std::endl;
    std::cout << "\nFirst 20 cycles:" << std::endl;
    
    // 测试 50% 占空比
    int high_count = 0;
    int low_count = 0;
    
    for (int cycle = 0; cycle < 20; ++cycle) {
        sim.tick();
        
        auto pwm_val = sim.get_port_value(top_device.instance().io().pwm_out);
        uint64_t pwm = static_cast<uint64_t>(pwm_val);
        
        std::cout << "Cycle " << cycle << ": pwm=" << pwm << std::endl;
        
        if (pwm) high_count++;
        else low_count++;
    }
    
    // 测试不同占空比
    std::cout << "\n=== Testing Different Duty Cycles ===" << std::endl;
    
    uint8_t test_duties[] = {0, 64, 128, 192, 255};
    
    for (uint8_t duty : test_duties) {
        sim.set_input_value(top_device.instance().io().duty_cycle, duty);
        
        // 等待一个完整周期 (256 cycles)
        high_count = 0;
        for (int cycle = 0; cycle < 256; ++cycle) {
            sim.tick();
            auto pwm_val = sim.get_port_value(top_device.instance().io().pwm_out);
            if (static_cast<uint64_t>(pwm_val)) high_count++;
        }
        
        float expected_percent = (duty / 255.0f) * 100.0f;
        float actual_percent = (high_count / 256.0f) * 100.0f;
        
        std::cout << "Duty=" << (int)duty << " (" << expected_percent << "%): "
                  << "High=" << high_count << "/256 (" << actual_percent << "%)"
                  << std::endl;
    }
    
    // 测试使能信号
    std::cout << "\n=== Testing Enable Signal ===" << std::endl;
    sim.set_input_value(top_device.instance().io().duty_cycle, 128);
    
    std::cout << "Enable=1: pwm should toggle" << std::endl;
    sim.set_input_value(top_device.instance().io().enable, true);
    for (int i = 0; i < 5; i++) {
        sim.tick();
        auto pwm_val = sim.get_port_value(top_device.instance().io().pwm_out);
        std::cout << "  pwm=" << static_cast<uint64_t>(pwm_val) << std::endl;
    }
    
    std::cout << "Enable=0: pwm should be 0" << std::endl;
    sim.set_input_value(top_device.instance().io().enable, false);
    for (int i = 0; i < 5; i++) {
        sim.tick();
        auto pwm_val = sim.get_port_value(top_device.instance().io().pwm_out);
        std::cout << "  pwm=" << static_cast<uint64_t>(pwm_val) << std::endl;
        if (static_cast<uint64_t>(pwm_val) != 0) {
            std::cerr << "FAILURE: pwm_out should be 0 when enable=0" << std::endl;
            return 1;
        }
    }
    
    // 生成 Verilog
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("pwm.v", top_device.context());
    std::cout << "Verilog generated: pwm.v" << std::endl;
    
    std::cout << "\nPWM test completed successfully!" << std::endl;
    return 0;
}
