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

using namespace ch::core;

namespace axi4 {

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
    )
    
    AxiLiteSpi(ch::Component* parent = nullptr, const std::string& name = "axi_spi")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // AXI4-Lite State Machine
        // ========================================================================
        
        ch_reg<ch_bool> axi_busy(ch_bool(false));
        
        // ========================================================================
        // Register Storage
        // ========================================================================
        
        ch_reg<ch_uint<32>> tx_data_reg(0_d);
        ch_reg<ch_uint<32>> rx_data_reg(0_d);
        ch_reg<ch_uint<32>> status_reg(0_d);
        ch_reg<ch_uint<32>> ctrl_reg(0_d);
        ch_reg<ch_uint<BAUD_WIDTH>> baud_reg(100_d);  // Default divider
        
        // ========================================================================
        // SPI State Machine (2 bits: 0=IDLE, 1=START, 2=TRANSFER, 3=STOP)
        // ========================================================================
        
        ch_reg<ch_uint<2>> spi_state(ch_uint<2>(0_d));
        
        // State decode
        auto is_idle = (spi_state == ch_uint<2>(0_d));
        auto is_start = (spi_state == ch_uint<2>(1_d));
        auto is_transfer = (spi_state == ch_uint<2>(2_d));
        auto is_stop = (spi_state == ch_uint<2>(3_d));
        
        // ========================================================================
        // SPI Internal State
        // ========================================================================
        
        // Shift register
        ch_reg<ch_uint<DATA_WIDTH>> shift_reg(0_d);
        
        // Bit counter (0 to DATA_WIDTH-1)
        constexpr unsigned COUNTER_WIDTH = DATA_WIDTH <= 8 ? 3 : 5;
        ch_reg<ch_uint<COUNTER_WIDTH>> bit_counter(0_d);
        
        // Baud rate divider
        ch_reg<ch_uint<BAUD_WIDTH>> baud_divider(0_d);
        
        // Transfer active flag
        ch_reg<ch_bool> transfer_active(ch_bool(false));
        
        // RX data valid flag
        ch_reg<ch_bool> rx_valid(ch_bool(false));
        
        // RX overrun flag
        ch_reg<ch_bool> rx_overrun(ch_bool(false));
        
        // ========================================================================
        // Register Write Enable Signals
        // ========================================================================
        
        // Address decoding (2-bit, 4-byte aligned)
        auto wr_addr = io().awaddr >> ch_uint<32>(2_d);
        wr_addr = wr_addr & ch_uint<32>(static_cast<uint32_t>(0x0F));  // Support up to 16 registers
        
        auto rd_addr = io().araddr >> ch_uint<32>(2_d);
        rd_addr = rd_addr & ch_uint<32>(static_cast<uint32_t>(0x0F));
        
        // Write handshake using select instead of &&
        auto aw_handshake = select(io().awvalid, select(axi_busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().awready = aw_handshake;
        
        auto w_handshake = select(io().wvalid, aw_handshake, ch_bool(false));
        io().wready = w_handshake;
        
        // Register write enables
        auto we_tx_data = select(w_handshake, (wr_addr == ch_uint<32>(0_d)), ch_bool(false));
        auto we_ctrl = select(w_handshake, (wr_addr == ch_uint<32>(3_d)), ch_bool(false));
        auto we_baud = select(w_handshake, (wr_addr == ch_uint<32>(4_d)), ch_bool(false));
        
        // ========================================================================
        // Control Register Logic
        // ========================================================================
        
        // Extract control bits
        auto ctrl_enable = select((ctrl_reg & ch_uint<32>(1_d)) != ch_uint<32>(0_d), ch_bool(true), ch_bool(false));
        auto ctrl_start = select((ctrl_reg & ch_uint<32>(2_d)) != ch_uint<32>(0_d), ch_bool(true), ch_bool(false));
        auto ctrl_int_en = select((ctrl_reg & ch_uint<32>(4_d)) != ch_uint<32>(0_d), ch_bool(true), ch_bool(false));
        auto ctrl_clr_rx = select((ctrl_reg & ch_uint<32>(8_d)) != ch_uint<32>(0_d), ch_bool(true), ch_bool(false));
        
        // START bit is self-clearing
        auto start_pulse = select(ctrl_start, select(!transfer_active, ch_bool(true), ch_bool(false)), ch_bool(false));
        
        // Clear RX buffer
        auto clr_rx_pulse = ctrl_clr_rx;
        
        // ========================================================================
        // Status Register Bits
        // ========================================================================
        
        // bit0: BUSY
        ch_bool status_busy = transfer_active;
        
        // bit1: TX_EMPTY (ready for new data)
        ch_bool status_tx_empty = select(!transfer_active, is_idle, ch_bool(false));
        
        // bit2: RX_FULL (data available)
        ch_bool status_rx_full = rx_valid;
        
        // bit3: RX_OVERRUN (new data received before old data read)
        ch_bool status_rx_overrun = rx_overrun;
        
        // ========================================================================
        // SPI State Machine Transitions
        // ========================================================================
        
        // IDLE -> START: when START is pulsed and controller is enabled
        auto idle_to_start = select(ctrl_enable, start_pulse, ch_bool(false));
        
        // START -> TRANSFER: after 1 cycle (load data)
        ch_bool start_to_transfer = ch_bool(true);
        
        // TRANSFER -> STOP: when all bits transferred
        constexpr unsigned MAX_COUNT = DATA_WIDTH - 1;
        auto transfer_complete = (bit_counter == ch_uint<COUNTER_WIDTH>(MAX_COUNT));
        ch_bool transfer_to_stop = transfer_complete;
        
        // STOP -> IDLE: after 1 cycle
        ch_bool stop_to_idle = ch_bool(true);
        
        // Next state logic
        ch_uint<2> next_state;
        next_state = select(is_idle,
                            select(idle_to_start, ch_uint<2>(1_d), ch_uint<2>(0_d)),
                            select(is_start, ch_uint<2>(2_d),
                                   select(is_transfer,
                                          select(transfer_complete, ch_uint<2>(3_d), ch_uint<2>(2_d)),
                                          ch_uint<2>(0_d))));
        spi_state->next = next_state;
        
        // ========================================================================
        // Transfer Control
        // ========================================================================
        
        // Transfer active: set on START, clear on STOP
        transfer_active->next = select(idle_to_start, ch_bool(true),
                                       select(is_stop, ch_bool(false), transfer_active));
        
        // Load data on START
        ch_bool load_data = is_start;
        
        // ========================================================================
        // Bit Counter
        // ========================================================================
        
        // Increment on each SPI clock cycle during TRANSFER
        auto baud_tick = (baud_divider == ch_uint<BAUD_WIDTH>(0_d));
        auto bit_inc = select(is_transfer, baud_tick, ch_bool(false));
        auto next_bit_counter = select(load_data,
                                        ch_uint<COUNTER_WIDTH>(0_d),
                                        select(select(bit_inc, select(!transfer_complete, ch_bool(true), ch_bool(false)), ch_bool(false)),
                                               bit_counter + ch_uint<COUNTER_WIDTH>(1_d),
                                               bit_counter));
        bit_counter->next = next_bit_counter;
        
        // ========================================================================
        // Baud Rate Divider
        // ========================================================================
        
        // Reset divider on START, count during TRANSFER
        auto baud_reset = load_data;
        ch_uint<BAUD_WIDTH> baud_max = baud_reg;
        auto baud_max_half = (baud_max >> ch_uint<BAUD_WIDTH>(1_d)) + ch_uint<BAUD_WIDTH>(1_d);
        auto baud_full_tick = (baud_divider == baud_max);
        
        ch_uint<BAUD_WIDTH> next_baud;
        next_baud = select(baud_reset, ch_uint<BAUD_WIDTH>(0_d),
                           select(is_transfer,
                                  select(baud_full_tick, ch_uint<BAUD_WIDTH>(0_d),
                                         baud_divider + ch_uint<BAUD_WIDTH>(1_d)),
                                  ch_uint<BAUD_WIDTH>(0_d)));
        baud_divider->next = next_baud;
        
        // ========================================================================
        // SPI Clock Generation (Mode 0: CPOL=0, CPHA=0)
        // ========================================================================
        
        // Mode 0: Clock idle low, sample on rising edge, shift on falling edge
        // SCLK high for first half of baud period, low for second half
        ch_bool sclk_high = baud_divider < baud_max_half;
        io().spi_sclk = select(is_transfer, sclk_high, ch_bool(false));
        
        // SCLK edges
        ch_bool sclk_rising = select(is_transfer, (baud_divider == ch_uint<BAUD_WIDTH>(0_d)), ch_bool(false));
        ch_bool sclk_falling = select(is_transfer, baud_full_tick, ch_bool(false));
        
        // ========================================================================
        // Shift Register
        // ========================================================================
        
        // MOSI: output MSB of shift register
        auto shift_msb = (shift_reg >> ch_uint<DATA_WIDTH>(DATA_WIDTH - 1)) & ch_uint<DATA_WIDTH>(1_d);
        io().spi_mosi = select((shift_msb != ch_uint<DATA_WIDTH>(0_d)), ch_bool(true), ch_bool(false));
        
        // MISO input - convert ch_bool to ch_uint
        auto miso_bit = select(io().spi_miso, ch_uint<DATA_WIDTH>(1_d), ch_uint<DATA_WIDTH>(0_d));
        
        // Shift on falling edge (after sampling on rising edge)
        auto shifted_data = (shift_reg << ch_uint<DATA_WIDTH>(1_d)) | miso_bit;
        
        // Get lower DATA_WIDTH bits from wdata for loading
        // Use bit masking to extract lower bits
        ch_uint<32> wdata_mask = ch_uint<32>((1ULL << DATA_WIDTH) - 1);
        ch_uint<32> wdata_masked = io().wdata & wdata_mask;
        ch_uint<DATA_WIDTH> wdata_lower = ch_uint<DATA_WIDTH>(wdata_masked);
        
        // Load TX data on START, shift on SCLK falling edge during TRANSFER
        ch_uint<DATA_WIDTH> next_shift;
        next_shift = select(load_data, wdata_lower,
                            select(sclk_falling, shifted_data, shift_reg));
        shift_reg->next = next_shift;
        
        // ========================================================================
        // RX Data Capture
        // ========================================================================
        
        // Capture shift register on last bit
        auto capture_rx = select(is_transfer, select(transfer_complete, sclk_falling, ch_bool(false)), ch_bool(false));
        rx_data_reg->next = select(capture_rx, shift_reg, rx_data_reg);
        
        // RX valid flag - set on capture, cleared on read or CLR_RX
        auto rx_read = select((rd_addr == ch_uint<32>(1_d)), io().arvalid, ch_bool(false));
        rx_valid->next = select(capture_rx, ch_bool(true),
                                select(select(clr_rx_pulse, ch_bool(true), rx_read), ch_bool(false),
                                       rx_valid));
        
        // RX overrun: new data received while RX_FULL is set
        rx_overrun->next = select(select(capture_rx, rx_valid, ch_bool(false)), ch_bool(true),
                                  select(clr_rx_pulse, ch_bool(false), rx_overrun));
        
        // ========================================================================
        // Chip Select (active low)
        // ========================================================================
        
        // CS low during START, TRANSFER, STOP
        io().spi_cs = select(is_idle, ch_bool(true), ch_bool(false));
        
        // ========================================================================
        // Interrupt
        // ========================================================================
        
        // Interrupt on transfer complete or RX overrun
        auto irq_transfer = select(select(capture_rx, ctrl_int_en, ch_bool(false)), ch_bool(true), ch_bool(false));
        auto irq_overrun = select(select(rx_overrun, ctrl_int_en, ch_bool(false)), ch_bool(true), ch_bool(false));
        io().irq = select(irq_transfer, ch_bool(true), select(irq_overrun, ch_bool(true), ch_bool(false)));
        
        // ========================================================================
        // Status Register Update
        // ========================================================================
        
        ch_uint<32> status_val(0_d);
        status_val = select(status_busy, status_val | ch_uint<32>(1_d), status_val);
        status_val = select(status_tx_empty, status_val | ch_uint<32>(2_d), status_val);
        status_val = select(status_rx_full, status_val | ch_uint<32>(4_d), status_val);
        status_val = select(status_rx_overrun, status_val | ch_uint<32>(8_d), status_val);
        status_reg->next = status_val;
        
        // ========================================================================
        // Register Updates
        // ========================================================================
        
        // TX data: written via AXI, loaded into shift register on START
        tx_data_reg->next = select(we_tx_data, io().wdata, tx_data_reg);
        
        // Control: written via AXI, START bit self-clears
        ch_uint<32> ctrl_next = select(we_ctrl, io().wdata, ctrl_reg);
        // Self-clear START bit when transfer starts
        ctrl_next = select(select(start_pulse, select(!we_ctrl, ch_bool(true), ch_bool(false)), ch_bool(false)), 
                           ctrl_next & ~ch_uint<32>(2_d), ctrl_next);
        // Self-clear CLR_RX bit
        ctrl_next = select(select(clr_rx_pulse, select(!we_ctrl, ch_bool(true), ch_bool(false)), ch_bool(false)),
                           ctrl_next & ~ch_uint<32>(8_d), ctrl_next);
        ctrl_reg->next = ctrl_next;
        
        // Baud rate - extract lower BAUD_WIDTH bits
        // Use bit masking to extract lower bits
        ch_uint<32> baud_mask = ch_uint<32>((1ULL << BAUD_WIDTH) - 1);
        ch_uint<32> wdata_baud_masked = io().wdata & baud_mask;
        ch_uint<BAUD_WIDTH> wdata_baud = ch_uint<BAUD_WIDTH>(wdata_baud_masked);
        baud_reg->next = select(we_baud, wdata_baud, baud_reg);
        
        // ========================================================================
        // AXI4-Lite Read
        // ========================================================================
        
        // Read handshake using select instead of &&
        auto ar_handshake = select(io().arvalid, select(axi_busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().arready = ar_handshake;
        
        // Read data multiplexer
        ch_uint<32> read_val(0_d);
        read_val = select((rd_addr == ch_uint<32>(0_d)), tx_data_reg, read_val);
        read_val = select((rd_addr == ch_uint<32>(1_d)), rx_data_reg, read_val);
        read_val = select((rd_addr == ch_uint<32>(2_d)), status_reg, read_val);
        read_val = select((rd_addr == ch_uint<32>(3_d)), ctrl_reg, read_val);
        
        // Convert baud_reg to 32-bit for reading
        ch_uint<32> baud_32 = ch_uint<32>(baud_reg);
        read_val = select((rd_addr == ch_uint<32>(4_d)), baud_32, read_val);
        
        io().rdata = read_val;
        io().rvalid = ar_handshake;
        io().rresp = ch_uint<2>(0_d);  // OKAY
        
        // ========================================================================
        // AXI4-Lite Write Response
        // ========================================================================
        
        io().bvalid = w_handshake;
        io().bresp = ch_uint<2>(0_d);  // OKAY
        
        // ========================================================================
        // AXI State Machine
        // ========================================================================
        
        auto any_handshake = select(aw_handshake, ch_bool(true), ar_handshake);
        auto any_ready = select(io().bready, ch_bool(true), select(io().rready, ch_bool(true), ch_bool(false)));
        axi_busy->next = select(any_handshake, ch_bool(true),
                                select(any_ready, ch_bool(false), axi_busy));
    }
};

} // namespace axi4
