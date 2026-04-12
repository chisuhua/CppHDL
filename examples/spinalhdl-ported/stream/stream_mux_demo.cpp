/**
 * @file stream_mux_demo.cpp
 * @brief Stream Mux 演示 - 4 选 1 流多路选择器
 * @author DevMate
 * @date 2026-04-12
 */

#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include "chlib/stream.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

int main() {
    std::cout << "CppHDL vs SpinalHDL - Stream Mux Example" << std::endl;
    std::cout << "=========================================" << std::endl;

    const unsigned N = 4;

    ch::core::context ctx("stream_mux_test");
    ch::core::ctx_swap swap(&ctx);

    std::array<ch_stream<ch_uint<8>>, N> inputs;

    for (unsigned i = 0; i < N; ++i) {
        inputs[i].payload = make_uint<8>(0x10 + i * 0x10);
        inputs[i].valid = ch_bool(true);
    }

    ch_uint<2> select_signal(0_d);
    auto mux_result = stream_mux<N, ch_uint<8>>(inputs, select_signal);
    mux_result.output_stream.ready = ch_bool(true);
    stream_mux_connect_ready(mux_result);

    ch::Simulator sim(&ctx);

    for (int sel = 0; sel < 4; ++sel) {
        sim.set_input_value(select_signal, sel);
        sim.tick();
        sim.tick();

        auto out = static_cast<uint64_t>(sim.get_value(mux_result.output_stream.payload));
        auto out_v = static_cast<uint64_t>(sim.get_value(mux_result.output_stream.valid));
        uint64_t expected = 0x10 + sel * 0x10;

        std::cout << "  select=" << sel << " (expect 0x" << std::hex << expected << ")";
        std::cout << ": out=0x" << out << ", valid=" << out_v << std::dec;

        if (out != expected || out_v != 1) {
            std::cout << " FAIL" << std::endl;
            return 1;
        }
        std::cout << " PASS" << std::endl;
    }

    std::cout << "\nStream Mux test completed successfully!" << std::endl;
    return 0;
}
