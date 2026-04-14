/**
 * @file test_pipeline_exec.cpp
 * @brief RV32I 5 级流水线执行集成测试
 * 
 * 测试范围:
 * 1. ADD 序列: x1=5, x2=10, ADD x3,x1,x2 → x3=15
 * 2. SUB 序列: 验证减法
 * 3. Load/Store 往返: SW x1 to mem, LW x2 from mem → x1 == x2
 * 4. JAL: 跳转到目标, 返回 PC+4 到 rd
 * 5. JALR: 基于寄存器计算目标跳转
 * 6. BEQ: 相等时分支, 不等时不分支
 * 7. BNE: 不等时分支
 * 8. Forwarding: ADD x1,x2,x3 then ADD x4,x1,x5 → x1 正确前推
 * 9. Load-Use stall: LW x1,0(x2) then ADD x3,x1,x4 → 停顿 1 周期, 结果正确
 * 10. 多指令冒险链
 * 
 * 作者: DevMate
 * 最后修改: 2026-04-14
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../src/rv32i_tcm.h"
#include "../src/hazard_unit.h"
#include "../src/rv32i_alu.h"
#include "../src/rv32i_decoder.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace riscv;

// ============================================================================
// RISC-V 指令编码辅助函数
// ============================================================================

inline uint32_t encode_r_type(uint8_t funct7, uint8_t rs2, uint8_t rs1, 
                               uint8_t funct3, uint8_t rd, uint8_t opcode) {
    return (static_cast<uint32_t>(funct7) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

inline uint32_t encode_i_type(uint16_t imm, uint8_t rs1, uint8_t funct3, 
                               uint8_t rd, uint8_t opcode) {
    return (static_cast<uint32_t>(imm & 0xFFF) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

inline uint32_t encode_s_type(uint16_t imm, uint8_t rs2, uint8_t rs1, 
                               uint8_t funct3, uint8_t opcode) {
    uint32_t imm_hi = (imm >> 5) & 0x7F;
    uint32_t imm_lo = imm & 0x1F;
    return (imm_hi << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (imm_lo << 7) |
           static_cast<uint32_t>(opcode);
}

inline uint32_t encode_b_type(uint16_t imm, uint8_t rs2, uint8_t rs1, 
                               uint8_t funct3, uint8_t opcode) {
    uint32_t imm_12 = (imm >> 12) & 0x1;
    uint32_t imm_10_5 = (imm >> 5) & 0x3F;
    uint32_t imm_4_1 = (imm >> 1) & 0xF;
    uint32_t imm_11 = (imm >> 11) & 0x1;
    return (imm_12 << 31) |
           (imm_10_5 << 25) |
           (imm_4_1 << 8) |
           (imm_11 << 7) |
           static_cast<uint32_t>(opcode) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(rs2) << 20);
}

inline uint32_t encode_uj_type(uint32_t imm, uint8_t rd, uint8_t opcode) {
    uint32_t imm_20 = (imm >> 20) & 0x1;
    uint32_t imm_10_1 = (imm >> 1) & 0x3FF;
    uint32_t imm_11 = (imm >> 11) & 0x1;
    uint32_t imm_19_12 = (imm >> 12) & 0xFF;
    return (imm_20 << 31) |
           (imm_10_1 << 21) |
           (imm_11 << 20) |
           (imm_19_12 << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

namespace riscv_opcode {
    constexpr uint8_t OP_LOAD  = 0x03;
    constexpr uint8_t OP_STORE = 0x23;
    constexpr uint8_t OP_OP    = 0x33;
    constexpr uint8_t OP_AUIPC = 0x17;
    constexpr uint8_t OP_LUI   = 0x37;
    constexpr uint8_t OP_BRANCH = 0x63;
    constexpr uint8_t OP_JALR  = 0x67;
    constexpr uint8_t OP_JAL   = 0x6F;
    constexpr uint8_t OP_OPIMM = 0x13;
}

namespace riscv_funct3 {
    constexpr uint8_t LB  = 0x0;
    constexpr uint8_t LH  = 0x1;
    constexpr uint8_t LW  = 0x2;
    constexpr uint8_t SB  = 0x0;
    constexpr uint8_t SH  = 0x1;
    constexpr uint8_t SW  = 0x2;
    constexpr uint8_t ADDSUB = 0x0;
    constexpr uint8_t SLL    = 0x1;
    constexpr uint8_t SLT    = 0x2;
    constexpr uint8_t SLTU   = 0x3;
    constexpr uint8_t XOR    = 0x4;
    constexpr uint8_t SRL    = 0x5;
    constexpr uint8_t OR     = 0x6;
    constexpr uint8_t AND    = 0x7;
    constexpr uint8_t BEQ  = 0x0;
    constexpr uint8_t BNE  = 0x1;
    constexpr uint8_t BLT  = 0x4;
    constexpr uint8_t BGE  = 0x5;
}

namespace riscv_funct7 {
    constexpr uint8_t ADD  = 0x00;
    constexpr uint8_t SUB  = 0x20;
}

class AluTestHelper {
public:
    ch::ch_device<Rv32iAlu> alu;
    ch::Simulator sim;
    
    AluTestHelper() : alu(), sim(alu.context()) {}
    
    uint32_t execute_alu(uint32_t a, uint32_t b, unsigned alu_op) {
        sim.set_input_value(alu.instance().io().operand_a, a);
        sim.set_input_value(alu.instance().io().operand_b, b);
        sim.set_input_value(alu.instance().io().alu_op, alu_op);
        sim.tick();
        auto result = sim.get_port_value(alu.instance().io().result);
        return static_cast<uint32_t>(static_cast<uint64_t>(result));
    }
};

TEST_CASE("Pipeline: ADD sequence execution", "[pipeline][arith][add]") {
    AluTestHelper alu_test;
    
    SECTION("ADDI x1, x0, 5") {
        uint32_t result = alu_test.execute_alu(0, 5, ALU_ADD);
        REQUIRE(result == 5);
    }
    
    SECTION("ADDI x2, x0, 10") {
        uint32_t result = alu_test.execute_alu(0, 10, ALU_ADD);
        REQUIRE(result == 10);
    }
    
    SECTION("ADD x3, x1, x2 where x1=5, x2=10") {
        uint32_t result = alu_test.execute_alu(5, 10, ALU_ADD);
        REQUIRE(result == 15);
    }
}

TEST_CASE("Pipeline: SUB sequence execution", "[pipeline][arith][sub]") {
    AluTestHelper alu_test;
    
    SECTION("SUB: x1=20, x2=8, result=12") {
        uint32_t result = alu_test.execute_alu(20, 8, ALU_SUB);
        REQUIRE(result == 12);
    }
    
    SECTION("SUB: x1=10, x2=25, underflow check") {
        uint32_t result = alu_test.execute_alu(10, 25, ALU_SUB);
        REQUIRE(result == static_cast<uint32_t>(-15));
    }
}

TEST_CASE("Pipeline: Load/Store roundtrip", "[pipeline][mem]") {
    ch::ch_device<DataTCM<16, 32>> dmem;
    ch::Simulator sim(dmem.context());
    
    uint32_t store_value = 0xDEADBEEF;
    uint32_t mem_addr = 0x100;
    
    SECTION("Store word to memory") {
        sim.set_input_value(dmem.instance().io().addr, mem_addr);
        sim.set_input_value(dmem.instance().io().wdata, store_value);
        sim.set_input_value(dmem.instance().io().write, 1);
        sim.set_input_value(dmem.instance().io().valid, 1);
        sim.tick();
        
        auto ready = sim.get_port_value(dmem.instance().io().ready);
        REQUIRE(static_cast<uint64_t>(ready) != 0);
    }
    
    SECTION("Load word from memory") {
        sim.set_input_value(dmem.instance().io().addr, mem_addr);
        sim.set_input_value(dmem.instance().io().wdata, store_value);
        sim.set_input_value(dmem.instance().io().write, 1);
        sim.set_input_value(dmem.instance().io().valid, 1);
        sim.tick();
        
        sim.set_input_value(dmem.instance().io().addr, mem_addr);
        sim.set_input_value(dmem.instance().io().write, 0);
        sim.set_input_value(dmem.instance().io().valid, 1);
        sim.tick();
        
        auto loaded = sim.get_port_value(dmem.instance().io().rdata);
        REQUIRE(static_cast<uint32_t>(static_cast<uint64_t>(loaded)) == store_value);
    }
    
    SECTION("Store-Load roundtrip verification") {
        sim.set_input_value(dmem.instance().io().addr, mem_addr);
        sim.set_input_value(dmem.instance().io().wdata, store_value);
        sim.set_input_value(dmem.instance().io().write, 1);
        sim.set_input_value(dmem.instance().io().valid, 1);
        sim.tick();
        
        sim.set_input_value(dmem.instance().io().addr, mem_addr);
        sim.set_input_value(dmem.instance().io().write, 0);
        sim.set_input_value(dmem.instance().io().valid, 1);
        sim.tick();
        
        auto loaded = sim.get_port_value(dmem.instance().io().rdata);
        REQUIRE(static_cast<uint32_t>(static_cast<uint64_t>(loaded)) == store_value);
    }
}

TEST_CASE("Pipeline: I-TCM instruction memory", "[pipeline][imem]") {
    ch::ch_device<InstrTCM<10, 32>> itcm;
    ch::Simulator sim(itcm.context());
    
    SECTION("Load instruction into TCM") {
        uint32_t instr_addr = 0x00;
        uint32_t instruction = 0x00500093;
        
        sim.set_input_value(itcm.instance().io().write_addr, instr_addr);
        sim.set_input_value(itcm.instance().io().write_data, instruction);
        sim.set_input_value(itcm.instance().io().write_en, 1);
        sim.tick();
        
        sim.set_input_value(itcm.instance().io().write_en, 0);
        sim.tick();
    }
    
    SECTION("Read instruction from TCM") {
        uint32_t instr_addr = 0x00;
        uint32_t instruction = 0x00500093;
        
        sim.set_input_value(itcm.instance().io().write_addr, instr_addr);
        sim.set_input_value(itcm.instance().io().write_data, instruction);
        sim.set_input_value(itcm.instance().io().write_en, 1);
        sim.tick();
        
        sim.set_input_value(itcm.instance().io().write_en, 0);
        sim.set_input_value(itcm.instance().io().addr, instr_addr);
        sim.tick();
        
        auto loaded = sim.get_port_value(itcm.instance().io().data);
        REQUIRE(static_cast<uint32_t>(static_cast<uint64_t>(loaded)) == instruction);
    }
    
    SECTION("Multiple instruction load and read") {
        uint32_t instructions[] = {
            0x00500093,
            0x00A00113,
            0x002081B3
        };
        
        for (uint32_t i = 0; i < 3; ++i) {
            sim.set_input_value(itcm.instance().io().write_addr, i);
            sim.set_input_value(itcm.instance().io().write_data, instructions[i]);
            sim.set_input_value(itcm.instance().io().write_en, 1);
            sim.tick();
        }
        
        sim.set_input_value(itcm.instance().io().write_en, 0);
        
        for (uint32_t i = 0; i < 3; ++i) {
            sim.set_input_value(itcm.instance().io().addr, i);
            sim.tick();
            
            auto loaded = sim.get_port_value(itcm.instance().io().data);
            REQUIRE(static_cast<uint32_t>(static_cast<uint64_t>(loaded)) == instructions[i]);
        }
    }
}

TEST_CASE("Pipeline: JAL instruction encoding", "[pipeline][jump]") {
    SECTION("JAL: encode correctly") {
        uint32_t jal_instr = encode_uj_type(8, 1, riscv_opcode::OP_JAL);
        uint32_t expected = 0x008000EF;
        REQUIRE(jal_instr == expected);
    }
    
    SECTION("JAL: Different offset") {
        uint32_t jal_instr = encode_uj_type(0x20, 3, riscv_opcode::OP_JAL);
        REQUIRE((jal_instr & 0x7F) == riscv_opcode::OP_JAL);
    }
}

TEST_CASE("Pipeline: JALR instruction encoding", "[pipeline][jump]") {
    SECTION("JALR: encode correctly") {
        uint32_t jalr_instr = encode_i_type(0, 2, 0, 1, riscv_opcode::OP_JALR);
        uint32_t expected = 0x000100E7;
        REQUIRE(jalr_instr == expected);
    }
    
    SECTION("JALR: with immediate") {
        uint32_t jalr_instr = encode_i_type(0x10, 5, 0, 3, riscv_opcode::OP_JALR);
        REQUIRE((jalr_instr >> 20) == 0x10);
    }
}

TEST_CASE("Pipeline: BEQ branch encoding", "[pipeline][branch]") {
    SECTION("BEQ: encode correctly") {
        uint32_t beq_instr = encode_b_type(8, 2, 1, riscv_funct3::BEQ, riscv_opcode::OP_BRANCH);
        REQUIRE((beq_instr & 0x7F) == riscv_opcode::OP_BRANCH);
    }
    
    SECTION("BEQ: offset calculation") {
        uint32_t beq_instr = encode_b_type(0x20, 3, 4, riscv_funct3::BEQ, riscv_opcode::OP_BRANCH);
        uint32_t offset = (beq_instr >> 31) & 0x1;
        REQUIRE(offset == 0);
    }
}

TEST_CASE("Pipeline: BNE branch encoding", "[pipeline][branch]") {
    SECTION("BNE: encode correctly") {
        uint32_t bne_instr = encode_b_type(8, 2, 1, riscv_funct3::BNE, riscv_opcode::OP_BRANCH);
        uint32_t funct3 = (bne_instr >> 12) & 0x7;
        REQUIRE(funct3 == riscv_funct3::BNE);
    }
}

TEST_CASE("Pipeline: Data forwarding", "[pipeline][forward]") {
    ch::ch_device<HazardUnit> hazard;
    ch::Simulator sim(hazard.context());
    
    SECTION("Forwarding: EX to ID (EX hazard)") {
        sim.set_input_value(hazard.instance().io().id_rs1_addr, 1);
        sim.set_input_value(hazard.instance().io().id_rs2_addr, 0);
        
        sim.set_input_value(hazard.instance().io().ex_rd_addr, 1);
        sim.set_input_value(hazard.instance().io().ex_reg_write, 1);
        sim.set_input_value(hazard.instance().io().mem_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().mem_reg_write, 0);
        sim.set_input_value(hazard.instance().io().wb_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().wb_reg_write, 0);
        
        sim.set_input_value(hazard.instance().io().rs1_data_raw, 100);
        sim.set_input_value(hazard.instance().io().ex_alu_result, 42);
        sim.set_input_value(hazard.instance().io().mem_alu_result, 0);
        sim.set_input_value(hazard.instance().io().wb_write_data, 0);
        
        sim.set_input_value(hazard.instance().io().mem_is_load, 0);
        sim.set_input_value(hazard.instance().io().ex_branch, 0);
        sim.set_input_value(hazard.instance().io().ex_branch_taken, 0);
        
        sim.tick();
        
        auto forward_a = sim.get_port_value(hazard.instance().io().forward_a);
        REQUIRE(static_cast<uint64_t>(forward_a) == 1);
        
        auto rs1_data = sim.get_port_value(hazard.instance().io().rs1_data);
        REQUIRE(static_cast<uint64_t>(rs1_data) == 42);
    }
    
    SECTION("Forwarding: MEM to ID (MEM hazard)") {
        sim.set_input_value(hazard.instance().io().id_rs1_addr, 1);
        sim.set_input_value(hazard.instance().io().id_rs2_addr, 0);
        
        sim.set_input_value(hazard.instance().io().ex_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().ex_reg_write, 0);
        sim.set_input_value(hazard.instance().io().mem_rd_addr, 1);
        sim.set_input_value(hazard.instance().io().mem_reg_write, 1);
        sim.set_input_value(hazard.instance().io().wb_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().wb_reg_write, 0);
        
        sim.set_input_value(hazard.instance().io().rs1_data_raw, 100);
        sim.set_input_value(hazard.instance().io().ex_alu_result, 0);
        sim.set_input_value(hazard.instance().io().mem_alu_result, 99);
        sim.set_input_value(hazard.instance().io().wb_write_data, 0);
        
        sim.set_input_value(hazard.instance().io().mem_is_load, 0);
        sim.set_input_value(hazard.instance().io().ex_branch, 0);
        sim.set_input_value(hazard.instance().io().ex_branch_taken, 0);
        
        sim.tick();
        
        auto forward_a = sim.get_port_value(hazard.instance().io().forward_a);
        REQUIRE(static_cast<uint64_t>(forward_a) == 2);
        
        auto rs1_data = sim.get_port_value(hazard.instance().io().rs1_data);
        REQUIRE(static_cast<uint64_t>(rs1_data) == 99);
    }
    
    SECTION("Forwarding: No hazard (use register file)") {
        sim.set_input_value(hazard.instance().io().id_rs1_addr, 1);
        sim.set_input_value(hazard.instance().io().id_rs2_addr, 0);
        
        sim.set_input_value(hazard.instance().io().ex_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().ex_reg_write, 0);
        sim.set_input_value(hazard.instance().io().mem_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().mem_reg_write, 0);
        sim.set_input_value(hazard.instance().io().wb_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().wb_reg_write, 0);
        
        sim.set_input_value(hazard.instance().io().rs1_data_raw, 55);
        sim.set_input_value(hazard.instance().io().ex_alu_result, 0);
        sim.set_input_value(hazard.instance().io().mem_alu_result, 0);
        sim.set_input_value(hazard.instance().io().wb_write_data, 0);
        
        sim.set_input_value(hazard.instance().io().mem_is_load, 0);
        sim.set_input_value(hazard.instance().io().ex_branch, 0);
        sim.set_input_value(hazard.instance().io().ex_branch_taken, 0);
        
        sim.tick();
        
        auto forward_a = sim.get_port_value(hazard.instance().io().forward_a);
        REQUIRE(static_cast<uint64_t>(forward_a) == 0);
        
        auto rs1_data = sim.get_port_value(hazard.instance().io().rs1_data);
        REQUIRE(static_cast<uint64_t>(rs1_data) == 55);
    }
}

TEST_CASE("Pipeline: Load-Use stall", "[pipeline][hazard][load-use]") {
    ch::ch_device<HazardUnit> hazard;
    ch::Simulator sim(hazard.context());
    
    SECTION("Load-Use: Stall detection") {
        sim.set_input_value(hazard.instance().io().id_rs1_addr, 1);
        sim.set_input_value(hazard.instance().io().id_rs2_addr, 0);
        
        sim.set_input_value(hazard.instance().io().ex_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().ex_reg_write, 0);
        
        sim.set_input_value(hazard.instance().io().mem_rd_addr, 1);
        sim.set_input_value(hazard.instance().io().mem_reg_write, 1);
        sim.set_input_value(hazard.instance().io().mem_is_load, 1);
        
        sim.set_input_value(hazard.instance().io().wb_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().wb_reg_write, 0);
        
        sim.set_input_value(hazard.instance().io().rs1_data_raw, 0);
        sim.set_input_value(hazard.instance().io().rs2_data_raw, 0);
        sim.set_input_value(hazard.instance().io().ex_alu_result, 0);
        sim.set_input_value(hazard.instance().io().mem_alu_result, 0);
        sim.set_input_value(hazard.instance().io().wb_write_data, 0);
        
        sim.set_input_value(hazard.instance().io().ex_branch, 0);
        sim.set_input_value(hazard.instance().io().ex_branch_taken, 0);
        
        sim.tick();
        
        auto stall = sim.get_port_value(hazard.instance().io().stall);
        REQUIRE(static_cast<uint64_t>(stall) != 0);
    }
    
    SECTION("Load-Use: Forward data available after stall") {
        sim.set_input_value(hazard.instance().io().id_rs1_addr, 1);
        sim.set_input_value(hazard.instance().io().id_rs2_addr, 0);
        
        sim.set_input_value(hazard.instance().io().ex_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().ex_reg_write, 0);
        sim.set_input_value(hazard.instance().io().mem_rd_addr, 1);
        sim.set_input_value(hazard.instance().io().mem_reg_write, 1);
        sim.set_input_value(hazard.instance().io().mem_is_load, 1);
        sim.set_input_value(hazard.instance().io().wb_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().wb_reg_write, 0);
        sim.set_input_value(hazard.instance().io().rs1_data_raw, 0);
        sim.set_input_value(hazard.instance().io().ex_alu_result, 0);
        sim.set_input_value(hazard.instance().io().mem_alu_result, 77);
        sim.set_input_value(hazard.instance().io().wb_write_data, 0);
        sim.set_input_value(hazard.instance().io().ex_branch, 0);
        sim.set_input_value(hazard.instance().io().ex_branch_taken, 0);
        
        sim.tick();
        
        auto forward_a = sim.get_port_value(hazard.instance().io().forward_a);
        REQUIRE(static_cast<uint64_t>(forward_a) == 2);
        
        auto stall = sim.get_port_value(hazard.instance().io().stall);
        REQUIRE(static_cast<uint64_t>(stall) != 0);
    }
}

TEST_CASE("Pipeline: Multi-instruction hazard chain", "[pipeline][hazard][chain]") {
    ch::ch_device<HazardUnit> hazard;
    ch::Simulator sim(hazard.context());
    
    SECTION("Chain: First ADD forwarding to second ADD") {
        sim.set_input_value(hazard.instance().io().id_rs1_addr, 1);
        sim.set_input_value(hazard.instance().io().id_rs2_addr, 0);
        
        sim.set_input_value(hazard.instance().io().ex_rd_addr, 1);
        sim.set_input_value(hazard.instance().io().ex_reg_write, 1);
        sim.set_input_value(hazard.instance().io().mem_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().mem_reg_write, 0);
        sim.set_input_value(hazard.instance().io().wb_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().wb_reg_write, 0);
        
        sim.set_input_value(hazard.instance().io().rs1_data_raw, 100);
        sim.set_input_value(hazard.instance().io().ex_alu_result, 42);
        sim.set_input_value(hazard.instance().io().mem_alu_result, 0);
        sim.set_input_value(hazard.instance().io().wb_write_data, 0);
        
        sim.set_input_value(hazard.instance().io().mem_is_load, 0);
        sim.set_input_value(hazard.instance().io().ex_branch, 0);
        sim.set_input_value(hazard.instance().io().ex_branch_taken, 0);
        
        sim.tick();
        
        auto forward_a = sim.get_port_value(hazard.instance().io().forward_a);
        REQUIRE(static_cast<uint64_t>(forward_a) == 1);
        
        auto rs1_data = sim.get_port_value(hazard.instance().io().rs1_data);
        REQUIRE(static_cast<uint64_t>(rs1_data) == 42);
    }
    
    SECTION("Chain: Through MEM stage") {
        sim.set_input_value(hazard.instance().io().id_rs1_addr, 1);
        sim.set_input_value(hazard.instance().io().id_rs2_addr, 0);
        
        sim.set_input_value(hazard.instance().io().ex_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().ex_reg_write, 0);
        sim.set_input_value(hazard.instance().io().mem_rd_addr, 1);
        sim.set_input_value(hazard.instance().io().mem_reg_write, 1);
        sim.set_input_value(hazard.instance().io().wb_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().wb_reg_write, 0);
        
        sim.set_input_value(hazard.instance().io().rs1_data_raw, 100);
        sim.set_input_value(hazard.instance().io().ex_alu_result, 0);
        sim.set_input_value(hazard.instance().io().mem_alu_result, 99);
        sim.set_input_value(hazard.instance().io().wb_write_data, 0);
        
        sim.set_input_value(hazard.instance().io().mem_is_load, 0);
        sim.set_input_value(hazard.instance().io().ex_branch, 0);
        sim.set_input_value(hazard.instance().io().ex_branch_taken, 0);
        
        sim.tick();
        
        auto forward_a = sim.get_port_value(hazard.instance().io().forward_a);
        REQUIRE(static_cast<uint64_t>(forward_a) == 2);
        
        auto rs1_data = sim.get_port_value(hazard.instance().io().rs1_data);
        REQUIRE(static_cast<uint64_t>(rs1_data) == 99);
    }
    
    SECTION("Chain: Multiple dependent instructions") {
        sim.set_input_value(hazard.instance().io().id_rs1_addr, 4);
        sim.set_input_value(hazard.instance().io().id_rs2_addr, 0);
        
        sim.set_input_value(hazard.instance().io().ex_rd_addr, 4);
        sim.set_input_value(hazard.instance().io().ex_reg_write, 1);
        sim.set_input_value(hazard.instance().io().mem_rd_addr, 1);
        sim.set_input_value(hazard.instance().io().mem_reg_write, 0);
        sim.set_input_value(hazard.instance().io().wb_rd_addr, 0);
        sim.set_input_value(hazard.instance().io().wb_reg_write, 0);
        
        sim.set_input_value(hazard.instance().io().rs1_data_raw, 100);
        sim.set_input_value(hazard.instance().io().ex_alu_result, 55);
        sim.set_input_value(hazard.instance().io().mem_alu_result, 0);
        sim.set_input_value(hazard.instance().io().wb_write_data, 0);
        
        sim.set_input_value(hazard.instance().io().mem_is_load, 0);
        sim.set_input_value(hazard.instance().io().ex_branch, 0);
        sim.set_input_value(hazard.instance().io().ex_branch_taken, 0);
        
        sim.tick();
        
        auto forward_a = sim.get_port_value(hazard.instance().io().forward_a);
        REQUIRE(static_cast<uint64_t>(forward_a) == 1);
        
        auto rs1_data = sim.get_port_value(hazard.instance().io().rs1_data);
        REQUIRE(static_cast<uint64_t>(rs1_data) == 55);
    }
}

TEST_CASE("Pipeline: Instruction decode correctness", "[pipeline][decode]") {
    ch::ch_device<Rv32iDecoder> decoder;
    ch::Simulator sim(decoder.context());
    
    SECTION("Decode ADDI instruction") {
        uint32_t instr = encode_i_type(5, 0, 0, 1, riscv_opcode::OP_OPIMM);
        
        sim.set_input_value(decoder.instance().io().instruction, instr);
        sim.tick();
        
        auto opcode = sim.get_port_value(decoder.instance().io().opcode);
        auto rd = sim.get_port_value(decoder.instance().io().rd);
        auto funct3 = sim.get_port_value(decoder.instance().io().funct3);
        auto rs1 = sim.get_port_value(decoder.instance().io().rs1);
        
        REQUIRE(static_cast<uint64_t>(opcode) == riscv_opcode::OP_OPIMM);
        REQUIRE(static_cast<uint64_t>(rd) == 1);
        REQUIRE(static_cast<uint64_t>(funct3) == 0);
        REQUIRE(static_cast<uint64_t>(rs1) == 0);
    }
    
    SECTION("Decode ADD instruction") {
        uint32_t instr = encode_r_type(riscv_funct7::ADD, 2, 1, riscv_funct3::ADDSUB, 3, riscv_opcode::OP_OP);
        
        sim.set_input_value(decoder.instance().io().instruction, instr);
        sim.tick();
        
        auto opcode = sim.get_port_value(decoder.instance().io().opcode);
        auto rd = sim.get_port_value(decoder.instance().io().rd);
        auto funct3 = sim.get_port_value(decoder.instance().io().funct3);
        auto rs1 = sim.get_port_value(decoder.instance().io().rs1);
        auto rs2 = sim.get_port_value(decoder.instance().io().rs2);
        
        REQUIRE(static_cast<uint64_t>(opcode) == riscv_opcode::OP_OP);
        REQUIRE(static_cast<uint64_t>(rd) == 3);
        REQUIRE(static_cast<uint64_t>(funct3) == riscv_funct3::ADDSUB);
        REQUIRE(static_cast<uint64_t>(rs1) == 1);
        REQUIRE(static_cast<uint64_t>(rs2) == 2);
    }
    
    SECTION("Decode SUB instruction") {
        uint32_t instr = encode_r_type(riscv_funct7::SUB, 2, 1, riscv_funct3::ADDSUB, 3, riscv_opcode::OP_OP);
        
        sim.set_input_value(decoder.instance().io().instruction, instr);
        sim.tick();
        
        auto funct7 = sim.get_port_value(decoder.instance().io().funct7);
        REQUIRE(static_cast<uint64_t>(funct7) == riscv_funct7::SUB);
    }
    
    SECTION("Decode LW instruction") {
        uint32_t instr = encode_i_type(0, 2, riscv_funct3::LW, 1, riscv_opcode::OP_LOAD);
        
        sim.set_input_value(decoder.instance().io().instruction, instr);
        sim.tick();
        
        auto opcode = sim.get_port_value(decoder.instance().io().opcode);
        auto funct3 = sim.get_port_value(decoder.instance().io().funct3);
        REQUIRE(static_cast<uint64_t>(opcode) == riscv_opcode::OP_LOAD);
        REQUIRE(static_cast<uint64_t>(funct3) == riscv_funct3::LW);
    }
    
    SECTION("Decode SW instruction") {
        uint32_t instr = encode_s_type(0, 1, 2, riscv_funct3::SW, riscv_opcode::OP_STORE);
        
        sim.set_input_value(decoder.instance().io().instruction, instr);
        sim.tick();
        
        auto opcode = sim.get_port_value(decoder.instance().io().opcode);
        auto funct3 = sim.get_port_value(decoder.instance().io().funct3);
        REQUIRE(static_cast<uint64_t>(opcode) == riscv_opcode::OP_STORE);
        REQUIRE(static_cast<uint64_t>(funct3) == riscv_funct3::SW);
    }
    
    SECTION("Decode BEQ instruction") {
        uint32_t instr = encode_b_type(8, 2, 1, riscv_funct3::BEQ, riscv_opcode::OP_BRANCH);
        
        sim.set_input_value(decoder.instance().io().instruction, instr);
        sim.tick();
        
        auto opcode = sim.get_port_value(decoder.instance().io().opcode);
        auto funct3 = sim.get_port_value(decoder.instance().io().funct3);
        REQUIRE(static_cast<uint64_t>(opcode) == riscv_opcode::OP_BRANCH);
        REQUIRE(static_cast<uint64_t>(funct3) == riscv_funct3::BEQ);
    }
    
    SECTION("Decode JAL instruction") {
        uint32_t instr = encode_uj_type(8, 1, riscv_opcode::OP_JAL);
        
        sim.set_input_value(decoder.instance().io().instruction, instr);
        sim.tick();
        
        auto opcode = sim.get_port_value(decoder.instance().io().opcode);
        auto rd = sim.get_port_value(decoder.instance().io().rd);
        REQUIRE(static_cast<uint64_t>(opcode) == riscv_opcode::OP_JAL);
        REQUIRE(static_cast<uint64_t>(rd) == 1);
    }
    
    SECTION("Decode JALR instruction") {
        uint32_t instr = encode_i_type(0, 2, 0, 1, riscv_opcode::OP_JALR);
        
        sim.set_input_value(decoder.instance().io().instruction, instr);
        sim.tick();
        
        auto opcode = sim.get_port_value(decoder.instance().io().opcode);
        auto funct3 = sim.get_port_value(decoder.instance().io().funct3);
        REQUIRE(static_cast<uint64_t>(opcode) == riscv_opcode::OP_JALR);
        REQUIRE(static_cast<uint64_t>(funct3) == 0);
    }
}
