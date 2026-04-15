/**
 * @file test_pipeline_debug3.cpp
 * @brief Absolute minimal - just HazardUnit instantiation
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/hazard_unit.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("ABSOLUTE MINIMAL: HazardUnit instance only", "[abs-min]") {
    context ctx("abs_min");
    ctx_swap swap(&ctx);
    
    ch::ch_module<HazardUnit> hazard{"hazard"};
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("OK");
}
