/**
 * @file test_pipeline_debug6.cpp
 * @brief Use ch_device instead of ch_module for HazardUnit
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/hazard_unit.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("HazardUnit via ch_device - 16 inputs", "[ch-device]") {
    ch::ch_device<HazardUnit> hazard;
    ch::Simulator sim(hazard.context());
    
    sim.set_input_value(hazard.instance().io().rs1_data_raw, 0);
    sim.set_input_value(hazard.instance().io().rs2_data_raw, 0);
    sim.set_input_value(hazard.instance().io().ex_alu_result, 0);
    sim.set_input_value(hazard.instance().io().mem_alu_result, 0);
    sim.set_input_value(hazard.instance().io().wb_write_data, 0);
    sim.set_input_value(hazard.instance().io().id_rs1_addr, 0);
    sim.set_input_value(hazard.instance().io().id_rs2_addr, 0);
    sim.set_input_value(hazard.instance().io().ex_rd_addr, 0);
    sim.set_input_value(hazard.instance().io().mem_rd_addr, 0);
    sim.set_input_value(hazard.instance().io().wb_rd_addr, 0);
    sim.set_input_value(hazard.instance().io().ex_reg_write, 0);
    sim.set_input_value(hazard.instance().io().mem_reg_write, 0);
    sim.set_input_value(hazard.instance().io().wb_reg_write, 0);
    sim.set_input_value(hazard.instance().io().mem_is_load, 0);
    sim.set_input_value(hazard.instance().io().ex_branch, 0);
    sim.set_input_value(hazard.instance().io().ex_branch_taken, 0);
    
    sim.tick();
    
    SUCCEED("OK");
}
