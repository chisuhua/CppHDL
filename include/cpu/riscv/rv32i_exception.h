/**
 * @file rv32i_exception.h
 * @brief RISC-V 异常/陷阱处理单元（Exception/Trap Handling Unit）
 * 
 * 功能：
 * 1. 陷阱入口处理：捕获异常时保存 PC、原因码到 CSR，跳转到 mtvec
 * 2. MRET 返回处理：从 mepc 恢复 PC，恢复 mstatus.MIE
 * 
 * `mcause` 编码（Priv Spec §2.1.1.15）：`bit31` = **Interrupt** · `[30:0]` = **code**
 * ⇒ ⇒ **同一数字在两侧含义不同** ⇒ ⇒ 必须分表（下表**仅列本核相关项**）：
 *
 * 异常（`bit31 = 0`）：
 * - 2: Illegal instruction（未知 opcode / 保留组合 / `SRET` 在 M-only 下不可编码）
 * - 3: Breakpoint（`EBREAK`）
 * - 11: Environment call from **M-mode**（`ECALL` 在本核唯一可达的取值；
 *   8 = from U / 9 = from S ⇒ 本核**只有 M-mode** ⇒ 不可达）
 *
 * 中断（`bit31 = 1` ⇒ 写作 `0x8000_0000 | code`；**P6-6 才实现**）：
 * - 3: Machine software interrupt
 * - 7: Machine timer interrupt
 * - 11: Machine external interrupt
 *
 * ⚠️ **旧注释（已废）**：曾把上面两张表**混写**成一张（`3` = MSI / `8` = ECALL /
 *    `11` = MEI）⇒ ⇒ 与异常侧的 `3` = breakpoint / `11` = ECALL-from-M **直接冲突**。
 * 
 * CSR 地址：
 * - mepc (0x341): 异常指令 PC 保存寄存器
 * - mcause (0x342): 异常原因寄存器
 * - mtvec (0x305): 陷阱向量基址寄存器
 * - mstatus (0x300): 机器状态寄存器
 * 
 * 作者：DevMate
 * 最后修改：2026-04-14
 */
#pragma once

#include "ch.hpp"
#include "component.h"

namespace riscv {

using namespace ch::core;

/**
 * @brief 异常/陷阱处理单元
 * 
 * 负责：
 * 1. 陷阱入口：当 exception_valid 时，写 CSR 并跳转到 mtvec
 * 2. MRET 返回：当 mret_instruction 时，从 mepc 恢复 PC
 */
class ExceptionUnit : public ch::Component {
public:
    __io(
        // ========================================================================
        // 输入：来自流水线
        // ========================================================================
        ch_in<ch_uint<32>> current_pc;       // 当前指令 PC
        ch_in<ch_uint<32>> exception_cause;  // 异常原因码
        ch_in<ch_bool>     exception_valid; // 异常有效信号
        
        // ========================================================================
        // 输入：CSR 接口（从 CSR Bank 读取）
        // ========================================================================
        ch_in<ch_uint<32>> csr_mstatus;     // 当前 mstatus
        ch_in<ch_uint<32>> csr_mtvec;       // 陷阱向量基址
        ch_in<ch_uint<32>> csr_mepc;        // MRET 返回地址
        ch_in<ch_uint<32>> csr_mie;         // 中断使能
        ch_in<ch_uint<32>> csr_mcause;      // 当前异常原因
        
        // ========================================================================
        // 输出：CSR 写接口（写入 CSR Bank）
        // ========================================================================
        ch_out<ch_uint<12>> csr_write_addr;  // CSR 写地址 (mepc=0x341, mcause=0x342)
        ch_out<ch_uint<32>> csr_write_data; // CSR 写数据
        ch_out<ch_bool>     csr_write_en;   // CSR 写使能
        
        // ========================================================================
        // 输出：到流水线
        // ========================================================================
        ch_out<ch_uint<32>> trap_entry_pc;   // 陷阱入口 PC (跳转到 mtvec)
        ch_out<ch_bool>     trap_entry_valid; // 陷阱入口有效
        ch_out<ch_bool>     trap_flush;       // 流水线刷新信号
        
        // ========================================================================
        // MRET 信号
        // ========================================================================
        ch_in<ch_bool>     mret_instruction; // MRET 指令检测到
        ch_out<ch_uint<32>> mret_pc;         // MRET 返回 PC
        ch_out<ch_bool>     mret_valid;      // MRET 有效信号
    )
    
    ExceptionUnit(ch::Component* parent = nullptr, const std::string& name = "exception") 
        : ch::Component(parent, name) {}
    
    void create_ports() override { 
        new (io_storage_) io_type; 
    }
    
    void describe() override {
        // ========================================================================
        // 常量定义
        // ========================================================================
        constexpr uint32_t CSR_MEPC_ADDR = 0x341;
        constexpr uint32_t CSR_MCAUSE_ADDR = 0x342;
        
        // mtvec[31:2] = 基址（`[1:0]` 是 WARL 的 MODE，本实现合法集 = {0}
        // ⇒ ⇒ 由 `CsrBank` 写路径掩零；本组件只需 BASE）
        constexpr uint32_t MTVEC_BASE_MASK = ~0x3;
        
        // mstatus 位偏移
        constexpr uint32_t MSTATUS_MIE_BIT = 3;
        constexpr uint32_t MSTATUS_MPIE_BIT = 7;
        
        // ========================================================================
        // 陷阱入口处理
        // ========================================================================
        
        // 计算 mtvec 基址（清除低2位模式位）
        auto mtvec_base = io().csr_mtvec & MTVEC_BASE_MASK;
        
        // 计算陷阱入口 PC：**只实现 Direct**
        // ⚠️ `mtvec.MODE` 是 **WARL** 且合法集 = `{0}`（`CsrBank` 写路径把 `[1:0]` 掩零）
        //    ⇒ ⇒ 恒取 BASE；Vectored（`BASE + 4×cause`）**未实现**
        //    ⇒ ⇒ 若日后实现，MODE 合法集与 BASE 的对齐约束 MUST 一并更新
        //    （规范允许 Vectored 模式要求**更严**的对齐）
        auto trap_pc = mtvec_base;
        
        // 陷阱入口信号
        auto do_trap_entry = io().exception_valid;
        
        // 输出陷阱入口 PC 和有效信号
        io().trap_entry_pc = trap_pc;
        io().trap_entry_valid = do_trap_entry;
        io().trap_flush = do_trap_entry;
        
        // ========================================================================
        // CSR 写操作：保存异常上下文（**两拍**写完 mepc + mcause）
        //
        // ⚠️⚠️ **2026-11-16 · B4 · G5（原缺陷）**：旧实现是
        //      `write_mepc = write_mcause = do_trap_entry` ⇒ ⇒ 两个**同时**为真
        //      ⇒ ⇒ `csr_addr = select(write_mepc, MEPC, …)` ⇒ ⇒ **MEPC 恒命中**
        //      ⇒ ⇒ **`mcause` 分支永远不可达** ⇒ ⇒ `mcause` **永远写不到**
        //      ⇒ ⇒（实测：`test_csr_unit.cpp` 的既有断言只查**写口**，
        //      ⇒ ⇒ ⇒ 从不查 `CsrBank` 的**读回** ⇒ ⇒ 该缺陷**零信号**）。
        //
        // ⚠️ **修法 = 跨两拍**（`CsrBank` **零改动** ⇒ ⇒ 不触发「新增输入端口 ⇒
        //    全部实例化点 MUST 显式驱动」的义务）：
        //    · **拍 1** ⇒ 写 `mepc`（= `current_pc`）⇒ 并**锁存** `exception_cause`
        //    · **拍 2** ⇒ 写 `mcause`（= **拍 1 锁存**的原因码 ⇒ ⇒ 与 mepc **同配对**）
        // ⇒ ⇒ **配对正确性**：即便 `exception_valid` 是**电平**（持续拉高），
        //    ⇒ ⇒ `mepc` 与 `mcause` 始终是**同一次异常**的一对（mcause 晚 1 拍）
        //    ⇒ ⇒ **无数据丢失**（旧的「每拍重写 mepc」也不会丢 mcause —— 因为它压根写不到）。
        //
        // ⚠️ **`exception_valid` 的契约**：它是**电平** ⇒ ⇒ 持续拉高 ⇒ ⇒ 每 **2** 拍
        //    重复一次「写 mepc / 写 mcause」序列（两者的**值**始终等于最近一次异常
        //    ⇒ ⇒ **无害**）⇒ ⇒ **真实使用方 MUST 在取走陷阱后撤销 `exception_valid`**。
        // ========================================================================
        // 阶段：0 = 空闲（可开始一次陷阱）· 1 = 已写 mepc（待写 mcause）
        ch_reg<ch_uint<2>> trap_phase{0_d, "trap_phase"};
        // 拍 1 锁存的原因码（拍 2 写 mcause 时用它 ⇒ ⇒ 保证与 mepc **同配对**）
        ch_reg<ch_uint<32>> cause_latched{0_d, "cause_latched"};
        
        // 拍 1：开始一次陷阱（写 mepc）
        auto start_trap = (trap_phase == ch_uint<2>(0_d)) & do_trap_entry;
        // 拍 2：写 mcause
        auto finish_trap = (trap_phase == ch_uint<2>(1_d));
        
        auto write_mepc = start_trap;
        auto write_mcause = finish_trap;
        
        // 状态推进：拍 1 ⇒ 1；拍 2 ⇒ 0（其余保持）
        trap_phase->next = select(start_trap, ch_uint<2>(1_d),
                                 select(finish_trap, ch_uint<2>(0_d), trap_phase));
        // 原因码在拍 1 锁存（拍 2 读它 ⇒ ⇒ 与 mepc 同配对）
        cause_latched->next = select(start_trap, io().exception_cause, cause_latched);
        
        // CSR 写地址选择：mepc 或 mcause（**互斥** ⇒ ⇒ 两拍各写一个）
        auto csr_addr = select(write_mepc,
                              CSR_MEPC_ADDR,
                              select(write_mcause,
                                     CSR_MCAUSE_ADDR,
                                     ch_uint<12>(0_d)));
        
        // CSR 写数据选择：current_pc 或 **锁存的** exception_cause
        auto csr_data = select(write_mepc,
                               io().current_pc,
                               select(write_mcause,
                                      cause_latched,
                                      ch_uint<32>(0_d)));
        
        // CSR 写使能：任一 CSR 需要写
        auto csr_write = write_mepc | write_mcause;
        
        io().csr_write_addr = csr_addr;
        io().csr_write_data = csr_data;
        io().csr_write_en = csr_write;
        
        // ========================================================================
        // MRET 处理
        // ========================================================================
        
        // MRET 有效条件：检测到 MRET 指令
        auto do_mret = io().mret_instruction;
        
        // MRET 返回 PC = mepc
        io().mret_pc = io().csr_mepc;
        io().mret_valid = do_mret;
        
        // ⚠️⚠️ **2026-11-16 订正（原注释是**假陈述** · oracle SHOULD_FIX-1）**：原文写
        //    「mstatus 恢复由 CSR Bank 在 MRET 时自动处理」⇒ ⇒ **实测不成立** ——
        //    `CsrBank` 的 mstatus **只有写口**（`include/cpu/riscv/rv32i_csr.h` 的写路径）
        //    ⇒ ⇒ **没有**任何「MRET 自动恢复」路径 ⇒ ⇒ 该行为**当前不存在**。
        // ⇒ ⇒ 本组件**只**输出 `mret_valid` 表示「MRET 已被识别」⇒ ⇒ **恢复语义不在本组件**。
        // ⇒ ⇒ `mstatus` 的 MIE↔MPIE 更新 / `mret` 的 PC 恢复 ⇒ ⇒ **不在覆盖内**
        //    ⇒ ⇒ 属**特权级立项**（capability `rv32i-privilege` 的范围声明已登记）。
    }
};

} // namespace riscv
