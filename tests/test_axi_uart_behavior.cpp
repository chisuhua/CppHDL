// tests/test_axi_uart_behavior.cpp
//
// AxiLiteUart behavior parity runtime test (interpreter + verilator).
//
// Per openspec/changes/dual-backend-axi4-peripherals-extended-2026-10 —
// Phase 3 收尾 of dual-backend-test-coverage. Existing
// `tests/test_axi_uart.cpp` is the interface/protocol structure test
// (interpreter-only); this file tests observable register behavior and
// assertion parity across both backends.
//
// 注意：本 peripheral **无 TX 移位 / RX 接收引擎**（归 change
// `axi4-peripheral-deferred-2026-10` 的 D2）。因此本文件只断言寄存器级语义
// （tx_empty / status_reg bit0），不断言 uart_tx 串行波形或 rx_valid 行为。
//
// Per ADR-035: max ch_out = 32 bits (rdata), well under the 64-bit cap.
// Per dual-backend-test-coverage R3: only set_input_value / get_port_value.

#include "catch_amalgamated.hpp"
#include "axi_lite_slave_fixture.h"
#include "axi4/peripherals/axi_uart.h"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace axi4;

// Typedef to avoid preprocessor comma-split on DUAL_BACKEND_TEST_PAIR args.
using Uart16 = axi4::AxiLiteUart<16>;

namespace {

// 寄存器偏移（见 axi_uart.h 头部寄存器映射）
constexpr uint32_t kRegTxData = 0x00;
constexpr uint32_t kRegStatus = 0x08;
constexpr uint32_t kRegCtrl = 0x0C;

// 复位脉冲 + 释放，并清空所有 VALID/BREADY + uart_rx。
inline void uart_init(ch::Simulator &sim, Uart16 &uart) {
    auto &io = uart.io();
    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.wvalid, 0);
    sim.set_input_value(io.bready, 0);
    sim.set_input_value(io.arvalid, 0);
    sim.set_input_value(io.rready, 0);
    sim.set_input_value(io.uart_rx, 0);
    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.set_input_value(io.rst_n, 1);
    sim.tick();
    sim.tick();  // settle
}

inline void uart_write(ch::Simulator &sim, Uart16 &uart,
                       uint32_t addr, uint32_t data,
                       bool separate_aw_w = false) {
    ch_test::axi_lite::write_io(sim, uart.io(), addr, data, separate_aw_w);
}

inline uint64_t uart_read(ch::Simulator &sim, Uart16 &uart, uint32_t addr) {
    return ch_test::axi_lite::read_io(sim, uart.io(), addr);
}

// 写事务 + BVALID 必须在 BREADY 拉高前保持。
template <typename Io>
inline void write_expect_bvalid_hold(ch::Simulator &sim, Io &io,
                                     uint32_t addr, uint32_t data) {
    sim.set_input_value(io.awaddr, addr);
    sim.set_input_value(io.awprot, 0);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, data);
    sim.set_input_value(io.wstrb, 0xF);
    sim.set_input_value(io.wvalid, 1);

    REQUIRE(ch_test::axi_lite::wait_for(sim, io.bvalid));

    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.wvalid, 0);
    sim.tick();
    REQUIRE(ch_test::axi_lite::pv(sim, io.bvalid) == 1u);

    sim.set_input_value(io.bready, 1);
    sim.tick();
    sim.set_input_value(io.bready, 0);
    REQUIRE(ch_test::axi_lite::pv(sim, io.bvalid) == 0u);
}

} // namespace

// ============================================================================
// Task 1: AXI write BRESP=OKAY
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - AXI write captures BRESP=OKAY",
    "[axi][uart][behavior][write-bresp][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        uart_write(sim, dev.instance(), kRegTxData, 0x41u);

        REQUIRE(sim.get_port_value(dev.instance().io().bresp) == 0u);  // OKAY
    });

// ============================================================================
// Task 2: write-then-read TX_DATA (latch parity)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - write then read TX_DATA returns latched value",
    "[axi][uart][behavior][tx-data-latch][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        uart_write(sim, dev.instance(), kRegTxData, 0x55u);

        REQUIRE(uart_read(sim, dev.instance(), kRegTxData) == 0x55ULL);
    });

// ============================================================================
// Task 3: peripheral-specific state — STATUS bit0 TX_EMPTY, bit1 RX_VALID
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - STATUS reflects TX_EMPTY only (no RX engine)",
    "[axi][uart][behavior][status-bits][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        // 无 RX 引擎 → rx_valid 恒 0、irq 恒 0（设计结果，见 header 注释）
        REQUIRE(uart_read(sim, dev.instance(), kRegStatus) == 0x1ULL);
        REQUIRE(sim.get_port_value(dev.instance().io().irq) == 0u);
    });

// ============================================================================
// Task 4: read CTRL returns reset default
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - read CTRL returns default 0 after reset",
    "[axi][uart][behavior][read-ctrl-default][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        REQUIRE(uart_read(sim, dev.instance(), kRegCtrl) == 0ULL);
    });

// ============================================================================
// Task 5: rst_n=0 reset clears TX_DATA / CTRL and restores TX_EMPTY
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - rst_n=0 reset clears TX_DATA and CTRL",
    "[axi][uart][behavior][rst-n-reset][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        uart_write(sim, dev.instance(), kRegTxData, 0xABu);
        uart_write(sim, dev.instance(), kRegCtrl, 0x03u);
        REQUIRE(uart_read(sim, dev.instance(), kRegTxData) == 0xABULL);
        REQUIRE(uart_read(sim, dev.instance(), kRegCtrl) == 0x03ULL);

        sim.set_input_value(dev.instance().io().rst_n, 0);
        sim.tick();
        sim.tick();
        sim.set_input_value(dev.instance().io().rst_n, 1);
        sim.tick();

        REQUIRE(uart_read(sim, dev.instance(), kRegTxData) == 0ULL);
        REQUIRE(uart_read(sim, dev.instance(), kRegCtrl) == 0ULL);
        // tx_empty 复位值 true → STATUS bit0 恢复
        REQUIRE(uart_read(sim, dev.instance(), kRegStatus) == 0x1ULL);
    });

// ============================================================================
// Task 6: AW→W phase separation (W accepted one cycle after AW)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - AW→W phase separation still latches TX_DATA",
    "[axi][uart][behavior][aw-w-separation][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        uart_write(sim, dev.instance(), kRegTxData, 0x7Eu,
                   /*separate_aw_w=*/true);

        REQUIRE(uart_read(sim, dev.instance(), kRegTxData) == 0x7EULL);
    });

// ============================================================================
// Task 7: BVALID held until BREADY (response hold)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - BVALID held until BREADY",
    "[axi][uart][behavior][bvalid-hold][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        write_expect_bvalid_hold(sim, dev.instance().io(), kRegTxData, 0x33u);

        REQUIRE(uart_read(sim, dev.instance(), kRegTxData) == 0x33ULL);
    });
