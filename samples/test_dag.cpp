#include "ch.hpp"
#include "codegen_dag.h"
#include "simulator.h"
#include <iostream>

int main() {
    // 创建一个空的上下文并生成空的DAG图
    ch::core::context ctx;
    ch::toDAG("empty_test.dot", &ctx);
    std::cout << "Generated empty_test.dot" << std::endl;

    // 创建一个带简单电路的上下文
    auto ctx_with_circuit = std::make_unique<ch::core::context>("test_circuit");
    ch::core::ctx_swap ctx_swapper(ctx_with_circuit.get());

    // 创建一个简单的加法电路
    ch::core::ch_uint<8> a(5_d, "a");
    ch::core::ch_uint<8> b(3_d, "b");
    auto sum1 = a + b;
    auto sum2 = a + b;

    // 创建仿真器
    ch::Simulator simulator(ctx_with_circuit.get());

    // 运行仿真
    simulator.tick();

    // 生成带值的DAG图
    ch::toDAG("circuit_with_values.dot", ctx_with_circuit.get(), simulator);
    std::cout << "Generated circuit_with_values.dot with simulation values"
              << std::endl;

    // 创建一个更复杂的电路 - 包含寄存器的时序电路
    auto ctx_sequential =
        std::make_unique<ch::core::context>("sequential_circuit");
    ch::core::ctx_swap seq_ctx_swapper(ctx_sequential.get());

    // 创建时钟和输入信号
    ch::core::ch_uint<8> data_in = 10_d;

    // 创建一个简单的计数器
    auto counter = ch::core::ch_reg<ch::core::ch_uint<8>>(0);
    counter->next = counter + 1_d;

    // 连接电路
    ch::core::ch_uint<8> result = data_in + counter;

    // 创建仿真器并运行多个时钟周期
    ch::Simulator seq_simulator(ctx_sequential.get());

    // 运行几个仿真周期
    for (int i = 0; i < 5; ++i) {
        seq_simulator.tick();
        std::cout << "Cycle " << i << ": counter = "
                  << static_cast<uint64_t>(seq_simulator.get_value(counter))
                  << ", result = "
                  << static_cast<uint64_t>(seq_simulator.get_value(result))
                  << std::endl;
    }

    // 生成带值的DAG图
    ch::toDAG("sequential_circuit_with_values.dot", ctx_sequential.get(),
              seq_simulator);
    std::cout
        << "Generated sequential_circuit_with_values.dot with simulation values"
        << std::endl;

    return 0;
}