/**
 * @file test_pipeline_integration.cpp
 * @brief RISC-V 5 级流水线集成测试（编译验证）
 * 
 * 当前任务：验证流水线寄存器和新 IO 模式可以编译
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */

#include "catch_amalgamated.hpp"
#include "../examples/riscv-mini/src/hazard_unit.h"
#include "../examples/riscv-mini/src/rv32i_pipeline_regs.h"

using namespace riscv;

TEST_CASE("Pipeline Register Compilation", "[pipeline][regs]") {
    // 验证流水线寄存器类型可以编译
    // 注意：这些是编译时验证，不运行仿真
    
    SECTION("IfIdPipelineReg Type Check") {
        // IfIdPipelineReg 类型存在并可实例化
        IfIdPipelineReg reg{nullptr, "if_id_reg"};
        REQUIRE(true);
    }
    
    SECTION("IdExPipelineReg Type Check") {
        IdExPipelineReg reg{nullptr, "id_ex_reg"};
        REQUIRE(true);
    }
    
    SECTION("ExMemPipelineReg Type Check") {
        ExMemPipelineReg reg{nullptr, "ex_mem_reg"};
        REQUIRE(true);
    }
    
    SECTION("MemWbPipelineReg Type Check") {
        MemWbPipelineReg reg{nullptr, "mem_wb_reg"};
        REQUIRE(true);
    }
}

TEST_CASE("Hazard Unit Type Check", "[hazard][type]") {
    // 验证 HazardUnit 类型可以编译
    // 注意：不实例化为 ch_module，只验证类型定义
    
    SECTION("HazardUnit Type Exists") {
        // HazardUnit 类型存在
        HazardUnit hazard{nullptr, "hazard_unit"};
        REQUIRE(true);
    }
}
