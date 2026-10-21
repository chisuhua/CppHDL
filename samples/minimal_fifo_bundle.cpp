// samples/minimal_fifo_bundle.cpp
#include "bundle/common_bundles.h"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;

template <typename T, unsigned N> class SimpleFifo : public ch::Component {
public:
    static_assert((N & (N - 1)) == 0, "FIFO size must be power of 2");

    fifo_bundle<T> io;

    SimpleFifo(ch::Component *parent = nullptr,
               const std::string &name = "fifo")
        : ch::Component(parent, name) {
        io.as_slave();
    }

    void describe() override {
        // Simple implementation - just pass through for testing
        io.full = false;
        io.empty = true;
    }
};

// 顶层用**普通端口**承载 TB 接口，bundle 只用于**模块间**连接。
// ⚠️ 顶层用 bundle 的问题：bundle 成员**不会**被注册为组件 IO ⇒ 它的信号不会
// 成为生成 Verilog 的顶层端口（实测：改前生成的 fifo_bundle.v 顶层只有
// default_clock / default_reset）⇒ 外部 TB 无法驱动。
// 见 OpenSpec change `bundle-master-slave-io-2026-10` 的 D6。
class Top : public ch::Component {
public:
    __io(ch_in<ch_uint<2>> data; ch_in<ch_bool> push; ch_in<ch_bool> pop;
         ch_out<ch_bool> full; ch_out<ch_bool> empty;)

    Top(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<SimpleFifo<ch_uint<2>, 2>> fifo_inst{"fifo_inst"};
        fifo_inst.instance().io.push <<= io().push;
        fifo_inst.instance().io.pop <<= io().pop;
        fifo_inst.instance().io.data <<= io().data;
        io().full <<= fifo_inst.instance().io.full;
        io().empty <<= fifo_inst.instance().io.empty;
    }
};

int main() {
    ch::ch_device<Top> top_device;
    ch::Simulator sim(top_device.context());

    // Test basic Bundle functionality（顶层是普通端口）
    auto &io = top_device.instance().io();
    sim.set_input_value(io.data, 0);
    sim.set_input_value(io.push, 0);
    sim.set_input_value(io.pop, 0);

    std::cout << "Starting minimal FIFO Bundle test..." << std::endl;

    for (int cycle = 0; cycle < 3; ++cycle) {
        sim.tick();
        uint64_t full_val =
            static_cast<uint64_t>(sim.get_port_value(io.full));
        uint64_t empty_val =
            static_cast<uint64_t>(sim.get_port_value(io.empty));

        std::cout << "Cycle " << cycle << ": full=" << full_val
                  << ", empty=" << empty_val << std::endl;
    }

    std::cout << "Minimal FIFO Bundle test completed successfully!"
              << std::endl;
    return 0;
}