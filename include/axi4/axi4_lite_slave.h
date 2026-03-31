/**
 * AXI4-Lite Slave Controller - 简化版
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace axi4 {

template <unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32, unsigned NUM_REGS = 4>
class AxiLiteSlave : public ch::Component {
public:
    __io(
        ch_in<ch_uint<ADDR_WIDTH>> awaddr;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;
        
        ch_in<ch_uint<DATA_WIDTH>> wdata;
        ch_in<ch_uint<DATA_WIDTH/8>> wstrb;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;
        
        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> araddr;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;
        
        ch_out<ch_uint<DATA_WIDTH>> rdata;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;
    )
    
    AxiLiteSlave(ch::Component* parent = nullptr, const std::string& name = "axi_lite_slave")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 状态机：0=IDLE, 1=BUSY
        ch_reg<ch_bool> busy(ch_bool(false));
        
        // 寄存器文件
        ch_reg<ch_uint<DATA_WIDTH>> reg0(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg1(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg2(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg3(0_d);
        
        // 地址解码 (2 位，4 字节对齐)
        auto addr_bits = io().awaddr >> ch_uint<ADDR_WIDTH>(2_d);
        addr_bits = addr_bits & ch_uint<ADDR_WIDTH>(3_d);  // 取低 2 位
        
        // 写地址握手
        auto aw_handshake = select(io().awvalid, ch_bool(true), ch_bool(false));
        aw_handshake = select(busy, ch_bool(false), aw_handshake);
        io().awready = aw_handshake;
        
        // 写数据握手
        auto w_handshake = select(io().wvalid, aw_handshake, ch_bool(false));
        io().wready = w_handshake;
        
        // 写寄存器
        auto sel0 = (addr_bits == ch_uint<ADDR_WIDTH>(0_d));
        auto sel1 = (addr_bits == ch_uint<ADDR_WIDTH>(1_d));
        auto sel2 = (addr_bits == ch_uint<ADDR_WIDTH>(2_d));
        auto sel3 = (addr_bits == ch_uint<ADDR_WIDTH>(3_d));
        
        auto we0 = select(w_handshake, sel0, ch_bool(false));
        auto we1 = select(w_handshake, sel1, ch_bool(false));
        auto we2 = select(w_handshake, sel2, ch_bool(false));
        auto we3 = select(w_handshake, sel3, ch_bool(false));
        
        reg0->next = select(we0, io().wdata, reg0);
        reg1->next = select(we1, io().wdata, reg1);
        reg2->next = select(we2, io().wdata, reg2);
        reg3->next = select(we3, io().wdata, reg3);
        
        // 写响应
        io().bvalid = w_handshake;
        io().bresp = ch_uint<2>(0_d);  // OKAY
        
        // 读地址握手
        auto ar_addr_bits = io().araddr >> ch_uint<ADDR_WIDTH>(2_d);
        ar_addr_bits = ar_addr_bits & ch_uint<ADDR_WIDTH>(3_d);
        
        auto ar_handshake = select(io().arvalid, ch_bool(true), ch_bool(false));
        ar_handshake = select(busy, ch_bool(false), ar_handshake);
        io().arready = ar_handshake;
        
        // 读数据
        auto read_sel0 = (ar_addr_bits == ch_uint<ADDR_WIDTH>(0_d));
        auto read_sel1 = (ar_addr_bits == ch_uint<ADDR_WIDTH>(1_d));
        auto read_sel2 = (ar_addr_bits == ch_uint<ADDR_WIDTH>(2_d));
        auto read_sel3 = (ar_addr_bits == ch_uint<ADDR_WIDTH>(3_d));
        
        ch_uint<DATA_WIDTH> read_val(0_d);
        read_val = select(read_sel0, reg0, read_val);
        read_val = select(read_sel1, reg1, read_val);
        read_val = select(read_sel2, reg2, read_val);
        read_val = select(read_sel3, reg3, read_val);
        
        io().rdata = read_val;
        io().rvalid = ar_handshake;
        io().rresp = ch_uint<2>(0_d);  // OKAY
        
        // 状态更新
        auto any_handshake = select(aw_handshake, ch_bool(true), ar_handshake);
        auto any_ready = select(io().bready, ch_bool(true), io().rready);
        busy->next = select(any_handshake, ch_bool(true), select(any_ready, ch_bool(false), busy));
    }
};

} // namespace axi4
