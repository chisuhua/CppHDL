/**
 * @file stream_join_example.cpp
 * @brief Stream Join 演示 —— 多个输入合成一个输出（全部输入有效才输出）
 *
 * ⚠️ **为什么本 sample 不读 `.ready`**（2026-10-22 实测）：
 * `stream_join()` 把 `result.input_streams[i].ready` 接到
 * `result.output_stream.ready`，而后者由**外部连接**决定（chlib 侧未驱动）
 * ⇒ 该节点为 null ⇒ 读 `.ready` 会 CHABORT（"ch_bool implementation is null"）。
 *
 * ⚠️ **所有节点 MUST 在 `ch::Simulator` 构造之前创建** —— Simulator 在构造时
 * 建立 data_map_，之后新建的节点不在其中 ⇒ `get_value()` 会读到
 * "Value not found" 警告。因此「一个输入无效」这一场景用**第二组输入 +
 * 第二个 join**表达，而不是在 Simulator 之后改字段。
 */
#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "chlib/stream.h"
#include <array>
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

std::array<ch_stream<ch_uint<8>>, 2> make_inputs(bool v0, bool v1) {
    std::array<ch_stream<ch_uint<8>>, 2> ins;
    ins[0].payload = make_uint<8>(0x12);
    ins[0].valid = ch_bool(v0);
    ins[0].ready = ch_bool(true);
    ins[1].payload = make_uint<8>(0x34);
    ins[1].valid = ch_bool(v1);
    ins[1].ready = ch_bool(true);
    return ins;
}

}  // namespace

int main() {
    std::cout << "Stream Join Example:" << std::endl;

    // 节点创建需要活跃 context（`make_uint` / `ch_bool` 都会建节点）
    ch::core::context ctx("stream_join_example");
    ch::core::ctx_swap swap(&ctx);

    // 场景 A：两个输入都有效；场景 B：第二个输入无效
    auto input_streams = make_inputs(true, true);
    auto input_streams2 = make_inputs(true, false);

    // 创建两个 Join（⚠️ MUST 在 Simulator 构造之前）
    auto join_result = stream_join<ch_uint<8>, 2>(input_streams);
    auto join_result2 = stream_join<ch_uint<8>, 2>(input_streams2);

    ch::Simulator sim(&ctx);
    sim.tick();
    sim.tick();

    std::cout << "Input 0 payload: 0x" << std::hex << sim.get_value(input_streams[0].payload) << std::endl;
    std::cout << "Input 1 payload: 0x" << std::hex << sim.get_value(input_streams[1].payload) << std::endl;
    std::cout << "Input 0 valid: " << std::dec << sim.get_value(input_streams[0].valid) << std::endl;
    std::cout << "Input 1 valid: " << sim.get_value(input_streams[1].valid) << std::endl;
    std::cout << "Output payload: 0x" << std::hex << sim.get_value(join_result.output_stream.payload) << std::endl;
    std::cout << "Output valid: " << std::dec << sim.get_value(join_result.output_stream.valid) << std::endl;

    // 自检：两个输入都有效 ⇒ 输出有效（payload 取第一个输入）
    if (sim.get_value(join_result.output_stream.valid) != 1) {
        std::cerr << "ERROR: output must be valid when all inputs are valid"
                  << std::endl;
        return 1;
    }
    if (static_cast<uint64_t>(sim.get_value(join_result.output_stream.payload)) != 0x12) {
        std::cerr << "ERROR: output payload must be the first input's payload"
                  << std::endl;
        return 1;
    }

    std::cout << "\nWhen one input is invalid:" << std::endl;
    std::cout << "Input 0 valid: " << sim.get_value(input_streams2[0].valid) << std::endl;
    std::cout << "Input 1 valid: " << sim.get_value(input_streams2[1].valid) << std::endl;
    std::cout << "Output valid: " << sim.get_value(join_result2.output_stream.valid) << std::endl;

    // 自检：一个输入无效 ⇒ 输出无效
    if (sim.get_value(join_result2.output_stream.valid) != 0) {
        std::cerr << "ERROR: output must be invalid when one input is invalid"
                  << std::endl;
        return 1;
    }

    std::cout << "\nStream Join example completed successfully!" << std::endl;

    return 0;
}
