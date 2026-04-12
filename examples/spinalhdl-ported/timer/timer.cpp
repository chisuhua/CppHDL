/**
 * Timer Example - SpinalHDL Port
 * 
 * 功能:
 * - 可配置预分频器
 * - 自动重装载计数器
 * - 中断信号生成
 * 
 * CppHDL 移植版本
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
// Timer 模块
// ============================================================================

template <unsigned COUNT_WIDTH = 16, unsigned PRESCALE_WIDTH = 8>
class Timer : public ch::Component {
public:
    __io(
        ch_out<ch_bool> interrupt;     // 定时器中断
        ch_in<ch_bool> enable;         // 定时器使能
        ch_in<ch_bool> restart;        // 重启动计数器
        ch_in<ch_uint<COUNT_WIDTH>> period;  // 计数周期
        ch_in<ch_uint<PRESCALE_WIDTH>> prescale; // 预分频值
        ch_out<ch_uint<COUNT_WIDTH>> counter;  // 当前计数值
    )
    
    Timer(ch::Component* parent = nullptr, const std::string& name = "timer")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 计数器寄存器
        ch_reg<ch_uint<COUNT_WIDTH>> count_reg(0_d);
        
        // 预分频计数器
        ch_reg<ch_uint<PRESCALE_WIDTH>> prescale_reg(0_d);
        
        // 中断脉冲寄存器
        ch_reg<ch_bool> interrupt_reg(ch_bool(false));
        
        auto enable_sig = io().enable;
        auto restart_sig = io().restart;
        auto period_sig = io().period;
        auto prescale_sig = io().prescale;
        
        // ==================== 预分频器 ====================
        
        // 预分频计数器递增
        auto prescale_done = (prescale_reg == prescale_sig);
        prescale_reg->next = select(!enable_sig, ch_uint<PRESCALE_WIDTH>(0_d),
                                    select(restart_sig, ch_uint<PRESCALE_WIDTH>(0_d),
                                           select(prescale_done, ch_uint<PRESCALE_WIDTH>(0_d),
                                                  prescale_reg + ch_uint<PRESCALE_WIDTH>(1_d))));
        
        // ==================== 主计数器 ====================
        
        // 计数器递增 (每个 prescale 周期加 1)
        auto count_done = (count_reg == period_sig);
        count_reg->next = select(!enable_sig, ch_uint<COUNT_WIDTH>(0_d),
                                 select(restart_sig, ch_uint<COUNT_WIDTH>(0_d),
                                        select(prescale_done && count_done, ch_uint<COUNT_WIDTH>(0_d),
                                               select(prescale_done, count_reg + ch_uint<COUNT_WIDTH>(1_d),
                                                      count_reg))));
        
        io().counter = count_reg;
        
        // ==================== 中断生成 ====================
        
        // 当计数器达到周期值时生成中断脉冲
        // 使用 select 替代 &&
        auto interrupt_gen = select(enable_sig,
                                     select(prescale_done, count_done, ch_bool(false)),
                                     ch_bool(false));
        interrupt_reg->next = interrupt_gen;
        io().interrupt = interrupt_reg;
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class TimerTop : public ch::Component {
public:
    __io(
        ch_out<ch_bool> interrupt;
        ch_in<ch_bool> enable;
        ch_in<ch_bool> restart;
        ch_in<ch_uint<16>> period;
        ch_in<ch_uint<8>> prescale;
        ch_out<ch_uint<16>> counter;
    )
    
    TimerTop(ch::Component* parent = nullptr, const std::string& name = "timer_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<Timer<16, 8>> timer_inst{"timer_inst"};
        
        timer_inst.io().enable <<= io().enable;
        timer_inst.io().restart <<= io().restart;
        timer_inst.io().period <<= io().period;
        timer_inst.io().prescale <<= io().prescale;
        io().interrupt <<= timer_inst.io().interrupt;
        io().counter <<= timer_inst.io().counter;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - Timer Example" << std::endl;
    std::cout << "====================================" << std::endl;
    
    ch::ch_device<TimerTop> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化输入
    sim.set_input_value(top_device.instance().io().enable, false);
    sim.set_input_value(top_device.instance().io().restart, false);
    sim.set_input_value(top_device.instance().io().period, 9);    // 计数 0-9 (10 个周期)
    sim.set_input_value(top_device.instance().io().prescale, 3);  // 预分频 4 (0-3)
    
    std::cout << "\n=== Test 1: Basic Timer ===" << std::endl;
    std::cout << "Period: 10 (0-9), Prescale: 4 (0-3)" << std::endl;
    std::cout << "Expected: Interrupt every 40 cycles" << std::endl;
    
    sim.set_input_value(top_device.instance().io().enable, true);
    
    int interrupt_count = 0;
    for (int cycle = 0; cycle < 500; ++cycle) {
        sim.tick();
        
        auto interrupt = sim.get_port_value(top_device.instance().io().interrupt);
        auto counter = sim.get_port_value(top_device.instance().io().counter);
        
        if (cycle < 50 || cycle % 50 == 0 || static_cast<uint64_t>(interrupt)) {
            std::cout << "Cycle " << cycle << ": counter=" << static_cast<uint64_t>(counter)
                      << ", interrupt=" << static_cast<uint64_t>(interrupt) << std::endl;
        }
        
        if (static_cast<uint64_t>(interrupt)) {
            interrupt_count++;
            std::cout << "  >>> INTERRUPT! Count: " << interrupt_count << std::endl;
            
            if (interrupt_count >= 3) {
                break;
            }
        }
    }
    
    std::cout << "\nTotal interrupts: " << interrupt_count << std::endl;
    if (interrupt_count < 3) {
        std::cerr << "FAILURE: Expected at least 3 interrupts, got " << interrupt_count << std::endl;
        return 1;
    }
    
    // 生成 Verilog
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("timer.v", top_device.context());
    std::cout << "Verilog generated: timer.v" << std::endl;
    
    std::cout << "\nTimer test completed!" << std::endl;
    return 0;
}
