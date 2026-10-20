// tests/test_axi_timer.cpp
// AXI4-Lite Timer Controller — 通道 / 复位语义回归测试
//
// 覆盖（fix-axi4-peripheral-rst-n-port-2026-10 规范形态）：
//   1. rst_n 端口存在
//   2. 寄存器读写（LOAD / CTRL）
//   3. AW→W 相位分离（AW 与 W 分两周期，写仍须落地）
//   4. BVALID 保持语义（master 不预拉 BREADY 也必须看到 BVALID —— 反死锁）
//   5. RVALID 保持语义
//   6. rst_n 复位语义
//   7. Verilog 生成
//
// 注：本文件的存在同时消除了 `AxiLiteTimer` 全仓库零实例化导致
//     `axi_timer.h` 的编译错误长期潜伏、CI 看不见的问题。

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "axi4/peripherals/axi_timer.h"
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

// 寄存器偏移（见 axi_timer.h 头部寄存器映射）
constexpr uint32_t kRegLoad = 0x00;
constexpr uint32_t kRegCount = 0x04;
constexpr uint32_t kRegCtrl = 0x08;
constexpr uint32_t kRegStatus = 0x0C;

} // namespace

// ============================================================================
// 结构
// ============================================================================

TEST_CASE("AxiLiteTimer - ComponentCreation", "[axi][timer][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_timer_basic");
    ch::core::ctx_swap ctx_guard(ctx.get());

    AxiLiteTimer<32> timer(nullptr, "timer");
    REQUIRE(timer.name() == "timer");
}

TEST_CASE("AxiLiteTimer - 端口接口", "[axi][timer][interface]") {
    auto ctx = std::make_unique<ch::core::context>("test_timer_iface");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch::ch_device<AxiLiteTimer<32>> dev;
    auto &io = dev.instance().io();

    (void)io.awaddr; (void)io.awprot; (void)io.awvalid; (void)io.awready;
    (void)io.wdata;  (void)io.wstrb;  (void)io.wvalid;  (void)io.wready;
    (void)io.bresp;  (void)io.bvalid; (void)io.bready;
    (void)io.araddr; (void)io.arprot; (void)io.arvalid; (void)io.arready;
    (void)io.rdata;  (void)io.rresp;  (void)io.rvalid;  (void)io.rready;
    (void)io.irq;
    // 复位口（规范形态要求）
    (void)io.rst_n;
}

// ============================================================================
// 寄存器读写
// ============================================================================

TEST_CASE("AxiLiteTimer - 写 LOAD 可读回", "[axi][timer][reg]") {
    ch::ch_device<AxiLiteTimer<32>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);

    write(sim, dev, kRegLoad, 0x20u);

    REQUIRE(read(sim, dev, kRegLoad) == 0x20u);
}

TEST_CASE("AxiLiteTimer - 写 CTRL 可读回", "[axi][timer][reg]") {
    ch::ch_device<AxiLiteTimer<32>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);

    write(sim, dev, kRegCtrl, 0x01u);

    REQUIRE(read(sim, dev, kRegCtrl) == 0x01u);
}

// ============================================================================
// 协议语义
// ============================================================================

TEST_CASE("AxiLiteTimer - AW/W 相位分离后写仍落地", "[axi][timer][axi-protocol]") {
    ch::ch_device<AxiLiteTimer<32>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);

    write(sim, dev, kRegLoad, 0x44u, /*separate_aw_w=*/true);

    REQUIRE(read(sim, dev, kRegLoad) == 0x44u);
}

TEST_CASE("AxiLiteTimer - AW 握手后改写 awaddr 不改变写目标", "[axi][timer][axi-protocol]") {
    ch::ch_device<AxiLiteTimer<32>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);

    // AW 阶段：目标 = LOAD (0x00)
    sim.set_input_value(io.awaddr, kRegLoad);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, 0x3Cu);
    sim.set_input_value(io.wstrb, 0xF);  // AXI4-Lite: master 必须驱动 WSTRB
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

    REQUIRE(read(sim, dev, kRegLoad) == 0x3Cu);
    REQUIRE(read(sim, dev, kRegCtrl) == 0u);
}

TEST_CASE("AxiLiteTimer - BVALID 保持到 BREADY（master 不预拉 BREADY 不死锁）",
          "[axi][timer][axi-protocol]") {
    ch::ch_device<AxiLiteTimer<32>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);

    sim.set_input_value(io.awaddr, kRegLoad);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, 0x66u);
    sim.set_input_value(io.wstrb, 0xF);  // AXI4-Lite: master 必须驱动 WSTRB
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

TEST_CASE("AxiLiteTimer - RVALID 保持到 RREADY", "[axi][timer][axi-protocol]") {
    ch::ch_device<AxiLiteTimer<32>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    write(sim, dev, kRegLoad, 0x99u);

    sim.set_input_value(io.araddr, kRegLoad);
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

TEST_CASE("AxiLiteTimer - 事务结束后可接受下一笔", "[axi][timer][axi-protocol]") {
    ch::ch_device<AxiLiteTimer<32>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);

    write(sim, dev, kRegLoad, 0x01u);
    write(sim, dev, kRegLoad, 0x02u);

    REQUIRE(read(sim, dev, kRegLoad) == 0x02u);
}

// ============================================================================
// 复位语义
// ============================================================================

TEST_CASE("AxiLiteTimer - rst_n 复位清空寄存器并释放 busy", "[axi][timer][rst-n]") {
    ch::ch_device<AxiLiteTimer<32>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);

    write(sim, dev, kRegLoad, 0xABu);
    write(sim, dev, kRegCtrl, 0x01u);
    REQUIRE(read(sim, dev, kRegLoad) == 0xABu);
    REQUIRE(read(sim, dev, kRegCtrl) == 0x01u);

    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.tick();

    // 复位期间 peripheral 处于复位态（不响应 AXI 事务），先释放复位再读回
    sim.set_input_value(io.rst_n, 1);
    sim.tick();

    REQUIRE(read(sim, dev, kRegLoad) == 0u);
    REQUIRE(read(sim, dev, kRegCtrl) == 0u);

    // 复位后仍可接受新事务
    write(sim, dev, kRegLoad, 0x33u);
    REQUIRE(read(sim, dev, kRegLoad) == 0x33u);
}

// ============================================================================
// Verilog 生成
// ============================================================================

TEST_CASE("AxiLiteTimer - VerilogGeneration", "[axi][timer][verilog]") {
    ch::ch_device<AxiLiteTimer<32>> dev;
    const std::string file = "test_axi_timer.v";
    toVerilog(file, dev.context());

    std::ifstream f(file);
    REQUIRE(f.good());
    const std::string content((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    REQUIRE_FALSE(content.empty());
    std::remove(file.c_str());
}
