#include "chlib/stream.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include "core/literal.h"
#include <iostream>

using namespace ch::core;
using namespace chlib;

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("stream_demux_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // 创建输入Stream
    Stream<ch_uint<8>> input_stream;
    input_stream.payload = 0xAB_d;
    input_stream.valid = true;
    input_stream.ready = true;

    // 选择信号，选择输出端口1
    ch_uint<1> select_signal = 1_d;

    std::cout << "Stream Demux Example:" << std::endl;

    // 创建解复用器
    auto demux_result = stream_demux<ch_uint<8>, 2>(input_stream, select_signal);
    
    std::cout << "Input payload: 0x" << std::hex << simulator.get_value(input_stream.payload) << std::endl;
    std::cout << "Input valid: " << std::dec << simulator.get_value(input_stream.valid) << std::endl;
    std::cout << "Input ready: " << simulator.get_value(input_stream.ready) << std::endl;
    std::cout << "Select signal: " << simulator.get_value(select_signal) << std::endl;
    
    std::cout << "\nOutput streams:" << std::endl;
    std::cout << "Output 0 payload: 0x" << std::hex << simulator.get_value(demux_result.output_streams[0].payload) << std::endl;
    std::cout << "Output 0 valid: " << std::dec << simulator.get_value(demux_result.output_streams[0].valid) << std::endl;
    std::cout << "Output 0 ready: " << simulator.get_value(demux_result.output_streams[0].ready) << std::endl;
    
    std::cout << "Output 1 payload: 0x" << std::hex << simulator.get_value(demux_result.output_streams[1].payload) << std::endl;
    std::cout << "Output 1 valid: " << std::dec << simulator.get_value(demux_result.output_streams[1].valid) << std::endl;
    std::cout << "Output 1 ready: " << simulator.get_value(demux_result.output_streams[1].ready) << std::endl;

    // 测试选择另一个输出端口
    select_signal = 0_d;
    auto demux_result2 = stream_demux<ch_uint<8>, 2>(input_stream, select_signal);
    
    std::cout << "\nWhen select signal is 0:" << std::endl;
    std::cout << "Select signal: " << simulator.get_value(select_signal) << std::endl;
    std::cout << "Output 0 valid: " << simulator.get_value(demux_result2.output_streams[0].valid) << std::endl;
    std::cout << "Output 1 valid: " << simulator.get_value(demux_result2.output_streams[1].valid) << std::endl;

    // 演示输入就绪信号如何依赖于选中的输出
    std::cout << "\nInput ready (depends on selected output readiness): " 
              << simulator.get_value(demux_result2.input_stream.ready) << std::endl;

    std::cout << "\nStream Demux example completed successfully!" << std::endl;

    return 0;
}