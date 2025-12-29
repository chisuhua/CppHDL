#ifndef CHLIB_AXI4LITE_H
#define CHLIB_AXI4LITE_H

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include "chlib/logic.h"
#include <cassert>

using namespace ch::core;

namespace chlib {

/**
 * AXI4-Lite Write Address Channel Signals
 */
template <unsigned ADDR_WIDTH>
struct Axi4LiteWriteAddr {
    ch_uint<ADDR_WIDTH> awaddr;      // Write address
    ch_uint<3> awprot;              // Write protection
    ch_bool awvalid;                // Write address valid
    ch_bool awready;                // Write address ready
};

/**
 * AXI4-Lite Write Data Channel Signals
 */
template <unsigned DATA_WIDTH>
struct Axi4LiteWriteData {
    ch_uint<DATA_WIDTH> wdata;       // Write data
    ch_uint<DATA_WIDTH/8> wstrb;    // Write strobes
    ch_bool wlast;                  // Write last
    ch_bool wvalid;                 // Write valid
    ch_bool wready;                 // Write ready
};

/**
 * AXI4-Lite Write Response Channel Signals
 */
template <unsigned DATA_WIDTH>
struct Axi4LiteWriteResp {
    ch_uint<2> bresp;               // Write response
    ch_bool bvalid;                 // Write response valid
    ch_bool bready;                 // Write response ready
};

/**
 * AXI4-Lite Read Address Channel Signals
 */
template <unsigned ADDR_WIDTH>
struct Axi4LiteReadAddr {
    ch_uint<ADDR_WIDTH> araddr;      // Read address
    ch_uint<3> arprot;              // Read protection
    ch_bool arvalid;                // Read address valid
    ch_bool arready;                // Read address ready
};

/**
 * AXI4-Lite Read Data Channel Signals
 */
template <unsigned DATA_WIDTH>
struct Axi4LiteReadData {
    ch_uint<DATA_WIDTH> rdata;       // Read data
    ch_uint<2> rresp;               // Read response
    ch_bool rlast;                  // Read last
    ch_bool rvalid;                 // Read valid
    ch_bool rready;                 // Read ready
};

/**
 * AXI4-Lite Interface (Master side)
 */
template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH>
struct Axi4LiteMaster {
    // Write address channel
    Axi4LiteWriteAddr<ADDR_WIDTH> aw;
    // Write data channel
    Axi4LiteWriteData<DATA_WIDTH> w;
    // Write response channel
    Axi4LiteWriteResp<DATA_WIDTH> b;
    // Read address channel
    Axi4LiteReadAddr<ADDR_WIDTH> ar;
    // Read data channel
    Axi4LiteReadData<DATA_WIDTH> r;
};

/**
 * AXI4-Lite Interface (Slave side)
 */
template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH>
struct Axi4LiteSlave {
    // Write address channel
    Axi4LiteWriteAddr<ADDR_WIDTH> aw;
    // Write data channel
    Axi4LiteWriteData<DATA_WIDTH> w;
    // Write response channel
    Axi4LiteWriteResp<DATA_WIDTH> b;
    // Read address channel
    Axi4LiteReadAddr<ADDR_WIDTH> ar;
    // Read data channel
    Axi4LiteReadData<DATA_WIDTH> r;
};

/**
 * AXI4-Lite Slave Memory Component
 * 
 * Implements a simple AXI4-Lite slave memory interface
 */
template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH>
class Axi4LiteMemorySlave {
private:
    ch_ram<(1 << ADDR_WIDTH), DATA_WIDTH> memory;
    
    // Write address state
    ch_reg<ch_uint<ADDR_WIDTH>> write_addr_reg;
    ch_reg<ch_bool> write_addr_valid_reg;
    
    // Write data state
    ch_reg<ch_uint<DATA_WIDTH>> write_data_reg;
    ch_reg<ch_uint<DATA_WIDTH/8>> write_strb_reg;
    ch_reg<ch_bool> write_valid_reg;
    
    // Read address state
    ch_reg<ch_uint<ADDR_WIDTH>> read_addr_reg;
    ch_reg<ch_bool> read_addr_valid_reg;
    
    // Read data state
    ch_reg<ch_uint<DATA_WIDTH>> read_data_reg;
    ch_reg<ch_bool> read_valid_reg;
    
    // Write response state
    ch_reg<ch_uint<2>> write_resp_reg;
    ch_reg<ch_bool> write_resp_valid_reg;

public:
    Axi4LiteMemorySlave(const std::string& name = "axi4lite_mem_slave") : 
        memory(name + "_mem"),
        write_addr_reg(0_d, name + "_waddr"),
        write_addr_valid_reg(false, name + "_waddr_vld"),
        write_data_reg(0_d, name + "_wdata"),
        write_strb_reg(0_d, name + "_wstrb"),
        write_valid_reg(false, name + "_wdata_vld"),
        read_addr_reg(0_d, name + "_raddr"),
        read_addr_valid_reg(false, name + "_raddr_vld"),
        read_data_reg(0_d, name + "_rdata"),
        read_valid_reg(false, name + "_rdata_vld"),
        write_resp_reg(0_d, name + "_wresp"),
        write_resp_valid_reg(false, name + "_wresp_vld") {}

    Axi4LiteSlave<ADDR_WIDTH, DATA_WIDTH> process(
        ch_bool clk,
        ch_bool rst,
        Axi4LiteSlave<ADDR_WIDTH, DATA_WIDTH> axi_in
    ) {
        Axi4LiteSlave<ADDR_WIDTH, DATA_WIDTH> axi_out = axi_in;
        
        // Clock and reset for internal registers
        write_addr_reg->clk = clk;
        write_addr_reg->rst = rst;
        write_addr_valid_reg->clk = clk;
        write_addr_valid_reg->rst = rst;
        write_data_reg->clk = clk;
        write_data_reg->rst = rst;
        write_strb_reg->clk = clk;
        write_strb_reg->rst = rst;
        write_valid_reg->clk = clk;
        write_valid_reg->rst = rst;
        read_addr_reg->clk = clk;
        read_addr_reg->rst = rst;
        read_addr_valid_reg->clk = clk;
        read_addr_valid_reg->rst = rst;
        read_data_reg->clk = clk;
        read_data_reg->rst = rst;
        read_valid_reg->clk = clk;
        read_valid_reg->rst = rst;
        write_resp_reg->clk = clk;
        write_resp_reg->rst = rst;
        write_resp_valid_reg->clk = clk;
        write_resp_valid_reg->rst = rst;
        
        // Write address channel handshake
        ch_bool aw_handshake = axi_in.aw.awvalid && axi_out.aw.awready;
        axi_out.aw.awready = !write_addr_valid_reg;
        
        // Update write address register on handshake
        write_addr_reg->next = select(aw_handshake, axi_in.aw.awaddr, write_addr_reg);
        write_addr_valid_reg->next = select(aw_handshake, true, 
                                           select(axi_in.w.wready && write_valid_reg, false, write_addr_valid_reg));
        
        // Write data channel handshake
        ch_bool w_handshake = axi_in.w.wvalid && axi_out.w.wready;
        axi_out.w.wready = !write_valid_reg;
        
        // Update write data register on handshake
        write_data_reg->next = select(w_handshake, axi_in.w.wdata, write_data_reg);
        write_strb_reg->next = select(w_handshake, axi_in.w.wstrb, write_strb_reg);
        write_valid_reg->next = select(w_handshake, true, 
                                      select(axi_in.b.bready && write_resp_valid_reg, false, write_valid_reg));
        
        // Perform write to memory when both address and data are valid
        ch_bool perform_write = write_addr_valid_reg && write_valid_reg;
        memory.write(clk, write_addr_reg, write_data_reg, perform_write);
        
        // Write response channel
        axi_out.b.bresp = write_resp_reg;
        axi_out.b.bvalid = write_resp_valid_reg;
        ch_bool b_handshake = axi_in.b.bready && axi_out.b.bvalid;
        
        // Update write response
        write_resp_reg->next = select(perform_write, 0_d, write_resp_reg);  // OKAY response
        write_resp_valid_reg->next = select(perform_write, true,
                                           select(b_handshake, false, write_resp_valid_reg));
        
        // Read address channel handshake
        ch_bool ar_handshake = axi_in.ar.arvalid && axi_out.ar.arready;
        axi_out.ar.arready = !read_addr_valid_reg;
        
        // Update read address register on handshake
        read_addr_reg->next = select(ar_handshake, axi_in.ar.araddr, read_addr_reg);
        read_addr_valid_reg->next = select(ar_handshake, true,
                                          select(axi_in.r.rready && read_valid_reg, false, read_addr_valid_reg));
        
        // Read data channel
        ch_uint<DATA_WIDTH> read_data = memory.read(read_addr_reg);
        axi_out.r.rdata = select(read_addr_valid_reg, read_data, read_data_reg);
        axi_out.r.rresp = 0_d;  // OKAY response
        axi_out.r.rvalid = read_valid_reg;
        
        // Update read data register
        read_data_reg->next = select(read_addr_valid_reg, read_data, read_data_reg);
        read_valid_reg->next = select(read_addr_valid_reg, true,
                                     select(axi_in.r.rready, false, read_valid_reg));
        
        // Handle read response handshake
        ch_bool r_handshake = axi_in.r.rready && axi_out.r.rvalid;
        
        return axi_out;
    }
};

/**
 * AXI4-Lite Simple Master Component
 * 
 * Implements a simple AXI4-Lite master for testing purposes
 */
template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH>
class Axi4LiteSimpleMaster {
private:
    enum class MasterState {
        IDLE = 0,
        WRITE_ADDR = 1,
        WRITE_DATA = 2,
        WRITE_RESP = 3,
        READ_ADDR = 4,
        READ_DATA = 5
    };
    
    ch_reg<ch_uint<2>> state_reg;
    ch_reg<ch_uint<ADDR_WIDTH>> target_addr_reg;
    ch_reg<ch_uint<DATA_WIDTH>> target_data_reg;
    ch_reg<ch_bool> do_write_reg;
    ch_reg<ch_bool> transaction_done_reg;

public:
    Axi4LiteSimpleMaster(const std::string& name = "axi4lite_simple_master") : 
        state_reg(0_d, name + "_state"),
        target_addr_reg(0_d, name + "_target_addr"),
        target_data_reg(0_d, name + "_target_data"),
        do_write_reg(false, name + "_do_write"),
        transaction_done_reg(false, name + "_done") {}

    Axi4LiteMaster<ADDR_WIDTH, DATA_WIDTH> process(
        ch_bool clk,
        ch_bool rst,
        ch_bool start,
        ch_bool write,
        ch_uint<ADDR_WIDTH> addr,
        ch_uint<DATA_WIDTH> data
    ) {
        Axi4LiteMaster<ADDR_WIDTH, DATA_WIDTH> axi_out;
        
        // Clock and reset for internal registers
        state_reg->clk = clk;
        state_reg->rst = rst;
        target_addr_reg->clk = clk;
        target_addr_reg->rst = rst;
        target_data_reg->clk = clk;
        target_data_reg->rst = rst;
        do_write_reg->clk = clk;
        do_write_reg->rst = rst;
        transaction_done_reg->clk = clk;
        transaction_done_reg->rst = rst;
        
        // State transition logic
        ch_uint<2> next_state = state_reg;
        ch_bool transaction_complete = false;
        
        switch (state_reg.value()) {
            case static_cast<int>(MasterState::IDLE):
                next_state = select(start, 
                                   select(write, 
                                         make_literal(static_cast<int>(MasterState::WRITE_ADDR)), 
                                         make_literal(static_cast<int>(MasterState::READ_ADDR))), 
                                   make_literal(static_cast<int>(MasterState::IDLE)));
                break;
                
            case static_cast<int>(MasterState::WRITE_ADDR):
                next_state = select(axi_out.aw.awready && axi_out.aw.awvalid, 
                                   make_literal(static_cast<int>(MasterState::WRITE_DATA)), 
                                   make_literal(static_cast<int>(MasterState::WRITE_ADDR)));
                break;
                
            case static_cast<int>(MasterState::WRITE_DATA):
                next_state = select(axi_out.w.wready && axi_out.w.wvalid, 
                                   make_literal(static_cast<int>(MasterState::WRITE_RESP)), 
                                   make_literal(static_cast<int>(MasterState::WRITE_DATA)));
                break;
                
            case static_cast<int>(MasterState::WRITE_RESP):
                next_state = select(axi_out.b.bready && axi_out.b.bvalid, 
                                   make_literal(static_cast<int>(MasterState::IDLE)), 
                                   make_literal(static_cast<int>(MasterState::WRITE_RESP)));
                transaction_complete = axi_out.b.bready && axi_out.b.bvalid;
                break;
                
            case static_cast<int>(MasterState::READ_ADDR):
                next_state = select(axi_out.ar.arready && axi_out.ar.arvalid, 
                                   make_literal(static_cast<int>(MasterState::READ_DATA)), 
                                   make_literal(static_cast<int>(MasterState::READ_ADDR)));
                break;
                
            case static_cast<int>(MasterState::READ_DATA):
                next_state = select(axi_out.r.rready && axi_out.r.rvalid, 
                                   make_literal(static_cast<int>(MasterState::IDLE)), 
                                   make_literal(static_cast<int>(MasterState::READ_DATA)));
                transaction_complete = axi_out.r.rready && axi_out.r.rvalid;
                break;
        }
        
        // Update registers
        state_reg->next = select(rst, 0_d, next_state);
        target_addr_reg->next = select(start, addr, target_addr_reg);
        target_data_reg->next = select(start, data, target_data_reg);
        do_write_reg->next = select(start, write, do_write_reg);
        transaction_done_reg->next = select(rst, false, 
                                          select(transaction_complete, true, 
                                                select(axi_out.r.rvalid, false, transaction_done_reg)));
        
        // Initialize all signals to defaults
        axi_out.aw.awaddr = target_addr_reg;
        axi_out.aw.awprot = 0_d;
        axi_out.aw.awvalid = false;
        
        axi_out.w.wdata = target_data_reg;
        axi_out.w.wstrb = ~ch_uint<DATA_WIDTH/8>(0_d);  // All bytes valid
        axi_out.w.wlast = true;
        axi_out.w.wvalid = false;
        
        axi_out.b.bready = true;
        
        axi_out.ar.araddr = target_addr_reg;
        axi_out.ar.arprot = 0_d;
        axi_out.ar.arvalid = false;
        
        axi_out.r.rready = true;
        
        // Set valid signals based on current state
        ch_bool is_write = do_write_reg;
        ch_bool is_idle = (state_reg == make_literal(static_cast<int>(MasterState::IDLE)));
        ch_bool start_write_addr = (state_reg == make_literal(static_cast<int>(MasterState::WRITE_ADDR)));
        ch_bool start_write_data = (state_reg == make_literal(static_cast<int>(MasterState::WRITE_DATA)));
        ch_bool start_read_addr = (state_reg == make_literal(static_cast<int>(MasterState::READ_ADDR)));
        
        axi_out.aw.awvalid = select(is_write && start_write_addr, true, false);
        axi_out.w.wvalid = select(is_write && start_write_data, true, false);
        axi_out.ar.arvalid = select(!is_write && start_read_addr, true, false);
        
        return axi_out;
    }
    
    ch_bool is_transaction_done() const {
        return transaction_done_reg;
    }
};

} // namespace chlib

#endif // CHLIB_AXI4LITE_H