/**
 * @file if_stage.h
 * @brief RISC-V 5 级流水线 - 取指级 (Instruction Fetch)
 * 
 * 功能:
 * - PC 寄存器 (32 位) 带时钟使能
 * - 指令存储器接口
 * - PC+4 计算
 * - 分支目标选择
 * - 停顿 (stall) 控制
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "rv32i_tcm.h"

namespace riscv {

using namespace ch::core;

/**
 * @brief IF 级 (取指级)
 * 
 * 从指令存储器中取指令，并传递给 ID 级
 */
class IfStage : public ch::Component {
public:
    __io(
        // 控制信号
        ch_in<ch_bool>      stall;          // 流水线停顿
        ch_in<ch_bool>      flush;          // 冲刷流水线 (分支预测错误)
        ch_in<ch_bool>      rst;            // 复位
        
        // 分支控制
        ch_in<ch_uint<32>>  branch_target;  // 分支目标地址
        ch_in<ch_bool>      branch_valid;   // 分支有效
        
        // 输出到 ID 级
        ch_out<ch_uint<32>> pc;             // 当前 PC
        ch_out<ch_uint<32>> instruction;    // 取出的指令
        ch_out<ch_bool>     valid;          // 有效信号
        
        // 指令存储器接口
        ch_out<ch_uint<32>> instr_addr;     // 指令地址输出
        ch_in<ch_uint<32>>  instr_data;     // 指令数据输入
        ch_in<ch_bool>      instr_ready;    // 指令存储器就绪
    )
    
    IfStage(ch::Component* parent = nullptr, const std::string& name = "if_stage")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // PC 寄存器 (32 位)
        // PC 寄存器存储当前指令地址
        // 复位值: 0x80000000 (ITCM 基地址, riscv-tests 入口点)
        ch_reg<ch_uint<32>> pc_reg(ch_uint<32>(0x80000000), "pc_reg");
        
        // ========================================================================
        // PC 更新逻辑
        // ========================================================================
        // 计算 PC+4 (RISC-V 指令固定 4 字节)
        auto pc_plus_4 = pc_reg + ch_uint<32>(4_d);
        
        // 选择下一个 PC 值：
        // - 分支有效时：使用分支目标地址
        // - 正常情况：使用 PC+4
        ch_uint<32> next_pc_target = select(
            io().branch_valid,
            io().branch_target,
            pc_plus_4
        );
        
        // PC 更新使能信号：
        // - 不复位 且 不 stall 时更新（**flush 不在此列** —— 见下方说明）
        //
        // ⚠️ `pc_update_en` **不看 `flush`**（2026-10-25，第 14 轮 critic NIT
        //    补注）：今天安全，因为 flush 的**唯一**来源是
        //    `hazard.io().flush = ex_branch & ex_branch_taken`，而
        //    `next_pc_target` 的选择条件 `branch_valid` 接的正是同一个
        //    `ex_stage.io().branch_taken` ⇒ flush 为真时 `next_pc_target`
        //    **已经是**分支目标 ⇒ PC 无需额外覆盖。
        //    ⇒ 这是一条**隐式不变量**：若将来 flush 有第二个来源
        //      （例如预测失败），此处 MUST 改为「flush 时也更新 PC」。
        //
        // ⚠️ **flush 与 stall 不会同拍**（第 18 轮 critic 要求写明的互斥论证）：
        //    flush 由 **EX 级**的 `branch_taken` 产生 ⇒ 触发那一拍 EX 级只有
        //    那一条（分支）指令 ⇒ 它**不是** load ⇒ `ex_is_load` 为假 ⇒
        //    load-use 检测不可能同拍成立。
        //    ⇒ 若将来引入**第二个 flush 来源**（如预测失败）或让 stall 与
        //      flush 可同拍，MUST 重新审视此处（stall 冻结 PC 会让 PC 失去
        //      分支重定向）。
        //
        // ⚠️ **flush 与 stall 不会同拍**（第 18 轮 critic 要求写明的互斥论证）：
        //    flush 由 **EX 级**的 `branch_taken` 产生 ⇒ 触发那一拍 EX 级只有
        //    那一条（分支）指令 ⇒ 它**不是** load ⇒ `ex_is_load` 为假 ⇒
        //    load-use 检测不可能同拍成立。
        //    ⇒ 若将来引入**第二个 flush 来源**（如预测失败）或让 stall 与
        //      flush 可同拍，MUST 重新审视此处（stall 冻结 PC 会让 PC 失去
        //      分支重定向）。
        auto pc_update_en = select(
            io().rst,
            ch_bool(false),
            select(io().stall, ch_bool(false), ch_bool(true))
        );
        
        // ⚠️ 历史注释「flush 时 PC 重置为 0 (简化处理，实际应该是分支目标)」是一个
        //    **占位实现**，它让每一次 flush 都把 PC 打回 0 ⇒ 程序每几拍就从头重启
        //    （SoC 级实测：41 条固件 200 拍只退休 18 条，`perf_branch_mispredict`
        //    与重启次数同步增长）。flush 的唯一来源是 `hazard.io().flush =
        //    is_branch & branch_taken`（`hazard_unit.h`），而 `branch_valid` 接的
        //    正是同一个 `ex_stage.branch_taken` ⇒ flush 时 `next_pc_target`
        //    已经是分支目标 ⇒ **不需要**任何 flush 覆盖。
        //
        // PC 寄存器更新
        pc_reg->next = select(
            pc_update_en,
            next_pc_target,
            pc_reg
        );
        
        // ========================================================================
        // 指令存储器接口
        // ========================================================================
        // 输出当前 PC 作为指令地址
        io().instr_addr = pc_reg;
        
        // 从指令存储器读取的指令 (组合逻辑)
        ch_uint<32> fetched_instr(0_d, "fetched_instr");
        fetched_instr = io().instr_data;
        
        // ========================================================================
        // 流水线寄存器 (IF 级到 ID 级的输出)
        // ========================================================================
        // IF/ID 流水线寄存器
        ch_reg<ch_uint<32>> if_id_pc_reg(0_d, "if_id_pc");
        ch_reg<ch_uint<32>> if_id_instr_reg(0_d, "if_id_instr");
        ch_reg<ch_bool>     if_id_valid_reg(false, "if_id_valid");

        // ⚠️⚠️ **flush MUST 多杀一拍**（2026-10-25，第 16 轮 critic BLOCKER B1）：
        //    imem 是 **1-cycle `sread`** ⇒ flush 那一拍（T）`pc_reg` 已经指向
        //    「分支 + 12 字节」处 ⇒ **该地址在 T 拍已经提交给 imem** ⇒
        //    T+1 拍数据才返回。
        //    ⇒ 只在 T 拍门控 IF/ID 装载**不够**：T+1 拍那条「错误路径」的
        //      指令会**正常锁存进 IF/ID** ⇒ 它被执行了（且在分支目标**之前**，
        //      顺序也乱）。
        //    ⇒ 用 `flush_d1`（flush 延迟一拍）把 T+1 拍的 IF/ID 装载**也**门掉。
        //    ⚠️ 代价：分支目标的指令要到 T+2 才进 ID ⇒ 冲刷气泡多 1 拍。
        ch_reg<ch_bool>     flush_d1(false, "flush_d1");

        // ⚠️ **pc 必须与指令同拍进 ID**（实测缺陷，见下）
        //
        // imem 是 **1-cycle `sread`**（ADR-041 强制：RV32I imem MUST sread）。
        // 时序：周期 T 提交地址 word W_T ⇒ 周期 T+1 `instr_data` = word W_T
        //       ⇒ 周期 T+2 `if_id_instr_reg` 装 word W_T。
        // 而 `pc_reg` 已在 T→T+1 边沿前进 ⇒ 直接锁存 `pc_reg` 会让
        // `if_id_pc_reg`(T+2) = pc_reg(T+1) = **word W_{T+1}** ⇒ 比它译码的
        // 指令**晚一个字**（实测：ID 解码 word T-1 而 dbg_id_pc 是 word T）。
        // ⇒ 分支目标 `pc + imm` 会算在错误的 pc 上（差 4 字节）。
        //
        // ⇒ pc 必须与指令**同样延迟一拍**：这里加一个 `fetch_pc_reg` 把 pc
        //    推迟一拍，再由 IF/ID 寄存器锁存它。
        ch_reg<ch_uint<32>> fetch_pc_reg(0_d, "fetch_pc");
        
        // flush 延迟一拍（见 `flush_d1` 的说明）
        flush_d1->next = io().flush;

        // IF/ID 寄存器更新使能：
        // - 指令存储器就绪
        // - 不复位
        // - 不 stall
        // - 不 flush（**也不在 flush 的下一拍** —— 见 `flush_d1`）
        auto if_id_update_en = select(
            io().rst,
            ch_bool(false),
            select(
                io().stall | io().flush | flush_d1,
                ch_bool(false),
                ch_bool(true)
            )
        );
        
        // fetch_pc_reg：把 pc 推迟一拍（与 imem 的 1-cycle sread 对齐）
        fetch_pc_reg->next = pc_reg;

        // IF/ID 寄存器更新（pc 取**延迟一拍**的值，与指令同拍）
        if_id_pc_reg->next = select(
            if_id_update_en,
            fetch_pc_reg,
            if_id_pc_reg
        );
        
        if_id_instr_reg->next = select(
            if_id_update_en,
            fetched_instr,
            if_id_instr_reg
        );
        
        // 有效信号在 flush 时清零（**含 flush 的下一拍** —— 见 `flush_d1`）
        if_id_valid_reg->next = select(
            if_id_update_en,
            io().instr_ready,
            select(
                io().flush | flush_d1,
                ch_bool(false),
                if_id_valid_reg
            )
        );
        
        // ========================================================================
        // 输出到 ID 级
        // ========================================================================
        io().pc = if_id_pc_reg;
        io().instruction = if_id_instr_reg;
        io().valid = if_id_valid_reg;
    }
};

} // namespace riscv
