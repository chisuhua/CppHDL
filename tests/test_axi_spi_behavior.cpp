// tests/test_axi_spi_behavior.cpp
//
// AxiLiteSpi behavior parity runtime test (interpreter + verilator).
//
// Per openspec/changes/axi-spi-behavior-tests/ — Phase 3 partial of
// dual-backend-test-coverage. Existing `tests/test_axi_spi.cpp` is
// interface-structure test (port.impl() != nullptr); this file tests
// actual AXI4-Lite handshake + SPI state machine behavior.
//
// Per ADR-035: max ch_out = 32 bits (rdata), well under 64-bit cap.
// Per dual-backend-test-coverage R3: only set_input_value / get_port_value.

#include "catch_amalgamated.hpp"
#include "axi_lite_slave_fixture.h"
#include "axi4/peripherals/axi_spi.h"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace axi4;

// Typedefs to avoid preprocessor comma-split on DUAL_BACKEND_TEST_PAIR
// macro args. Macro is positional (4 args); `AxiLiteSpi<8, 16>` has a
// comma in template args, which the C preprocessor treats as a macro
// arg separator. Aliasing collapses the comma.
using Spi8x16  = axi4::AxiLiteSpi<8, 16>;
using Spi32x16 = axi4::AxiLiteSpi<32, 16>;

// ============================================================================
// Helpers — AXI4-Lite write / read helpers
// ============================================================================

// Issue a single AXI4-Lite write transaction, wait for bvalid=1.
// 复用共享的**协议合规** master (tests/axi_lite_slave_fixture.h)。
//
// 旧写法在 phase 1 结束时**同时**撤掉 awvalid 与 wvalid —— 在规范形态下
// wvalid 会在 W 握手（AW 握手后一拍）之前就掉下去 → 写数据永不被接受 →
// bvalid 永不拉高。规范形态要求 WVALID 保持到 W 握手完成。
template <typename SpiT>
static inline void axi_write_and_wait_bvalid(
    ch::Simulator &sim,
    SpiT &spi,
    uint32_t addr, uint32_t data) {
    ch_test::axi_lite::write_io(sim, spi.io(), addr, data);
}

// Issue a single AXI4-Lite read transaction, wait for rvalid=1.
// 复用共享的协议合规 master：规范形态下 R 响应在 AR 握手**之后**才拉高，
// 必须等 RVALID 再继续。
template <typename SpiT>
static inline void axi_read_and_wait_rvalid(
    ch::Simulator &sim,
    SpiT &spi,
    uint32_t addr) {
    ch_test::axi_lite::read_io(sim, spi.io(), addr);
}

// ============================================================================
// Task 1: AXI write → BRESP=OKAY (probe handshake timing)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Spi8x16,
    "AxiLiteSpi - AXI write TX_DATA captures BRESP=OKAY",
    "[axi][spi][behavior][write-bresp][dual]",
    [&]() {
        // Initial state: de-assert all valid signals
        sim.set_input_value(dev.instance().io().awvalid, 0);
        sim.set_input_value(dev.instance().io().wvalid,  0);
        sim.set_input_value(dev.instance().io().bready,  0);
        sim.set_input_value(dev.instance().io().arvalid, 0);
        sim.set_input_value(dev.instance().io().rready,  0);
        sim.set_input_value(dev.instance().io().spi_miso, 0);
        sim.set_input_value(dev.instance().io().rst_n,   1);
        sim.tick();

        // Write TX_DATA register (0x00) with 0xAB
        axi_write_and_wait_bvalid(sim, dev.instance(), 0x00, 0xAB);

        // BRESP must be OKAY (0)
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().bresp)) == 0u);
        // B 握手已完成 → BVALID 合法地落下。
        // （"BVALID 曾被 master 观察到"由共享 fixture 内部 REQUIRE 断言）
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().bvalid)) == 0u);
    });

// ============================================================================
// Task 2: 4 个 AXI-Lite 行为测试
// ============================================================================

// 2.1: Write TX_DATA captures bvalid within handshake window (parity-safe).
//      We don't assert exact rdata value (timing-sensitive to register
//      propagation across 1-cycle handshake), only assert bvalid toggled
//      to 1 at some cycle, which both backends must agree on (parity).
DUAL_BACKEND_TEST_PAIR(
    Spi8x16,
    "AxiLiteSpi - AXI write TX_DATA triggers bvalid (parity)",
    "[axi][spi][behavior][tx-data][parity][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().awvalid, 0);
        sim.set_input_value(dev.instance().io().wvalid,  0);
        sim.set_input_value(dev.instance().io().bready,  1);
        sim.set_input_value(dev.instance().io().arvalid, 0);
        sim.set_input_value(dev.instance().io().rready,  1);
        sim.set_input_value(dev.instance().io().spi_miso, 0);
        sim.set_input_value(dev.instance().io().rst_n,   1);
        sim.tick();

        // AXI write 0x00 = 0x55
        axi_write_and_wait_bvalid(sim, dev.instance(), 0x00, 0x55);

        // B 握手完成后 BVALID 落下（BVALID 曾被观察到的断言在共享 fixture 内）
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().bvalid)) == 0u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().bresp))  == 0u);
    });

// 2.2: Read STATUS in idle — busy=0, tx_empty=1, rx_full=0, rx_overrun=0
DUAL_BACKEND_TEST_PAIR(
    Spi8x16,
    "AxiLiteSpi - Read STATUS in idle: busy=0, tx_empty=1",
    "[axi][spi][behavior][status-idle][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().awvalid, 0);
        sim.set_input_value(dev.instance().io().wvalid,  0);
        sim.set_input_value(dev.instance().io().bready,  0);
        sim.set_input_value(dev.instance().io().arvalid, 0);
        sim.set_input_value(dev.instance().io().rready,  1);
        sim.set_input_value(dev.instance().io().spi_miso, 0);
        sim.set_input_value(dev.instance().io().rst_n,   1);
        // Tick to let status_reg settle from initial 0 to computed value (idle → 2)
        for (int i = 0; i < 4; ++i) {
            sim.tick();
        }

        axi_read_and_wait_rvalid(sim, dev.instance(), 0x08);

        auto status = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rdata));
        // STATUS bits (axi_spi.h:275-278): 0=busy 1=tx_empty 2=rx_full 3=rx_overrun
        REQUIRE((status & 0x1) == 0u);     // busy = 0
        REQUIRE((status & 0x2) == 0x2u);   // tx_empty = 1 (per axi_spi.h:276)
        REQUIRE((status & 0x4) == 0u);     // rx_full = 0
    });

// 2.3: Transfer inactive keeps MOSI low + CS high (active low)
DUAL_BACKEND_TEST_PAIR(
    Spi8x16,
    "AxiLiteSpi - Idle state MOSI low + CS high (active low)",
    "[axi][spi][behavior][idle-mosi-cs][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().awvalid, 0);
        sim.set_input_value(dev.instance().io().wvalid,  0);
        sim.set_input_value(dev.instance().io().bready,  0);
        sim.set_input_value(dev.instance().io().arvalid, 0);
        sim.set_input_value(dev.instance().io().rready,  0);
        sim.set_input_value(dev.instance().io().spi_miso, 0);
        sim.set_input_value(dev.instance().io().rst_n,   1);
        sim.tick();

        // Idle: MOSI = 0, CS = 1 (active low, idle high)
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().spi_mosi)) == 0u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().spi_cs))   == 1u);
    });

// 2.4: AXI write captures BRESP=OKAY for CTRL register (0x0C)
DUAL_BACKEND_TEST_PAIR(
    Spi8x16,
    "AxiLiteSpi - AXI write CTRL captures BRESP=OKAY",
    "[axi][spi][behavior][ctrl-write][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().awvalid, 0);
        sim.set_input_value(dev.instance().io().wvalid,  0);
        sim.set_input_value(dev.instance().io().bready,  0);
        sim.set_input_value(dev.instance().io().arvalid, 0);
        sim.set_input_value(dev.instance().io().rready,  0);
        sim.set_input_value(dev.instance().io().spi_miso, 0);
        sim.set_input_value(dev.instance().io().rst_n,   1);
        sim.tick();

        // Write CTRL (0x0C) with 0x01 (start transfer)
        axi_write_and_wait_bvalid(sim, dev.instance(), 0x0C, 0x01);

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().bresp)) == 0u);
    });

// ============================================================================
// Task 3: 32-bit variant
// ============================================================================

// 3.1: 32-bit variant write TX_DATA captures BRESP=OKAY
DUAL_BACKEND_TEST_PAIR(
    Spi32x16,
    "AxiLiteSpi<32,16> - AXI write TX_DATA captures BRESP=OKAY",
    "[axi][spi][behavior][write-bresp][32-bit][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().awvalid, 0);
        sim.set_input_value(dev.instance().io().wvalid,  0);
        sim.set_input_value(dev.instance().io().bready,  0);
        sim.set_input_value(dev.instance().io().arvalid, 0);
        sim.set_input_value(dev.instance().io().rready,  0);
        sim.set_input_value(dev.instance().io().spi_miso, 0);
        sim.set_input_value(dev.instance().io().rst_n,   1);
        sim.tick();

        axi_write_and_wait_bvalid(sim, dev.instance(), 0x00, 0xDEADBEEF);

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().bresp)) == 0u);
        // B 握手已完成 → BVALID 合法地落下
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().bvalid)) == 0u);
    });

// ============================================================================
// Task 4: Reset behavior
// ============================================================================

// 4.1: rst_n=0 clears busy + forces SPI idle
DUAL_BACKEND_TEST_PAIR(
    Spi8x16,
    "AxiLiteSpi - Reset clears busy + SPI idle",
    "[axi][spi][behavior][reset][dual]",
    [&]() {
        // Initial state: rst_n=1, idle
        sim.set_input_value(dev.instance().io().awvalid, 0);
        sim.set_input_value(dev.instance().io().wvalid,  0);
        sim.set_input_value(dev.instance().io().bready,  0);
        sim.set_input_value(dev.instance().io().arvalid, 0);
        sim.set_input_value(dev.instance().io().rready,  0);
        sim.set_input_value(dev.instance().io().spi_miso, 0);
        sim.set_input_value(dev.instance().io().rst_n,   1);
        sim.tick();

        // Trigger a transfer (write TX_DATA)
        axi_write_and_wait_bvalid(sim, dev.instance(), 0x00, 0xFF);
        sim.tick();

        // Assert reset (rst_n=0)
        sim.set_input_value(dev.instance().io().rst_n, 0);
        sim.tick();
        sim.tick();

        // 复位**期间** peripheral 处于复位态（r_valid 被清零 → 不响应 AXI 事务），
        // 且读地址锁存也被复位清零 → 此时采样 RDATA 没有意义。先释放复位再读。
        sim.set_input_value(dev.instance().io().rst_n, 1);
        sim.tick();

        // Read STATUS — busy should be cleared
        sim.set_input_value(dev.instance().io().rready, 1);
        axi_read_and_wait_rvalid(sim, dev.instance(), 0x08);

        auto status = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rdata));
        REQUIRE((status & 0x1) == 0u);  // busy cleared
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().spi_cs))   == 1u);  // CS high
    });