/**
 * @file rv32i_core.h
 * @brief RV32I 处理器核心 (Phase 1: 基础版本)
 * 
 * 支持指令:
 * - R-type: ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
 * - I-type: ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI
 * - U-type: LUI, AUIPC
 * 
 * Phase 1 目标: 支持计算指令，可运行简单循环
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "riscv/rv32i_regfile.h"
#include "riscv/rv32i_alu.h"
#include "riscv/rv32i_decoder.h"
#include "riscv/rv32i_pc.h"

using namespace ch::core;

namespace riscv {

// 控制信号结构
struct RvControlSignals {
    ch_bool reg_write;
    ch_bool alu_src;      // 0=寄存器，1=立即数
    ch_bool branch;       // 分支使能
    ch_bool jump;         // 跳转使能
    ch_uint<4> alu_op;    // ALU 操作码
};

class Rv32iCore : public ch::Component {
public:
    __io(
        // AXI4 指令接口 (简化版)
        ch_in<ch_uint<32>>  instr_data;     // 从 I-TCM 读取的指令
        ch_in<ch_bool>      instr_ready;    // 指令存储器就绪
        ch_out<ch_uint<32>> instr_addr;     // 指令地址
        ch_out<ch_bool>     instr_valid;    // 指令请求有效
        
        // AXI4 数据接口 (简化版)
        ch_in<ch_uint<32>>  data_rdata;     // 从 D-TCM 读取的数据
        ch_in<ch_bool>      data_ready;     // 数据存储器就绪
        ch_out<ch_uint<32>> data_addr;      // 数据地址
        ch_out<ch_uint<32>> data_wdata;     // 写入数据
        ch_out<ch_bool>     data_read;      // 读请求
        ch_out<ch_bool>     data_write;     // 写请求
        ch_out<ch_uint<4>>  data_strbe;     // 字节使能
        
        // 控制
        ch_in<ch_bool>      clk;
        ch_in<ch_bool>      rst;
        ch_out<ch_bool>     running;        // CPU 运行状态
    )
    
    Rv32iCore(ch::Component* parent = nullptr, const std::string& name = "rv32i_core")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 程序计数器
        // ========================================================================
        ch::ch_module<Rv32iPc> pc_unit{"pc_unit"};
        pc_unit.io().rst = rst();
        pc_unit.io().clk = clk();
        
        // ========================================================================
        // 指令锁存器 (模拟取指延迟)
        // ========================================================================
        ch_reg<ch_uint<32>> current_instr(0_d);
        ch_reg<ch_bool>     instr_valid_reg(false);
        
        // 取指请求
        instr_addr() = pc_unit.io().pc;
        instr_valid() = !instr_valid_reg;  // 需要新指令时请求
        
        // 锁存指令
        auto fetch_done = instr_valid() && instr_ready;
        current_instr->next = select(fetch_done, instr_data, current_instr);
        instr_valid_reg->next = select(fetch_done, ch_bool(true), 
                                  select(instr_valid_reg, ch_bool(true), ch_bool(false)));
        
        // ========================================================================
        // 指令译码器
        // ========================================================================
        ch::ch_module<Rv32iDecoder> decoder{"decoder"};
        decoder.io().instruction = current_instr;
        
        // ========================================================================
        // 寄存器文件
        // ========================================================================
        ch::ch_module<Rv32iRegFile> regfile{"regfile"};
        regfile.io().clk = clk();
        regfile.io().rst = rst();
        regfile.io().rs1_addr = decoder.io().rs1;
        regfile.io().rs2_addr = decoder.io().rs2;
        
        // ========================================================================
        // ALU
        // ========================================================================
        ch::ch_module<Rv32iAlu> alu{"alu"};
        
        // ALU 操作数 A
        ch_uint<32> alu_a(0_d);
        alu_a = regfile.io().rs1_data;
        
        // ALU 操作数 B (寄存器或立即数)
        ch_uint<32> alu_b(0_d);
        
        // 根据指令类型选择 alu_b
        auto is_i_type = (decoder.io().instr_type == ch_uint<3>(INSTR_I));
        auto is_u_type = (decoder.io().instr_type == ch_uint<3>(INSTR_U));
        auto is_b_type = (decoder.io().instr_type == ch_uint<3>(INSTR_B));  // 分支指令也需要立即数
        alu_b = select(is_i_type || is_u_type || is_b_type, decoder.io().imm, regfile.io().rs2_data);
        
        alu.io().operand_a = alu_a;
        alu.io().operand_b = alu_b;
        
        // ========================================================================
        // ALU 操作码生成
        // ========================================================================
        ch_uint<4> alu_op(0_d);
        
        // R-type: 根据 funct3 和 funct7 确定
        auto is_r_type = (decoder.io().instr_type == ch_uint<3>(INSTR_R));
        auto funct3 = decoder.io().funct3;
        auto funct7 = decoder.io().funct7;
        
        // ADD/SUB: funct3=000, funct7[5]=0->ADD, =1->SUB
        auto is_add = (funct3 == ch_uint<3>(0_d)) && (bits<5, 5>(funct7) == ch_bool(false));
        auto is_sub = (funct3 == ch_uint<3>(0_d)) && (bits<5, 5>(funct7) == ch_bool(true));
        
        alu_op = select(is_add, ch_uint<4>(ALU_ADD), alu_op);
        alu_op = select(is_sub, ch_uint<4>(ALU_SUB), alu_op);
        alu_op = select((funct3 == ch_uint<3>(1_d)) && is_r_type, ch_uint<4>(ALU_SLL), alu_op);
        alu_op = select((funct3 == ch_uint<3>(2_d)) && is_r_type, ch_uint<4>(ALU_SLT), alu_op);
        alu_op = select((funct3 == ch_uint<3>(3_d)) && is_r_type, ch_uint<4>(ALU_SLTU), alu_op);
        alu_op = select((funct3 == ch_uint<3>(4_d)) && is_r_type, ch_uint<4>(ALU_XOR), alu_op);
        alu_op = select((funct3 == ch_uint<3>(5_d)) && (bits<5, 5>(funct7) == ch_bool(false)), ch_uint<4>(ALU_SRL), alu_op);
        alu_op = select((funct3 == ch_uint<3>(5_d)) && (bits<5, 5>(funct7) == ch_bool(true)), ch_uint<4>(ALU_SRA), alu_op);
        alu_op = select((funct3 == ch_uint<3>(6_d)) && is_r_type, ch_uint<4>(ALU_OR), alu_op);
        alu_op = select((funct3 == ch_uint<3>(7_d)) && is_r_type, ch_uint<4>(ALU_AND), alu_op);
        
        // I-type ALU 操作
        auto is_opimm = (decoder.io().opcode == ch_uint<7>(OP_OPIMM));
        alu_op = select((funct3 == ch_uint<3>(0_d)) && is_opimm, ch_uint<4>(ALU_ADD), alu_op);
        alu_op = select((funct3 == ch_uint<3>(1_d)) && is_opimm, ch_uint<4>(ALU_SLL), alu_op);
        alu_op = select((funct3 == ch_uint<3>(2_d)) && is_opimm, ch_uint<4>(ALU_SLT), alu_op);
        alu_op = select((funct3 == ch_uint<3>(3_d)) && is_opimm, ch_uint<4>(ALU_SLTU), alu_op);
        alu_op = select((funct3 == ch_uint<3>(4_d)) && is_opimm, ch_uint<4>(ALU_XOR), alu_op);
        alu_op = select((funct3 == ch_uint<3>(5_d)) && is_opimm, ch_uint<4>(ALU_SRL), alu_op);
        alu_op = select((funct3 == ch_uint<3>(6_d)) && is_opimm, ch_uint<4>(ALU_OR), alu_op);
        alu_op = select((funct3 == ch_uint<3>(7_d)) && is_opimm, ch_uint<4>(ALU_AND), alu_op);
        
        // LUI/AUIPC
        auto is_lui = (decoder.io().opcode == ch_uint<7>(OP_LUI));
        auto is_auipc = (decoder.io().opcode == ch_uint<7>(OP_AUIPC));
        alu_op = select(is_lui, ch_uint<4>(ALU_ADD), alu_op);  // LUI 直接使用 imm
        alu_op = select(is_auipc, ch_uint<4>(ALU_ADD), alu_op);  // AUIPC: PC + imm
        
        alu.io().alu_op = alu_op;
        
        // ========================================================================
        // 写回数据选择
        // ========================================================================
        ch_uint<32> write_data(0_d);
        
        // 默认 ALU 结果
        write_data = alu.io().result;
        
        // LUI: 直接使用 imm
        write_data = select(is_lui, decoder.io().imm, write_data);
        
        // AUIPC: PC + imm
        auto auipc_result = pc_unit.io().pc + decoder.io().imm;
        write_data = select(is_auipc, auipc_result, write_data);
        
        // ========================================================================
        // LOAD: 从内存读取数据并处理 (Phase 3 实现)
        // ========================================================================
        ch_bool is_load = (decoder.io().opcode == ch_uint<7>(OP_LOAD));
        ch_uint<32> load_result(0_d);
        
        // 从 D-TCM 读取的原始数据
        ch_uint<32> mem_raw_data = data_rdata;
        
        // 根据地址低 2 位选择字节/半字位置
        auto addr_low_2 = bits<1, 0>(alu.io().result);
        
        // LB: 有符号字节加载
        auto lb_byte = bits<7, 0>(mem_raw_data >> (addr_low_2 * ch_uint<2>(8_d)));
        auto lb_result = sext<32>(lb_byte);
        
        // LH: 有符号半字加载
        auto lh_half_sel = bits<1, 1>(alu.io().result);
        auto lh_half = bits<15, 0>(mem_raw_data >> (lh_half_sel * ch_uint<2>(16_d)));
        auto lh_result = sext<32>(lh_half);
        
        // LW: 字加载 (直接赋值)
        auto lw_result = mem_raw_data;
        
        // LBU: 无符号字节加载 (零扩展)
        auto lbu_byte = bits<7, 0>(mem_raw_data >> (addr_low_2 * ch_uint<2>(8_d)));
        auto lbu_result = lbu_byte;  // 零扩展 (高位自动补 0)
        
        // LHU: 无符号半字加载 (零扩展)
        auto lhu_half_sel = bits<1, 1>(alu.io().result);
        auto lhu_half = bits<15, 0>(mem_raw_data >> (lhu_half_sel * ch_uint<2>(16_d)));
        auto lhu_result = lhu_half;  // 零扩展 (高位自动补 0)
        
        // 根据 funct3 选择加载结果
        load_result = select((decoder.io().funct3 == ch_uint<3>(0_d)), lb_result, load_result);   // LB
        load_result = select((decoder.io().funct3 == ch_uint<3>(1_d)), lh_result, load_result);   // LH
        load_result = select((decoder.io().funct3 == ch_uint<3>(2_d)), lw_result, load_result);   // LW
        load_result = select((decoder.io().funct3 == ch_uint<3>(4_d)), lbu_result, load_result);  // LBU
        load_result = select((decoder.io().funct3 == ch_uint<3>(5_d)), lhu_result, load_result);  // LHU
        
        write_data = select(is_load, load_result, write_data);
        
        // JAL: 写入返回地址 (PC+4)
        auto jal_return = pc_unit.io().pc + ch_uint<32>(4_d);
        write_data = select((decoder.io().opcode == ch_uint<7>(OP_JAL)), jal_return, write_data);
        write_data = select((decoder.io().opcode == ch_uint<7>(OP_JALR)), jal_return, write_data);
        
        // ========================================================================
        // 寄存器写使能
        // ========================================================================
        ch_bool reg_we(false);
        
        // R-type/I-type 写寄存器
        reg_we = select(is_r_type, ch_bool(true), reg_we);
        reg_we = select(is_opimm, ch_bool(true), reg_we);
        reg_we = select(is_lui, ch_bool(true), reg_we);
        reg_we = select(is_auipc, ch_bool(true), reg_we);
        reg_we = select((decoder.io().opcode == ch_uint<7>(OP_JAL)), ch_bool(true), reg_we);
        reg_we = select((decoder.io().opcode == ch_uint<7>(OP_JALR)), ch_bool(true), reg_we);
        reg_we = select((decoder.io().opcode == ch_uint<7>(OP_LOAD)), ch_bool(true), reg_we);
        
        // x0 不能写
        reg_we = reg_we && (decoder.io().rd != ch_uint<5>(0_d));
        
        regfile.io().rd_addr = decoder.io().rd;
        regfile.io().rd_data = write_data;
        regfile.io().rd_we = reg_we;
        
        // ========================================================================
        // PC 更新逻辑
        // ========================================================================
        ch_uint<32> jump_target(0_d);
        ch_bool jump_enable(false);
        
        // JAL: PC + imm
        auto jal_target = pc_unit.io().pc + decoder.io().imm;
        jump_target = select((decoder.io().opcode == ch_uint<7>(OP_JAL)), jal_target, jump_target);
        
        // JALR: (rs1 + imm) & ~1 (最后一位强制为 0)
        auto jalr_target = (regfile.io().rs1_data + decoder.io().imm) & ch_uint<32>(0xFFFFFFFE_d);
        jump_target = select((decoder.io().opcode == ch_uint<7>(OP_JALR)), jalr_target, jump_target);
        
        // ========================================================================
        // 分支条件判断 (使用 ALU 比较标志)
        // ========================================================================
        // BRANCH: PC + imm (条件满足时)
        auto branch_target = pc_unit.io().pc + decoder.io().imm;
        
        // 从 ALU 获取比较结果
        auto branch_eq = alu.io().equal;        // rs1 == rs2
        auto branch_neq = !alu.io().equal;      // rs1 != rs2
        auto branch_lt = alu.io().less_than;    // rs1 < rs2 (有符号)
        auto branch_lt_u = alu.io().less_than_u; // rs1 < rs2 (无符号)
        
        // 根据 funct3 选择分支条件
        ch_bool branch_taken(false);
        branch_taken = select((decoder.io().funct3 == ch_uint<3>(0b000_d)), branch_eq, branch_taken);   // BEQ
        branch_taken = select((decoder.io().funct3 == ch_uint<3>(0b001_d)), branch_neq, branch_taken);  // BNE
        branch_taken = select((decoder.io().funct3 == ch_uint<3>(0b100_d)), branch_lt, branch_taken);   // BLT
        branch_taken = select((decoder.io().funct3 == ch_uint<3>(0b101_d)), !branch_lt, branch_taken);  // BGE
        branch_taken = select((decoder.io().funct3 == ch_uint<3>(0b110_d)), branch_lt_u, branch_taken); // BLTU
        branch_taken = select((decoder.io().funct3 == ch_uint<3>(0b111_d)), !branch_lt_u, branch_taken);// BGEU
        
        // 分支跳转目标和使能
        jump_target = select((decoder.io().opcode == ch_uint<7>(OP_BRANCH)), branch_target, jump_target);
        jump_enable = select((decoder.io().opcode == ch_uint<7>(OP_BRANCH)), branch_taken, jump_enable);
        jump_enable = select((decoder.io().opcode == ch_uint<7>(OP_JAL)), ch_bool(true), jump_enable);
        jump_enable = select((decoder.io().opcode == ch_uint<7>(OP_JALR)), ch_bool(true), jump_enable);
        
        pc_unit.io().jump_target = jump_target;
        pc_unit.io().jump_enable = jump_enable;
        
        // ========================================================================
        // 内存接口 (Phase 3 实现)
        // ========================================================================
        ch_bool is_store = (decoder.io().opcode == ch_uint<7>(OP_STORE));
        
        // 存储使能信号和数据处理
        ch_bool store_enable(false);
        ch_uint<4> store_strb(0_d);  // 字节使能
        ch_uint<32> store_data(0_d); // 存储数据
        
        // 地址低 2 位用于字节/半字选择
        auto store_addr_low_2 = bits<1, 0>(alu.io().result);
        auto store_half_sel = bits<1, 1>(alu.io().result);
        
        // SB: 字节存储
        auto sb_strb = ch_uint<4>(1_d) << store_addr_low_2;
        auto sb_data = regfile.io().rs2_data << (store_addr_low_2 * ch_uint<2>(8_d));
        
        // SH: 半字存储
        auto sh_strb = ch_uint<4>(3_d) << (store_half_sel * ch_uint<2>(2_d));
        auto sh_data = regfile.io().rs2_data << (store_half_sel * ch_uint<2>(16_d));
        
        // SW: 字存储
        auto sw_strb = ch_uint<4>(15_d);
        auto sw_data = regfile.io().rs2_data;
        
        // 根据 funct3 选择存储数据
        store_data = select((decoder.io().funct3 == ch_uint<3>(0_d)), sb_data, store_data);  // SB
        store_data = select((decoder.io().funct3 == ch_uint<3>(1_d)), sh_data, store_data);  // SH
        store_data = select((decoder.io().funct3 == ch_uint<3>(2_d)), sw_data, store_data);  // SW
        
        // 生成字节使能
        store_strb = select((decoder.io().funct3 == ch_uint<3>(0_d)), sb_strb, store_strb);  // SB
        store_strb = select((decoder.io().funct3 == ch_uint<3>(1_d)), sh_strb, store_strb);  // SH
        store_strb = select((decoder.io().funct3 == ch_uint<3>(2_d)), sw_strb, store_strb);  // SW
        
        // 存储使能
        store_enable = select((decoder.io().funct3 == ch_uint<3>(0_d)), ch_bool(true), store_enable);  // SB
        store_enable = select((decoder.io().funct3 == ch_uint<3>(1_d)), ch_bool(true), store_enable);  // SH
        store_enable = select((decoder.io().funct3 == ch_uint<3>(2_d)), ch_bool(true), store_enable);  // SW
        
        // 内存地址和数据
        data_addr() = alu.io().result;
        data_wdata() = select(is_store, store_data, regfile.io().rs2_data);
        
        // 读写控制
        data_read() = select(is_load, ch_bool(true), ch_bool(false));
        data_write() = select(is_store, ch_bool(true), ch_bool(false));
        data_strbe() = select(is_store, store_strb, ch_uint<4>(0xF_d));
        
        // ========================================================================
        // 运行状态
        // ========================================================================
        running() = !rst();
    }
};

} // namespace riscv
