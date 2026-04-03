/**
 * @file rv32i_axi_interface.h
 * @brief RV32I AXI4 总线接口
 * 
 * 将 RV32I 核心连接到 AXI4 总线:
 * - 指令端口 (AXI4 Lite)
 * - 数据端口 (AXI4 Lite)
 * - 中断接口 (简化)
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "axi4/axi4_lite_slave.h"

using namespace ch::core;
using namespace axi4;

namespace riscv {

// ============================================================================
// RV32I AXI4 接口
// ============================================================================

class RV32IAxiInterface : public ch::Component {
public:
    __io(
        // ==================== RV32I 核心接口 ====================
        // 指令接口
        ch_out<ch_uint<32>> core_i_addr;
        ch_in<ch_bool> core_i_valid;
        ch_out<ch_bool> core_i_ready;
        ch_in<ch_uint<32>> core_i_data;
        
        // 数据接口
        ch_out<ch_uint<32>> core_d_addr;
        ch_out<ch_bool> core_d_write;
        ch_out<ch_uint<32>> core_d_wdata;
        ch_out<ch_bool> core_d_valid;
        ch_in<ch_bool> core_d_ready;
        ch_in<ch_uint<32>> core_d_rdata;
        
        // ==================== AXI4 主设备接口 ====================
        // 指令端口 (连接到 AXI Interconnect)
        ch_out<ch_uint<32>> axi_i_awaddr;
        ch_out<ch_bool> axi_i_awvalid;
        ch_in<ch_bool> axi_i_awready;
        
        ch_out<ch_uint<32>> axi_i_wdata;
        ch_out<ch_uint<4>> axi_i_wstrb;
        ch_out<ch_bool> axi_i_wvalid;
        ch_in<ch_bool> axi_i_wready;
        
        ch_in<ch_uint<2>> axi_i_bresp;
        ch_in<ch_bool> axi_i_bvalid;
        ch_out<ch_bool> axi_i_bready;
        
        ch_out<ch_uint<32>> axi_i_araddr;
        ch_out<ch_bool> axi_i_arvalid;
        ch_in<ch_bool> axi_i_arready;
        
        ch_in<ch_uint<32>> axi_i_rdata;
        ch_in<ch_uint<2>> axi_i_rresp;
        ch_in<ch_bool> axi_i_rvalid;
        ch_out<ch_bool> axi_i_rready;
        
        // 数据端口 (连接到 AXI Interconnect)
        ch_out<ch_uint<32>> axi_d_awaddr;
        ch_out<ch_bool> axi_d_awvalid;
        ch_in<ch_bool> axi_d_awready;
        
        ch_out<ch_uint<32>> axi_d_wdata;
        ch_out<ch_uint<4>> axi_d_wstrb;
        ch_out<ch_bool> axi_d_wvalid;
        ch_in<ch_bool> axi_d_wready;
        
        ch_in<ch_uint<2>> axi_d_bresp;
        ch_in<ch_bool> axi_d_bvalid;
        ch_out<ch_bool> axi_d_bready;
        
        ch_out<ch_uint<32>> axi_d_araddr;
        ch_out<ch_bool> axi_d_arvalid;
        ch_in<ch_bool> axi_d_arready;
        
        ch_in<ch_uint<32>> axi_d_rdata;
        ch_in<ch_uint<2>> axi_d_rresp;
        ch_in<ch_bool> axi_d_rvalid;
        ch_out<ch_bool> axi_d_rready;
    )
    
    RV32IAxiInterface(ch::Component* parent = nullptr, const std::string& name = "rv32i_axi")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 指令端口逻辑 (简化：仅读)
        // ========================================================================
        
        // AR 通道：读地址
        io().axi_i_araddr <<= io().core_i_addr;
        io().axi_i_arvalid <<= io().core_i_valid;
        io().core_i_ready <<= io().axi_i_arready;
        
        // R 通道：读数据
        io().core_i_data <<= io().axi_i_rdata;
        io().axi_i_rready <<= io().core_i_valid;
        
        // AW/W/B 通道：未使用 (指令端口只读)
        io().axi_i_awvalid <<= ch_bool(false);
        io().axi_i_wvalid <<= ch_bool(false);
        io().axi_i_bready <<= ch_bool(true);
        
        // ========================================================================
        // 数据端口逻辑 (读/写)
        // ========================================================================
        
        // 写地址通道
        io().axi_d_awaddr <<= io().core_d_addr;
        io().axi_d_awvalid <<= select(io().core_d_write, io().core_d_valid, ch_bool(false));
        
        // 写数据通道
        io().axi_d_wdata <<= io().core_d_wdata;
        io().axi_d_wstrb <<= ch_uint<4>(0xF_d);  // 全字节选通
        io().axi_d_wvalid <<= select(io().core_d_write, io().core_d_valid, ch_bool(false));
        
        // 写响应
        io().axi_d_bready <<= select(io().core_d_write, io().core_d_valid, ch_bool(false));
        io().core_d_ready <<= select(io().core_d_write, io().axi_d_bvalid, io().axi_d_rvalid);
        
        // 读地址通道
        io().axi_d_araddr <<= select(!io().core_d_write, io().core_d_addr, ch_uint<32>(0_d));
        io().axi_d_arvalid <<= select(!io().core_d_write, io().core_d_valid, ch_bool(false));
        
        // 读数据通道
        io().core_d_rdata <<= io().axi_d_rdata;
        io().axi_d_rready <<= select(!io().core_d_write, io().core_d_valid, ch_bool(false));
    }
};

// ============================================================================
// PLIC 简化版 (Platform-Level Interrupt Controller)
// ============================================================================

class SimplePLIC : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> irq_input;
        ch_out<ch_bool> irq_output;
        ch_in<ch_bool> mie;  // Machine Interrupt Enable
    )
    
    SimplePLIC(ch::Component* parent = nullptr, const std::string& name = "plic")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 简化：任意中断输入触发输出
        auto irq_pending = (io().irq_input != ch_uint<8>(0_d));
        io().irq_output = select(io().mie, irq_pending, ch_bool(false));
    }
};

} // namespace riscv
