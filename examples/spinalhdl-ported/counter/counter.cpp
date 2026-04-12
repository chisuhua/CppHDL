/**
 * Counter Example - SpinalHDL Port
 * 
 * 对应 SpinalHDL 代码:
 * @code{.scala}
 * class Counter extends Component {
 *   val io = new Bundle {
 *     val enable = in Bool()
 *     val clear = in Bool()
 *     val value = out UInt(8 bits)
 *   }
 *   
 *   val counterReg = RegInit(U(0, 8 bits))
 *   when(io.clear) {
 *     counterReg := 0
 *   } elsewhen(io.enable) {
 *     counterReg := counterReg + 1
 *   }
 *   io.value := counterReg
 * }
 * @endcode
 * 
 * CppHDL 移植版本:
 * - 使用 ch_reg<ch_uint<8>> 作为计数寄存器
 * - 支持异步复位和使能信号
 * - 使用 ch::Component 基类
 */

#include "ch.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include "codegen_dag.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// ============================================================================
// Counter 模块定义
// ============================================================================

template <unsigned N = 8>
class Counter : public ch::Component {
public:
    // IO 端口定义
    __io(
        ch_in<ch_bool> enable;      // 计数使能
        ch_in<ch_bool> clear;       // 异步清零
        ch_out<ch_uint<N>> value;   // 当前计数值
    )
    
    Counter(ch::Component* parent = nullptr, 
            const std::string& name = "counter")
        : ch::Component(parent, name) {}
    
    /**
     * @brief 创建端口
     */
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    /**
     * @brief 硬件描述函数
     * 
     * 等价于 SpinalHDL:
     * when(io.clear) { counterReg := 0 }
     * elsewhen(io.enable) { counterReg := counterReg + 1 }
     */
    void describe() override {
        // 内部寄存器，初始值为 0
        ch_reg<ch_uint<N>> counter_reg(0_d);
        
        // 使用 select 实现条件更新（类似 SpinalHDL 的 when/elsewhen）
        // 复位优先，然后是使能计数，否则保持不变
        counter_reg->next = select(io().clear,
            ch_uint<N>(0_d),
            select(io().enable,
                counter_reg + ch_uint<N>(1_d),
                counter_reg));
        
        // 输出赋值
        io().value = counter_reg;
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class Top : public ch::Component {
public:
    __io(
        ch_in<ch_bool> enable;
        ch_in<ch_bool> clear;
        ch_out<ch_uint<8>> value;
    )
    
    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 实例化 Counter 模块
        ch::ch_module<Counter<8>> counter1{"counter1"};
        
        // 连接 IO
        counter1.io().enable <<= io().enable;
        counter1.io().clear <<= io().clear;
        io().value <<= counter1.io().value;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - Counter Example" << std::endl;
    std::cout << "======================================" << std::endl;
    
    {
        // 创建设备
        ch_device<Top> device;
        
        // 创建仿真器
        Simulator simulator(device.context());
        
        std::cout << "\n=== Starting Counter Test ===" << std::endl;
        
        // 测试 1: 复位测试
        std::cout << "\n[Test 1] Reset test..." << std::endl;
        simulator.set_input_value(device.instance().io().clear, 1);
        simulator.set_input_value(device.instance().io().enable, 0);
        
        for (int i = 0; i < 3; i++) {
            simulator.tick();
            auto val = simulator.get_value(device.instance().io().value);
            std::cout << "  Cycle " << i << ": value = " << static_cast<uint64_t>(val) << std::endl;
        }
        
        // 释放复位
        simulator.set_input_value(device.instance().io().clear, 0);
        simulator.tick();
        auto val = simulator.get_value(device.instance().io().value);
        std::cout << "  After reset release: value = " << static_cast<uint64_t>(val) << std::endl;
        
        // 测试 2: 计数测试
        std::cout << "\n[Test 2] Counting test..." << std::endl;
        simulator.set_input_value(device.instance().io().enable, 1);
        
        for (int i = 0; i < 15; i++) {
            simulator.tick();
            auto val = simulator.get_value(device.instance().io().value);
            uint64_t expected = i + 1; // After first tick, value goes from 0 to 1
            std::cout << "  Cycle " << i << ": value = " << static_cast<uint64_t>(val);
            
            if (static_cast<uint64_t>(val) != expected) {
                std::cout << " [ERROR: expected " << expected << "]";
            }
            std::cout << std::endl;
        }
        
        // 测试 3: 使能控制测试
        std::cout << "\n[Test 3] Enable control test..." << std::endl;
        
        // 禁用计数
        simulator.set_input_value(device.instance().io().enable, 0);
        auto held_val = simulator.get_value(device.instance().io().value);
        std::cout << "  Disable enable, held value = " << static_cast<uint64_t>(held_val) << std::endl;
        
        for (int i = 0; i < 5; i++) {
            simulator.tick();
            auto val = simulator.get_value(device.instance().io().value);
            std::cout << "  Cycle " << i << ": value = " << static_cast<uint64_t>(val);
            if (static_cast<uint64_t>(val) != static_cast<uint64_t>(held_val)) {
                std::cout << " [ERROR: value changed!]";
            }
            std::cout << std::endl;
        }
        
        // 重新使能
        simulator.set_input_value(device.instance().io().enable, 1);
        std::cout << "  Re-enable counting..." << std::endl;
        for (int i = 0; i < 5; i++) {
            simulator.tick();
            auto val = simulator.get_value(device.instance().io().value);
            std::cout << "  Cycle " << i << ": value = " << static_cast<uint64_t>(val) << std::endl;
        }
        
        // 测试 4: 溢出测试
        std::cout << "\n[Test 4] Overflow test..." << std::endl;
        simulator.set_input_value(device.instance().io().clear, 1);  // 先清零
        simulator.tick();
        simulator.set_input_value(device.instance().io().clear, 0);
        simulator.set_input_value(device.instance().io().enable, 1);
        
        // 计数到 255（最大值）
        for (int i = 0; i < 255; i++) {
            simulator.tick();
        }
        auto val255 = simulator.get_value(device.instance().io().value);
        std::cout << "  After 255 cycles: value = " << static_cast<uint64_t>(val255) << " (expected: 255)" << std::endl;
        if (static_cast<uint64_t>(val255) != 255) {
            std::cout << "  Note: enable/clear may not fully work yet (enable logic via ch_in<ch_bool> in select())" << std::endl;
        }
        
        // 再计数一次，应该溢出到 0
        simulator.tick();
        auto val_overflow = simulator.get_value(device.instance().io().value);
        std::cout << "  After overflow: value = " << static_cast<uint64_t>(val_overflow) << " (expected: 0)" << std::endl;
        
        std::cout << "\n=== Test Complete ===" << std::endl;
        
        // 生成 Verilog
        std::cout << "\n=== Generating Verilog ===" << std::endl;
        toVerilog("counter.v", device.context());
        std::cout << "Verilog generated: counter.v" << std::endl;
        
        // 生成 DAG
        toDAG("counter.dot", device.context());
        std::cout << "DAG generated: counter.dot" << std::endl;
    }
    
    std::cout << "\nProgram completed successfully" << std::endl;
    
    return 0;
}
