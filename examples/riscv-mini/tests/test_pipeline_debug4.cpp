/**
 * @file test_pipeline_debug4.cpp
 * @brief Incrementally add HazardUnit connections to find crash source
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/hazard_unit.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("HazardUnit: 1 input - rs1_data_raw", "[inc][1]") {
    context ctx("h1");
    ctx_swap swap(&ctx);
    ch::ch_module<HazardUnit> hazard{"hazard"};
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 2 inputs - rs1_data_raw + rs2_data_raw", "[inc][2]") {
    context ctx("h2");
    ctx_swap swap(&ctx);
    ch::ch_module<HazardUnit> hazard{"hazard"};
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 3 inputs - + ex_alu_result", "[inc][3]") {
    context ctx("h3");
    ctx_swap swap(&ctx);
    ch::ch_module<HazardUnit> hazard{"hazard"};
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
    hazard.io().ex_alu_result <<= ch_uint<32>(0_d);
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 4 inputs - + mem_alu_result", "[inc][4]") {
    context ctx("h4");
    ctx_swap swap(&ctx);
    ch::ch_module<HazardUnit> hazard{"hazard"};
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
    hazard.io().ex_alu_result <<= ch_uint<32>(0_d);
    hazard.io().mem_alu_result <<= ch_uint<32>(0_d);
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 5 inputs - + wb_write_data", "[inc][5]") {
    context ctx("h5");
    ctx_swap swap(&ctx);
    ch::ch_module<HazardUnit> hazard{"hazard"};
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
    hazard.io().ex_alu_result <<= ch_uint<32>(0_d);
    hazard.io().mem_alu_result <<= ch_uint<32>(0_d);
    hazard.io().wb_write_data <<= ch_uint<32>(0_d);
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 6 inputs - + id_rs1_addr", "[inc][6]") {
    context ctx("h6");
    ctx_swap swap(&ctx);
    ch::ch_module<HazardUnit> hazard{"hazard"};
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
    hazard.io().ex_alu_result <<= ch_uint<32>(0_d);
    hazard.io().mem_alu_result <<= ch_uint<32>(0_d);
    hazard.io().wb_write_data <<= ch_uint<32>(0_d);
    hazard.io().id_rs1_addr <<= ch_uint<5>(0_d);
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 7 inputs - + id_rs2_addr", "[inc][7]") {
    context ctx("h7");
    ctx_swap swap(&ctx);
    ch::ch_module<HazardUnit> hazard{"hazard"};
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
    hazard.io().ex_alu_result <<= ch_uint<32>(0_d);
    hazard.io().mem_alu_result <<= ch_uint<32>(0_d);
    hazard.io().wb_write_data <<= ch_uint<32>(0_d);
    hazard.io().id_rs1_addr <<= ch_uint<5>(0_d);
    hazard.io().id_rs2_addr <<= ch_uint<5>(0_d);
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 8 inputs - + ex_rd_addr", "[inc][8]") {
    context ctx("h8");
    ctx_swap swap(&ctx);
    ch::ch_module<HazardUnit> hazard{"hazard"};
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
    hazard.io().ex_alu_result <<= ch_uint<32>(0_d);
    hazard.io().mem_alu_result <<= ch_uint<32>(0_d);
    hazard.io().wb_write_data <<= ch_uint<32>(0_d);
    hazard.io().id_rs1_addr <<= ch_uint<5>(0_d);
    hazard.io().id_rs2_addr <<= ch_uint<5>(0_d);
    hazard.io().ex_rd_addr <<= ch_uint<5>(0_d);
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 9 inputs - + mem_rd_addr", "[inc][9]") {
    context ctx("h9");
    ctx_swap swap(&ctx);
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
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 10 inputs - + wb_rd_addr", "[inc][10]") {
    context ctx("h10");
    ctx_swap swap(&ctx);
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
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 11 inputs - + ex_reg_write", "[inc][11]") {
    context ctx("h11");
    ctx_swap swap(&ctx);
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
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 12 inputs - + mem_reg_write", "[inc][12]") {
    context ctx("h12");
    ctx_swap swap(&ctx);
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
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 13 inputs - + wb_reg_write", "[inc][13]") {
    context ctx("h13");
    ctx_swap swap(&ctx);
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
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 14 inputs - + mem_is_load", "[inc][14]") {
    context ctx("h14");
    ctx_swap swap(&ctx);
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
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 15 inputs - + ex_branch", "[inc][15]") {
    context ctx("h15");
    ctx_swap swap(&ctx);
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
    Simulator sim(&ctx);
    sim.tick();
    SUCCEED("OK");
}

TEST_CASE("HazardUnit: 16 inputs - + ex_branch_taken", "[inc][16]") {
    context ctx("h16");
    ctx_swap swap(&ctx);
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
