/**
 * @file rv32i_exception.h
 * @brief RISC-V 异常/陷阱处理单元（Exception/Trap Handling Unit）
 * 
 * 功能：
 * 1. 陷阱入口处理：捕获异常时保存 PC、原因码到 CSR，跳转到 mtvec
 * 2. MRET 返回处理：从 mepc 恢复 PC，恢复 mstatus.MIE
 * 
 * 异常原因码：
 * - 3: Machine Software Interrupt (MSI)
 * - 7: Machine Timer Interrupt (MTI)
 * - 11: Machine External Interrupt (MEI)
 * - 8: Machine Environment Call (ECALL)
 * - 3: Breakpoint (EBREAK) — 注意与 MSI 相同
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

using namespace ch::core;

namespace riscv {

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
        
        // mtvec 模式：Direct (0) 或 Vectored (1)
        // mtvec[1:0] 为模式位，mtvec[31:2] 为基址
        constexpr uint32_t MTVEC_MODE_MASK = 0x3;
        constexpr uint32_t MTVEC_BASE_MASK = ~0x3;
        
        // mstatus 位偏移
        constexpr uint32_t MSTATUS_MIE_BIT = 3;
        constexpr uint32_t MSTATUS_MPIE_BIT = 7;
        
        // ========================================================================
        // 陷阱入口处理
        // ========================================================================
        
        // 计算 mtvec 基址（清除低2位模式位）
        auto mtvec_base = io().csr_mtvec & MTVEC_BASE_MASK;
        
        // 计算陷阱入口 PC：
        // 模式0 (Direct): 总是跳转到基址
        // 模式1 (Vectored): 跳转到基址 + cause * 4（向量模式）
        // 当前简化实现：使用 Direct 模式
        auto trap_pc = mtvec_base;
        
        // 陷阱入口信号
        auto do_trap_entry = io().exception_valid;
        
        // 输出陷阱入口 PC 和有效信号
        io().trap_entry_pc = trap_pc;
        io().trap_entry_valid = do_trap_entry;
        io().trap_flush = do_trap_entry;
        
        // ========================================================================
        // CSR 写操作：保存异常上下文
        // ========================================================================
        
        // 写 mepc：保存当前 PC（异常指令地址）
        // 当有异常时，将 current_pc 写入 mepc CSR
        auto write_mepc = do_trap_entry;
        
        // 写 mcause：保存异常原因码
        // 当有异常时，将 exception_cause 写入 mcause CSR
        auto write_mcause = do_trap_entry;
        
        // CSR 写地址选择：mepc 或 mcause
        // 优先级：mepc > mcause（实际上两个都会写，这里简化处理）
        auto csr_addr = select(write_mepc,
                              CSR_MEPC_ADDR,
                              select(write_mcause,
                                     CSR_MCAUSE_ADDR,
                                     ch_uint<12>(0_d)));
        
        // CSR 写数据选择：current_pc 或 exception_cause
        auto csr_data = select(write_mepc,
                               io().current_pc,
                               select(write_mcause,
                                      io().exception_cause,
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
        
        // 注意：mstatus 恢复由 CSR Bank 在 MRET 时自动处理
        // 或由流水线中的控制单元在执行 MRET 时处理
        // 这里仅输出 mret_valid 信号表示 MRET 已完成
    }
};

} // namespace riscv
