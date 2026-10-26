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

namespace riscv {

using namespace ch::core;

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
        ch_out<ch_bool>     branch_taken; // **重定向成立**（条件分支成立 / JAL·JALR 无条件跳）
        ch_out<ch_uint<32>> branch_target; // **重定向目标**（B 型 = pc+imm_b；JAL = pc+imm_j；JALR = (rs1+imm_i)&~1）
        ch_out<ch_uint<32>> link_value;   // **返回地址** = pc + 4（JAL/JALR 的写回值）
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
        // ⚠️ **OP_STORE 必须在此列表内**（2026-10-25 实测修复）：
        //    store 的 ALU 结果是**访存地址** = rs1 + imm（S 型立即数），
        //    而被存的数据走独立通路（`exmem_rs2_data`）。
        //    漏掉 OP_STORE ⇒ 第二操作数取 `rs2_data` ⇒ 地址算成 rs1+rs2_data
        //    （实测：store 地址恒为 0，dmem 被写到 word 0）。
        auto is_i_type = (io().opcode == ch_uint<7>(OP_OPIMM)) | 
                         (io().opcode == ch_uint<7>(OP_LOAD)) |
                         (io().opcode == ch_uint<7>(OP_STORE)) |
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

        // 1.3b 算术右移 (SRA) —— 2026-10-26 实现（此前 `sra_result = srl_result`）
        //
        // ⚠️ 框架层**没有**算术右移原语：`include/core/operators_shift.h` 里
        //    `operator>>>`（`sshr_op`）是**被注释掉**的 ⇒ 在 ALU 层合成：
        //      sra = (a >> sh) | (sign ? (0xFFFFFFFF << (32 - sh)) : 0)
        //    `ch_uint` 无符号 ⇒ `>>` 天然是**逻辑**右移 ⇒ 只需补**高位符号填充**。
        // ⚠️ `sh == 0` 时 MUST NOT 填充（否则 `32 - 0 = 32` ⇒ 移位量越界）；
        //    `sh ∈ [1,31]` ⇒ `32 - sh ∈ [1,31]` ⇒ 不触越界。
        auto sra_sign_set = (bits<31, 31>(io().rs1_data) == ch_uint<1>(1_d));
        auto sra_fill = select(sra_sign_set & (shift_amt != ch_uint<5>(0_d)),
                               shl<32>(ch_uint<32>(0xFFFFFFFF_h),
                                       ch_uint<32>(32_d) - zext<32>(shift_amt)),
                               ch_uint<32>(0_d));
        auto sra_result = srl_result | sra_fill;

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
        // 3. 重定向目标地址计算（2026-10-26 扩展：不止条件分支）
        // ====================================================================
        // 三种来源（一条指令**至多命中一种** ⇒ 三者互斥）：
        //   OP_BRANCH ⇒ pc + imm_b            （条件分支目标）
        //   OP_JAL    ⇒ pc + imm_j            （无条件跳；imm_j 已是**字节**偏移 ——
        //                                     decoder 把 instr[10:1] 放在 bit[10:1]
        //                                     ⇒ bit0 恒 0 ⇒ 天然 2 字节对齐）
        //   OP_JALR   ⇒ (rs1 + imm_i) & ~1    （最低位 MUST 清零，RISC-V 规范）
        // ⚠️ `JALR` 的 `rs1` 取 `io().rs1_data` ⇒ **无数据前推**（ADR-046 D5 悬置）
        //    ⇒ 若 `rs1` 由**前一条**指令写入 ⇒ 读到**旧值** ⇒ 已知限制
        //    （见 design.md §5）。
        auto is_op_branch = (io().opcode == ch_uint<7>(OP_BRANCH));
        auto is_op_jal    = (io().opcode == ch_uint<7>(OP_JAL));
        auto is_op_jalr   = (io().opcode == ch_uint<7>(OP_JALR));
        auto branch_target_calc = io().pc + io().imm;  // B 型 / JAL 共用
        auto jalr_target_calc =
            (io().rs1_data + io().imm) & ch_uint<32>(0xFFFFFFFE_h);  // & ~1
        auto jump_target_calc = select(is_op_jalr, jalr_target_calc,
                                       branch_target_calc);
        auto jump_unconditional = is_op_jal | is_op_jalr;

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

        // 最终**重定向**信号（2026-10-26 扩展）：
        //   - 条件分支 ⇒ 条件成立才跳
        //   - JAL / JALR ⇒ **无条件**跳（优先级高于条件分支 —— 三者互斥，取哪支都一样）
        auto branch_taken_sig = select(jump_unconditional, ch_bool(true),
                                       select(io().is_branch, branch_cond,
                                              ch_bool(false)));

        // ====================================================================
        // 5. 零标志生成
        // ====================================================================
        // 零标志用于判断 ALU 结果是否为 0
        // 在某些实现中也用于分支判断

        auto zero_flag = (alu_out == ch_uint<32>(0_d));

        // ====================================================================
        // 6. 输出端口连接
        // ====================================================================

        // ⚠️ **LUI / AUIPC 的结果不是「rs1 + imm」**：
        //    LUI = imm（高位填充）；AUIPC = pc + imm。
        //    而 ALU 的第二操作数选择把 LUI/AUIPC 归到「用 imm」的 I 型 ⇒
        //    结果会是 `rs1 + imm`（静默错误）。
        auto ex_is_lui = (io().opcode == ch_uint<7>(OP_LUI));
        auto ex_is_auipc = (io().opcode == ch_uint<7>(OP_AUIPC));
        io().alu_result = select(ex_is_lui, io().imm,
                                 select(ex_is_auipc, io().pc + io().imm, alu_out));
        io().branch_target = select(jump_unconditional, jump_target_calc,
                                    branch_target_calc);
        io().branch_taken = branch_taken_sig;
        // 返回地址（JAL/JALR 的写回值）= pc + 4
        // ⚠️ `pc` 是**该指令自己的** pc（ID/EX 锁存）⇒ +4 就是「下一条指令」的地址
        io().link_value = io().pc + ch_uint<32>(4_d);
        io().zero = zero_flag;
        io().less_than = slt_result_sig;
        io().less_than_u = sltu_result_sig;
        io().is_store_alu = io().is_store;
    }
};

} // namespace riscv
