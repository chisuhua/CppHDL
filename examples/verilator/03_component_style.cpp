// examples/verilator/03_component_style.cpp
// Verilator 仿真后端 - Component 风格 (ch_device)
//
// 注: 当前 VerilatorBackend 仍要求原始 context 接口
// (verilator_backend.cpp 需要非 Component 包装的 context)。
// 此示例展示的是推荐的 CppHDL 风格, 配合默认 Simulator
// 验证设计正确性. 切换到 VerilatorBackend 需要
// Phase 2.3+ 完成后使用 sim.set_backend().
#include "ch.hpp"
#include "component.h"
#include "core/reg.h"
#include "core/uint.h"
#include "module.h"
#include "simulator.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

class CounterWithEnable : public ch::Component {
public:
    __io(ch_in<ch_uint<4>>  incr;
         ch_in<ch_bool>     enable;
         ch_out<ch_uint<4>> count;)

    CounterWithEnable(ch::Component *parent = nullptr,
                      const std::string &name = "counter")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<4>> reg_c(0);
        reg_c->next = select(io().enable, reg_c + io().incr, reg_c);
        io().count = reg_c;
    }
};

int main() {
    ch_device<CounterWithEnable> device;
    Simulator sim(device.context());

    for (int cycle = 0; cycle < 5; ++cycle) {
        sim.set_input_value(device.instance().io().incr, 1);
        sim.set_input_value(device.instance().io().enable, true);
        sim.tick();
        auto val = sim.get_value(device.instance().io().count);
        std::cout << "Cycle " << cycle
                  << ": count = " << static_cast<uint64_t>(val) << "\n";
    }
    return 0;
}
