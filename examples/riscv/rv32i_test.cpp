/**
 * RV32I Core Test
 * 
 * 测试寄存器文件、ALU、指令译码器
 */

#include "ch.hpp"
#include "riscv/rv32i_regs.h"
#include "riscv/rv32i_alu.h"
#include "riscv/rv32i_decoder.h"
#include "riscv/rv32i_pc.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace riscv;

int main() {
    std::cout << "CppHDL - RV32I Core Components Test" << std::endl;
    std::cout << "====================================" << std::endl;
    
    // ==================== Test 1: Register File ====================
    std::cout << "\n=== Test 1: Register File ===" << std::endl;
    
    {
        ch::ch_device<RegFile> regfile;
        ch::Simulator sim(regfile.context());
        
        // 写寄存器 x1 = 0x12345678 (305419896)
        sim.set_input_value(regfile.instance().io().rd_addr, 1_d);
        sim.set_input_value(regfile.instance().io().rd_data, 305419896_d);
        sim.set_input_value(regfile.instance().io().rd_we, true);
        sim.tick();
        
        // 读寄存器 x1
        sim.set_input_value(regfile.instance().io().rs1_addr, 1_d);
        sim.set_input_value(regfile.instance().io().rd_we, false);
        sim.tick();
        
        auto rs1_data = sim.get_port_value(regfile.instance().io().rs1_data);
        std::cout << "Read x1: 0x" << std::hex << static_cast<uint64_t>(rs1_data) << std::dec << std::endl;
        
        if (static_cast<uint64_t>(rs1_data) == 305419896) {
            std::cout << "  [PASS] Register file write/read OK" << std::endl;
        } else {
            std::cout << "  [FAIL] Register file data mismatch" << std::endl;
        }
        
        // 测试 x0 (硬连线为 0)
        sim.set_input_value(regfile.instance().io().rs1_addr, 0_d);
        sim.tick();
        rs1_data = sim.get_port_value(regfile.instance().io().rs1_data);
        
        if (static_cast<uint64_t>(rs1_data) == 0) {
            std::cout << "  [PASS] x0 is hardwired to 0" << std::endl;
        } else {
            std::cout << "  [FAIL] x0 should be 0" << std::endl;
        }
    }
    
    // ==================== Test 2: ALU ====================
    std::cout << "\n=== Test 2: ALU ===" << std::endl;
    
    {
        ch::ch_device<ALU> alu;
        ch::Simulator sim(alu.context());
        
        // 测试 ADD
        sim.set_input_value(alu.instance().io().operand_a, 10_d);
        sim.set_input_value(alu.instance().io().operand_b, 5_d);
        sim.set_input_value(alu.instance().io().funct3, funct3::ADD);
        sim.set_input_value(alu.instance().io().is_sub, false);
        sim.tick();
        
        auto result = sim.get_port_value(alu.instance().io().result);
        std::cout << "ADD: 10 + 5 = " << static_cast<uint64_t>(result) << std::endl;
        
        if (static_cast<uint64_t>(result) == 15) {
            std::cout << "  [PASS] ADD OK" << std::endl;
        } else {
            std::cout << "  [FAIL] ADD failed" << std::endl;
        }
        
        // 测试 SUB
        sim.set_input_value(alu.instance().io().is_sub, true);
        sim.tick();
        
        result = sim.get_port_value(alu.instance().io().result);
        std::cout << "SUB: 10 - 5 = " << static_cast<uint64_t>(result) << std::endl;
        
        if (static_cast<uint64_t>(result) == 5) {
            std::cout << "  [PASS] SUB OK" << std::endl;
        } else {
            std::cout << "  [FAIL] SUB failed" << std::endl;
        }
        
        // 测试 AND
        sim.set_input_value(alu.instance().io().operand_a, 255_d);  // 0xFF
        sim.set_input_value(alu.instance().io().operand_b, 15_d);   // 0x0F
        sim.set_input_value(alu.instance().io().funct3, funct3::AND);
        sim.set_input_value(alu.instance().io().is_sub, false);
        sim.tick();
        
        result = sim.get_port_value(alu.instance().io().result);
        std::cout << "AND: 255 & 15 = " << static_cast<uint64_t>(result) << std::endl;
        
        if (static_cast<uint64_t>(result) == 15) {
            std::cout << "  [PASS] AND OK" << std::endl;
        } else {
            std::cout << "  [FAIL] AND failed" << std::endl;
        }
    }
    
    // ==================== Test 3: Program Counter ====================
    std::cout << "\n=== Test 3: Program Counter ===" << std::endl;
    
    {
        ch::ch_device<ProgramCounter> pc_unit;
        ch::Simulator sim(pc_unit.context());
        
        // 复位
        sim.set_input_value(pc_unit.instance().io().reset, true);
        sim.set_input_value(pc_unit.instance().io().stall, false);
        sim.tick();
        
        auto pc = sim.get_port_value(pc_unit.instance().io().pc);
        std::cout << "After reset: PC = 0x" << std::hex << static_cast<uint64_t>(pc) << std::dec << std::endl;
        
        // 顺序执行 (PC+4)
        sim.set_input_value(pc_unit.instance().io().reset, false);
        sim.set_input_value(pc_unit.instance().io().next_pc, 4_d);
        sim.set_input_value(pc_unit.instance().io().branch_taken, false);
        sim.set_input_value(pc_unit.instance().io().jump, false);
        sim.tick();
        
        pc = sim.get_port_value(pc_unit.instance().io().pc);
        std::cout << "After tick: PC = 0x" << std::hex << static_cast<uint64_t>(pc) << std::dec << std::endl;
        
        if (static_cast<uint64_t>(pc) == 4) {
            std::cout << "  [PASS] PC increment OK" << std::endl;
        } else {
            std::cout << "  [FAIL] PC increment failed" << std::endl;
        }
    }
    
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    
    // 生成 Verilog
    {
        ch::ch_device<RegFile> regfile;
        toVerilog("rv32i_regfile.v", regfile.context());
        std::cout << "Generated: rv32i_regfile.v" << std::endl;
    }
    
    {
        ch::ch_device<ALU> alu;
        toVerilog("rv32i_alu.v", alu.context());
        std::cout << "Generated: rv32i_alu.v" << std::endl;
    }
    
    {
        ch::ch_device<ProgramCounter> pc_unit;
        toVerilog("rv32i_pc.v", pc_unit.context());
        std::cout << "Generated: rv32i_pc.v" << std::endl;
    }
    
    std::cout << "\n✅ RV32I core components test completed!" << std::endl;
    return 0;
}
