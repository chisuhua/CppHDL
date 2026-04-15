/**
 * @file test_pipeline_debug2.cpp
 * @brief Minimal reproduction of "Child component has been destroyed" crash
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/hazard_unit.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("MINIMAL: HazardUnit with zero connections", "[minimal]") {
    context ctx("min_hazard");
    ctx_swap swap(&ctx);
    
    // Just instantiate - no connections
    ch::ch_module<HazardUnit> hazard{"hazard"};
    
    // Minimal connections - all inputs grounded
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
    hazard.io().ex_alu_result <<= ch_uint<32>(0_d);
    hazard.io().mem_alu_result <<= ch_uint<32>(0_d);
    hazard.io().wb_write_data <<= ch_uint<32>(0_d);
    hazard.io().id_rs1_addr <<= ch_uint<5>(0_d);
    hazard.io().id_rs2_addr <<= ch_uint<5>(0_d);
    hazard.io().ex_rd_addr <<= ch_uint<5>(0_d);
    hazard.io().mem_rd_addr <<= ch_uint<5>(0_d);
    hazard.io().wb_rd_addr <<= ch_uint<5>(0_d);
    hazard.io().ex_reg_write <<= ch_bool(false);
    hazard.io().mem_reg_write <<= ch_bool(false);
    hazard.io().wb_reg_write <<= ch_bool(false);
    hazard.io().mem_is_load <<= ch_bool(false);
    hazard.io().ex_branch <<= ch_bool(false);
    hazard.io().ex_branch_taken <<= ch_bool(false);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("OK");
}
