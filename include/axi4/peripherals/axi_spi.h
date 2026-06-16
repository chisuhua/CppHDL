/**
 * @file axi_spi.h
 * @brief AXI4-Lite SPI Controller - SPI Master Mode 0
 * 
 * Features:
 * - AXI4-Lite Slave Interface
 * - SPI Master Mode 0 (CPOL=0, CPHA=0)
 * - 8-bit or 32-bit Data Width Support
 * - TX/RX Data Registers
 * - Control Register (ENABLE, START, Interrupt Enable)
 * - Status Register (BUSY, TX_EMPTY, RX_FULL)
 * - Programmable Baud Rate Divider
 * - Shift Register for Serial Transfer
 * 
 * Register Map:
 * - 0x00: TX_DATA  - Transmit Data Register (write triggers transfer)
 * - 0x04: RX_DATA  - Receive Data Register (read to get received data)
 * - 0x08: STATUS   - Status Register
 *   - bit0: BUSY     - Transfer in progress
 *   - bit1: TX_EMPTY - TX buffer empty (ready for new data)
 *   - bit2: RX_FULL  - RX data available
 *   - bit3: RX_OVERRUN - RX overrun error
 * - 0x0C: CTRL     - Control Register
 *   - bit0: ENABLE   - SPI controller enable
 *   - bit1: START    - Start transfer (write 1 to trigger)
 *   - bit2: INT_EN   - Interrupt enable
 *   - bit3: CLR_RX   - Clear RX buffer (write 1 to clear)
 * - 0x10: BAUD     - Baud Rate Divider (SPI clock = SYS clock / (BAUD + 1))
 */

#pragma once

#include "ch.hpp"
#include "component.h"

namespace axi4 {

using namespace ch::core;

// ============================================================================
// AXI4-Lite SPI Controller
// ============================================================================

template <
    unsigned DATA_WIDTH = 8,      // SPI data width (8 or 32)
    unsigned BAUD_WIDTH = 16      // Baud divider width
>
class AxiLiteSpi : public ch::Component {
public:
    __io(
        // ========================================================================
        // AXI4-Lite Slave Interface
        // ========================================================================
        
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
        
        // ========================================================================
        // SPI Physical Interface
        // ========================================================================
        
        ch_out<ch_bool> spi_mosi;   // Master Out Slave In
        ch_in<ch_bool> spi_miso;    // Master In Slave Out
        ch_out<ch_bool> spi_sclk;   // SPI Clock
        ch_out<ch_bool> spi_cs;     // Chip Select (active low)
        ch_out<ch_bool> irq;        // Interrupt Request
        
        // ========================================================================
        // Reset (active low, asynchronous)
        // ========================================================================
        
        ch_in<ch_bool> rst_n;       // Active-low asynchronous reset
    )
    
    AxiLiteSpi(ch::Component* parent = nullptr, const std::string& name = "axi_spi")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    // Phase 5: describe() split into 3 helper methods. Each helper operates on
    // a per-describe() TransferState struct that holds all the ch_reg<> state.
    // describe() sets up the struct, computes the shared combinational signals,
    // and calls the 3 helpers in sequence. Each helper returns outputs that
    // describe() writes to ch_reg<->next or io().
    struct TransferState {
        // AXI handshake
        ch_reg<ch_bool> busy;
        ch_reg<ch_bool> aw_done;
        ch_reg<ch_bool> r_valid;
        // Register storage
        ch_reg<ch_uint<32>> tx_data_reg;
        ch_reg<ch_uint<32>> rx_data_reg;
        ch_reg<ch_uint<32>> status_reg;
        ch_reg<ch_uint<32>> ctrl_reg;
        ch_reg<ch_uint<BAUD_WIDTH>> baud_reg;
        // SPI state machine
        ch_reg<ch_uint<2>> spi_state;
        ch_reg<ch_uint<DATA_WIDTH>> shift_reg;
        constexpr static unsigned COUNTER_WIDTH = DATA_WIDTH <= 8 ? 3 : 5;
        ch_reg<ch_uint<COUNTER_WIDTH>> bit_counter;
        ch_reg<ch_uint<BAUD_WIDTH>> baud_divider;
        ch_reg<ch_bool> transfer_active;
        ch_reg<ch_bool> rx_valid;
        ch_reg<ch_bool> rx_overrun;

        TransferState()
            : busy(ch_bool(false)),
              aw_done(ch_bool(false)),
              r_valid(ch_bool(false)),
              tx_data_reg(0_d),
              rx_data_reg(0_d),
              status_reg(0_d),
              ctrl_reg(0_d),
              baud_reg(100_d),
              spi_state(ch_uint<2>(0_d)),
              shift_reg(0_d),
              bit_counter(0_d),
              baud_divider(0_d),
              transfer_active(ch_bool(false)),
              rx_valid(ch_bool(false)),
              rx_overrun(ch_bool(false)) {}
    };

    // Clock generator: baud rate divider, bit counter, SCLK generation.
    // Returns (sclk_rising, sclk_falling) so describe() can pass them to the FSM.
    template <typename TS>
    void build_clock_generator(TS &s, ch_bool is_transfer, ch_bool load_data,
                               ch_bool transfer_complete) {
        // Increment on each SPI clock cycle during TRANSFER
        auto baud_tick = (s.baud_divider == ch_uint<BAUD_WIDTH>(0_d));
        auto bit_inc = select(is_transfer, baud_tick, ch_bool(false));
        auto next_bit_counter = select(load_data,
                                        ch_uint<TS::COUNTER_WIDTH>(0_d),
                                        select(select(bit_inc, select(!transfer_complete, ch_bool(true), ch_bool(false)), ch_bool(false)),
                                               s.bit_counter + ch_uint<TS::COUNTER_WIDTH>(1_d),
                                               s.bit_counter));
        s.bit_counter->next = select(!io().rst_n, ch_uint<TS::COUNTER_WIDTH>(0_d), next_bit_counter);

        // Reset divider on START, count during TRANSFER
        auto baud_reset = load_data;
        ch_uint<BAUD_WIDTH> baud_max = s.baud_reg;
        auto baud_max_half = (baud_max >> ch_uint<BAUD_WIDTH>(1_d)) + ch_uint<BAUD_WIDTH>(1_d);
        auto baud_full_tick = (s.baud_divider == baud_max);

        ch_uint<BAUD_WIDTH> next_baud;
        next_baud = select(baud_reset, ch_uint<BAUD_WIDTH>(0_d),
                           select(is_transfer,
                                  select(baud_full_tick, ch_uint<BAUD_WIDTH>(0_d),
                                         s.baud_divider + ch_uint<BAUD_WIDTH>(1_d)),
                                  ch_uint<BAUD_WIDTH>(0_d)));
        s.baud_divider->next = select(!io().rst_n, ch_uint<BAUD_WIDTH>(0_d), next_baud);

        // Mode 0: SCLK high for first half of baud period, low for second half
        ch_bool sclk_high = s.baud_divider < baud_max_half;
        io().spi_sclk = select(is_transfer, sclk_high, ch_bool(false));

        // SCLK edges
        ch_bool sclk_rising = select(is_transfer, (s.baud_divider == ch_uint<BAUD_WIDTH>(0_d)), ch_bool(false));
        ch_bool sclk_falling = select(is_transfer, baud_full_tick, ch_bool(false));

        // Stash for use by the FSM helper
        last_sclk_rising_ = sclk_rising;
        last_sclk_falling_ = sclk_falling;
    }

    // Transfer FSM: state machine, shift register, RX capture, status/control update.
    // Receives all the pre-computed combinational signals as parameters.
    template <typename TS>
    void build_transfer_fsm(
        TS &s,
        ch_bool aw_handshake, ch_bool w_handshake, ch_bool ar_handshake,
        ch_bool we_tx_data, ch_bool we_ctrl, ch_bool we_baud,
        ch_bool ctrl_enable, ch_bool ctrl_start, ch_bool ctrl_int_en,
        ch_bool ctrl_clr_rx, ch_bool start_pulse, ch_bool clr_rx_pulse,
        ch_bool status_busy, ch_bool status_tx_empty,
        ch_bool status_rx_full, ch_bool status_rx_overrun,
        ch_bool is_idle, ch_bool is_start, ch_bool is_transfer, ch_bool is_stop,
        ch_bool capture_rx, ch_bool rx_read) {
        (void)status_tx_empty; (void)status_rx_overrun;  // used in status below
        // IDLE -> START: when START is pulsed and controller is enabled
        auto idle_to_start = select(ctrl_enable, start_pulse, ch_bool(false));

        constexpr unsigned MAX_COUNT = DATA_WIDTH - 1;
        auto transfer_complete = (s.bit_counter == ch_uint<TS::COUNTER_WIDTH>(MAX_COUNT));

        // Next state logic
        ch_uint<2> next_state;
        next_state = select(is_idle,
                            select(idle_to_start, ch_uint<2>(1_d), ch_uint<2>(0_d)),
                            select(is_start, ch_uint<2>(2_d),
                                   select(is_transfer,
                                          select(transfer_complete, ch_uint<2>(3_d), ch_uint<2>(2_d)),
                                          ch_uint<2>(0_d))));
        s.spi_state->next = select(!io().rst_n, ch_uint<2>(0_d), next_state);

        // Transfer active: set on START, clear on STOP
        s.transfer_active->next = select(!io().rst_n, ch_bool(false),
                                          select(idle_to_start, ch_bool(true),
                                                 select(is_stop, ch_bool(false), s.transfer_active)));

        ch_bool load_data = is_start;

        // MOSI: output MSB of shift register
        auto shift_msb = bits<DATA_WIDTH - 1, DATA_WIDTH - 1>(s.shift_reg);
        io().spi_mosi = select(shift_msb != ch_uint<1>(0_d), ch_bool(true), ch_bool(false));

        // MISO input - convert ch_bool to ch_uint
        auto miso_bit = select(io().spi_miso, ch_uint<DATA_WIDTH>(1_d), ch_uint<DATA_WIDTH>(0_d));

        // Shift on falling edge (after sampling on rising edge)
        auto shifted_data = (s.shift_reg << ch_uint<DATA_WIDTH>(1_d)) | miso_bit;

        // Mask wdata to DATA_WIDTH bits
        ch_uint<32> wdata_masked;
        if constexpr (DATA_WIDTH == 8) {
            wdata_masked = io().wdata & ch_uint<32>(255_d);
        } else {
            wdata_masked = io().wdata;
        }
        ch_uint<DATA_WIDTH> wdata_lower = ch_uint<DATA_WIDTH>(wdata_masked);

        // Load TX data on START, shift on SCLK falling edge during TRANSFER
        ch_uint<DATA_WIDTH> next_shift;
        next_shift = select(load_data, wdata_lower,
                            select(last_sclk_falling_, shifted_data, s.shift_reg));
        s.shift_reg->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d), next_shift);

        // RX data capture
        s.rx_data_reg->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d), select(capture_rx, s.shift_reg, s.rx_data_reg));

        // RX valid flag
        s.rx_valid->next = select(!io().rst_n, ch_bool(false),
                                  select(capture_rx, ch_bool(true),
                                         select(select(clr_rx_pulse, ch_bool(true), rx_read), ch_bool(false),
                                                s.rx_valid)));

        // RX overrun
        s.rx_overrun->next = select(!io().rst_n, ch_bool(false),
                                    select(select(capture_rx, s.rx_valid, ch_bool(false)), ch_bool(true),
                                           select(clr_rx_pulse, ch_bool(false), s.rx_overrun)));

        // Chip Select
        io().spi_cs = select(is_idle, ch_bool(true), ch_bool(false));

        // Status register
        ch_uint<32> status_val(0_d);
        status_val = select(status_busy, status_val | ch_uint<32>(1_d), status_val);
        status_val = select(status_tx_empty, status_val | ch_uint<32>(2_d), status_val);
        status_val = select(status_rx_full, status_val | ch_uint<32>(4_d), status_val);
        status_val = select(status_rx_overrun, status_val | ch_uint<32>(8_d), status_val);
        s.status_reg->next = select(!io().rst_n, ch_uint<32>(0_d), status_val);

        // TX data register
        s.tx_data_reg->next = select(!io().rst_n, ch_uint<32>(0_d), select(we_tx_data, io().wdata, s.tx_data_reg));

        // Control register with self-clearing START and CLR_RX bits
        ch_uint<32> ctrl_next = select(we_ctrl, io().wdata, s.ctrl_reg);
        ctrl_next = select(select(start_pulse, select(!we_ctrl, ch_bool(true), ch_bool(false)), ch_bool(false)),
                           ctrl_next & ch_uint<32>(0xFFFFFFFD_h), ctrl_next);
        ctrl_next = select(select(clr_rx_pulse, select(!we_ctrl, ch_bool(true), ch_bool(false)), ch_bool(false)),
                           ctrl_next & ch_uint<32>(0xFFFFFFF7_h), ctrl_next);
        s.ctrl_reg->next = select(!io().rst_n, ch_uint<32>(0_d), ctrl_next);

        // Baud rate - mask wdata to BAUD_WIDTH bits
        ch_uint<32> wdata_baud_masked;
        if constexpr (BAUD_WIDTH == 8) {
            wdata_baud_masked = io().wdata & ch_uint<32>(255_d);
        } else if constexpr (BAUD_WIDTH == 16) {
            wdata_baud_masked = io().wdata & ch_uint<32>(65535_d);
        } else {
            wdata_baud_masked = io().wdata;
        }
        ch_uint<BAUD_WIDTH> wdata_baud = ch_uint<BAUD_WIDTH>(wdata_baud_masked);
        s.baud_reg->next = select(!io().rst_n, ch_uint<BAUD_WIDTH>(100_d), select(we_baud, wdata_baud, s.baud_reg));

        // Read data mux
        ch_uint<32> read_val(0_d);
        read_val = select((io().araddr >> ch_uint<32>(2_d) & ch_uint<32>(0x0F)) == ch_uint<32>(0_d), s.tx_data_reg, read_val);
        read_val = select((io().araddr >> ch_uint<32>(2_d) & ch_uint<32>(0x0F)) == ch_uint<32>(1_d), s.rx_data_reg, read_val);
        read_val = select((io().araddr >> ch_uint<32>(2_d) & ch_uint<32>(0x0F)) == ch_uint<32>(2_d), s.status_reg, read_val);
        read_val = select((io().araddr >> ch_uint<32>(2_d) & ch_uint<32>(0x0F)) == ch_uint<32>(3_d), s.ctrl_reg, read_val);
        ch_uint<32> baud_32 = ch_uint<32>(s.baud_reg);
        read_val = select((io().araddr >> ch_uint<32>(2_d) & ch_uint<32>(0x0F)) == ch_uint<32>(4_d), baud_32, read_val);
        io().rdata = read_val;
        io().rresp = ch_uint<2>(0_d);

        // aw_done and r_valid
        s.aw_done->next = select(!io().rst_n, ch_bool(false),
                                  select(aw_handshake, ch_bool(true), select(io().bready, ch_bool(false), s.aw_done)));
        s.r_valid->next = select(!io().rst_n, ch_bool(false),
                                  select(ar_handshake, ch_bool(true), select(io().rready, ch_bool(false), s.r_valid)));
        s.busy->next = select(!io().rst_n, ch_bool(false),
                              select(aw_handshake, ch_bool(true),
                                     select(ar_handshake, ch_bool(true),
                                            select(io().bready, ch_bool(false),
                                                   select(io().rready, ch_bool(false), s.busy)))));
    }

    // Interrupt: combine transfer-complete and RX-overrun events, masked by INT_EN.
    void build_interrupt_logic(ch_bool ctrl_int_en, ch_bool capture_rx, ch_bool rx_overrun) {
        auto irq_transfer = select(select(capture_rx, ctrl_int_en, ch_bool(false)), ch_bool(true), ch_bool(false));
        auto irq_overrun_i = select(select(rx_overrun, ctrl_int_en, ch_bool(false)), ch_bool(true), ch_bool(false));
        io().irq = select(irq_transfer, ch_bool(true), select(irq_overrun_i, ch_bool(true), ch_bool(false)));
    }

    void describe() override {
        TransferState s;

        // ========================================================================
        // AXI4-Lite Address Decode
        // ========================================================================
        auto wr_addr = io().awaddr >> ch_uint<32>(2_d);
        wr_addr = wr_addr & ch_uint<32>(0x0F);
        auto rd_addr = io().araddr >> ch_uint<32>(2_d);
        rd_addr = rd_addr & ch_uint<32>(0x0F);

        // Write/read handshakes
        auto aw_handshake = select(io().awvalid, select(s.busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().awready = aw_handshake;
        auto w_handshake = select(io().wvalid, s.aw_done, ch_bool(false));
        io().wready = w_handshake;
        io().bvalid = select(s.aw_done, ch_bool(true), ch_bool(false));
        auto ar_handshake = select(io().arvalid, select(s.busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().arready = ar_handshake;
        io().rvalid = select(s.r_valid, ch_bool(true), ch_bool(false));

        // Register write enables
        auto we_tx_data = select(w_handshake, (wr_addr == ch_uint<32>(0_d)), ch_bool(false));
        auto we_ctrl = select(w_handshake, (wr_addr == ch_uint<32>(3_d)), ch_bool(false));
        auto we_baud = select(w_handshake, (wr_addr == ch_uint<32>(4_d)), ch_bool(false));

        // Control register bits
        auto ctrl_bit0 = bits<0, 0>(s.ctrl_reg);
        auto ctrl_bit1 = bits<1, 1>(s.ctrl_reg);
        auto ctrl_bit2 = bits<2, 2>(s.ctrl_reg);
        auto ctrl_bit3 = bits<3, 3>(s.ctrl_reg);

        auto ctrl_enable = select(ctrl_bit0 != ch_uint<1>(0_d), ch_bool(true), ch_bool(false));
        auto ctrl_start = select(ctrl_bit1 != ch_uint<1>(0_d), ch_bool(true), ch_bool(false));
        auto ctrl_int_en = select(ctrl_bit2 != ch_uint<1>(0_d), ch_bool(true), ch_bool(false));
        auto ctrl_clr_rx = select(ctrl_bit3 != ch_uint<1>(0_d), ch_bool(true), ch_bool(false));

        // START self-clearing
        auto start_pulse = select(ctrl_start, select(!s.transfer_active, ch_bool(true), ch_bool(false)), ch_bool(false));
        auto clr_rx_pulse = ctrl_clr_rx;

        // State decode
        auto is_idle = (s.spi_state == ch_uint<2>(0_d));
        auto is_start = (s.spi_state == ch_uint<2>(1_d));
        auto is_transfer = (s.spi_state == ch_uint<2>(2_d));
        auto is_stop = (s.spi_state == ch_uint<2>(3_d));

        // Status register bits
        ch_bool status_busy = s.transfer_active;
        ch_bool status_tx_empty = select(!s.transfer_active, is_idle, ch_bool(false));
        ch_bool status_rx_full = s.rx_valid;
        ch_bool status_rx_overrun = s.rx_overrun;

        // Capture and read signals (used by both build_transfer_fsm and
        // build_interrupt_logic)
        auto transfer_complete = (s.bit_counter == ch_uint<TransferState::COUNTER_WIDTH>(DATA_WIDTH - 1));
        ch_bool load_data = is_start;
        auto capture_rx = select(is_transfer, select(transfer_complete, last_sclk_falling_, ch_bool(false)), ch_bool(false));
        auto rx_read = select((rd_addr == ch_uint<32>(1_d)), io().arvalid, ch_bool(false));

        // ========================================================================
        // Helper invocations
        // ========================================================================
        build_clock_generator(s, is_transfer, load_data, transfer_complete);
        build_transfer_fsm(s,
                           aw_handshake, w_handshake, ar_handshake,
                           we_tx_data, we_ctrl, we_baud,
                           ctrl_enable, ctrl_start, ctrl_int_en, ctrl_clr_rx,
                           start_pulse, clr_rx_pulse,
                           status_busy, status_tx_empty, status_rx_full, status_rx_overrun,
                           is_idle, is_start, is_transfer, is_stop,
                           capture_rx, rx_read);
        build_interrupt_logic(ctrl_int_en, capture_rx, s.rx_overrun);
    }

private:
    // Phase 5: scratch state shared between describe() and the helpers
    ch_bool last_sclk_rising_{ch_bool(false)};
    ch_bool last_sclk_falling_{ch_bool(false)};
};

} // namespace axi4
