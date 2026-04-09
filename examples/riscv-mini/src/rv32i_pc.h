/**
 * @file rv32i_pc.h
 * @brief RV32I 程序计数器 (PC) 更新逻辑
 * 
 * 功能:
 * - PC 寄存器 (32 位)
 * - 顺序更新：PC + 4
 * - 分支/跳转目标更新
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

class Rv32iPc : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> jump_target;   // 跳转目标地址
        ch_in<ch_bool>     jump_enable;   // 跳转使能
        ch_in<ch_bool>     rst;           // 复位
        ch_in<ch_bool>     clk;           // 时钟
        ch_out<ch_uint<32>> pc;           // 当前 PC
    )
    
    Rv32iPc(ch::Component* parent = nullptr, const std::string& name = "rv32i_pc")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch_reg<ch_uint<32>> pc_reg(0_d);
        ch_uint<32> next_pc(0_d);
        auto pc_plus_4 = pc_reg + ch_uint<32>(4_d);
        next_pc = select(io().jump_enable, io().jump_target, pc_plus_4);
        pc_reg->next = select(io().rst, ch_uint<32>(0_d), next_pc);
        io().pc = pc_reg;
    }
};

} // namespace riscv
