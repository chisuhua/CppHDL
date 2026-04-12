/**
 * @file stream_demux_demo.cpp
 * @brief Stream Demux 演示 - 1 到 4 解复用器
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
    std::cout << "CppHDL vs SpinalHDL - Stream Demux Example" << std::endl;
    std::cout << "===========================================" << std::endl;

    const unsigned N = 4;

    ch::core::context ctx("stream_demux_test");
    ch::core::ctx_swap swap(&ctx);

    ch_stream<ch_uint<8>> input_stream;
    input_stream.payload = make_uint<8>(0xAB);
    input_stream.valid = ch_bool(true);

    ch_uint<2> select_signal(0_d);
    auto demux_result = stream_demux<ch_uint<8>, N>(input_stream, select_signal);

    ch::Simulator sim(&ctx);

    for (int sel = 0; sel < 4; ++sel) {
        sim.set_input_value(select_signal, sel);
        sim.tick();
        sim.tick();

        for (int i = 0; i < 4; ++i) {
            auto v = static_cast<uint64_t>(sim.get_value(demux_result.output_streams[i].valid));
            if (sel == i) {
                std::cout << "  select=" << sel << ", out[" << i << "].valid=" << v << " (expect 1)";
                if (v != 1) { std::cout << " FAIL" << std::endl; return 1; }
                std::cout << " PASS" << std::endl;
            }
        }
    }

    std::cout << "\nStream Demux test completed successfully!" << std::endl;
    return 0;
}
