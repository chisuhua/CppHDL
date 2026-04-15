/**
 * @file test_ifstage_device.cpp
 * @brief Test ch_device<IfStage> directly
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/stages/if_stage.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("ch_device<IfStage>: construct + tick", "[ifdev]") {
    context ctx("ifdev");
    ctx_swap swap(&ctx);
    ch::ch_device<IfStage> if_stage;
    Simulator sim(if_stage.context());
    sim.set_input_value(if_stage.instance().io().stall, 0);
    sim.set_input_value(if_stage.instance().io().flush, 0);
    sim.set_input_value(if_stage.instance().io().rst, 0);
    sim.set_input_value(if_stage.instance().io().branch_target, 0);
    sim.set_input_value(if_stage.instance().io().branch_valid, 0);
    sim.set_input_value(if_stage.instance().io().instr_data, 0x00000013);
    sim.set_input_value(if_stage.instance().io().instr_ready, 1);
    sim.tick();
    SUCCEED("OK");
}
