#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../src/rv32i_pipeline_top.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("PipelineTop with ITCM/DTCM: construct + tick", "[pipeline][smoke]") {
    ch::ch_device<Rv32iPipelineTop> top;

    auto& io = top.io();
    io.ext_rst <<= ch_bool(true);
    io.ext_clk <<= ch_bool(false);
    io.ext_instr_data <<= ch_uint<32>(0x00000013);
    io.ext_instr_ready <<= ch_bool(true);
    io.ext_data_read_data <<= ch_uint<32>(0);
    io.ext_data_ready <<= ch_bool(true);

    Simulator sim(top.context());
    sim.tick();

    SUCCEED("PipelineTop with ITCM/DTCM ticked without crash");
}
