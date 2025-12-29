#include "chlib/stream.h"
#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "simulator.h"
#include "core/literal.h"
#include "core/operators.h"
#include <iostream>
#include <vector>

using namespace ch::core;
using namespace chlib;

// 一个使用Stream接口的Component示例，模拟内部电路忙状态
class StreamFifoComponent : public ch::Component {
public:
    __io(
        ch_in<Stream<ch_uint<8>>> input_stream;
        ch_out<Stream<ch_uint<8>>> output_stream;
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
        // 创建一个内部FIFO
        ch_bool clk = ctx_->get_default_clk();
        ch_bool rst = ctx_->get_default_rst();
        
        // 创建内部FIFO结果结构
        auto internal_fifo = stream_fifo<ch_uint<8>, 16>(clk, rst, io().input_stream);
        
        // 模拟内部电路忙状态
        // 使用计数器来模拟周期性的忙状态
        ch_reg<ch_uint<4>> busy_counter(0_d);
        busy_counter->clk = clk;
        busy_counter->rst = rst;
        busy_counter->next = select(rst, 0_d, busy_counter + 1_d);
        
        // 内部电路忙的条件：每4个周期中有2个周期忙
        ch_bool internal_busy = (busy_counter < 2_d);
        
        // 模拟内部处理，连接到内部FIFO
        Stream<ch_uint<8>> internal_output;
        internal_output.payload = internal_fifo.pop_stream.payload;
        internal_output.valid = internal_fifo.pop_stream.valid && !internal_busy;
        internal_output.ready = io().output_stream.ready;
        
        // 连接内部逻辑到输出端口
        io().output_stream = internal_output;
        
        // 控制输入流的ready信号，实现反压
        // 当内部FIFO满或者内部电路忙时，不接受新数据
        io().input_stream.ready = !internal_fifo.full && !internal_busy;
    }
};

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("stream_handshake_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL Stream Handshake Example with Backpressure" << std::endl;
    std::cout << "=================================================" << std::endl;

    // 创建Stream FIFO组件实例
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
    fifo_comp.io().input_stream = input_stream;

    std::cout << "\nInitial State:" << std::endl;
    std::cout << "Input ready (from FIFO): " << simulator.get_value(fifo_comp.io().input_stream.ready) << std::endl;

    // 重置完成
    rst = true;
    sim.tick();  // tick会自动驱动默认时钟
    
    std::cout << "\nSimulating Stream Component Operations with Backpressure:" << std::endl;
    
    // 模拟多个时钟周期，演示反压机制
    for (int cycle = 0; cycle < 20; cycle++) {
        std::cout << "\nCycle " << cycle << ":" << std::endl;
        
        // 退出复位状态
        if (cycle == 0) {
            rst = false;
        }
        
        // 更新输入数据
        input_stream.payload = static_cast<uint8_t>(0x10 + cycle);
        
        // 根据FIFO输入端口的ready信号来决定是否发送数据 - 实现反压机制
        bool input_ready = simulator.get_value(fifo_comp.io().input_stream.ready);
        
        if (input_ready) {
            input_stream.valid = true;   // FIFO准备好接收，可以发送
            std::cout << "FIFO input ready, simulator sends data" << std::endl;
        } else {
            input_stream.valid = false;  // FIFO未准备好，暂停发送
            std::cout << "FIFO input not ready, simulator pauses sending (backpressure)" << std::endl;
        }
        
        // 模拟下游消费者行为，控制output_stream的ready信号
        // 在某些周期模拟消费者忙碌
        bool output_ready = (cycle % 3 != 0); // 每3个周期中有1个周期不就绪
        
        // 连接更新后的输入
        fifo_comp.io().input_stream = input_stream;
        
        // 显示当前状态
        std::cout << "Input valid: " << simulator.get_value(input_stream.valid) 
                  << ", Input payload: 0x" << std::hex << simulator.get_value(input_stream.payload) << std::dec << std::endl;
        std::cout << "Input ready (from FIFO): " << input_ready 
                  << ", Output ready (simulator): " << output_ready << std::endl;
        
        // 从FIFO获取输出数据
        Stream<ch_uint<8>> output_stream = fifo_comp.io().output_stream;
        output_stream.ready = ch::core::literal<bool>(output_ready); // 根据模拟的消费者状态设置ready信号
        
        std::cout << "Output valid: " << simulator.get_value(output_stream.valid) 
                  << ", Output payload: 0x" << std::hex 
                  << simulator.get_value(output_stream.payload) << std::dec << std::endl;
        
        // tick函数会自动驱动默认时钟
        sim.tick();
    }

    std::cout << "\nStream handshake example with backpressure completed successfully!" << std::endl;

    return 0;
}