/**
 * @file rv32i_branch_predict.h
 * @brief RV32I 分支预测单元定义
 * 
 * 功能：检测分支指令并预测分支结果，生成 flush/stall 控制信号
 * 
 * 预测策略:
 * - 静态预测：分支永远不跳转 (BNT - Branch Not Taken)
 * - 后续可优化为动态预测 (1-bit saturating counter)
 * 
 * 控制冒险处理:
 * - 分支指令在 ID 阶段检测
 * - 预测错误时 (实际跳转但预测不跳转) flush IF/ID 寄存器
 * - 预测正确时继续执行 (无 bubble)
 * 
 * 分支指令类型 (RISC-V):
 * - BEQ:  Branch if Equal           (rs1 == rs2)
 * - BNE:  Branch if Not Equal       (rs1 != rs2)
 * - BLT:  Branch if Less Than       (rs1 < rs2, signed)
 * - BGE:  Branch if Greater Equal   (rs1 >= rs2, signed)
 * - BLTU: Branch if Less Than Un    (rs1 < rs2, unsigned)
 * - BGEU: Branch if Greater Equal U (rs1 >= rs2, unsigned)
 */

#pragma once

#include "ch.hpp"
#include "riscv/rv32i_decoder.h"
#include "riscv/rv32i_pipeline_regs.h"

using namespace ch::core;

namespace riscv {

// ============================================================================
// 分支检测单元 (组合逻辑)
// ============================================================================
/**
 * @brief 分支指令检测单元
 * 
 * 根据指令的 opcode 和 funct3 字段，检测是否为分支指令
 * 
 * RISC-V 分支指令格式 (B-type):
 * - opcode: 7'b1100011 (99 = 0x63)
 * - funct3: 3 位分支类型编码
 *   - 000: BEQ
 *   - 001: BNE
 *   - 100: BLT
 *   - 101: BGE
 *   - 110: BLTU
 *   - 111: BGEU
 * 
 * 输入:
 * - instruction: 32 位指令
 * 
 * 输出:
 * - is_branch: 是否为分支指令
 * - branch_type: 分支类型 (funct3)
 */
class BranchDetector : public ch::Component {
public:
    __io(
        // 输入：来自 IF/ID 的指令
        ch_in<ch_uint<32>>  instruction;    // 32 位指令
        
        // 输出：分支检测结果
        ch_out<ch_bool>     is_branch;      // 是否为分支指令
        ch_out<ch_uint<3>>  branch_type     // 分支类型 (funct3)
    )
    
    BranchDetector(ch::Component* parent = nullptr, const std::string& name = "branch_detector")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 提取 opcode (bits [6:0])
        auto opcode = bits<6, 0>(io().instruction);
        
        // 提取 funct3 (bits [14:12])
        auto funct3 = bits<14, 12>(io().instruction);
        
        // 分支指令 opcode: 99 (7'b1100011 = 0x63)
        auto is_branch_opcode = (opcode == ch_uint<7>(99_d));
        
        // 有效的分支 funct3: 000, 001, 100, 101, 110, 111
        // 所有 3 位组合都是有效的分支类型
        auto valid_branch_funct3 = ch_bool(true);
        
        // 是分支指令：opcode 匹配 + funct3 有效
        io().is_branch = is_branch_opcode & valid_branch_funct3;
        io().branch_type = funct3;
    }
};

// ============================================================================
// 分支条件计算单元 (组合逻辑)
// ============================================================================
/**
 * @brief 分支条件计算单元
 * 
 * 根据分支类型和两个操作数，计算分支条件是否满足
 * 
 * 输入:
 * - branch_type: 分支类型 (funct3)
 * - rs1_data: RS1 寄存器数据 (32 位)
 * - rs2_data: RS2 寄存器数据 (32 位)
 * 
 * 输出:
 * - branch_condition: 分支条件是否满足 (1=跳转，0=不跳转)
 * 
 * 条件计算:
 * - BEQ  (000): rs1 == rs2
 * - BNE  (001): rs1 != rs2
 * - BLT  (100): rs1 < rs2 (有符号)
 * - BGE  (101): rs1 >= rs2 (有符号)
 * - BLTU (110): rs1 < rs2 (无符号)
 * - BGEU (111): rs1 >= rs2 (无符号)
 * 
 * 注意：有符号比较通过检查符号位实现
 */
class BranchConditionUnit : public ch::Component {
public:
    __io(
        // 输入：分支类型和操作数
        ch_in<ch_uint<3>>  branch_type;     // 分支类型 (funct3)
        ch_in<ch_uint<32>> rs1_data;        // RS1 数据
        ch_in<ch_uint<32>> rs2_data;        // RS2 数据
        
        // 输出：分支条件结果
        ch_out<ch_bool>    branch_condition // 条件是否满足 (1=跳转)
    )
    
    BranchConditionUnit(ch::Component* parent = nullptr, const std::string& name = "branch_condition")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 提取操作数符号位 (最高位 bit 31)
        // ========================================================================
        auto rs1_sign = bits<31, 31>(io().rs1_data) != ch_uint<1>(0_d);
        auto rs2_sign = bits<31, 31>(io().rs2_data) != ch_uint<1>(0_d);
        auto signs_equal = (rs1_sign == rs2_sign);
        
        // ========================================================================
        // 有符号比较 (使用减法，检查符号位)
        // ========================================================================
        // 有符号比较 (使用减法，检查符号位)
        // 参考 RV32I ALU 的 SLT 实现
        // 符号不同：a < b 当且仅当 a 是负数
        // 符号相同：a < b 当且仅当 a - b 是负数
        auto sub_result = io().rs1_data - io().rs2_data;
        auto sub_sign = bits<31, 31>(sub_result);
        auto signed_less = signs_equal 
                           ? (sub_sign != ch_uint<1>(0))  // 符号相同：检查 a-b 的符号
                           : rs1_sign;                     // 符号不同：a 为负则 a < b
        
        auto signed_ge = !signed_less;
        
        // ========================================================================
        // 无符号比较
        // ========================================================================
        auto unsigned_less = io().rs1_data < io().rs2_data;
        auto unsigned_ge = !unsigned_less;
        
        // ========================================================================
        // 相等性比较 (BEQ/BNE 有符号无符号相同)
        // ========================================================================
        auto equal = (io().rs1_data == io().rs2_data);
        auto not_equal = !equal;
        
        // ========================================================================
        // 根据 branch_type 选择条件
        // ========================================================================
        // branch_type:
        // 000: BEQ  -> equal
        // 001: BNE  -> not_equal
        // 100: BLT  -> signed_less
        // 101: BGE  -> signed_ge
        // 110: BLTU -> unsigned_less
        // 111: BGEU -> unsigned_ge
        
        auto is_beq  = io().branch_type == ch_uint<3>(0);
        auto is_bne  = io().branch_type == ch_uint<3>(1);
        auto is_blt  = io().branch_type == ch_uint<3>(4);
        auto is_bge  = io().branch_type == ch_uint<3>(5);
        auto is_bltu = io().branch_type == ch_uint<3>(6);
        auto is_bgeu = io().branch_type == ch_uint<3>(7);
        
        // 分支条件 = 各类型条件的 OR
        io().branch_condition = 
            (is_beq  & equal) |
            (is_bne  & not_equal) |
            (is_blt  & signed_less) |
            (is_bge  & signed_ge) |
            (is_bltu & unsigned_less) |
            (is_bgeu & unsigned_ge);
    }
};

// ============================================================================
// 分支预测单元 (静态 BNT 预测)
// ============================================================================
/**
 * @brief 分支预测单元 (整合检测和条件计算)
 * 
 * 预测策略：Branch Not Taken (BNT) - 永远预测分支不跳转
 * 
 * 工作流程:
 * 1. ID 阶段检测是否为分支指令
 * 2. 如果是分支指令，计算实际分支条件
 * 3. BNT 策略：预测永远不跳转
 * 4. 如果实际条件满足 (应该跳转)，则预测错误，需要 flush
 * 
 * 输入:
 * - instruction: 32 位指令
 * - rs1_data: RS1 寄存器数据
 * - rs2_data: RS2 寄存器数据
 * - pc: 当前指令 PC
 * 
 * 输出:
 * - is_branch: 是否为分支指令
 * - branch_taken: 分支实际是否跳转
 * - predict_error: 预测错误信号 (用于 flush IF/ID)
 * - branch_target: 分支目标地址 (PC + 符号扩展立即数)
 */
class BranchPredictor : public ch::Component {
public:
    __io(
        // 输入：来自 ID 阶段的数据
        ch_in<ch_uint<32>>  instruction;    // 32 位指令
        ch_in<ch_uint<32>>  rs1_data;       // RS1 数据
        ch_in<ch_uint<32>>  rs2_data;       // RS2 数据
        ch_in<ch_uint<32>>  pc;             // 当前指令 PC
        
        // 输出：分支预测结果
        ch_out<ch_bool>     is_branch;          // 是否为分支指令
        ch_out<ch_bool>     branch_taken;       // 分支实际是否跳转
        ch_out<ch_bool>     predict_error;      // 预测错误信号 (flush IF/ID)
        ch_out<ch_uint<32>> branch_target       // 分支目标地址
    )
    
    BranchPredictor(ch::Component* parent = nullptr, const std::string& name = "branch_predictor")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 提取指令字段
        // ========================================================================
        auto opcode = bits<6, 0>(io().instruction);
        auto funct3 = bits<14, 12>(io().instruction);
        
        // 分支指令 opcode: 99 (7'b1100011 = 0x63)
        auto is_branch_opcode = (opcode == ch_uint<7>(99_d));
        io().is_branch = is_branch_opcode;
        
        // ========================================================================
        // 分支条件计算
        // ========================================================================
        // 提取操作数符号位
        auto rs1_sign = bits<31, 31>(io().rs1_data);
        auto rs2_sign = bits<31, 31>(io().rs2_data);
        auto signs_equal = (rs1_sign == rs2_sign);
        
        // 有符号比较 (直接比较，避免减法溢出)
        // 符号相同：直接比较大小
        // 符号不同：符号位大的反而小 (负数 < 正数)
        auto signed_less = signs_equal 
                           ? (io().rs1_data < io().rs2_data)  // 符号相同，直接比较
                           : (rs1_sign > rs2_sign);           // 符号不同，负数 < 正数
        
        auto signed_ge = !signed_less;
        
        // 无符号比较
        auto unsigned_less = io().rs1_data < io().rs2_data;
        auto unsigned_ge = !unsigned_less;
        
        // 相等性比较
        auto equal = (io().rs1_data == io().rs2_data);
        auto not_equal = !equal;
        
        // 根据 funct3 选择条件
        auto is_beq  = funct3 == ch_uint<3>(0_d);
        auto is_bne  = funct3 == ch_uint<3>(1_d);
        auto is_blt  = funct3 == ch_uint<3>(4_d);
        auto is_bge  = funct3 == ch_uint<3>(5_d);
        auto is_bltu = funct3 == ch_uint<3>(6_d);
        auto is_bgeu = funct3 == ch_uint<3>(7_d);
        
        // 分支条件
        auto branch_condition = 
            (is_beq  & equal) |
            (is_bne  & not_equal) |
            (is_blt  & signed_less) |
            (is_bge  & signed_ge) |
            (is_bltu & unsigned_less) |
            (is_bgeu & unsigned_ge);
        
        // ========================================================================
        // BNT 预测逻辑
        // ========================================================================
        auto branch_taken_val = io().is_branch & branch_condition;
        io().branch_taken = branch_taken_val;
        io().predict_error = branch_taken_val;  // BNT 预测错误 = 实际跳转
        
        // ========================================================================
        // 分支目标地址计算
        // ========================================================================
        auto imm_12   = bits<31, 31>(io().instruction);
        auto imm_11   = bits<7, 7>(io().instruction);
        auto imm_10_5 = bits<30, 25>(io().instruction);
        auto imm_4_1  = bits<11, 8>(io().instruction);
        
        // 拼接立即数 (未移位前，bit 0 = 0)
        ch_uint<13> imm_unshifted(0_d);
        imm_unshifted = (ch_uint<13>(imm_12) << 12_d) |
                        (ch_uint<13>(imm_11) << 11_d) |
                        (ch_uint<13>(imm_10_5) << 5_d) |
                        (ch_uint<13>(imm_4_1) << 1_d);
        
        // 提取低 12 位用于拼接
        auto imm_low_12 = bits<11, 0>(imm_unshifted);
        
        // 符号扩展到 32 位
        ch_uint<32> imm_extended(0_d);
        imm_extended = select(imm_12,
                              (ch_uint<32>(1048575_d) << 12_d) | ch_uint<32>(imm_low_12),
                              ch_uint<32>(imm_unshifted));
        
        // 分支目标 = PC + 符号扩展立即数
        io().branch_target = io().pc + imm_extended;
    }
};

// ============================================================================
// 控制冒险处理单元 (生成 flush/stall 信号)
// ============================================================================
/**
 * @brief 控制冒险处理单元
 * 
 * 根据分支预测结果，生成流水线控制信号
 * 
 * 输入:
 * - is_branch: 是否为分支指令
 * - predict_error: 预测错误信号
 * - branch_taken: 分支实际是否跳转
 * - branch_target: 分支目标地址
 * - pc: 当前 PC
 * - instr_valid: 指令有效
 * 
 * 输出:
 * - flush_if_id: flush IF/ID 寄存器 (预测错误时)
 * - stall_if: 停顿 IF 阶段 (可选，用于更复杂的预测)
 * - pc_src: PC 源选择 (0=PC+4, 1=branch_target)
 * - pc_target: 下一条指令的 PC 目标地址
 */
class ControlHazardUnit : public ch::Component {
public:
    __io(
        // 输入：来自分支预测单元
        ch_in<ch_bool>     is_branch;         // 是否为分支指令
        ch_in<ch_bool>     predict_error;     // 预测错误信号
        ch_in<ch_bool>     branch_taken;      // 分支实际是否跳转
        ch_in<ch_uint<32>> branch_target;     // 分支目标地址
        ch_in<ch_uint<32>> pc;                // 当前 PC
        ch_in<ch_bool>     instr_valid;       // 指令有效
        
        // 输出：流水线控制信号
        ch_out<ch_bool>    flush_if_id;       // flush IF/ID 寄存器
        ch_out<ch_bool>    stall_if;          // 停顿 IF 阶段
        ch_out<ch_bool>    pc_src;            // PC 源选择
        ch_out<ch_uint<32>> pc_target         // 下一条指令 PC
    )
    
    ControlHazardUnit(ch::Component* parent = nullptr, const std::string& name = "control_hazard")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // Flush 信号生成
        // ========================================================================
        // 预测错误时 flush IF/ID 寄存器
        // flush 条件：是有效分支指令 + 预测错误
        io().flush_if_id = io().instr_valid & io().predict_error;
        
        // ========================================================================
        // Stall 信号生成
        // ========================================================================
        // BNT 策略下不需要 stall (预测不跳转，继续取指)
        // 后续动态预测可能需要 stall 等待预测结果
        io().stall_if = ch_bool(false);
        
        // ========================================================================
        // PC 源选择
        // ========================================================================
        // pc_src = 1 时选择 branch_target，否则选择 PC+4
        // 条件：分支实际跳转 (无论预测是否正确，都要跳转到正确目标)
        io().pc_src = io().branch_taken;
        
        // ========================================================================
        // PC 目标计算
        // ========================================================================
        auto pc_plus_4 = io().pc + ch_uint<32>(4_d);
        io().pc_target = select(io().pc_src, io().branch_target, pc_plus_4);
    }
};

// ============================================================================
// 分支预测状态寄存器 (用于动态预测扩展)
// ============================================================================
/**
 * @brief 分支预测状态寄存器 (预留动态预测扩展)
 * 
 * 当前实现：静态 BNT 预测，不需要状态
 * 未来扩展：1-bit saturating counter 动态预测
 * 
 * 状态编码:
 * - 0: Strongly Not Taken
 * - 1: Weakly Not Taken
 * - 2: Weakly Taken
 * - 3: Strongly Taken
 * 
 * 输入:
 * - pc: 分支指令 PC (用于索引预测表)
 * - branch_taken: 分支实际是否跳转
 * - update_enable: 更新使能 (分支解析后)
 * - rst: 复位
 * - clk: 时钟
 * 
 * 输出:
 * - prediction: 预测结果 (0=Not Taken, 1=Taken)
 */
class BranchPredictorState : public ch::Component {
public:
    __io(
        // 输入
        ch_in<ch_uint<32>>  pc;             // 分支指令 PC
        ch_in<ch_bool>      branch_taken;   // 分支实际结果
        ch_in<ch_bool>      update_enable;  // 状态更新使能
        ch_in<ch_bool>      rst;            // 复位
        ch_in<ch_bool>      clk;            // 时钟
        
        // 输出
        ch_out<ch_bool>     prediction      // 预测结果 (0=BNT, 1=BT)
    )
    
    BranchPredictorState(ch::Component* parent = nullptr, const std::string& name = "bp_state")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 静态 BNT 预测 (当前实现)
        // ========================================================================
        // 永远预测不跳转
        io().prediction = ch_bool(false);
        
        // ========================================================================
        // 预留：动态预测状态更新逻辑
        // ========================================================================
        // 未来扩展：使用 PC 低 bits 索引预测表，更新 saturating counter
        // 
        // 示例 (1-bit counter):
        // auto bp_index = bits<9, 2>(pc);  // 256-entry 预测表
        // ch_reg<ch_uint<1>> counter(0_d);
        // counter->next = select(update_enable, 
        //                        select(branch_taken, ch_uint<1>(1_d), ch_uint<1>(0_d)),
        //                        counter);
        // io().prediction = counter;
    }
};

} // namespace riscv
