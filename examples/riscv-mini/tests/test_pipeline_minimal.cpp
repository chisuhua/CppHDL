#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("Pipeline instantiation only", "[pipeline][minimal]") {
    context ctx("pipeline_only");
    ctx_swap swap(&ctx);
    
    ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
    pipeline.io().rst <<= ch_bool(true);
    pipeline.io().instr_data <<= ch_uint<32>(0_d);
    pipeline.io().instr_ready <<= ch_bool(true);
    pipeline.io().data_read_data <<= ch_uint<32>(0_d);
    pipeline.io().data_ready <<= ch_bool(true);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("Pipeline instantiated and ticked");
}
