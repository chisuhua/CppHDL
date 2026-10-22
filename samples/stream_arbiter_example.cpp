/**
 * @file stream_arbiter_example.cpp
 * @brief Stream Arbiter 演示 —— round-robin 仲裁器在多个有效输入间轮转
 *
 * ⚠️ API 漂移记录（2026-10-22）：
 * ① 旧 API 是 `stream_arbiter_round_robin<T, N>(clk, rst, input_streams)`，
 *    当前 API 是 `stream_arbiter_round_robin<T, N>(input_streams)` ——
 *    仲裁器内部已改用 context 的**默认时钟/复位**（`ch_reg`），
 *    因此本 sample 不再自行提供 clk / rst。
 * ② **所有节点 MUST 在 `ch::Simulator` 构造之前创建** —— Simulator 在构造时
 *    建立 data_map_，之后新建的节点不在其中 ⇒ `get_value()` 会读到
 *    "Value not found" 警告。因此「让第 2 个输入变有效」这一场景用
 *    **第二组输入 + 第二个仲裁器**表达，而不是在 Simulator 之后改字段。
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

// 4 个输入 Stream，valid 由调用方给出
std::array<ch_stream<ch_uint<8>>, 4> make_inputs(bool v0, bool v1, bool v2, bool v3) {
    std::array<ch_stream<ch_uint<8>>, 4> ins;
    ins[0].payload = make_uint<8>(0x11);
    ins[0].valid = ch_bool(v0);
    ins[1].payload = make_uint<8>(0x22);
    ins[1].valid = ch_bool(v1);
    ins[2].payload = make_uint<8>(0x33);
    ins[2].valid = ch_bool(v2);
    ins[3].payload = make_uint<8>(0x44);
    ins[3].valid = ch_bool(v3);
    return ins;
}

}  // namespace

int main() {
    std::cout << "Stream Arbiter Example:" << std::endl;

    // 节点创建需要活跃 context（`make_uint` / `ch_bool` 都会建节点）
    ch::core::context ctx("stream_arbiter_example");
    ch::core::ctx_swap swap(&ctx);

    // 场景 A：输入 0 与 2 有效（1 与 3 无效）
    auto input_streams = make_inputs(true, false, true, false);
    // 场景 B：输入 0 / 1 / 2 有效（1 也变有效了）
    auto input_streams2 = make_inputs(true, true, true, false);

    // 创建两个仲裁器（⚠️ MUST 在 Simulator 构造之前）
    auto arb_result = stream_arbiter_round_robin<ch_uint<8>, 4>(input_streams);
    auto arb_result2 = stream_arbiter_round_robin<ch_uint<8>, 4>(input_streams2);

    ch::Simulator sim(&ctx);
    sim.tick();
    sim.tick();

    std::cout << "Input 0 payload: 0x" << std::hex << sim.get_value(input_streams[0].payload) << std::endl;
    std::cout << "Input 0 valid: " << std::dec << sim.get_value(input_streams[0].valid) << std::endl;
    std::cout << "Input 1 payload: 0x" << std::hex << sim.get_value(input_streams[1].payload) << std::endl;
    std::cout << "Input 1 valid: " << sim.get_value(input_streams[1].valid) << std::endl;
    std::cout << "Input 2 payload: 0x" << std::hex << sim.get_value(input_streams[2].payload) << std::endl;
    std::cout << "Input 2 valid: " << sim.get_value(input_streams[2].valid) << std::endl;
    std::cout << "Input 3 payload: 0x" << std::hex << sim.get_value(input_streams[3].payload) << std::endl;
    std::cout << "Input 3 valid: " << sim.get_value(input_streams[3].valid) << std::endl;

    std::cout << "\nArbiter Output:" << std::endl;
    std::cout << "Output payload: 0x" << std::hex << sim.get_value(arb_result.output_stream.payload) << std::endl;
    std::cout << "Output valid: " << std::dec << sim.get_value(arb_result.output_stream.valid) << std::endl;
    std::cout << "Selected input index: " << sim.get_value(arb_result.selected) << std::endl;

    // 自检：只有有效的输入被选中（此处 0 与 2 有效 ⇒ selected ∈ {0, 2}）
    auto selected = static_cast<uint64_t>(sim.get_value(arb_result.selected));
    if (selected != 0 && selected != 2) {
        std::cerr << "ERROR: round-robin must select a valid input (0 or 2), got "
                  << selected << std::endl;
        return 1;
    }
    if (sim.get_value(arb_result.output_stream.valid) != 1) {
        std::cerr << "ERROR: output must be valid when some input is valid"
                  << std::endl;
        return 1;
    }

    std::cout << "\nWhen input 1 becomes valid:" << std::endl;
    std::cout << "Input 1 valid: " << sim.get_value(input_streams2[1].valid) << std::endl;
    std::cout << "Output payload: 0x" << std::hex << sim.get_value(arb_result2.output_stream.payload) << std::endl;
    std::cout << "Output valid: " << std::dec << sim.get_value(arb_result2.output_stream.valid) << std::endl;
    std::cout << "Selected input index: " << sim.get_value(arb_result2.selected) << std::endl;

    // 自检：三个输入有效 ⇒ 仍必须选中一个**有效**的输入
    auto selected2 = static_cast<uint64_t>(sim.get_value(arb_result2.selected));
    if (selected2 > 3) {
        std::cerr << "ERROR: selected index out of range: " << selected2 << std::endl;
        return 1;
    }
    if (sim.get_value(arb_result2.output_stream.valid) != 1) {
        std::cerr << "ERROR: output must be valid when some input is valid"
                  << std::endl;
        return 1;
    }

    std::cout << "\nStream Arbiter example completed successfully!" << std::endl;

    return 0;
}
