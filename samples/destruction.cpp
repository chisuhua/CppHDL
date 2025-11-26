#include "ch.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "component.h"
#include "simulator.h"
#include "utils/destruction_manager.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

class SimpleComponent : public Component {
public:
    __io(
        ch_out<ch_uint<4>> out;
    )
    
    SimpleComponent(ch::Component* parent = nullptr, const std::string& name = "simple") 
        : Component(parent, name) {
    }
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        auto reg = ch_reg<ch_uint<4>>(0);
        reg->next = reg + 1;
        io().out = reg;
    }
};

int main() {
    std::cout << "Creating device and simulator..." << std::endl;
    
    // Create device and simulator
    ch::ch_device<SimpleComponent> device;
    ch::Simulator simulator(device.context());
    
    std::cout << "Running simulation..." << std::endl;
    
    // Run a few cycles
    for (int i = 0; i < 5; ++i) {
        simulator.tick();
        std::cout << "Cycle " << i << ": out = " << simulator.get_value(device.instance().io().out) << std::endl;
    }
    
    std::cout << "Program completed successfully" << std::endl;
    
    // Clean up properly before static destruction
    //ch::pre_static_destruction_cleanup();
    
    // Also set the static destruction flag to prevent any further logging
    //ch::detail::set_static_destruction();
    
    return 0;
}