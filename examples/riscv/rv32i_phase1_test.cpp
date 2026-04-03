/**
 * RV32I Core Test - Phase 1
 * 
 * 测试寄存器文件、ALU、指令译码器
 */

#include "ch.hpp"
#include "riscv/rv32i_regfile.h"
#include "riscv/rv32i_alu.h"
#include "riscv/rv32i_decoder.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace riscv;

int main() {
    std::cout << "=== RV32I Core Phase 1 Test ===" << std::endl;
    std::cout << std::endl;
    
    bool all_pass = true;
    
    // ==================== Test 1: Register File ====================
    std::cout << "Test 1: Register File" << std::endl;
    std::cout << "---------------------" << std::endl;
    
    {
        ch::ch_device<Rv32iRegFile> regfile;
        ch::Simulator sim(regfile.context());
        
        // 写 x1 = 0x12345678
        sim.set_input_value(regfile.instance().io().rd_addr, 1);
        sim.set_input_value(regfile.instance().io().rd_data, 0x12345678);
        sim.set_input_value(regfile.instance().io().rd_we, true);
        sim.set_input_value(regfile.instance().io().clk, false);
        sim.set_input_value(regfile.instance().io().rst, true);
        sim.tick();
        
        // 释放复位
        sim.set_input_value(regfile.instance().io().rst, false);
        sim.tick();
        
        // 读 x1
        sim.set_input_value(regfile.instance().io().rs1_addr, 1);
        sim.set_input_value(regfile.instance().io().rd_we, false);
        sim.tick();
        
        auto rs1_data = static_cast<uint64_t>(sim.get_port_value(regfile.instance().io().rs1_data));
        std::cout << "  Read x1: 0x" << std::hex << rs1_data << std::dec << std::endl;
        
        if (rs1_data == 0x12345678) {
            std::cout << "  [PASS] Register file write/read" << std::endl;
        } else {
            std::cout << "  [FAIL] Expected 0x12345678, got 0x" << std::hex << rs1_data << std::dec << std::endl;
            all_pass = false;
        }
        
        // 测试 x0 (硬连线为 0)
        sim.set_input_value(regfile.instance().io().rs1_addr, 0);
        sim.tick();
        rs1_data = static_cast<uint64_t>(sim.get_port_value(regfile.instance().io().rs1_data));
        
        if (rs1_data == 0) {
            std::cout << "  [PASS] x0 hardwired to 0" << std::endl;
        } else {
            std::cout << "  [FAIL] x0 should be 0, got 0x" << std::hex << rs1_data << std::dec << std::endl;
            all_pass = false;
        }
    }
    
    std::cout << std::endl;
    
    // ==================== Test 2: ALU ====================
    std::cout << "Test 2: ALU" << std::endl;
    std::cout << "-----------" << std::endl;
    
    {
        ch::ch_device<Rv32iAlu> alu;
        ch::Simulator sim(alu.context());
        
        // 测试 ADD
        sim.set_input_value(alu.instance().io().operand_a, 10);
        sim.set_input_value(alu.instance().io().operand_b, 5);
        sim.set_input_value(alu.instance().io().alu_op, ALU_ADD);
        sim.tick();
        
        auto result = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().result));
        std::cout << "  ADD: 10 + 5 = " << result << std::endl;
        
        if (result == 15) {
            std::cout << "  [PASS] ADD" << std::endl;
        } else {
            std::cout << "  [FAIL] ADD expected 15, got " << result << std::endl;
            all_pass = false;
        }
        
        // 测试 SUB
        sim.set_input_value(alu.instance().io().alu_op, ALU_SUB);
        sim.tick();
        
        result = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().result));
        std::cout << "  SUB: 10 - 5 = " << result << std::endl;
        
        if (result == 5) {
            std::cout << "  [PASS] SUB" << std::endl;
        } else {
            std::cout << "  [FAIL] SUB expected 5, got " << result << std::endl;
            all_pass = false;
        }
        
        // 测试 AND
        sim.set_input_value(alu.instance().io().alu_op, ALU_AND);
        sim.tick();
        
        result = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().result));
        std::cout << "  AND: 10 & 5 = " << result << std::endl;
        
        if (result == 0) {  // 1010 & 0101 = 0000
            std::cout << "  [PASS] AND" << std::endl;
        } else {
            std::cout << "  [FAIL] AND expected 0, got " << result << std::endl;
            all_pass = false;
        }
        
        // 测试 OR
        sim.set_input_value(alu.instance().io().alu_op, ALU_OR);
        sim.tick();
        
        result = static_cast<uint64_t>(sim.get_port_value(alu.instance().io().result));
        std::cout << "  OR: 10 | 5 = " << result << std::endl;
        
        if (result == 15) {  // 1010 | 0101 = 1111
            std::cout << "  [PASS] OR" << std::endl;
        } else {
            std::cout << "  [FAIL] OR expected 15, got " << result << std::endl;
            all_pass = false;
        }
    }
    
    std::cout << std::endl;
    
    // ==================== Test 3: Instruction Decoder ====================
    std::cout << "Test 3: Instruction Decoder" << std::endl;
    std::cout << "---------------------------" << std::endl;
    
    {
        ch::ch_device<Rv32iDecoder> decoder;
        ch::Simulator sim(decoder.context());
        
        // 测试 ADDI x1, x0, 10 (0x00A00093)
        sim.set_input_value(decoder.instance().io().instruction, 0x00A00093);
        sim.tick();
        
        auto opcode = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().opcode));
        auto rd = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().rd));
        auto funct3 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().funct3));
        auto rs1 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().rs1));
        auto imm = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().imm));
        
        std::cout << "  ADDI x1, x0, 10 (0x00A00093):" << std::endl;
        std::cout << "    opcode=0x" << std::hex << opcode << std::dec 
                  << ", rd=" << rd 
                  << ", funct3=" << funct3 
                  << ", rs1=" << rs1 
                  << ", imm=" << imm << std::endl;
        
        if (opcode == 0x13 && rd == 1 && funct3 == 0 && rs1 == 0 && imm == 10) {
            std::cout << "  [PASS] ADDI decode" << std::endl;
        } else {
            std::cout << "  [FAIL] ADDI decode" << std::endl;
            all_pass = false;
        }
        
        // 测试 ADD x3, x1, x2 (0x002081B3)
        sim.set_input_value(decoder.instance().io().instruction, 0x002081B3);
        sim.tick();
        
        opcode = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().opcode));
        rd = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().rd));
        funct3 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().funct3));
        rs1 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().rs1));
        uint64_t rs2 = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().rs2));
        
        std::cout << "  ADD x3, x1, x2 (0x002081B3):" << std::endl;
        std::cout << "    opcode=0x" << std::hex << opcode << std::dec 
                  << ", rd=" << rd 
                  << ", funct3=" << funct3 
                  << ", rs1=" << rs1 
                  << ", rs2=" << rs2 << std::endl;
        
        if (opcode == 0x33 && rd == 3 && funct3 == 0 && rs1 == 1 && rs2 == 2) {
            std::cout << "  [PASS] ADD decode" << std::endl;
        } else {
            std::cout << "  [FAIL] ADD decode" << std::endl;
            all_pass = false;
        }
    }
    
    std::cout << std::endl;
    
    // ==================== Summary ====================
    std::cout << "=== Test Summary ===" << std::endl;
    if (all_pass) {
        std::cout << "All tests PASSED!" << std::endl;
    } else {
        std::cout << "Some tests FAILED!" << std::endl;
    }
    
    return all_pass ? 0 : 1;
}
