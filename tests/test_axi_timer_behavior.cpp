// tests/test_axi_timer_behavior.cpp
//
// AxiLiteTimer behavior parity runtime test (interpreter + verilator).
//
// Per openspec/changes/dual-backend-axi4-peripherals-extended-2026-10 —
// Phase 3 收尾 of dual-backend-test-coverage. Existing
// `tests/test_axi_timer.cpp` is the interface/protocol structure test
// (interpreter-only); this file tests observable register behavior and
// assertion parity across both backends.
//
// Per ADR-035: max ch_out = 32 bits (rdata), well under the 64-bit cap.
// Per dual-backend-test-coverage R3: only set_input_value / get_port_value.

#include "catch_amalgamated.hpp"
#include "axi_lite_slave_fixture.h"
#include "axi4/peripherals/axi_timer.h"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace axi4;

// Typedef to avoid preprocessor comma-split on DUAL_BACKEND_TEST_PAIR args.
using Timer32 = axi4::AxiLiteTimer<32>;

namespace {

// 寄存器偏移（见 axi_timer.h 头部寄存器映射）
constexpr uint32_t kRegLoad = 0x00;
constexpr uint32_t kRegCtrl = 0x08;
constexpr uint32_t kRegStatus = 0x0C;

// 复位脉冲 + 释放，并清空所有 VALID/BREADY。
// 未驱动的 rst_n 读出为 0 = 永久复位，peripheral 不会响应任何 AXI 事务。
inline void timer_init(ch::Simulator &sim, Timer32 &timer) {
    auto &io = timer.io();
    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.wvalid, 0);
    sim.set_input_value(io.bready, 0);
    sim.set_input_value(io.arvalid, 0);
    sim.set_input_value(io.rready, 0);
    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.set_input_value(io.rst_n, 1);
    sim.tick();
    sim.tick();  // settle
}

inline void timer_write(ch::Simulator &sim, Timer32 &timer,
                        uint32_t addr, uint32_t data,
                        bool separate_aw_w = false) {
    ch_test::axi_lite::write_io(sim, timer.io(), addr, data, separate_aw_w);
}

inline uint64_t timer_read(ch::Simulator &sim, Timer32 &timer, uint32_t addr) {
    return ch_test::axi_lite::read_io(sim, timer.io(), addr);
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

    // BVALID 必须自己出现（不被 BREADY 触发）
    REQUIRE(ch_test::axi_lite::wait_for(sim, io.bvalid));

    // master 撤掉 VALID，在 BREADY=0 期间 BVALID MUST 保持
    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.wvalid, 0);
    sim.tick();
    REQUIRE(ch_test::axi_lite::pv(sim, io.bvalid) == 1u);

    // BREADY 拉高后才允许撤
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
    Timer32,
    "AxiLiteTimer - AXI write captures BRESP=OKAY",
    "[axi][timer][behavior][write-bresp][dual]",
    [&]() {
        timer_init(sim, dev.instance());

        timer_write(sim, dev.instance(), kRegLoad, 0xDEADBEEF);

        REQUIRE(sim.get_port_value(dev.instance().io().bresp) == 0u);  // OKAY
    });

// ============================================================================
// Task 2: write-then-read LOAD (latch parity)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Timer32,
    "AxiLiteTimer - write then read LOAD returns latched value",
    "[axi][timer][behavior][load-latch][dual]",
    [&]() {
        timer_init(sim, dev.instance());

        timer_write(sim, dev.instance(), kRegLoad, 0xDEADBEEF);

        REQUIRE(timer_read(sim, dev.instance(), kRegLoad) == 0xDEADBEEFULL);
    });

// ============================================================================
// Task 3: peripheral-specific state — TIMEOUT status at initial count 0
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Timer32,
    "AxiLiteTimer - STATUS reflects TIMEOUT at initial count 0",
    "[axi][timer][behavior][status-timeout][dual]",
    [&]() {
        timer_init(sim, dev.instance());

        // count_reg 复位值 0 → timeout 成立 → status bit0=1、irq=1
        REQUIRE(timer_read(sim, dev.instance(), kRegStatus) == 0x1ULL);
        REQUIRE(sim.get_port_value(dev.instance().io().irq) == 1u);
    });

// ============================================================================
// Task 4: read CTRL returns reset default
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Timer32,
    "AxiLiteTimer - read CTRL returns default 0 after reset",
    "[axi][timer][behavior][read-ctrl-default][dual]",
    [&]() {
        timer_init(sim, dev.instance());

        REQUIRE(timer_read(sim, dev.instance(), kRegCtrl) == 0ULL);
    });

// ============================================================================
// Task 5: rst_n=0 reset clears LOAD / CTRL
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Timer32,
    "AxiLiteTimer - rst_n=0 reset clears LOAD and CTRL",
    "[axi][timer][behavior][rst-n-reset][dual]",
    [&]() {
        timer_init(sim, dev.instance());

        timer_write(sim, dev.instance(), kRegLoad, 0xABu);
        timer_write(sim, dev.instance(), kRegCtrl, 0x01u);
        REQUIRE(timer_read(sim, dev.instance(), kRegLoad) == 0xABULL);
        REQUIRE(timer_read(sim, dev.instance(), kRegCtrl) == 0x01ULL);

        sim.set_input_value(dev.instance().io().rst_n, 0);
        sim.tick();
        sim.tick();
        // 复位期间不响应 AXI 事务，先释放复位再读回
        sim.set_input_value(dev.instance().io().rst_n, 1);
        sim.tick();

        REQUIRE(timer_read(sim, dev.instance(), kRegLoad) == 0ULL);
        REQUIRE(timer_read(sim, dev.instance(), kRegCtrl) == 0ULL);
    });

// ============================================================================
// Task 6: AW→W phase separation (W accepted one cycle after AW)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Timer32,
    "AxiLiteTimer - AW→W phase separation still latches LOAD",
    "[axi][timer][behavior][aw-w-separation][dual]",
    [&]() {
        timer_init(sim, dev.instance());

        timer_write(sim, dev.instance(), kRegLoad, 0x44u,
                    /*separate_aw_w=*/true);

        REQUIRE(timer_read(sim, dev.instance(), kRegLoad) == 0x44ULL);
    });

// ============================================================================
// Task 7: BVALID held until BREADY (response hold)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Timer32,
    "AxiLiteTimer - BVALID held until BREADY",
    "[axi][timer][behavior][bvalid-hold][dual]",
    [&]() {
        timer_init(sim, dev.instance());

        write_expect_bvalid_hold(sim, dev.instance().io(), kRegLoad, 0x66u);

        // 响应握手完成后写已落地
        REQUIRE(timer_read(sim, dev.instance(), kRegLoad) == 0x66ULL);
    });
