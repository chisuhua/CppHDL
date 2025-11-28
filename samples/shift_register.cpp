#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/core/reg.h"
#include "../include/core/bool.h"
#include "../include/core/uint.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// Component for testing shift register behavior
class ShiftRegister : public ch::Component {
public:
    __io(
        ch_in<ch_uint<1>> in;
        ch_out<ch_uint<4>> out;
    )

    ShiftRegister(ch::Component* parent = nullptr, const std::string& name = "shift_reg")
        : ch::Component(parent, name)
    {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // Create a 4-bit shift register
        ch_reg<ch_uint<1>> bit1(0_b);
        ch_reg<ch_uint<1>> bit2(0_b);
        ch_reg<ch_uint<1>> bit3(0_b);
        ch_reg<ch_uint<1>> bit4(0_b);
        
        bit1->next = io().in;
        bit2->next = bit1;
        bit3->next = bit2;
        bit4->next = bit3;

        auto result1 = concat(bit4, bit3);
        auto result2 = concat(result1, bit2);
        io().out = concat(result2, bit1);
        
        // Concatenate bits to form output
        //io().out = concat(concat(concat(bit4, bit3), bit2), bit1);
    }
};

int main() {
    std::cout << "Starting Shift Register Timing test" << std::endl;
    
    // Create device and simulator in proper order to ensure correct destruction
    ch_device<ShiftRegister> device;
    
    // Create simulator
    Simulator simulator(device.context());
    
    // Tick 0 - Initially all zeros
    simulator.tick();
    auto val = simulator.get_port_value(device.instance().io().out);
    std::cout << "Cycle 0: out = " << static_cast<uint64_t>(val) << " (expected: 0 - 0000)" << std::endl;
    
    // Shift in a 1
    simulator.set_input_value(device.instance().io().in, 1);
    
    // Tick 1 - 1 in bit1
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    std::cout << "Cycle 1: out = " << static_cast<uint64_t>(val) << " (expected: 1 - 0001)" << std::endl;
    
    // Tick 2 - 1 in bit2
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    std::cout << "Cycle 2: out = " << static_cast<uint64_t>(val) << " (expected: 2 - 0010)" << std::endl;
    
    // Tick 3 - 1 in bit3
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    std::cout << "Cycle 3: out = " << static_cast<uint64_t>(val) << " (expected: 4 - 0100)" << std::endl;
    
    // Tick 4 - 1 in bit4 (output)
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    std::cout << "Cycle 4: out = " << static_cast<uint64_t>(val) << " (expected: 8 - 1000)" << std::endl;
    
    // Shift in a 0
    simulator.set_input_value(device.instance().io().in, 0);
    
    // Tick 5 - 0 in bit1, 1 in bit2
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    std::cout << "Cycle 5: out = " << static_cast<uint64_t>(val) << " (expected: 4 - 0100)" << std::endl;
    
    // Tick 6 - 0 in bit2, 1 in bit3
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    std::cout << "Cycle 6: out = " << static_cast<uint64_t>(val) << " (expected: 2 - 0010)" << std::endl;
    
    // Tick 7 - 0 in bit3, 1 in bit4
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    std::cout << "Cycle 7: out = " << static_cast<uint64_t>(val) << " (expected: 1 - 0001)" << std::endl;
    
    // Tick 8 - All zeros
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    std::cout << "Cycle 8: out = " << static_cast<uint64_t>(val) << " (expected: 0 - 0000)" << std::endl;
    
    std::cout << "Finished Shift Register test" << std::endl;
    
    return 0;
}