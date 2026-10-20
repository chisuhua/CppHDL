// tests/test_axi_gpio_behavior.cpp
//
// AxiLiteGpio behavior parity runtime test (interpreter + verilator).
//
// Per openspec/changes/dual-backend-axi4-peripherals-extended-2026-10 —
// Phase 3 收尾 of dual-backend-test-coverage. Existing
// `tests/test_axi_gpio.cpp` is the interface/protocol structure test
// (interpreter-only); this file tests observable register behavior and
// assertion parity across both backends.
//
// Per ADR-035: max ch_out = 32 bits (rdata), well under the 64-bit cap.
// Per dual-backend-test-coverage R3: only set_input_value / get_port_value.

#include "catch_amalgamated.hpp"
#include "axi_lite_slave_fixture.h"
#include "axi4/peripherals/axi_gpio.h"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace axi4;

// Typedef to avoid preprocessor comma-split on DUAL_BACKEND_TEST_PAIR args.
using Gpio8 = axi4::AxiLiteGpio<8>;

namespace {

// 寄存器偏移（见 axi_gpio.h 头部寄存器映射；写侧/读侧已对齐）
constexpr uint32_t kRegDataOut = 0x00;
constexpr uint32_t kRegDataIn = 0x04;   // 只读
constexpr uint32_t kRegDirection = 0x08;

// 复位脉冲 + 释放，并清空所有 VALID/BREADY + gpio_in。
inline void gpio_init(ch::Simulator &sim, Gpio8 &gpio) {
    auto &io = gpio.io();
    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.wvalid, 0);
    sim.set_input_value(io.bready, 0);
    sim.set_input_value(io.arvalid, 0);
    sim.set_input_value(io.rready, 0);
    sim.set_input_value(io.gpio_in, 0);
    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.set_input_value(io.rst_n, 1);
    sim.tick();
    sim.tick();  // settle
}

inline void gpio_write(ch::Simulator &sim, Gpio8 &gpio,
                       uint32_t addr, uint32_t data,
                       bool separate_aw_w = false) {
    ch_test::axi_lite::write_io(sim, gpio.io(), addr, data, separate_aw_w);
}

inline uint64_t gpio_read(ch::Simulator &sim, Gpio8 &gpio, uint32_t addr) {
    return ch_test::axi_lite::read_io(sim, gpio.io(), addr);
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
    Gpio8,
    "AxiLiteGpio - AXI write captures BRESP=OKAY",
    "[axi][gpio][behavior][write-bresp][dual]",
    [&]() {
        gpio_init(sim, dev.instance());

        gpio_write(sim, dev.instance(), kRegDataOut, 0xABu);

        REQUIRE(sim.get_port_value(dev.instance().io().bresp) == 0u);  // OKAY
    });

// ============================================================================
// Task 2: write-then-read DATA_OUT (latch parity)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Gpio8,
    "AxiLiteGpio - write then read DATA_OUT returns latched value",
    "[axi][gpio][behavior][data-out-latch][dual]",
    [&]() {
        gpio_init(sim, dev.instance());

        gpio_write(sim, dev.instance(), kRegDataOut, 0x5Au);

        REQUIRE(gpio_read(sim, dev.instance(), kRegDataOut) == 0x5AULL);
    });

// ============================================================================
// Task 3: peripheral-specific state — DATA_OUT drive on gpio_out
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Gpio8,
    "AxiLiteGpio - write DATA_OUT drives gpio_out",
    "[axi][gpio][behavior][gpio-out][dual]",
    [&]() {
        gpio_init(sim, dev.instance());

        gpio_write(sim, dev.instance(), kRegDataOut, 0xA5u);

        REQUIRE(sim.get_port_value(dev.instance().io().gpio_out) == 0xA5u);
    });

// ============================================================================
// Task 4: read DATA_IN reflects gpio_in (read-only register)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Gpio8,
    "AxiLiteGpio - read DATA_IN reflects gpio_in",
    "[axi][gpio][behavior][data-in][dual]",
    [&]() {
        gpio_init(sim, dev.instance());
        sim.set_input_value(dev.instance().io().gpio_in, 0x3Cu);

        REQUIRE(gpio_read(sim, dev.instance(), kRegDataIn) == 0x3CULL);
    });

// ============================================================================
// Task 5: rst_n=0 reset clears DATA_OUT / DIRECTION
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Gpio8,
    "AxiLiteGpio - rst_n=0 reset clears DATA_OUT and DIRECTION",
    "[axi][gpio][behavior][rst-n-reset][dual]",
    [&]() {
        gpio_init(sim, dev.instance());

        gpio_write(sim, dev.instance(), kRegDataOut, 0xABu);
        gpio_write(sim, dev.instance(), kRegDirection, 0x0Fu);
        REQUIRE(sim.get_port_value(dev.instance().io().gpio_out) == 0xABu);
        REQUIRE(sim.get_port_value(dev.instance().io().gpio_dir) == 0x0Fu);

        sim.set_input_value(dev.instance().io().rst_n, 0);
        sim.tick();
        sim.tick();
        sim.set_input_value(dev.instance().io().rst_n, 1);
        sim.tick();

        REQUIRE(sim.get_port_value(dev.instance().io().gpio_out) == 0u);
        REQUIRE(sim.get_port_value(dev.instance().io().gpio_dir) == 0u);
        REQUIRE(gpio_read(sim, dev.instance(), kRegDataOut) == 0ULL);
    });

// ============================================================================
// Task 6: AW→W phase separation (W accepted one cycle after AW)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Gpio8,
    "AxiLiteGpio - AW→W phase separation still latches DATA_OUT",
    "[axi][gpio][behavior][aw-w-separation][dual]",
    [&]() {
        gpio_init(sim, dev.instance());

        gpio_write(sim, dev.instance(), kRegDataOut, 0x77u,
                   /*separate_aw_w=*/true);

        REQUIRE(sim.get_port_value(dev.instance().io().gpio_out) == 0x77u);
    });

// ============================================================================
// Task 7: BVALID held until BREADY (response hold)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Gpio8,
    "AxiLiteGpio - BVALID held until BREADY",
    "[axi][gpio][behavior][bvalid-hold][dual]",
    [&]() {
        gpio_init(sim, dev.instance());

        write_expect_bvalid_hold(sim, dev.instance().io(), kRegDataOut, 0x11u);

        REQUIRE(sim.get_port_value(dev.instance().io().gpio_out) == 0x11u);
    });
