#include "chlib/stream.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;
using namespace chlib;

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("stream_fork_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // 创建Stream接口
    Stream<ch_uint<8>> input_stream;
    input_stream.payload = 0xAB_d;
    input_stream.valid = true;

    std::cout << "Stream Fork Example:" << std::endl;

    // 创建同步模式的Fork (两个输出都必须就绪才传输)
    auto sync_fork_result = stream_fork<ch_uint<8>, 2>(input_stream, true);
    
    std::cout << "Synchronous Fork:" << std::endl;
    std::cout << "Input payload: 0x" << std::hex << simulator.get_value(input_stream.payload) << std::endl;
    std::cout << "Output 0 payload: 0x" << std::hex << simulator.get_value(sync_fork_result.output_streams[0].payload) << std::endl;
    std::cout << "Output 1 payload: 0x" << std::hex << simulator.get_value(sync_fork_result.output_streams[1].payload) << std::endl;
    std::cout << "Input valid: " << simulator.get_value(input_stream.valid) << std::endl;
    std::cout << "Output 0 valid: " << simulator.get_value(sync_fork_result.output_streams[0].valid) << std::endl;
    std::cout << "Output 1 valid: " << simulator.get_value(sync_fork_result.output_streams[1].valid) << std::endl;
    
    // 设置输出就绪信号，以验证输入是否就绪
    Stream<ch_uint<8>> temp_output0 = sync_fork_result.output_streams[0];
    Stream<ch_uint<8>> temp_output1 = sync_fork_result.output_streams[1];
    temp_output0.ready = true;
    temp_output1.ready = true;
    
    // 重新计算同步Fork以验证输入就绪信号
    // 在同步模式下，只有当所有输出都就绪时，输入才会就绪
    std::cout << "Synchronous Fork - Input ready (when both outputs ready): " 
              << simulator.get_value(sync_fork_result.input_stream.ready) << std::endl;

    // 创建异步模式的Fork (任意输出就绪即可传输)
    auto async_fork_result = stream_fork<ch_uint<8>, 2>(input_stream, false);
    
    std::cout << "\nAsynchronous Fork:" << std::endl;
    std::cout << "Input payload: 0x" << std::hex << simulator.get_value(input_stream.payload) << std::endl;
    std::cout << "Output 0 payload: 0x" << std::hex << simulator.get_value(async_fork_result.output_streams[0].payload) << std::endl;
    std::cout << "Output 1 payload: 0x" << std::hex << simulator.get_value(async_fork_result.output_streams[1].payload) << std::endl;
    std::cout << "Input valid: " << simulator.get_value(input_stream.valid) << std::endl;
    std::cout << "Output 0 valid: " << simulator.get_value(async_fork_result.output_streams[0].valid) << std::endl;
    std::cout << "Output 1 valid: " << simulator.get_value(async_fork_result.output_streams[1].valid) << std::endl;
    
    std::cout << "Asynchronous Fork - Input ready: " 
              << simulator.get_value(async_fork_result.input_stream.ready) << std::endl;

    std::cout << "\nStream Fork example completed successfully!" << std::endl;

    return 0;
}