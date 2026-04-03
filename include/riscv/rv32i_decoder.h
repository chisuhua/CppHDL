/**
 * @file rv32i_decoder.h
 * @brief RV32I 指令译码器
 * 
 * 支持指令格式:
 * - R-type: 寄存器 - 寄存器操作
 * - I-type: 立即数操作/加载
 * - S-type: 存储
 * - B-type: 分支
 * - U-type: 上位立即数
 * - J-type: 跳转
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

// 指令字段结构
struct RvInstrFields {
    ch_uint<7> opcode;
    ch_uint<5> rd;
    ch_uint<3> funct3;
    ch_uint<5> rs1;
    ch_uint<5> rs2;
    ch_uint<7> funct7;
    ch_uint<32> imm;  // 符号扩展后的立即数
};

// 指令类型枚举
constexpr unsigned INSTR_R = 0;
constexpr unsigned INSTR_I = 1;
constexpr unsigned INSTR_S = 2;
constexpr unsigned INSTR_B = 3;
constexpr unsigned INSTR_U = 4;
constexpr unsigned INSTR_J = 5;

// Opcode 定义
constexpr unsigned OP_LOAD   = 0b0000011;
constexpr unsigned OP_STORE  = 0b0100011;
constexpr unsigned OP_BRANCH = 0b1100011;
constexpr unsigned OP_JALR   = 0b1100111;
constexpr unsigned OP_JAL    = 0b1101111;
constexpr unsigned OP_LUI    = 0b0110111;
constexpr unsigned OP_AUIPC  = 0b0010111;
constexpr unsigned OP_OPIMM  = 0b0010011;
constexpr unsigned OP_OP     = 0b0110011;
constexpr unsigned OP_SYSTEM = 0b1110011;

class Rv32iDecoder : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> instruction;
        ch_out<ch_uint<7>>  opcode;
        ch_out<ch_uint<5>>  rd;
        ch_out<ch_uint<3>>  funct3;
        ch_out<ch_uint<5>>  rs1;
        ch_out<ch_uint<5>>  rs2;
        ch_out<ch_uint<7>>  funct7;
        ch_out<ch_uint<32>> imm;       // 符号扩展立即数
        ch_out<ch_uint<3>>  instr_type; // 指令类型
    )
    
    Rv32iDecoder(ch::Component* parent = nullptr, const std::string& name = "rv32i_decoder")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 提取指令字段 (组合逻辑)
        // ========================================================================
        
        ch_uint<7>  op(0_d);
        ch_uint<5>  dest(0_d);
        ch_uint<3>  f3(0_d);
        ch_uint<5>  src1(0_d);
        ch_uint<5>  src2(0_d);
        ch_uint<7>  f7(0_d);
        ch_uint<32> imm_val(0_d);
        ch_uint<3>  type(INSTR_R);
        
        // 提取固定字段
        op = bits<6, 0>(io().instruction);
        dest = bits<11, 7>(io().instruction);
        f3 = bits<14, 12>(io().instruction);
        src1 = bits<19, 15>(io().instruction);
        src2 = bits<24, 20>(io().instruction);
        f7 = bits<31, 25>(io().instruction);
        
        // ========================================================================
        // 立即数生成 (根据指令类型)
        // ========================================================================
        
        // I-type: imm[11:0] = instr[31:20], sign-extended to 32 bits
        ch_uint<32> imm_i = sext<32>(bits<31, 20>(io().instruction));
        
        // S-type: imm[11:0] = instr[31:25] << 5 | instr[11:7], sign-extended
        ch_uint<12> imm_s_base = (bits<31, 25>(io().instruction) << ch_uint<7>(5_d)) | bits<11, 7>(io().instruction);
        ch_uint<32> imm_s = sext<32>(imm_s_base);
        
        // B-type: imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7], sign-extended
        ch_uint<13> imm_b_base = 
            (bits<31, 31>(io().instruction) << ch_uint<12>(12_d)) |  // imm[12]
            (bits<30, 25>(io().instruction) << ch_uint<6>(5_d)) |    // imm[10:5]
            (bits<11, 8>(io().instruction) << ch_uint<4>(1_d)) |     // imm[4:1]
            bits<7, 7>(io().instruction);                            // imm[11]
        ch_uint<32> imm_b = sext<32>(imm_b_base);
        
        // U-type: imm[31:12] = instr[31:12]
        ch_uint<32> imm_u = io().instruction & ch_uint<32>(0xFFFFF000);
        
        // J-type: imm[20|10:1|11|19:12] = instr[31|30:21|20|19:12], sign-extended
        ch_uint<21> imm_j_base = 
            (bits<31, 31>(io().instruction) << ch_uint<20>(20_d)) |  // imm[20]
            (bits<30, 21>(io().instruction) << ch_uint<10>(1_d)) |   // imm[10:1]
            (bits<20, 20>(io().instruction) << ch_uint<11>(11_d)) |  // imm[11]
            (bits<19, 12>(io().instruction) << ch_uint<12>(12_d));   // imm[19:12]
        ch_uint<32> imm_j = sext<32>(imm_j_base);
        
        // 根据 opcode 选择立即数和指令类型
        imm_val = select(op == ch_uint<7>(OP_OPIMM),  imm_i, imm_val);
        imm_val = select(op == ch_uint<7>(OP_LOAD),   imm_i, imm_val);
        imm_val = select(op == ch_uint<7>(OP_JALR),   imm_i, imm_val);
        imm_val = select(op == ch_uint<7>(OP_STORE),  imm_s, imm_val);
        imm_val = select(op == ch_uint<7>(OP_BRANCH), imm_b, imm_val);
        imm_val = select(op == ch_uint<7>(OP_LUI),    imm_u, imm_val);
        imm_val = select(op == ch_uint<7>(OP_AUIPC),  imm_u, imm_val);
        imm_val = select(op == ch_uint<7>(OP_JAL),    imm_j, imm_val);
        
        type = select(op == ch_uint<7>(OP_OP),    ch_uint<3>(INSTR_R), type);
        type = select(op == ch_uint<7>(OP_OPIMM), ch_uint<3>(INSTR_I), type);
        type = select(op == ch_uint<7>(OP_LOAD),  ch_uint<3>(INSTR_I), type);
        type = select(op == ch_uint<7>(OP_JALR),  ch_uint<3>(INSTR_I), type);
        type = select(op == ch_uint<7>(OP_STORE), ch_uint<3>(INSTR_S), type);
        type = select(op == ch_uint<7>(OP_BRANCH), ch_uint<3>(INSTR_B), type);
        type = select(op == ch_uint<7>(OP_LUI),   ch_uint<3>(INSTR_U), type);
        type = select(op == ch_uint<7>(OP_AUIPC), ch_uint<3>(INSTR_U), type);
        type = select(op == ch_uint<7>(OP_JAL),   ch_uint<3>(INSTR_J), type);
        
        // ========================================================================
        // 输出
        // ========================================================================
        io().opcode = op;
        io().rd = dest;
        io().funct3 = f3;
        io().rs1 = src1;
        io().rs2 = src2;
        io().funct7 = f7;
        io().imm = imm_val;
        io().instr_type = type;
    }
};

} // namespace riscv
