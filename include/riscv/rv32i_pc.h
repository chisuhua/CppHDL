/**
 * RV32I Program Counter
 * 
 * 程序计数器 + 分支目标计算
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

class ProgramCounter : public ch::Component {
public:
    __io(
        ch_out<ch_uint<32>> pc;           // 当前 PC
        ch_in<ch_uint<32>> next_pc;       // 下一 PC (顺序)
        ch_in<ch_uint<32>> branch_target; // 分支目标
        ch_in<ch_bool> branch_taken;      // 分支跳转
        ch_in<ch_bool> jump;              // 跳转
        ch_in<ch_bool> stall;             // 暂停
        ch_in<ch_bool> reset;             // 复位
    )
    
    ProgramCounter(ch::Component* parent = nullptr, const std::string& name = "pc")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // PC 寄存器
        ch_reg<ch_uint<32>> pc_reg(0_d);
        
        // 下一 PC 选择
        auto next_pc_val = select(branch_taken || jump, branch_target, io().next_pc);
        
        // PC 更新 (暂停时保持不变)
        pc_reg->next = select(io().stall, pc_reg,
                              select(io().reset, ch_uint<32>(0_d), next_pc_val));
        
        io().pc = pc_reg;
    }
};

/**
 * 分支目标计算单元
 */
class BranchTargetCalc : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> pc;
        ch_in<ch_int<32>> imm;
        ch_out<ch_uint<32>> target;
    )
    
    BranchTargetCalc(ch::Component* parent = nullptr, const std::string& name = "branch_calc")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 分支目标 = PC + imm (imm 已经是符号扩展的)
        // PC 总是 4 字节对齐，imm 也是 4 字节对齐 (B 格式和 J 格式)
        io().target = pc + ch_uint<32>(imm);
    }
};

} // namespace riscv
