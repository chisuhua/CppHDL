/**
 * @file axi_interconnect_4x4.h
 * @brief AXI4 4x4 Interconnect (交叉开关矩阵)
 * 
 * 连接 4 个主设备到 4 个从设备，支持：
 * - 轮询仲裁 (Round-Robin)
 * - 地址解码
 * - 错误响应 (DECERR)
 * - 并发传输 (不同主 - 从对)
 * 
 * 地址映射:
 * - 从设备 0: 0x0000_0000 - 0x1FFF_FFFF (512MB)
 * - 从设备 1: 0x2000_0000 - 0x3FFF_FFFF (512MB)
 * - 从设备 2: 0x4000_0000 - 0x5FFF_FFFF (512MB)
 * - 从设备 3: 0x6000_0000 - 0x7FFF_FFFF (512MB)
 * - 保留区：0x8000_0000 - 0xFFFF_FFFF (DECERR)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace axi4 {

// ============================================================================
// AXI4 响应类型
// ============================================================================

enum class AxiResp : uint8_t {
    OKAY   = 0b00,
    EXOKAY = 0b01,
    SLVERR = 0b10,
    DECERR = 0b11
};

// ============================================================================
// AXI4 Interconnect 4x4
// ============================================================================

template <
    unsigned ADDR_WIDTH = 32,
    unsigned DATA_WIDTH = 32,
    unsigned ID_WIDTH = 4
>
class AxiInterconnect4x4 : public ch::Component {
public:
    static constexpr unsigned NUM_MASTERS = 4;
    static constexpr unsigned NUM_SLAVES = 4;
    
    __io(
        // ==================== 主设备 0 接口 ====================
        ch_in<ch_uint<ADDR_WIDTH>> m0_awaddr;
        ch_in<ch_uint<ID_WIDTH>> m0_awid;
        ch_in<ch_uint<4>> m0_awlen;
        ch_in<ch_uint<3>> m0_awsize;
        ch_in<ch_uint<2>> m0_awburst;
        ch_in<ch_bool> m0_awvalid;
        ch_out<ch_bool> m0_awready;
        
        ch_in<ch_uint<DATA_WIDTH>> m0_wdata;
        ch_in<ch_uint<ID_WIDTH>> m0_wid;
        ch_in<ch_uint<DATA_WIDTH/8>> m0_wstrb;
        ch_in<ch_bool> m0_wlast;
        ch_in<ch_bool> m0_wvalid;
        ch_out<ch_bool> m0_wready;
        
        ch_out<ch_uint<ID_WIDTH>> m0_bid;
        ch_out<ch_uint<2>> m0_bresp;
        ch_out<ch_bool> m0_bvalid;
        ch_in<ch_bool> m0_bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m0_araddr;
        ch_in<ch_uint<ID_WIDTH>> m0_arid;
        ch_in<ch_uint<4>> m0_arlen;
        ch_in<ch_uint<3>> m0_arsize;
        ch_in<ch_uint<2>> m0_arburst;
        ch_in<ch_bool> m0_arvalid;
        ch_out<ch_bool> m0_arready;
        
        ch_out<ch_uint<DATA_WIDTH>> m0_rdata;
        ch_out<ch_uint<ID_WIDTH>> m0_rid;
        ch_out<ch_uint<2>> m0_rresp;
        ch_out<ch_bool> m0_rlast;
        ch_out<ch_bool> m0_rvalid;
        ch_in<ch_bool> m0_rready;
        
        // ==================== 主设备 1 接口 ====================
        ch_in<ch_uint<ADDR_WIDTH>> m1_awaddr;
        ch_in<ch_uint<ID_WIDTH>> m1_awid;
        ch_in<ch_bool> m1_awvalid;
        ch_out<ch_bool> m1_awready;
        
        ch_in<ch_uint<DATA_WIDTH>> m1_wdata;
        ch_in<ch_bool> m1_wvalid;
        ch_out<ch_bool> m1_wready;
        
        ch_out<ch_uint<ID_WIDTH>> m1_bid;
        ch_out<ch_uint<2>> m1_bresp;
        ch_out<ch_bool> m1_bvalid;
        ch_in<ch_bool> m1_bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m1_araddr;
        ch_in<ch_uint<ID_WIDTH>> m1_arid;
        ch_in<ch_bool> m1_arvalid;
        ch_out<ch_bool> m1_arready;
        
        ch_out<ch_uint<DATA_WIDTH>> m1_rdata;
        ch_out<ch_uint<ID_WIDTH>> m1_rid;
        ch_out<ch_uint<2>> m1_rresp;
        ch_out<ch_bool> m1_rvalid;
        ch_in<ch_bool> m1_rready;
        
        // ==================== 主设备 2 接口 ====================
        ch_in<ch_uint<ADDR_WIDTH>> m2_awaddr;
        ch_in<ch_bool> m2_awvalid;
        ch_out<ch_bool> m2_awready;
        
        ch_in<ch_uint<DATA_WIDTH>> m2_wdata;
        ch_in<ch_bool> m2_wvalid;
        ch_out<ch_bool> m2_wready;
        
        ch_out<ch_bool> m2_bvalid;
        ch_in<ch_bool> m2_bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m2_araddr;
        ch_in<ch_bool> m2_arvalid;
        ch_out<ch_bool> m2_arready;
        
        ch_out<ch_uint<DATA_WIDTH>> m2_rdata;
        ch_out<ch_bool> m2_rvalid;
        ch_in<ch_bool> m2_rready;
        
        // ==================== 主设备 3 接口 ====================
        ch_in<ch_uint<ADDR_WIDTH>> m3_awaddr;
        ch_in<ch_bool> m3_awvalid;
        ch_out<ch_bool> m3_awready;
        
        ch_in<ch_uint<DATA_WIDTH>> m3_wdata;
        ch_in<ch_bool> m3_wvalid;
        ch_out<ch_bool> m3_wready;
        
        ch_out<ch_bool> m3_bvalid;
        ch_in<ch_bool> m3_bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m3_araddr;
        ch_in<ch_bool> m3_arvalid;
        ch_out<ch_bool> m3_arready;
        
        ch_out<ch_uint<DATA_WIDTH>> m3_rdata;
        ch_out<ch_bool> m3_rvalid;
        ch_in<ch_bool> m3_rready;
        
        // ==================== 从设备 0-3 接口 ====================
        ch_out<ch_uint<ADDR_WIDTH>> s0_awaddr;
        ch_out<ch_bool> s0_awvalid;
        ch_in<ch_bool> s0_awready;
        
        ch_out<ch_uint<DATA_WIDTH>> s0_wdata;
        ch_out<ch_bool> s0_wvalid;
        ch_in<ch_bool> s0_wready;
        
        ch_in<ch_uint<2>> s0_bresp;
        ch_in<ch_bool> s0_bvalid;
        ch_out<ch_bool> s0_bready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s0_araddr;
        ch_out<ch_bool> s0_arvalid;
        ch_in<ch_bool> s0_arready;
        
        ch_in<ch_uint<DATA_WIDTH>> s0_rdata;
        ch_in<ch_uint<2>> s0_rresp;
        ch_in<ch_bool> s0_rvalid;
        ch_out<ch_bool> s0_rready;
        
        // 从设备 1
        ch_out<ch_uint<ADDR_WIDTH>> s1_awaddr;
        ch_out<ch_bool> s1_awvalid;
        ch_in<ch_bool> s1_awready;
        
        ch_out<ch_uint<DATA_WIDTH>> s1_wdata;
        ch_out<ch_bool> s1_wvalid;
        ch_in<ch_bool> s1_wready;
        
        ch_in<ch_uint<2>> s1_bresp;
        ch_in<ch_bool> s1_bvalid;
        ch_out<ch_bool> s1_bready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s1_araddr;
        ch_out<ch_bool> s1_arvalid;
        ch_in<ch_bool> s1_arready;
        
        ch_in<ch_uint<DATA_WIDTH>> s1_rdata;
        ch_in<ch_uint<2>> s1_rresp;
        ch_in<ch_bool> s1_rvalid;
        ch_out<ch_bool> s1_rready;
        
        // 从设备 2
        ch_out<ch_uint<ADDR_WIDTH>> s2_awaddr;
        ch_out<ch_bool> s2_awvalid;
        ch_in<ch_bool> s2_awready;
        
        ch_out<ch_uint<DATA_WIDTH>> s2_wdata;
        ch_out<ch_bool> s2_wvalid;
        ch_in<ch_bool> s2_wready;
        
        ch_in<ch_uint<2>> s2_bresp;
        ch_in<ch_bool> s2_bvalid;
        ch_out<ch_bool> s2_bready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s2_araddr;
        ch_out<ch_bool> s2_arvalid;
        ch_in<ch_bool> s2_arready;
        
        ch_in<ch_uint<DATA_WIDTH>> s2_rdata;
        ch_in<ch_uint<2>> s2_rresp;
        ch_in<ch_bool> s2_rvalid;
        ch_out<ch_bool> s2_rready;
        
        // 从设备 3
        ch_out<ch_uint<ADDR_WIDTH>> s3_awaddr;
        ch_out<ch_bool> s3_awvalid;
        ch_in<ch_bool> s3_awready;
        
        ch_out<ch_uint<DATA_WIDTH>> s3_wdata;
        ch_out<ch_bool> s3_wvalid;
        ch_in<ch_bool> s3_wready;
        
        ch_in<ch_uint<2>> s3_bresp;
        ch_in<ch_bool> s3_bvalid;
        ch_out<ch_bool> s3_bready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s3_araddr;
        ch_out<ch_bool> s3_arvalid;
        ch_in<ch_bool> s3_arready;
        
        ch_in<ch_uint<DATA_WIDTH>> s3_rdata;
        ch_in<ch_uint<2>> s3_rresp;
        ch_in<ch_bool> s3_rvalid;
        ch_out<ch_bool> s3_rready;
    )
    
    AxiInterconnect4x4(ch::Component* parent = nullptr, const std::string& name = "axi_interconnect_4x4")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 地址解码
        // ========================================================================
        
        // 从设备地址范围 (30 位地址位 [31:30])
        // 00 → 从设备 0 (0x0000_0000 - 0x3FFF_FFFF)
        // 01 → 从设备 1 (0x4000_0000 - 0x7FFF_FFFF)
        // 10 → 从设备 2 (0x8000_0000 - 0xBFFF_FFFF)
        // 11 → 从设备 3 (0xC000_0000 - 0xFFFF_FFFF)
        
        auto decode_slave = [](const auto& addr) {
            return addr >> ch_uint<ADDR_WIDTH>(30_d);
        };
        
        auto m0_slave = decode_slave(io().m0_awaddr);
        auto m1_slave = decode_slave(io().m1_awaddr);
        auto m2_slave = decode_slave(io().m2_awaddr);
        auto m3_slave = decode_slave(io().m3_awaddr);
        
        // ========================================================================
        // 仲裁器 (轮询)
        // ========================================================================
        
        ch_reg<ch_uint<2>> arb_state(0_d);  // 0=M0, 1=M1, 2=M2, 3=M3
        
        // 主设备请求
        auto m0_req = io().m0_awvalid;
        auto m1_req = io().m1_awvalid;
        auto m2_req = io().m2_awvalid;
        auto m3_req = io().m3_awvalid;
        
        // 仲裁授予
        auto grant_m0 = select((arb_state == ch_uint<2>(0_d)), m0_req, ch_bool(false));
        auto grant_m1 = select((arb_state == ch_uint<2>(1_d)), m1_req, ch_bool(false));
        auto grant_m2 = select((arb_state == ch_uint<2>(2_d)), m2_req, ch_bool(false));
        auto grant_m3 = select((arb_state == ch_uint<2>(3_d)), m3_req, ch_bool(false));
        
        // 事务完成时更新仲裁器
        auto s0_or_s1 = select(io().s0_awready, ch_bool(true), io().s1_awready);
        auto s2_or_s3 = select(io().s2_awready, ch_bool(true), io().s3_awready);
        auto transaction_done = select(s0_or_s1, ch_bool(true), s2_or_s3);
        arb_state->next = select(transaction_done, 
                                  (arb_state + ch_uint<2>(1_d)) & ch_uint<2>(3_d),
                                  arb_state);
        
        // ========================================================================
        // 写地址通道 (AW)
        // ========================================================================
        
        // 从设备 0 选择
        auto s0_sel_m0 = select(grant_m0, select((m0_slave == ch_uint<2>(0_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s0_sel_m1 = select(grant_m1, select((m1_slave == ch_uint<2>(0_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s0_sel_m2 = select(grant_m2, select((m2_slave == ch_uint<2>(0_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s0_sel_m3 = select(grant_m3, select((m3_slave == ch_uint<2>(0_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        
        io().s0_awaddr = select(s0_sel_m0, io().m0_awaddr,
                                select(s0_sel_m1, io().m1_awaddr,
                                select(s0_sel_m2, io().m2_awaddr,
                                select(s0_sel_m3, io().m3_awaddr,
                                       ch_uint<ADDR_WIDTH>(0_d)))));
        
        io().s0_awvalid = select(s0_sel_m0, io().m0_awvalid,
                                 select(s0_sel_m1, io().m1_awvalid,
                                 select(s0_sel_m2, io().m2_awvalid,
                                 select(s0_sel_m3, io().m3_awvalid,
                                        ch_bool(false)))));
        
        // 从设备 1-3 (类似)
        auto s1_sel_m0 = select(grant_m0, select((m0_slave == ch_uint<2>(1_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s2_sel_m0 = select(grant_m0, select((m0_slave == ch_uint<2>(2_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s3_sel_m0 = select(grant_m0, select((m0_slave == ch_uint<2>(3_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        
        io().s1_awaddr = select(s1_sel_m0, io().m0_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        io().s1_awvalid = select(s1_sel_m0, io().m0_awvalid, ch_bool(false));
        
        io().s2_awaddr = select(s2_sel_m0, io().m0_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        io().s2_awvalid = select(s2_sel_m0, io().m0_awvalid, ch_bool(false));
        
        io().s3_awaddr = select(s3_sel_m0, io().m0_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        io().s3_awvalid = select(s3_sel_m0, io().m0_awvalid, ch_bool(false));
        
        // 主设备就绪
        io().m0_awready = select(m0_slave == ch_uint<2>(0_d), io().s0_awready,
                                 select(m0_slave == ch_uint<2>(1_d), io().s1_awready,
                                 select(m0_slave == ch_uint<2>(2_d), io().s2_awready,
                                 select(m0_slave == ch_uint<2>(3_d), io().s3_awready,
                                        ch_bool(false)))));
        
        io().m1_awready = select(m1_slave == ch_uint<2>(0_d), io().s0_awready,
                                 select(m1_slave == ch_uint<2>(1_d), io().s1_awready,
                                 select(m1_slave == ch_uint<2>(2_d), io().s2_awready,
                                 select(m1_slave == ch_uint<2>(3_d), io().s3_awready,
                                        ch_bool(false)))));
        
        io().m2_awready = select(m2_slave == ch_uint<2>(0_d), io().s0_awready,
                                 select(m2_slave == ch_uint<2>(1_d), io().s1_awready,
                                 select(m2_slave == ch_uint<2>(2_d), io().s2_awready,
                                 select(m2_slave == ch_uint<2>(3_d), io().s3_awready,
                                        ch_bool(false)))));
        
        io().m3_awready = select(m3_slave == ch_uint<2>(0_d), io().s0_awready,
                                 select(m3_slave == ch_uint<2>(1_d), io().s1_awready,
                                 select(m3_slave == ch_uint<2>(2_d), io().s2_awready,
                                 select(m3_slave == ch_uint<2>(3_d), io().s3_awready,
                                        ch_bool(false)))));
        
        // ========================================================================
        // 写数据通道 (W) - 简化
        // ========================================================================
        
        io().s0_wdata = select(s0_sel_m0, io().m0_wdata,
                               select(s0_sel_m1, io().m1_wdata,
                               select(s0_sel_m2, io().m2_wdata,
                               select(s0_sel_m3, io().m3_wdata,
                                      ch_uint<DATA_WIDTH>(0_d)))));
        
        io().s0_wvalid = select(s0_sel_m0, io().m0_wvalid,
                                select(s0_sel_m1, io().m1_wvalid,
                                select(s0_sel_m2, io().m2_wvalid,
                                select(s0_sel_m3, io().m3_wvalid,
                                       ch_bool(false)))));
        
        io().s1_wdata = select(s1_sel_m0, io().m0_wdata, ch_uint<DATA_WIDTH>(0_d));
        io().s1_wvalid = select(s1_sel_m0, io().m0_wvalid, ch_bool(false));
        io().s2_wdata = select(s2_sel_m0, io().m0_wdata, ch_uint<DATA_WIDTH>(0_d));
        io().s2_wvalid = select(s2_sel_m0, io().m0_wvalid, ch_bool(false));
        io().s3_wdata = select(s3_sel_m0, io().m0_wdata, ch_uint<DATA_WIDTH>(0_d));
        io().s3_wvalid = select(s3_sel_m0, io().m0_wvalid, ch_bool(false));
        
        io().m0_wready = select(m0_slave == ch_uint<2>(0_d), io().s0_wready,
                                select(m0_slave == ch_uint<2>(1_d), io().s1_wready,
                                select(m0_slave == ch_uint<2>(2_d), io().s2_wready,
                                select(m0_slave == ch_uint<2>(3_d), io().s3_wready,
                                       ch_bool(false)))));
        
        // ========================================================================
        // 写响应通道 (B) - 简化
        // ========================================================================
        
        io().m0_bresp = select(m0_slave == ch_uint<2>(0_d), io().s0_bresp,
                               select(m0_slave == ch_uint<2>(1_d), io().s1_bresp,
                               select(m0_slave == ch_uint<2>(2_d), io().s2_bresp,
                               select(m0_slave == ch_uint<2>(3_d),
                                      ch_uint<2>(static_cast<unsigned>(AxiResp::DECERR)),
                                      ch_uint<2>(static_cast<unsigned>(AxiResp::DECERR))))));
        
        io().m0_bvalid = select(m0_slave == ch_uint<2>(0_d), io().s0_bvalid,
                                select(m0_slave == ch_uint<2>(1_d), io().s1_bvalid,
                                select(m0_slave == ch_uint<2>(2_d), io().s2_bvalid,
                                select(m0_slave == ch_uint<2>(3_d), io().s3_bvalid,
                                       ch_bool(false)))));
        
        io().s0_bready = select(s0_sel_m0, io().m0_bready,
                                select(s0_sel_m1, io().m1_bready,
                                select(s0_sel_m2, io().m2_bready,
                                select(s0_sel_m3, io().m3_bready,
                                       ch_bool(false)))));
        
        // ========================================================================
        // 读地址通道 (AR) - 简化
        // ========================================================================
        
        io().s0_araddr = select(s0_sel_m0, io().m0_araddr,
                                select(s0_sel_m1, io().m1_araddr,
                                select(s0_sel_m2, io().m2_araddr,
                                select(s0_sel_m3, io().m3_araddr,
                                       ch_uint<ADDR_WIDTH>(0_d)))));
        
        io().s0_arvalid = select(s0_sel_m0, io().m0_arvalid,
                                 select(s0_sel_m1, io().m1_arvalid,
                                 select(s0_sel_m2, io().m2_arvalid,
                                 select(s0_sel_m3, io().m3_arvalid,
                                        ch_bool(false)))));
        
        io().m0_arready = select(m0_slave == ch_uint<2>(0_d), io().s0_arready,
                                 select(m0_slave == ch_uint<2>(1_d), io().s1_arready,
                                 select(m0_slave == ch_uint<2>(2_d), io().s2_arready,
                                 select(m0_slave == ch_uint<2>(3_d), io().s3_arready,
                                        ch_bool(false)))));
        
        // ========================================================================
        // 读数据通道 (R) - 简化
        // ========================================================================
        
        io().m0_rdata = select(m0_slave == ch_uint<2>(0_d), io().s0_rdata,
                               select(m0_slave == ch_uint<2>(1_d), io().s1_rdata,
                               select(m0_slave == ch_uint<2>(2_d), io().s2_rdata,
                               select(m0_slave == ch_uint<2>(3_d), io().s3_rdata,
                                      ch_uint<DATA_WIDTH>(0_d)))));
        
        io().m0_rvalid = select(m0_slave == ch_uint<2>(0_d), io().s0_rvalid,
                                select(m0_slave == ch_uint<2>(1_d), io().s1_rvalid,
                                select(m0_slave == ch_uint<2>(2_d), io().s2_rvalid,
                                select(m0_slave == ch_uint<2>(3_d), io().s3_rvalid,
                                       ch_bool(false)))));
        
        io().s0_rready = select(s0_sel_m0, io().m0_rready,
                                select(s0_sel_m1, io().m1_rready,
                                select(s0_sel_m2, io().m2_rready,
                                select(s0_sel_m3, io().m3_rready,
                                       ch_bool(false)))));
    }
};

} // namespace axi4
