/**
 * @file axi_i2c.h
 * @brief AXI4-Lite I2C Controller - I2C Master Mode
 * 
 * Features:
 * - AXI4-Lite Slave Interface
 * - I2C Master Mode
 * - START/STOP Condition Generation
 * - TX/RX Data Registers
 * - Control Register (Enable, Start, Stop, ACK Enable)
 * - Status Register (Busy, ACK Received, Bus Busy, TX Empty, RX Full)
 * - Baud Rate Prescaler
 * - State Machine: IDLE → START → WRITE → READ → STOP
 * 
 * Register Map:
 * - 0x00: CTRL     - Control Register
 *            bit0: ENABLE   - I2C Controller Enable
 *            bit1: START    - Generate START condition (write 1 to trigger)
 *            bit2: STOP     - Generate STOP condition (write 1 to trigger)
 *            bit3: ACK_EN   - ACK Enable for receive (1=send ACK, 0=send NACK)
 *            bit4: READ     - Read operation (1=read, 0=write)
 *            bit5: INT_EN   - Interrupt Enable
 * - 0x04: STATUS   - Status Register
 *            bit0: BUSY     - Transfer in progress
 *            bit1: ACK_RECV - ACK received from slave
 *            bit2: BUS_BUSY - I2C bus is busy
 *            bit3: TX_EMPTY - TX data register empty
 *            bit4: RX_FULL  - RX data register has data
 *            bit5: DONE     - Transfer complete
 *            bit6: IRQ      - Interrupt pending
 * - 0x08: TX_DATA  - Transmit Data Register (8-bit)
 * - 0x0C: RX_DATA  - Receive Data Register (8-bit)
 * - 0x10: PRESCALE - Baud Rate Prescaler (16-bit)
 *            f_SCL = f_CLK / (2 * PRESCALE * 8)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

namespace axi4 {

using namespace ch::core;

template <unsigned PRESCALE_WIDTH = 16>
class AxiLiteI2c : public ch::Component {
public:
    __io(
        // Write Address Channel
        ch_in<ch_uint<32>> awaddr;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;
        
        // Write Data Channel
        ch_in<ch_uint<32>> wdata;
        ch_in<ch_uint<4>> wstrb;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;
        
        // Write Response Channel
        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;
        
        // Read Address Channel
        ch_in<ch_uint<32>> araddr;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;
        
        // Read Data Channel
        ch_out<ch_uint<32>> rdata;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;
        
        // I2C Physical Interface
        ch_out<ch_bool> i2c_sda;
        ch_in<ch_bool> i2c_sda_in;
        ch_out<ch_bool> i2c_scl;
        ch_in<ch_bool> i2c_scl_in;
        
        ch_out<ch_bool> irq;
        ch_in<ch_bool> rst_n;
    )
    
    AxiLiteI2c(ch::Component* parent = nullptr, const std::string& name = "axi_i2c")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    // Phase 5: describe() split into 2 helpers. All local ch_reg<> state
    // hoisted into I2cState struct local to describe() and passed by reference.
    struct I2cState {
        ch_reg<ch_bool> busy;
        ch_reg<ch_bool> aw_done;
        ch_reg<ch_bool> r_valid;
        ch_reg<ch_uint<32>> ctrl_reg;
        ch_reg<ch_uint<32>> status_reg;
        ch_reg<ch_uint<8>> tx_data_reg;
        ch_reg<ch_uint<8>> rx_data_reg;
        ch_reg<ch_uint<PRESCALE_WIDTH>> prescale_reg;
        ch_reg<ch_uint<4>> i2c_state;
        ch_reg<ch_uint<3>> bit_counter;
        ch_reg<ch_uint<PRESCALE_WIDTH>> clk_div;
        ch_reg<ch_uint<8>> shift_reg;
        ch_reg<ch_bool> sda_out_reg;
        ch_reg<ch_bool> scl_out_reg;

        I2cState()
            : busy(ch_bool(false)),
              aw_done(ch_bool(false)),
              r_valid(ch_bool(false)),
              ctrl_reg(0_d),
              status_reg(0_d),
              tx_data_reg(0_d),
              rx_data_reg(0_d),
              prescale_reg(100_d),
              i2c_state(ch_uint<4>(0_d)),
              bit_counter(0_d),
              clk_div(0_d),
              shift_reg(0_d),
              sda_out_reg(ch_bool(true)),
              scl_out_reg(ch_bool(true)) {}
    };

    // Register bank: address decode + AXI4-Lite read/write transactions
    // for the 5 user-visible registers (CTRL, STATUS, TX_DATA, RX_DATA, PRESCALE).
    template <typename TS>
    void build_register_bank(
        TS &s,
        ch_bool aw_handshake, ch_bool w_handshake, ch_bool ar_handshake,
        ch_bool we_ctrl, ch_bool we_tx_data, ch_bool we_prescale) {
        // Control register update (clear START/STOP bits)
        auto ctrl_write_val = io().wdata & ch_uint<32>(~static_cast<uint32_t>(6_d));
        s.ctrl_reg->next = select(we_ctrl, ctrl_write_val, s.ctrl_reg);

        // TX data register
        s.tx_data_reg->next = select(we_tx_data, io().wdata, s.tx_data_reg);

        // Prescale register
        s.prescale_reg->next = select(we_prescale, io().wdata, s.prescale_reg);

        // AXI write response
        io().bvalid = w_handshake;
        io().bresp = ch_uint<2>(0_d);

        // AXI read response
        auto rd_addr = io().araddr >> ch_uint<32>(2_d);
        rd_addr = rd_addr & ch_uint<32>(7_d);
        auto sel_read_ctrl = (rd_addr == ch_uint<32>(0_d));
        auto sel_read_status = (rd_addr == ch_uint<32>(1_d));
        auto sel_read_tx = (rd_addr == ch_uint<32>(2_d));
        auto sel_read_rx = (rd_addr == ch_uint<32>(3_d));
        auto sel_read_prescale = (rd_addr == ch_uint<32>(4_d));

        ch_uint<32> read_val(0_d);
        read_val = select(sel_read_ctrl, s.ctrl_reg, read_val);
        read_val = select(sel_read_status, s.status_reg, read_val);
        read_val = select(sel_read_tx, ch_uint<32>(s.tx_data_reg), read_val);
        read_val = select(sel_read_rx, ch_uint<32>(s.rx_data_reg), read_val);
        read_val = select(sel_read_prescale, ch_uint<32>(s.prescale_reg), read_val);

        io().rdata = read_val;
        io().rresp = ch_uint<2>(0_d);

        // AXI handshake registers
        s.aw_done->next = select(aw_handshake, ch_bool(true), select(io().bready, ch_bool(false), s.aw_done));
        s.r_valid->next = select(ar_handshake, ch_bool(true), select(io().rready, ch_bool(false), s.r_valid));
        s.busy->next = select(aw_handshake, ch_bool(true),
                              select(ar_handshake, ch_bool(true),
                                     select(io().bready, ch_bool(false),
                                            select(io().rready, ch_bool(false), s.busy))));
    }

    // I2C FSM: clock divider, state machine, SCL/SDA outputs, shift register,
    // status register, interrupt.
    template <typename TS>
    void build_i2c_fsm(TS &s, ch_bool ctrl_enable) {
        // State decode
        auto is_idle = (s.i2c_state == ch_uint<4>(0_d));
        auto is_start = (s.i2c_state == ch_uint<4>(1_d));
        auto is_addr_shift = (s.i2c_state == ch_uint<4>(2_d));
        auto is_addr_ack = (s.i2c_state == ch_uint<4>(3_d));
        auto is_write_bit = (s.i2c_state == ch_uint<4>(4_d));
        auto is_write_ack = (s.i2c_state == ch_uint<4>(5_d));
        auto is_read_bit = (s.i2c_state == ch_uint<4>(6_d));
        auto is_read_ack = (s.i2c_state == ch_uint<4>(7_d));
        auto is_stop = (s.i2c_state == ch_uint<4>(8_d));
        auto is_done = (s.i2c_state == ch_uint<4>(9_d));

        // Control bits
        auto ctrl_start = (s.ctrl_reg & ch_uint<32>(2_d)) != ch_uint<32>(0_d);
        auto ctrl_stop = (s.ctrl_reg & ch_uint<32>(4_d)) != ch_uint<32>(0_d);

        // Clock divider
        auto clk_en = !is_idle && ctrl_enable;
        auto clk_div_max = s.prescale_reg - ch_uint<PRESCALE_WIDTH>(1_d);
        s.clk_div->next = select(clk_en,
                                  select(s.clk_div >= clk_div_max, ch_uint<PRESCALE_WIDTH>(0_d),
                                         s.clk_div + ch_uint<PRESCALE_WIDTH>(1_d)),
                                  ch_uint<PRESCALE_WIDTH>(0_d));
        auto clk_end = (s.clk_div == clk_div_max);

        // State transitions
        auto start_requested = ctrl_start && is_idle;
        auto stop_requested = ctrl_stop && is_idle;
        auto shift_done = (s.bit_counter == ch_uint<3>(7_d));

        ch_uint<4> next_state_val(0_d);
        next_state_val = select(is_idle, select(start_requested, ch_uint<4>(1_d), select(stop_requested, ch_uint<4>(8_d), ch_uint<4>(0_d))), next_state_val);
        next_state_val = select(is_start, select(clk_end, ch_uint<4>(2_d), ch_uint<4>(1_d)), next_state_val);
        next_state_val = select(is_addr_shift, select(clk_end, select(shift_done, ch_uint<4>(3_d), ch_uint<4>(2_d)), ch_uint<4>(2_d)), next_state_val);
        next_state_val = select(is_addr_ack, select(clk_end, ch_uint<4>(4_d), ch_uint<4>(3_d)), next_state_val);
        next_state_val = select(is_write_bit, select(clk_end, select(shift_done, ch_uint<4>(5_d), ch_uint<4>(4_d)), ch_uint<4>(4_d)), next_state_val);
        next_state_val = select(is_write_ack, select(clk_end, ch_uint<4>(0_d), ch_uint<4>(5_d)), next_state_val);
        next_state_val = select(is_read_bit, select(clk_end, select(shift_done, ch_uint<4>(7_d), ch_uint<4>(6_d)), ch_uint<4>(6_d)), next_state_val);
        next_state_val = select(is_read_ack, select(clk_end, ch_uint<4>(0_d), ch_uint<4>(7_d)), next_state_val);
        next_state_val = select(is_stop, select(clk_end, ch_uint<4>(9_d), ch_uint<4>(8_d)), next_state_val);
        next_state_val = select(is_done, ch_uint<4>(0_d), next_state_val);

        s.i2c_state->next = next_state_val;

        // SCL output
        auto scl_toggle = !is_idle && !is_done && ctrl_enable;
        auto scl_target = select(scl_toggle,
                                  select(s.clk_div < (s.prescale_reg >> ch_uint<PRESCALE_WIDTH>(1_d)),
                                         ch_bool(false), ch_bool(true)),
                                  ch_bool(true));
        s.scl_out_reg->next = scl_target;
        io().i2c_scl = s.scl_out_reg;

        // SDA output
        auto data_msb = (s.shift_reg >> ch_uint<8>(7_d)) & ch_uint<8>(1_d);
        auto sda_data = data_msb != ch_uint<8>(0_d);

        auto start_cond = is_start && clk_end;
        auto stop_cond = is_stop && clk_end;

        auto sda_target = select(start_cond, ch_bool(false),
                                  select(stop_cond, ch_bool(true),
                                         select(!s.scl_out_reg, sda_data, s.sda_out_reg)));
        s.sda_out_reg->next = sda_target;
        io().i2c_sda = s.sda_out_reg;

        // Shift register
        auto load_tx = is_start && clk_end;
        auto shift_en = (is_write_bit || is_addr_shift) && clk_end && !shift_done;

        auto next_bit_counter = select(load_tx, ch_uint<3>(0_d),
                                        select(shift_en && !shift_done,
                                                s.bit_counter + ch_uint<3>(1_d), s.bit_counter));
        s.bit_counter->next = next_bit_counter;

        auto shifted_val = (s.shift_reg << ch_uint<8>(1_d)) | ch_uint<8>(0_d);
        s.shift_reg->next = select(load_tx, s.tx_data_reg,
                                    select(shift_en, shifted_val, s.shift_reg));

        // Status register
        auto busy_set = !is_idle && !is_done;
        auto tx_empty = is_idle;

        ch_uint<32> status_next(0_d);
        status_next = select(busy_set, status_next | ch_uint<32>(1_d), status_next);
        status_next = select(tx_empty, status_next | ch_uint<32>(8_d), status_next);
        s.status_reg->next = status_next;

        // Interrupt (simplified)
        io().irq = ch_bool(false);
    }

    void describe() override {
        I2cState s;

        // ========================================================================
        // AXI4-Lite Address Decode
        // ========================================================================
        auto wr_addr = io().awaddr >> ch_uint<32>(2_d);
        wr_addr = wr_addr & ch_uint<32>(7_d);

        // Write/read handshakes
        auto aw_handshake = select(io().awvalid, select(s.busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().awready = aw_handshake;
        auto w_handshake = select(io().wvalid, s.aw_done, ch_bool(false));
        io().wready = w_handshake;
        io().bvalid = select(s.aw_done, ch_bool(true), ch_bool(false));

        auto ar_handshake = select(io().arvalid, select(s.busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().arready = ar_handshake;
        io().rvalid = select(s.r_valid, ch_bool(true), ch_bool(false));

        // Write enable signals
        auto sel_ctrl = (wr_addr == ch_uint<32>(0_d));
        auto sel_tx_data = (wr_addr == ch_uint<32>(2_d));
        auto sel_prescale = (wr_addr == ch_uint<32>(4_d));
        auto we_ctrl = select(w_handshake, sel_ctrl, ch_bool(false));
        auto we_tx_data = select(w_handshake, sel_tx_data, ch_bool(false));
        auto we_prescale = select(w_handshake, sel_prescale, ch_bool(false));

        // Control signal for I2C FSM
        auto ctrl_enable = (s.ctrl_reg & ch_uint<32>(1_d)) != ch_uint<32>(0_d);

        // ========================================================================
        // Helper invocations
        // ========================================================================
        build_register_bank(s, aw_handshake, w_handshake, ar_handshake,
                            we_ctrl, we_tx_data, we_prescale);
        build_i2c_fsm(s, ctrl_enable);
    }
};

} // namespace axi4
