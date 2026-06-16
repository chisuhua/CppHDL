/**
 * @file axi4_full.h
 * @brief AXI4 全功能从设备控制器
 * 
 * 支持 AXI4 协议完整功能：
 * - 5 个独立通道 (AW/W/B/AR/R)
 * - 突发传输 (INCR/FIXED)
 * - 最大突发长度：16
 * - 可配置数据位宽 (32/64/128/256/512/1024)
 * 
 * AXI4 vs AXI4-Lite:
 * | 特性 | AXI4-Lite | AXI4 |
 * |------|-----------|------|
 * | 突发传输 | ❌ | ✅ |
 * | 突发长度 | 1 | 1-16 |
 * | 位宽 | 32/64 | 32-1024 |
 * | 地址宽度 | 32 | 32-64 |
 * | 响应类型 | OKAY/SLVERR | OKAY/EXOKAY/SLVERR/DECERR |
 */

#pragma once

#include "ch.hpp"
#include "component.h"

namespace axi4 {

using namespace ch::core;

#ifndef AXI4_AXIRESP_DEFINED
#define AXI4_AXIRESP_DEFINED
enum class AxiResp : uint8_t {
    OKAY   = 0b00,
    EXOKAY = 0b01,
    SLVERR = 0b10,
    DECERR = 0b11
};
#endif

enum class AxiBurst : uint8_t {
    FIXED = 0b00,
    INCR  = 0b01,
    WRAP  = 0b10
};

template <
    unsigned ADDR_WIDTH = 32,
    unsigned DATA_WIDTH = 32,
    unsigned ID_WIDTH = 4,
    unsigned NUM_REGS = 16,
    unsigned MAX_BURST_LEN = 16
>
class Axi4Slave : public ch::Component {
public:
    static constexpr unsigned NUM_BYTES = DATA_WIDTH / 8;
    
    __io(
        ch_in<ch_uint<ADDR_WIDTH>> awaddr;
        ch_in<ch_uint<ID_WIDTH>> awid;
        ch_in<ch_uint<4>> awlen;
        ch_in<ch_uint<3>> awsize;
        ch_in<ch_uint<2>> awburst;
        ch_in<ch_bool> awlock;
        ch_in<ch_uint<4>> awcache;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;
        
        ch_in<ch_uint<DATA_WIDTH>> wdata;
        ch_in<ch_uint<ID_WIDTH>> wid;
        ch_in<ch_uint<NUM_BYTES>> wstrb;
        ch_in<ch_bool> wlast;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;
        
        ch_out<ch_uint<ID_WIDTH>> bid;
        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> araddr;
        ch_in<ch_uint<ID_WIDTH>> arid;
        ch_in<ch_uint<4>> arlen;
        ch_in<ch_uint<3>> arsize;
        ch_in<ch_uint<2>> arburst;
        ch_in<ch_bool> arlock;
        ch_in<ch_uint<4>> arcache;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;
        
        ch_out<ch_uint<DATA_WIDTH>> rdata;
        ch_out<ch_uint<ID_WIDTH>> rid;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rlast;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;
    )
    
    Axi4Slave(ch::Component* parent = nullptr, const std::string& name = "axi4_slave")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    // Phase 5: describe() split into 3 helpers (id_handshake, data_channels,
    // response_logic). All ch_reg<> state hoisted into SlaveState struct local
    // to describe() and passed by reference. Helper return values flow through
    // member scratch fields to avoid huge parameter lists.
    struct SlaveState {
        ch_reg<ch_uint<2>> write_state;
        ch_reg<ch_uint<2>> read_state;
        ch_reg<ch_uint<DATA_WIDTH>> reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7;
        ch_reg<ch_uint<DATA_WIDTH>> reg8, reg9, reg10, reg11, reg12, reg13, reg14, reg15;
        ch_reg<ch_uint<4>> write_burst_cnt;
        ch_reg<ch_uint<ID_WIDTH>> write_id_reg;
        ch_reg<ch_uint<4>> read_burst_cnt;
        ch_reg<ch_uint<ID_WIDTH>> read_id_reg;
        ch_reg<ch_uint<ADDR_WIDTH>> read_addr_reg;

        SlaveState()
            : write_state(0_d), read_state(0_d),
              reg0(0_d), reg1(0_d), reg2(0_d), reg3(0_d),
              reg4(0_d), reg5(0_d), reg6(0_d), reg7(0_d),
              reg8(0_d), reg9(0_d), reg10(0_d), reg11(0_d),
              reg12(0_d), reg13(0_d), reg14(0_d), reg15(0_d),
              write_burst_cnt(0_d), write_id_reg(0_d),
              read_burst_cnt(0_d), read_id_reg(0_d), read_addr_reg(0_d) {}
    };

    // ID + transaction tracking: write_id_reg, read_id_reg, read_addr_reg,
    // write_burst_cnt, read_burst_cnt, plus write/read state machine updates.
    template <typename TS>
    void build_id_handshake(
        TS &s,
        ch_bool aw_ready_sig, ch_bool w_ready_sig, ch_bool ar_ready_sig,
        ch_bool is_write_idle, ch_bool is_aw_wait, ch_bool is_b_resp,
        ch_bool is_read_idle, ch_bool is_r_data,
        ch_bool burst_len_match) {
        // Write ID capture (latch AW ID when AW handshake)
        s.write_id_reg->next = select(aw_ready_sig, io().awid, s.write_id_reg);

        // Read ID + address capture
        s.read_addr_reg->next = select(ar_ready_sig, io().araddr, s.read_addr_reg);
        s.read_id_reg->next = select(ar_ready_sig, io().arid, s.read_id_reg);

        // Write burst counter
        auto burst_not_last = !io().wlast;
        s.write_burst_cnt->next = select(w_ready_sig && burst_not_last,
                                          s.write_burst_cnt + ch_uint<4>(1_d),
                                          ch_uint<4>(0_d));

        // Read burst counter
        auto r_valid_ready = select(io().rvalid, io().rready, ch_bool(false));
        s.read_burst_cnt->next = select(ar_ready_sig, ch_uint<4>(0_d),
                                        select(select(is_read_idle, r_valid_ready, ch_bool(false)),
                                                s.read_burst_cnt + ch_uint<4>(1_d),
                                                s.read_burst_cnt));

        // Write state machine
        auto w_handshake = select(io().wvalid, io().wready, ch_bool(false));
        auto next_write_state = select(is_write_idle,
                                        select(io().awvalid, ch_uint<2>(1_d), ch_uint<2>(0_d)),
                                        select(is_aw_wait,
                                            select(w_handshake,
                                                select(io().wlast, ch_uint<2>(3_d), ch_uint<2>(2_d)),
                                                ch_uint<2>(1_d)),
                                            select(is_write_idle, ch_uint<2>(0_d),
                                                select(io().bready, ch_uint<2>(0_d), ch_uint<2>(3_d)))));
        s.write_state->next = next_write_state;

        // Read state machine
        auto r_handshake = select(io().rvalid, io().rready, ch_bool(false));
        auto next_read_state = select(is_read_idle,
                                       select(io().arvalid, ch_uint<2>(2_d), ch_uint<2>(0_d)),
                                       select(is_read_idle, ch_uint<2>(0_d),
                                           select(r_handshake,
                                                select(burst_len_match, ch_uint<2>(0_d), ch_uint<2>(2_d)),
                                                ch_uint<2>(2_d))));
        s.read_state->next = next_read_state;

        // Stash for response logic
        last_is_b_resp_ = is_b_resp;
        last_is_r_data_ = is_r_data;
        last_burst_len_match_ = burst_len_match;
    }

    // Data channels: register file + W channel register writes + R channel mux
    template <typename TS>
    void build_data_channels(TS &s, ch_bool w_ready_sig) {
        auto addr_index = io().awaddr >> ch_uint<ADDR_WIDTH>(2_d);

        auto sel0 = (addr_index == ch_uint<ADDR_WIDTH>(0_d));
        auto sel1 = (addr_index == ch_uint<ADDR_WIDTH>(1_d));
        auto sel2 = (addr_index == ch_uint<ADDR_WIDTH>(2_d));
        auto sel3 = (addr_index == ch_uint<ADDR_WIDTH>(3_d));
        auto sel4 = (addr_index == ch_uint<ADDR_WIDTH>(4_d));
        auto sel5 = (addr_index == ch_uint<ADDR_WIDTH>(5_d));
        auto sel6 = (addr_index == ch_uint<ADDR_WIDTH>(6_d));
        auto sel7 = (addr_index == ch_uint<ADDR_WIDTH>(7_d));
        auto sel8 = (addr_index == ch_uint<ADDR_WIDTH>(8_d));
        auto sel9 = (addr_index == ch_uint<ADDR_WIDTH>(9_d));
        auto sel10 = (addr_index == ch_uint<ADDR_WIDTH>(10_d));
        auto sel11 = (addr_index == ch_uint<ADDR_WIDTH>(11_d));
        auto sel12 = (addr_index == ch_uint<ADDR_WIDTH>(12_d));
        auto sel13 = (addr_index == ch_uint<ADDR_WIDTH>(13_d));
        auto sel14 = (addr_index == ch_uint<ADDR_WIDTH>(14_d));
        auto sel15 = (addr_index == ch_uint<ADDR_WIDTH>(15_d));

        auto we0 = w_ready_sig && sel0;
        auto we1 = w_ready_sig && sel1;
        auto we2 = w_ready_sig && sel2;
        auto we3 = w_ready_sig && sel3;
        auto we4 = w_ready_sig && sel4;
        auto we5 = w_ready_sig && sel5;
        auto we6 = w_ready_sig && sel6;
        auto we7 = w_ready_sig && sel7;
        auto we8 = w_ready_sig && sel8;
        auto we9 = w_ready_sig && sel9;
        auto we10 = w_ready_sig && sel10;
        auto we11 = w_ready_sig && sel11;
        auto we12 = w_ready_sig && sel12;
        auto we13 = w_ready_sig && sel13;
        auto we14 = w_ready_sig && sel14;
        auto we15 = w_ready_sig && sel15;

        s.reg0->next = select(we0, io().wdata, s.reg0);
        s.reg1->next = select(we1, io().wdata, s.reg1);
        s.reg2->next = select(we2, io().wdata, s.reg2);
        s.reg3->next = select(we3, io().wdata, s.reg3);
        s.reg4->next = select(we4, io().wdata, s.reg4);
        s.reg5->next = select(we5, io().wdata, s.reg5);
        s.reg6->next = select(we6, io().wdata, s.reg6);
        s.reg7->next = select(we7, io().wdata, s.reg7);
        s.reg8->next = select(we8, io().wdata, s.reg8);
        s.reg9->next = select(we9, io().wdata, s.reg9);
        s.reg10->next = select(we10, io().wdata, s.reg10);
        s.reg11->next = select(we11, io().wdata, s.reg11);
        s.reg12->next = select(we12, io().wdata, s.reg12);
        s.reg13->next = select(we13, io().wdata, s.reg13);
        s.reg14->next = select(we14, io().wdata, s.reg14);
        s.reg15->next = select(we15, io().wdata, s.reg15);

        // Read mux (computed here, used by response_logic via scratch)
        auto read_addr_index = s.read_addr_reg >> ch_uint<ADDR_WIDTH>(2_d);
        ch_uint<DATA_WIDTH> read_val(0_d);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(0_d), s.reg0, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(1_d), s.reg1, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(2_d), s.reg2, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(3_d), s.reg3, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(4_d), s.reg4, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(5_d), s.reg5, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(6_d), s.reg6, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(7_d), s.reg7, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(8_d), s.reg8, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(9_d), s.reg9, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(10_d), s.reg10, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(11_d), s.reg11, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(12_d), s.reg12, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(13_d), s.reg13, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(14_d), s.reg14, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(15_d), s.reg15, read_val);

        last_read_val_ = read_val;
    }

    // Response channels: B (write response) + R (read data) outputs
    template <typename TS>
    void build_response_logic(TS &s) {
        io().bid = s.write_id_reg;
        io().bresp = ch_uint<2>(static_cast<unsigned>(AxiResp::OKAY));
        io().bvalid = last_is_b_resp_;

        io().rdata = last_read_val_;
        io().rid = s.read_id_reg;
        io().rresp = ch_uint<2>(static_cast<unsigned>(AxiResp::OKAY));
        io().rvalid = last_is_r_data_;
        io().rlast = select(last_is_r_data_, last_burst_len_match_, ch_bool(false));
    }

    void describe() override {
        SlaveState s;

        // Channel ready signals
        auto is_write_idle = (s.write_state == ch_uint<2>(0_d));
        auto aw_ready_sig = select(is_write_idle, io().awvalid, ch_bool(false));
        io().awready = aw_ready_sig;

        auto is_aw_wait = (s.write_state == ch_uint<2>(1_d));
        auto w_ready_sig = select(is_aw_wait, io().wvalid, ch_bool(false));
        io().wready = w_ready_sig;

        auto is_read_idle = (s.read_state == ch_uint<2>(0_d));
        auto ar_ready_sig = select(is_read_idle, io().arvalid, ch_bool(false));
        io().arready = ar_ready_sig;

        auto is_b_resp = (s.write_state == ch_uint<2>(3_d));
        auto is_r_data = (s.read_state == ch_uint<2>(2_d));
        auto burst_len_match = (s.read_burst_cnt == io().arlen);

        // ========================================================================
        // Helper invocations
        // ========================================================================
        build_id_handshake(s, aw_ready_sig, w_ready_sig, ar_ready_sig,
                            is_write_idle, is_aw_wait, is_b_resp,
                            is_read_idle, is_r_data, burst_len_match);
        build_data_channels(s, w_ready_sig);
        build_response_logic(s);
    }

private:
    ch_bool last_is_b_resp_{ch_bool(false)};
    ch_bool last_is_r_data_{ch_bool(false)};
    ch_bool last_burst_len_match_{ch_bool(false)};
    ch_uint<DATA_WIDTH> last_read_val_{0_d};
};

} // namespace axi4
