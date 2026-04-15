/**
 * @file test_pipeline_debug.cpp
 * @brief Incremental pipeline debugging - isolate SIGSEGV source
 * 
 * Tests pipeline components one by one to find exact crash location.
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"
#include "../../include/cpu/pipeline/hazard_unit.h"
#include "../../include/cpu/pipeline/stages/if_stage.h"
#include "../../include/cpu/pipeline/stages/id_stage.h"
#include "../../include/cpu/pipeline/stages/ex_stage.h"
#include "../../include/cpu/pipeline/stages/mem_stage.h"
#include "../../include/cpu/pipeline/stages/wb_stage.h"

using namespace ch::core;
using namespace riscv;

// ===== Test 1: HazardUnit standalone =====
TEST_CASE("DEBUG 1: HazardUnit standalone", "[debug][1]") {
    context ctx("debug_hazard");
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
    
    SUCCEED("HazardUnit standalone OK");
}

// ===== Test 2: IfStage standalone =====
TEST_CASE("DEBUG 2: IfStage standalone", "[debug][2]") {
    context ctx("debug_if");
    ctx_swap swap(&ctx);
    
    ch::ch_module<IfStage> if_stage{"if_stage"};
    if_stage.io().stall <<= ch_bool(false);
    if_stage.io().flush <<= ch_bool(false);
    if_stage.io().rst <<= ch_bool(false);
    if_stage.io().branch_target <<= ch_uint<32>(0_d);
    if_stage.io().branch_valid <<= ch_bool(false);
    if_stage.io().instr_data <<= ch_uint<32>(0_d);
    if_stage.io().instr_ready <<= ch_bool(true);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("IfStage standalone OK");
}

// ===== Test 3: IdStage standalone =====
TEST_CASE("DEBUG 3: IdStage standalone", "[debug][3]") {
    context ctx("debug_id");
    ctx_swap swap(&ctx);
    
    ch::ch_module<IdStage> id_stage{"id_stage"};
    id_stage.io().pc <<= ch_uint<32>(0_d);
    id_stage.io().instruction <<= ch_uint<32>(0_d);
    id_stage.io().valid <<= ch_bool(true);
    id_stage.io().reg_rs1_data <<= ch_uint<32>(0_d);
    id_stage.io().reg_rs2_data <<= ch_uint<32>(0_d);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("IdStage standalone OK");
}

// ===== Test 4: ExStage standalone =====
TEST_CASE("DEBUG 4: ExStage standalone", "[debug][4]") {
    context ctx("debug_ex");
    ctx_swap swap(&ctx);
    
    ch::ch_module<ExStage> ex_stage{"ex_stage"};
    ex_stage.io().rs1_data <<= ch_uint<32>(0_d);
    ex_stage.io().rs2_data <<= ch_uint<32>(0_d);
    ex_stage.io().imm <<= ch_uint<32>(0_d);
    ex_stage.io().pc <<= ch_uint<32>(0_d);
    ex_stage.io().opcode <<= ch_uint<4>(0_d);
    ex_stage.io().funct3 <<= ch_uint<3>(0_d);
    ex_stage.io().funct7 <<= ch_uint<7>(0_d);
    ex_stage.io().alu_op <<= ch_uint<4>(0_d);
    ex_stage.io().is_branch <<= ch_bool(false);
    ex_stage.io().branch_type <<= ch_uint<3>(0_d);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("ExStage standalone OK");
}

// ===== Test 5: MemStage standalone =====
TEST_CASE("DEBUG 5: MemStage standalone", "[debug][5]") {
    context ctx("debug_mem");
    ctx_swap swap(&ctx);
    
    ch::ch_module<MemStage> mem_stage{"mem_stage"};
    mem_stage.io().alu_result <<= ch_uint<32>(0_d);
    mem_stage.io().rs2_data <<= ch_uint<32>(0_d);
    mem_stage.io().is_load <<= ch_bool(false);
    mem_stage.io().is_store <<= ch_bool(false);
    mem_stage.io().funct3 <<= ch_uint<3>(0_d);
    mem_stage.io().mem_valid <<= ch_bool(true);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("MemStage standalone OK");
}

// ===== Test 6: WbStage standalone =====
TEST_CASE("DEBUG 6: WbStage standalone", "[debug][6]") {
    context ctx("debug_wb");
    ctx_swap swap(&ctx);
    
    ch::ch_module<WbStage> wb_stage{"wb_stage"};
    wb_stage.io().alu_result <<= ch_uint<32>(0_d);
    wb_stage.io().mem_data <<= ch_uint<32>(0_d);
    wb_stage.io().rd_addr <<= ch_uint<5>(0_d);
    wb_stage.io().is_load <<= ch_bool(false);
    wb_stage.io().is_alu <<= ch_bool(false);
    wb_stage.io().is_jump <<= ch_bool(false);
    wb_stage.io().mem_valid <<= ch_bool(false);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("WbStage standalone OK");
}

// ===== Test 7: Pipeline - no external connections =====
TEST_CASE("DEBUG 7: Pipeline - inputs grounded", "[debug][7]") {
    context ctx("debug_pipeline_grounded");
    ctx_swap swap(&ctx);
    
    ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
    
    // Ground all external inputs
    pipeline.io().rst <<= ch_bool(true);
    pipeline.io().clk <<= ch_bool(true);
    pipeline.io().instr_data <<= ch_uint<32>(0_d);
    pipeline.io().instr_ready <<= ch_bool(true);
    pipeline.io().data_read_data <<= ch_uint<32>(0_d);
    pipeline.io().data_ready <<= ch_bool(true);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("Pipeline grounded OK");
}

// ===== Test 8: Pipeline - release reset =====
TEST_CASE("DEBUG 8: Pipeline - release reset", "[debug][8]") {
    context ctx("debug_pipeline_reset");
    ctx_swap swap(&ctx);
    
    ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
    pipeline.io().rst <<= ch_bool(false);
    pipeline.io().clk <<= ch_bool(true);
    pipeline.io().instr_data <<= ch_uint<32>(0_d);
    pipeline.io().instr_ready <<= ch_bool(true);
    pipeline.io().data_read_data <<= ch_uint<32>(0_d);
    pipeline.io().data_ready <<= ch_bool(true);
    
    Simulator sim(&ctx);
    sim.tick();
    
    SUCCEED("Pipeline reset release OK");
}

// ===== Test 9: Pipeline - multiple ticks =====
TEST_CASE("DEBUG 9: Pipeline - 10 ticks", "[debug][9]") {
    context ctx("debug_pipeline_ticks");
    ctx_swap swap(&ctx);
    
    ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
    pipeline.io().rst <<= ch_bool(false);
    pipeline.io().clk <<= ch_bool(true);
    pipeline.io().instr_data <<= ch_uint<32>(0_d);
    pipeline.io().instr_ready <<= ch_bool(true);
    pipeline.io().data_read_data <<= ch_uint<32>(0_d);
    pipeline.io().data_ready <<= ch_bool(true);
    
    Simulator sim(&ctx);
    for (int i = 0; i < 10; i++) {
        sim.tick();
    }
    
    SUCCEED("Pipeline 10 ticks OK");
}
