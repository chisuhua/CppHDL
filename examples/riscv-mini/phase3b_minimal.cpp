/**
 * Phase 3B Minimal Test
 * 
 * 测试 RV32I 核心简化版
 */

#include "ch.hpp"
#include "src/rv32i_core_simple.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace riscv;

int main() {
    std::cout << "=== CppHDL Phase 3B: RV32I Minimal Core Test ===" << std::endl;
    
    // 创建 RV32I 核心
    ch::ch_device<Rv32iCore> cpu;
    ch::Simulator sim(cpu.context());
    
    std::cout << "Creating RV32I core... OK" << std::endl;
    
    // 初始化输入
    sim.set_input_value(cpu.instance().io().clk, false);
    sim.set_input_value(cpu.instance().io().rst, true);
    sim.set_input_value(cpu.instance().io().ifetch_data, 0);
    sim.set_input_value(cpu.instance().io().ifetch_ready, true);
    sim.set_input_value(cpu.instance().io().dmem_rdata, 0);
    sim.set_input_value(cpu.instance().io().dmem_ready, true);
    
    // 复位
    std::cout << "Resetting CPU..." << std::endl;
    for (int i = 0; i < 5; i++) {
        sim.set_input_value(cpu.instance().io().clk, !sim.get_port_value(cpu.instance().io().clk));
        sim.tick();
    }
    
    // 释放复位
    sim.set_input_value(cpu.instance().io().rst, false);
    std::cout << "Releasing reset..." << std::endl;
    
    // 运行几个周期
    for (int cycle = 0; cycle < 20; cycle++) {
        // 时钟上升沿
        sim.set_input_value(cpu.instance().io().clk, true);
        
        // 模拟指令存储器响应
        if (sim.get_port_value(cpu.instance().io().ifetch_valid)) {
            uint64_t addr = static_cast<uint64_t>(sim.get_port_value(cpu.instance().io().ifetch_addr));
            // 简单测试指令：addi x1, x0, 1 (0x00100093)
            sim.set_input_value(cpu.instance().io().ifetch_data, 0x00100093);
            sim.set_input_value(cpu.instance().io().ifetch_ready, true);
            std::cout << "  Cycle " << cycle << ": Fetching from 0x" << std::hex << addr << std::dec << std::endl;
        } else {
            sim.set_input_value(cpu.instance().io().ifetch_ready, false);
        }
        
        // 模拟数据存储器响应
        if (sim.get_port_value(cpu.instance().io().dmem_read)) {
            sim.set_input_value(cpu.instance().io().dmem_ready, true);
            std::cout << "  Cycle " << cycle << ": Memory READ" << std::endl;
        } else if (sim.get_port_value(cpu.instance().io().dmem_write)) {
            sim.set_input_value(cpu.instance().io().dmem_ready, true);
            uint64_t addr = static_cast<uint64_t>(sim.get_port_value(cpu.instance().io().dmem_addr));
            uint64_t wdata = static_cast<uint64_t>(sim.get_port_value(cpu.instance().io().dmem_wdata));
            std::cout << "  Cycle " << cycle << ": Memory WRITE to 0x" << std::hex << addr << ", data=0x" << wdata << std::dec << std::endl;
        } else {
            sim.set_input_value(cpu.instance().io().dmem_ready, false);
        }
        
        sim.tick();
        
        // 时钟下降沿
        sim.set_input_value(cpu.instance().io().clk, false);
        sim.tick();
        
        // 检查运行状态
        if (cycle == 0) {
            bool running = static_cast<bool>(sim.get_port_value(cpu.instance().io().running));
            std::cout << "  CPU running: " << (running ? "YES" : "NO") << std::endl;
        }
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    
    // 生成 Verilog
    std::cout << "\nGenerating Verilog..." << std::endl;
    ch::toVerilog(cpu, "phase3b_minimal_output");
    std::cout << "Verilog output written to: phase3b_minimal_output/" << std::endl;
    
    return 0;
}
