// tests/test_axi_rst_n_guard.cpp
// 「未接线 `rst_n` = 永久复位」的**可执行**护栏（**双后端**：interpreter + verilator）
//
// 背景（`include/axi4/AGENTS.md` CONVENTIONS → 复位）：
// `rst_n` 是 AXI slave 的**必需端口**。未接线的 `ch_in<ch_bool>` 读出为 0 →
// **永久复位**。这是**零编译错误**的静默破坏：组件编译通过、测试二进制构建通过、
// 仿真不报任何错，但**全部寄存器停在初值**（写不进去、读全 0）→
// 依赖它的测试用例**静默空转**。
//
// 本文件把这条约定钉成可执行契约：**故意不驱动 `rst_n`**，断言
//   * 写事务**不产生** B 响应（`bvalid` 恒 0）
//   * 目标寄存器**保持初值**（读回 0）
//   * 读事务**不产生** R 响应（`rvalid` 恒 0，`rdata` 恒 0）
//
// ⚠️ 静态护栏（`scripts/verify_rst_n_wiring.sh`）负责「别忘了接线」，
//    本文件负责「接错了会怎样」——两者互补。

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

#include "axi4/axi4_full.h"
#include "axi4/axi4_lite_slave.h"
#include "ch.hpp"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

using Lite32 = axi4::AxiLiteSlave<32, 32, 4>;
using Full32 = axi4::Axi4Slave<32, 32, 4, 16, 16>;

namespace {

// 复位态下的行为断言（对 Lite / Full 两个通用 slave 都适用）。
//
// ⚠️ 这里**故意不驱动 `rst_n`** —— 未接线 = 永久复位。
//    推进用固定拍数（不是「等 ready 变 1」的无界循环）：复位态下
//    `bvalid` / `rvalid` **永不**出现，无界等待会 hang 到 ctest TIMEOUT。
constexpr int kSettleTicks = 8;

template <typename Io>
inline void drive_write_without_reset(ch::Simulator &sim, Io &io,
                                       std::uint64_t addr,
                                       std::uint64_t data) {
    // --- AW 拍 ---
    sim.set_input_value(io.awaddr, addr);
    sim.set_input_value(io.awprot, 0);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wvalid, 0);
    sim.tick();
    sim.set_input_value(io.awvalid, 0);

    // --- W 拍 ---
    sim.set_input_value(io.wdata, data);
    sim.set_input_value(io.wstrb, 0xF);
    sim.set_input_value(io.wvalid, 1);
    sim.tick();
    sim.set_input_value(io.wvalid, 0);

    for (int i = 0; i < kSettleTicks; ++i) {
        sim.tick();
    }
}

template <typename Io>
inline void drive_read_without_reset(ch::Simulator &sim, Io &io,
                                      std::uint64_t addr) {
    sim.set_input_value(io.araddr, addr);
    sim.set_input_value(io.arprot, 0);
    sim.set_input_value(io.arvalid, 1);
    sim.set_input_value(io.rready, 0);
    sim.tick();
    sim.set_input_value(io.arvalid, 0);

    for (int i = 0; i < kSettleTicks; ++i) {
        sim.tick();
    }
}

} // namespace

// ============================================================================
// 通用 AXI4-Lite slave：未接线 rst_n → 永久复位
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Lite32,
    "AxiLiteSlave - unwired rst_n keeps the slave permanently in reset",
    "[axi][rst-n][guard][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // ⚠️ 故意**不驱动** `rst_n`（未接线 = 永久复位）

        drive_write_without_reset(sim, io, 0x00, 0xDEADBEEF);

        // 永久复位 → 永不产生 B 响应
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.bvalid)) == 0);

        drive_read_without_reset(sim, io, 0x00);

        // 永久复位 → 永不产生 R 响应，且数据恒为初值
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rvalid)) == 0);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rdata)) == 0);
    });

// ============================================================================
// 通用 AXI4 Full slave：未接线 rst_n → 永久复位
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave - unwired rst_n keeps the slave permanently in reset",
    "[axi][rst-n][guard][dual]",
    [&]() {
        auto &io = dev.instance().io();
        // ⚠️ 故意**不驱动** `rst_n`（未接线 = 永久复位）

        drive_write_without_reset(sim, io, 0x00, 0xDEADBEEF);

        // 永久复位 → 永不产生 B 响应
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.bvalid)) == 0);

        drive_read_without_reset(sim, io, 0x00);

        // 永久复位 → 永不产生 R 响应，且数据恒为初值
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rvalid)) == 0);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rdata)) == 0);
    });
