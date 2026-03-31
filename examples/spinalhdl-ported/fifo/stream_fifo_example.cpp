/**
 * Stream FIFO Example - SpinalHDL Port
 * 
 * 参考官方 samples/fifo_example.cpp 的实现方式
 */

#include "chlib/fifo.h"
#include "ch.hpp"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// ============================================================================
// Stream FIFO 模块（使用官方 FIFO 实现方式）
// ============================================================================

template <typename T, unsigned N>
class FiFo : public ch::Component {
public:
    static constexpr unsigned addr_width = N <= 1 ? 0 : (N & (N-1)) ? 0 : __builtin_ctz(N);
    
    __io(
        ch_in<T> din;
        ch_in<ch_bool> push;
        ch_in<ch_bool> pop;
        ch_out<T> dout;
        ch_out<ch_bool> empty;
        ch_out<ch_bool> full;
    )
    
    FiFo(ch::Component* parent = nullptr, const std::string& name = "fifo")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch_reg<ch_uint<addr_width + 1>> rd_ptr(0_d, "rd_ptr");
        ch_reg<ch_uint<addr_width + 1>> wr_ptr(0_d, "wr_ptr");
        
        ch_uint<addr_width> rd_a = bits<addr_width - 1, 0>(rd_ptr);
        ch_uint<addr_width> wr_a = bits<addr_width - 1, 0>(wr_ptr);
        
        auto reading = io().pop;
        auto writing = io().push;
        
        rd_ptr->next = select(reading, rd_ptr + 1_b, rd_ptr);
        wr_ptr->next = select(writing, wr_ptr + 1_b, wr_ptr);
        
        ch_mem<T, N> mem("fifo_mem");
        mem.write(wr_a, io().din, writing);
        
        auto data_out = mem.aread(rd_a);
        io().dout = data_out;
        
        io().empty = (rd_ptr == wr_ptr);
        auto wr_plus_one = wr_ptr + 1_b;
        io().full = (wr_plus_one == rd_ptr);
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class Top : public ch::Component {
public:
    __io(
        ch_out<ch_uint<8>> dout;
        ch_out<ch_bool> empty;
        ch_out<ch_bool> full;
        ch_in<ch_uint<8>> din;
        ch_in<ch_bool> push;
        ch_in<ch_bool> pop;
    )
    
    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<FiFo<ch_uint<8>, 16>> fifo_inst{"fifo_inst"};
        
        fifo_inst.io().din <<= io().din;
        fifo_inst.io().push <<= io().push;
        fifo_inst.io().pop <<= io().pop;
        io().dout <<= fifo_inst.io().dout;
        io().empty <<= fifo_inst.io().empty;
        io().full <<= fifo_inst.io().full;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - Stream FIFO Example" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    ch::ch_device<Top> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化输入
    sim.set_input_value(top_device.instance().io().din, 0);
    sim.set_input_value(top_device.instance().io().push, false);
    sim.set_input_value(top_device.instance().io().pop, false);
    
    std::cout << "\nStarting FIFO simulation..." << std::endl;
    std::cout << "FIFO size: 16, addr_width: 4, pointer width: 5" << std::endl;
    
    for (int cycle = 0; cycle <= 20; ++cycle) {
        sim.tick();
        
        auto dout_val = sim.get_port_value(top_device.instance().io().dout);
        auto empty_val = sim.get_port_value(top_device.instance().io().empty);
        auto full_val = sim.get_port_value(top_device.instance().io().full);
        auto push_val = sim.get_port_value(top_device.instance().io().push);
        auto pop_val = sim.get_port_value(top_device.instance().io().pop);
        
        std::cout << "Cycle " << cycle << ": dout=0x" << std::hex
                  << static_cast<uint64_t>(dout_val)
                  << ", empty=" << static_cast<uint64_t>(empty_val)
                  << ", full=" << static_cast<uint64_t>(full_val)
                  << ", push=" << static_cast<uint64_t>(push_val)
                  << ", pop=" << static_cast<uint64_t>(pop_val)
                  << std::dec << std::endl;
        
        // 测试激励
        if (cycle == 0) {
            // 初始状态检查
            if (static_cast<uint64_t>(empty_val) != 1) {
                std::cerr << "ERROR: Initial state incorrect!" << std::endl;
                return 1;
            }
            // 写入数据 0xAA
            sim.set_input_value(top_device.instance().io().din, 0xAA);
            sim.set_input_value(top_device.instance().io().push, true);
        } else if (cycle == 1) {
            // 继续写入
            sim.set_input_value(top_device.instance().io().din, 0xBB);
            sim.set_input_value(top_device.instance().io().push, true);
        } else if (cycle == 2) {
            // 停止写入，开始读取
            sim.set_input_value(top_device.instance().io().push, false);
            sim.set_input_value(top_device.instance().io().pop, true);
        } else if (cycle >= 3 && cycle <= 4) {
            // 继续读取
            sim.set_input_value(top_device.instance().io().pop, true);
        } else {
            // 停止读取
            sim.set_input_value(top_device.instance().io().pop, false);
        }
    }
    
    // 生成 Verilog
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("stream_fifo.v", top_device.context());
    std::cout << "Verilog generated: stream_fifo.v" << std::endl;
    
    std::cout << "\nFIFO simulation completed successfully!" << std::endl;
    return 0;
}
