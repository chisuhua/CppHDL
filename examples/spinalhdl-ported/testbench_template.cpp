/**
 * Testbench Template for CppHDL SpinalHDL Port Examples
 * 
 * 用途：为 SpinalHDL 移植示例提供标准测试平台模板
 * 
 * 使用说明:
 * 1. 复制此模板到新示例目录
 * 2. 替换 <Module> 为你的模块名
 * 3. 实现 setup() 中的 IO 初始化
 * 4. 实现 run_test() 中的测试逻辑
 */

#include "ch.hpp"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include "codegen_dag.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// ============================================================================
// 被测模块 (DUT - Device Under Test)
// ============================================================================

template <typename T>
class ExampleModule : public ch::Component {
public:
    // TODO: 定义 IO 端口
    __io(
        // ch_in<...> input_signal;
        // ch_out<...> output_signal;
    )
    
    ExampleModule(ch::Component* parent = nullptr, 
                  const std::string& name = "example")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // TODO: 实现硬件逻辑
        // 参考：samples/counter.cpp
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class Top : public ch::Component {
public:
    // TODO: 定义顶层 IO（可选，用于外部测试）
    __io(
        // ch_out<ch_uint<8>> output;
    )
    
    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // TODO: 实例化被测模块
        // CH_MODULE(ExampleModule<...>, dut);
        
        // TODO: 连接 IO
        // io().output <<= dut.io().output;
    }
};

// ============================================================================
// 测试平台
// ============================================================================

class Testbench {
private:
    Top& top;
    Simulator& sim;
    int cycle_count;
    
public:
    Testbench(Top& t, Simulator& s) : top(t), sim(s), cycle_count(0) {}
    
    /**
     * @brief 生成一个时钟周期
     */
    void tick() {
        sim.tick();
        cycle_count++;
    }
    
    /**
     * @brief 生成多个时钟周期
     */
    void tick(int n) {
        for (int i = 0; i < n; i++) {
            tick();
        }
    }
    
    /**
     * @brief 获取当前周期数
     */
    int get_cycle() const {
        return cycle_count;
    }
    
    /**
     * @brief 初始化测试平台
     * 
     * TODO: 实现 IO 信号初始化
     */
    void setup() {
        std::cout << "=== Testbench Setup ===" << std::endl;
        
        // 示例：初始化输入信号
        // top.io().input_signal = false;
        
        std::cout << "Setup completed" << std::endl;
    }
    
    /**
     * @brief 运行测试
     * 
     * TODO: 实现测试逻辑
     */
    void run_test() {
        std::cout << "\n=== Starting Test ===" << std::endl;
        
        // 示例测试流程：
        
        // 1. 复位测试
        std::cout << "[Test 1] Reset test..." << std::endl;
        // top.io().reset = true;
        // tick(3);
        // top.io().reset = false;
        // tick(1);
        
        // 2. 功能测试
        std::cout << "[Test 2] Functionality test..." << std::endl;
        // top.io().enable = true;
        // for (int i = 0; i < 10; i++) {
        //     tick();
        //     auto val = sim.get_value(top.io().output);
        //     std::cout << "  Cycle " << i << ": output = " << val << std::endl;
        // }
        
        // 3. 边界测试
        std::cout << "[Test 3] Boundary test..." << std::endl;
        // ...
        
        std::cout << "\n=== Test Complete ===" << std::endl;
    }
};

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - Example Module Test" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    {
        // 创建设备
        ch_device<Top> device;
        
        // 创建仿真器
        Simulator simulator(device.context());
        
        // 创建测试平台
        Testbench tb(device.instance(), simulator);
        
        // 初始化
        tb.setup();
        
        // 运行测试
        tb.run_test();
        
        // 生成 Verilog
        std::cout << "\n=== Generating Verilog ===" << std::endl;
        toVerilog("example_module.v", device.context());
        std::cout << "Verilog generated: example_module.v" << std::endl;
        
        // 生成 DAG
        toDAG("example_module.dot", device.context());
        std::cout << "DAG generated: example_module.dot" << std::endl;
    }
    
    std::cout << "\nProgram completed successfully" << std::endl;
    
    return 0;
}
