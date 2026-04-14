/**
 * @file rv32i_system.h
 * @brief RV32I SYSTEM 指令执行单元
 * 
 * 功能说明:
 * 1. CSR 指令执行 (CSRRW/CSRRS/CSRRC/CSRRWI/CSRRSI/CSRRCI)
 * 2. 特权指令执行 (MRET/ECALL/EBREAK/WFI)
 * 3. CSR 读写数据路由到目标寄存器/CSR
 * 
 * CSR 指令类型 (funct3):
 * - 000: PRIV (MRET/ECALL/EBREAK/WFI)
 * - 001: CSRRW  (atomic read-write CSR)
 * - 010: CSRRS  (atomic read and set)
 * - 011: CSRRC  (atomic read and clear)
 * - 101: CSRRWI (atomic write CSR with immediate)
 * - 110: CSRRSI (atomic read and set immediate)
 * - 111: CSRRCI (atomic read and clear immediate)
 * 
 * 作者: DevMate
 * 日期: 2026-04-14
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

class SystemUnit : public ch::Component {
public:
    __io(
        // 输入: 来自译码器
        ch_in<ch_uint<12>> csr_addr;       // CSR 地址
        ch_in<ch_uint<32>> rs1_data;       // 源寄存器 rs1 的数据
        ch_in<ch_uint<32>> instr;          // 完整指令字
        ch_in<ch_bool>     is_csr;         // CSR 指令标志
        ch_in<ch_bool>     is_csr_imm;     // CSR 立即数指令 (CSRRWI/CSRRSI/CSRRCI)
        ch_in<ch_bool>     is_mret;        // MRET 指令
        ch_in<ch_bool>     is_ecall;       // ECALL 指令
        ch_in<ch_bool>     is_ebreak;      // EBREAK 指令
        ch_in<ch_bool>     is_wfi;         // WFI (Wait For Interrupt) 指令
        
        // CSR 寄存器组读接口
        ch_in<ch_uint<32>> csr_rdata;      // CSR 寄存器读出数据
        
        // 输出: 目标寄存器 rd 写接口
        ch_out<ch_uint<32>> rd_write_data; // 写入 rd 的数据
        ch_out<ch_bool>     rd_write_en;   // rd 写使能
        
        // 输出: CSR 寄存器写接口
        ch_out<ch_uint<12>> csr_waddr;     // CSR 写地址
        ch_out<ch_uint<32>> csr_wdata;     // CSR 写数据
        ch_out<ch_bool>     csr_write_en;  // CSR 写使能
        
        // 输出: 异常请求
        ch_out<ch_bool>     exception_req;  // 异常请求信号 (ECALL/EBREAK)
        ch_out<ch_uint<32>> exception_cause; // 异常原因码
        
        // 输出: MRET 相关
        ch_out<ch_uint<32>> mret_pc;       // MRET 返回地址 (来自 mepc)
        ch_out<ch_bool>     mret_valid;    // MRET 有效信号
    )
    
    SystemUnit(ch::Component* parent = nullptr, const std::string& name = "system_unit")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 常量定义
        // ========================================================================
        
        // ECALL/EBREAK 异常原因码 (RISC-V Privileged Spec v1.11)
        constexpr ch_uint<32> EXC_ECALL  = ch_uint<32>(8_d);   // Environment Call
        constexpr ch_uint<32> EXC_EBREAK = ch_uint<32>(3_d);   // Breakpoint
        
        // funct3 字段位置
        constexpr ch_uint<2> FUNCT3_LSB = ch_uint<2>(12_d);
        constexpr ch_uint<2> FUNCT3_MSB = ch_uint<2>(14_d);
        
        // ========================================================================
        // 从指令中提取 funct3 字段
        // ========================================================================
        
        // instr[14:12] = funct3
        ch_uint<3> funct3 = bits<FUNCT3_MSB, FUNCT3_LSB>(io().instr);
        
        // 判断具体 CSR 操作类型
        ch_bool is_csrrw  = (funct3 == ch_uint<3>(1_d));  // CSRRW
        ch_bool is_csrrs  = (funct3 == ch_uint<3>(2_d));  // CSRRS
        ch_bool is_csrrc  = (funct3 == ch_uint<3>(3_d));  // CSRRC
        ch_bool is_csrrwi = (funct3 == ch_uint<3>(5_d)); // CSRRWI
        ch_bool is_csrssi = (funct3 == ch_uint<3>(6_d)); // CSRRSI
        ch_bool is_csrci  = (funct3 == ch_uint<3>(7_d)); // CSRRCI
        
        // ========================================================================
        // 提取 rs1 字段作为立即数 (用于 CSRRWI/CSRRSI/CSRRCI)
        // instr[19:15] = rs1/uimm
        // ========================================================================
        
        ch_uint<5> rs1_uimm = bits<19, 15>(io().instr);
        
        // 零扩展到 32 位
        ch_uint<32> uimm32 = ch_uint<32>(rs1_uimm);
        
        // ========================================================================
        // CSR 读操作: 总是读取当前 CSR 值
        // ========================================================================
        
        // rd 写入数据: 总是 CSR 读出值
        io().rd_write_data = io().csr_rdata;
        
        // CSR 读始终有效 (rd 总是被写入)
        // 例外: rs1=0 的 CSRRS/CSRRC 不写入 rd，但这里简化为总是写入
        ch_bool csr_read_valid = io().is_csr;
        
        // ========================================================================
        // CSR 写操作: 根据指令类型计算写数据
        // ========================================================================
        
        // CSRRW: rs1 的值写入 CSR，rd = 原始 CSR 值
        // CSRRWI: uimm 写入 CSR，rd = 原始 CSR 值
        ch_uint<32> csr_wdata_csrrw = select(
            io().is_csr_imm,
            uimm32,
            io().rs1_data
        );
        
        // CSRRS: CSR | rs1 (设置指定位)
        // CSRRSI: CSR | uimm
        ch_uint<32> csr_wdata_csrrs = select(
            io().is_csr_imm,
            io().csr_rdata | uimm32,
            io().csr_rdata | io().rs1_data
        );
        
        // CSRRC: CSR & ~rs1 (清除指定位)
        // CSRRCI: CSR & ~uimm
        ch_uint<32> csr_wdata_csrrc = select(
            io().is_csr_imm,
            io().csr_rdata & (~uimm32),
            io().csr_rdata & (~io().rs1_data)
        );
        
        // 根据 funct3 选择最终 CSR 写数据
        ch_uint<32> csr_wdata_final = ch_uint<32>(0_d);
        csr_wdata_final = select(is_csrrw || is_csrrwi, csr_wdata_csrrw, csr_wdata_final);
        csr_wdata_final = select(is_csrrs || is_csrssi, csr_wdata_csrrs, csr_wdata_final);
        csr_wdata_final = select(is_csrrc || is_csrci,  csr_wdata_csrrc, csr_wdata_final);
        
        // ========================================================================
        // CSR 写使能逻辑
        // ========================================================================
        
        // CSRRW/CSRRWI: 总是写入 CSR
        ch_bool csr_write_csrrw = is_csrrw || is_csrrwi;
        
        // CSRRS/CSRRC: 只在 rs1/uimm != 0 时写入
        ch_bool rs1_nonzero = io().is_csr_imm 
            ? (rs1_uimm != ch_uint<5>(0_d)) 
            : (io().rs1_data != ch_uint<32>(0_d));
        
        ch_bool csr_write_csrrs = (is_csrrs || is_csrssi) && rs1_nonzero;
        ch_bool csr_write_csrrc = (is_csrrc || is_csrci)  && rs1_nonzero;
        
        // 汇总 CSR 写使能
        ch_bool csr_write_enable = csr_write_csrrw || csr_write_csrrs || csr_write_csrrc;
        
        // ========================================================================
        // 输出赋值
        // ========================================================================
        
        // CSR 写接口
        io().csr_waddr = io().csr_addr;
        io().csr_wdata = csr_wdata_final;
        io().csr_write_en = csr_write_enable && io().is_csr;
        
        // rd 写使能: CSR 指令总是写 rd (简化处理)
        io().rd_write_en = io().is_csr;
        
        // ========================================================================
        // ECALL 异常处理
        // ========================================================================
        
        // ECALL 触发环境调用异常
        io().exception_req  = io().is_ecall;
        io().exception_cause = EXC_ECALL;
        
        // ========================================================================
        // EBREAK 异常处理 (可选: 调试模式触发)
        // EBREAK 可以作为调试断点或异常处理
        // ========================================================================
        
        // 当前实现: EBREAK 也触发异常请求
        // 实际行为可能因调试模式而异
        io().exception_req = io().exception_req || io().is_ebreak;
        io().exception_cause = select(
            io().is_ebreak,
            EXC_EBREAK,
            io().exception_cause
        );
        
        // ========================================================================
        // WFI 处理
        // ========================================================================
        
        // WFI (Wait For Interrupt) 指令:
        // 提示处理器可以进入低功耗等待状态直到中断发生
        // 当前简化实现: 视为无操作 (NOP)
        // 实际实现需要暂停流水线直到中断或唤醒事件
        
        // ========================================================================
        // MRET 处理
        // ========================================================================
        
        // MRET 从 mepc 恢复 PC
        // 注意: mret_pc 实际来自 CSR 寄存器组 (mepc)
        // 这里仅输出控制信号，实际 PC 值由 ExceptionUnit 提供
        io().mret_valid = io().is_mret;
        
        // MRET 时不写 CSR (由 ExceptionUnit 处理 mstatus 恢复)
        // MRET 时不写 rd (rd=0)
        io().csr_write_en = select(io().is_mret, ch_bool(false), io().csr_write_en);
        io().rd_write_en  = select(io().is_mret, ch_bool(false), io().rd_write_en);
        
        // mret_pc 由 ExceptionUnit 通过 CSR Bank 提供
        // SystemUnit 输出 mret_valid 信号供流水线同步
        io().mret_pc = ch_uint<32>(0_d); // 占位，实际值来自 mepc
    }
};

} // namespace riscv
