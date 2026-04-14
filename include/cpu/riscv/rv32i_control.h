/**
 * @file rv32i_control.h
 * @brief RV32I 流水线控制信号生成单元
 * 
 * 功能说明:
 * - 根据译码器输出的指令类型生成所有流水线控制信号
 * - 支持 R/I/S/B/U/J 型指令的控制信号生成
 * - 不处理 SYSTEM 指令 (由 Phase A4 单独处理)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

// Opcode 定义 (与 rv32i_decoder.h 保持一致)
constexpr unsigned OP_LOAD   = 0b0000011;
constexpr unsigned OP_STORE  = 0b0100011;
constexpr unsigned OP_BRANCH = 0b1100011;
constexpr unsigned OP_JALR   = 0b1100111;
constexpr unsigned OP_JAL    = 0b1101111;
constexpr unsigned OP_LUI    = 0b0110111;
constexpr unsigned OP_AUIPC  = 0b0010111;
constexpr unsigned OP_OPIMM  = 0b0010011;
constexpr unsigned OP_OP     = 0b0110011;

/**
 * @brief RV32I 控制信号生成单元
 * 
 * 根据译码器输出的指令信息生成控制信号:
 * - reg_write: 寄存器写使能
 * - alu_src: ALU 操作数 B 选择 (0=rs2, 1=立即数)
 * - alu_op: ALU 操作选择 (0=立即数操作, 1=funct7 控制)
 * - mem_read: 内存读使能
 * - mem_write: 内存写使能
 * - pc_src: PC 源选择 (0=PC+4, 1=分支/跳转目标)
 * - wb_sel: 写回数据选择 (0=ALU 结果, 1=内存数据)
 */
class ControlUnit : public ch::Component {
public:
    __io(
        // 输入: 来自译码器
        ch_in<ch_uint<7>>  opcode;    // 指令 opcode
        ch_in<ch_uint<3>>  funct3;    // funct3 字段
        ch_in<ch_uint<7>>  funct7;    // funct7[6:0] 字段
        ch_in<ch_bool>     is_branch; // 分支指令标志
        ch_in<ch_bool>     is_jump;   // 跳转指令标志
        ch_in<ch_bool>     is_load;   // 加载指令标志
        ch_in<ch_bool>     is_store;  // 存储指令标志
        
        // 输出: 控制信号
        ch_out<ch_bool>    reg_write; // 寄存器写使能
        ch_out<ch_bool>    alu_src;   // ALU 操作数 B 选择 (1=立即数)
        ch_out<ch_bool>    alu_op;    // ALU 操作来自 funct7 (1=使用)
        ch_out<ch_bool>    mem_read;  // 内存读使能
        ch_out<ch_bool>    mem_write; // 内存写使能
        ch_out<ch_bool>    pc_src;    // PC 源选择 (1=分支/跳转)
        ch_out<ch_uint<2>> wb_sel;    // 写回选择 (0=ALU, 1=MEM)
    )
    
    ControlUnit(ch::Component* parent = nullptr, const std::string& name = "control_unit")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 指令类型识别
        // ========================================================================
        
        // R-type: OP opcode (寄存器-寄存器操作)
        ch_bool is_op = (io().opcode == ch_uint<7>(OP_OP));
        
        // I-type: OPIMM opcode (立即数操作)
        ch_bool is_opimm = (io().opcode == ch_uint<7>(OP_OPIMM));
        
        // I-type: LOAD opcode (加载操作)
        ch_bool is_load_instr = (io().opcode == ch_uint<7>(OP_LOAD));
        
        // S-type: STORE opcode (存储操作)
        ch_bool is_store_instr = (io().opcode == ch_uint<7>(OP_STORE));
        
        // B-type: BRANCH opcode (分支操作) - is_branch 已由译码器提供
        // J-type: JAL opcode (跳转链接)
        ch_bool is_jal = (io().opcode == ch_uint<7>(OP_JAL));
        
        // I-type: JALR opcode (跳转链接寄存器)
        ch_bool is_jalr = (io().opcode == ch_uint<7>(OP_JALR));
        
        // U-type: LUI opcode (加载上位立即数)
        ch_bool is_lui = (io().opcode == ch_uint<7>(OP_LUI));
        
        // U-type: AUIPC opcode (加 PC 上位立即数)
        ch_bool is_auipc = (io().opcode == ch_uint<7>(OP_AUIPC));
        
        // ========================================================================
        // 控制信号生成
        // ========================================================================
        
        // reg_write: 寄存器写使能
        // R-type, I-type ALU, I-type Load, JAL, JALR, LUI, AUIPC 需要写寄存器
        // S-type (Store), B-type (Branch) 不写寄存器
        io().reg_write = is_op | is_opimm | is_load_instr | 
                         is_jal | is_jalr | is_lui | is_auipc;
        
        // alu_src: ALU 操作数 B 选择
        // R-type: 使用 rs2 (0)
        // I-type ALU, I-type Load, S-type, JALR: 使用立即数 (1)
        io().alu_src = is_opimm | is_load_instr | is_store_instr | is_jalr;
        
        // alu_op: ALU 操作选择
        // R-type: funct7 控制 ALU 操作 (1)
        // 其他: 直接使用立即数操作 (0)
        io().alu_op = is_op;
        
        // mem_read: 内存读使能
        // 只有 Load 指令需要读内存
        io().mem_read = is_load_instr;
        
        // mem_write: 内存写使能
        // 只有 Store 指令需要写内存
        io().mem_write = is_store_instr;
        
        // pc_src: PC 源选择
        // Branch, JAL, JALR 需要跳转到目标地址 (1)
        // 其他: PC+4 (0)
        io().pc_src = io().is_branch | is_jal | is_jalr;
        
        // wb_sel: 写回数据选择
        // Load: 从内存读取 (1)
        // 其他: ALU 结果 (0)
        io().wb_sel = select(is_load_instr, ch_uint<2>(1_d), ch_uint<2>(0_d));
    }
};

} // namespace riscv
