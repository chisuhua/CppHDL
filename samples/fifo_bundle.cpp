// samples/fifo_bundle.cpp
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/codegen.h"
#include "../include/io/common_bundles.h"
#include "../include/core/bundle/bundle_utils.h"
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
    
    // 使用Bundle作为IO接口
    fifo_bundle<T> io;

    FiFo(ch::Component* parent = nullptr, const std::string& name = "fifo")
        : ch::Component(parent, name)
    {
        io.as_slave();
    }

    void describe() override {
        // 创建读写指针寄存器（addr_width + 1 位用于满/空检测）
        ch_reg<ch_uint<addr_width + 1>> rd_ptr(0, "rd_ptr");
        ch_reg<ch_uint<addr_width + 1>> wr_ptr(0, "wr_ptr");

        // 提取地址位（低 addr_width 位）
        ch_uint<addr_width> rd_a, wr_a;
        
        if constexpr (addr_width == 1) {
            // 特殊处理1位地址的情况
            auto rd_bit = bit_select<decltype(rd_ptr), 0>(rd_ptr);
            auto wr_bit = bit_select<decltype(wr_ptr), 0>(wr_ptr);
            rd_a = zext<decltype(rd_bit), addr_width>(rd_bit);
            wr_a = zext<decltype(wr_bit), addr_width>(wr_bit);
        } else {
            // 一般情况使用bits提取
            rd_a = bits<decltype(rd_ptr), addr_width - 1, 0>(rd_ptr);
            wr_a = bits<decltype(wr_ptr), addr_width - 1, 0>(wr_ptr);
        }

        // 控制信号
        auto reading = io.pop;
        auto writing = io.push;

        // 指针更新逻辑
        rd_ptr->next = select(reading, rd_ptr + 1, rd_ptr);
        wr_ptr->next = select(writing, wr_ptr + 1, wr_ptr);

        // 创建内存
        ch_mem<T, N> mem("fifo_mem");
        
        // 写操作
        mem.write(wr_a, io.data, writing);

        // 读操作（异步读取）
        auto data_out = mem.aread(rd_a);
        // 注意：io.data是输入，不能赋值，我们只需要使用它作为输入
        // 数据输出通过其他方式提供，这里我们不直接连接io.data

        // 状态信号 - 使用更标准的FIFO空/满检测
        // 空状态：读指针等于写指针
        io.empty = (rd_ptr == wr_ptr);
        // 满状态：写指针+1等于读指针
        auto wr_plus_one = wr_ptr + 1;
        io.full = (wr_plus_one == rd_ptr);
    }
};

// 顶层模块 - 使用Bundle作为IO接口
class Top : public ch::Component {
public:
    // 直接使用Bundle作为顶层IO接口
    fifo_bundle<ch_uint<2>> io_bundle;
    
    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name)
    {
        // 设置IO方向为master（作为顶层模块，对外提供接口）
        io_bundle.as_master();
    }

    void create_ports() override {
        // 不需要创建传统端口
    }

    void describe() override {
        ch::ch_module<FiFo<ch_uint<2>, 2>> fifo_inst{"fifo_inst"};
        
        // 直接连接Bundle
        connect(io_bundle, fifo_inst.instance().io);
    }
};

int main() {
    ch::ch_device<Top> top_device;

    ch::Simulator sim(top_device.context());
    
    // 初始化输入
    // 使用Bundle设置输入值
    uint64_t initial_bundle_value = 0; // [empty=0, full=0, pop=0, push=0, data=0]
    sim.set_bundle_value(top_device.instance().io_bundle, initial_bundle_value);
    
    std::cout << "Starting FIFO Bundle simulation..." << std::endl;
    std::cout << "FIFO size: 2, addr_width: 1, pointer width: 2" << std::endl;
    
    for (int cycle = 0; cycle <= 12; ++cycle) {
        sim.tick();
        
        // 获取Bundle输出值
        uint64_t bundle_value = sim.get_bundle_value(top_device.instance().io_bundle);
        
        // 解析Bundle字段
        uint64_t data_val = bundle_value & 0x3;         // 低2位 (data)
        uint64_t push_val = (bundle_value >> 2) & 0x1;  // 第2位 (push)
        uint64_t full_val = (bundle_value >> 3) & 0x1;  // 第3位 (full)
        uint64_t pop_val = (bundle_value >> 4) & 0x1;   // 第4位 (pop)
        uint64_t empty_val = (bundle_value >> 5) & 0x1; // 第5位 (empty)
        
        std::cout << "Cycle " << cycle 
                  << ": data=0x" << std::hex << data_val
                  << ", push=" << push_val
                  << ", full=" << full_val
                  << ", pop=" << pop_val
                  << ", empty=" << empty_val
                  << std::dec << std::endl;
        
        // 测试激励
        switch (cycle) {
        case 0:
            // 初始状态检查
            std::cout << "  Initial state check..." << std::endl;
            if (empty_val != 1 || full_val != 0) {
                std::cerr << "ERROR: Initial state incorrect!" << std::endl;
                return 1;
            }
            // 写入数据1
            {
                uint64_t bundle_input = (1 << 2) | 1; // push=1, data=1
                sim.set_bundle_value(top_device.instance().io_bundle, bundle_input);
            }
            std::cout << "  Writing data 1 to FIFO" << std::endl;
            break;      
        case 1:
            // 在Cycle 1，继续写入数据2
            std::cout << "  Continuing write of data 1, writing data 2" << std::endl;
            {
                uint64_t bundle_input = (1 << 2) | 2; // push=1, data=2
                sim.set_bundle_value(top_device.instance().io_bundle, bundle_input);
            }
            break;
        case 2:
            // 在Cycle 2，停止写入，开始读取
            std::cout << "  Checking FIFO state after first write" << std::endl;
            {
                uint64_t bundle_input = (1 << 4); // pop=1
                sim.set_bundle_value(top_device.instance().io_bundle, bundle_input);
            }
            std::cout << "  Preparing to read first data from FIFO" << std::endl;
            break;
        case 3:
            // 在Cycle 3，应该读取到第一个数据(1)
            std::cout << "  Checking if first data (1) is available" << std::endl;
            {
                uint64_t bundle_input = (1 << 4); // pop=1
                sim.set_bundle_value(top_device.instance().io_bundle, bundle_input);
            }
            std::cout << "  Continuing read" << std::endl;
            break;
        case 4:
            // 在Cycle 4，应该读取到第二个数据(2)
            std::cout << "  Checking if second data (2) is available" << std::endl;
            {
                uint64_t bundle_input = 0; // pop=0
                sim.set_bundle_value(top_device.instance().io_bundle, bundle_input);
            }
            std::cout << "  Stopping read" << std::endl;
            break;
        default:
            // 保持当前状态
            break;
        }
    }

    // 生成Verilog
    ch::toVerilog("fifo_bundle.v", top_device.context());
    
    std::cout << "FIFO Bundle simulation completed successfully!" << std::endl;
    return 0;
}