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
    auto ctx = std::make_unique<ch::core::context>("stream_arbiter_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // 创建时钟和复位信号
    ch_bool clk = false;
    ch_bool rst = true;

    // 创建4个输入Stream
    std::array<Stream<ch_uint<8>>, 4> input_streams;
    input_streams[0].payload = 0x11_d;
    input_streams[0].valid = true;
    input_streams[1].payload = 0x22_d;
    input_streams[1].valid = false;  // 这个无效
    input_streams[2].payload = 0x33_d;
    input_streams[2].valid = true;
    input_streams[3].payload = 0x44_d;
    input_streams[3].valid = false;  // 这个也无效

    std::cout << "Stream Arbiter Example:" << std::endl;

    // 创建仲裁器
    auto arb_result = stream_arbiter_round_robin<ch_uint<8>, 4>(clk, rst, input_streams);
    
    std::cout << "Input 0 payload: 0x" << std::hex << simulator.get_value(input_streams[0].payload) << std::endl;
    std::cout << "Input 0 valid: " << std::dec << simulator.get_value(input_streams[0].valid) << std::endl;
    std::cout << "Input 1 payload: 0x" << std::hex << simulator.get_value(input_streams[1].payload) << std::endl;
    std::cout << "Input 1 valid: " << std::dec << simulator.get_value(input_streams[1].valid) << std::endl;
    std::cout << "Input 2 payload: 0x" << std::hex << simulator.get_value(input_streams[2].payload) << std::endl;
    std::cout << "Input 2 valid: " << std::dec << simulator.get_value(input_streams[2].valid) << std::endl;
    std::cout << "Input 3 payload: 0x" << std::hex << simulator.get_value(input_streams[3].payload) << std::endl;
    std::cout << "Input 3 valid: " << std::dec << simulator.get_value(input_streams[3].valid) << std::endl;
    
    std::cout << "\nArbiter Output:" << std::endl;
    std::cout << "Output payload: 0x" << std::hex << simulator.get_value(arb_result.output_stream.payload) << std::endl;
    std::cout << "Output valid: " << std::dec << simulator.get_value(arb_result.output_stream.valid) << std::endl;
    std::cout << "Selected input index: " << simulator.get_value(arb_result.selected) << std::endl;

    // 复位后测试
    rst = false;
    clk = true;
    auto arb_result2 = stream_arbiter_round_robin<ch_uint<8>, 4>(clk, rst, input_streams);
    
    std::cout << "\nAfter reset:" << std::endl;
    std::cout << "Output payload: 0x" << std::hex << simulator.get_value(arb_result2.output_stream.payload) << std::endl;
    std::cout << "Output valid: " << std::dec << simulator.get_value(arb_result2.output_stream.valid) << std::endl;
    std::cout << "Selected input index: " << simulator.get_value(arb_result2.selected) << std::endl;

    // 现在让第二个输入也有效，看看仲裁器如何选择
    input_streams[1].valid = true;
    auto arb_result3 = stream_arbiter_round_robin<ch_uint<8>, 4>(clk, rst, input_streams);
    
    std::cout << "\nWhen input 1 becomes valid:" << std::endl;
    std::cout << "Input 1 valid: " << simulator.get_value(input_streams[1].valid) << std::endl;
    std::cout << "Output payload: 0x" << std::hex << simulator.get_value(arb_result3.output_stream.payload) << std::endl;
    std::cout << "Output valid: " << std::dec << simulator.get_value(arb_result3.output_stream.valid) << std::endl;
    std::cout << "Selected input index: " << simulator.get_value(arb_result3.selected) << std::endl;

    std::cout << "\nStream Arbiter example completed successfully!" << std::endl;

    return 0;
}