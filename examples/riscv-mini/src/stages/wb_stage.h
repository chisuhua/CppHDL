/**
 * @file wb_stage.h
 * @brief RISC-V 5 级流水线 - WB 级 (写回级)
 * 
 * 功能:
 * - 写回数据多选器 (ALU 结果 vs 内存数据)
 * - 写使能控制
 * - 防止 x0 寄存器写入
 * - 写回寄存器地址输出
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/arithmetic.h"
#include "chlib/logic.h"

using namespace ch::core;

namespace riscv {

/**
 * WB 级 (写回级) 组件
 * 
 * 包含:
 * - 写回数据多选器：选择 ALU 结果或内存数据作为写回数据
 * - 写使能生成逻辑：根据指令类型生成写使能信号
 * - x0 寄存器保护：防止意外写入 x0 (硬连线为 0)
 * - 写回寄存器地址输出
 */
class WbStage : public ch::Component {
public:
    __io(
        // ====================================================================
        // 输入端口：来自 MEM/WB 流水线寄存器
        // ====================================================================
        ch_in<ch_uint<32>> alu_result;   // ALU 计算结果 (来自 EX/MEM)
        ch_in<ch_uint<32>> mem_data;     // 内存读出的数据 (来自 MEM 级)
        ch_in<ch_uint<5>>  rd_addr;      // 目标寄存器地址 (来自 ID 级)
        ch_in<ch_bool>     is_load;      // 是否为加载指令
        ch_in<ch_bool>     is_alu;       // 是否为 ALU 指令 (R 型/I 型算术逻辑)
        ch_in<ch_bool>     is_jump;      // 是否为跳转指令 (JAL/JALR)
        ch_in<ch_bool>     mem_valid;    // 内存数据有效 (来自 MEM 级)
        
        // ====================================================================
        // 输出端口：连接到寄存器文件的写端口
        // ====================================================================
        ch_out<ch_uint<5>> rd_addr_out;  // 写回寄存器地址
        ch_out<ch_uint<32>> write_data;  // 写回数据
        ch_out<ch_bool>     write_en;    // 写使能信号
    )

    WbStage(ch::Component* parent = nullptr, const std::string& name = "wb_stage")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // ====================================================================
        // 1. 写回数据多选器
        // ====================================================================
        // 根据指令类型选择写回数据的来源:
        // - 加载指令 (LW/LH/LB 等): 使用内存数据 (mem_data)
        // - ALU 指令 (ADD/SUB/AND 等): 使用 ALU 结果 (alu_result)
        // - 跳转指令 (JAL/JALR): 使用 ALU 结果 (PC+4 或 PC+offset)
        
        // ---- 1.1 加载指令数据选择 ----
        // 如果是加载指令，选择内存数据
        // 否则选择 ALU 结果
        ch_uint<32> wb_data = ch_uint<32>(0_d);
        
        // 使用 select() 进行数据选择 (关键：不能用 && 运算符)
        // 优先级：加载指令 > ALU 指令/跳转指令
        wb_data = select(io().is_load, io().mem_data, wb_data);
        wb_data = select(io().is_alu | io().is_jump, io().alu_result, wb_data);

        // ====================================================================
        // 2. 写使能生成逻辑
        // ====================================================================
        // 写使能条件:
        // 1. 指令需要写回寄存器 (is_alu | is_load | is_jump)
        // 2. 目标寄存器不是 x0 (rd_addr != 0)
        // 3. 指令有效 (假设有 valid 信号，这里简化处理)
        
        // ---- 2.1 指令写回类型判断 ----
        auto needs_writeback = io().is_alu | io().is_load | io().is_jump;
        
        // ---- 2.2 x0 寄存器保护 ----
        // RISC-V 规定 x0 寄存器硬连线为 0，不能写入
        // 检测 rd_addr 是否为 0
        auto rd_is_zero = (io().rd_addr == ch_uint<5>(0_d));
        auto rd_not_zero = !rd_is_zero;
        
        // ---- 2.3 最终写使能信号 ----
        // 只有当需要写回且目标寄存器不是 x0 时才允许写入
        auto write_enable_sig = needs_writeback & rd_not_zero;

        // ====================================================================
        // 3. 写回寄存器地址
        // ====================================================================
        // 直接传递 rd_addr 到输出
        // 注意：即使写使能为 0，地址也需要传递 (但寄存器文件不会写入)
        
        auto rd_addr_output = io().rd_addr;

        // ====================================================================
        // 4. 输出端口连接
        // ====================================================================

        // 写回寄存器地址
        io().rd_addr_out = rd_addr_output;
        
        // 写回数据 (ALU 结果或内存数据)
        io().write_data = wb_data;
        
        // 写使能信号 (控制寄存器文件写入)
        io().write_en = write_enable_sig;
    }
};

} // namespace riscv
