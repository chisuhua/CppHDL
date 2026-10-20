// tests/test_axi_gpio.cpp
// AXI4-Lite GPIO Controller — 通道 / 复位语义回归测试
//
// 覆盖（fix-axi4-peripheral-rst-n-port-2026-10 规范形态）：
//   1. rst_n 端口存在
//   2. 寄存器读写（DATA_OUT / DIRECTION）
//   3. AW→W 相位分离（AW 与 W 分两周期，写仍须落地）
//   4. BVALID 保持语义（master 不预拉 BREADY 也必须看到 BVALID —— 反死锁）
//   5. RVALID 保持语义
//   6. rst_n 复位语义（寄存器归位 + busy 清除 + 可再次接受事务）
//   7. Verilog 生成

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "axi4/peripherals/axi_gpio.h"
#include "axi_lite_slave_fixture.h"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "device.h"
#include "simulator.h"

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

// 读侧地址映射（与 axi_gpio.h 头部文档一致）
constexpr uint32_t kRegDataOut = 0x00;
constexpr uint32_t kRegDataIn = 0x04;
constexpr uint32_t kRegDirection = 0x08;
constexpr uint32_t kRegInterrupt = 0x0C;

// 写侧地址映射：**当前实现**与读侧/文档不一致 ——
//   写侧 0x00→DATA_OUT、0x04→DIRECTION、0x08→INTERRUPT
//   读侧 0x00→DATA_OUT、0x04→DATA_IN、 0x08→DIRECTION、0x0C→INTERRUPT
// 该不一致由 follow-up change (axi4-peripheral-followups-2026-10) 修正；
// 本 change 只锁定写侧当前行为，不改动地址映射。
constexpr uint32_t kRegDirectionWrite = 0x04;

} // namespace

// ============================================================================
// 结构
// ============================================================================

TEST_CASE("AxiLiteGpio - ComponentCreation", "[axi][gpio][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_gpio_basic");
    ch::core::ctx_swap ctx_guard(ctx.get());

    AxiLiteGpio<8> gpio(nullptr, "gpio");
    REQUIRE(gpio.name() == "gpio");
}

TEST_CASE("AxiLiteGpio - 端口接口", "[axi][gpio][interface]") {
    auto ctx = std::make_unique<ch::core::context>("test_gpio_iface");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch::ch_device<AxiLiteGpio<8>> dev;
    auto &io = dev.instance().io();

    // AXI4-Lite 通道
    (void)io.awaddr; (void)io.awprot; (void)io.awvalid; (void)io.awready;
    (void)io.wdata;  (void)io.wstrb;  (void)io.wvalid;  (void)io.wready;
    (void)io.bresp;  (void)io.bvalid; (void)io.bready;
    (void)io.araddr; (void)io.arprot; (void)io.arvalid; (void)io.arready;
    (void)io.rdata;  (void)io.rresp;  (void)io.rvalid;  (void)io.rready;
    // GPIO 物理接口
    (void)io.gpio_out; (void)io.gpio_in; (void)io.gpio_dir; (void)io.irq;
    // 复位口（规范形态要求）
    (void)io.rst_n;
}

// ============================================================================
// 寄存器读写
// ============================================================================

TEST_CASE("AxiLiteGpio - 写 DATA_OUT 反映到 gpio_out", "[axi][gpio][reg]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0);

    write(sim, dev, kRegDataOut, 0xABu);

    REQUIRE(pv(sim, io.gpio_out) == 0xABu);
}

TEST_CASE("AxiLiteGpio - 写 DIRECTION 反映到 gpio_dir", "[axi][gpio][reg]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0);

    // 注意：写侧 DIRECTION 的偏移当前是 0x04（见 kRegDirectionWrite 注释）
    write(sim, dev, kRegDirectionWrite, 0x0Fu);

    REQUIRE(pv(sim, io.gpio_dir) == 0x0Fu);
}

TEST_CASE("AxiLiteGpio - 读 DATA_OUT 返回写入值", "[axi][gpio][reg]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0);

    write(sim, dev, kRegDataOut, 0x5Au);
    const uint32_t rdata = read(sim, dev, kRegDataOut);

    REQUIRE(rdata == 0x5Au);
}

TEST_CASE("AxiLiteGpio - 读 DATA_IN 反映 gpio_in", "[axi][gpio][reg]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0x3Cu);

    const uint32_t rdata = read(sim, dev, kRegDataIn);

    REQUIRE(rdata == 0x3Cu);
}

// ============================================================================
// 协议语义
// ============================================================================

TEST_CASE("AxiLiteGpio - AW/W 相位分离后写仍落地", "[axi][gpio][axi-protocol]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0);

    // AW 与 W 分两周期（AXI4-Lite 合法用法）
    write(sim, dev, kRegDataOut, 0x77u, /*separate_aw_w=*/true);

    REQUIRE(pv(sim, io.gpio_out) == 0x77u);
}

TEST_CASE("AxiLiteGpio - AW 握手后改写 awaddr 不改变写目标", "[axi][gpio][axi-protocol]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0);

    // AW 阶段：目标 = DATA_OUT (0x00)
    sim.set_input_value(io.awaddr, kRegDataOut);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, 0x3Cu);
    sim.set_input_value(io.wvalid, 0);
    sim.tick();  // AW 握手

    // AXI 只要求 AWADDR 保持到 AW 握手 —— master 此后可以改写它。
    // peripheral 必须在握手时锁存地址，否则写会落到错误寄存器。
    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.awaddr, kRegDirectionWrite);
    sim.set_input_value(io.wvalid, 1);

    const bool b_seen = wait_for(sim, io.bvalid);
    REQUIRE(b_seen);
    sim.set_input_value(io.wvalid, 0);
    sim.set_input_value(io.bready, 1);
    sim.tick();
    sim.set_input_value(io.bready, 0);

    REQUIRE(pv(sim, io.gpio_out) == 0x3Cu);
    REQUIRE(pv(sim, io.gpio_dir) == 0u);
}

TEST_CASE("AxiLiteGpio - BVALID 保持到 BREADY（master 不预拉 BREADY 不死锁）",
          "[axi][gpio][axi-protocol]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0);

    // master 同周期拉 AWVALID + WVALID，但**不**预拉 BREADY
    sim.set_input_value(io.awaddr, kRegDataOut);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, 0x11u);
    sim.set_input_value(io.wvalid, 1);

    // BVALID 必须自己出现（不能靠 BREADY 触发）
    const bool b_seen = wait_for(sim, io.bvalid);
    REQUIRE(b_seen);

    // master 撤掉 AWVALID/WVALID 后，BVALID 仍须保持
    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.wvalid, 0);
    sim.tick();
    REQUIRE(pv(sim, io.bvalid) == 1);

    // 拉 BREADY 完成握手
    sim.set_input_value(io.bready, 1);
    sim.tick();
    sim.set_input_value(io.bready, 0);
    REQUIRE(pv(sim, io.bvalid) == 0);
}

TEST_CASE("AxiLiteGpio - RVALID 保持到 RREADY", "[axi][gpio][axi-protocol]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0x7Eu);

    sim.set_input_value(io.araddr, kRegDataIn);
    sim.set_input_value(io.arvalid, 1);

    const bool r_seen = wait_for(sim, io.rvalid);
    REQUIRE(r_seen);

    // 撤掉 ARVALID 后 RVALID 仍须保持
    sim.set_input_value(io.arvalid, 0);
    sim.tick();
    REQUIRE(pv(sim, io.rvalid) == 1);
    REQUIRE(pv(sim, io.rdata) == 0x7Eu);

    sim.set_input_value(io.rready, 1);
    sim.tick();
    sim.set_input_value(io.rready, 0);
    REQUIRE(pv(sim, io.rvalid) == 0);
}

TEST_CASE("AxiLiteGpio - 事务结束后可接受下一笔", "[axi][gpio][axi-protocol]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0);

    write(sim, dev, kRegDataOut, 0x01u);
    REQUIRE(pv(sim, io.gpio_out) == 0x01u);

    // 第二笔（第一笔完成后 busy 必须已清除）
    write(sim, dev, kRegDataOut, 0x02u);
    REQUIRE(pv(sim, io.gpio_out) == 0x02u);
}

// ============================================================================
// 复位语义
// ============================================================================

TEST_CASE("AxiLiteGpio - rst_n 复位清空寄存器并释放 busy", "[axi][gpio][rst-n]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    sim.set_input_value(io.rst_n, 1);
    sim.set_input_value(io.gpio_in, 0);

    write(sim, dev, kRegDataOut, 0xABu);
    write(sim, dev, kRegDirectionWrite, 0x0Fu);
    REQUIRE(pv(sim, io.gpio_out) == 0xABu);
    REQUIRE(pv(sim, io.gpio_dir) == 0x0Fu);

    // 拉低 rst_n
    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.tick();

    REQUIRE(pv(sim, io.gpio_out) == 0u);
    REQUIRE(pv(sim, io.gpio_dir) == 0u);

    // 复位后仍可接受新事务
    sim.set_input_value(io.rst_n, 1);
    sim.tick();
    write(sim, dev, kRegDataOut, 0x33u);
    REQUIRE(pv(sim, io.gpio_out) == 0x33u);
}
// ============================================================================
// Verilog 生成
// ============================================================================

TEST_CASE("AxiLiteGpio - VerilogGeneration", "[axi][gpio][verilog]") {
    ch::ch_device<AxiLiteGpio<8>> dev;
    const std::string file = "test_axi_gpio.v";
    toVerilog(file, dev.context());

    std::ifstream f(file);
    REQUIRE(f.good());
    const std::string content((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    REQUIRE_FALSE(content.empty());
    std::remove(file.c_str());
}
