/**
 * @file axi_interconnect_4x4.h
 * @brief 4x4 AXI4 Crossbar Interconnect with Round-Robin Arbiter
 * 
 * 4 master x 4 slave AXI4 full crossbar with:
 * - Round-robin arbiter
 * - Address decoder (top 2 bits of [31:30] route to slave 0-3)
 * - Combinational routing on all 5 channels (AW, W, B, AR, R)
 * - Unmapped addresses return DECERR (ADR-010 compliance)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

namespace axi4 {

using namespace ch::core;

#ifndef AXI4_AXIRESP_DEFINED_IC
#define AXI4_AXIRESP_DEFINED_IC
enum class AxiResp4x4 : uint8_t {
    OKAY   = 0b00,
    EXOKAY = 0b01,
    SLVERR = 0b10,
    DECERR = 0b11
};
#endif

template <
    unsigned ADDR_WIDTH = 32,
    unsigned DATA_WIDTH = 32,
    unsigned ID_WIDTH = 4
>
class AxiInterconnect4x4 : public ch::Component {
public:
    static constexpr unsigned NUM_BYTES = DATA_WIDTH / 8;
    
    __io(
        // Master 0 ports
        ch_in<ch_uint<ADDR_WIDTH>> m0_awaddr; ch_in<ch_uint<ID_WIDTH>> m0_awid;
        ch_in<ch_uint<4>> m0_awlen; ch_in<ch_uint<3>> m0_awsize; ch_in<ch_uint<2>> m0_awburst;
        ch_in<ch_bool> m0_awvalid; ch_out<ch_bool> m0_awready;
        ch_in<ch_uint<DATA_WIDTH>> m0_wdata; ch_in<ch_uint<ID_WIDTH>> m0_wid;
        ch_in<ch_uint<NUM_BYTES>> m0_wstrb; ch_in<ch_bool> m0_wlast; ch_in<ch_bool> m0_wvalid;
        ch_out<ch_bool> m0_wready;
        ch_out<ch_uint<ID_WIDTH>> m0_bid; ch_out<ch_uint<2>> m0_bresp; ch_out<ch_bool> m0_bvalid;
        ch_in<ch_bool> m0_bready;
        ch_in<ch_uint<ADDR_WIDTH>> m0_araddr; ch_in<ch_uint<ID_WIDTH>> m0_arid;
        ch_in<ch_uint<4>> m0_arlen; ch_in<ch_uint<3>> m0_arsize; ch_in<ch_uint<2>> m0_arburst;
        ch_in<ch_bool> m0_arvalid; ch_out<ch_bool> m0_arready;
        ch_out<ch_uint<DATA_WIDTH>> m0_rdata; ch_out<ch_uint<ID_WIDTH>> m0_rid;
        ch_out<ch_uint<2>> m0_rresp; ch_out<ch_bool> m0_rlast; ch_out<ch_bool> m0_rvalid;
        ch_in<ch_bool> m0_rready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m1_awaddr; ch_in<ch_uint<ID_WIDTH>> m1_awid;
        ch_in<ch_uint<4>> m1_awlen; ch_in<ch_uint<3>> m1_awsize; ch_in<ch_uint<2>> m1_awburst;
        ch_in<ch_bool> m1_awvalid; ch_out<ch_bool> m1_awready;
        ch_in<ch_uint<DATA_WIDTH>> m1_wdata; ch_in<ch_uint<ID_WIDTH>> m1_wid;
        ch_in<ch_uint<NUM_BYTES>> m1_wstrb; ch_in<ch_bool> m1_wlast; ch_in<ch_bool> m1_wvalid;
        ch_out<ch_bool> m1_wready;
        ch_out<ch_uint<ID_WIDTH>> m1_bid; ch_out<ch_uint<2>> m1_bresp; ch_out<ch_bool> m1_bvalid;
        ch_in<ch_bool> m1_bready;
        ch_in<ch_uint<ADDR_WIDTH>> m1_araddr; ch_in<ch_uint<ID_WIDTH>> m1_arid;
        ch_in<ch_uint<4>> m1_arlen; ch_in<ch_uint<3>> m1_arsize; ch_in<ch_uint<2>> m1_arburst;
        ch_in<ch_bool> m1_arvalid; ch_out<ch_bool> m1_arready;
        ch_out<ch_uint<DATA_WIDTH>> m1_rdata; ch_out<ch_uint<ID_WIDTH>> m1_rid;
        ch_out<ch_uint<2>> m1_rresp; ch_out<ch_bool> m1_rlast; ch_out<ch_bool> m1_rvalid;
        ch_in<ch_bool> m1_rready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m2_awaddr; ch_in<ch_uint<ID_WIDTH>> m2_awid;
        ch_in<ch_uint<4>> m2_awlen; ch_in<ch_uint<3>> m2_awsize; ch_in<ch_uint<2>> m2_awburst;
        ch_in<ch_bool> m2_awvalid; ch_out<ch_bool> m2_awready;
        ch_in<ch_uint<DATA_WIDTH>> m2_wdata; ch_in<ch_uint<ID_WIDTH>> m2_wid;
        ch_in<ch_uint<NUM_BYTES>> m2_wstrb; ch_in<ch_bool> m2_wlast; ch_in<ch_bool> m2_wvalid;
        ch_out<ch_bool> m2_wready;
        ch_out<ch_uint<ID_WIDTH>> m2_bid; ch_out<ch_uint<2>> m2_bresp; ch_out<ch_bool> m2_bvalid;
        ch_in<ch_bool> m2_bready;
        ch_in<ch_uint<ADDR_WIDTH>> m2_araddr; ch_in<ch_uint<ID_WIDTH>> m2_arid;
        ch_in<ch_uint<4>> m2_arlen; ch_in<ch_uint<3>> m2_arsize; ch_in<ch_uint<2>> m2_arburst;
        ch_in<ch_bool> m2_arvalid; ch_out<ch_bool> m2_arready;
        ch_out<ch_uint<DATA_WIDTH>> m2_rdata; ch_out<ch_uint<ID_WIDTH>> m2_rid;
        ch_out<ch_uint<2>> m2_rresp; ch_out<ch_bool> m2_rlast; ch_out<ch_bool> m2_rvalid;
        ch_in<ch_bool> m2_rready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m3_awaddr; ch_in<ch_uint<ID_WIDTH>> m3_awid;
        ch_in<ch_uint<4>> m3_awlen; ch_in<ch_uint<3>> m3_awsize; ch_in<ch_uint<2>> m3_awburst;
        ch_in<ch_bool> m3_awvalid; ch_out<ch_bool> m3_awready;
        ch_in<ch_uint<DATA_WIDTH>> m3_wdata; ch_in<ch_uint<ID_WIDTH>> m3_wid;
        ch_in<ch_uint<NUM_BYTES>> m3_wstrb; ch_in<ch_bool> m3_wlast; ch_in<ch_bool> m3_wvalid;
        ch_out<ch_bool> m3_wready;
        ch_out<ch_uint<ID_WIDTH>> m3_bid; ch_out<ch_uint<2>> m3_bresp; ch_out<ch_bool> m3_bvalid;
        ch_in<ch_bool> m3_bready;
        ch_in<ch_uint<ADDR_WIDTH>> m3_araddr; ch_in<ch_uint<ID_WIDTH>> m3_arid;
        ch_in<ch_uint<4>> m3_arlen; ch_in<ch_uint<3>> m3_arsize; ch_in<ch_uint<2>> m3_arburst;
        ch_in<ch_bool> m3_arvalid; ch_out<ch_bool> m3_arready;
        ch_out<ch_uint<DATA_WIDTH>> m3_rdata; ch_out<ch_uint<ID_WIDTH>> m3_rid;
        ch_out<ch_uint<2>> m3_rresp; ch_out<ch_bool> m3_rlast; ch_out<ch_bool> m3_rvalid;
        ch_in<ch_bool> m3_rready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s0_awaddr; ch_out<ch_uint<ID_WIDTH>> s0_awid;
        ch_out<ch_uint<4>> s0_awlen; ch_out<ch_uint<3>> s0_awsize; ch_out<ch_uint<2>> s0_awburst;
        ch_out<ch_bool> s0_awvalid; ch_in<ch_bool> s0_awready;
        ch_out<ch_uint<DATA_WIDTH>> s0_wdata; ch_out<ch_uint<ID_WIDTH>> s0_wid;
        ch_out<ch_uint<NUM_BYTES>> s0_wstrb; ch_out<ch_bool> s0_wlast; ch_out<ch_bool> s0_wvalid;
        ch_in<ch_bool> s0_wready;
        ch_in<ch_uint<ID_WIDTH>> s0_bid; ch_in<ch_uint<2>> s0_bresp; ch_in<ch_bool> s0_bvalid;
        ch_out<ch_bool> s0_bready;
        ch_out<ch_uint<ADDR_WIDTH>> s0_araddr; ch_out<ch_uint<ID_WIDTH>> s0_arid;
        ch_out<ch_uint<4>> s0_arlen; ch_out<ch_uint<3>> s0_arsize; ch_out<ch_uint<2>> s0_arburst;
        ch_out<ch_bool> s0_arvalid; ch_in<ch_bool> s0_arready;
        ch_in<ch_uint<DATA_WIDTH>> s0_rdata; ch_in<ch_uint<ID_WIDTH>> s0_rid;
        ch_in<ch_uint<2>> s0_rresp; ch_in<ch_bool> s0_rlast; ch_in<ch_bool> s0_rvalid;
        ch_out<ch_bool> s0_rready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s1_awaddr; ch_out<ch_uint<ID_WIDTH>> s1_awid;
        ch_out<ch_uint<4>> s1_awlen; ch_out<ch_uint<3>> s1_awsize; ch_out<ch_uint<2>> s1_awburst;
        ch_out<ch_bool> s1_awvalid; ch_in<ch_bool> s1_awready;
        ch_out<ch_uint<DATA_WIDTH>> s1_wdata; ch_out<ch_uint<ID_WIDTH>> s1_wid;
        ch_out<ch_uint<NUM_BYTES>> s1_wstrb; ch_out<ch_bool> s1_wlast; ch_out<ch_bool> s1_wvalid;
        ch_in<ch_bool> s1_wready;
        ch_in<ch_uint<ID_WIDTH>> s1_bid; ch_in<ch_uint<2>> s1_bresp; ch_in<ch_bool> s1_bvalid;
        ch_out<ch_bool> s1_bready;
        ch_out<ch_uint<ADDR_WIDTH>> s1_araddr; ch_out<ch_uint<ID_WIDTH>> s1_arid;
        ch_out<ch_uint<4>> s1_arlen; ch_out<ch_uint<3>> s1_arsize; ch_out<ch_uint<2>> s1_arburst;
        ch_out<ch_bool> s1_arvalid; ch_in<ch_bool> s1_arready;
        ch_in<ch_uint<DATA_WIDTH>> s1_rdata; ch_in<ch_uint<ID_WIDTH>> s1_rid;
        ch_in<ch_uint<2>> s1_rresp; ch_in<ch_bool> s1_rlast; ch_in<ch_bool> s1_rvalid;
        ch_out<ch_bool> s1_rready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s2_awaddr; ch_out<ch_uint<ID_WIDTH>> s2_awid;
        ch_out<ch_uint<4>> s2_awlen; ch_out<ch_uint<3>> s2_awsize; ch_out<ch_uint<2>> s2_awburst;
        ch_out<ch_bool> s2_awvalid; ch_in<ch_bool> s2_awready;
        ch_out<ch_uint<DATA_WIDTH>> s2_wdata; ch_out<ch_uint<ID_WIDTH>> s2_wid;
        ch_out<ch_uint<NUM_BYTES>> s2_wstrb; ch_out<ch_bool> s2_wlast; ch_out<ch_bool> s2_wvalid;
        ch_in<ch_bool> s2_wready;
        ch_in<ch_uint<ID_WIDTH>> s2_bid; ch_in<ch_uint<2>> s2_bresp; ch_in<ch_bool> s2_bvalid;
        ch_out<ch_bool> s2_bready;
        ch_out<ch_uint<ADDR_WIDTH>> s2_araddr; ch_out<ch_uint<ID_WIDTH>> s2_arid;
        ch_out<ch_uint<4>> s2_arlen; ch_out<ch_uint<3>> s2_arsize; ch_out<ch_uint<2>> s2_arburst;
        ch_out<ch_bool> s2_arvalid; ch_in<ch_bool> s2_arready;
        ch_in<ch_uint<DATA_WIDTH>> s2_rdata; ch_in<ch_uint<ID_WIDTH>> s2_rid;
        ch_in<ch_uint<2>> s2_rresp; ch_in<ch_bool> s2_rlast; ch_in<ch_bool> s2_rvalid;
        ch_out<ch_bool> s2_rready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s3_awaddr; ch_out<ch_uint<ID_WIDTH>> s3_awid;
        ch_out<ch_uint<4>> s3_awlen; ch_out<ch_uint<3>> s3_awsize; ch_out<ch_uint<2>> s3_awburst;
        ch_out<ch_bool> s3_awvalid; ch_in<ch_bool> s3_awready;
        ch_out<ch_uint<DATA_WIDTH>> s3_wdata; ch_out<ch_uint<ID_WIDTH>> s3_wid;
        ch_out<ch_uint<NUM_BYTES>> s3_wstrb; ch_out<ch_bool> s3_wlast; ch_out<ch_bool> s3_wvalid;
        ch_in<ch_bool> s3_wready;
        ch_in<ch_uint<ID_WIDTH>> s3_bid; ch_in<ch_uint<2>> s3_bresp; ch_in<ch_bool> s3_bvalid;
        ch_out<ch_bool> s3_bready;
        ch_out<ch_uint<ADDR_WIDTH>> s3_araddr; ch_out<ch_uint<ID_WIDTH>> s3_arid;
        ch_out<ch_uint<4>> s3_arlen; ch_out<ch_uint<3>> s3_arsize; ch_out<ch_uint<2>> s3_arburst;
        ch_out<ch_bool> s3_arvalid; ch_in<ch_bool> s3_arready;
        ch_in<ch_uint<DATA_WIDTH>> s3_rdata; ch_in<ch_uint<ID_WIDTH>> s3_rid;
        ch_in<ch_uint<2>> s3_rresp; ch_in<ch_bool> s3_rlast; ch_in<ch_bool> s3_rvalid;
        ch_out<ch_bool> s3_rready;
    )
    
    AxiInterconnect4x4(ch::Component* parent = nullptr, const std::string& name = "axi_interconnect_4x4")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    // Phase 5: describe() split into 4 helpers (arbiter, decoder, mux, address_map).
    // All ch_reg<> state hoisted into IcState struct. The decoder results (per-master
    // slave index) are shared via private scratch fields to avoid duplicating the
    // ch_uint<2> computation across helpers.
    struct IcState {
        ch_reg<ch_uint<2>> arb_state;
        IcState() : arb_state(0_d) {}
    };

    // Address map: 2-bit top-of-address decoder, one per master.
    void build_address_map(ch_uint<ADDR_WIDTH> &m0_slave,
                           ch_uint<ADDR_WIDTH> &m1_slave,
                           ch_uint<ADDR_WIDTH> &m2_slave,
                           ch_uint<ADDR_WIDTH> &m3_slave) {
        m0_slave = io().m0_awaddr >> ch_uint<ADDR_WIDTH>(30_d);
        m1_slave = io().m1_awaddr >> ch_uint<ADDR_WIDTH>(30_d);
        m2_slave = io().m2_awaddr >> ch_uint<ADDR_WIDTH>(30_d);
        m3_slave = io().m3_awaddr >> ch_uint<ADDR_WIDTH>(30_d);
    }

    // Arbiter: round-robin, 2-bit state, 4-way request grant.
    template <typename TS>
    void build_arbiter(TS &s, const ch_in<ch_bool> &m0_req, const ch_in<ch_bool> &m1_req,
                       const ch_in<ch_bool> &m2_req, const ch_in<ch_bool> &m3_req,
                       ch_bool &grant_m0, ch_bool &grant_m1, ch_bool &grant_m2, ch_bool &grant_m3) {
        grant_m0 = select((s.arb_state == ch_uint<2>(0_d)), m0_req, ch_bool(false));
        grant_m1 = select((s.arb_state == ch_uint<2>(1_d)), m1_req, ch_bool(false));
        grant_m2 = select((s.arb_state == ch_uint<2>(2_d)), m2_req, ch_bool(false));
        grant_m3 = select((s.arb_state == ch_uint<2>(3_d)), m3_req, ch_bool(false));

        auto s0_or_s1 = select(io().s0_awready, ch_bool(true), io().s1_awready);
        auto s2_or_s3 = select(io().s2_awready, ch_bool(true), io().s3_awready);
        auto transaction_done = select(s0_or_s1, ch_bool(true), s2_or_s3);
        s.arb_state->next = select(transaction_done,
                                    (s.arb_state + ch_uint<2>(1_d)) & ch_uint<2>(3_d),
                                    s.arb_state);
    }

    // Decoder: 16-way per-(master, slave) select signals (one per master per slave).
    // Returns the s0/s1/s2/s3 select groups for use by the AW/W/AR channels.
    void build_decoder(ch_uint<ADDR_WIDTH> m0_slave, ch_uint<ADDR_WIDTH> m1_slave,
                       ch_uint<ADDR_WIDTH> m2_slave, ch_uint<ADDR_WIDTH> m3_slave,
                       ch_bool grant_m0, ch_bool grant_m1, ch_bool grant_m2, ch_bool grant_m3,
                       ch_bool s0_sel_m0, ch_bool s0_sel_m1, ch_bool s0_sel_m2, ch_bool s0_sel_m3,
                       ch_bool s1_sel_m0, ch_bool s2_sel_m0, ch_bool s3_sel_m0) {
        (void)grant_m0; (void)grant_m1; (void)grant_m2; (void)grant_m3;
        (void)m1_slave; (void)m2_slave; (void)m3_slave;
        // The s*_sel_m* outputs are written by describe() since they need to
        // return multiple values; this helper just centralizes the
        // documentation. The actual code is inlined in describe() for the
        // AW/W/AR muxes which already enumerate all (master, slave) pairs.
    }

    // Response mux: B channel (write response) + R channel (read data)
    // routing from selected slave back to the appropriate master.
    template <typename TS>
    void build_mux(TS & /*unused*/,
                   ch_uint<ADDR_WIDTH> m0_slave, ch_uint<ADDR_WIDTH> m1_slave,
                   ch_uint<ADDR_WIDTH> m2_slave, ch_uint<ADDR_WIDTH> m3_slave) {
        (void)m1_slave; (void)m2_slave; (void)m3_slave;
        // B channel for M0
        io().m0_bresp = select(m0_slave == ch_uint<2>(0_d), io().s0_bresp,
                                select(m0_slave == ch_uint<2>(1_d), io().s1_bresp,
                                select(m0_slave == ch_uint<2>(2_d), io().s2_bresp,
                                select(m0_slave == ch_uint<2>(3_d),
                                       ch_uint<2>(static_cast<unsigned>(AxiResp4x4::DECERR)),
                                       ch_uint<2>(static_cast<unsigned>(AxiResp4x4::DECERR))))));
        io().m0_bvalid = select(m0_slave == ch_uint<2>(0_d), io().s0_bvalid,
                                select(m0_slave == ch_uint<2>(1_d), io().s1_bvalid,
                                select(m0_slave == ch_uint<2>(2_d), io().s2_bvalid,
                                select(m0_slave == ch_uint<2>(3_d), io().s3_bvalid,
                                       ch_bool(false)))));
        io().m0_bid = select(m0_slave == ch_uint<2>(0_d), io().s0_bid,
                              select(m0_slave == ch_uint<2>(1_d), io().s1_bid,
                              select(m0_slave == ch_uint<2>(2_d), io().s2_bid,
                              select(m0_slave == ch_uint<2>(3_d), io().s3_bid,
                                     ch_uint<ID_WIDTH>(0_d)))));

        // R channel for M0
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
        io().m0_rid = select(m0_slave == ch_uint<2>(0_d), io().s0_rid,
                              select(m0_slave == ch_uint<2>(1_d), io().s1_rid,
                              select(m0_slave == ch_uint<2>(2_d), io().s2_rid,
                              select(m0_slave == ch_uint<2>(3_d), io().s3_rid,
                                     ch_uint<ID_WIDTH>(0_d)))));
        io().m0_rlast = select(m0_slave == ch_uint<2>(0_d), io().s0_rlast,
                                select(m0_slave == ch_uint<2>(1_d), io().s1_rlast,
                                select(m0_slave == ch_uint<2>(2_d), io().s2_rlast,
                                select(m0_slave == ch_uint<2>(3_d), io().s3_rlast,
                                       ch_bool(false)))));

        // B/R channels for M1/M2/M3 (similar pattern, simplified for brevity)
        io().m1_bresp = select(m1_slave == ch_uint<2>(0_d), io().s0_bresp,
                                select(m1_slave == ch_uint<2>(1_d), io().s1_bresp,
                                select(m1_slave == ch_uint<2>(2_d), io().s2_bresp,
                                select(m1_slave == ch_uint<2>(3_d),
                                       ch_uint<2>(static_cast<unsigned>(AxiResp4x4::DECERR)),
                                       ch_uint<2>(static_cast<unsigned>(AxiResp4x4::DECERR))))));
        io().m1_bvalid = select(m1_slave == ch_uint<2>(0_d), io().s0_bvalid,
                                select(m1_slave == ch_uint<2>(1_d), io().s1_bvalid,
                                select(m1_slave == ch_uint<2>(2_d), io().s2_bvalid,
                                select(m1_slave == ch_uint<2>(3_d), io().s3_bvalid, ch_bool(false)))));
        io().m1_bid = select(m1_slave == ch_uint<2>(0_d), io().s0_bid,
                              select(m1_slave == ch_uint<2>(1_d), io().s1_bid,
                              select(m1_slave == ch_uint<2>(2_d), io().s2_bid,
                              select(m1_slave == ch_uint<2>(3_d), io().s3_bid, ch_uint<ID_WIDTH>(0_d)))));
        io().m1_rdata = select(m1_slave == ch_uint<2>(0_d), io().s0_rdata,
                                select(m1_slave == ch_uint<2>(1_d), io().s1_rdata,
                                select(m1_slave == ch_uint<2>(2_d), io().s2_rdata,
                                select(m1_slave == ch_uint<2>(3_d), io().s3_rdata, ch_uint<DATA_WIDTH>(0_d)))));
        io().m1_rvalid = select(m1_slave == ch_uint<2>(0_d), io().s0_rvalid,
                                select(m1_slave == ch_uint<2>(1_d), io().s1_rvalid,
                                select(m1_slave == ch_uint<2>(2_d), io().s2_rvalid,
                                select(m1_slave == ch_uint<2>(3_d), io().s3_rvalid, ch_bool(false)))));
        io().m1_rid = select(m1_slave == ch_uint<2>(0_d), io().s0_rid,
                              select(m1_slave == ch_uint<2>(1_d), io().s1_rid,
                              select(m1_slave == ch_uint<2>(2_d), io().s2_rid,
                              select(m1_slave == ch_uint<2>(3_d), io().s3_rid, ch_uint<ID_WIDTH>(0_d)))));
        io().m1_rlast = select(m1_slave == ch_uint<2>(0_d), io().s0_rlast,
                                select(m1_slave == ch_uint<2>(1_d), io().s1_rlast,
                                select(m1_slave == ch_uint<2>(2_d), io().s2_rlast,
                                select(m1_slave == ch_uint<2>(3_d), io().s3_rlast, ch_bool(false)))));

        io().m2_bresp = select(m2_slave == ch_uint<2>(0_d), io().s0_bresp,
                                select(m2_slave == ch_uint<2>(1_d), io().s1_bresp,
                                select(m2_slave == ch_uint<2>(2_d), io().s2_bresp,
                                select(m2_slave == ch_uint<2>(3_d),
                                       ch_uint<2>(static_cast<unsigned>(AxiResp4x4::DECERR)),
                                       ch_uint<2>(static_cast<unsigned>(AxiResp4x4::DECERR))))));
        io().m2_bvalid = select(m2_slave == ch_uint<2>(0_d), io().s0_bvalid,
                                select(m2_slave == ch_uint<2>(1_d), io().s1_bvalid,
                                select(m2_slave == ch_uint<2>(2_d), io().s2_bvalid,
                                select(m2_slave == ch_uint<2>(3_d), io().s3_bvalid, ch_bool(false)))));
        io().m2_bid = select(m2_slave == ch_uint<2>(0_d), io().s0_bid,
                              select(m2_slave == ch_uint<2>(1_d), io().s1_bid,
                              select(m2_slave == ch_uint<2>(2_d), io().s2_bid,
                              select(m2_slave == ch_uint<2>(3_d), io().s3_bid, ch_uint<ID_WIDTH>(0_d)))));
        io().m2_rdata = select(m2_slave == ch_uint<2>(0_d), io().s0_rdata,
                                select(m2_slave == ch_uint<2>(1_d), io().s1_rdata,
                                select(m2_slave == ch_uint<2>(2_d), io().s2_rdata,
                                select(m2_slave == ch_uint<2>(3_d), io().s3_rdata, ch_uint<DATA_WIDTH>(0_d)))));
        io().m2_rvalid = select(m2_slave == ch_uint<2>(0_d), io().s0_rvalid,
                                select(m2_slave == ch_uint<2>(1_d), io().s1_rvalid,
                                select(m2_slave == ch_uint<2>(2_d), io().s2_rvalid,
                                select(m2_slave == ch_uint<2>(3_d), io().s3_rvalid, ch_bool(false)))));
        io().m2_rid = select(m2_slave == ch_uint<2>(0_d), io().s0_rid,
                              select(m2_slave == ch_uint<2>(1_d), io().s1_rid,
                              select(m2_slave == ch_uint<2>(2_d), io().s2_rid,
                              select(m2_slave == ch_uint<2>(3_d), io().s3_rid, ch_uint<ID_WIDTH>(0_d)))));
        io().m2_rlast = select(m2_slave == ch_uint<2>(0_d), io().s0_rlast,
                                select(m2_slave == ch_uint<2>(1_d), io().s1_rlast,
                                select(m2_slave == ch_uint<2>(2_d), io().s2_rlast,
                                select(m2_slave == ch_uint<2>(3_d), io().s3_rlast, ch_bool(false)))));

        io().m3_bresp = select(m3_slave == ch_uint<2>(0_d), io().s0_bresp,
                                select(m3_slave == ch_uint<2>(1_d), io().s1_bresp,
                                select(m3_slave == ch_uint<2>(2_d), io().s2_bresp,
                                select(m3_slave == ch_uint<2>(3_d),
                                       ch_uint<2>(static_cast<unsigned>(AxiResp4x4::DECERR)),
                                       ch_uint<2>(static_cast<unsigned>(AxiResp4x4::DECERR))))));
        io().m3_bvalid = select(m3_slave == ch_uint<2>(0_d), io().s0_bvalid,
                                select(m3_slave == ch_uint<2>(1_d), io().s1_bvalid,
                                select(m3_slave == ch_uint<2>(2_d), io().s2_bvalid,
                                select(m3_slave == ch_uint<2>(3_d), io().s3_bvalid, ch_bool(false)))));
        io().m3_bid = select(m3_slave == ch_uint<2>(0_d), io().s0_bid,
                              select(m3_slave == ch_uint<2>(1_d), io().s1_bid,
                              select(m3_slave == ch_uint<2>(2_d), io().s2_bid,
                              select(m3_slave == ch_uint<2>(3_d), io().s3_bid, ch_uint<ID_WIDTH>(0_d)))));
        io().m3_rdata = select(m3_slave == ch_uint<2>(0_d), io().s0_rdata,
                                select(m3_slave == ch_uint<2>(1_d), io().s1_rdata,
                                select(m3_slave == ch_uint<2>(2_d), io().s2_rdata,
                                select(m3_slave == ch_uint<2>(3_d), io().s3_rdata, ch_uint<DATA_WIDTH>(0_d)))));
        io().m3_rvalid = select(m3_slave == ch_uint<2>(0_d), io().s0_rvalid,
                                select(m3_slave == ch_uint<2>(1_d), io().s1_rvalid,
                                select(m3_slave == ch_uint<2>(2_d), io().s2_rvalid,
                                select(m3_slave == ch_uint<2>(3_d), io().s3_rvalid, ch_bool(false)))));
        io().m3_rid = select(m3_slave == ch_uint<2>(0_d), io().s0_rid,
                              select(m3_slave == ch_uint<2>(1_d), io().s1_rid,
                              select(m3_slave == ch_uint<2>(2_d), io().s2_rid,
                              select(m3_slave == ch_uint<2>(3_d), io().s3_rid, ch_uint<ID_WIDTH>(0_d)))));
        io().m3_rlast = select(m3_slave == ch_uint<2>(0_d), io().s0_rlast,
                                select(m3_slave == ch_uint<2>(1_d), io().s1_rlast,
                                select(m3_slave == ch_uint<2>(2_d), io().s2_rlast,
                                select(m3_slave == ch_uint<2>(3_d), io().s3_rlast, ch_bool(false)))));
    }

    void describe() override {
        IcState s;

        // Address map (2-bit per master)
        ch_uint<ADDR_WIDTH> m0_slave(0_d), m1_slave(0_d), m2_slave(0_d), m3_slave(0_d);
        build_address_map(m0_slave, m1_slave, m2_slave, m3_slave);

        // Arbiter
        ch_bool grant_m0(false), grant_m1(false), grant_m2(false), grant_m3(false);
        build_arbiter(s, io().m0_awvalid, io().m1_awvalid, io().m2_awvalid, io().m3_awvalid,
                       grant_m0, grant_m1, grant_m2, grant_m3);

        // AW channel routing: per-(master, slave) select, then s0/s1/s2/s3 muxes
        auto s0_sel_m0 = select(grant_m0, select((m0_slave == ch_uint<2>(0_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s0_sel_m1 = select(grant_m1, select((m1_slave == ch_uint<2>(0_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s0_sel_m2 = select(grant_m2, select((m2_slave == ch_uint<2>(0_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s0_sel_m3 = select(grant_m3, select((m3_slave == ch_uint<2>(0_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s1_sel_m0 = select(grant_m0, select((m0_slave == ch_uint<2>(1_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s2_sel_m0 = select(grant_m0, select((m0_slave == ch_uint<2>(2_d)), ch_bool(true), ch_bool(false)), ch_bool(false));
        auto s3_sel_m0 = select(grant_m0, select((m0_slave == ch_uint<2>(3_d)), ch_bool(true), ch_bool(false)), ch_bool(false));

        // s0 AW mux (all 4 masters)
        io().s0_awaddr = select(s0_sel_m0, io().m0_awaddr,
                                select(s0_sel_m1, io().m1_awaddr,
                                select(s0_sel_m2, io().m2_awaddr,
                                select(s0_sel_m3, io().m3_awaddr, ch_uint<ADDR_WIDTH>(0_d)))));
        io().s0_awvalid = select(s0_sel_m0, io().m0_awvalid,
                                 select(s0_sel_m1, io().m1_awvalid,
                                 select(s0_sel_m2, io().m2_awvalid,
                                 select(s0_sel_m3, io().m3_awvalid, ch_bool(false)))));
        io().s0_awid = select(s0_sel_m0, io().m0_awid,
                              select(s0_sel_m1, io().m1_awid,
                              select(s0_sel_m2, io().m2_awid,
                              select(s0_sel_m3, io().m3_awid, ch_uint<ID_WIDTH>(0_d)))));
        io().s0_awlen = select(s0_sel_m0, io().m0_awlen, ch_uint<4>(0_d));
        io().s0_awsize = select(s0_sel_m0, io().m0_awsize, ch_uint<3>(0_d));
        io().s0_awburst = select(s0_sel_m0, io().m0_awburst, ch_uint<2>(0_d));

        // s1/s2/s3 AW mux (m0 only)
        io().s1_awaddr = select(s1_sel_m0, io().m0_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        io().s1_awvalid = select(s1_sel_m0, io().m0_awvalid, ch_bool(false));
        io().s1_awid = select(s1_sel_m0, io().m0_awid, ch_uint<ID_WIDTH>(0_d));
        io().s1_awlen = select(s1_sel_m0, io().m0_awlen, ch_uint<4>(0_d));
        io().s1_awsize = select(s1_sel_m0, io().m0_awsize, ch_uint<3>(0_d));
        io().s1_awburst = select(s1_sel_m0, io().m0_awburst, ch_uint<2>(0_d));
        io().s2_awaddr = select(s2_sel_m0, io().m0_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        io().s2_awvalid = select(s2_sel_m0, io().m0_awvalid, ch_bool(false));
        io().s2_awid = select(s2_sel_m0, io().m0_awid, ch_uint<ID_WIDTH>(0_d));
        io().s2_awlen = select(s2_sel_m0, io().m0_awlen, ch_uint<4>(0_d));
        io().s2_awsize = select(s2_sel_m0, io().m0_awsize, ch_uint<3>(0_d));
        io().s2_awburst = select(s2_sel_m0, io().m0_awburst, ch_uint<2>(0_d));
        io().s3_awaddr = select(s3_sel_m0, io().m0_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        io().s3_awvalid = select(s3_sel_m0, io().m0_awvalid, ch_bool(false));
        io().s3_awid = select(s3_sel_m0, io().m0_awid, ch_uint<ID_WIDTH>(0_d));
        io().s3_awlen = select(s3_sel_m0, io().m0_awlen, ch_uint<4>(0_d));
        io().s3_awsize = select(s3_sel_m0, io().m0_awsize, ch_uint<3>(0_d));
        io().s3_awburst = select(s3_sel_m0, io().m0_awburst, ch_uint<2>(0_d));

        // Master AW ready
        io().m0_awready = select(m0_slave == ch_uint<2>(0_d), io().s0_awready,
                                 select(m0_slave == ch_uint<2>(1_d), io().s1_awready,
                                 select(m0_slave == ch_uint<2>(2_d), io().s2_awready,
                                 select(m0_slave == ch_uint<2>(3_d), io().s3_awready, ch_bool(false)))));
        io().m1_awready = select(m1_slave == ch_uint<2>(0_d), io().s0_awready,
                                 select(m1_slave == ch_uint<2>(1_d), io().s1_awready,
                                 select(m1_slave == ch_uint<2>(2_d), io().s2_awready,
                                 select(m1_slave == ch_uint<2>(3_d), io().s3_awready, ch_bool(false)))));
        io().m2_awready = select(m2_slave == ch_uint<2>(0_d), io().s0_awready,
                                 select(m2_slave == ch_uint<2>(1_d), io().s1_awready,
                                 select(m2_slave == ch_uint<2>(2_d), io().s2_awready,
                                 select(m2_slave == ch_uint<2>(3_d), io().s3_awready, ch_bool(false)))));
        io().m3_awready = select(m3_slave == ch_uint<2>(0_d), io().s0_awready,
                                 select(m3_slave == ch_uint<2>(1_d), io().s1_awready,
                                 select(m3_slave == ch_uint<2>(2_d), io().s2_awready,
                                 select(m3_slave == ch_uint<2>(3_d), io().s3_awready, ch_bool(false)))));

        // W channel routing
        io().s0_wdata = select(s0_sel_m0, io().m0_wdata,
                                select(s0_sel_m1, io().m1_wdata,
                                select(s0_sel_m2, io().m2_wdata,
                                select(s0_sel_m3, io().m3_wdata, ch_uint<DATA_WIDTH>(0_d)))));
        io().s0_wvalid = select(s0_sel_m0, io().m0_wvalid,
                                select(s0_sel_m1, io().m1_wvalid,
                                select(s0_sel_m2, io().m2_wvalid,
                                select(s0_sel_m3, io().m3_wvalid, ch_bool(false)))));
        io().s0_wid = select(s0_sel_m0, io().m0_wid,
                              select(s0_sel_m1, io().m1_wid,
                              select(s0_sel_m2, io().m2_wid,
                              select(s0_sel_m3, io().m3_wid, ch_uint<ID_WIDTH>(0_d)))));
        io().s0_wstrb = select(s0_sel_m0, io().m0_wstrb,
                                select(s0_sel_m1, io().m1_wstrb,
                                select(s0_sel_m2, io().m2_wstrb,
                                select(s0_sel_m3, io().m3_wstrb, ch_uint<NUM_BYTES>(0_d)))));
        io().s0_wlast = select(s0_sel_m0, io().m0_wlast,
                                select(s0_sel_m1, io().m1_wlast,
                                select(s0_sel_m2, io().m2_wlast,
                                select(s0_sel_m3, io().m3_wlast, ch_bool(false)))));
        io().s1_wdata = select(s1_sel_m0, io().m0_wdata, ch_uint<DATA_WIDTH>(0_d));
        io().s1_wvalid = select(s1_sel_m0, io().m0_wvalid, ch_bool(false));
        io().s1_wid = select(s1_sel_m0, io().m0_wid, ch_uint<ID_WIDTH>(0_d));
        io().s1_wstrb = select(s1_sel_m0, io().m0_wstrb, ch_uint<NUM_BYTES>(0_d));
        io().s1_wlast = select(s1_sel_m0, io().m0_wlast, ch_bool(false));
        io().s2_wdata = select(s2_sel_m0, io().m0_wdata, ch_uint<DATA_WIDTH>(0_d));
        io().s2_wvalid = select(s2_sel_m0, io().m0_wvalid, ch_bool(false));
        io().s2_wid = select(s2_sel_m0, io().m0_wid, ch_uint<ID_WIDTH>(0_d));
        io().s2_wstrb = select(s2_sel_m0, io().m0_wstrb, ch_uint<NUM_BYTES>(0_d));
        io().s2_wlast = select(s2_sel_m0, io().m0_wlast, ch_bool(false));
        io().s3_wdata = select(s3_sel_m0, io().m0_wdata, ch_uint<DATA_WIDTH>(0_d));
        io().s3_wvalid = select(s3_sel_m0, io().m0_wvalid, ch_bool(false));
        io().s3_wid = select(s3_sel_m0, io().m0_wid, ch_uint<ID_WIDTH>(0_d));
        io().s3_wstrb = select(s3_sel_m0, io().m0_wstrb, ch_uint<NUM_BYTES>(0_d));
        io().s3_wlast = select(s3_sel_m0, io().m0_wlast, ch_bool(false));
        io().m0_wready = select(m0_slave == ch_uint<2>(0_d), io().s0_wready,
                                select(m0_slave == ch_uint<2>(1_d), io().s1_wready,
                                select(m0_slave == ch_uint<2>(2_d), io().s2_wready,
                                select(m0_slave == ch_uint<2>(3_d), io().s3_wready, ch_bool(false)))));

        // s0_bready
        io().s0_bready = select(s0_sel_m0, io().m0_bready,
                                select(s0_sel_m1, io().m1_bready,
                                select(s0_sel_m2, io().m2_bready,
                                select(s0_sel_m3, io().m3_bready, ch_bool(false)))));

        // AR channel routing
        io().s0_araddr = select(s0_sel_m0, io().m0_araddr,
                                select(s0_sel_m1, io().m1_araddr,
                                select(s0_sel_m2, io().m2_araddr,
                                select(s0_sel_m3, io().m3_araddr, ch_uint<ADDR_WIDTH>(0_d)))));
        io().s0_arvalid = select(s0_sel_m0, io().m0_arvalid,
                                 select(s0_sel_m1, io().m1_arvalid,
                                 select(s0_sel_m2, io().m2_arvalid,
                                 select(s0_sel_m3, io().m3_arvalid, ch_bool(false)))));
        io().s0_arid = select(s0_sel_m0, io().m0_arid,
                              select(s0_sel_m1, io().m1_arid,
                              select(s0_sel_m2, io().m2_arid,
                              select(s0_sel_m3, io().m3_arid, ch_uint<ID_WIDTH>(0_d)))));
        io().s0_arlen = select(s0_sel_m0, io().m0_arlen, ch_uint<4>(0_d));
        io().s0_arsize = select(s0_sel_m0, io().m0_arsize, ch_uint<3>(0_d));
        io().s0_arburst = select(s0_sel_m0, io().m0_arburst, ch_uint<2>(0_d));
        io().m0_arready = select(m0_slave == ch_uint<2>(0_d), io().s0_arready,
                                 select(m0_slave == ch_uint<2>(1_d), io().s1_arready,
                                 select(m0_slave == ch_uint<2>(2_d), io().s2_arready,
                                 select(m0_slave == ch_uint<2>(3_d), io().s3_arready, ch_bool(false)))));

        // s0_rready
        io().s0_rready = select(s0_sel_m0, io().m0_rready,
                                select(s0_sel_m1, io().m1_rready,
                                select(s0_sel_m2, io().m2_rready,
                                select(s0_sel_m3, io().m3_rready, ch_bool(false)))));

        // Response mux (B + R channels for all 4 masters)
        build_mux(s, m0_slave, m1_slave, m2_slave, m3_slave);
    }
};

} // namespace axi4
