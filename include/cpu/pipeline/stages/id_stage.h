/**
 * @file id_stage.h
 * @brief RISC-V 5 级流水线 - 译码级 (Instruction Decode)
 * 
 * 功能:
 * - 指令译码 (R/I/S/B/U/J 型)
 * - 寄存器文件读 (2 个读端口)
 * - 符号/零扩展
 * - 控制信号生成
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "rv32i_decoder.h"

using namespace ch::core;

namespace riscv {

/**
 * @brief ID 级 (译码级)
 * 
 * 从 IF 级接收指令，译码后传递给 EX 级
 */
class IdStage : public ch::Component {
public:
    __io(
        // 输入 (来自 IF 级)
        ch_in<ch_uint<32>>  pc;             // IF/ID PC (用于分支目标计算)
        ch_in<ch_uint<32>>  instruction;    // 32 位指令
        ch_in<ch_bool>      valid;          // 指令有效
        
        // 输入 (来自 MEM/WB 流水线寄存器 - 写回)
        ch_in<ch_uint<5>>   wb_rd_addr;    // 写回目标寄存器地址
        ch_in<ch_uint<32>>  wb_write_data;  // 写回数据
        ch_in<ch_bool>       wb_reg_write;  // 写回使能
        
        // 输出 (到 EX 级)
        ch_out<ch_uint<5>>  rs1_addr;       // RS1 地址
        ch_out<ch_uint<5>>  rs2_addr;       // RS2 地址
        ch_out<ch_uint<32>> rs1_data;       // RS1 数据
        ch_out<ch_uint<32>> rs2_data;       // RS2 数据
        ch_out<ch_uint<32>> imm;            // 立即数 (符号扩展)
        ch_out<ch_uint<4>>  opcode;         // 操作码
        ch_out<ch_uint<3>>  funct3;         // funct3 字段
        ch_out<ch_bool>     is_branch;      // 分支指令
        ch_out<ch_bool>     is_load;        // 加载指令
        ch_out<ch_bool>     is_store;       // 存储指令
        ch_out<ch_bool>     is_jump;        // 跳转指令
        ch_out<ch_bool>     is_alu;         // ALU 指令
        ch_out<ch_uint<5>>  rd_addr;        // 目的寄存器地址
    )
    
    IdStage(ch::Component* parent = nullptr, const std::string& name = "id_stage")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 指令字段提取 (组合逻辑)
        // ========================================================================
        // 从指令中提取各个字段
        ch_uint<7>  opcode_raw(0_d, "opcode_raw");
        ch_uint<5>  rd(0_d, "rd");
        ch_uint<3>  funct3(0_d, "funct3");
        ch_uint<5>  rs1(0_d, "rs1");
        ch_uint<5>  rs2(0_d, "rs2");
        ch_uint<7>  funct7(0_d, "funct7");
        
        // 提取固定位字段
        opcode_raw = bits<6, 0>(io().instruction);
        rd         = bits<11, 7>(io().instruction);
        funct3     = bits<14, 12>(io().instruction);
        rs1        = bits<19, 15>(io().instruction);
        rs2        = bits<24, 20>(io().instruction);
        funct7     = bits<31, 25>(io().instruction);
        
        // ========================================================================
        // 指令类型检测 (组合逻辑)
        // ========================================================================
        // 检测指令类型 (opcode 比较)
        auto is_op    = (opcode_raw == ch_uint<7>(OP_OP));
        auto is_opimm = (opcode_raw == ch_uint<7>(OP_OPIMM));
        auto is_load  = (opcode_raw == ch_uint<7>(OP_LOAD));
        auto is_store = (opcode_raw == ch_uint<7>(OP_STORE));
        auto is_branch= (opcode_raw == ch_uint<7>(OP_BRANCH));
        auto is_jalr  = (opcode_raw == ch_uint<7>(OP_JALR));
        auto is_jal   = (opcode_raw == ch_uint<7>(OP_JAL));
        auto is_lui   = (opcode_raw == ch_uint<7>(OP_LUI));
        auto is_auipc = (opcode_raw == ch_uint<7>(OP_AUIPC));
        
        // ========================================================================
        // 立即数生成 (组合逻辑)
        // ========================================================================
        // I-type 立即数：instr[31:20] 符号扩展到 32 位
        ch_uint<32> imm_i = sext<32>(bits<31, 20>(io().instruction));
        
        // S-type 立即数：instr[31:25] << 5 | instr[11:7], 符号扩展
        ch_uint<12> imm_s_base = 
            (bits<31, 25>(io().instruction) << ch_uint<7>(5_d)) | 
            bits<11, 7>(io().instruction);
        ch_uint<32> imm_s = sext<32>(imm_s_base);
        
        // B-type 立即数：instr[31|30:25|11:8|7], 符号扩展
        ch_uint<13> imm_b_base = 
            (bits<31, 31>(io().instruction) << ch_uint<12>(12_d)) |  // imm[12]
            (bits<30, 25>(io().instruction) << ch_uint<6>(5_d)) |    // imm[10:5]
            (bits<11, 8>(io().instruction) << ch_uint<4>(1_d)) |     // imm[4:1]
            bits<7, 7>(io().instruction);                            // imm[11]
        ch_uint<32> imm_b = sext<32>(imm_b_base);
        
        // U-type 立即数：instr[31:12] 直接作为高 20 位
        // 使用位掩码 0xFFFFF (十进制 1048575) 提取指令低 20 位
        ch_uint<32> instr_masked = io().instruction & ch_uint<32>(1048575_d);  // 0xFFFFF = 1048575
        ch_uint<32> imm_u = instr_masked << ch_uint<32>(12_d);  // 左移 12 位得到立即数
        
        // J-type 立即数：instr[31|30:21|20|19:12], 符号扩展
        ch_uint<21> imm_j_base = 
            (bits<31, 31>(io().instruction) << ch_uint<20>(20_d)) |  // imm[20]
            (bits<30, 21>(io().instruction) << ch_uint<10>(1_d)) |   // imm[10:1]
            (bits<20, 20>(io().instruction) << ch_uint<11>(11_d)) |  // imm[11]
            (bits<19, 12>(io().instruction) << ch_uint<12>(12_d));   // imm[19:12]
        ch_uint<32> imm_j = sext<32>(imm_j_base);
        
        // 根据指令类型选择立即数
        ch_uint<32> selected_imm(0_d, "selected_imm");
        selected_imm = select(is_opimm,   imm_i, selected_imm);
        selected_imm = select(is_load,    imm_i, selected_imm);
        selected_imm = select(is_jalr,    imm_i, selected_imm);
        selected_imm = select(is_store,   imm_s, selected_imm);
        selected_imm = select(is_branch,  imm_b, selected_imm);
        selected_imm = select(is_lui,     imm_u, selected_imm);
        selected_imm = select(is_auipc,   imm_u, selected_imm);
        selected_imm = select(is_jal,     imm_j, selected_imm);
        
        // ========================================================================
        // 寄存器文件 (32x32 bit, 2 读端口)
        // ========================================================================
        // 使用 ch_mem 实现寄存器文件
        // 注意：实际 RISC-V 寄存器 x0 硬连线为 0
        ch_mem<ch_uint<32>, 32> reg_file("reg_file");
        
        // 写端口 (来自 WB 级 - 寄存器写回)
        // x0 寄存器硬连线为 0，禁止写入
        auto wb_write_en = io().wb_reg_write & (io().wb_rd_addr != ch_uint<5>(0_d));
        reg_file.write(io().wb_rd_addr, io().wb_write_data, wb_write_en, "reg_file_write");
        
        // 读端口 A (组合逻辑) - rs1
        auto rs1_addr_mux = select(
            is_op || is_opimm || is_load || is_store || is_branch || is_jalr,
            rs1,
            ch_uint<5>(0_d)  // 对于 LUI/AUIPC/JAL，不使用 rs1
        );
        auto rs1_data_raw = reg_file.sread(rs1_addr_mux, ch_bool(true));
        
        // 读端口 B (组合逻辑) - rs2
        auto rs2_addr_mux = select(
            is_op || is_store || is_branch,
            rs2,
            ch_uint<5>(0_d)  // 对于其他指令，不使用 rs2
        );
        auto rs2_data_raw = reg_file.sread(rs2_addr_mux, ch_bool(true));
        
        // ========================================================================
        // 控制信号生成 (组合逻辑)
        // ========================================================================
        // 分支指令标志
        ch_bool branch_flag = is_branch;
        
        // 加载指令标志
        ch_bool load_flag = is_load;
        
        // 存储指令标志
        ch_bool store_flag = is_store;
        
        // 跳转指令标志
        ch_bool jump_flag = is_jal || is_jalr;
        
        // ALU 指令标志 (R-type, I-type, LUI, AUIPC)
        ch_bool alu_flag = is_op || is_opimm || is_lui || is_auipc;
        
        // ========================================================================
        // ID/EX 流水线寄存器
        // ========================================================================
        // 存储译码结果，传递给 EX 级
        ch_reg<ch_uint<5>>  idex_rs1_addr(0_d, "idex_rs1_addr");
        ch_reg<ch_uint<5>>  idex_rs2_addr(0_d, "idex_rs2_addr");
        ch_reg<ch_uint<32>> idex_rs1_data(0_d, "idex_rs1_data");
        ch_reg<ch_uint<32>> idex_rs2_data(0_d, "idex_rs2_data");
        ch_reg<ch_uint<32>> idex_imm(0_d, "idex_imm");
        ch_reg<ch_uint<7>>  idex_opcode(0_d, "idex_opcode");
        ch_reg<ch_uint<3>>  idex_funct3(0_d, "idex_funct3");
        ch_reg<ch_bool>     idex_is_branch(false, "idex_is_branch");
        ch_reg<ch_bool>     idex_is_load(false, "idex_is_load");
        ch_reg<ch_bool>     idex_is_store(false, "idex_is_store");
        ch_reg<ch_bool>     idex_is_jump(false, "idex_is_jump");
        ch_reg<ch_bool>     idex_is_alu(false, "idex_is_alu");
        ch_reg<ch_uint<5>>  idex_rd_addr(0_d, "idex_rd_addr");
        ch_reg<ch_bool>     idex_valid(false, "idex_valid");
        
        // ID/EX 寄存器更新使能
        // 当输入有效且有效信号稳定时更新
        auto idex_update_en = io().valid;
        
        // 更新寄存器
        idex_rs1_addr->next = select(idex_update_en, rs1_addr_mux, idex_rs1_addr);
        idex_rs2_addr->next = select(idex_update_en, rs2_addr_mux, idex_rs2_addr);
        idex_rs1_data->next = select(idex_update_en, rs1_data_raw, idex_rs1_data);
        idex_rs2_data->next = select(idex_update_en, rs2_data_raw, idex_rs2_data);
        idex_imm->next      = select(idex_update_en, selected_imm, idex_imm);
        idex_opcode->next   = select(idex_update_en, opcode_raw, idex_opcode);
        idex_funct3->next   = select(idex_update_en, funct3, idex_funct3);
        idex_is_branch->next= select(idex_update_en, branch_flag, idex_is_branch);
        idex_is_load->next  = select(idex_update_en, load_flag, idex_is_load);
        idex_is_store->next = select(idex_update_en, store_flag, idex_is_store);
        idex_is_jump->next  = select(idex_update_en, jump_flag, idex_is_jump);
        idex_is_alu->next   = select(idex_update_en, alu_flag, idex_is_alu);
        idex_rd_addr->next  = select(idex_update_en, rd, idex_rd_addr);
        idex_valid->next    = select(idex_update_en, io().valid, idex_valid);
        
        // ========================================================================
        // 输出到 EX 级
        // ========================================================================
        io().rs1_addr  = idex_rs1_addr;
        io().rs2_addr  = idex_rs2_addr;
        io().rs1_data  = idex_rs1_data;
        io().rs2_data  = idex_rs2_data;
        io().imm       = idex_imm;
        io().opcode    = idex_opcode;
        io().funct3    = idex_funct3;
        io().is_branch = idex_is_branch;
        io().is_load   = idex_is_load;
        io().is_store  = idex_is_store;
        io().is_jump   = idex_is_jump;
        io().is_alu    = idex_is_alu;
        io().rd_addr   = idex_rd_addr;
        io().valid     = idex_valid;
    }
};

} // namespace riscv
