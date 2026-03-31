/**
 * AXI4-Lite 2x2 Matrix
 * 
 * 连接 2 个主设备到 2 个从设备，支持：
 * - 轮询仲裁
 * - 地址解码
 * - 错误响应
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace axi4 {

template <unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32>
class AxiLiteMatrix2x2 : public ch::Component {
public:
    __io(
        // 主设备 0 接口
        ch_out<ch_uint<ADDR_WIDTH>> m0_awaddr;
        ch_out<ch_uint<2>> m0_awprot;
        ch_out<ch_bool> m0_awvalid;
        ch_in<ch_bool> m0_awready;
        
        ch_out<ch_uint<DATA_WIDTH>> m0_wdata;
        ch_out<ch_uint<DATA_WIDTH/8>> m0_wstrb;
        ch_out<ch_bool> m0_wvalid;
        ch_in<ch_bool> m0_wready;
        
        ch_in<ch_uint<2>> m0_bresp;
        ch_in<ch_bool> m0_bvalid;
        ch_out<ch_bool> m0_bready;
        
        ch_out<ch_uint<ADDR_WIDTH>> m0_araddr;
        ch_out<ch_uint<2>> m0_arprot;
        ch_out<ch_bool> m0_arvalid;
        ch_in<ch_bool> m0_arready;
        
        ch_in<ch_uint<DATA_WIDTH>> m0_rdata;
        ch_in<ch_uint<2>> m0_rresp;
        ch_in<ch_bool> m0_rvalid;
        ch_out<ch_bool> m0_rready;
        
        // 主设备 1 接口 (简化)
        ch_out<ch_uint<ADDR_WIDTH>> m1_awaddr;
        ch_out<ch_bool> m1_awvalid;
        ch_in<ch_bool> m1_awready;
        
        ch_out<ch_uint<DATA_WIDTH>> m1_wdata;
        ch_out<ch_bool> m1_wvalid;
        ch_in<ch_bool> m1_wready;
        
        ch_in<ch_bool> m1_bvalid;
        ch_out<ch_bool> m1_bready;
        
        ch_out<ch_uint<ADDR_WIDTH>> m1_araddr;
        ch_out<ch_bool> m1_arvalid;
        ch_in<ch_bool> m1_arready;
        
        ch_in<ch_uint<DATA_WIDTH>> m1_rdata;
        ch_in<ch_bool> m1_rvalid;
        ch_out<ch_bool> m1_rready;
        
        // 从设备 0 接口
        ch_in<ch_uint<ADDR_WIDTH>> s0_awaddr;
        ch_in<ch_bool> s0_awvalid;
        ch_out<ch_bool> s0_awready;
        
        ch_in<ch_uint<DATA_WIDTH>> s0_wdata;
        ch_in<ch_bool> s0_wvalid;
        ch_out<ch_bool> s0_wready;
        
        ch_out<ch_uint<2>> s0_bresp;
        ch_out<ch_bool> s0_bvalid;
        ch_in<ch_bool> s0_bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> s0_araddr;
        ch_in<ch_bool> s0_arvalid;
        ch_out<ch_bool> s0_arready;
        
        ch_out<ch_uint<DATA_WIDTH>> s0_rdata;
        ch_out<ch_uint<2>> s0_rresp;
        ch_out<ch_bool> s0_rvalid;
        ch_in<ch_bool> s0_rready;
        
        // 从设备 1 接口
        ch_in<ch_uint<ADDR_WIDTH>> s1_awaddr;
        ch_in<ch_bool> s1_awvalid;
        ch_out<ch_bool> s1_awready;
        
        ch_in<ch_uint<DATA_WIDTH>> s1_wdata;
        ch_in<ch_bool> s1_wvalid;
        ch_out<ch_bool> s1_wready;
        
        ch_out<ch_uint<2>> s1_bresp;
        ch_out<ch_bool> s1_bvalid;
        ch_in<ch_bool> s1_bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> s1_araddr;
        ch_in<ch_bool> s1_arvalid;
        ch_out<ch_bool> s1_arready;
        
        ch_out<ch_uint<DATA_WIDTH>> s1_rdata;
        ch_out<ch_uint<2>> s1_rresp;
        ch_out<ch_bool> s1_rvalid;
        ch_in<ch_bool> s1_rready;
    )
    
    AxiLiteMatrix2x2(ch::Component* parent = nullptr, const std::string& name = "axi_matrix_2x2")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 仲裁器：轮询
        ch_reg<ch_bool> arb_state(ch_bool(false));  // 0=主设备 0, 1=主设备 1
        
        // 地址解码：高位选择从设备
        // 0x0000_0000 - 0x7FFF_FFFF → 从设备 0
        // 0x8000_0000 - 0xFFFF_FFFF → 从设备 1
        auto addr_bit_m0 = (m0_awaddr >> ch_uint<ADDR_WIDTH>(ADDR_WIDTH-1_d));
        auto addr_bit_m1 = (m1_awaddr >> ch_uint<ADDR_WIDTH>(ADDR_WIDTH-1_d));
        
        auto select_s1_m0 = (addr_bit_m0 != ch_uint<ADDR_WIDTH>(0_d));
        auto select_s1_m1 = (addr_bit_m1 != ch_uint<ADDR_WIDTH>(0_d));
        
        // 仲裁逻辑
        auto m0_req = m0_awvalid;
        auto m1_req = m1_awvalid;
        
        // 选择哪个主设备
        auto grant_m0 = (!arb_state) && m0_req;
        auto grant_m1 = arb_state && m1_req;
        
        // 更新仲裁器
        auto transaction_done = s0_awready || s1_awready;
        arb_state->next = select(transaction_done, !arb_state, arb_state);
        
        // ==================== 写地址通道 ====================
        
        // 主设备 0 → 从设备
        s0_awaddr = select(grant_m0 && (!select_s1_m0), m0_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        s1_awaddr = select(grant_m0 && select_s1_m0, m0_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        
        // 主设备 1 → 从设备
        s0_awaddr = select(grant_m1 && (!select_s1_m1), m1_awaddr, s0_awaddr);
        s1_awaddr = select(grant_m1 && select_s1_m1, m1_awaddr, s1_awaddr);
        
        // 从设备就绪
        s0_awready = select(grant_m0 && (!select_s1_m0), m0_awready,
                            select(grant_m1 && (!select_s1_m1), m1_awready, ch_bool(false)));
        s1_awready = select(grant_m0 && select_s1_m0, m0_awready,
                            select(grant_m1 && select_s1_m1, m1_awready, ch_bool(false)));
        
        // 主设备就绪
        m0_awready = select(!select_s1_m0, s0_awready, s1_awready);
        m1_awready = select(!select_s1_m1, s0_awready, s1_awready);
        
        // ==================== 写数据通道 (简化) ====================
        
        s0_wdata = select(grant_m0 && (!select_s1_m0), m0_wdata,
                          select(grant_m1 && (!select_s1_m1), m1_wdata, ch_uint<DATA_WIDTH>(0_d)));
        s1_wdata = select(grant_m0 && select_s1_m0, m0_wdata,
                          select(grant_m1 && select_s1_m1, m1_wdata, ch_uint<DATA_WIDTH>(0_d)));
        
        s0_wready = select(grant_m0 && (!select_s1_m0), m0_wready,
                           select(grant_m1 && (!select_s1_m1), m1_wready, ch_bool(false)));
        s1_wready = select(grant_m0 && select_s1_m0, m0_wready,
                           select(grant_m1 && select_s1_m1, m1_wready, ch_bool(false)));
        
        m0_wready = select(!select_s1_m0, s0_wready, s1_wready);
        m1_wready = select(!select_s1_m1, s0_wready, s1_wready);
        
        // ==================== 写响应通道 (简化) ====================
        
        m0_bresp = select(!select_s1_m0, s0_bresp, s1_bresp);
        m1_bresp = select(!select_s1_m1, s0_bresp, s1_bresp);
        
        m0_bvalid = select(!select_s1_m0, s0_bvalid, s1_bvalid);
        m1_bvalid = select(!select_s1_m1, s0_bvalid, s1_bvalid);
        
        s0_bready = select(grant_m0 && (!select_s1_m0), m0_bready,
                           select(grant_m1 && (!select_s1_m1), m1_bready, ch_bool(false)));
        s1_bready = select(grant_m0 && select_s1_m0, m0_bready,
                           select(grant_m1 && select_s1_m1, m1_bready, ch_bool(false)));
        
        // ==================== 读地址通道 (简化) ====================
        
        s0_araddr = select(grant_m0 && (!select_s1_m0), m0_araddr,
                           select(grant_m1 && (!select_s1_m1), m1_araddr, ch_uint<ADDR_WIDTH>(0_d)));
        s1_araddr = select(grant_m0 && select_s1_m0, m0_araddr,
                           select(grant_m1 && select_s1_m1, m1_araddr, ch_uint<ADDR_WIDTH>(0_d)));
        
        s0_arready = select(grant_m0 && (!select_s1_m0), m0_arready,
                            select(grant_m1 && (!select_s1_m1), m1_arready, ch_bool(false)));
        s1_arready = select(grant_m0 && select_s1_m0, m0_arready,
                            select(grant_m1 && select_s1_m1, m1_arready, ch_bool(false)));
        
        m0_arready = select(!select_s1_m0, s0_arready, s1_arready);
        m1_arready = select(!select_s1_m1, s0_arready, s1_arready);
        
        // ==================== 读数据通道 (简化) ====================
        
        m0_rdata = select(!select_s1_m0, s0_rdata, s1_rdata);
        m1_rdata = select(!select_s1_m1, s0_rdata, s1_rdata);
        
        m0_rvalid = select(!select_s1_m0, s0_rvalid, s1_rvalid);
        m1_rvalid = select(!select_s1_m1, s0_rvalid, s1_rvalid);
        
        s0_rready = select(grant_m0 && (!select_s1_m0), m0_rready,
                           select(grant_m1 && (!select_s1_m1), m1_rready, ch_bool(false)));
        s1_rready = select(grant_m0 && select_s1_m0, m0_rready,
                           select(grant_m1 && select_s1_m1, m1_rready, ch_bool(false)));
    }
};

} // namespace axi4
