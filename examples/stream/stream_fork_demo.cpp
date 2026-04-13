/**
 * @file stream_fork_demo.cpp
 * @brief Stream Fork 演示 - 1 到 3 同步复制器
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
    std::cout << "CppHDL vs SpinalHDL - Stream Fork Example" << std::endl;
    std::cout << "=========================================" << std::endl;

    const unsigned N = 3;

    ch::core::context ctx("stream_fork_test");
    ch::core::ctx_swap swap(&ctx);

    ch_stream<ch_uint<8>> input_stream;
    input_stream.payload = make_uint<8>(0x5A);
    input_stream.valid = ch_bool(true);

    auto fork_result = stream_fork<ch_uint<8>, N>(input_stream, true);
    for (unsigned i = 0; i < N; ++i) {
        fork_result.output_streams[i].ready = ch_bool(true);
    }

    ch::Simulator sim(&ctx);
    sim.tick();
    sim.tick();

    for (unsigned i = 0; i < N; ++i) {
        auto v = static_cast<uint64_t>(sim.get_value(fork_result.output_streams[i].valid));
        auto d = static_cast<uint64_t>(sim.get_value(fork_result.output_streams[i].payload));
        std::cout << "  Output[" << i << "]: valid=" << v << " data=0x" << std::hex << d << std::dec;
        if (v != 1 || d != 0x5A) { std::cout << " FAIL" << std::endl; return 1; }
        std::cout << " PASS" << std::endl;
    }

    std::cout << "\nStream Fork test completed successfully!" << std::endl;
    return 0;
}
