// tests/test_axi_uart.cpp
// AXI4-Lite UART Controller — 通道 / 复位语义回归测试
//
// 覆盖（fix-axi4-peripheral-rst-n-port-2026-10 规范形态）：
//   1. rst_n 端口存在
//   2. 寄存器读写（TX_DATA / CTRL）
//   3. AW→W 相位分离（AW 与 W 分两周期，写仍须落地）
//   4. BVALID 保持语义（master 不预拉 BREADY 也必须看到 BVALID —— 反死锁）
//   5. RVALID 保持语义
//   6. rst_n 复位语义
//   7. Verilog 生成

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "axi4/peripherals/axi_uart.h"
#include "axi_lite_slave_fixture.h"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "device.h"
#include "simulator.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

using namespace ch;
using namespace ch::core;
using namespace axi4;
using ch_test::axi_lite::pv;
using ch_test::axi_lite::wait_for;
using ch_test::axi_lite::read;
using ch_test::axi_lite::write;

namespace {

// 寄存器偏移（见 axi_uart.h 头部寄存器映射）
constexpr uint32_t kRegTxData = 0x00;
constexpr uint32_t kRegRxData = 0x04;
constexpr uint32_t kRegStatus = 0x08;
constexpr uint32_t kRegCtrl = 0x0C;

} // namespace

// ============================================================================
// 结构
// ============================================================================

TEST_CASE("AxiLiteUart - ComponentCreation", "[axi][uart][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_uart_basic");
    ch::core::ctx_swap ctx_guard(ctx.get());

    AxiLiteUart<16> uart(nullptr, "uart");
    REQUIRE(uart.name() == "uart");
}

TEST_CASE("AxiLiteUart - 端口接口", "[axi][uart][interface]") {
    auto ctx = std::make_unique<ch::core::context>("test_uart_iface");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch::ch_device<AxiLiteUart<16>> dev;
    auto &io = dev.instance().io();

    (void)io.awaddr; (void)io.awprot; (void)io.awvalid; (void)io.awready;
    (void)io.wdata;  (void)io.wstrb;  (void)io.wvalid;  (void)io.wready;
    (void)io.bresp;  (void)io.bvalid; (void)io.bready;
    (void)io.araddr; (void)io.arprot; (void)io.arvalid; (void)io.arready;
    (void)io.rdata;  (void)io.rresp;  (void)io.rvalid;  (void)io.rready;
    (void)io.uart_tx; (void)io.uart_rx; (void)io.irq;
    // 复位口（规范形态要求）
    (void)io.rst_n;
}

// ============================================================================
// 寄存器读写
// ============================================================================

TEST_CASE("AxiLiteUart - 写 TX_DATA 可读回", "[axi][uart][reg]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.uart_rx, 0);

    write(sim, dev, kRegTxData, 0x41u);

    REQUIRE(read(sim, dev, kRegTxData) == 0x41u);
}

TEST_CASE("AxiLiteUart - 写 CTRL 可读回", "[axi][uart][reg]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.uart_rx, 0);

    write(sim, dev, kRegCtrl, 0x03u);

    REQUIRE(read(sim, dev, kRegCtrl) == 0x03u);
}

TEST_CASE("AxiLiteUart - 读 STATUS 恒为 0（TX_EMPTY/RX_VALID 语义待 follow-up）",
          "[axi][uart][reg]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.uart_rx, 0);

    // STATUS 位的有效语义由 follow-up change（axi4-peripheral-followups-2026-10）定义，
    // 本测试只锁定"复位后为 0"这一当前实际行为。
    REQUIRE(read(sim, dev, kRegStatus) == 0u);
}

// ============================================================================
// 协议语义
// ============================================================================

TEST_CASE("AxiLiteUart - AW/W 相位分离后写仍落地", "[axi][uart][axi-protocol]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.uart_rx, 0);

    write(sim, dev, kRegTxData, 0x55u, /*separate_aw_w=*/true);

    REQUIRE(read(sim, dev, kRegTxData) == 0x55u);
}

TEST_CASE("AxiLiteUart - AW 握手后改写 awaddr 不改变写目标", "[axi][uart][axi-protocol]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.uart_rx, 0);

    // AW 阶段：目标 = TX_DATA (0x00)
    sim.set_input_value(io.awaddr, kRegTxData);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, 0x3Cu);
    sim.set_input_value(io.wvalid, 0);
    sim.tick();  // AW 握手

    // AXI 只要求 AWADDR 保持到 AW 握手 —— master 此后可以改写它。
    // peripheral 必须在握手时锁存地址，否则写会落到错误寄存器。
    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.awaddr, kRegCtrl);
    sim.set_input_value(io.wvalid, 1);

    const bool b_seen = wait_for(sim, io.bvalid);
    REQUIRE(b_seen);
    sim.set_input_value(io.wvalid, 0);
    sim.set_input_value(io.bready, 1);
    sim.tick();
    sim.set_input_value(io.bready, 0);

    REQUIRE(read(sim, dev, kRegTxData) == 0x3Cu);
    REQUIRE(read(sim, dev, kRegCtrl) == 0u);
}

TEST_CASE("AxiLiteUart - BVALID 保持到 BREADY（master 不预拉 BREADY 不死锁）",
          "[axi][uart][axi-protocol]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.uart_rx, 0);

    sim.set_input_value(io.awaddr, kRegTxData);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, 0x66u);
    sim.set_input_value(io.wvalid, 1);

    const bool b_seen = wait_for(sim, io.bvalid);
    REQUIRE(b_seen);

    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.wvalid, 0);
    sim.tick();
    REQUIRE(pv(sim, io.bvalid) == 1);

    sim.set_input_value(io.bready, 1);
    sim.tick();
    sim.set_input_value(io.bready, 0);
    REQUIRE(pv(sim, io.bvalid) == 0);
}

TEST_CASE("AxiLiteUart - RVALID 保持到 RREADY", "[axi][uart][axi-protocol]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.uart_rx, 0);
    write(sim, dev, kRegTxData, 0x99u);

    sim.set_input_value(io.araddr, kRegTxData);
    sim.set_input_value(io.arvalid, 1);

    const bool r_seen = wait_for(sim, io.rvalid);
    REQUIRE(r_seen);

    sim.set_input_value(io.arvalid, 0);
    sim.tick();
    REQUIRE(pv(sim, io.rvalid) == 1);
    REQUIRE(pv(sim, io.rdata) == 0x99u);

    sim.set_input_value(io.rready, 1);
    sim.tick();
    sim.set_input_value(io.rready, 0);
    REQUIRE(pv(sim, io.rvalid) == 0);
}

TEST_CASE("AxiLiteUart - 事务结束后可接受下一笔", "[axi][uart][axi-protocol]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.uart_rx, 0);

    write(sim, dev, kRegTxData, 0x01u);
    write(sim, dev, kRegTxData, 0x02u);

    REQUIRE(read(sim, dev, kRegTxData) == 0x02u);
}

// ============================================================================
// 复位语义
// ============================================================================

TEST_CASE("AxiLiteUart - rst_n 复位清空寄存器并释放 busy", "[axi][uart][rst-n]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.uart_rx, 0);

    write(sim, dev, kRegTxData, 0xABu);
    write(sim, dev, kRegCtrl, 0x05u);
    REQUIRE(read(sim, dev, kRegTxData) == 0xABu);
    REQUIRE(read(sim, dev, kRegCtrl) == 0x05u);

    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.tick();

    // 复位期间 peripheral 处于复位态（不响应 AXI 事务），先释放复位再读回
    sim.set_input_value(io.rst_n, 1);
    sim.tick();

    REQUIRE(read(sim, dev, kRegTxData) == 0u);
    REQUIRE(read(sim, dev, kRegCtrl) == 0u);

    // 复位后仍可接受新事务
    write(sim, dev, kRegTxData, 0x33u);
    REQUIRE(read(sim, dev, kRegTxData) == 0x33u);
}

// ============================================================================
// Verilog 生成
// ============================================================================

TEST_CASE("AxiLiteUart - VerilogGeneration", "[axi][uart][verilog]") {
    ch::ch_device<AxiLiteUart<16>> dev;
    const std::string file = "test_axi_uart.v";
    toVerilog(file, dev.context());

    std::ifstream f(file);
    REQUIRE(f.good());
    const std::string content((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    REQUIRE_FALSE(content.empty());
    std::remove(file.c_str());
}
