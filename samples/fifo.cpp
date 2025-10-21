// samples/fifo.cpp
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/codegen.h"
#include <iostream>

using namespace ch::core;

// 辅助函数
constexpr bool ispow2(unsigned n) {
    return n > 0 && (n & (n - 1)) == 0;
}

constexpr unsigned log2ceil(unsigned n) {
    if (n <= 1) return 0;
    unsigned result = 0;
    while (n > 1) {
        result++;
        n >>= 1;
    }
    return result;
}

template <typename T, unsigned N>
class FiFo : public ch::Component {
public:
    static_assert(ispow2(N), "FIFO size must be power of 2");
    static constexpr unsigned addr_width = log2ceil(N);
    
    __io(
        ch_in<T> din;
        ch_in<ch_bool> push;
        ch_in<ch_bool> pop;
        ch_out<T> dout;
        ch_out<ch_bool> empty;
        ch_out<ch_bool> full;
    )

    FiFo(ch::Component* parent = nullptr, const std::string& name = "fifo")
        : ch::Component(parent, name)
    {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // 创建读写指针寄存器（addr_width + 1 位用于满/空检测）
        ch_reg<ch_uint<addr_width + 1>> rd_ptr(0, "rd_ptr");
        ch_reg<ch_uint<addr_width + 1>> wr_ptr(0, "wr_ptr");

        // 提取地址位（低 addr_width 位）
        auto rd_a = bits<decltype(rd_ptr), addr_width - 1, 0>(rd_ptr);
        auto wr_a = bits<decltype(wr_ptr), addr_width - 1, 0>(wr_ptr);

        // 控制信号
        auto reading = io().pop && !io().empty;
        auto writing = io().push && !io().full;

        // 指针更新逻辑
        rd_ptr->next = select(reading, rd_ptr + 1, rd_ptr);
        wr_ptr->next = select(writing, wr_ptr + 1, wr_ptr);

        // 创建内存
        ch_mem<T, N> mem("fifo_mem");
        
        // 写操作
        mem.write(wr_a, io().din, writing);

        // 读操作（异步读取）
        io().dout = mem.aread(rd_a);

        // 状态信号
        io().empty = (wr_ptr == rd_ptr);
        io().full = (wr_a == rd_a) && (wr_ptr[addr_width] != rd_ptr[addr_width]);
    }
};

// 顶层模块
class Top : public ch::Component {
public:
    __io(
        ch_out<ch_uint<2>> dout;
        ch_out<ch_bool> empty;
        ch_out<ch_bool> full;
        ch_in<ch_uint<2>> din;
        ch_in<ch_bool> push;
        ch_in<ch_bool> pop;
    )

    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name)
    {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        //CH_MODULE((FiFo<ch_uint<2>, 2>), fifo_inst);
        ch::ch_module<FiFo<ch_uint<2>,2>> fifo_inst{"fifo_inst"};
        
        // 连接IO
        fifo_inst.io().din = io().din;
        fifo_inst.io().push = io().push;
        fifo_inst.io().pop = io().pop;
        io().dout = fifo_inst.io().dout;
        io().empty = fifo_inst.io().empty;
        io().full = fifo_inst.io().full;
    }
};

int main() {
    ch::ch_device<Top> top_device;

    ch::Simulator sim(top_device.context());
    
    // 初始化输入
    sim.set_input_value(top_device.instance().io().din, 0);
    sim.set_input_value(top_device.instance().io().push, false);
    sim.set_input_value(top_device.instance().io().pop, false);
    
    for (int cycle = 0; cycle <= 12; ++cycle) {
        sim.tick();
        
        // 获取输出值
        auto dout_val = sim.get_value(top_device.instance().io().dout);
        auto empty_val = sim.get_value(top_device.instance().io().empty);
        auto full_val = sim.get_value(top_device.instance().io().full);
        
        std::cout << "Cycle " << cycle 
                  << ": dout=" << dout_val 
                  << ", empty=" << empty_val 
                  << ", full=" << full_val 
                  << ", din=" << sim.get_value(top_device.instance().io().din)
                  << ", push=" << sim.get_value(top_device.instance().io().push)
                  << ", pop=" << sim.get_value(top_device.instance().io().pop)
                  << std::endl;
        
        // 测试激励
        switch (cycle) {
        case 0:
            // 初始状态检查
            if (empty_val != true || full_val != false) {
                std::cerr << "ERROR: Initial state incorrect!" << std::endl;
                return 1;
            }
            sim.set_input_value(top_device.instance().io().din, 1);
            sim.set_input_value(top_device.instance().io().push, true);
            sim.set_input_value(top_device.instance().io().pop, false);
            break;      
        case 2:
            if (empty_val != false || full_val != false || dout_val != 1) {
                std::cerr << "ERROR: Cycle 2 state incorrect!" << std::endl;
                return 1;
            }
            sim.set_input_value(top_device.instance().io().din, 2);
            sim.set_input_value(top_device.instance().io().push, true);
            break;
        case 4:
            if (empty_val != false || full_val != true || dout_val != 1) {
                std::cerr << "ERROR: Cycle 4 state incorrect!" << std::endl;
                return 1;
            }
            sim.set_input_value(top_device.instance().io().din, 0);
            sim.set_input_value(top_device.instance().io().push, false);
            break;
        case 6:
            if (empty_val != false || full_val != true || dout_val != 1) {
                std::cerr << "ERROR: Cycle 6 state incorrect!" << std::endl;
                return 1;
            }
            sim.set_input_value(top_device.instance().io().pop, true);
            break;
        case 8:
            if (empty_val != false || full_val != false || dout_val != 2) {
                std::cerr << "ERROR: Cycle 8 state incorrect!" << std::endl;
                return 1;
            }
            sim.set_input_value(top_device.instance().io().pop, true);
            break;
        case 10:
            if (empty_val != true || full_val != false || dout_val != 1) {
                std::cerr << "ERROR: Cycle 10 state incorrect!" << std::endl;
                return 1;
            }
            sim.set_input_value(top_device.instance().io().pop, false);
            break;
        default:
            // 保持当前状态
            break;
        }
    }

    // 生成Verilog
    ch::toVerilog("fifo.v", top_device.context());
    
    std::cout << "FIFO simulation completed successfully!" << std::endl;
    return 0;
}
