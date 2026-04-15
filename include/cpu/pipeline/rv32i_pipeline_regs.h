/**
 * @file rv32i_pipeline_regs.h
 * @brief RV32I 5 级流水线寄存器定义
 * 
 * 流水线阶段:
 * - IF: Instruction Fetch (取指)
 * - ID: Instruction Decode (译码)
 * - EX: Execute (执行)
 * - MEM: Memory (访存)
 * - WB: Write Back (写回)
 * 
 * 流水线寄存器:
 * - IF/ID: 连接 IF 和 ID 阶段
 * - ID/EX: 连接 ID 和 EX 阶段
 * - EX/MEM: 连接 EX 和 MEM 阶段
 * - MEM/WB: 连接 MEM 和 WB 阶段
 */

#pragma once

#include "ch.hpp"
#include "rv32i_decoder.h"

using namespace ch::core;

namespace riscv {

// ============================================================================
// IF/ID 流水线寄存器 (数据结构)
// ============================================================================
/**
 * @brief IF/ID 流水线寄存器数据结构
 * 
 * 位置：IF → ID
 * 传递信号：
 * - pc: 当前指令的 PC 值 (用于分支/跳转计算)
 * - instruction: 从 I-TCM 读取的 32 位指令
 * - instr_valid: 指令有效标志
 */
struct IfIdRegs {
    ch_uint<32> pc;             // 当前指令的 PC 值
    ch_uint<32> instruction;    // 32 位指令
    ch_bool     instr_valid;    // 指令有效标志
    
    IfIdRegs() : pc(0_d), instruction(0_d), instr_valid(false) {}
};

// ============================================================================
// ID/EX 流水线寄存器 (数据结构)
// ============================================================================
/**
 * @brief ID/EX 流水线寄存器数据结构
 * 
 * 位置：ID → EX
 * 传递信号：
 * - 控制信号：reg_write, alu_src, branch, jump, alu_op
 * - RS1 数据：从寄存器文件读取的操作数 1
 * - RS2 数据：从寄存器文件读取的操作数 2
 * - 立即数：解码后的立即数
 * - 目的寄存器地址：rd
 * - PC: 用于分支/跳转目标计算
 */
struct IdExRegs {
    // 控制信号
    ch_bool         reg_write;      // 寄存器写使能
    ch_bool         alu_src;        // ALU 源选择：0=寄存器，1=立即数
    ch_bool         branch;         // 分支使能
    ch_bool         jump;           // 跳转使能
    ch_bool         is_load;        // LOAD 指令
    ch_bool         is_store;       // STORE 指令
    ch_uint<4>      alu_op;         // ALU 操作码
    
    // 寄存器数据
    ch_uint<32>     rs1_data;       // RS1 读取数据
    ch_uint<32>     rs2_data;       // RS2 读取数据
    
    // 立即数
    ch_uint<32>     imm;            // 解码后的立即数
    
    // 目的寄存器
    ch_uint<5>      rd;             // 目的寄存器地址
    
    // PC (用于分支/跳转)
    ch_uint<32>     pc;             // 当前指令 PC
    
    // 函数字段 (用于分支条件判断)
    ch_uint<3>      funct3;         // funct3 字段
    
    IdExRegs() : 
        reg_write(false), alu_src(false), branch(false), jump(false),
        is_load(false), is_store(false), alu_op(0_d),
        rs1_data(0_d), rs2_data(0_d), imm(0_d), rd(0_d), pc(0_d),
        funct3(0_d) {}
};

// ============================================================================
// EX/MEM 流水线寄存器 (数据结构)
// ============================================================================
/**
 * @brief EX/MEM 流水线寄存器数据结构
 * 
 * 位置：EX → MEM
 * 传递信号：
 * - ALU 结果：ALU 计算结果或有效地址
 * - 写数据：要写入内存的数据 (来自 RS2)
 * - 控制信号：reg_write, is_load, is_store, jump
 * - 目的寄存器地址：rd
 * - PC: 用于异常处理
 */
struct ExMemRegs {
    // ALU 结果
    ch_uint<32>     alu_result;     // ALU 计算结果或有效地址
    
    // 写数据 (用于 STORE)
    ch_uint<32>     write_data;     // 要写入内存的数据
    
    // 控制信号
    ch_bool         reg_write;      // 寄存器写使能
    ch_bool         is_load;        // LOAD 指令
    ch_bool         is_store;       // STORE 指令
    ch_bool         jump;           // 跳转使能
    ch_uint<4>      alu_op;         // ALU 操作码 (用于除法等特殊指令)
    
    // 目的寄存器
    ch_uint<5>      rd;             // 目的寄存器地址
    
    // PC (用于异常处理)
    ch_uint<32>     pc;             // 当前指令 PC
    
    ExMemRegs() : 
        alu_result(0_d), write_data(0_d),
        reg_write(false), is_load(false), is_store(false), jump(false),
        alu_op(0_d), rd(0_d), pc(0_d) {}
};

// ============================================================================
// MEM/WB 流水线寄存器 (数据结构)
// ============================================================================
/**
 * @brief MEM/WB 流水线寄存器数据结构
 * 
 * 位置：MEM → WB
 * 传递信号：
 * - 读数据：从 D-TCM 读取的数据 (LOAD 指令)
 * - ALU 结果：ALU 计算结果 (非 LOAD 指令)
 * - 控制信号：reg_write
 * - 目的寄存器地址：rd
 */
struct MemWbRegs {
    // 从内存读取的数据 (LOAD 指令)
    ch_uint<32>     mem_read_data;  // 从 D-TCM 读取的数据
    
    // ALU 结果 (非 LOAD 指令)
    ch_uint<32>     alu_result;     // ALU 计算结果
    
    // 控制信号
    ch_bool         reg_write;      // 寄存器写使能
    ch_bool         is_load;        // LOAD 指令 (用于选择写回数据源)
    
    // 目的寄存器
    ch_uint<5>      rd;             // 目的寄存器地址
    
    MemWbRegs() : 
        mem_read_data(0_d), alu_result(0_d),
        reg_write(false), is_load(false), rd(0_d) {}
};

// ============================================================================
// IF/ID 流水线寄存器组件
// ============================================================================
/**
 * @brief IF/ID 流水线寄存器组件
 * 
 * 在时钟上升沿锁存 IF 阶段输出，传递给 ID 阶段
 */
class IfIdPipelineReg : public ch::Component {
public:
    __io(
        // 输入 (来自 IF 阶段)
        ch_in<ch_uint<32>>  if_pc;
        ch_in<ch_uint<32>>  if_instruction;
        ch_in<ch_bool>      if_instr_valid;
        
        // 输出 (传递给 ID 阶段)
        ch_out<ch_uint<32>> id_pc;
        ch_out<ch_uint<32>> id_instruction;
        ch_out<ch_bool>     id_instr_valid;
        
        // 控制
        ch_in<ch_bool>      clk;
        ch_in<ch_bool>      rst;
        ch_in<ch_bool>      stall;      // 流水线停顿
        ch_in<ch_bool>      flush;      // 流水线冲刷 (用于分支预测错误)
    )
    
    IfIdPipelineReg(ch::Component* parent = nullptr, const std::string& name = "if_id_reg")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 寄存器状态
        ch_reg<ch_uint<32>> pc_reg(0_d);
        ch_reg<ch_uint<32>> instr_reg(0_d);
        ch_reg<ch_bool>     valid_reg(false);
        
        // 更新逻辑
        auto update = select(!io().rst, select(!io().stall, !io().flush, ch_bool(false)), ch_bool(false));
        
        pc_reg->next = select(update, io().if_pc, pc_reg);
        instr_reg->next = select(update, io().if_instruction, instr_reg);
        valid_reg->next = select(update, io().if_instr_valid, 
                            select(io().flush, ch_bool(false), valid_reg));
        
        // 输出
        io().id_pc = pc_reg;
        io().id_instruction = instr_reg;
        io().id_instr_valid = valid_reg;
    }
};

// ============================================================================
// ID/EX 流水线寄存器组件
// ============================================================================
/**
 * @brief ID/EX 流水线寄存器组件
 * 
 * 在时钟上升沿锁存 ID 阶段输出，传递给 EX 阶段
 */
class IdExPipelineReg : public ch::Component {
public:
    __io(
        // 输入 (来自 ID 阶段)
        ch_in<ch_bool>      id_reg_write;
        ch_in<ch_bool>      id_alu_src;
        ch_in<ch_bool>      id_branch;
        ch_in<ch_bool>      id_jump;
        ch_in<ch_bool>      id_is_load;
        ch_in<ch_bool>      id_is_store;
        ch_in<ch_uint<4>>   id_alu_op;
        ch_in<ch_uint<32>>  id_rs1_data;
        ch_in<ch_uint<32>>  id_rs2_data;
        ch_in<ch_uint<32>>  id_imm;
        ch_in<ch_uint<5>>   id_rd;
        ch_in<ch_uint<32>>  id_pc;
        ch_in<ch_uint<3>>   id_funct3;
        
        // 输出 (传递给 EX 阶段)
        ch_out<ch_bool>     ex_reg_write;
        ch_out<ch_bool>     ex_alu_src;
        ch_out<ch_bool>     ex_branch;
        ch_out<ch_bool>     ex_jump;
        ch_out<ch_bool>     ex_is_load;
        ch_out<ch_bool>     ex_is_store;
        ch_out<ch_uint<4>>  ex_alu_op;
        ch_out<ch_uint<32>> ex_rs1_data;
        ch_out<ch_uint<32>> ex_rs2_data;
        ch_out<ch_uint<32>> ex_imm;
        ch_out<ch_uint<5>>  ex_rd;
        ch_out<ch_uint<32>> ex_pc;
        ch_out<ch_uint<3>>  ex_funct3;
        
        // 控制
        ch_in<ch_bool>      clk;
        ch_in<ch_bool>      rst;
        ch_in<ch_bool>      stall;
        ch_in<ch_bool>      flush;
    )
    
    IdExPipelineReg(ch::Component* parent = nullptr, const std::string& name = "id_ex_reg")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 控制信号寄存器
        ch_reg<ch_bool>     reg_write_reg(false);
        ch_reg<ch_bool>     alu_src_reg(false);
        ch_reg<ch_bool>     branch_reg(false);
        ch_reg<ch_bool>     jump_reg(false);
        ch_reg<ch_bool>     is_load_reg(false);
        ch_reg<ch_bool>     is_store_reg(false);
        ch_reg<ch_uint<4>>  alu_op_reg(0_d);
        ch_reg<ch_uint<3>>  funct3_reg(0_d);
        
        // 数据寄存器
        ch_reg<ch_uint<32>> rs1_data_reg(0_d);
        ch_reg<ch_uint<32>> rs2_data_reg(0_d);
        ch_reg<ch_uint<32>> imm_reg(0_d);
        ch_reg<ch_uint<5>>  rd_reg(0_d);
        ch_reg<ch_uint<32>> pc_reg(0_d);
        
        // 更新逻辑
        auto update = select(!io().rst, select(!io().stall, !io().flush, ch_bool(false)), ch_bool(false));
        
        reg_write_reg->next = select(update, io().id_reg_write, reg_write_reg);
        alu_src_reg->next   = select(update, io().id_alu_src, alu_src_reg);
        branch_reg->next    = select(update, io().id_branch, branch_reg);
        jump_reg->next      = select(update, io().id_jump, jump_reg);
        is_load_reg->next   = select(update, io().id_is_load, is_load_reg);
        is_store_reg->next  = select(update, io().id_is_store, is_store_reg);
        alu_op_reg->next    = select(update, io().id_alu_op, alu_op_reg);
        funct3_reg->next    = select(update, io().id_funct3, funct3_reg);
        
        rs1_data_reg->next  = select(update, io().id_rs1_data, rs1_data_reg);
        rs2_data_reg->next  = select(update, io().id_rs2_data, rs2_data_reg);
        imm_reg->next       = select(update, io().id_imm, imm_reg);
        rd_reg->next        = select(update, io().id_rd, rd_reg);
        pc_reg->next        = select(update, io().id_pc, pc_reg);
        
        // 输出
        io().ex_reg_write  = reg_write_reg;
        io().ex_alu_src    = alu_src_reg;
        io().ex_branch     = branch_reg;
        io().ex_jump       = jump_reg;
        io().ex_is_load    = is_load_reg;
        io().ex_is_store   = is_store_reg;
        io().ex_alu_op     = alu_op_reg;
        io().ex_funct3     = funct3_reg;
        
        io().ex_rs1_data   = rs1_data_reg;
        io().ex_rs2_data   = rs2_data_reg;
        io().ex_imm        = imm_reg;
        io().ex_rd         = rd_reg;
        io().ex_pc         = pc_reg;
    }
};

// ============================================================================
// EX/MEM 流水线寄存器组件
// ============================================================================
/**
 * @brief EX/MEM 流水线寄存器组件
 * 
 * 在时钟上升沿锁存 EX 阶段输出，传递给 MEM 阶段
 */
class ExMemPipelineReg : public ch::Component {
public:
    __io(
        // 输入 (来自 EX 阶段)
        ch_in<ch_uint<32>>  ex_alu_result;
        ch_in<ch_uint<32>>  ex_write_data;
        ch_in<ch_bool>      ex_reg_write;
        ch_in<ch_bool>      ex_is_load;
        ch_in<ch_bool>      ex_is_store;
        ch_in<ch_bool>      ex_jump;
        ch_in<ch_uint<4>>   ex_alu_op;
        ch_in<ch_uint<5>>   ex_rd;
        ch_in<ch_uint<32>>  ex_pc;
        
        // 输出 (传递给 MEM 阶段)
        ch_out<ch_uint<32>> mem_alu_result;
        ch_out<ch_uint<32>> mem_write_data;
        ch_out<ch_bool>     mem_reg_write;
        ch_out<ch_bool>     mem_is_load;
        ch_out<ch_bool>     mem_is_store;
        ch_out<ch_bool>     mem_jump;
        ch_out<ch_uint<4>>  mem_alu_op;
        ch_out<ch_uint<5>>  mem_rd;
        ch_out<ch_uint<32>> mem_pc;
        
        // 控制
        ch_in<ch_bool>      clk;
        ch_in<ch_bool>      rst;
        ch_in<ch_bool>      stall;
        ch_in<ch_bool>      flush;
    )
    
    ExMemPipelineReg(ch::Component* parent = nullptr, const std::string& name = "ex_mem_reg")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 数据寄存器
        ch_reg<ch_uint<32>> alu_result_reg(0_d);
        ch_reg<ch_uint<32>> write_data_reg(0_d);
        ch_reg<ch_uint<32>> pc_reg(0_d);
        
        // 控制信号寄存器
        ch_reg<ch_bool>     reg_write_reg(false);
        ch_reg<ch_bool>     is_load_reg(false);
        ch_reg<ch_bool>     is_store_reg(false);
        ch_reg<ch_bool>     jump_reg(false);
        ch_reg<ch_uint<4>>  alu_op_reg(0_d);
        ch_reg<ch_uint<5>>  rd_reg(0_d);
        
        // 更新逻辑
        auto update = select(!io().rst, select(!io().stall, !io().flush, ch_bool(false)), ch_bool(false));
        
        alu_result_reg->next = select(update, io().ex_alu_result, alu_result_reg);
        write_data_reg->next = select(update, io().ex_write_data, write_data_reg);
        pc_reg->next         = select(update, io().ex_pc, pc_reg);
        
        reg_write_reg->next = select(update, io().ex_reg_write, reg_write_reg);
        is_load_reg->next   = select(update, io().ex_is_load, is_load_reg);
        is_store_reg->next  = select(update, io().ex_is_store, is_store_reg);
        jump_reg->next      = select(update, io().ex_jump, jump_reg);
        alu_op_reg->next    = select(update, io().ex_alu_op, alu_op_reg);
        rd_reg->next        = select(update, io().ex_rd, rd_reg);
        
        // 输出
        io().mem_alu_result = alu_result_reg;
        io().mem_write_data = write_data_reg;
        io().mem_pc         = pc_reg;
        
        io().mem_reg_write  = reg_write_reg;
        io().mem_is_load    = is_load_reg;
        io().mem_is_store   = is_store_reg;
        io().mem_jump       = jump_reg;
        io().mem_alu_op     = alu_op_reg;
        io().mem_rd         = rd_reg;
    }
};

// ============================================================================
// MEM/WB 流水线寄存器组件
// ============================================================================
/**
 * @brief MEM/WB 流水线寄存器组件
 * 
 * 在时钟上升沿锁存 MEM 阶段输出，传递给 WB 阶段
 */
class MemWbPipelineReg : public ch::Component {
public:
    __io(
        // 输入 (来自 MEM 阶段)
        ch_in<ch_uint<32>>  mem_read_data;
        ch_in<ch_uint<32>>  mem_alu_result;
        ch_in<ch_bool>      mem_reg_write;
        ch_in<ch_bool>      mem_is_load;
        ch_in<ch_uint<5>>   mem_rd;
        
        // 输出 (传递给 WB 阶段)
        ch_out<ch_uint<32>> wb_read_data;
        ch_out<ch_uint<32>> wb_alu_result;
        ch_out<ch_bool>     wb_reg_write;
        ch_out<ch_bool>     wb_is_load;
        ch_out<ch_uint<5>>  wb_rd;
        
        // 控制
        ch_in<ch_bool>      clk;
        ch_in<ch_bool>      rst;
        ch_in<ch_bool>      stall;
        ch_in<ch_bool>      flush;
    )
    
    MemWbPipelineReg(ch::Component* parent = nullptr, const std::string& name = "mem_wb_reg")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 数据寄存器
        ch_reg<ch_uint<32>> read_data_reg(0_d);
        ch_reg<ch_uint<32>> alu_result_reg(0_d);
        
        // 控制信号寄存器
        ch_reg<ch_bool>     reg_write_reg(false);
        ch_reg<ch_bool>     is_load_reg(false);
        ch_reg<ch_uint<5>>  rd_reg(0_d);
        
        // 更新逻辑
        auto update = select(!io().rst, select(!io().stall, !io().flush, ch_bool(false)), ch_bool(false));
        
        read_data_reg->next   = select(update, io().mem_read_data, read_data_reg);
        alu_result_reg->next  = select(update, io().mem_alu_result, alu_result_reg);
        
        reg_write_reg->next = select(update, io().mem_reg_write, reg_write_reg);
        is_load_reg->next   = select(update, io().mem_is_load, is_load_reg);
        rd_reg->next        = select(update, io().mem_rd, rd_reg);
        
        // 输出
        io().wb_read_data   = read_data_reg;
        io().wb_alu_result  = alu_result_reg;
        io().wb_reg_write   = reg_write_reg;
        io().wb_is_load     = is_load_reg;
        io().wb_rd          = rd_reg;
    }
};

} // namespace riscv
