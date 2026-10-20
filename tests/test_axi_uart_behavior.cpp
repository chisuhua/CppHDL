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
// 串行引擎：自 change `axi4-peripheral-deferred-2026-10` D2 起，本 peripheral
// 具备**真实 TX 移位 / RX 接收引擎**（bit 周期 = 16 周期，8N1）——
// 因此本文件同时断言 `uart_tx` 串行波形、`rx_valid` / `rx_data_reg` 行为。
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

#include <cstdint>
#include <vector>

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
    sim.set_input_value(io.uart_rx, 1);  // UART 空闲电平为高（低电平=起始位）
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

// ---------------------------------------------------------------------------
// 串行引擎辅助
// ---------------------------------------------------------------------------

// bit 周期（与 AxiLiteUart<16> 的 UART_BIT_PERIOD 一致）
constexpr unsigned kBitPeriod = 16;

// 写 TX_DATA 并**逐拍采集** uart_tx 波形（覆盖写事务全程 + 整帧）
inline void uart_write_capture_tx(ch::Simulator &sim, Uart16 &uart, uint8_t byte,
                                  std::vector<bool> &wave) {
    auto &io = uart.io();
    const auto sample = [&]() {
        wave.push_back(ch_test::axi_lite::pv(sim, io.uart_tx) != 0);
    };

    sim.set_input_value(io.awaddr, kRegTxData);
    sim.set_input_value(io.awprot, 0);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, byte);
    sim.set_input_value(io.wstrb, 0xF);
    sim.set_input_value(io.wvalid, 1);

    // AW 与 W 同周期发出（AXI4-Lite 允许），W 在下一拍被接受
    for (int i = 0; i < 16; ++i) {
        sim.tick();
        sample();
        if (ch_test::axi_lite::pv(sim, io.bvalid) == 1u) {
            break;
        }
    }
    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.wvalid, 0);
    sim.set_input_value(io.bready, 1);
    sim.tick();
    sample();
    sim.set_input_value(io.bready, 0);

    // 采集整帧（10 bit × 16 周期 = 160 周期）
    for (int i = 0; i < 220; ++i) {
        sim.tick();
        sample();
    }
}

// 从波形中解码一帧 8N1：返回 {起始位, 数据字节, 停止位}
// 找不到起始位时 data = 0xFF 作为哨兵
struct RxFrame {
    bool start_bit = false;
    uint8_t data = 0xFF;
    bool stop_bit = false;
};

inline RxFrame decode_uart_frame(const std::vector<bool> &wave) {
    RxFrame out;
    // 找起始位（1 → 0 的下降沿）
    std::size_t start = wave.size();
    for (std::size_t i = 1; i < wave.size(); ++i) {
        if (wave[i - 1] && !wave[i]) {
            start = i;
            break;
        }
    }
    if (start == wave.size()) {
        return out;
    }
    // 各位中点采样：起始位中点 = start + 8，第 k 位中点 = start + 8 + 16*k
    const auto mid = [&](int k) -> bool {
        const std::size_t idx = start + 8 + static_cast<std::size_t>(k) * kBitPeriod;
        if (idx >= wave.size()) {
            return false;
        }
        return wave[idx];
    };
    out.start_bit = mid(0);
    uint8_t data = 0;
    for (int b = 0; b < 8; ++b) {
        if (mid(b + 1)) {
            data |= static_cast<uint8_t>(1u << b);
        }
    }
    out.data = data;
    out.stop_bit = mid(9);
    return out;
}

// 在 uart_rx 上发送一帧 8N1（LSB 先出）
inline void uart_send_frame(ch::Simulator &sim, Uart16 &uart, uint8_t byte,
                            bool bad_stop = false) {
    auto &io = uart.io();
    // 起始位（低）
    sim.set_input_value(io.uart_rx, 0);
    for (unsigned i = 0; i < kBitPeriod; ++i) {
        sim.tick();
    }
    // 8 个数据位（LSB 先出）
    for (int b = 0; b < 8; ++b) {
        sim.set_input_value(io.uart_rx, (byte >> b) & 1u);
        for (unsigned i = 0; i < kBitPeriod; ++i) {
            sim.tick();
        }
    }
    // 停止位（高；bad_stop 时故意拉低制造帧错误）
    sim.set_input_value(io.uart_rx, bad_stop ? 0u : 1u);
    for (unsigned i = 0; i < kBitPeriod; ++i) {
        sim.tick();
    }
    sim.set_input_value(io.uart_rx, 1);
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


// ============================================================================
// 串行引擎（change axi4-peripheral-deferred-2026-10 D2）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - TX 串行帧可还原（起始位 + 8 数据位 + 停止位）",
    "[axi][uart][behavior][serial-tx][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        std::vector<bool> wave;
        uart_write_capture_tx(sim, dev.instance(), 0x5Au, wave);

        const RxFrame frame = decode_uart_frame(wave);
        REQUIRE(frame.start_bit == false);   // 起始位为低
        REQUIRE(frame.data == 0x5Au);        // 数据位 LSB 先出，可还原
        REQUIRE(frame.stop_bit == true);     // 停止位为高
    });

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - RX 收到帧后 rx_valid 置位、RX_DATA 可读、读走即清",
    "[axi][uart][behavior][serial-rx][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        uart_send_frame(sim, dev.instance(), 0xA5u);

        // bit1 = RX_VALID
        REQUIRE((uart_read(sim, dev.instance(), kRegStatus) & 0x2ULL) == 0x2ULL);
        // 收到的字节
        REQUIRE(uart_read(sim, dev.instance(), 0x04) == 0xA5ULL);
        // 读走 RX_DATA 后 rx_valid 清零
        REQUIRE((uart_read(sim, dev.instance(), kRegStatus) & 0x2ULL) == 0x0ULL);
    });

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - RX 停止位错误置 RX_FRAME_ERROR 且不置 rx_valid",
    "[axi][uart][behavior][serial-rx-error][dual]",
    [&]() {
        uart_init(sim, dev.instance());

        uart_send_frame(sim, dev.instance(), 0x3Cu, /*bad_stop=*/true);

        const uint64_t status = uart_read(sim, dev.instance(), kRegStatus);
        REQUIRE((status & 0x4ULL) == 0x4ULL);  // bit2 = RX_FRAME_ERROR
        REQUIRE((status & 0x2ULL) == 0x0ULL);  // bit1 = RX_VALID 保持 0
    });
