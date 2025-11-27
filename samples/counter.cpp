// samples/counter.cpp
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/codegen.h"
#include "../include/utils/logger.h"
#include <iostream>
#include <memory>

using namespace ch::core;

template<unsigned N>
class Counter : public ch::Component {
public:
    __io(ch_out<ch_uint<N>> out;)

    Counter(ch::Component* parent = nullptr, const std::string& name = "counter")
        : ch::Component(parent, name)
    {}

    void create_ports() override {
        CHDBG_FUNC();
        new (io_storage_) io_type;
        CHDBG("IO structure created for Counter");

        // 添加调试验证
        if (!io().out.impl()) {
            CHERROR("Counter output port not properly initialized!");
        }
    }

    void describe() override {
        CHDBG_FUNC();
        ch_reg<ch_uint<N>> reg(0_d); // 使用字面量初始化
        // Use the default clock for register updates
        reg->next = reg + 1_d; // 明确指定1为字面量
        io().out = reg;
        CHDBG("Counter logic described");
    }
};

class Top : public ch::Component {
public:
    __io(ch_out<ch_uint<4>> out;)

    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name)
    {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        CH_MODULE(Counter<4>, counter1);
        io().out = counter1.io().out;
    }
};

int main() {
    {
        // Create device and simulator in proper order to ensure correct destruction
        ch::ch_device<Top> device;
        
        // Create simulator
        ch::Simulator simulator(device.context());
        
        for (int i = 0; i < 18; ++i) {
            simulator.tick();
            std::cout << "Cycle " << i << ": out = " << simulator.get_value(device.instance().io().out) << std::endl;
        }
        
        std::cout << "Program completed successfully" << std::endl;
        
        // Generate Verilog before the device is destroyed
        ch::toVerilog("counter.v", device.context());
    }
    
    // Set flag to indicate we're in static destruction phase to avoid logging
    //ch::detail::set_static_destruction();
    
    return 0;
}