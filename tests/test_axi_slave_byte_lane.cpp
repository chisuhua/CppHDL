// tests/test_axi_slave_byte_lane.cpp
// 通用 AXI slave 的字节道（WSTRB）写语义回归测试（**双后端**：interpreter + verilator）
//
// 契约（openspec change `axi4-slave-byte-lane-2026-10`，语义登记于
// include/axi4/AGENTS.md CONVENTIONS → 字节道语义）：
//   * WSTRB 位 i ↔ 字节道 i，每道 8 bit；WSTRB 宽度 = DATA_WIDTH / 8
//   * 写路径：new = (old & ~mask) | (wdata & mask)
//   * WSTRB == 0 → 空写（寄存器保持原值）
//   * 覆盖通用 slave：`AxiLiteSlave`（32 / 64 bit 总线）与
//     `Axi4Slave`（32 / 64 bit 总线）
//
// ⚠️ Axi4Slave 的写状态机当前**每笔事务只接受 1 个 W beat**
// （`include/axi4/axi4_full.h` 中 `wready = (write_state == 1) && wvalid`）——
// 多 beat burst 属 `proposal.md` 的非目标，单独立项。
// 因此本文件的 Axi4Slave 用例验证的是**每次 W 握手用当拍 WSTRB**：
// AW 拍先给一个不同的 WSTRB，W 拍再改成掩码，断言用的是 W 拍的值
// （等价于"掩码 MUST NOT 在 AW 拍锁存"）。
//
// ⚠️ 两个通用 slave 都没有 `rst_n`、且响应是组合单拍（规范形态偏差，
// 属 proposal 非目标）—— 因此测试不等待 BVALID/RVALID，按已知时序推进，
// 断言只针对寄存器读回值。

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

#include "axi4/axi4_full.h"
#include "axi4/axi4_lite_slave.h"
#include "axi_lite_slave_fixture.h"
#include "ch.hpp"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;
using ch_test::axi_lite::pv;

// Typedefs to avoid preprocessor comma-split on DUAL_BACKEND_TEST_PAIR args.
using Lite32 = axi4::AxiLiteSlave<32, 32, 4>;
using Lite64 = axi4::AxiLiteSlave<32, 64, 4>;
using Full32 = axi4::Axi4Slave<32, 32, 4, 16, 16>;
using Full64 = axi4::Axi4Slave<32, 64, 4, 16, 16>;

namespace {

constexpr std::uint64_t kReg0 = 0x00;
constexpr std::uint64_t kReg1 = 0x04;

// ---------------------------------------------------------------------------
// AxiLiteSlave：AW 与 W 必须同拍握手（wready 由 awready 门控），
// bvalid 是组合单拍 → 不能用 "等 BVALID" 同步，按两拍推进：
//   拍 1：AW + W 握手（写提交，busy 置位）
//   拍 2：BREADY 释放 busy
// ---------------------------------------------------------------------------
template <typename Io>
inline void lite_write(ch::Simulator &sim, Io &io, std::uint64_t addr,
                       std::uint64_t data, std::uint64_t wstrb) {
    sim.set_input_value(io.awaddr, addr);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, data);
    sim.set_input_value(io.wstrb, wstrb);
    sim.set_input_value(io.wvalid, 1);
    sim.set_input_value(io.bready, 1);
    sim.tick(); // AW + W 握手，写提交
    sim.set_input_value(io.awvalid, 0);
    sim.set_input_value(io.wvalid, 0);
    sim.tick(); // BREADY 释放 busy
    sim.set_input_value(io.bready, 0);
}

template <typename Io>
inline std::uint64_t lite_read(ch::Simulator &sim, Io &io, std::uint64_t addr) {
    sim.set_input_value(io.araddr, addr);
    sim.set_input_value(io.arvalid, 1);
    sim.set_input_value(io.rready, 1);
    sim.tick(); // AR 握手；rdata 组合自 araddr
    const std::uint64_t value = pv(sim, io.rdata);
    sim.set_input_value(io.arvalid, 0);
    sim.tick(); // RREADY 释放 busy
    sim.set_input_value(io.rready, 0);
    return value;
}

// ---------------------------------------------------------------------------
// Axi4Slave：AW 拍只握手地址；W 拍才接受数据（wready 由 write_state==1 门控）；
// 第 3 拍给 BREADY 完成 B 响应并回到 idle。
//
// `wstrb_at_aw` 显式传一个与 W 拍不同的值 → 断言写用的是 **W 拍** 的 WSTRB。
// ---------------------------------------------------------------------------
template <typename Io>
inline void full_write(ch::Simulator &sim, Io &io, std::uint64_t addr,
                       std::uint64_t data, std::uint64_t wstrb,
                       std::uint64_t wstrb_at_aw = 0) {
    // --- AW 拍 ---
    sim.set_input_value(io.awaddr, addr);
    sim.set_input_value(io.awlen, 0);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wstrb, wstrb_at_aw); // 故意与 W 拍不同
    sim.set_input_value(io.wvalid, 0);
    sim.set_input_value(io.bready, 0);
    sim.tick();
    sim.set_input_value(io.awvalid, 0);

    // --- W 拍（当拍 WSTRB 生效）---
    sim.set_input_value(io.wdata, data);
    sim.set_input_value(io.wstrb, wstrb);
    sim.set_input_value(io.wlast, 1);
    sim.set_input_value(io.wvalid, 1);
    sim.tick();
    sim.set_input_value(io.wvalid, 0);
    sim.set_input_value(io.wlast, 0);

    // --- B 拍 ---
    sim.set_input_value(io.bready, 1);
    sim.tick();
    sim.set_input_value(io.bready, 0);
}

template <typename Io>
inline std::uint64_t full_read(ch::Simulator &sim, Io &io, std::uint64_t addr) {
    sim.set_input_value(io.araddr, addr);
    sim.set_input_value(io.arlen, 0);
    sim.set_input_value(io.arvalid, 1);
    sim.set_input_value(io.rready, 0);
    sim.tick(); // AR 握手 → read_state = R_DATA
    sim.set_input_value(io.arvalid, 0);
    const std::uint64_t value = pv(sim, io.rdata);
    sim.set_input_value(io.rready, 1);
    sim.tick(); // R 握手 → idle
    sim.set_input_value(io.rready, 0);
    return value;
}

} // namespace

// ============================================================================
// AxiLiteSlave（32 bit 总线）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - 部分字节写只改选中字节",
    "[axi][byte-lane][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();

        lite_write(sim, io, kReg0, 0xAABBCCDDu, 0xF);
        REQUIRE(lite_read(sim, io, kReg0) == 0xAABBCCDDu);

        // 只写 byte0
        lite_write(sim, io, kReg0, 0x00000011u, 0x1);
        REQUIRE(lite_read(sim, io, kReg0) == 0xAABBCC11u);

        // 只写 byte1
        lite_write(sim, io, kReg0, 0x00002200u, 0x2);
        REQUIRE(lite_read(sim, io, kReg0) == 0xAABB2211u);

        // 只写 byte3
        lite_write(sim, io, kReg0, 0x44000000u, 0x8);
        REQUIRE(lite_read(sim, io, kReg0) == 0x44BB2211u);
    });

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - WSTRB == 0 是空写，且不影响其它寄存器",
    "[axi][byte-lane][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();

        lite_write(sim, io, kReg0, 0x11223344u, 0xF);
        lite_write(sim, io, kReg1, 0x55667788u, 0xF);

        // 空写：地址已解码，但无字节被选中 → 两个寄存器都不变
        lite_write(sim, io, kReg0, 0xFFFFFFFFu, 0x0);
        REQUIRE(lite_read(sim, io, kReg0) == 0x11223344u);
        REQUIRE(lite_read(sim, io, kReg1) == 0x55667788u);

        // 部分写只落在 kReg0
        lite_write(sim, io, kReg0, 0x0000ABCDu, 0x3);
        REQUIRE(lite_read(sim, io, kReg0) == 0x1122ABCDu);
        REQUIRE(lite_read(sim, io, kReg1) == 0x55667788u);
    });

// ============================================================================
// AxiLiteSlave（64 bit 总线 —— byte_lane_mask<64> 展开 8 条字节道）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite64,
    "AxiLiteSlave - 64 bit 总线按 8 条字节道掩码",
    "[axi][byte-lane][lite][dual]",
    [&]() {
        auto &io = dev.instance().io();

        lite_write(sim, io, kReg0, 0x1122334455667788ull, 0xFF);
        REQUIRE(lite_read(sim, io, kReg0) == 0x1122334455667788ull);

        // 低 4 条道（byte0..3）
        lite_write(sim, io, kReg0, 0xAABBCCDDu, 0x0F);
        REQUIRE(lite_read(sim, io, kReg0) == 0x11223344AABBCCDDull);

        // 高 4 条道（byte4..7）
        lite_write(sim, io, kReg0, 0xEEFF000000000000ull, 0xF0);
        REQUIRE(lite_read(sim, io, kReg0) == 0xEEFF0000AABBCCDDull);

        // 只写 byte7（最高道）
        lite_write(sim, io, kReg0, 0x7700000000000000ull, 0x80);
        REQUIRE(lite_read(sim, io, kReg0) == 0x77FF0000AABBCCDDull);

        // 空写
        lite_write(sim, io, kReg0, 0xFFFFFFFFFFFFFFFFull, 0x00);
        REQUIRE(lite_read(sim, io, kReg0) == 0x77FF0000AABBCCDDull);
    });

// ============================================================================
// Axi4Slave（32 bit 总线）：WSTRB 在**每次 W 握手**取当拍值，不在 AW 拍锁存
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave - 每次 W 握手用当拍 WSTRB（AW 拍的值不得锁存）",
    "[axi][byte-lane][full][dual]",
    [&]() {
        auto &io = dev.instance().io();

        full_write(sim, io, kReg0, 0xAABBCCDDu, 0xF);
        REQUIRE(full_read(sim, io, kReg0) == 0xAABBCCDDu);

        // AW 拍给 0xF，W 拍只给 byte0 → 若掩码在 AW 拍锁存就会写成整字
        full_write(sim, io, kReg0, 0x00000011u, 0x1, 0xF);
        REQUIRE(full_read(sim, io, kReg0) == 0xAABBCC11u);

        // AW 拍给 0x0（空写），W 拍给 byte1 → 若锁存则会变成空写
        full_write(sim, io, kReg0, 0x00002200u, 0x2, 0x0);
        REQUIRE(full_read(sim, io, kReg0) == 0xAABB2211u);

        // W 拍空写 → 寄存器不变
        full_write(sim, io, kReg0, 0xFFFFFFFFu, 0x0, 0xF);
        REQUIRE(full_read(sim, io, kReg0) == 0xAABB2211u);
    });

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave - 字节写不串扰其它寄存器",
    "[axi][byte-lane][full][dual]",
    [&]() {
        auto &io = dev.instance().io();

        full_write(sim, io, kReg0, 0x11223344u, 0xF);
        full_write(sim, io, kReg1, 0x55667788u, 0xF);

        // reg0 只写 byte2
        full_write(sim, io, kReg0, 0x00AB0000u, 0x4, 0xF);
        REQUIRE(full_read(sim, io, kReg0) == 0x11AB3344u);
        REQUIRE(full_read(sim, io, kReg1) == 0x55667788u);
    });

// ============================================================================
// Axi4Slave（64 bit 总线）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full64,
    "Axi4Slave - 64 bit 总线按 8 条字节道掩码",
    "[axi][byte-lane][full][dual]",
    [&]() {
        auto &io = dev.instance().io();

        full_write(sim, io, kReg0, 0x1122334455667788ull, 0xFF);
        REQUIRE(full_read(sim, io, kReg0) == 0x1122334455667788ull);

        // W 拍只选低 4 道（AW 拍给 0xFF 证明不锁存）
        full_write(sim, io, kReg0, 0xAABBCCDDull, 0x0F, 0xFF);
        REQUIRE(full_read(sim, io, kReg0) == 0x11223344AABBCCDDull);

        // W 拍只选 byte7
        full_write(sim, io, kReg0, 0x7700000000000000ull, 0x80, 0x00);
        REQUIRE(full_read(sim, io, kReg0) == 0x77223344AABBCCDDull);
    });
