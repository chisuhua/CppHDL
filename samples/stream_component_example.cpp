#include "chlib/stream.h"
#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "simulator.h"
#include <iostream>
#include <vector>

using namespace ch::core;
using namespace chlib;

// 一个使用Stream接口的Component示例
class StreamFifoComponent : public ch::Component {
public:
    __io(
        ch_in<ch_bool> clk;
        ch_in<ch_bool> rst;
        ch_in<Stream<ch_uint<8>>> input_stream;
        ch_out<Stream<ch_uint<8>>> output_stream;
        ch_out<ch_bool> empty;
        ch_out<ch_bool> full;
        ch_out<ch_uint<6>> occupancy;  // log2(32)+1 = 6位
    )

    StreamFifoComponent(ch::Component* parent = nullptr, const std::string& name = "stream_fifo_comp") 
        : ch::Component(parent, name) {
        create_ports();
        describe();
    }

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // 在describe函数中实现FIFO逻辑
        auto fifo_result = stream_fifo<ch_uint<8>, 32>(
            io().clk,
            io().rst,
            io().input_stream
        );
        
        // 连接内部逻辑到输出端口
        io().output_stream = fifo_result.pop_stream;
        io().empty = fifo_result.empty;
        io().full = fifo_result.full;
        io().occupancy = fifo_result.occupancy;
    }
};

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("stream_component_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL Stream Component Example" << std::endl;
    std::cout << "=================================" << std::endl;

    // 创建Stream FIFO组件实例 - create_ports()和describe()会自动调用
    StreamFifoComponent fifo_comp;

    // 创建仿真器
    ch::Simulator sim(ctx.get());
    
    std::cout << "\nComponent hierarchy:" << std::endl;
    std::cout << "FIFO Component name: " << fifo_comp.name() << std::endl;

    // 使用默认的时钟和复位信号
    ch_bool& clk = ctx->get_default_clk();
    ch_bool& rst = ctx->get_default_rst();
    
    Stream<ch_uint<8>> input_stream;
    input_stream.payload = 0_d;
    input_stream.valid = false;
    input_stream.ready = false;
    
    // 连接组件输入
    fifo_comp.io().clk = clk;
    fifo_comp.io().rst = rst;
    fifo_comp.io().input_stream = input_stream;

    std::cout << "\nInitial State:" << std::endl;
    std::cout << "FIFO empty: " << simulator.get_value(fifo_comp.io().empty) 
              << ", FIFO full: " << simulator.get_value(fifo_comp.io().full) 
              << ", Occupancy: " << simulator.get_value(fifo_comp.io().occupancy) << std::endl;

    // 重置完成
    rst = true;
    sim.tick();  // tick会自动驱动默认时钟
    
    std::cout << "\nSimulating Stream Component Operations with Backpressure:" << std::endl;
    
    // 模拟多个时钟周期，演示反压机制
    for (int cycle = 0; cycle < 10; cycle++) {
        std::cout << "\nCycle " << cycle << ":" << std::endl;
        
        // 退出复位状态
        if (cycle == 0) {
            rst = false;
        }
        
        // 检查FIFO是否满，如果是满的，我们不应该发送数据（反压机制）
        bool fifo_full = simulator.get_value(fifo_comp.io().full);
        bool fifo_almost_full = simulator.get_value(fifo_comp.io().occupancy) >= 30; // 接近满
        
        // 更新输入数据
        input_stream.payload = static_cast<uint8_t>(0x10 + cycle);
        
        // 根据FIFO的full信号决定是否发送数据 - 实现反压机制
        if (fifo_full || fifo_almost_full) {
            input_stream.valid = false;  // FIFO满了，暂停发送
            std::cout << "FIFO is full/almost full, simulator pauses sending (backpressure)" << std::endl;
        } else {
            input_stream.valid = true;   // FIFO未满，可以发送
            std::cout << "FIFO has space, simulator sends data" << std::endl;
        }
        
        // 连接更新后的输入
        fifo_comp.io().input_stream = input_stream;
        
        // 显示当前状态
        std::cout << "Input valid: " << simulator.get_value(input_stream.valid) 
                  << ", Input payload: 0x" << std::hex << simulator.get_value(input_stream.payload) << std::dec << std::endl;
        std::cout << "FIFO empty: " << simulator.get_value(fifo_comp.io().empty) 
                  << ", FIFO full: " << simulator.get_value(fifo_comp.io().full) 
                  << ", Occupancy: " << simulator.get_value(fifo_comp.io().occupancy) << std::endl;
        
        // tick函数会自动驱动默认时钟
        sim.tick();
    }

    std::cout << "\nStream component example with backpressure completed successfully!" << std::endl;

    return 0;
}