#include "chlib/stream.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;
using namespace chlib;

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("stream_join_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // 创建两个输入Stream
    std::array<Stream<ch_uint<8>>, 2> input_streams;
    input_streams[0].payload = 0x12_d;
    input_streams[0].valid = true;
    input_streams[1].payload = 0x34_d;
    input_streams[1].valid = true;

    std::cout << "Stream Join Example:" << std::endl;

    // 创建Join组件
    auto join_result = stream_join<ch_uint<8>, 2>(input_streams);
    
    std::cout << "Input 0 payload: 0x" << std::hex << simulator.get_value(input_streams[0].payload) << std::endl;
    std::cout << "Input 1 payload: 0x" << std::hex << simulator.get_value(input_streams[1].payload) << std::endl;
    std::cout << "Input 0 valid: " << std::dec << simulator.get_value(input_streams[0].valid) << std::endl;
    std::cout << "Input 1 valid: " << std::dec << simulator.get_value(input_streams[1].valid) << std::endl;
    std::cout << "Output payload: 0x" << std::hex << simulator.get_value(join_result.output_stream.payload) << std::endl;
    std::cout << "Output valid: " << std::dec << simulator.get_value(join_result.output_stream.valid) << std::endl;
    
    // 验证只有当所有输入都有效时，输出才有效
    std::cout << "Output ready: " << simulator.get_value(join_result.output_stream.ready) << std::endl;
    std::cout << "Input 0 ready (when output ready): " << simulator.get_value(join_result.input_streams[0].ready) << std::endl;
    std::cout << "Input 1 ready (when output ready): " << simulator.get_value(join_result.input_streams[1].ready) << std::endl;

    // 测试当一个输入无效时的情况
    input_streams[1].valid = false;
    auto join_result2 = stream_join<ch_uint<8>, 2>(input_streams);
    
    std::cout << "\nWhen one input is invalid:" << std::endl;
    std::cout << "Input 0 valid: " << simulator.get_value(input_streams[0].valid) << std::endl;
    std::cout << "Input 1 valid: " << simulator.get_value(input_streams[1].valid) << std::endl;
    std::cout << "Output valid: " << simulator.get_value(join_result2.output_stream.valid) << std::endl;

    std::cout << "\nStream Join example completed successfully!" << std::endl;

    return 0;
}