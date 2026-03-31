/**
 * RV32I Register File
 * 
 * 32×32-bit 寄存器文件
 * - x0 硬连线为 0
 * - 双读端口，单写端口
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

class RegFile : public ch::Component {
public:
    __io(
        // 读端口 1
        ch_in<ch_uint<5>> rs1_addr;
        ch_out<ch_uint<32>> rs1_data;
        
        // 读端口 2
        ch_in<ch_uint<5>> rs2_addr;
        ch_out<ch_uint<32>> rs2_data;
        
        // 写端口
        ch_in<ch_uint<5>> rd_addr;
        ch_in<ch_uint<32>> rd_data;
        ch_in<ch_bool> rd_we;  // 写使能
    )
    
    RegFile(ch::Component* parent = nullptr, const std::string& name = "regfile")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 32 个 32 位寄存器
        ch_reg<ch_uint<32>> regs[32];
        for (unsigned i = 0; i < 32; ++i) {
            regs[i] = ch_reg<ch_uint<32>>(0_d);
        }
        
        // x0 硬连线为 0
        io().rs1_data = select(io().rs1_addr == ch_uint<5>(0_d),
                               ch_uint<32>(0_d),
                               regs[io().rs1_addr]);
        
        io().rs2_data = select(io().rs2_addr == ch_uint<5>(0_d),
                               ch_uint<32>(0_d),
                               regs[io().rs2_addr]);
        
        // 写寄存器 (x0 不可写)
        auto write_valid = io().rd_we && (io().rd_addr != ch_uint<5>(0_d));
        
        // 使用 select 链实现寄存器写入
        for (unsigned i = 1; i < 32; ++i) {
            auto sel = (io().rd_addr == ch_uint<5>(i));
            regs[i]->next = select(write_valid && sel, io().rd_data, regs[i]);
        }
    }
};

} // namespace riscv
