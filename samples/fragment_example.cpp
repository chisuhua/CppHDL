#include "chlib/fragment.h"
#include "chlib/stream.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>
#include <vector>
#include <array>

using namespace ch::core;
using namespace chlib;

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("fragment_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL Fragment Examples" << std::endl;
    std::cout << "=========================" << std::endl;

    // 示例1: 基本FragmentBundle使用
    std::cout << "\n1. Basic FragmentBundle Example:" << std::endl;
    
    FragmentBundle<ch_uint<8>> fragment_bundle("frag1");
    fragment_bundle.fragment = 0xAB_d;
    fragment_bundle.last = true;
    
    std::cout << "Fragment data: 0x" << std::hex << simulator.get_value(fragment_bundle.fragment) << std::endl;
    std::cout << "Is last: " << std::dec << simulator.get_value(fragment_bundle.last) << std::endl;

    // 示例2: Flow<FragmentBundle>使用
    std::cout << "\n2. Flow<FragmentBundle> Example:" << std::endl;
    
    Flow<FragmentBundle<ch_uint<8>>> flow_fragment;
    flow_fragment.payload.fragment = 0xCD_d;
    flow_fragment.payload.last = false;
    flow_fragment.valid = true;
    
    std::cout << "Flow fragment: 0x" << std::hex << simulator.get_value(flow_fragment.payload.fragment) << std::endl;
    std::cout << "Flow last: " << std::dec << simulator.get_value(flow_fragment.payload.last) << std::endl;
    std::cout << "Flow valid: " << std::dec << simulator.get_value(flow_fragment.valid) << std::endl;

    // 示例3: 使用fragment.h中提供的工具函数
    std::cout << "\n3. Fragment Utility Functions Example:" << std::endl;
    
    // 使用get_fragment_data和get_last_signal
    auto fragment_data = get_fragment_data(flow_fragment);
    auto last_signal = get_last_signal(flow_fragment);
    
    std::cout << "Extracted fragment data: 0x" << std::hex << simulator.get_value(fragment_data) << std::endl;
    std::cout << "Extracted last signal: " << std::dec << simulator.get_value(last_signal) << std::endl;

    // 示例4: 创建Fragment序列
    std::cout << "\n4. Fragment Sequence Example:" << std::endl;
    
    std::array<ch_uint<8>, 5> data_seq = {0x11_u8, 0x22_u8, 0x33_u8, 0x44_u8, 0x55_u8};
    auto fragment_seq = fragment_sequence(data_seq);
    
    for(size_t i = 0; i < fragment_seq.size(); ++i) {
        std::cout << "Fragment " << i << " - data: 0x" << std::hex 
                  << simulator.get_value(fragment_seq[i].payload.fragment)
                  << ", last: " << std::dec 
                  << simulator.get_value(fragment_seq[i].payload.last) << std::endl;
    }

    // 示例5: Fragment到普通Flow的转换
    std::cout << "\n5. Fragment to Flow Conversion Example:" << std::endl;
    
    Flow<FragmentBundle<ch_uint<8>>> input_frag_flow;
    input_frag_flow.payload.fragment = 0x99_d;
    input_frag_flow.payload.last = true;
    input_frag_flow.valid = true;
    
    // 使用fragment_to_payload函数
    auto converted_flow = fragment_to_payload(input_frag_flow);
    
    std::cout << "Original fragment: 0x" << std::hex 
              << simulator.get_value(input_frag_flow.payload.fragment) << std::endl;
    std::cout << "Converted payload: 0x" << std::hex 
              << simulator.get_value(converted_flow.payload) << std::endl;
    std::cout << "Flow valid: " << std::dec 
              << simulator.get_value(converted_flow.valid) << std::endl;

    // 示例6: 从普通数据创建Fragment
    std::cout << "\n6. Payload to Fragment Conversion Example:" << std::endl;
    
    ch_uint<8> payload_data = 0xFF_d;
    ch_bool is_last = true;
    
    auto created_fragment = payload_to_fragment(payload_data, is_last);
    
    std::cout << "Created fragment: 0x" << std::hex 
              << simulator.get_value(created_fragment.payload.fragment) << std::endl;
    std::cout << "Created last: " << std::dec 
              << simulator.get_value(created_fragment.payload.last) << std::endl;
    std::cout << "Created valid: " << std::dec 
              << simulator.get_value(created_fragment.valid) << std::endl;

    std::cout << "\nAll fragment examples completed successfully!" << std::endl;

    return 0;
}