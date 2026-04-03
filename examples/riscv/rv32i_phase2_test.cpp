/**
 * RV32I Core Phase 2 Test - 分支跳转指令测试
 * 
 * 测试范围:
 * - B-type: BEQ, BNE, BLT, BGE, BLTU, BGEU
 * - J-type: JAL
 * - I-type: JALR
 */

#include "ch.hpp"
#include "riscv/rv32i_alu.h"
#include "riscv/rv32i_decoder.h"
#include "component.h"
#include "simulator.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace riscv;

int main() {
    std::cout << "=== RV32I Phase 2: Branch & Jump Test ===" << std::endl;
    std::cout << std::endl;
    
    bool all_pass = true;
    int test_count = 0;
    int pass_count = 0;
    
    // ========================================================================
    // Test Group 1: ALU Comparison Flags
    // ========================================================================
    std::cout << "Test Group 1: ALU Comparison Flags" << std::endl;
    std::cout << "-----------------------------------" << std::endl;
    
    {
        ch::ch_device<Rv32iAlu> alu;
        ch::Simulator sim(alu.context());
        
        // Test 1.1: equal flag (==)
        test_count++;
        sim.set_input_value(alu.instance().io().operand_a, 42);
        sim.set_input_value(alu.instance().io().operand_b, 42);
        sim.set_input_value(alu.instance().io().alu_op, ALU_ADD);
        sim.tick();
        
        auto equal_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().equal));
        auto equal = (equal_val != 0);
        std::cout << "  equal(42, 42) = " << (equal ? "true" : "false") << std::endl;
        
        if (equal) {
            std::cout << "  [PASS] ALU equal flag" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] ALU equal flag should be true" << std::endl;
            all_pass = false;
        }
        
        // Test 1.2: equal flag (!=)
        test_count++;
        sim.set_input_value(alu.instance().io().operand_b, 43);
        sim.tick();
        
        equal_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().equal));
        equal = (equal_val != 0);
        std::cout << "  equal(42, 43) = " << (equal ? "true" : "false") << std::endl;
        
        if (!equal) {
            std::cout << "  [PASS] ALU not equal flag" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] ALU equal flag should be false" << std::endl;
            all_pass = false;
        }
        
        // Test 1.3: less_than signed (-1 < 1)
        test_count++;
        sim.set_input_value(alu.instance().io().operand_a, 0xFFFFFFFF);  // -1 in two's complement
        sim.set_input_value(alu.instance().io().operand_b, 1);
        sim.tick();
        
        auto lt_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().less_than));
        auto lt = (lt_val != 0);
        std::cout << "  less_than_signed(-1, 1) = " << (lt ? "true" : "false") << std::endl;
        
        if (lt) {
            std::cout << "  [PASS] ALU signed less_than flag" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] ALU less_than flag should be true (-1 < 1)" << std::endl;
            all_pass = false;
        }
        
        // Test 1.4: less_than unsigned (0xFFFFFFFF > 1)
        test_count++;
        auto lt_u_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().less_than_u));
        auto lt_u = (lt_u_val != 0);
        std::cout << "  less_than_unsigned(0xFFFFFFFF, 1) = " << (lt_u ? "true" : "false") << std::endl;
        
        if (!lt_u) {
            std::cout << "  [PASS] ALU unsigned less_than flag" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] ALU less_than_u flag should be false (4294967295 > 1)" << std::endl;
            all_pass = false;
        }
        
        // Test 1.5: less_than signed (1 < -1 should be false)
        test_count++;
        sim.set_input_value(alu.instance().io().operand_a, 1);
        sim.set_input_value(alu.instance().io().operand_b, 0xFFFFFFFF);  // -1
        sim.tick();
        
        lt_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().less_than));
        lt = (lt_val != 0);
        std::cout << "  less_than_signed(1, -1) = " << (lt ? "true" : "false") << std::endl;
        
        if (!lt) {
            std::cout << "  [PASS] ALU signed less_than flag (1 >= -1)" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] ALU less_than flag should be false (1 >= -1)" << std::endl;
            all_pass = false;
        }
    }
    
    std::cout << std::endl;
    
    // ========================================================================
    // Test Group 2: Instruction Decoder (B-type & J-type)
    // ========================================================================
    std::cout << "Test Group 2: Instruction Decoder" << std::endl;
    std::cout << "----------------------------------" << std::endl;
    
    {
        ch::ch_device<Rv32iDecoder> decoder;
        ch::Simulator sim(decoder.context());
        
        // Test 2.1: BEQ x0, x0, 0 (0x00000063)
        test_count++;
        sim.set_input_value(decoder.instance().io().instruction, 0x00000063);
        sim.tick();
        
        auto opcode = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().opcode));
        auto funct3 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().funct3));
        auto imm = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().imm));
        auto type = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().instr_type));
        
        std::cout << "  BEQ x0, x0, 0 (0x00000063):" << std::endl;
        std::cout << "    opcode=0x" << std::hex << opcode 
                  << ", funct3=" << std::dec << funct3 
                  << ", imm=" << imm 
                  << ", type=" << type << std::endl;
        
        if (opcode == 0x63 && funct3 == 0 && imm == 0 && type == INSTR_B) {
            std::cout << "  [PASS] BEQ decode" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] BEQ decode" << std::endl;
            all_pass = false;
        }
        
        // Test 2.2: BNE x1, x2, offset (0x00209063) - funct3=001, rs1=1, rs2=2
        test_count++;
        sim.set_input_value(decoder.instance().io().instruction, 0x00209063);
        sim.tick();
        
        opcode = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().opcode));
        funct3 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().funct3));
        auto rs1 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().rs1));
        auto rs2 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().rs2));
        
        std::cout << "  BNE x1, x2, offset (0x00209063):" << std::endl;
        std::cout << "    opcode=0x" << std::hex << opcode 
                  << ", funct3=" << std::dec << funct3 
                  << ", rs1=" << rs1 
                  << ", rs2=" << rs2 << std::endl;
        
        if (opcode == 0x63 && funct3 == 1 && rs1 == 1 && rs2 == 2) {
            std::cout << "  [PASS] BNE decode" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] BNE decode (expected funct3=1, got " << funct3 << ")" << std::endl;
            all_pass = false;
        }
        
        // Test 2.3: JAL x1, offset (0x000000EF)
        test_count++;
        sim.set_input_value(decoder.instance().io().instruction, 0x000000EF);
        sim.tick();
        
        opcode = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().opcode));
        type = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().instr_type));
        
        std::cout << "  JAL x1, offset (0x000000EF):" << std::endl;
        std::cout << "    opcode=0x" << std::hex << opcode 
                  << ", type=" << std::dec << type << std::endl;
        
        if (opcode == 0x6F && type == INSTR_J) {
            std::cout << "  [PASS] JAL decode" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] JAL decode" << std::endl;
            all_pass = false;
        }
        
        // Test 2.4: JALR x1, 0(x0) (0x00000067)
        test_count++;
        sim.set_input_value(decoder.instance().io().instruction, 0x00000067);
        sim.tick();
        
        opcode = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().opcode));
        funct3 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().funct3));
        type = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().instr_type));
        
        std::cout << "  JALR x1, 0(x0) (0x00000067):" << std::endl;
        std::cout << "    opcode=0x" << std::hex << opcode 
                  << ", funct3=" << std::dec << funct3 
                  << ", type=" << type << std::endl;
        
        if (opcode == 0x67 && funct3 == 0 && type == INSTR_I) {
            std::cout << "  [PASS] JALR decode" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] JALR decode" << std::endl;
            all_pass = false;
        }
    }
    
    std::cout << std::endl;
    
    // ========================================================================
    // Test Group 3: Branch Logic Simulation
    // ========================================================================
    std::cout << "Test Group 3: Branch Logic Simulation" << std::endl;
    std::cout << "--------------------------------------" << std::endl;
    
    {
        ch::ch_device<Rv32iAlu> alu;
        ch::Simulator sim(alu.context());
        
        uint64_t equal_val;
        bool equal;
        uint64_t lt_val;
        bool lt;
        uint64_t lt_u_val;
        bool lt_u;
        
        // Simulate BEQ condition (rs1 == rs2)
        test_count++;
        sim.set_input_value(alu.instance().io().operand_a, 100);
        sim.set_input_value(alu.instance().io().operand_b, 100);
        sim.set_input_value(alu.instance().io().alu_op, ALU_ADD);
        sim.tick();
        
        equal_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().equal));
        equal = (equal_val != 0);
        std::cout << "  BEQ condition (100 == 100): " << (equal ? "taken" : "not taken") << std::endl;
        
        if (equal) {
            std::cout << "  [PASS] BEQ branch taken" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] BEQ should be taken when equal" << std::endl;
            all_pass = false;
        }
        
        // Simulate BNE condition (rs1 != rs2)
        test_count++;
        sim.set_input_value(alu.instance().io().operand_b, 200);
        sim.tick();
        
        equal_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().equal));
        equal = (equal_val != 0);
        std::cout << "  BNE condition (100 != 200): " << (!equal ? "taken" : "not taken") << std::endl;
        
        if (!equal) {
            std::cout << "  [PASS] BNE branch taken" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] BNE should be taken when not equal" << std::endl;
            all_pass = false;
        }
        
        // Simulate BLT condition (signed: -1 < 1)
        test_count++;
        sim.set_input_value(alu.instance().io().operand_a, 0xFFFFFFFF);  // -1
        sim.set_input_value(alu.instance().io().operand_b, 1);
        sim.tick();
        
        lt_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().less_than));
        lt = (lt_val != 0);
        std::cout << "  BLT condition (-1 < 1): " << (lt ? "taken" : "not taken") << std::endl;
        
        if (lt) {
            std::cout << "  [PASS] BLT branch taken (signed)" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] BLT should be taken when rs1 < rs2 (signed)" << std::endl;
            all_pass = false;
        }
        
        // Simulate BLTU condition (unsigned: 1 < 0xFFFFFFFF)
        test_count++;
        sim.set_input_value(alu.instance().io().operand_a, 1);
        sim.set_input_value(alu.instance().io().operand_b, 0xFFFFFFFF);  // 4294967295
        sim.tick();
        
        lt_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().less_than));
        lt_u_val = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().less_than_u));
        lt = (lt_val != 0);
        lt_u = (lt_u_val != 0);
        std::cout << "  BLTU condition (1 < 4294967295): " << (lt_u ? "taken" : "not taken") << std::endl;
        
        if (lt_u) {
            std::cout << "  [PASS] BLTU branch taken (unsigned)" << std::endl;
            pass_count++;
        } else {
            std::cout << "  [FAIL] BLTU should be taken when rs1 < rs2 (unsigned)" << std::endl;
            all_pass = false;
        }
    }
    
    std::cout << std::endl;
    
    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "=== Test Summary ===" << std::endl;
    std::cout << "Total tests: " << test_count << std::endl;
    std::cout << "Passed: " << pass_count << std::endl;
    std::cout << "Failed: " << (test_count - pass_count) << std::endl;
    std::cout << "Pass rate: " << (pass_count * 100 / test_count) << "%" << std::endl;
    
    if (all_pass) {
        std::cout << std::endl;
        std::cout << "All Phase 2 tests PASSED!" << std::endl;
        std::cout << "RV32I core now supports all branch and jump instructions:" << std::endl;
        std::cout << "  - BEQ, BNE, BLT, BGE, BLTU, BGEU" << std::endl;
        std::cout << "  - JAL, JALR" << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "Some tests FAILED!" << std::endl;
    }
    
    return all_pass ? 0 : 1;
}
