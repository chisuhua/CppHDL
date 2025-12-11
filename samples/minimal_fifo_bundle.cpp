// samples/minimal_fifo_bundle.cpp
#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "io/common_bundles.h"
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

class Top : public ch::Component {
public:
    fifo_bundle<ch_uint<2>> io_bundle;

    Top(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {
        io_bundle.as_master();
    }

    void create_ports() override {
        // No traditional ports
    }

    void describe() override {
        ch::ch_module<SimpleFifo<ch_uint<2>, 2>> fifo_inst{"fifo_inst"};
        connect(io_bundle, fifo_inst.instance().io);
    }
};

int main() {
    ch::ch_device<Top> top_device;
    ch::Simulator sim(top_device.context());

    // Test basic Bundle functionality
    uint64_t initial_value = 0;
    sim.set_bundle_value(top_device.instance().io_bundle, initial_value);

    std::cout << "Starting minimal FIFO Bundle test..." << std::endl;

    for (int cycle = 0; cycle < 3; ++cycle) {
        sim.tick();
        uint64_t bundle_value =
            sim.get_bundle_value(top_device.instance().io_bundle);

        std::cout << "Cycle " << cycle << ": bundle_value=0x" << std::hex
                  << bundle_value << std::dec << std::endl;
    }

    std::cout << "Minimal FIFO Bundle test completed successfully!"
              << std::endl;
    return 0;
}