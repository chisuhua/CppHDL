#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/core/reg.h"
#include "../include/core/uint.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// Component for testing multi-register pipeline
class MultiStagePipeline : public ch::Component {
public:
    __io(
        ch_in<ch_uint<4>> in;
        ch_out<ch_uint<4>> out;
    )

    MultiStagePipeline(ch::Component* parent = nullptr, const std::string& name = "pipeline")
        : ch::Component(parent, name)
    {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // Create a 3-stage pipeline
        ch_reg<ch_uint<4>> stage1(0);
        ch_reg<ch_uint<4>> stage2(0);
        ch_reg<ch_uint<4>> stage3(0);
        
        stage1->next = io().in;
        stage2->next = stage1;
        stage3->next = stage2;
        io().out = stage3;
    }
};

int main() {
    std::cout << "Starting Multi-Stage Pipeline Timing test" << std::endl;
    
    // Create device and simulator in proper order to ensure correct destruction
    ch_device<MultiStagePipeline> device;
    
    // Create simulator
    Simulator simulator(device.context());
    
    // Tick 0 - Initial state
    simulator.tick();
    auto val = simulator.get_value(device.instance().io().out);
    std::cout << "Cycle 0: out = " << static_cast<uint64_t>(val) << " (expected: 0)" << std::endl;
    
    // Set input value
    simulator.set_input_value(device.instance().io().in, static_cast<uint64_t>(9));
    
    // Tick 1 - Data in stage1
    simulator.tick();
    val = simulator.get_value(device.instance().io().out);
    std::cout << "Cycle 1: out = " << static_cast<uint64_t>(val) << " (expected: 0)" << std::endl;
    
    // Tick 2 - Data in stage2
    simulator.tick();
    val = simulator.get_value(device.instance().io().out);
    std::cout << "Cycle 2: out = " << static_cast<uint64_t>(val) << " (expected: 0)" << std::endl;
    
    // Tick 3 - Data in stage3/output
    simulator.tick();
    val = simulator.get_value(device.instance().io().out);
    std::cout << "Cycle 3: out = " << static_cast<uint64_t>(val) << " (expected: 9)" << std::endl;
    
    // Change input
    simulator.set_input_value(device.instance().io().in, static_cast<uint64_t>(5));
    
    // Tick 4 - New input in stage1, old value still propagating
    simulator.tick();
    val = simulator.get_value(device.instance().io().out);
    std::cout << "Cycle 4: out = " << static_cast<uint64_t>(val) << " (expected: 9)" << std::endl;
    
    // Tick 5 - New input in stage2
    simulator.tick();
    val = simulator.get_value(device.instance().io().out);
    std::cout << "Cycle 5: out = " << static_cast<uint64_t>(val) << " (expected: 9)" << std::endl;
    
    // Tick 6 - New input reaches output
    simulator.tick();
    val = simulator.get_value(device.instance().io().out);
    std::cout << "Cycle 6: out = " << static_cast<uint64_t>(val) << " (expected: 5)" << std::endl;
    
    std::cout << "Finished Multi-Stage Pipeline test" << std::endl;
    
    // Clean up properly before static destruction
    ch::pre_static_destruction_cleanup();
    
    // Also set the static destruction flag to prevent any further logging
    ch::detail::set_static_destruction();
    
    return 0;
}