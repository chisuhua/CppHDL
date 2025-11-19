#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/core/reg.h"
#include "../include/core/uint.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// Component for testing feedback loop
class FeedbackCounter : public ch::Component {
public:
    __io(ch_out<ch_uint<4>> out;)

    FeedbackCounter(ch::Component* parent = nullptr, const std::string& name = "feedback_counter")
        : ch::Component(parent, name)
    {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch_reg<ch_uint<4>> counter(0);
        counter->next = counter + 1;
        io().out = counter;
    }
};

int main() {
    std::cout << "Starting Feedback Counter Timing test" << std::endl;
    
    // Create device and simulator in proper order to ensure correct destruction
    std::cout << "Creating device" << std::endl;
    ch_device<FeedbackCounter> device;
    
    std::cout << "Created device" << std::endl;
    // Create simulator
    std::cout << "Creating simulator" << std::endl;
    Simulator simulator(device.context());
    
    std::cout << "Created simulator" << std::endl;
    
    for (int i = 0; i <= 10; ++i) {
        simulator.tick();
        auto val = simulator.get_value(device.instance().io().out);
        std::cout << "Cycle " << i << ": out = " << static_cast<uint64_t>(val) << std::endl;
        // Expected: i == val
    }
    
    std::cout << "Finished Feedback Counter test" << std::endl;
    
    // Clean up properly before static destruction
    ch::pre_static_destruction_cleanup();
    
    // Also set the static destruction flag to prevent any further logging
    ch::detail::set_static_destruction();
    
    return 0;
}