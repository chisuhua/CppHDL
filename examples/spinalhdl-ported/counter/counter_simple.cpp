/**
 * Counter Simple Example - SpinalHDL Port (简化版)
 * 
 * 对应 SpinalHDL 代码:
 * @code{.scala}
 * class Counter extends Component {
 *   val io = new Bundle {
 *     val value = out UInt(8 bits)
 *   }
 *   
 *   val counterReg = RegInit(U(0, 8 bits))
 *   counterReg := counterReg + 1
 *   io.value := counterReg
 * }
 * @endcode
 * 
 * CppHDL 简化版本 - 自由运行计数器
 */

#include "ch.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include "codegen_dag.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// ============================================================================
// Counter 模块定义（简化版 - 无输入控制）
// ============================================================================

template <unsigned N = 8>
class Counter : public ch::Component {
public:
    // IO 端口定义 - 只有输出
    __io(
        ch_out<ch_uint<N>> value;   // 当前计数值
    )
    
    Counter(ch::Component* parent = nullptr, 
            const std::string& name = "counter")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 内部寄存器，初始值为 0
        ch_reg<ch_uint<N>> counter_reg(0_d);
        
        // 每个周期 +1
        counter_reg->next = counter_reg + 1_d;
        
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
        ch_out<ch_uint<8>> value;
    )
    
    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 实例化 Counter 模块
        CH_MODULE(Counter<8>, counter1);
        
        // 连接 IO
        io().value <<= counter1.io().value;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - Counter Simple Example" << std::endl;
    std::cout << "============================================" << std::endl;
    
    {
        // 创建设备
        ch_device<Top> device;
        
        // 创建仿真器
        Simulator simulator(device.context());
        
        std::cout << "\n=== Starting Counter Test ===" << std::endl;
        
        // 运行仿真
        // 注意：CppHDL 仿真语义是在 tick 之后读取值，所以 Cycle 0 对应计数值 1
        for (int i = 0; i <= 15; i++) {
            simulator.tick();
            auto val = simulator.get_value(device.instance().io().value);
            uint64_t expected = i + 1;  // tick 后计数值 = 周期数 + 1
            std::cout << "  Cycle " << i << ": value = " << static_cast<uint64_t>(val);
            
            if (static_cast<uint64_t>(val) != expected) {
                std::cout << " [ERROR: expected " << expected << "]";
                std::cerr << "FAILURE: value mismatch at cycle " << i << std::endl;
                return 1;
            } else {
                std::cout << " [OK]";
            }
            std::cout << std::endl;
        }
        
        // 溢出测试
        std::cout << "\n=== Overflow Test ===" << std::endl;
        // 再计数 239 次（当前是 16，需要到 255）
        for (int i = 0; i < 239; i++) {
            simulator.tick();
        }
        auto val255 = simulator.get_value(device.instance().io().value);
        std::cout << "  After 255 ticks: value = " << static_cast<uint64_t>(val255) << " (expected: 255)" << std::endl;
        if (static_cast<uint64_t>(val255) != 255) {
            std::cerr << "FAILURE: expected 255 after 255 ticks" << std::endl;
            return 1;
        }
        
        simulator.tick();
        auto val_overflow = simulator.get_value(device.instance().io().value);
        std::cout << "  After 256 ticks (overflow): value = " << static_cast<uint64_t>(val_overflow) << " (expected: 0)" << std::endl;
        if (static_cast<uint64_t>(val_overflow) != 0) {
            std::cerr << "FAILURE: expected 0 after overflow" << std::endl;
            return 1;
        }
        
        std::cout << "\n=== Test Complete ===" << std::endl;
        
        // 生成 Verilog
        std::cout << "\n=== Generating Verilog ===" << std::endl;
        toVerilog("counter_simple.v", device.context());
        std::cout << "Verilog generated: counter_simple.v" << std::endl;
        
        // 生成 DAG
        toDAG("counter_simple.dot", device.context());
        std::cout << "DAG generated: counter_simple.dot" << std::endl;
    }
    
    std::cout << "\nProgram completed successfully" << std::endl;
    
    return 0;
}
