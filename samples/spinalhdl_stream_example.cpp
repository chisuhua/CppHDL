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

    CH_BUNDLE_FIELDS_T(data, enable, valid, ready)

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
        Fragment<ch_uint<8>> input_frag;
        Fragment<ch_uint<8>> output_frag;
    };
    
    Result process(
        Fragment<ch_uint<8>> input_frag
    ) {
        Result result;
        
        result.input_frag = input_frag;
        result.output_frag = input_frag;  // 简单传递
        
        return result;
    }
};

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("spinalhdl_stream_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL vs SpinalHDL Stream/Flow Example" << std::endl;
    std::cout << "======================================" << std::endl;

    // 创建Stream FIFO示例
    StreamFifoExample fifo_example;
    
    // 创建Stream Fork示例
    StreamForkExample fork_example;
    
    // 创建Stream Join示例
    StreamJoinExample join_example;
    
    // 创建Stream Arbiter示例
    StreamArbiterExample arbiter_example;

    // 创建自定义Stream Bundle
    CustomStreamBundle<ch_uint<16>> custom_stream;
    custom_stream.as_master();
    custom_stream.set_name_prefix("custom_stream");

    // 创建Flow示例
    FlowExample flow_example;

    // 创建Fragment示例
    FragmentExample frag_example;

    std::cout << "Bundle created with width: " << custom_stream.width() << std::endl;
    std::cout << "Bundle name prefix set successfully!" << std::endl;

    // 创建仿真器
    ch::Simulator sim(ctx.get());
    
    std::cout << "\nStream/Flow examples initialized successfully!" << std::endl;
    std::cout << "Ready to simulate hardware designs..." << std::endl;

    return 0;
}
