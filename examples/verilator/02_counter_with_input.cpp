// examples/verilator/02_counter_with_input.cpp
// Verilator 仿真后端 - 带输入端口的计数器
// 演示: ch_in<> 输入 + VCD toggle + 元数据访问
#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/verilator_backend.h"
#include "core/eval_backend.h"
#include <filesystem>
#include <iostream>

using namespace ch::core;

int main() {
    auto ctx = std::make_unique<context>("counter_with_input");
    ctx_swap guard(ctx.get());

    ch_in<ch_uint<4>>  incr("incr");
    ch_in<ch_bool>     enable("enable");
    ch_out<ch_uint<4>> count("count");

    ch_reg<ch_uint<4>> reg_c(0_d, "counter");
    reg_c->next = select(enable, reg_c + incr, reg_c);
    count <<= reg_c;

    ch::data_map_t data_map;
    std::filesystem::create_directories("/tmp/vl_counter_in");
    ch::VerilatorBackend backend("/tmp/vl_counter_in");
    if (!backend.initialize(ctx.get(), data_map)) {
        std::cerr << "Failed to initialize Verilator backend\n";
        return 1;
    }

    std::cout << "Verilator backend initialized\n";
    std::cout << "  name()         = " << backend.name() << "\n";
    std::cout << "  is_native()    = " << std::boolalpha
              << backend.is_native() << "\n";
    std::cout << "  clock_node_id_ = " << backend.clock_node_id() << "\n";
    std::cout << "  vcd_enabled    = " << backend.vcd_enabled() << "\n";

    backend.enable_vcd(true);
    std::cout << "  After enable_vcd(true) = " << backend.vcd_enabled() << "\n";

    std::vector<std::pair<uint32_t, ch::instr_base *>> empty;
    backend.eval_combinational(data_map, empty, empty);
    backend.eval_sequential(data_map, empty);
    std::cout << "Counter example completed\n";
    return 0;
}
