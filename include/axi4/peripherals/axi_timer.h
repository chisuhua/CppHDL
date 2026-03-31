/**
 * AXI4-Lite Timer Controller
 * 
 * 将 Phase 1+2 的 Timer 封装成 AXI4 从设备
 * 
 * 寄存器映射:
 * - 0x00: LOAD     - 重载值寄存器
 * - 0x04: COUNT    - 当前计数值
 * - 0x08: CTRL     - 控制寄存器 (ENABLE, AUTO_RELOAD, INTERRUPT_EN)
 * - 0x0C: STATUS   - 状态寄存器 (TIMEOUT)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace axi4 {

template <unsigned COUNTER_WIDTH = 32>
class AxiLiteTimer : public ch::Component {
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
        
        // Timer 物理接口
        ch_out<ch_bool> irq;
    )
    
    AxiLiteTimer(ch::Component* parent = nullptr, const std::string& name = "axi_timer")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 状态机
        ch_reg<ch_bool> busy(ch_bool(false));
        
        // 寄存器
        ch_reg<ch_uint<32>> load_reg(0_d);
        ch_reg<ch_uint<32>> count_reg(0_d);
        ch_reg<ch_uint<32>> ctrl_reg(0_d);
        ch_reg<ch_uint<32>> status_reg(0_d);
        
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
        auto sel_load = (addr_bits == ch_uint<32>(0_d));
        auto sel_ctrl = (addr_bits == ch_uint<32>(2_d));
        
        load_reg->next = select(w_handshake && sel_load, io().wdata, load_reg);
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
        auto read_sel_load = (ar_addr_bits == ch_uint<32>(0_d));
        auto read_sel_count = (ar_addr_bits == ch_uint<32>(1_d));
        auto read_sel_ctrl = (ar_addr_bits == ch_uint<32>(2_d));
        auto read_sel_status = (ar_addr_bits == ch_uint<32>(3_d));
        
        ch_uint<32> read_val(0_d);
        read_val = select(read_sel_load, load_reg, read_val);
        read_val = select(read_sel_count, count_reg, read_val);
        read_val = select(read_sel_ctrl, ctrl_reg, read_val);
        read_val = select(read_sel_status, status_reg, read_val);
        
        io().rdata = read_val;
        io().rvalid = ar_handshake;
        io().rresp = ch_uint<2>(0_d);
        
        // 定时器逻辑 (简化)
        auto enable = (ctrl_reg & ch_uint<32>(1_d)) != ch_uint<32>(0_d);
        auto timeout = (count_reg == ch_uint<32>(0_d));
        
        count_reg->next = select(enable,
                                 select(timeout, load_reg, count_reg - ch_uint<32>(1_d)),
                                 count_reg);
        
        status_reg->next = select(timeout, status_reg | ch_uint<32>(1_d), status_reg);
        
        io().irq = (status_reg & ch_uint<32>(1_d)) != ch_uint<32>(0_d);
        
        // 状态更新
        busy->next = select(aw_handshake || ar_handshake, ch_bool(true),
                            select(io().bready || io().rready, ch_bool(false), busy));
    }
};

} // namespace axi4
