// examples/verilator/01_minimal_counter.cpp
// Verilator 仿真后端 - 最简示例
// 演示: 4-bit 计数器的 Verilog 生成 + verilator 编译 + dlopen
#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/verilator_backend.h"
#include "core/eval_backend.h"
#include <filesystem>
#include <iostream>

using namespace ch::core;

int main() {
    auto ctx = std::make_unique<context>("minimal_counter");
    ctx_swap guard(ctx.get());

    ch_out<ch_uint<4>> out_port("io");
    ch_reg<ch_uint<4>> reg_c(0_d, "counter");
    reg_c->next = reg_c + 1_d;
    out_port <<= reg_c;

    ch::data_map_t data_map;
    std::filesystem::create_directories("/tmp/vl_minimal");
    ch::VerilatorBackend backend("/tmp/vl_minimal");
    if (!backend.initialize(ctx.get(), data_map)) {
        std::cerr << "Failed to initialize Verilator backend\n";
        return 1;
    }

    std::vector<std::pair<uint32_t, ch::instr_base *>> empty;
    backend.eval_combinational(data_map, empty, empty);
    backend.eval_sequential(data_map, empty);
    std::cout << "Minimal example completed\n";
    return 0;
}
