#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/hazard_unit.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("HazardUnit instantiation only", "[hazard][minimal]") {
    context ctx("hazard_only");
    ctx_swap swap(&ctx);
    
    ch::ch_module<HazardUnit> hazard{"hazard"};
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("HazardUnit instantiated and ticked");
}
