/**
 * GPIO (General Purpose Input/Output) Example - SpinalHDL Port
 * 
 * 对应 SpinalHDL GPIO 示例：
 * @code{.scala}
 * class GPIO extends Component {
 *   val io = new Bundle {
 *     val pin = in Bool()
 *     val interrupt = out Bool()
 *     val value = out UInt(8 bits)
 *   }
 *   
 *   // 输入同步
 *   val sync = Reg(Bool()) init(false)
 *   sync := io.pin
 *   
 *   // 边沿检测
 *   val rise = io.pin & !sync
 *   val fall = !io.pin & sync
 *   
 *   // 中断
 *   io.interrupt := rise | fall
 *   
 *   // 输入锁存
 *   io.value := io.pin ## sync ## ... // 8 位 GPIO
 * }
 * @endcode
 * 
 * CppHDL 移植版本 - IO 映射 + 中断 + 边沿检测
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
// GPIO 模块 - 8 位 GPIO 带中断
// ============================================================================

class GPIO : public ch::Component {
public:
    __io(
        // 8 位 GPIO 输入
        ch_in<ch_uint<8>> gpio_in;
        
        // 8 位 GPIO 输出
        ch_out<ch_uint<8>> gpio_out;
        
        // 方向控制 (0=input, 1=output)
        ch_in<ch_bool> output_enable;
        
        // 中断标志
        ch_out<ch_bool> interrupt;
        
        // 中断使能
        ch_in<ch_bool> interrupt_enable;
    )
    
    GPIO(ch::Component* parent = nullptr, const std::string& name = "gpio")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 输入同步寄存器（消除亚稳态）
        ch_reg<ch_uint<8>> sync_stage1(0_d);
        ch_reg<ch_uint<8>> sync_stage2(0_d);
        
        // 双缓冲同步
        sync_stage1->next = io().gpio_in;
        sync_stage2->next = sync_stage1;
        
        // 边沿检测
        // 上升沿：当前为 1 且前一周期为 0
        auto rise = io().gpio_in & (~sync_stage2);
        
        // 下降沿：当前为 0 且前一周期为 1
        auto fall = (~io().gpio_in) & sync_stage2;
        
        // 中断：上升沿或下降沿
        auto edge_detect = rise | fall;
        
        // 中断输出（带使能）
        io().interrupt = select(io().interrupt_enable, 
                                (edge_detect != ch_uint<8>(0_d)),
                                ch_bool(false));
        
        // GPIO 输出逻辑
        // 当 output_enable=1 时，输出寄存器值；否则输出高阻（用 0 模拟）
        ch_reg<ch_uint<8>> output_reg(0_d);
        io().gpio_out = select(io().output_enable, output_reg, ch_uint<8>(0_d));
        
        // 输出寄存器更新（简单示例：直接锁存输入）
        output_reg->next = io().gpio_in;
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class GpioTop : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> gpio_in;
        ch_out<ch_uint<8>> gpio_out;
        ch_in<ch_bool> output_enable;
        ch_out<ch_bool> interrupt;
        ch_in<ch_bool> interrupt_enable;
    )
    
    GpioTop(ch::Component* parent = nullptr, const std::string& name = "gpio_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<GPIO> gpio_inst{"gpio_inst"};
        
        gpio_inst.io().gpio_in <<= io().gpio_in;
        gpio_inst.io().output_enable <<= io().output_enable;
        gpio_inst.io().interrupt_enable <<= io().interrupt_enable;
        io().gpio_out <<= gpio_inst.io().gpio_out;
        io().interrupt <<= gpio_inst.io().interrupt;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - GPIO Example" << std::endl;
    std::cout << "===================================" << std::endl;
    
    ch::ch_device<GpioTop> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化输入
    sim.set_input_value(top_device.instance().io().gpio_in, 0);
    sim.set_input_value(top_device.instance().io().output_enable, false);
    sim.set_input_value(top_device.instance().io().interrupt_enable, true);
    
    std::cout << "\n=== Test 1: Input Mode ===" << std::endl;
    
    // 测试输入模式
    for (int cycle = 0; cycle < 10; ++cycle) {
        sim.tick();
        
        auto gpio_out = sim.get_port_value(top_device.instance().io().gpio_out);
        auto interrupt = sim.get_port_value(top_device.instance().io().interrupt);
        
        std::cout << "Cycle " << cycle << ": gpio_out=0x" << std::hex
                  << static_cast<uint64_t>(gpio_out) << std::dec
                  << ", interrupt=" << static_cast<uint64_t>(interrupt)
                  << std::endl;
        
        // 模拟输入变化
        if (cycle == 2) {
            sim.set_input_value(top_device.instance().io().gpio_in, 0xFF);
            std::cout << "  [Input changed to 0xFF]" << std::endl;
        } else if (cycle == 5) {
            sim.set_input_value(top_device.instance().io().gpio_in, 0x00);
            std::cout << "  [Input changed to 0x00]" << std::endl;
        }
    }
    
    std::cout << "\n=== Test 2: Output Mode ===" << std::endl;
    
    // 切换到输出模式
    sim.set_input_value(top_device.instance().io().output_enable, true);
    sim.set_input_value(top_device.instance().io().gpio_in, 0xAA);
    
    for (int cycle = 0; cycle < 5; ++cycle) {
        sim.tick();
        
        auto gpio_out = sim.get_port_value(top_device.instance().io().gpio_out);
        
        std::cout << "Cycle " << cycle << ": gpio_out=0x" << std::hex
                  << static_cast<uint64_t>(gpio_out) << std::dec
                  << " (output mode)" << std::endl;
    }
    
    std::cout << "\n=== Test 3: Interrupt Test ===" << std::endl;
    
    // 切换回输入模式，测试中断
    sim.set_input_value(top_device.instance().io().output_enable, false);
    sim.set_input_value(top_device.instance().io().gpio_in, 0x00);
    
    // 等待同步稳定
    for (int i = 0; i < 3; i++) sim.tick();
    
    // 产生上升沿
    std::cout << "Generating rising edge..." << std::endl;
    sim.set_input_value(top_device.instance().io().gpio_in, 0x01);
    sim.tick();
    auto int_val = sim.get_port_value(top_device.instance().io().interrupt);
    std::cout << "  interrupt=" << static_cast<uint64_t>(int_val) << " (should be 1)" << std::endl;
    
    // 产生下降沿
    std::cout << "Generating falling edge..." << std::endl;
    sim.set_input_value(top_device.instance().io().gpio_in, 0x00);
    sim.tick();
    int_val = sim.get_port_value(top_device.instance().io().interrupt);
    std::cout << "  interrupt=" << static_cast<uint64_t>(int_val) << " (should be 1)" << std::endl;
    
    // 测试中断禁用
    std::cout << "Testing interrupt disable..." << std::endl;
    sim.set_input_value(top_device.instance().io().interrupt_enable, false);
    sim.set_input_value(top_device.instance().io().gpio_in, 0xFF);
    sim.tick();
    int_val = sim.get_port_value(top_device.instance().io().interrupt);
    std::cout << "  interrupt=" << static_cast<uint64_t>(int_val) << " (should be 0)" << std::endl;
    
    // 生成 Verilog
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("gpio.v", top_device.context());
    std::cout << "Verilog generated: gpio.v" << std::endl;
    
    std::cout << "\nGPIO test completed successfully!" << std::endl;
    return 0;
}
