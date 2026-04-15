/**
 * @file test_wrapper_ifstage.cpp
 * @brief Test ch_module<IfStage> inside Component::describe()
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "component.h"
#include "../../include/cpu/pipeline/stages/if_stage.h"

using namespace ch::core;
using namespace riscv;

class IfStageWrapper : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> instr_data;
        ch_in<ch_bool>      instr_ready;
        ch_out<ch_uint<32>> pc;
        ch_out<ch_uint<32>> instruction;
    )

    IfStageWrapper(ch::Component* parent = nullptr, const std::string& name = "if_wrapper")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<IfStage> if_stage{"if_stage"};
        if_stage.io().stall <<= ch_bool(false);
        if_stage.io().flush <<= ch_bool(false);
        if_stage.io().rst <<= ch_bool(false);
        if_stage.io().branch_target <<= ch_uint<32>(0_d);
        if_stage.io().branch_valid <<= ch_bool(false);
        if_stage.io().instr_data <<= io().instr_data;
        if_stage.io().instr_ready <<= io().instr_ready;
        io().pc <<= if_stage.io().pc;
        io().instruction <<= if_stage.io().instruction;
    }
};

TEST_CASE("IfStageWrapper: construct only", "[if][1]") {
    context ctx("if1");
    ctx_swap swap(&ctx);
    ch::ch_device<IfStageWrapper> top;
    SUCCEED("OK");
}

TEST_CASE("IfStageWrapper: construct + tick", "[if][2]") {
    context ctx("if2");
    ctx_swap swap(&ctx);
    ch::ch_device<IfStageWrapper> top;
    Simulator sim(top.context());
    sim.set_input_value(top.instance().io().instr_data, 0x00000013);
    sim.set_input_value(top.instance().io().instr_ready, 1);
    sim.tick();
    SUCCEED("OK");
}
