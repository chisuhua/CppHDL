#include "codegen_dag.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/reg.h"
#include "core/uint.h"
#include "device.h"
#include "simulator.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

class ShiftRegister : public ch::Component {
public:
    __io(ch_in<ch_uint<1>> in; ch_out<ch_uint<4>> out;)

    ShiftRegister(ch::Component *parent = nullptr,
                  const std::string &name = "shift_reg")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<1>> bit1(0_b);
        ch_reg<ch_uint<1>> bit2(0_b);
        ch_reg<ch_uint<1>> bit3(0_b);
        ch_reg<ch_uint<1>> bit4(0_b);

        bit1->next = io().in;
        bit2->next = bit1;
        bit3->next = bit2;
        bit4->next = bit3;

        auto r1 = concat(bit4, bit3);
        auto r2 = concat(r1, bit2);
        io().out = concat(r2, bit1);
    }
};

class ShifterRegisterNoConcat : public ch::Component {
public:
    __io(ch_in<ch_uint<1>> in;
         ch_out<ch_uint<1>> out0;
         ch_out<ch_uint<1>> out1;
         ch_out<ch_uint<1>> out2;
         ch_out<ch_uint<1>> out3;)

    ShifterRegisterNoConcat(ch::Component *parent = nullptr,
                            const std::string &name = "shifter_no_concat")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<1>> bit1(0_b);
        ch_reg<ch_uint<1>> bit2(0_b);
        ch_reg<ch_uint<1>> bit3(0_b);
        ch_reg<ch_uint<1>> bit4(0_b);

        bit1->next = io().in;
        bit2->next = bit1;
        bit3->next = bit2;
        bit4->next = bit3;

        io().out3 = bit4;
        io().out2 = bit3;
        io().out1 = bit2;
        io().out0 = bit1;
    }
};

int main() {
    {
        std::cout << "=== ShiftRegister (with concat) ===" << std::endl;
        ch_device<ShiftRegister> device;
        ch::Simulator sim(device.context());
        sim.set_input_value(device.instance().io().in, 1);
        sim.tick();
        ch::toDAG("/tmp/shift_register_concat.dot", device.context(), sim);
        std::cout << "Saved: /tmp/shift_register_concat.dot" << std::endl;
    }
    {
        std::cout << "\n=== ShifterRegisterNoConcat ===" << std::endl;
        ch_device<ShifterRegisterNoConcat> device;
        ch::Simulator sim(device.context());
        sim.set_input_value(device.instance().io().in, 1);
        sim.tick();
        ch::toDAG("/tmp/shift_register_no_concat.dot", device.context(), sim);
        std::cout << "Saved: /tmp/shift_register_no_concat.dot" << std::endl;
    }
    return 0;
}
