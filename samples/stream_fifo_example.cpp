#include "chlib/stream.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;
using namespace chlib;

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("stream_fifo_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // 创建时钟和复位信号
    ch_bool clk = false;
    ch_bool rst = true;

    // 创建Stream接口
    Stream<ch_uint<8>> input_stream;
    input_stream.payload = 0_d;
    input_stream.valid = false;
    input_stream.ready = false;

    ch::Simulator sim(ctx.get());

    // 初始化FIFO
    auto fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
    sim.tick();

    // 复位
    rst = false;
    clk = true;
    fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
    sim.tick();

    std::cout << "Stream FIFO Example:" << std::endl;
    std::cout << "Initial state - FIFO empty: " << simulator.get_value(fifo_result.empty) << std::endl;

    // 写入第一个数据
    clk = false;
    input_stream.payload = 0x55_d;
    input_stream.valid = true;
    fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
    sim.tick();

    clk = true;
    fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
    sim.tick();

    std::cout << "After writing 0x55 - FIFO empty: " << simulator.get_value(fifo_result.empty) 
              << ", FIFO full: " << simulator.get_value(fifo_result.full) 
              << ", Occupancy: " << simulator.get_value(fifo_result.occupancy) << std::endl;

    // 写入第二个数据
    clk = false;
    input_stream.payload = 0xAA_d;
    input_stream.valid = true;
    fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
    sim.tick();

    clk = true;
    fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
    sim.tick();

    std::cout << "After writing 0xAA - FIFO empty: " << simulator.get_value(fifo_result.empty) 
              << ", FIFO full: " << simulator.get_value(fifo_result.full) 
              << ", Occupancy: " << simulator.get_value(fifo_result.occupancy) << std::endl;

    // 读取数据
    Stream<ch_uint<8>> output_stream = fifo_result.pop_stream;
    output_stream.ready = true;  // 设置输出端就绪

    // 重新运行FIFO，使用更新的输出就绪信号
    input_stream.valid = false;  // 停止写入
    fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
    // 手动更新输出就绪信号，这在实际应用中需要更复杂的交互
    fifo_result.pop_stream.ready = true;

    std::cout << "FIFO example completed successfully!" << std::endl;

    return 0;
}