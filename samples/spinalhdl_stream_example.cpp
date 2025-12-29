#include "chlib/stream.h"
#include "chlib/fragment.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>
#include <vector>
#include <array>

using namespace ch::core;
using namespace chlib;

// 简单的Stream FIFO示例 - 对应SpinalHDL的StreamFifo
class StreamFifoExample {
public:
    struct Result {
        Stream<ch_uint<8>> push;
        Stream<ch_uint<8>> pop;
        ch_uint<3> occupancy;  // log2(depth) + 1，这里depth=4
        ch_bool full;
        ch_bool empty;
    };
    
    Result process(
        ch_bool clk,
        ch_bool rst,
        Stream<ch_uint<8>> input_stream
    ) {
        Result result;
        
        // 创建FIFO
        auto fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
        
        // 连接输入到FIFO
        result.push = fifo_result.push_stream;
        
        // 连接FIFO到输出
        result.pop = fifo_result.pop_stream;
        
        // 状态信号
        result.occupancy = fifo_result.occupancy;
        result.full = fifo_result.full;
        result.empty = fifo_result.empty;
        
        return result;
    }
};

// Stream Fork示例 - 对应SpinalHDL的Stream Fork
class StreamForkExample {
public:
    struct Result {
        Stream<ch_uint<8>> input;
        std::array<Stream<ch_uint<8>>, 2> output_streams;
    };
    
    Result process(
        Stream<ch_uint<8>> input_stream
    ) {
        Result result;
        
        // 使用stream_fork工具
        auto fork_result = stream_fork<ch_uint<8>, 2>(input_stream, false);
        
        result.input = fork_result.input_stream;
        result.output_streams[0] = fork_result.output_streams[0];
        result.output_streams[1] = fork_result.output_streams[1];
        
        return result;
    }
};

// Stream Join示例 - 对应SpinalHDL的Stream Join
class StreamJoinExample {
public:
    struct Result {
        std::array<Stream<ch_uint<8>>, 2> input_streams;
        Stream<ch_uint<16>> output;
    };
    
    Result process(
        std::array<Stream<ch_uint<8>>, 2> input_streams
    ) {
        Result result;
        
        // 使用stream_join工具
        auto join_result = stream_join<ch_uint<8>, 2>(input_streams);
        
        // 合并两个输入流的数据
        ch_uint<16> combined_data = 
            (input_streams[0].payload << 8) | input_streams[1].payload;
        
        result.output.payload = combined_data;
        result.output.valid = join_result.output_stream.valid;
        result.output.ready = true;  // 假设输出端总是就绪
        
        result.input_streams[0] = join_result.input_streams[0];
        result.input_streams[1] = join_result.input_streams[1];
        
        return result;
    }
};

// Stream Arbiter示例 - 对应SpinalHDL的Stream Arbiter
class StreamArbiterExample {
public:
    struct Result {
        std::array<Stream<ch_uint<8>>, 4> input_streams;
        Stream<ch_uint<8>> output;
        ch_uint<2> selected;  // 选择的输入索引
    };
    
    Result process(
        ch_bool clk,
        ch_bool rst,
        std::array<Stream<ch_uint<8>>, 4> input_streams
    ) {
        Result result;
        
        // 使用stream_arbiter_round_robin工具
        auto arb_result = stream_arbiter_round_robin<ch_uint<8>, 4>(clk, rst, input_streams);
        
        result.input_streams = arb_result.input_streams;
        result.output = arb_result.output_stream;
        result.selected = arb_result.selected;
        
        return result;
    }
};

// 自定义Stream Bundle示例 - 对应SpinalHDL的Custom Bundle
template<typename T>
struct CustomStreamBundle : public bundle_base<CustomStreamBundle<T>> {
    using Self = CustomStreamBundle<T>;
    T data;
    ch_bool enable;
    ch_bool valid;
    ch_bool ready;

    CustomStreamBundle() = default;
    explicit CustomStreamBundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, data, enable, valid, ready)

    void as_master() override {
        this->make_output(data, enable, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->make_input(data, enable, valid);
        this->make_output(ready);
    }
};

// Flow示例 - 对应SpinalHDL的Flow
class FlowExample {
public:
    struct Result {
        Flow<ch_uint<8>> input_flow;
        Flow<ch_uint<8>> output_flow;
    };
    
    Result process(
        Flow<ch_uint<8>> input_flow
    ) {
        Result result;
        
        result.input_flow = input_flow;
        result.output_flow = input_flow;  // Flow直接传递，无握手信号
        
        return result;
    }
};

// Fragment示例 - 对应SpinalHDL的Fragment
class FragmentExample {
public:
    struct Result {
        Flow<FragmentBundle<ch_uint<8>>> input_flow;
        ch_uint<8> payload_fragment;
        ch_bool is_last;
    };
    
    Result process(
        Flow<FragmentBundle<ch_uint<8>>> input_flow
    ) {
        Result result;
        
        result.input_flow = input_flow;
        result.payload_fragment = get_fragment_data(input_flow);  // 使用fragment.h中提供的函数
        result.is_last = get_last_signal(input_flow);             // 使用fragment.h中提供的函数
        
        return result;
    }
};

// Fragment序列处理示例
class FragmentSequenceExample {
public:
    struct Result {
        std::array<Flow<FragmentBundle<ch_uint<8>>>, 4> fragment_sequence;
    };
    
    Result process() {
        Result result;
        
        // 创建包含4个数据片段的序列
        std::array<ch_uint<8>, 4> data = {0x12_u8, 0x34_u8, 0x56_u8, 0x78_u8};
        
        // 使用fragment_sequence函数创建片段序列
        result.fragment_sequence = fragment_sequence(data);
        
        return result;
    }
};

// 简单的内存示例 - 对应SpinalHDL的Mem
class SimpleMemoryExample {
public:
    struct Result {
        ch_uint<5> address;
        ch_uint<8> data_out;
        ch_uint<8> data_in;
        ch_bool write_enable;
    };
    
    Result process(
        ch_uint<5> addr,
        ch_bool write_enable,
        ch_uint<8> data_in
    ) {
        Result result;
        
        // 创建一个32深度、8位宽的内存
        static std::array<ch_uint<8>, 32> memory = {};
        
        result.address = addr;
        result.data_out = memory[addr.value()];  // 异步读取
        result.data_in = data_in;
        result.write_enable = write_enable;
        
        // 如果写使能有效，写入数据
        if(write_enable.value()) {
            memory[addr.value()] = data_in;
        }
        
        return result;
    }
};

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("spinalhdl_stream_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL Stream Examples (based on SpinalHDL)" << std::endl;
    std::cout << "=============================================" << std::endl;

    // 示例1: Stream FIFO
    std::cout << "\n1. Stream FIFO Example:" << std::endl;
    
    ch_bool clk = false;
    ch_bool rst = true;
    
    Stream<ch_uint<8>> input_stream;
    input_stream.payload = 0_d;
    input_stream.valid = false;
    input_stream.ready = false;
    
    ch::Simulator sim(ctx.get());
    
    StreamFifoExample fifo_example;
    auto fifo_result = fifo_example.process(clk, rst, input_stream);
    sim.tick();
    
    rst = false;
    clk = true;
    input_stream.payload = 0x55_d;
    input_stream.valid = true;
    fifo_result = fifo_example.process(clk, rst, input_stream);
    sim.tick();
    
    std::cout << "After writing 0x55 - FIFO empty: " << simulator.get_value(fifo_result.empty) 
              << ", FIFO full: " << simulator.get_value(fifo_result.full) 
              << ", Occupancy: " << simulator.get_value(fifo_result.occupancy) << std::endl;

    // 示例2: Stream Fork
    std::cout << "\n2. Stream Fork Example:" << std::endl;
    
    Stream<ch_uint<8>> fork_input_stream;
    fork_input_stream.payload = 0xAB_d;
    fork_input_stream.valid = true;
    
    StreamForkExample fork_example;
    auto fork_result = fork_example.process(fork_input_stream);
    
    std::cout << "Fork input payload: 0x" << std::hex << simulator.get_value(fork_input_stream.payload) << std::endl;
    std::cout << "Fork output 0 payload: 0x" << std::hex << simulator.get_value(fork_result.output_streams[0].payload) << std::endl;
    std::cout << "Fork output 1 payload: 0x" << std::hex << simulator.get_value(fork_result.output_streams[1].payload) << std::endl;
    std::cout << "Fork input valid: " << std::dec << simulator.get_value(fork_input_stream.valid) << std::endl;
    std::cout << "Fork output 0 valid: " << std::dec << simulator.get_value(fork_result.output_streams[0].valid) << std::endl;
    std::cout << "Fork output 1 valid: " << std::dec << simulator.get_value(fork_result.output_streams[1].valid) << std::endl;

    // 示例3: Stream Join
    std::cout << "\n3. Stream Join Example:" << std::endl;
    
    std::array<Stream<ch_uint<8>>, 2> join_input_streams;
    join_input_streams[0].payload = 0x12_d;
    join_input_streams[0].valid = true;
    join_input_streams[1].payload = 0x34_d;
    join_input_streams[1].valid = true;
    
    StreamJoinExample join_example;
    auto join_result = join_example.process(join_input_streams);
    
    std::cout << "Join input 0 payload: 0x" << std::hex << simulator.get_value(join_input_streams[0].payload) << std::endl;
    std::cout << "Join input 1 payload: 0x" << std::hex << simulator.get_value(join_input_streams[1].payload) << std::endl;
    std::cout << "Join output payload: 0x" << std::hex << simulator.get_value(join_result.output.payload) << std::endl;
    std::cout << "Join output valid: " << std::dec << simulator.get_value(join_result.output.valid) << std::endl;

    // 示例4: Stream Arbiter
    std::cout << "\n4. Stream Arbiter Example:" << std::endl;
    
    std::array<Stream<ch_uint<8>>, 4> arb_input_streams;
    arb_input_streams[0].payload = 0x11_d;
    arb_input_streams[0].valid = true;
    arb_input_streams[1].payload = 0x22_d;
    arb_input_streams[1].valid = false;  // 这个无效
    arb_input_streams[2].payload = 0x33_d;
    arb_input_streams[2].valid = true;
    arb_input_streams[3].payload = 0x44_d;
    arb_input_streams[3].valid = false;  // 这个也无效

    StreamArbiterExample arb_example;
    auto arb_result = arb_example.process(clk, rst, arb_input_streams);
    
    std::cout << "Arbiter input 0 payload: 0x" << std::hex << simulator.get_value(arb_input_streams[0].payload) << std::endl;
    std::cout << "Arbiter input 0 valid: " << std::dec << simulator.get_value(arb_input_streams[0].valid) << std::endl;
    std::cout << "Arbiter input 2 payload: 0x" << std::hex << simulator.get_value(arb_input_streams[2].payload) << std::endl;
    std::cout << "Arbiter input 2 valid: " << std::dec << simulator.get_value(arb_input_streams[2].valid) << std::endl;
    
    std::cout << "\nArbiter Output:" << std::endl;
    std::cout << "Output payload: 0x" << std::hex << simulator.get_value(arb_result.output.payload) << std::endl;
    std::cout << "Output valid: " << std::dec << simulator.get_value(arb_result.output.valid) << std::endl;
    std::cout << "Selected input index: " << simulator.get_value(arb_result.selected) << std::endl;

    // 示例5: 自定义Stream Bundle
    std::cout << "\n5. Custom Stream Bundle Example:" << std::endl;
    
    CustomStreamBundle<ch_uint<8>> custom_stream("custom");
    custom_stream.data = 0xDE_d;
    custom_stream.enable = true;
    custom_stream.valid = true;
    custom_stream.ready = false;
    
    custom_stream.as_master();
    std::cout << "Custom stream data: 0x" << std::hex << simulator.get_value(custom_stream.data) << std::endl;
    std::cout << "Custom stream enable: " << std::dec << simulator.get_value(custom_stream.enable) << std::endl;
    std::cout << "Custom stream valid: " << simulator.get_value(custom_stream.valid) << std::endl;
    std::cout << "Custom stream ready: " << simulator.get_value(custom_stream.ready) << std::endl;

    // 示例6: Flow示例
    std::cout << "\n6. Flow Example:" << std::endl;
    
    Flow<ch_uint<8>> input_flow;
    input_flow.payload = 0xBC_d;
    input_flow.valid = true;
    
    FlowExample flow_example;
    auto flow_result = flow_example.process(input_flow);
    
    std::cout << "Flow payload: 0x" << std::hex << simulator.get_value(flow_result.input_flow.payload) << std::endl;
    std::cout << "Flow valid: " << std::dec << simulator.get_value(flow_result.input_flow.valid) << std::endl;

    // 示例7: Fragment示例
    std::cout << "\n7. Fragment Example:" << std::endl;
    
    Flow<FragmentBundle<ch_uint<8>>> fragment_flow;
    fragment_flow.payload.fragment = 0xEF_d;
    fragment_flow.payload.last = true;
    fragment_flow.valid = true;
    
    FragmentExample fragment_example;
    auto fragment_result = fragment_example.process(fragment_flow);
    
    std::cout << "Fragment payload: 0x" << std::hex << simulator.get_value(fragment_result.payload_fragment) << std::endl;
    std::cout << "Fragment last: " << std::dec << simulator.get_value(fragment_result.is_last) << std::endl;

    // 示例8: 简单内存示例
    std::cout << "\n8. Memory Example:" << std::endl;
    
    ch_uint<5> addr = 5_u5;
    ch_uint<8> data_in = 0xAA_d;
    ch_bool write_enable = false;
    
    SimpleMemoryExample mem_example;
    auto mem_result = mem_example.process(addr, write_enable, data_in);
    
    std::cout << "Memory address: " << std::dec << simulator.get_value(mem_result.address) << std::endl;
    std::cout << "Memory data out: 0x" << std::hex << simulator.get_value(mem_result.data_out) << std::endl;
    std::cout << "Memory write enable: " << std::dec << simulator.get_value(mem_result.write_enable) << std::endl;

    // 示例9: Fragment序列处理示例
    std::cout << "\n9. Fragment Sequence Example:" << std::endl;
    
    FragmentSequenceExample seq_example;
    auto seq_result = seq_example.process();
    
    for(size_t i = 0; i < seq_result.fragment_sequence.size(); ++i) {
        std::cout << "Fragment " << i << " - data: 0x" << std::hex 
                  << simulator.get_value(seq_result.fragment_sequence[i].payload.fragment)
                  << ", last: " << std::dec 
                  << simulator.get_value(seq_result.fragment_sequence[i].payload.last) << std::endl;
    }
    
    ch_uint<5> addr = 5_u5;
    ch_uint<8> data_in = 0xAA_d;
    ch_bool write_enable = false;
    
    SimpleMemoryExample mem_example;
    auto mem_result = mem_example.process(addr, write_enable, data_in);
    
    std::cout << "Memory address: " << std::dec << simulator.get_value(mem_result.address) << std::endl;
    std::cout << "Memory data out: 0x" << std::hex << simulator.get_value(mem_result.data_out) << std::endl;
    std::cout << "Memory write enable: " << std::dec << simulator.get_value(mem_result.write_enable) << std::endl;

    std::cout << "\nAll examples completed successfully!" << std::endl;

    return 0;
}