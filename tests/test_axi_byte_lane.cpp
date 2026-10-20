// tests/test_axi_byte_lane.cpp
// AXI4-Lite 字节道（byte lane）写语义回归测试（**双后端**：interpreter + verilator）
//
// 契约（openspec change `axi4-peripheral-deferred-2026-10` D1，
// 语义登记于 include/axi4/AGENTS.md CONVENTIONS → 字节道语义）：
//   * WSTRB 位 i ↔ 字节道 i（bit0 → byte0 … bit3 → byte3），每道 8 bit
//   * 写路径：new = (old & ~mask) | (wdata & mask)
//   * 亚 32 位寄存器：只有落在其有效字节道内的写才生效
//     （8 bit 寄存器仅 byte0；16 bit 寄存器 byte0..1）
//   * WSTRB == 0 → 无字节被选中 → 寄存器保持原值
//   * 只读寄存器不在写路径内 → 写其地址回 DECERR，值不变
//
// 覆盖 6 个 AXI4-Lite slave peripheral：gpio / timer / uart / pwm / i2c / spi
//
// ⚠️ 双后端的原因：字节道掩码由 `include/axi4/byte_lane.h` 的
// `byte_lane_apply<W>` 统一实现，但**窄寄存器截断**（`ch_uint<8>` /
// `ch_uint<16>`）在 Verilog codegen 路径上与 interpreter 路径不同 ——
// 只跑 interpreter 会漏掉 codegen 侧的截断/掩码偏差。

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

#include "axi4/peripherals/axi_gpio.h"
#include "axi4/peripherals/axi_i2c.h"
#include "axi4/peripherals/axi_pwm.h"
#include "axi4/peripherals/axi_spi.h"
#include "axi4/peripherals/axi_timer.h"
#include "axi4/peripherals/axi_uart.h"
#include "axi_lite_slave_fixture.h"
#include "ch.hpp"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace axi4;
using ch_test::axi_lite::pv;
using ch_test::axi_lite::read;
using ch_test::axi_lite::wait_for;
using ch_test::axi_lite::write;

// Typedefs to avoid preprocessor comma-split on DUAL_BACKEND_TEST_PAIR args.
using Gpio8 = axi4::AxiLiteGpio<8>;
using Timer32 = axi4::AxiLiteTimer<32>;
using Uart16 = axi4::AxiLiteUart<16>;
using Pwm16x4 = axi4::AxiLitePwm<16, 4>;
using I2c16 = axi4::AxiLiteI2c<16>;
using Spi8x16 = axi4::AxiLiteSpi<8, 16>;

namespace {

// 各 peripheral 的寄存器地址（与各头文件头部登记的映射一致）
constexpr uint32_t kGpioDataOut = 0x00;
constexpr uint32_t kGpioDataIn = 0x04;   // 只读
constexpr uint32_t kTimerLoad = 0x00;
constexpr uint32_t kUartTxData = 0x00;
constexpr uint32_t kPwmPeriod = 0x00;
constexpr uint32_t kPwmDuty0 = 0x10;
constexpr uint32_t kI2cTxData = 0x08;    // 8 bit
constexpr uint32_t kI2cPrescale = 0x10;  // 16 bit
constexpr uint32_t kSpiBaud = 0x10;      // BAUD_WIDTH bit

} // namespace

// ============================================================================
// 32 bit 寄存器
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Gpio8,
    "AxiLiteGpio - 部分字节写只改选中字节",
    "[axi][byte-lane][gpio][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);
        sim.set_input_value(io.gpio_in, 0);

        // 整字写
        write(sim, dev, kGpioDataOut, 0xAABBCCDDu);
        REQUIRE(read(sim, dev, kGpioDataOut) == 0xAABBCCDDu);

        // 只写 byte0（wstrb = 0b0001）→ 其余三字节保持原值
        write(sim, dev, kGpioDataOut, 0x00000011u, false, 0x1);
        REQUIRE(read(sim, dev, kGpioDataOut) == 0xAABBCC11u);

        // 只写 byte1（wstrb = 0b0010）—— 0x00002200 的 0x22 落在 byte1
        write(sim, dev, kGpioDataOut, 0x00002200u, false, 0x2);
        REQUIRE(read(sim, dev, kGpioDataOut) == 0xAABB2211u);

        // 只写 byte3（wstrb = 0b1000）
        write(sim, dev, kGpioDataOut, 0x44000000u, false, 0x8);
        REQUIRE(read(sim, dev, kGpioDataOut) == 0x44BB2211u);

        // WSTRB == 0 → 无字节被选中 → 值保持不变
        write(sim, dev, kGpioDataOut, 0xFFFFFFFFu, false, 0x0);
        REQUIRE(read(sim, dev, kGpioDataOut) == 0x44BB2211u);
    });

DUAL_BACKEND_TEST_PAIR(
    Gpio8,
    "AxiLiteGpio - 只读 DATA_IN 不被字节写改写",
    "[axi][byte-lane][gpio][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);
        sim.set_input_value(io.gpio_in, 0x3Cu);

        // 写只读寄存器 → DECERR 且值不变
        sim.set_input_value(io.awaddr, kGpioDataIn);
        sim.set_input_value(io.awprot, 0);
        sim.set_input_value(io.awvalid, 1);
        sim.set_input_value(io.wdata, 0xFFFFFFFFu);
        sim.set_input_value(io.wstrb, 0xF);
        sim.set_input_value(io.wvalid, 1);
        sim.tick();
        sim.set_input_value(io.awvalid, 0);

        REQUIRE(wait_for(sim, io.bvalid));
        // 未映射（只读）地址 → DECERR
        REQUIRE(pv(sim, io.bresp) == 0x3u);
        sim.set_input_value(io.wvalid, 0);
        sim.set_input_value(io.bready, 1);
        sim.tick();
        sim.set_input_value(io.bready, 0);

        // DATA_IN 仍反映 gpio_in，未被写改写
        REQUIRE(read(sim, dev, kGpioDataIn) == 0x3Cu);
    });

DUAL_BACKEND_TEST_PAIR(
    Timer32,
    "AxiLiteTimer - LOAD 部分字节写只改选中字节",
    "[axi][byte-lane][timer][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        write(sim, dev, kTimerLoad, 0x11223344u);
        REQUIRE(read(sim, dev, kTimerLoad) == 0x11223344u);

        write(sim, dev, kTimerLoad, 0x00009900u, false, 0x2);
        REQUIRE(read(sim, dev, kTimerLoad) == 0x11229944u);
    });

DUAL_BACKEND_TEST_PAIR(
    Uart16,
    "AxiLiteUart - TX_DATA 部分字节写只改选中字节",
    "[axi][byte-lane][uart][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);
        sim.set_input_value(io.uart_rx, 1);  // UART 空闲电平为高

        write(sim, dev, kUartTxData, 0xAABBCCDDu);
        REQUIRE(read(sim, dev, kUartTxData) == 0xAABBCCDDu);

        write(sim, dev, kUartTxData, 0x00000077u, false, 0x1);
        REQUIRE(read(sim, dev, kUartTxData) == 0xAABBCC77u);
    });

DUAL_BACKEND_TEST_PAIR(
    Pwm16x4,
    "AxiLitePwm - PERIOD / DUTY 部分字节写只改选中字节",
    "[axi][byte-lane][pwm][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        write(sim, dev, kPwmPeriod, 0x0A0B0C0Du);
        REQUIRE(read(sim, dev, kPwmPeriod) == 0x0A0B0C0Du);

        // 只写 byte0
        write(sim, dev, kPwmPeriod, 0x000000FFu, false, 0x1);
        REQUIRE(read(sim, dev, kPwmPeriod) == 0x0A0B0CFFu);

        // DUTY0：只写 byte2
        write(sim, dev, kPwmDuty0, 0x12340000u);
        REQUIRE(read(sim, dev, kPwmDuty0) == 0x12340000u);
        write(sim, dev, kPwmDuty0, 0x00AB0000u, false, 0x4);
        REQUIRE(read(sim, dev, kPwmDuty0) == 0x12AB0000u);
    });

// ============================================================================
// 亚 32 bit 寄存器（有效字节道受限 + codegen 截断路径）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    I2c16,
    "AxiLiteI2c - TX_DATA(8bit) 只受 byte0 影响，PRESCALE(16bit) 受 byte0..1 影响",
    "[axi][byte-lane][i2c][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        // TX_DATA 是 8 bit 寄存器 → 只有 byte0 有效
        write(sim, dev, kI2cTxData, 0x000000ABu);
        REQUIRE(read(sim, dev, kI2cTxData) == 0xABu);

        // 写 byte1（wstrb = 0b0010）→ 落在无效道 → 被忽略
        write(sim, dev, kI2cTxData, 0x0000CC00u, false, 0x2);
        REQUIRE(read(sim, dev, kI2cTxData) == 0xABu);

        // 写 byte0 → 生效
        write(sim, dev, kI2cTxData, 0x0000005Au, false, 0x1);
        REQUIRE(read(sim, dev, kI2cTxData) == 0x5Au);

        // PRESCALE 是 16 bit 寄存器 → byte0 / byte1 有效，byte2 无效
        write(sim, dev, kI2cPrescale, 0x00001234u);
        REQUIRE(read(sim, dev, kI2cPrescale) == 0x1234u);

        // 只写 byte0 → 低字节被替换，高字节保持
        write(sim, dev, kI2cPrescale, 0x00005678u, false, 0x1);
        REQUIRE(read(sim, dev, kI2cPrescale) == 0x1278u);

        // 只写 byte1（0x0000AB00 的 0xAB 落在 byte1）
        write(sim, dev, kI2cPrescale, 0x0000AB00u, false, 0x2);
        REQUIRE(read(sim, dev, kI2cPrescale) == 0xAB78u);

        // byte2 对 16 bit 寄存器无效
        write(sim, dev, kI2cPrescale, 0xCC000000u, false, 0x4);
        REQUIRE(read(sim, dev, kI2cPrescale) == 0xAB78u);
    });

DUAL_BACKEND_TEST_PAIR(
    Spi8x16,
    "AxiLiteSpi - BAUD(16bit) 部分字节写只改选中字节",
    "[axi][byte-lane][spi][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        write(sim, dev, kSpiBaud, 0x00001234u);
        REQUIRE(read(sim, dev, kSpiBaud) == 0x1234u);

        // 只写 byte0
        write(sim, dev, kSpiBaud, 0x00000056u, false, 0x1);
        REQUIRE(read(sim, dev, kSpiBaud) == 0x1256u);

        // 只写 byte1
        write(sim, dev, kSpiBaud, 0x0000AB00u, false, 0x2);
        REQUIRE(read(sim, dev, kSpiBaud) == 0xAB56u);

        // byte2 对 16 bit 寄存器无效
        write(sim, dev, kSpiBaud, 0xCC000000u, false, 0x4);
        REQUIRE(read(sim, dev, kSpiBaud) == 0xAB56u);
    });
