/**
 * @file rv32i_tcm.h
 * @brief RV32I TCM (Tightly Coupled Memory)
 * 
 * 指令和数据紧耦合存储器:
 * - I-TCM: 指令存储器 (ROM 模型)
 * - D-TCM: 数据存储器 (SRAM 模型)
 * - 单周期访问
 * - 可选 AXI4 接口
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

// ============================================================================
// I-TCM (Instruction TCM) - 指令存储器
// ============================================================================

template <unsigned ADDR_WIDTH = 20, unsigned DATA_WIDTH = 32>
class InstrTCM : public ch::Component {
public:
    static constexpr unsigned SIZE = 1 << ADDR_WIDTH;  // 存储器大小
    
    __io(
        // 指令接口 (简化)
        ch_in<ch_uint<ADDR_WIDTH>> addr;
        ch_out<ch_uint<DATA_WIDTH>> data;
        ch_out<ch_bool> ready;
        
        // 写接口 (用于加载程序)
        ch_in<ch_bool> write_en;
        ch_in<ch_uint<ADDR_WIDTH>> write_addr;
        ch_in<ch_uint<DATA_WIDTH>> write_data;
    )
    
    InstrTCM(ch::Component* parent = nullptr, const std::string& name = "itcm")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 简化 ROM 模型
        // 实际实现应该使用 ch_mem
        ch_mem<ch_uint<DATA_WIDTH>, SIZE> memory("itcm_memory");
        
        // 读端口
        auto read_data = memory.sread(io().addr, ch_bool(true));
        io().data <<= read_data;
        io().ready = ch_bool(true);  // 单周期访问
        
        // 写端口 (仅用于初始化)
        memory.write(io().write_addr, io().write_data, io().write_en);
    }
};

// ============================================================================
// D-TCM (Data TCM) - 数据存储器
// ============================================================================

template <unsigned ADDR_WIDTH = 20, unsigned DATA_WIDTH = 32>
class DataTCM : public ch::Component {
public:
    static constexpr unsigned SIZE = 1 << ADDR_WIDTH;  // 存储器大小
    
    __io(
        // 数据接口 (简化)
        ch_in<ch_uint<ADDR_WIDTH>> addr;
        ch_in<ch_bool> write;
        ch_in<ch_uint<DATA_WIDTH>> wdata;
        ch_in<ch_bool> valid;
        ch_out<ch_bool> ready;
        ch_out<ch_uint<DATA_WIDTH>> rdata;
    )
    
    DataTCM(ch::Component* parent = nullptr, const std::string& name = "dtcm")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // SRAM 模型
        ch_mem<ch_uint<DATA_WIDTH>, SIZE> memory("dtcm_memory");
        
        // 写操作
        memory.write(io().addr, io().wdata, io().write && io().valid);
        
        // 读操作
        auto read_data = memory.sread(io().addr, !io().write);
        io().rdata <<= read_data;
        io().ready = ch_bool(true);  // 单周期访问
    }
};

// ============================================================================
// Boot ROM - 启动 ROM
// ============================================================================

template <unsigned ADDR_WIDTH = 12, unsigned DATA_WIDTH = 32>
class BootROM : public ch::Component {
public:
    static constexpr unsigned SIZE = 1 << ADDR_WIDTH;  // 4KB
    
    __io(
        ch_in<ch_uint<ADDR_WIDTH>> addr;
        ch_out<ch_uint<DATA_WIDTH>> data;
        ch_out<ch_bool> ready;
    )
    
    BootROM(ch::Component* parent = nullptr, const std::string& name = "boot_rom")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 简化：返回固定值
        // 实际应该包含启动代码
        io().data <<= ch_uint<DATA_WIDTH>(0x13_d);  // NOP 指令
        io().ready = ch_bool(true);
    }
};

} // namespace riscv
