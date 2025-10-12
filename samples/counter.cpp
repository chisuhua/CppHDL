// samples/counter.cpp
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/codegen.h"
#include <iostream>

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
        ch_reg<ch_uint<N>> reg(0);
        reg->next = reg + 1;
        io().out = ~reg; // ✅ 通过 io() 访问
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
        //ch::ch_module<Counter<4>> counter1("counter1");
        io().out = counter1.io().out;
    }
};

int main() {
    ch::ch_device<Top> top_device;

    ch::Simulator sim(top_device.context());
    for (int i = 0; i < 18; ++i) {
        sim.tick();
        std::cout << "Cycle " << i << ": out = " << sim.get_value(top_device.instance().io().out) << std::endl;
    }

    ch::toVerilog("counter.v", top_device.context());
    return 0;
}
