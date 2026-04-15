#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"
#include "../../include/cpu/pipeline/rv32i_tcm.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("Pipeline + TCM instantiation in single context", "[pipeline][smoke]") {
    context ctx("smoke_test");
    ctx_swap swap(&ctx);
    
    ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
    ch::ch_module<InstrTCM<20, 32>> itcm{"itcm"};
    ch::ch_module<DataTCM<20, 32>> dtcm{"dtcm"};
    
    itcm.io().addr <<= pipeline.io().instr_addr;
    pipeline.io().instr_data <<= itcm.io().data;
    pipeline.io().instr_ready <<= itcm.io().ready;
    
    dtcm.io().addr <<= pipeline.io().data_addr;
    dtcm.io().wdata <<= pipeline.io().data_write_data;
    dtcm.io().write <<= pipeline.io().data_write_en;
    dtcm.io().valid <<= ch_bool(true);
    pipeline.io().data_read_data <<= dtcm.io().rdata;
    pipeline.io().data_ready <<= dtcm.io().ready;
    
    pipeline.io().rst <<= ch_bool(true);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("Pipeline + TCM instantiated and ticked without crash");
}
