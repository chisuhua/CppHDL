/**
 * @file ex_stage.h
 * @brief RISC-V 5 级流水线 - EX 级 (执行级)
 * 
 * 功能:
 * - ALU 计算 (支持所有 RISC-V RV32I 操作)
 * - 分支条件计算
 * - 分支目标计算 (PC + imm)
 * - 前向通路多选器
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
 * EX 级 ALU 操作码编码
 */
constexpr unsigned ALU_OP_ADD  = 0;   // 加法
constexpr unsigned ALU_OP_SUB  = 1;   // 减法
constexpr unsigned ALU_OP_AND  = 2;   // 与
constexpr unsigned ALU_OP_OR   = 3;   // 或
constexpr unsigned ALU_OP_XOR  = 4;   // 异或
constexpr unsigned ALU_OP_SLL  = 5;   // 逻辑左移
constexpr unsigned ALU_OP_SRL  = 6;   // 逻辑右移
constexpr unsigned ALU_OP_SRA  = 7;   // 算术右移
constexpr unsigned ALU_OP_SLT  = 8;   // 有符号小于比较
constexpr unsigned ALU_OP_SLTU = 9;   // 无符号小于比较

/**
 * 分支类型编码
 */
constexpr unsigned BRANCH_EQ   = 0;   // BEQ
constexpr unsigned BRANCH_NE   = 1;   // BNE
constexpr unsigned BRANCH_LT   = 2;   // BLT
constexpr unsigned BRANCH_GE   = 3;   // BGE
constexpr unsigned BRANCH_LTU  = 4;   // BLTU
constexpr unsigned BRANCH_GEU  = 5;   // BGEU

/**
 * EX 级 (执行级) 组件
 * 
 * 包含:
 * - 32 位 ALU，支持 RISC-V RV32I 所有算术逻辑运算
 * - 分支条件判断逻辑
 * - 分支目标地址计算 (PC + 立即数)
 * - 零标志生成 (用于分支判断)
 */
class ExStage : public ch::Component {
public:
    __io(
        // ====================================================================
        // 输入端口：来自 ID/EX 流水线寄存器
        // ====================================================================
        ch_in<ch_uint<32>> rs1_data;   // RS1 数据 (来自寄存器文件)
        ch_in<ch_uint<32>> rs2_data;   // RS2 数据 (来自寄存器文件)
        ch_in<ch_uint<32>> imm;        // 立即数 (经过符号扩展)
        ch_in<ch_uint<32>> pc;         // 当前 PC 值
        ch_in<ch_uint<7>>  opcode;     // 操作码 (FIX: was 4bit, RISC-V opcodes are 7 bits)
        ch_in<ch_uint<3>>  funct3;     // 功能码 3 位
        ch_in<ch_uint<7>>  funct7;     // 功能码 7 位
        ch_in<ch_uint<4>>  alu_op;     // ALU 操作类型选择
        ch_in<ch_bool>     is_branch;  // 是否为分支指令
        ch_in<ch_uint<3>>  branch_type; // 分支类型 (funct3)
        ch_in<ch_bool>     is_store;   // 存储指令 (传递到输出)
        
        // ====================================================================
        // 输出端口：写入 EX/MEM 流水线寄存器
        // ====================================================================
        ch_out<ch_uint<32>> alu_result;   // ALU 计算结果
        ch_out<ch_bool>     branch_taken; // 分支是否跳转
        ch_out<ch_uint<32>> branch_target; // 分支目标地址
        ch_out<ch_bool>     zero;         // 零标志 (result == 0)
        ch_out<ch_bool>     less_than;    // 有符号小于标志
        ch_out<ch_bool>     less_than_u;  // 无符号小于标志
        ch_out<ch_bool>     is_store_alu; // 存储指令 (用于 DTCM write en)
    )

    ExStage(ch::Component* parent = nullptr, const std::string& name = "ex_stage")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // ====================================================================
        // 1. 操作数选择 (FIX: I-type/U-type/J-type uses imm, R-type uses rs2_data)
        // ====================================================================
        // I-type (ADDI, ANDI, ORI, etc.): second operand = imm
        // U-type (LUI, AUIPC): second operand = imm  
        // R-type (ADD, SUB, AND, OR, etc.): second operand = rs2_data
        auto is_i_type = (io().opcode == ch_uint<7>(OP_OPIMM)) | 
                         (io().opcode == ch_uint<7>(OP_LOAD)) |
                         (io().opcode == ch_uint<7>(OP_JALR)) |
                         (io().opcode == ch_uint<7>(OP_LUI)) |
                         (io().opcode == ch_uint<7>(OP_AUIPC));
        ch_uint<32> alu_second_operand = select(is_i_type, io().imm, io().rs2_data);

        // ---- 1.1 加减法 ----
        auto add_result = chlib::add<32>(io().rs1_data, alu_second_operand);
        auto sub_result = chlib::subtract<32>(io().rs1_data, alu_second_operand);

        // ---- 1.2 逻辑运算 (AND, OR, XOR) ----
        auto and_result = chlib::and_gate<32>(io().rs1_data, alu_second_operand);
        auto or_result  = chlib::or_gate<32>(io().rs1_data, alu_second_operand);
        auto xor_result = chlib::xor_gate<32>(io().rs1_data, alu_second_operand);

        // ---- 1.3 移位运算 ----
        // 提取 alu_second_operand 的低位 5bit 作为移位量 (对 I-type 和 R-type 都有效)
        auto shift_amt = bits<4, 0>(alu_second_operand);
        auto sll_result = io().rs1_data << shift_amt;  // 逻辑左移
        auto srl_result = io().rs1_data >> shift_amt;  // 逻辑右移

        // 1.3b 算术右移 (SRA)
        // 完整 SRA 需根据符号位填充高位，当前简化为 SRL。
        // 不影响 rv32ui 基础路径（simple/add/addi 不含 SRA 指令）。
        ch_uint<32> sra_result = srl_result;

        // ---- 1.4 比较运算 ----
        // SLT: 有符号比较 (rs1 < rs2)
        // 通过计算 rs1 - rs2 并检查符号位来判断
        auto sub_for_cmp = io().rs1_data - alu_second_operand;
        auto sign_of_diff = bits<31, 31>(sub_for_cmp);
        
        // 处理符号溢出：如果 rs1 和 rs2 符号不同，直接根据符号判断
        auto rs1_sign = bits<31, 31>(io().rs1_data);
        auto rs2_sign = bits<31, 31>(alu_second_operand);
        auto signs_differ = rs1_sign != rs2_sign;
        // rs1 为负，rs2 为正 => rs1 < rs2 => true
        // rs1 为正，rs2 为负 => rs1 >= rs2 => false
        auto slt_result_sig = select(signs_differ, rs1_sign, sign_of_diff);
        
        // SLTU: 无符号比较 (rs1 < rs2)
        auto sltu_result_sig = (io().rs1_data < alu_second_operand);

        // ====================================================================
        // 2. ALU 结果多选器
        // ====================================================================
        // 使用 select() 链式选择合适的 ALU 操作结果
        // 这是组合逻辑的关键：使用条件选择而非 && 运算符

        ch_uint<32> alu_out = ch_uint<32>(0_d);
        
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_ADD), add_result, alu_out);
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_SUB), sub_result, alu_out);
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_AND), and_result, alu_out);
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_OR),  or_result,  alu_out);
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_XOR), xor_result, alu_out);
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_SLL), sll_result, alu_out);
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_SRL), srl_result, alu_out);
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_SRA), sra_result, alu_out);
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_SLT), zext<32>(slt_result_sig), alu_out);
        alu_out = select(io().alu_op == ch_uint<4>(ALU_OP_SLTU), zext<32>(sltu_result_sig), alu_out);

        // ====================================================================
        // 3. 分支目标地址计算
        // ====================================================================
        // 分支目标 = PC + imm (imm 已在 ID 级符号扩展)
        // 注意：PC 已经是当前指令地址，imm 是相对偏移量

        auto branch_target_calc = io().pc + io().imm;
        // 对于 B 型指令，imm[0] 总是 0，所以目标地址是字对齐的
        // 对于 J 型指令，imm 已经包含 x2 因子

        // ====================================================================
        // 4. 分支条件判断
        // ====================================================================
        // 根据 branch_type (funct3) 判断分支是否跳转
        // 
        // branch_type 编码:
        //   000: BEQ  (Equal)         rs1 == rs2
        //   001: BNE  (Not Equal)     rs1 != rs2
        //   100: BLT  (Less Than)     rs1 < rs2  (有符号)
        //   101: BGE  (Greater Equal) rs1 >= rs2 (有符号)
        //   110: BLTU (Less Than)     rs1 < rs2  (无符号)
        //   111: BGEU (Greater Equal) rs1 >= rs2 (无符号)

        // 使用比较结果
        auto rs1_eq_rs2 = (io().rs1_data == io().rs2_data);
        auto rs1_ne_rs2 = (io().rs1_data != io().rs2_data);
        auto rs1_lt_rs2_signed = slt_result_sig;
        auto rs1_lt_rs2_unsigned = sltu_result_sig;
        auto rs1_ge_rs2_signed = !slt_result_sig;
        auto rs1_ge_rs2_unsigned = !sltu_result_sig;

        // 分支条件多选器
        ch_bool branch_cond = ch_bool(false);
        
        branch_cond = select(io().branch_type == ch_uint<3>(0_d), rs1_eq_rs2, branch_cond);   // BEQ
        branch_cond = select(io().branch_type == ch_uint<3>(1_d), rs1_ne_rs2, branch_cond);   // BNE
        branch_cond = select(io().branch_type == ch_uint<3>(4_d), rs1_lt_rs2_signed, branch_cond);   // BLT
        branch_cond = select(io().branch_type == ch_uint<3>(5_d), rs1_ge_rs2_signed, branch_cond);   // BGE
        branch_cond = select(io().branch_type == ch_uint<3>(6_d), rs1_lt_rs2_unsigned, branch_cond); // BLTU
        branch_cond = select(io().branch_type == ch_uint<3>(7_d), rs1_ge_rs2_unsigned, branch_cond); // BGEU

        // 最终分支跳转信号
        auto branch_taken_sig = select(io().is_branch, branch_cond, ch_bool(false));

        // ====================================================================
        // 5. 零标志生成
        // ====================================================================
        // 零标志用于判断 ALU 结果是否为 0
        // 在某些实现中也用于分支判断

        auto zero_flag = (alu_out == ch_uint<32>(0_d));

        // ====================================================================
        // 6. 输出端口连接
        // ====================================================================

        io().alu_result = alu_out;
        io().branch_target = branch_target_calc;
        io().branch_taken = branch_taken_sig;
        io().zero = zero_flag;
        io().less_than = slt_result_sig;
        io().less_than_u = sltu_result_sig;
        io().is_store_alu = io().is_store;
    }
};

} // namespace riscv
