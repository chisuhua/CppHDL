// tests/test_axi_lite_slave_conformance.cpp
//
// 通用 AXI4-Lite slave（`include/axi4/axi4_lite_slave.h` 的 `AxiLiteSlave`）
// 的协议规范形态回归测试（双后端：interpreter + verilator）。
//
// Capability spec: openspec/specs/axi-slave-conformance/spec.md
// Change:           openspec/changes/axi4-lite-generic-slave-conformance-2026-10
//
// 规范形态要点（与 `include/axi4/peripherals/axi_gpio.h` 的四件套同源）：
//   * `rst_n`：低有效，**在时钟沿生效**
//   * AW→W 相位分离：W 可在 AW 之后的任意周期被接受；
//     **同拍 AW+W 时该拍 W 不被接受**（master 必须保持 WVALID 到 WREADY）
//   * 地址锁存：AW/AR 握手时锁存地址；握手后 master 改写地址不影响写/读目标
//   * 响应保持：`bvalid` 保持到 BREADY，`rvalid` 保持到 RREADY
//   * `busy` 只在事务真正完成（`b_valid && bready` / `r_valid && rready`）时释放
//   * 未映射地址 → `DECERR`(0b11)，MUST NOT 别名到已实现寄存器
//   * 已映射地址但 `wstrb == 0` → 空写，响应仍为 `OKAY`(0b00)
//
// 时序模型见 `tests/axi_lite_slave_fixture.h` 头部说明：
//   `sim.tick()` = 组合求值 → 时钟沿更新寄存器 → 再组合求值
//   ⇒ tick 结束后读到的端口值 = **下一周期**的值
//   ⇒ 组合信号（awready/wready/bvalid/rvalid）只在 tick 之间可观测。
//   本文件因此用**事件驱动**（等 BVALID / RVALID 出现）而非「等 ready 变 1」。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "core/context.h"
#include "device.h"
#include "simulator.h"

#include "axi4/axi4_lite_slave.h"

#include "axi_lite_slave_fixture.h"
#include "dual_backend_fixture.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

using ch_test::axi_lite::pv;
using ch_test::axi_lite::wait_for;

using Lite32 = axi4::AxiLiteSlave<32, 32, 4>;

namespace {

constexpr std::uint32_t kReg0 = 0x00;
constexpr std::uint32_t kReg1 = 0x04;
constexpr std::uint32_t kReg2 = 0x08;
constexpr std::uint32_t kReg3 = 0x0C;

// 窗口外（4 字节步长 → 寄存器窗口 = 字节地址 0x00..0x0C）
constexpr std::uint32_t kUnmapped = 0x10;
// 低位与 reg0 相同但落在窗口外（别名陷阱）
constexpr std::uint32_t kAliased = 0x100;

constexpr std::uint32_t kRespOkay = 0b00;
constexpr std::uint32_t kRespDecErr = 0b11;

// ---------------------------------------------------------------------------
// 手写 AW / W 握手（需要精确控制「AW 与 W 之间」的拍数与地址改写时机时用）
// ---------------------------------------------------------------------------

// AW 握手一拍。返回后 `awvalid` 已撤销。
template <typename Io>
inline void aw_handshake(ch::Simulator &sim, Io &io, std::uint32_t addr) {
    sim.set_input_value(io.awaddr, addr);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wvalid, 0);
    sim.tick();
    sim.set_input_value(io.awvalid, 0);
}

// W 握手一拍 + 等待 BVALID + B 握手。返回后事务已结束。
template <typename Io>
inline void w_handshake_and_b(ch::Simulator &sim, Io &io, std::uint32_t data,
                              std::uint8_t wstrb) {
    sim.set_input_value(io.wdata, data);
    sim.set_input_value(io.wstrb, wstrb);
    sim.set_input_value(io.wvalid, 1);
    sim.tick();
    sim.set_input_value(io.wvalid, 0);
    const bool b_seen = wait_for(sim, io.bvalid);
    REQUIRE(b_seen);
    sim.set_input_value(io.bready, 1);
    sim.tick();
    sim.set_input_value(io.bready, 0);
}

// AR 握手一拍。返回后 `arvalid` 已撤销。
template <typename Io>
inline void ar_handshake(ch::Simulator &sim, Io &io, std::uint32_t addr) {
    sim.set_input_value(io.araddr, addr);
    sim.set_input_value(io.arvalid, 1);
    sim.tick();
    sim.set_input_value(io.arvalid, 0);
}

// 读事务（AR → 等 RVALID → 读 rdata → RREADY 握手）
template <typename Io>
inline std::uint32_t read_reg(ch::Simulator &sim, Io &io, std::uint32_t addr) {
    ar_handshake(sim, io, addr);
    const bool r_seen = wait_for(sim, io.rvalid);
    REQUIRE(r_seen);
    const auto value = static_cast<std::uint32_t>(pv(sim, io.rdata));
    sim.set_input_value(io.rready, 1);
    sim.tick();
    sim.set_input_value(io.rready, 0);
    return value;
}

} // namespace

// ============================================================================
// 用例 3：AW 握手后改写 awaddr → 写目标不变
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - AW 握手后改写 awaddr 不改变写目标",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        // 前置：reg0 / reg1 写入互不相同的值（否则「目标不变」可空过）
        ch_test::axi_lite::write_io(sim, io, kReg0, 0xAAAA0000u, true);
        ch_test::axi_lite::write_io(sim, io, kReg1, 0xBBBB1111u, true);
        REQUIRE(read_reg(sim, io, kReg0) == 0xAAAA0000u);
        REQUIRE(read_reg(sim, io, kReg1) == 0xBBBB1111u);

        // AW 握手（地址 = reg0）
        sim.set_input_value(io.awaddr, kReg0);
        sim.set_input_value(io.awvalid, 1);
        sim.set_input_value(io.wvalid, 0);
        sim.tick();
        sim.set_input_value(io.awvalid, 0);

        // 握手后改写 awaddr（AXI 允许：AWADDR 只需保持到 AW 握手）
        sim.set_input_value(io.awaddr, kReg1);

        // W 拍 → 写目标 MUST 仍是 AW 握手时锁存的 reg0
        w_handshake_and_b(sim, io, 0xCCCC2222u, 0xF);

        REQUIRE(read_reg(sim, io, kReg0) == 0xCCCC2222u);
        REQUIRE(read_reg(sim, io, kReg1) == 0xBBBB1111u);
    });

// ============================================================================
// 用例 4：AR 握手后改写 araddr → 读目标不变
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - AR 握手后改写 araddr 不改变读目标",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        // 前置：reg0 / reg1 写入互不相同的值
        ch_test::axi_lite::write_io(sim, io, kReg0, 0xAAAA0000u, true);
        ch_test::axi_lite::write_io(sim, io, kReg1, 0xBBBB1111u, true);

        // AR 握手（地址 = reg0）
        ar_handshake(sim, io, kReg0);

        // 握手后改写 araddr
        sim.set_input_value(io.araddr, kReg1);

        // rvalid 保持到 RREADY
        const bool r_seen = wait_for(sim, io.rvalid);
        REQUIRE(r_seen);
        REQUIRE(static_cast<std::uint32_t>(pv(sim, io.rdata)) == 0xAAAA0000u);

        sim.set_input_value(io.rready, 1);
        sim.tick();
        sim.set_input_value(io.rready, 0);
    });

// ============================================================================
// 用例 5：W 延后多个周期被接受
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - W 延后 3 拍仍被接受",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        aw_handshake(sim, io, kReg0);

        // 延迟 3 拍再送 W；期间再发一笔 AW（不同地址）→ MUST 被 busy 拒绝
        sim.set_input_value(io.awaddr, kReg1);
        sim.set_input_value(io.awvalid, 1);
        for (int i = 0; i < 3; ++i) sim.tick();
        sim.set_input_value(io.awvalid, 0);
        sim.set_input_value(io.awaddr, kReg0);

        w_handshake_and_b(sim, io, 0x12345678u, 0xF);
        REQUIRE(read_reg(sim, io, kReg0) == 0x12345678u);
        REQUIRE(read_reg(sim, io, kReg1) == 0u); // 第二笔 AW 被 busy 拒绝
    });

// ============================================================================
// 用例 6：同拍 AW+W → 该拍 W 不被接受；保持 WVALID 后下一拍被接受
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - 同拍 AW+W 时 W 不被接受，保持 WVALID 后下一拍被接受",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        // AW 与 W 同拍
        sim.set_input_value(io.awaddr, kReg0);
        sim.set_input_value(io.awvalid, 1);
        sim.set_input_value(io.wdata, 0xCAFEBABEu);
        sim.set_input_value(io.wstrb, 0xF);
        sim.set_input_value(io.wvalid, 1);
        sim.tick();

        // 该拍只完成 AW 握手 → W 未被接受 → 无 B 响应
        REQUIRE(pv(sim, io.bvalid) == 0);

        // master 保持 WVALID → 下一拍 W 被接受
        sim.tick();
        REQUIRE(pv(sim, io.bvalid) == 1);

        sim.set_input_value(io.awvalid, 0);
        sim.set_input_value(io.wvalid, 0);
        sim.set_input_value(io.bready, 1);
        sim.tick();
        sim.set_input_value(io.bready, 0);

        REQUIRE(read_reg(sim, io, kReg0) == 0xCAFEBABEu);
    });

// ============================================================================
// 用例 7：master 不拉 BREADY 时 bvalid 保持
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - 不拉 BREADY 时 bvalid 保持且不接受新事务",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        aw_handshake(sim, io, kReg0);
        sim.set_input_value(io.wdata, 0x0BADF00Du);
        sim.set_input_value(io.wstrb, 0xF);
        sim.set_input_value(io.wvalid, 1);
        sim.tick();
        sim.set_input_value(io.wvalid, 0);

        // 前置：W 已被接受
        REQUIRE(pv(sim, io.bvalid) == 1);

        // 不拉 BREADY，连续 3 拍；期间再发一笔 AW（不同地址）→ MUST 被 busy 拒绝
        sim.set_input_value(io.awaddr, kReg1);
        sim.set_input_value(io.awvalid, 1);
        for (int i = 0; i < 3; ++i) {
            sim.tick();
            REQUIRE(pv(sim, io.bvalid) == 1);
        }
        sim.set_input_value(io.awvalid, 0);
        sim.set_input_value(io.awaddr, kReg0);

        // B 握手 → bvalid 清除
        sim.set_input_value(io.bready, 1);
        sim.tick();
        REQUIRE(pv(sim, io.bvalid) == 0);
        sim.set_input_value(io.bready, 0);

        // 写必须落在 AW#1 锁存的 reg0（第二笔 AW 被 busy 拒绝 → 无写事务交叠）
        REQUIRE(read_reg(sim, io, kReg0) == 0x0BADF00Du);
        REQUIRE(read_reg(sim, io, kReg1) == 0u);
    });

// ============================================================================
// 用例 8：BREADY 握手后 bvalid 清除
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - BREADY 握手后 bvalid 清除",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        aw_handshake(sim, io, kReg0);
        sim.set_input_value(io.wdata, 0x5A5A5A5Au);
        sim.set_input_value(io.wstrb, 0xF);
        sim.set_input_value(io.wvalid, 1);
        sim.tick();
        sim.set_input_value(io.wvalid, 0);
        REQUIRE(pv(sim, io.bvalid) == 1);

        sim.set_input_value(io.bready, 1);
        sim.tick();
        REQUIRE(pv(sim, io.bvalid) == 0);
        sim.set_input_value(io.bready, 0);
    });

// ============================================================================
// 用例 9：bready 常态拉高时，busy 不得提前释放（不得接受第二笔 AW）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - bready 常态拉高时 busy 不得提前释放",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        // master 常态拉高 BREADY
        sim.set_input_value(io.bready, 1);

        // AW 拍：W 尚未到达 → 事务未完成 → busy MUST 保持（不得被裸 bready 释放）
        aw_handshake(sim, io, kReg0);
        REQUIRE(pv(sim, io.bvalid) == 0);

        // 连续两拍再发 AW（不同地址）：busy 保持时 MUST 被拒绝
        sim.set_input_value(io.awaddr, kReg1);
        sim.set_input_value(io.awvalid, 1);
        sim.tick();
        sim.tick();
        sim.set_input_value(io.awvalid, 0);

        // W 拍 → B 响应出现（写目标 MUST 仍是 AW#1 锁存的 reg0）
        sim.set_input_value(io.wdata, 0x1BADB002u);
        sim.set_input_value(io.wstrb, 0xF);
        sim.set_input_value(io.wvalid, 1);
        sim.tick();
        sim.set_input_value(io.wvalid, 0);
        REQUIRE(pv(sim, io.bvalid) == 1);
        REQUIRE(static_cast<std::uint32_t>(pv(sim, io.bresp)) == kRespOkay);

        // B 握手（bready 一直为高）→ busy 释放
        sim.tick();
        sim.set_input_value(io.bready, 0);

        REQUIRE(read_reg(sim, io, kReg0) == 0x1BADB002u);
        REQUIRE(read_reg(sim, io, kReg1) == 0u); // 第二笔 AW 被 busy 拒绝
    });

// ============================================================================
// 用例 10：master 不拉 RREADY 时 rvalid 保持
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - 不拉 RREADY 时 rvalid 保持且 rdata 稳定",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        ch_test::axi_lite::write_io(sim, io, kReg0, 0x0FF1CE00u, true);

        ar_handshake(sim, io, kReg0);
        REQUIRE(pv(sim, io.rvalid) == 1);
        const auto expected = static_cast<std::uint32_t>(pv(sim, io.rdata));

        for (int i = 0; i < 3; ++i) {
            sim.tick();
            REQUIRE(pv(sim, io.rvalid) == 1);
            REQUIRE(static_cast<std::uint32_t>(pv(sim, io.rdata)) == expected);
        }

        sim.set_input_value(io.rready, 1);
        sim.tick();
        sim.set_input_value(io.rready, 0);
    });

// ============================================================================
// 用例 11：RREADY 握手后 rvalid 清除
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - RREADY 握手后 rvalid 清除",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        ch_test::axi_lite::write_io(sim, io, kReg0, 0x0FF1CE11u, true);

        ar_handshake(sim, io, kReg0);
        REQUIRE(pv(sim, io.rvalid) == 1);

        sim.set_input_value(io.rready, 1);
        sim.tick();
        REQUIRE(pv(sim, io.rvalid) == 0);
        sim.set_input_value(io.rready, 0);
    });

// ============================================================================
// 用例 12：未映射地址写 → DECERR，寄存器不变
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - 未映射地址写返回 DECERR 且寄存器不变",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        // 前置：4 个寄存器写入互不相同的值
        ch_test::axi_lite::write_io(sim, io, kReg0, 0x11111111u, true);
        ch_test::axi_lite::write_io(sim, io, kReg1, 0x22222222u, true);
        ch_test::axi_lite::write_io(sim, io, kReg2, 0x33333333u, true);
        ch_test::axi_lite::write_io(sim, io, kReg3, 0x44444444u, true);

        // 写未映射地址
        aw_handshake(sim, io, kUnmapped);
        sim.set_input_value(io.wdata, 0xDEADBEEFu);
        sim.set_input_value(io.wstrb, 0xF);
        sim.set_input_value(io.wvalid, 1);
        sim.tick();
        sim.set_input_value(io.wvalid, 0);
        REQUIRE(pv(sim, io.bvalid) == 1);
        REQUIRE(static_cast<std::uint32_t>(pv(sim, io.bresp)) == kRespDecErr);

        sim.set_input_value(io.bready, 1);
        sim.tick();
        sim.set_input_value(io.bready, 0);

        REQUIRE(read_reg(sim, io, kReg0) == 0x11111111u);
        REQUIRE(read_reg(sim, io, kReg1) == 0x22222222u);
        REQUIRE(read_reg(sim, io, kReg2) == 0x33333333u);
        REQUIRE(read_reg(sim, io, kReg3) == 0x44444444u);
    });

// ============================================================================
// 用例 13：未映射地址读 → DECERR，rdata == 0
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - 未映射地址读返回 DECERR 且 rdata 为 0",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        ch_test::axi_lite::write_io(sim, io, kReg0, 0x11111111u, true);

        ar_handshake(sim, io, kUnmapped);
        REQUIRE(pv(sim, io.rvalid) == 1);
        REQUIRE(static_cast<std::uint32_t>(pv(sim, io.rresp)) == kRespDecErr);
        REQUIRE(static_cast<std::uint32_t>(pv(sim, io.rdata)) == 0u);

        sim.set_input_value(io.rready, 1);
        sim.tick();
        sim.set_input_value(io.rready, 0);
    });

// ============================================================================
// 用例 14：别名地址（0x100）→ DECERR，不得别名到 reg0
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - 别名地址 0x100 返回 DECERR 且不写 reg0",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        // 前置：reg0 写入非 0 值
        ch_test::axi_lite::write_io(sim, io, kReg0, 0xAAAAAAAAu, true);
        REQUIRE(read_reg(sim, io, kReg0) == 0xAAAAAAAAu);

        // 写别名地址 0x100（低位与 reg0 相同）
        aw_handshake(sim, io, kAliased);
        sim.set_input_value(io.wdata, 0xBBBBBBBBu);
        sim.set_input_value(io.wstrb, 0xF);
        sim.set_input_value(io.wvalid, 1);
        sim.tick();
        sim.set_input_value(io.wvalid, 0);
        REQUIRE(pv(sim, io.bvalid) == 1);
        REQUIRE(static_cast<std::uint32_t>(pv(sim, io.bresp)) == kRespDecErr);

        sim.set_input_value(io.bready, 1);
        sim.tick();
        sim.set_input_value(io.bready, 0);

        // reg0 MUST 未被别名写
        REQUIRE(read_reg(sim, io, kReg0) == 0xAAAAAAAAu);
    });

// ============================================================================
// 用例 15：空写（wstrb == 0）→ 响应仍为 OKAY，寄存器不变
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - 空写（wstrb == 0）响应为 OKAY 且寄存器不变",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
        sim.set_input_value(io.rst_n, 1);

        // 前置：目标寄存器写入非 0 值
        ch_test::axi_lite::write_io(sim, io, kReg0, 0x11223344u, true);

        // 空写：地址已解码，但无字节被选中
        aw_handshake(sim, io, kReg0);
        sim.set_input_value(io.wdata, 0xFFFFFFFFu);
        sim.set_input_value(io.wstrb, 0x0);
        sim.set_input_value(io.wvalid, 1);
        sim.tick();
        sim.set_input_value(io.wvalid, 0);
        REQUIRE(pv(sim, io.bvalid) == 1);
        REQUIRE(static_cast<std::uint32_t>(pv(sim, io.bresp)) == kRespOkay);

        sim.set_input_value(io.bready, 1);
        sim.tick();
        sim.set_input_value(io.bready, 0);

        REQUIRE(read_reg(sim, io, kReg0) == 0x11223344u);
    });

// ============================================================================
// 用例 1：rst_n 拉低 → 全部状态寄存器读回初值
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - rst_n 拉低后全部寄存器读回初值",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        // 前置：写入非 0 值
        ch_test::axi_lite::write_io(sim, io, kReg0, 0xDEADBEEFu, true);
        REQUIRE(read_reg(sim, io, kReg0) == 0xDEADBEEFu);

        // 拉低 rst_n 并经过至少 1 个时钟沿
        sim.set_input_value(io.rst_n, 0);
        sim.tick();
        sim.tick();

        // 复位生效后释放
        sim.set_input_value(io.rst_n, 1);
        sim.tick();

        // 全部寄存器读回初值
        REQUIRE(read_reg(sim, io, kReg0) == 0u);
        REQUIRE(read_reg(sim, io, kReg1) == 0u);
        REQUIRE(read_reg(sim, io, kReg2) == 0u);
        REQUIRE(read_reg(sim, io, kReg3) == 0u);
    });

// ============================================================================
// 用例 2：rst_n 提供逃生路径（打断进行中的事务）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - rst_n 可打断进行中的事务并释放 busy",
    "[axi][slave-conformance][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        // AW 握手后不送 W → 事务卡在「等 W」状态
        aw_handshake(sim, io, kReg0);
        REQUIRE(pv(sim, io.bvalid) == 0);

        // 复位打断
        sim.set_input_value(io.rst_n, 0);
        sim.tick();
        sim.set_input_value(io.rst_n, 1);
        sim.tick();

        // busy 已释放 → 下一笔事务能正常完成（写不进即说明 busy 仍卡住）
        ch_test::axi_lite::write_io(sim, io, kReg0, 0x0BADF00Du, true);
        REQUIRE(read_reg(sim, io, kReg0) == 0x0BADF00Du);
    });
