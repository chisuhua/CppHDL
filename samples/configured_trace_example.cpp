// examples/configured_trace_example.cpp
// 示例：如何使用配置文件控制信号跟踪

#include "ch.hpp"
#include "simulator.h"
#include <iostream>

using namespace ch::core;

// 简单的计数器模块
template <unsigned N> ch_bool counter_top(ch_uint<N> &count) {
    static ch_reg<ch_uint<N>> cnt(0, "counter_reg");
    static ch_bool overflow("overflow");

    cnt->next = cnt + 1_d;

    count = cnt;
    overflow = cnt == static_cast<uint64_t>((1 << N) - 1);

    return overflow;
}

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("configured_trace_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // 定义输出信号
    ch_uint<8> count_out;
    ch_bool overflow_out;

    // 实例化模块
    auto overflow = counter_top<8>(count_out);
    overflow_out = overflow;

    // 创建带配置文件的模拟器
    ch::Simulator sim(ctx.get(), "trace.ini");

    // 运行几个周期
    std::cout << "Running simulation with configured trace..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        sim.tick();
        std::cout << "Cycle " << i
                  << ": count=" << static_cast<uint64_t>(count_out)
                  << ", overflow=" << static_cast<bool>(overflow_out)
                  << std::endl;
    }

    sim.toVCD("configred_trace_example.vcd");

    std::cout << "Traced signals count: " << sim.get_traced_signals_count()
              << std::endl;
    std::cout << "Trace enabled: " << sim.is_tracing_enabled() << std::endl;

    return 0;
}