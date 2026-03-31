/**
 * AXI4-Lite UART Controller
 * 
 * 将 Phase 1+2 的 UART TX 封装成 AXI4 从设备
 * 
 * 寄存器映射:
 * - 0x00: TX_DATA  - 发送数据寄存器
 * - 0x04: RX_DATA  - 接收数据寄存器
 * - 0x08: STATUS   - 状态寄存器 (TX_EMPTY, RX_VALID)
 * - 0x0C: CTRL     - 控制寄存器 (ENABLE, INTERRUPT_EN)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace axi4 {

template <unsigned PRESCALE_WIDTH = 16>
class AxiLiteUart : public ch::Component {
public:
    __io(
        // AXI4-Lite 接口
        ch_in<ch_uint<32>> awaddr;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;
        
        ch_in<ch_uint<32>> wdata;
        ch_in<ch_uint<4>> wstrb;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;
        
        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;
        
        ch_in<ch_uint<32>> araddr;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;
        
        ch_out<ch_uint<32>> rdata;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;
        
        // UART 物理接口
        ch_out<ch_bool> uart_tx;
        ch_in<ch_bool> uart_rx;
        ch_out<ch_bool> irq;
    )
    
    AxiLiteUart(ch::Component* parent = nullptr, const std::string& name = "axi_uart")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 状态机
        ch_reg<ch_bool> busy(ch_bool(false));
        
        // 寄存器
        ch_reg<ch_uint<32>> tx_data_reg(0_d);
        ch_reg<ch_uint<32>> rx_data_reg(0_d);
        ch_reg<ch_uint<32>> status_reg(0_d);
        ch_reg<ch_uint<32>> ctrl_reg(0_d);
        
        // UART 状态
        ch_reg<ch_bool> tx_empty(ch_bool(true));
        ch_reg<ch_bool> rx_valid(ch_bool(false));
        
        // 地址解码
        auto addr_bits = io().awaddr >> ch_uint<32>(2_d);
        addr_bits = addr_bits & ch_uint<32>(3_d);
        
        // 写地址握手
        auto aw_handshake = io().awvalid && (!busy);
        io().awready = aw_handshake;
        
        // 写数据握手
        auto w_handshake = io().wvalid && aw_handshake;
        io().wready = w_handshake;
        
        // 写寄存器
        auto sel_tx_data = (addr_bits == ch_uint<32>(0_d));
        auto sel_ctrl = (addr_bits == ch_uint<32>(3_d));
        
        tx_data_reg->next = select(w_handshake && sel_tx_data, io().wdata, tx_data_reg);
        ctrl_reg->next = select(w_handshake && sel_ctrl, io().wdata, ctrl_reg);
        
        // 写响应
        io().bvalid = w_handshake;
        io().bresp = ch_uint<2>(0_d);
        
        // 读地址握手
        auto ar_addr_bits = io().araddr >> ch_uint<32>(2_d);
        ar_addr_bits = ar_addr_bits & ch_uint<32>(3_d);
        
        auto ar_handshake = io().arvalid && (!busy);
        io().arready = ar_handshake;
        
        // 读数据
        auto read_sel_tx_data = (ar_addr_bits == ch_uint<32>(0_d));
        auto read_sel_rx_data = (ar_addr_bits == ch_uint<32>(1_d));
        auto read_sel_status = (ar_addr_bits == ch_uint<32>(2_d));
        auto read_sel_ctrl = (ar_addr_bits == ch_uint<32>(3_d));
        
        // 更新状态寄存器
        status_reg->next = select(tx_empty, status_reg | ch_uint<32>(1_d), status_reg & ch_uint<32>(~1_d));
        status_reg->next = select(rx_valid, status_reg | ch_uint<32>(2_d), status_reg & ch_uint<32>(~2_d));
        
        ch_uint<32> read_val(0_d);
        read_val = select(read_sel_tx_data, tx_data_reg, read_val);
        read_val = select(read_sel_rx_data, rx_data_reg, read_val);
        read_val = select(read_sel_status, status_reg, read_val);
        read_val = select(read_sel_ctrl, ctrl_reg, read_val);
        
        io().rdata = read_val;
        io().rvalid = ar_handshake;
        io().rresp = ch_uint<2>(0_d);
        
        // UART TX 简化实现 (写入 TX_DATA 后自动发送)
        io().uart_tx = select(tx_empty, ch_bool(true), ch_bool(false));
        io().irq = rx_valid;
        
        // 状态更新
        busy->next = select(aw_handshake || ar_handshake, ch_bool(true),
                            select(io().bready || io().rready, ch_bool(false), busy));
    }
};

} // namespace axi4
