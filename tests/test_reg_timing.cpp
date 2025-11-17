// tests/test_reg_timing.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "core/context.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/reg.h"
#include "simulator.h"
#include "component.h"
#include "module.h"
#include "device.h"
#include "utils/logger.h"
#include <memory>

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

TEST_CASE("Register - Feedback Counter Structure", "[reg][feedback][structure]") {
    // Test that we can declare the component
    REQUIRE(std::is_class_v<FeedbackCounter>);
    
    // Test instantiation
    context ctx("feedback_counter_test");
    ctx_swap swap(&ctx);
    
    ch_reg<ch_uint<4>> counter(0_d);
    REQUIRE(counter.impl() != nullptr);
    
    auto incremented = counter + 1_d;
    REQUIRE(incremented.impl() != nullptr);
    
    counter->next = incremented;
    // next_assignment_proxy doesn't support comparison operations
}

TEST_CASE("Register - Feedback Counter Timing", "[reg][feedback][timing]") {
    // Create device and simulator in proper order to ensure correct destruction
    ch_device<FeedbackCounter> device;
    
    // Create simulator
    Simulator simulator(device.context());
    
    for (int i = 0; i <= 10; ++i) {
        simulator.tick();
        auto val = simulator.get_port_value(device.instance().io().out);
        REQUIRE(static_cast<uint64_t>(val) == i);
    }
}

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

TEST_CASE("Register - Multi-Stage Pipeline Structure", "[reg][pipeline][structure]") {
    // Test that we can declare the component
    REQUIRE(std::is_class_v<MultiStagePipeline>);
    
    // Test instantiation
    context ctx("pipeline_test");
    ctx_swap swap(&ctx);
    
    // Create a 3-stage pipeline
    ch_reg<ch_uint<4>> stage1(0_d);
    ch_reg<ch_uint<4>> stage2(0_d);
    ch_reg<ch_uint<4>> stage3(0_d);
    
    REQUIRE(stage1.impl() != nullptr);
    REQUIRE(stage2.impl() != nullptr);
    REQUIRE(stage3.impl() != nullptr);
    
    // Test connections
    ch_uint<4> input(9_d);
    stage1->next = input;
    stage2->next = stage1;
    stage3->next = stage2;
    
    // next_assignment_proxy doesn't support comparison operations
}

TEST_CASE("Register - Multi-Stage Pipeline Timing", "[reg][pipeline][timing]") {
    // Create device and simulator in proper order to ensure correct destruction
    ch_device<MultiStagePipeline> device;
    
    // Create simulator
    Simulator simulator(device.context());
    
    // Tick 0 - Initial state
    simulator.tick();
    auto val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 0); // Initial value
    
    // Set input value
    simulator.set_input_value(device.instance().io().in, static_cast<uint64_t>(9));
    
    // Tick 1 - Data in stage1
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 0); // Still initial value
    
    // Tick 2 - Data in stage2
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 0); // Still initial value
    
    // Tick 3 - Data in stage3/output
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 9); // Now we see the input value
    
    // Change input
    simulator.set_input_value(device.instance().io().in, static_cast<uint64_t>(5));
    
    // Tick 4 - New input in stage1, old value still propagating
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 9); // Still seeing old value
    
    // Tick 5 - New input in stage2
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 9); // Still seeing old value
    
    // Tick 6 - New input reaches output
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 5); // Now seeing new value
}

// Component for testing shift register behavior
class ShiftRegister : public ch::Component {
public:
    __io(
        ch_in<ch_bool> in;
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
        ch_reg<ch_bool> bit1(0_b);
        ch_reg<ch_bool> bit2(0_b);
        ch_reg<ch_bool> bit3(0_b);
        ch_reg<ch_bool> bit4(0_b);
        
        bit1->next = io().in;
        bit2->next = bit1;
        bit3->next = bit2;
        bit4->next = bit3;
        
        // Concatenate bits to form output
        io().out = concat(bit4, concat(bit3, concat(bit2, bit1)));
    }
};

TEST_CASE("Register - Shift Register Structure", "[reg][shift][structure]") {
    // Test that we can declare the component
    REQUIRE(std::is_class_v<ShiftRegister>);
    
    // Test instantiation
    context ctx("shift_register_test");
    ctx_swap swap(&ctx);
    
    // Create a 4-bit shift register
    ch_reg<ch_bool> bit1(0_b);
    ch_reg<ch_bool> bit2(0_b);
    ch_reg<ch_bool> bit3(0_b);
    ch_reg<ch_bool> bit4(0_b);
    
    REQUIRE(bit1.impl() != nullptr);
    REQUIRE(bit2.impl() != nullptr);
    REQUIRE(bit3.impl() != nullptr);
    REQUIRE(bit4.impl() != nullptr);
    
    // Test connections
    ch_bool input(1_b);
    bit1->next = input;
    bit2->next = bit1;
    bit3->next = bit2;
    bit4->next = bit3;
    
    // Test concatenation
    auto concatenated = concat(bit4, concat(bit3, concat(bit2, bit1)));
    REQUIRE(concatenated.impl() != nullptr);
    
    // next_assignment_proxy doesn't support comparison operations
}

TEST_CASE("Register - Shift Register Timing", "[reg][shift][timing]") {
    // Create device and simulator in proper order to ensure correct destruction
    ch_device<ShiftRegister> device;
    
    // Create simulator
    Simulator simulator(device.context());
    
    // Tick 0 - Initially all zeros
    simulator.tick();
    auto val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 0); // 0000
    
    // Shift in a 1
    simulator.set_input_value(device.instance().io().in, static_cast<uint64_t>(1));
    
    // Tick 1 - 1 in bit1
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 1); // 0001
    
    // Tick 2 - 1 in bit2
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 2); // 0010
    
    // Tick 3 - 1 in bit3
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 4); // 0100
    
    // Tick 4 - 1 in bit4 (output)
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 8); // 1000
    
    // Shift in a 0
    simulator.set_input_value(device.instance().io().in, static_cast<uint64_t>(0));
    
    // Tick 5 - 0 in bit1, 1 in bit2
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 4); // 0100
    
    // Tick 6 - 0 in bit2, 1 in bit3
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 2); // 0010
    
    // Tick 7 - 0 in bit3, 1 in bit4
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 1); // 0001
    
    // Tick 8 - All zeros
    simulator.tick();
    val = simulator.get_port_value(device.instance().io().out);
    REQUIRE(static_cast<uint64_t>(val) == 0); // 0000
}