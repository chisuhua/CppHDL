/**
 * @file rv32i_csr.h
 * @brief RV32I CSR (Control and Status Register) Bank
 * 
 * RISC-V Privileged Spec v1.11 定义的 CSR 寄存器组:
 * - mstatus: 机器状态寄存器
 * - misa: ISA 寄存器
 * - mie: 机器中断使能寄存器
 * - mtvec: 机器陷阱向量寄存器
 * - mscratch: 机器 Scratch 寄存器
 * - mepc: 机器异常程序计数器
 * - mcause: 机器异常原因寄存器
 * - mtval: 机器异常值寄存器
 * - mip: 机器中断待处理寄存器
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

// CSR 地址定义 (RISC-V Privileged Spec v1.11)
constexpr uint32_t CSR_MSTATUS  = 0x300;   // 机器状态
constexpr uint32_t CSR_MISA     = 0x301;   // ISA
constexpr uint32_t CSR_MIE      = 0x304;   // 机器中断使能
constexpr uint32_t CSR_MTVEC    = 0x305;   // 陷阱向量
constexpr uint32_t CSR_MSCRATCH = 0x340;   // Scratch
constexpr uint32_t CSR_MEPC     = 0x341;   // 异常PC
constexpr uint32_t CSR_MCAUSE   = 0x342;   // 异常原因
constexpr uint32_t CSR_MTVAL    = 0x343;   // 异常值
constexpr uint32_t CSR_MIP      = 0x344;   // 中断待处理

class CsrBank : public ch::Component {
public:
    __io(
        // CSR 访问接口
        ch_in<ch_uint<12>>  csr_addr;       // CSR 地址 (12-bit)
        ch_in<ch_uint<32>>  csr_wdata;      // 写数据
        ch_in<ch_bool>      csr_write_en;   // 写使能
        ch_out<ch_uint<32>> csr_rdata;      // 读数据
        
        // 中断输入 (来自 CLINT 和外部)
        ch_in<ch_bool> mtip;   // 机器定时器中断待处理
        ch_in<ch_bool> meip;   // 机器外部中断待处理
        
        // 中断请求输出
        ch_out<ch_bool> interrupt_req;
        
        // 单独的中断待处理信号 (供 ExceptionUnit 确定中断类型)
        ch_out<ch_bool> timer_interrupt_pending;
        ch_out<ch_bool> external_interrupt_pending;
        
        // mstatus 字段访问器 (用于流水线阶段)
        ch_out<ch_bool>    mstatus_mie;
        ch_out<ch_bool>    mstatus_mpie;
        ch_out<ch_uint<2>> mstatus_mpp;
        
        // CSR 寄存器值输出 (供 ExceptionUnit 使用)
        ch_out<ch_uint<32>> mstatus_value;
        ch_out<ch_uint<32>> mtvec_value;
        ch_out<ch_uint<32>> mepc_value;
        ch_out<ch_uint<32>> mie_value;
        ch_out<ch_uint<32>> mcause_value;
    )
    
    CsrBank(ch::Component* parent = nullptr, const std::string& name = "csr_bank")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // CSR 寄存器定义 (带复位值)
        // ========================================================================
        
        // mstatus: 0x00000080
        //   MIE(3)=0, MPIE(7)=1, MPP(11:12)=0
        ch_reg<ch_uint<32>> mstatus(0x80);
        
        // misa: 0x40101101 (RV32I)
        //   MXL(31:30)=01 (32位), EXT(25:20)=0, EXPERIMENTAL(19:18)=00
        //   Base(7:0)=01 (I扩展), others=100000 (20位)
        ch_reg<ch_uint<32>> misa(0x40101101);
        
        // mie: 0x00000000 (所有中断初始禁用)
        ch_reg<ch_uint<32>> mie(0x0);
        
        // mtvec: 0x20000100
        //   BASE(31:2)=0x20000000, MODE(1:0)=01 (Direct模式)
        ch_reg<ch_uint<32>> mtvec(0x20000100);
        
        // mscratch: 0x00000000
        ch_reg<ch_uint<32>> mscratch(0x0);
        
        // mepc: 0x00000000
        ch_reg<ch_uint<32>> mepc(0x0);
        
        // mcause: 0x00000000
        ch_reg<ch_uint<32>> mcause(0x0);
        
        // mtval: 0x00000000
        ch_reg<ch_uint<32>> mtval(0x0);
        
        // mip: 0x00000000 (只读，软件不可写)
        ch_reg<ch_uint<32>> mip(0x0);
        
        // ========================================================================
        // mip 更新逻辑 (组合逻辑，取决于外部输入)
        // ========================================================================
        
        // mip[7] = mtip (定时器中断)
        // mip[11] = meip (外部中断)
        ch_uint<32> mip_with_mtip = mip;
        mip_with_mtip = select(io().mtip, mip | ch_uint<32>(0x80), mip);
        ch_uint<32> mip_with_meip = mip_with_mtip;
        mip_with_meip = select(io().meip, mip_with_mtip | ch_uint<32>(0x800), mip_with_mtip);
        mip->next = mip_with_meip;
        
        // ========================================================================
        // CSR 读逻辑 (组合逻辑)
        // ========================================================================
        
        ch_uint<32> rdata(0_d);
        
        // 根据 csr_addr 选择要读取的寄存器
        auto sel_mstatus  = (io().csr_addr == CSR_MSTATUS);
        auto sel_misa     = (io().csr_addr == CSR_MISA);
        auto sel_mie      = (io().csr_addr == CSR_MIE);
        auto sel_mtvec    = (io().csr_addr == CSR_MTVEC);
        auto sel_mscratch = (io().csr_addr == CSR_MSCRATCH);
        auto sel_mepc     = (io().csr_addr == CSR_MEPC);
        auto sel_mcause   = (io().csr_addr == CSR_MCAUSE);
        auto sel_mtval    = (io().csr_addr == CSR_MTVAL);
        auto sel_mip      = (io().csr_addr == CSR_MIP);
        
        rdata = select(sel_mstatus,  mstatus,  rdata);
        rdata = select(sel_misa,     misa,     rdata);
        rdata = select(sel_mie,      mie,      rdata);
        rdata = select(sel_mtvec,    mtvec,    rdata);
        rdata = select(sel_mscratch, mscratch, rdata);
        rdata = select(sel_mepc,     mepc,     rdata);
        rdata = select(sel_mcause,   mcause,   rdata);
        rdata = select(sel_mtval,    mtval,    rdata);
        rdata = select(sel_mip,      mip,      rdata);
        
        io().csr_rdata = rdata;
        
        // ========================================================================
        // CSR 写逻辑 (时序逻辑)
        // ========================================================================
        
        // 写使能且地址匹配时更新寄存器
        auto we = io().csr_write_en;
        
        mstatus->next = select(
            we & sel_mstatus,
            io().csr_wdata,
            mstatus
        );
        
        // misa 是只读的，写入被忽略
        // mie 可写
        mie->next = select(
            we & sel_mie,
            io().csr_wdata,
            mie
        );
        
        mtvec->next = select(
            we & sel_mtvec,
            io().csr_wdata,
            mtvec
        );
        
        mscratch->next = select(
            we & sel_mscratch,
            io().csr_wdata,
            mscratch
        );
        
        mepc->next = select(
            we & sel_mepc,
            io().csr_wdata,
            mepc
        );
        
        mcause->next = select(
            we & sel_mcause,
            io().csr_wdata,
            mcause
        );
        
        mtval->next = select(
            we & sel_mtval,
            io().csr_wdata,
            mtval
        );
        
        // mip 是只读的，由硬件更新
        
        // ========================================================================
        // mstatus 字段访问器输出
        // ========================================================================
        
        // mstatus_mie: mstatus[3]
        io().mstatus_mie = bits<3, 3>(mstatus);
        
        // mstatus_mpie: mstatus[7]
        io().mstatus_mpie = bits<7, 7>(mstatus);
        
        // mstatus_mpp: mstatus[11:12]
        io().mstatus_mpp = bits<12, 11>(mstatus);
        
        // CSR 寄存器值输出
        io().mstatus_value = mstatus;
        io().mtvec_value = mtvec;
        io().mepc_value = mepc;
        io().mie_value = mie;
        io().mcause_value = mcause;
        
        // ========================================================================
        // 中断请求生成逻辑
        // ========================================================================
        
        // 中断使能位
        ch_bool mie_mtie = bits<7, 7>(mie);   // MTIE: 定时器中断使能
        ch_bool mie_meie = bits<11, 11>(mie);  // MEIE: 外部中断使能
        ch_bool mstatus_mie_bit = bits<3, 3>(mstatus);  // MIE: 全局中断使能
        
        // 定时器中断待处理: mtip && MTIE && MIE
        ch_bool timer_interrupt_pending = io().mtip & mie_mtie & mstatus_mie_bit;
        
        // 外部中断待处理: meip && MEIE && MIE
        ch_bool external_interrupt_pending = io().meip & mie_meie & mstatus_mie_bit;
        
        // 全局中断请求
        io().interrupt_req = timer_interrupt_pending | external_interrupt_pending;
        
        // 输出单独的中断待处理信号
        io().timer_interrupt_pending = timer_interrupt_pending;
        io().external_interrupt_pending = external_interrupt_pending;
    }

};

} // namespace riscv
