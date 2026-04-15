/**
 * @file test_pipeline_debug5.cpp
 * @brief Test with parent component wrapper
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "component.h"
#include "../../include/cpu/pipeline/hazard_unit.h"

using namespace ch::core;
using namespace riscv;

class TestTop : public ch::Component {
public:
    TestTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override {}
    void describe() override {}
};

TEST_CASE("HazardUnit with parent component - 1 input", "[with-parent][1]") {
    context ctx("wp1");
    ctx_swap swap(&ctx);
    
    ch::ch_device<TestTop> top;
    ch::ch_module<HazardUnit> hazard{"hazard"};
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit with parent component - 16 inputs", "[with-parent][16]") {
    context ctx("wp16");
    ctx_swap swap(&ctx);
    
    ch::ch_device<TestTop> top;
    ch::ch_module<HazardUnit> hazard{"hazard"};
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
