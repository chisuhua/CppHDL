/**
 * @file stream_arbiter_demo.cpp
 * @brief Stream Arbiter 演示 - 4 输入轮询仲裁器
 * @author DevMate
 * @date 2026-04-12
 */
#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "chlib/stream.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

int main() {
    std::cout << "CppHDL vs SpinalHDL - Stream Arbiter (Round Robin) Example" << std::endl;
    std::cout << "===========================================================" << std::endl;

    const unsigned N = 4;

    ch::core::context ctx("stream_arbiter_test");
    ch::core::ctx_swap swap(&ctx);

    std::array<ch_stream<ch_uint<8>>, N> inputs;
    for (unsigned i = 0; i < N; ++i) {
        inputs[i].payload = make_uint<8>(0x10 + i * 0x10);
        inputs[i].valid = ch_bool(true);
    }

    auto arb_result = stream_arbiter_round_robin<ch_uint<8>, N>(inputs);
    arb_result.output_stream.ready = ch_bool(true);

    ch::Simulator sim(&ctx);

    // 运行多个周期验证轮询行为
    for (int cycle = 0; cycle < 8; ++cycle) {
        sim.tick();
        sim.tick();

        auto out = static_cast<uint64_t>(sim.get_value(arb_result.output_stream.payload));
        auto valid = static_cast<uint64_t>(sim.get_value(arb_result.output_stream.valid));
        auto selected = static_cast<uint64_t>(sim.get_value(arb_result.selected));

        std::cout << "  Cycle " << cycle << ": selected=" << selected
                  << " out=0x" << std::hex << out << " valid=" << valid << std::dec;
        if (valid != 1) { std::cout << " FAIL" << std::endl; return 1; }
        std::cout << " PASS" << std::endl;
    }

    std::cout << "\nStream Arbiter test completed successfully!" << std::endl;
    return 0;
}
