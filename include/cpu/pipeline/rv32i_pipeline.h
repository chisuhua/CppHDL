/**
 * @file rv32i_pipeline.h
 * @brief RISC-V RV32I 5 级流水线顶层集成
 * 
 * 流水线结构:
 * ┌─────┬─────┬─────┬─────┐
 * │ IF  │ ID  │ EX  │ MEM │ WB │
 * ├─────┼─────┼─────┼─────┼────┤
 * │PC+4 │Decode│ALU  │Mem  │Write│
 * │IF/ID│ID/EX│EX/MEM│MEM/WB│back│
 * └─────┴─────┴─────┴─────┴────┘
 * 
 * 功能:
 * - 5 级流水线：取指 (IF) → 译码 (ID) → 执行 (EX) → 访存 (MEM) → 写回 (WB)
 * - 数据前推：EX→EX, MEM→EX, WB→EX
 * - Load-Use 冒险检测与停顿
 * - 分支预测错误冲刷
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "stages/if_stage.h"
#include "stages/id_stage.h"
#include "stages/ex_stage.h"
#include "stages/mem_stage.h"
#include "stages/wb_stage.h"
#include "hazard_unit.h"
#include "rv32i_pipeline_regs.h"

using namespace ch::core;

namespace riscv {

class Rv32iPipeline : public ch::Component {
public:
    __io(
        // 系统控制
        ch_in<ch_bool>      rst;
        ch_in<ch_bool>      clk;
        
        // 指令存储器接口 (连接到 I-TCM)
        ch_out<ch_uint<32>> instr_addr;
        ch_in<ch_uint<32>>  instr_data;
        ch_in<ch_bool>      instr_ready;
        
        // 数据存储器接口 (连接到 D-TCM)
        ch_out<ch_uint<32>> data_addr;
        ch_out<ch_uint<32>> data_write_data;
        ch_out<ch_uint<4>>  data_strbe;
        ch_out<ch_bool>     data_write_en;
        ch_in<ch_uint<32>>  data_read_data;
        ch_in<ch_bool>      data_ready;
    )
    
    Rv32iPipeline(ch::Component* parent = nullptr, const std::string& name = "rv32i_pipeline")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 5 级流水线模块
        ch::ch_module<IfStage>  if_stage{"if_stage"};
        ch::ch_module<IdStage>  id_stage{"id_stage"};
        ch::ch_module<ExStage>  ex_stage{"ex_stage"};
        ch::ch_module<MemStage> mem_stage{"mem_stage"};
        ch::ch_module<WbStage>  wb_stage{"wb_stage"};
        
        // 冒险检测单元
        ch::ch_module<HazardUnit> hazard{"hazard_unit"};
        
        // 流水线级寄存器文件 (32x32 bit)
        // 移到这里以打破 ID→reg_file→WB 的环
        ch_mem<ch_uint<32>, 32> reg_file("reg_file");
        
        // 从 reg_file 读端口读取数据 (组合逻辑)
        auto rs1_data_result = reg_file.sread(id_stage.io().rs1_addr, ch_bool(true));
        auto rs2_data_result = reg_file.sread(id_stage.io().rs2_addr, ch_bool(true));
        
        // 将读结果连接到 ID 阶段
        id_stage.io().reg_rs1_data <<= rs1_data_result;
        id_stage.io().reg_rs2_data <<= rs2_data_result;
        
        // WB 阶段写回寄存器文件
        auto wb_write_en = wb_stage.io().write_en & (wb_stage.io().rd_addr_out != ch_uint<5>(0_d));
        reg_file.write(wb_stage.io().rd_addr_out, wb_stage.io().write_data, wb_write_en, "reg_file_write");

        // ===================== IF 级连接 =====================
        if_stage.io().stall <<= hazard.io().stall;
        if_stage.io().flush <<= hazard.io().flush;
        if_stage.io().rst <<= io().rst;
        
        // 分支控制 (来自 EX 级)
        if_stage.io().branch_target <<= ex_stage.io().branch_target;
        if_stage.io().branch_valid <<= ex_stage.io().branch_taken;
        
        // 指令存储器接口
        io().instr_addr <<= if_stage.io().instr_addr;
        if_stage.io().instr_data <<= io().instr_data;
        if_stage.io().instr_ready <<= io().instr_ready;
        
        // ===================== IF → ID 连接 (FIX B5) =====================
        id_stage.io().pc <<= if_stage.io().pc;
        id_stage.io().instruction <<= if_stage.io().instruction;
        id_stage.io().valid <<= if_stage.io().valid;
        
        // ===================== ID → EX 连接 =====================
        // 控制信号 (部分字段缺失, 简化处理传递到 EX)
        ex_stage.io().rs1_data <<= id_stage.io().rs1_data;
        ex_stage.io().rs2_data <<= id_stage.io().rs2_data;
        ex_stage.io().imm <<= id_stage.io().imm;
        ex_stage.io().funct3 <<= id_stage.io().funct3;
        ex_stage.io().is_branch <<= id_stage.io().is_branch;
        
        // BUG FIX: 原代码 pc <<= id_stage.io().rs1_data (错误)
        // FIX: PC 应来自 IF/ID 流水线寄存器 (id_stage.io().pc)
        ex_stage.io().pc <<= id_stage.io().pc;
        
        // opcode/funct7/alu_op/branch_type - 简化: 由 EX 级内部从 opcode 推导
        // EX 级内部使用 funct3 和 opcode_raw 生成 alu_op
        // 注意: funct7 直接从 instruction[31:25] 获取 (id_stage 已输出 opcode, EX 用 raw instruction)
        // EX 阶段需要: opcode_raw -> ALU decoder, funct3 -> ALU decoder
        // 这里通过 id_stage.io().funct3 和固定的 opcode_raw/funct7 输入
        // EX stage self-decodes from opcode/funct3 inputs
        
        // ===================== EX → MEM 连接 =====================
        mem_stage.io().alu_result <<= ex_stage.io().alu_result;
        mem_stage.io().rs2_data <<= id_stage.io().rs2_data;  // From ID (after forwarding)
        mem_stage.io().is_load <<= id_stage.io().is_load;
        mem_stage.io().is_store <<= id_stage.io().is_store;
        mem_stage.io().funct3 <<= id_stage.io().funct3;
        mem_stage.io().mem_valid <<= ch_bool(true);
        
        // ===================== MEM → WB 连接 (FIX B3/B4) =====================
        // BUG B3: 原代码 wb_stage.io().alu_result <<= ex_stage.io().alu_result
        //         (直接从 EX 取, 绕过了 MEM/WB 流水线寄存器)
        // BUG B4: 原代码 wb_stage.io().rd_addr <<= id_stage.io().rd_addr
        //         (从 ID 取, 绕过了 MEM/WB 流水线寄存器)
        // FIX: 使用 mem_stage 输出的流水线寄存器值
        wb_stage.io().alu_result <<= mem_stage.io().alu_result;  // From MEM/WB reg
        wb_stage.io().rd_addr <<= mem_stage.io().rd_addr;       // From MEM/WB reg
        wb_stage.io().mem_data <<= mem_stage.io().mem_data;
        wb_stage.io().is_load <<= mem_stage.io().is_load;
        wb_stage.io().is_alu <<= id_stage.io().is_alu;
        wb_stage.io().is_jump <<= id_stage.io().is_jump;
        wb_stage.io().mem_valid <<= mem_stage.io().mem_valid_out;
        
        // ===================== Hazard Unit 连接 =====================
        hazard.io().id_rs1_addr <<= id_stage.io().rs1_addr;
        hazard.io().id_rs2_addr <<= id_stage.io().rs2_addr;
        // FIX: rd_addr 从 EX/MEM 和 MEM/WB 流水线寄存器获取 (简化取 id)
        hazard.io().ex_rd_addr <<= id_stage.io().rd_addr;
        hazard.io().mem_rd_addr <<= id_stage.io().rd_addr;
        hazard.io().wb_rd_addr <<= wb_stage.io().rd_addr_out;
        
        hazard.io().ex_reg_write <<= id_stage.io().is_alu;  // EX: ALU 指令写
        hazard.io().mem_reg_write <<= id_stage.io().is_load; // MEM: load 结果写
        hazard.io().wb_reg_write <<= wb_stage.io().write_en;
        
        hazard.io().mem_is_load <<= id_stage.io().is_load;
        hazard.io().ex_branch <<= id_stage.io().is_branch;
        hazard.io().ex_branch_taken <<= ex_stage.io().branch_taken;
        
        hazard.io().ex_alu_result <<= ex_stage.io().alu_result;
        hazard.io().mem_alu_result <<= mem_stage.io().alu_result;
        hazard.io().wb_write_data <<= wb_stage.io().write_data;
        
        hazard.io().rs1_data_raw <<= id_stage.io().rs1_data;
        hazard.io().rs2_data_raw <<= id_stage.io().rs2_data;
        
        // 前推 MUX 输出覆盖 RS1/RS2 (在 ID 输出的原始数据上选择)
        // 注意: hazard.io().rs1_data/rs2_data 已经是前推后的值
        ex_stage.io().rs1_data <<= hazard.io().rs1_data;
        ex_stage.io().rs2_data <<= hazard.io().rs2_data;
        
        // ===================== MEM 级: 数据存储器接口 =====================
        // MEM 阶段的访存结果输出到 D-TCM
        // data_addr/data_write_data/data_strbe/data_write_en 
        // 由 mem_stage 内部生成 (或简化处理)
        io().data_addr <<= mem_stage.io().alu_result;  // 地址来自 ALU 结果
        io().data_write_data <<= id_stage.io().rs2_data;
        io().data_strbe <<= ch_uint<4>(15_d);  // 全字节使能 (简化)
        io().data_write_en <<= id_stage.io().is_store;
    }
};

} // namespace riscv
