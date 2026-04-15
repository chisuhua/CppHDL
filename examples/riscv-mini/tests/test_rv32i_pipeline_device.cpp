/**
 * @file test_rv32i_pipeline_device.cpp
 * @brief Test ch_device<Rv32iPipeline> directly (no wrapper)
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("ch_device<Rv32iPipeline>: construct only", "[direct][1]") {
    context ctx("direct1");
    ctx_swap swap(&ctx);
    ch::ch_device<Rv32iPipeline> pipeline;
    SUCCEED("OK");
}

TEST_CASE("ch_device<Rv32iPipeline>: construct + tick", "[direct][2]") {
    context ctx("direct2");
    ctx_swap swap(&ctx);
    ch::ch_device<Rv32iPipeline> pipeline;
    Simulator sim(pipeline.context());
    sim.set_input_value(pipeline.instance().io().instr_data, 0x00000013);
    sim.set_input_value(pipeline.instance().io().instr_ready, 1);
    sim.set_input_value(pipeline.instance().io().data_read_data, 0);
    sim.set_input_value(pipeline.instance().io().data_ready, 1);
    sim.tick();
    SUCCEED("OK");
}
