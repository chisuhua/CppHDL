/**
 * @file axi_dma.h
 * @brief AXI4 DMA Controller - Memory to Memory Transfer
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "../axi4_full.h"

using namespace ch::core;

namespace axi4 {

template <unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32, unsigned ID_WIDTH = 4, unsigned MAX_BURST_LEN = 16>
class AxiDma : public ch::Component {
public:
    static constexpr unsigned NUM_BYTES = DATA_WIDTH / 8;
    static constexpr unsigned MAX_BURST_BYTES = NUM_BYTES * MAX_BURST_LEN;
    
    __io(
        ch_out<ch_uint<ADDR_WIDTH>> araddr; ch_out<ch_uint<ID_WIDTH>> arid;
        ch_out<ch_uint<4>> arlen; ch_out<ch_uint<3>> arsize; ch_out<ch_uint<2>> arburst;
        ch_out<ch_bool> arlock; ch_out<ch_uint<4>> arcache; ch_out<ch_uint<2>> arprot;
        ch_out<ch_bool> arvalid; ch_in<ch_bool> arready;
        ch_in<ch_uint<DATA_WIDTH>> rdata; ch_in<ch_uint<ID_WIDTH>> rid;
        ch_in<ch_uint<2>> rresp; ch_in<ch_bool> rlast; ch_in<ch_bool> rvalid; ch_out<ch_bool> rready;
        ch_out<ch_uint<ADDR_WIDTH>> awaddr; ch_out<ch_uint<ID_WIDTH>> awid;
        ch_out<ch_uint<4>> awlen; ch_out<ch_uint<3>> awsize; ch_out<ch_uint<2>> awburst;
        ch_out<ch_bool> awlock; ch_out<ch_uint<4>> awcache; ch_out<ch_uint<2>> awprot;
        ch_out<ch_bool> awvalid; ch_in<ch_bool> awready;
        ch_out<ch_uint<DATA_WIDTH>> wdata; ch_out<ch_uint<ID_WIDTH>> wid;
        ch_out<ch_uint<NUM_BYTES>> wstrb; ch_out<ch_bool> wlast; ch_out<ch_bool> wvalid; ch_in<ch_bool> wready;
        ch_in<ch_uint<ID_WIDTH>> bid; ch_in<ch_uint<2>> bresp; ch_in<ch_bool> bvalid; ch_out<ch_bool> bready;
        ch_in<ch_bool> reg_start; ch_in<ch_bool> reg_stop;
        ch_in<ch_uint<ADDR_WIDTH>> reg_src_addr; ch_in<ch_uint<ADDR_WIDTH>> reg_dst_addr;
        ch_in<ch_uint<32>> reg_length;
        ch_out<ch_bool> status_busy; ch_out<ch_bool> status_done;
        ch_out<ch_bool> status_error; ch_out<ch_bool> irq;
    )
    
    AxiDma(ch::Component* parent = nullptr, const std::string& name = "axi_dma") : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    
    void describe() override {
        ch_reg<ch_uint<3>> state(0_d);
        auto is_idle = (state == ch_uint<3>(0_d));
        auto is_rd_addr = (state == ch_uint<3>(1_d));
        auto is_rd_data = (state == ch_uint<3>(2_d));
        auto is_wr_addr = (state == ch_uint<3>(3_d));
        auto is_wr_data = (state == ch_uint<3>(4_d));
        auto is_wr_resp = (state == ch_uint<3>(5_d));
        auto is_done_st = (state == ch_uint<3>(6_d));
        
        ch_reg<ch_uint<ADDR_WIDTH>> src_addr(0_d), dst_addr(0_d), curr_raddr(0_d), curr_waddr(0_d);
        ch_reg<ch_uint<32>> length_reg(0_d), bytes_rem(0_d);
        ch_reg<ch_uint<4>> beat_cnt(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> rdata_buf(0_d);
        ch_reg<ch_bool> data_vld(ch_bool(false)), err_flag(ch_bool(false)), irq_en(ch_bool(false));
        ch_reg<ch_uint<ID_WIDTH>> id_reg(0_d);
        
        auto ar_rdy = io().arready, r_vld = io().rvalid, r_rdy_sig = io().rready;
        auto aw_rdy = io().awready, w_vld = io().wvalid, w_rdy = io().wready;
        auto b_vld = io().bvalid, b_rdy = io().bready;
        auto rlast = io().rlast, rresp = io().rresp, bresp = io().bresp;
        
        auto start = select(io().reg_start, select(is_idle, select(err_flag, ch_bool(false), ch_bool(true)), ch_bool(false)), ch_bool(false));
        auto stop = select(io().reg_stop, select(is_idle, ch_bool(false), select(is_done_st, ch_bool(false), ch_bool(true))), ch_bool(false));
        
        auto addr_lsb = ch_uint<32>(NUM_BYTES > 8 ? 4 : (NUM_BYTES > 4 ? 3 : (NUM_BYTES > 2 ? 2 : (NUM_BYTES > 1 ? 1 : 0))));
        auto beats = (bytes_rem + ch_uint<32>(NUM_BYTES - 1)) >> addr_lsb;
        auto burst_len = select(beats > ch_uint<32>(MAX_BURST_LEN), ch_uint<4>(MAX_BURST_LEN - 1_d),
                                ch_uint<4>((beats - ch_uint<32>(1_d)) & ch_uint<32>(15_d)));
        auto bytes_in_burst = (burst_len + ch_uint<4>(1_d)) * ch_uint<32>(NUM_BYTES);
        auto addr_inc = ch_uint<ADDR_WIDTH>(MAX_BURST_BYTES);
        
        // Handshake signals
        auto ar_hs = select(is_rd_addr, ar_rdy, ch_bool(false));
        auto r_hs = select(is_rd_data, select(r_vld, r_rdy_sig, ch_bool(false)), ch_bool(false));
        auto aw_hs = select(is_wr_addr, aw_rdy, ch_bool(false));
        auto w_hs = select(is_wr_data, select(w_vld, w_rdy, ch_bool(false)), ch_bool(false));
        auto b_hs = select(is_wr_resp, select(b_vld, b_rdy, ch_bool(false)), ch_bool(false));
        
        src_addr->next = select(start, io().reg_src_addr, select(ar_hs, curr_raddr + addr_inc, src_addr));
        dst_addr->next = select(start, io().reg_dst_addr, select(aw_hs, curr_waddr + addr_inc, dst_addr));
        length_reg->next = select(start, io().reg_length, select(b_hs, bytes_rem, length_reg));
        curr_raddr->next = select(start, src_addr, select(ar_hs, curr_raddr + addr_inc, curr_raddr));
        curr_waddr->next = select(start, dst_addr, select(aw_hs, curr_waddr + addr_inc, curr_waddr));
        bytes_rem->next = select(start, io().reg_length, select(b_hs, select(stop, ch_uint<32>(0_d), bytes_rem - bytes_in_burst), select(stop, ch_uint<32>(0_d), bytes_rem)));
        
        auto wlast = (beat_cnt == burst_len);
        beat_cnt->next = select(start || is_done_st, ch_uint<4>(0_d),
                               select(r_hs, select(rlast, ch_uint<4>(0_d), beat_cnt + ch_uint<4>(1_d)),
                               select(w_hs, select(wlast, ch_uint<4>(0_d), beat_cnt + ch_uint<4>(1_d)), beat_cnt)));
        
        rdata_buf->next = select(r_hs, io().rdata, rdata_buf);
        data_vld->next = select(r_hs, ch_bool(true), select(w_hs, ch_bool(false), data_vld));
        // ID increments only at transaction start (reg_start), stays same for all beats/bursts in same transaction
        id_reg->next = select(start, id_reg + ch_uint<ID_WIDTH>(1_d), id_reg);
        
        auto rd_err = select(is_rd_data, select(r_vld, rresp != ch_uint<2>(0_d), ch_bool(false)), ch_bool(false));
        auto wr_err = select(is_wr_resp, select(b_vld, bresp != ch_uint<2>(0_d), ch_bool(false)), ch_bool(false));
        err_flag->next = select(start, ch_bool(false), select(rd_err || wr_err || stop, ch_bool(true), err_flag));
        
        auto rd_complete = select(is_rd_data, select(r_vld, select(r_rdy_sig, rlast, ch_bool(false)), ch_bool(false)), ch_bool(false));
        auto wr_complete = select(is_wr_data, select(w_vld, select(w_rdy, wlast, ch_bool(false)), ch_bool(false)), ch_bool(false));
        auto more_data = bytes_rem > bytes_in_burst;
        
        auto next_state = select(is_idle, select(start, ch_uint<3>(1_d), ch_uint<3>(0_d)),
                                select(is_rd_addr, select(ar_rdy, ch_uint<3>(2_d), ch_uint<3>(1_d)),
                                select(is_rd_data, select(rd_complete, select(err_flag, ch_uint<3>(6_d), ch_uint<3>(3_d)), ch_uint<3>(2_d)),
                                select(is_wr_addr, select(aw_rdy, ch_uint<3>(4_d), ch_uint<3>(3_d)),
                                select(is_wr_data, select(wr_complete, ch_uint<3>(5_d), ch_uint<3>(4_d)),
                                select(is_wr_resp, select(b_hs, select(more_data, ch_uint<3>(1_d), ch_uint<3>(6_d)), ch_uint<3>(5_d)), ch_uint<3>(5_d)))))));
        state->next = select(stop && !is_done_st, ch_uint<3>(6_d), next_state);
        
        auto aligned_raddr = curr_raddr & ~(ch_uint<ADDR_WIDTH>(NUM_BYTES - 1));
        auto aligned_waddr = curr_waddr & ~(ch_uint<ADDR_WIDTH>(NUM_BYTES - 1));
        
        io().araddr = select(is_rd_addr, aligned_raddr, ch_uint<ADDR_WIDTH>(0_d));
        io().arid = id_reg; io().arlen = burst_len; io().arsize = addr_lsb;
        io().arburst = ch_uint<2>(1_d); io().arlock = ch_bool(false);
        io().arcache = ch_uint<4>(3_d); io().arprot = ch_uint<2>(0_d);
        io().arvalid = select(is_rd_addr, ch_bool(true), select(is_rd_data, select(rlast, ch_bool(false), ch_bool(true)), ch_bool(false)));
        io().rready = select(is_rd_data, select(err_flag, ch_bool(false), ch_bool(true)), ch_bool(false));
        
        io().awaddr = select(is_wr_addr, aligned_waddr, ch_uint<ADDR_WIDTH>(0_d));
        io().awid = id_reg; io().awlen = burst_len; io().awsize = addr_lsb;
        io().awburst = ch_uint<2>(1_d); io().awlock = ch_bool(false);
        io().awcache = ch_uint<4>(3_d); io().awprot = ch_uint<2>(0_d);
        io().awvalid = select(is_wr_addr, ch_bool(true), select(is_wr_data, select(wlast, ch_bool(false), ch_bool(true)), ch_bool(false)));
        
        io().wdata = rdata_buf; io().wid = id_reg;
        io().wstrb = (ch_uint<NUM_BYTES>(1_d) << NUM_BYTES) - ch_uint<NUM_BYTES>(1_d);
        io().wlast = wlast;
        io().wvalid = select(is_wr_data, data_vld, ch_bool(false));
        io().bready = is_wr_resp;
        
        auto busy = select(is_idle, ch_bool(false), select(is_done_st, ch_bool(false), ch_bool(true)));
        auto done = select(is_done_st, select(err_flag, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().status_busy = busy; io().status_done = done; io().status_error = err_flag;
        
        irq_en->next = select(start, ch_bool(true), select(done || err_flag, ch_bool(false), irq_en));
        io().irq = select(done || err_flag, irq_en, ch_bool(false));
    }
};

} // namespace axi4
