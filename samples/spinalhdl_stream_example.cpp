/**
 * @file spinalhdl_stream_example.cpp
 * @brief CppHDL vs SpinalHDL 的 Stream / Flow / Fragment API 对照演示
 *
 * ⚠️ API 漂移记录（2026-10-22）：
 * ① `chlib/fragment.h` **不存在** ⇒ Fragment 的头文件是
 *    `bundle/fragment.h`。
 * ② `stream_fifo<T, D>(clk, rst, in)` → `stream_fifo<T, D>(in)`；
 *    `stream_arbiter_round_robin<T, N>(clk, rst, ins)` →
 *    `stream_arbiter_round_robin<T, N>(ins)` —— 两者内部都已改用
 *    context 的**默认时钟/复位**。
 * ③ `as_master()` 不再是虚函数 —— 它是「设置角色」的入口，内部调用
 *    派生类实现的 `as_master_direction()` ⇒ 派生类 MUST 实现
 *    `as_master_direction()`（**MUST NOT** `override as_master()`）。
 * ④ `Stream<T>` / `Flow<T>` / `Fragment<T>` 的规范名是
 *    `ch_stream<T>` / `ch_flow<T>` / `ch_fragment<T>`。
 */
#include "bundle/fragment.h"
#include "ch.hpp"
#include "chlib/stream.h"
#include "component.h"
#include "core/context.h"
#include "simulator.h"
#include <array>
#include <iostream>
#include <vector>

using namespace ch::core;
using namespace chlib;

// 简单的Stream FIFO示例 - 对应SpinalHDL的StreamFifo
class StreamFifoExample {
public:
    struct Result {
        ch_stream<ch_uint<8>> push;
        ch_stream<ch_uint<8>> pop;
        ch_uint<3> occupancy;  // log2(depth) + 1，这里depth=4
        ch_bool full;
        ch_bool empty;
    };

    Result process(ch_stream<ch_uint<8>> input_stream) {
        Result result;

        // 创建FIFO
        auto fifo_result = stream_fifo<ch_uint<8>, 4>(input_stream);

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
        ch_stream<ch_uint<8>> input;
        std::array<ch_stream<ch_uint<8>>, 2> output_streams;
    };

    Result process(ch_stream<ch_uint<8>> input_stream) {
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
        std::array<ch_stream<ch_uint<8>>, 2> input_streams;
        ch_stream<ch_uint<16>> output;
    };

    Result process(std::array<ch_stream<ch_uint<8>>, 2> input_streams) {
        Result result;

        // 使用stream_join工具
        auto join_result = stream_join<ch_uint<8>, 2>(input_streams);

        // 合并两个输入流的数据
        ch_uint<16> combined_data =
            (input_streams[0].payload << 8) | input_streams[1].payload;

        result.output.payload = combined_data;
        result.output.valid = join_result.output_stream.valid;
        result.output.ready = ch_bool(true);  // 假设输出端总是就绪

        result.input_streams[0] = join_result.input_streams[0];
        result.input_streams[1] = join_result.input_streams[1];

        return result;
    }
};

// Stream Arbiter示例 - 对应SpinalHDL的Stream Arbiter
class StreamArbiterExample {
public:
    struct Result {
        std::array<ch_stream<ch_uint<8>>, 4> input_streams;
        ch_stream<ch_uint<8>> output;
        ch_uint<2> selected;  // 选择的输入索引
    };

    Result process(std::array<ch_stream<ch_uint<8>>, 4> input_streams) {
        Result result;

        // 使用stream_arbiter_round_robin工具
        auto arb_result = stream_arbiter_round_robin<ch_uint<8>, 4>(input_streams);

        result.input_streams = arb_result.input_streams;
        result.output = arb_result.output_stream;
        result.selected = arb_result.selected;

        return result;
    }
};

// 自定义Stream Bundle示例 - 对应SpinalHDL的Custom Bundle
template <typename T>
struct CustomStreamBundle : public bundle_base<CustomStreamBundle<T>> {
    using Self = CustomStreamBundle<T>;
    T payload;
    ch_bool valid;
    ch_bool ready;

    CustomStreamBundle() = default;
    explicit CustomStreamBundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(payload, valid, ready)

    // ⚠️ MUST 实现 `as_master_direction()`（`as_master()` 是入口，不是虚函数）
    void as_master_direction() {
        // Master: 输出payload和valid，输入ready
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave_direction() {
        // Slave: 输入payload和valid，输出ready
        this->make_input(payload, valid);
        this->make_output(ready);
    }
};

// Flow示例 - 对应SpinalHDL的Flow
class FlowExample {
public:
    struct Result {
        ch_flow<ch_uint<8>> input_flow;
        ch_flow<ch_uint<8>> output_flow;
    };

    Result process(ch_flow<ch_uint<8>> input_flow) {
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
        ch_fragment<ch_uint<8>> input_frag;
        ch_fragment<ch_uint<8>> output_frag;
    };

    Result process(ch_fragment<ch_uint<8>> input_frag) {
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

    // ---- 准备输入 stream（⚠️ 所有节点 MUST 在 Simulator 构造之前创建）----
    ch_stream<ch_uint<8>> fifo_input;
    fifo_input.payload = make_uint<8>(0x11);
    fifo_input.valid = ch_bool(true);
    fifo_input.ready = ch_bool(true);

    ch_stream<ch_uint<8>> fork_input;
    fork_input.payload = make_uint<8>(0x22);
    fork_input.valid = ch_bool(true);
    fork_input.ready = ch_bool(true);

    std::array<ch_stream<ch_uint<8>>, 2> join_inputs;
    join_inputs[0].payload = make_uint<8>(0x33);
    join_inputs[0].valid = ch_bool(true);
    join_inputs[0].ready = ch_bool(true);
    join_inputs[1].payload = make_uint<8>(0x44);
    join_inputs[1].valid = ch_bool(true);
    join_inputs[1].ready = ch_bool(true);

    std::array<ch_stream<ch_uint<8>>, 4> arb_inputs;
    for (unsigned i = 0; i < 4; ++i) {
        arb_inputs[i].payload = make_uint<8>(0x50 + i);
        arb_inputs[i].valid = ch_bool(i == 0);
        arb_inputs[i].ready = ch_bool(true);
    }

    ch_flow<ch_uint<8>> flow_input;
    flow_input.payload = make_uint<8>(0x66);
    flow_input.valid = ch_bool(true);

    // ⚠️ `ch_fragment` 的字段是 `data_beat` / `last` / `first`
    //    （不是 payload / valid）
    ch_fragment<ch_uint<8>> frag_input;
    frag_input.data_beat = make_uint<8>(0x77);
    frag_input.last = ch_bool(false);
    frag_input.first = ch_bool(true);

    // ---- 跑一遍各 API ----
    StreamFifoExample fifo_example;
    StreamForkExample fork_example;
    StreamJoinExample join_example;
    StreamArbiterExample arbiter_example;

    // 创建自定义Stream Bundle
    CustomStreamBundle<ch_uint<16>> custom_stream;
    custom_stream.as_master();
    custom_stream.set_name_prefix("custom_stream");

    // 创建Flow示例
    FlowExample flow_example;

    // 创建Fragment示例
    FragmentExample frag_example;

    // 真正调用各个 process()（旧版只构造对象、从**不调用** ⇒ 是个空壳）
    auto fifo_result = fifo_example.process(fifo_input);
    auto fork_result = fork_example.process(fork_input);
    auto join_result = join_example.process(join_inputs);
    auto arb_result = arbiter_example.process(arb_inputs);
    auto flow_result = flow_example.process(flow_input);
    auto frag_result = frag_example.process(frag_input);

    std::cout << "Bundle created with width: " << custom_stream.width() << std::endl;
    std::cout << "Bundle name prefix set successfully!" << std::endl;

    // 创建仿真器
    ch::Simulator sim(ctx.get());
    sim.tick();

    std::cout << "\n--- API 行为检查 ---" << std::endl;

    // Stream Fork：payload 必须复制到两个输出
    auto f0 = static_cast<uint64_t>(sim.get_value(fork_result.output_streams[0].payload));
    auto f1 = static_cast<uint64_t>(sim.get_value(fork_result.output_streams[1].payload));
    std::cout << "fork out0=0x" << std::hex << f0 << " out1=0x" << f1 << std::dec << std::endl;
    if (f0 != 0x22 || f1 != 0x22) {
        std::cerr << "ERROR: stream_fork must replicate the payload" << std::endl;
        return 1;
    }

    // Stream Join：两个输入都有效 ⇒ 输出有效
    auto jv = static_cast<uint64_t>(sim.get_value(join_result.output.valid));
    std::cout << "join out valid=" << jv << std::endl;
    if (jv != 1) {
        std::cerr << "ERROR: stream_join output must be valid when all inputs are valid"
                  << std::endl;
        return 1;
    }

    // Stream Arbiter：只有输入 0 有效 ⇒ 选中索引必须是 0
    auto sel = static_cast<uint64_t>(sim.get_value(arb_result.selected));
    std::cout << "arbiter selected=" << sel << std::endl;
    if (sel != 0) {
        std::cerr << "ERROR: round-robin arbiter must select the only valid input (0)"
                  << std::endl;
        return 1;
    }

    // Flow / Fragment：直接传递
    std::cout << "flow payload=0x" << std::hex
              << static_cast<uint64_t>(sim.get_value(flow_result.output_flow.payload))
              << std::dec << std::endl;
    std::cout << "fragment data_beat=0x" << std::hex
              << static_cast<uint64_t>(
                     sim.get_value(frag_result.output_frag.data_beat))
              << std::dec << std::endl;

    // Stream FIFO：写入了 1 个元素 ⇒ 不应是空的
    auto empty = static_cast<uint64_t>(sim.get_value(fifo_result.empty));
    std::cout << "fifo empty=" << empty << std::endl;
    if (empty != 0) {
        std::cerr << "ERROR: stream_fifo must not be empty after a write" << std::endl;
        return 1;
    }

    // Stream FIFO（2026-10-22，ADR-045 裁决 2.2 / change
    // `chlib-stream-semantics-2026-10`）：**数据通路**断言。
    //
    // ⚠️ 此前只断言 empty ⇒ payload 再错也 exit 0 ⇒ sample 绿没有信号。
    //    FWFT（0-cycle）+ write-through ⇒ 推入 0x11 之后**同拍**
    //    `pop.valid == 1` 且 `pop.payload == 0x11`。
    //    （1-cycle `sread` 在这一拍 payload 是 0x0 ⇒ 可区分。）
    auto pop_valid = static_cast<uint64_t>(sim.get_value(fifo_result.pop.valid));
    auto pop_payload =
        static_cast<uint64_t>(sim.get_value(fifo_result.pop.payload));
    std::cout << "fifo pop valid=" << pop_valid << " payload=0x" << std::hex
              << pop_payload << std::dec << std::endl;
    if (pop_valid != 1 || pop_payload != 0x11) {
        std::cerr << "ERROR: FWFT head must show the pushed value 0x11 in the "
                     "same tick (valid=" << pop_valid << ", payload=0x"
                  << std::hex << pop_payload << std::dec << ")" << std::endl;
        return 1;
    }

    std::cout << "\nStream/Flow examples initialized successfully!" << std::endl;
    std::cout << "Ready to simulate hardware designs..." << std::endl;

    return 0;
}
