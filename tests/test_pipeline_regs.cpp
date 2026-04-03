/**
 * @file test_pipeline_regs.cpp
 * @brief RV32I 流水线寄存器编译测试
 * 
 * 验证流水线寄存器定义可以正确编译，无错误无警告
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "riscv/rv32i_pipeline_regs.h"

using namespace riscv;

TEST_CASE("Pipeline Register Structures", "[pipeline][regs]") {
    SECTION("IfIdRegs compiles and initializes") {
        IfIdRegs if_id;
        // 验证结构体可以实例化
        (void)if_id.pc;
        (void)if_id.instruction;
        (void)if_id.instr_valid;
    }
    
    SECTION("IdExRegs compiles and initializes") {
        IdExRegs id_ex;
        // 验证所有字段可以访问
        (void)id_ex.reg_write;
        (void)id_ex.alu_src;
        (void)id_ex.branch;
        (void)id_ex.jump;
        (void)id_ex.is_load;
        (void)id_ex.is_store;
        (void)id_ex.alu_op;
        (void)id_ex.rs1_data;
        (void)id_ex.rs2_data;
        (void)id_ex.imm;
        (void)id_ex.rd;
        (void)id_ex.pc;
        (void)id_ex.funct3;
    }
    
    SECTION("ExMemRegs compiles and initializes") {
        ExMemRegs ex_mem;
        // 验证所有字段可以访问
        (void)ex_mem.alu_result;
        (void)ex_mem.write_data;
        (void)ex_mem.reg_write;
        (void)ex_mem.is_load;
        (void)ex_mem.is_store;
        (void)ex_mem.jump;
        (void)ex_mem.alu_op;
        (void)ex_mem.rd;
        (void)ex_mem.pc;
    }
    
    SECTION("MemWbRegs compiles and initializes") {
        MemWbRegs mem_wb;
        // 验证所有字段可以访问
        (void)mem_wb.mem_read_data;
        (void)mem_wb.alu_result;
        (void)mem_wb.reg_write;
        (void)mem_wb.is_load;
        (void)mem_wb.rd;
    }
}

TEST_CASE("Pipeline Register Components", "[pipeline][component]") {
    SECTION("IfIdPipelineReg compiles") {
        ch::Component* parent = nullptr;
        IfIdPipelineReg reg(parent, "test_if_id");
        (void)reg;
    }
    
    SECTION("IdExPipelineReg compiles") {
        ch::Component* parent = nullptr;
        IdExPipelineReg reg(parent, "test_id_ex");
        (void)reg;
    }
    
    SECTION("ExMemPipelineReg compiles") {
        ch::Component* parent = nullptr;
        ExMemPipelineReg reg(parent, "test_ex_mem");
        (void)reg;
    }
    
    SECTION("MemWbPipelineReg compiles") {
        ch::Component* parent = nullptr;
        MemWbPipelineReg reg(parent, "test_mem_wb");
        (void)reg;
    }
}
