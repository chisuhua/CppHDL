// tests/test_axi4_full_burst.cpp
// 通用 AXI4 Full slave（`include/axi4/axi4_full.h` 的 `Axi4Slave`）的
// **多 beat burst 语义**回归测试（**双后端**：interpreter + verilator）
//
// 契约（openspec capability `axi4-full-slave-burst`，
//       openspec/specs/axi4-full-slave-burst/spec.md）：
//   * `rst_n`：低有效复位，**在时钟沿生效** → 实例化点 MUST 驱动
//   * AW→W **相位分离**：AW 握手与 W beat 分属不同周期
//   * 地址**锁存**：AW/AR 握手时锁存，解码基于锁存地址
//   * `bvalid` 保持到 `bready`；`rvalid` 在整个 burst 期间保持为 1（流式）
//   * 写 / 读方向**各自独立** outstanding（无共享 busy）
//   * `awlen` / `arlen` 是「beat 数 − 1」→ 一笔事务 = `len + 1` 个 beat
//   * `INCR`：每 beat 地址前进 `2^awsize` 字节；`FIXED`：地址不变
//   * `WRAP`：以 `SLVERR` 明确拒绝（MUST NOT 静默当作 INCR）
//   * 未映射地址 → `DECERR`；burst 长度不符 / 超 `MAX_BURST_LEN` → `SLVERR`
//
// ⚠️ master stimulus 的纪律（对齐 tests/axi_lite_slave_fixture.h）：
//   * AWVALID 只保持到 AW 握手；WVALID 保持到 W 握手
//   * 每一拍的数据 / wstrb / wlast 在被接受前保持稳定
//   * 所有握手等待都是**有界**循环（超时即 FAIL）——
//     「握手永不完成」类缺陷的表现是 hang 而非断言失败
//   * BREADY / RREADY 由 master 自行拉高（不用从设备的 ready gate 自己的 valid）

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

#include "axi4/axi4_full.h"
#include "axi_lite_slave_fixture.h"
#include "ch.hpp"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>
#include <utility>
#include <vector>

using namespace ch;
using namespace ch::core;
using ch_test::axi_lite::pv;

using Full32 = axi4::Axi4Slave<32, 32, 4, 16, 16>;
using Full32Max4 = axi4::Axi4Slave<32, 32, 4, 16, 4>;
using Full64 = axi4::Axi4Slave<32, 64, 4, 16, 16>;

namespace {

// AXI burst 编码（`AxiBurst`）
constexpr std::uint64_t kBurstFixed = 0;
constexpr std::uint64_t kBurstIncr = 1;
constexpr std::uint64_t kBurstWrap = 2;

// 响应编码（`AxiResp`）
constexpr std::uint64_t kRespOkay = 0;
constexpr std::uint64_t kRespSlverr = 2;
constexpr std::uint64_t kRespDecerr = 3;

constexpr std::uint64_t kMaxCycles = 64;

// 有界等待组合信号拉高（期间调用方的输入保持不变）
template <typename Port>
inline bool wait_high(ch::Simulator &sim, const Port &port,
                      std::uint64_t max_cycles = kMaxCycles) {
    for (std::uint64_t i = 0; i < max_cycles; ++i) {
        if (pv(sim, port) == 1) {
            return true;
        }
        sim.tick();
    }
    return pv(sim, port) == 1;
}

// AXI4 写事务（支持多 beat burst）。
// 返回 B 响应的 `bresp`。
//
// 时序模型（CppHDL Simulator）：
//   `sim.tick()` = 组合求值 → 时钟沿更新寄存器 → 再组合求值。
//   组合信号只在 tick 之间可观测；tick 结束后读到的是**下一周期**的值。
template <typename Io>
inline std::uint64_t axi4_write_burst(ch::Simulator &sim, Io &io,
                                       std::uint64_t addr, std::uint64_t len,
                                       std::uint64_t size,
                                       std::uint64_t burst,
                                       std::uint64_t id,
                                       const std::vector<std::uint64_t> &data,
                                       const std::vector<std::uint8_t> &strb,
                                       const std::vector<std::uint8_t> &wlast_flags = {}) {
    const std::size_t beats = data.size();

    // ---------------- AW 拍 ----------------
    sim.set_input_value(io.awaddr, addr);
    sim.set_input_value(io.awid, id);
    sim.set_input_value(io.awlen, len);
    sim.set_input_value(io.awsize, size);
    sim.set_input_value(io.awburst, burst);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wvalid, 0);
    sim.tick(); // AW 握手发生在本次时钟沿
    sim.set_input_value(io.awvalid, 0);

    // ---------------- W beats ----------------
    // ⚠️ slave 在 `aw_done == 1` 且 burst 未结束期间**每拍接受一个 beat**；
    //    burst 结束（wlast 到达 / beat 数用尽）后 `wready` 撤销 →
    //    **不再接受额外的 beat**（否则越界 beat 会静默写入寄存器）。
    //    因此「某拍确实被接受」由**数据断言**（寄存器内容）验证，
    //    这里只做固定拍数的推进（不会挂死：拍数是有限的）。
    for (std::size_t i = 0; i < beats; ++i) {
        sim.set_input_value(io.wdata, data[i]);
        sim.set_input_value(io.wstrb,
                            strb.empty() ? std::uint64_t{0xF}
                                          : std::uint64_t{strb[i]});
        // wlast_flags 为空 → 末拍带 wlast；否则逐拍给定（用于构造
        // 「wlast 提前」/「wlast 缺失」两类协议错误）
        const bool last = wlast_flags.empty() ? (i + 1 == beats)
                                              : (wlast_flags[i] != 0);
        sim.set_input_value(io.wlast, last ? 1 : 0);
        sim.set_input_value(io.wvalid, 1);
        sim.tick();
    }
    sim.set_input_value(io.wvalid, 0);
    sim.set_input_value(io.wlast, 0);

    // ---------------- B 拍 ----------------
    // bvalid 保持到 bready → master 看到后才拉 BREADY
    REQUIRE(wait_high(sim, io.bvalid));
    const std::uint64_t bresp = pv(sim, io.bresp);
    sim.set_input_value(io.bready, 1);
    sim.tick(); // B 握手 → 写事务结束
    sim.set_input_value(io.bready, 0);
    return bresp;
}

// AXI4 读事务（支持多 beat burst）。
// 返回每个 beat 的 (rdata, rlast)。
template <typename Io>
inline std::vector<std::pair<std::uint64_t, bool>>
axi4_read_burst(ch::Simulator &sim, Io &io, std::uint64_t addr,
                std::uint64_t len, std::uint64_t size, std::uint64_t burst,
                std::uint64_t id, std::size_t max_beats) {
    // ---------------- AR 拍 ----------------
    sim.set_input_value(io.araddr, addr);
    sim.set_input_value(io.arid, id);
    sim.set_input_value(io.arlen, len);
    sim.set_input_value(io.arsize, size);
    sim.set_input_value(io.arburst, burst);
    sim.set_input_value(io.arvalid, 1);
    sim.set_input_value(io.rready, 0);
    sim.tick(); // AR 握手发生在本次时钟沿
    sim.set_input_value(io.arvalid, 0);

    // ---------------- R beats（流式：rvalid 整个 burst 期间保持为 1）----------------
    std::vector<std::pair<std::uint64_t, bool>> out;
    sim.set_input_value(io.rready, 1);
    for (std::size_t i = 0; i < max_beats; ++i) {
        REQUIRE(wait_high(sim, io.rvalid));
        out.emplace_back(pv(sim, io.rdata), pv(sim, io.rlast) == 1);
        sim.tick(); // R 握手 → 下一个 beat
        if (out.back().second) {
            break; // rlast：末拍
        }
    }
    sim.set_input_value(io.rready, 0);
    return out;
}

// 单 beat 读（读回一个寄存器）
template <typename Io>
inline std::uint64_t axi4_read_reg(ch::Simulator &sim, Io &io,
                                    std::uint64_t addr) {
    auto beats = axi4_read_burst(sim, io, addr, 0, 2, kBurstIncr, 0, 1);
    REQUIRE(beats.size() == 1);
    return beats.front().first;
}

} // namespace

// ============================================================================
// 1. 4-beat INCR 写 → 4 个**不同**寄存器（数据断言，非仅握手断言）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave burst - 4-beat INCR write lands in 4 different registers",
    "[axi][axi4][burst][write][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        const std::uint64_t bresp = axi4_write_burst(
            sim, io, /*addr=*/0x00, /*len=*/3, /*size=*/2, kBurstIncr, /*id=*/7,
            {0x11, 0x22, 0x33, 0x44}, {0xF, 0xF, 0xF, 0xF});
        REQUIRE(bresp == kRespOkay);

        REQUIRE(axi4_read_reg(sim, io, 0x00) == 0x11);
        REQUIRE(axi4_read_reg(sim, io, 0x04) == 0x22);
        REQUIRE(axi4_read_reg(sim, io, 0x08) == 0x33);
        REQUIRE(axi4_read_reg(sim, io, 0x0C) == 0x44);
    });

// ============================================================================
// 2. 4-beat INCR 读 → 4 个不同数据被依次读回，rlast 只在末拍
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave burst - 4-beat INCR read returns 4 different values",
    "[axi][axi4][burst][read][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        axi4_write_burst(sim, io, 0x00, 3, 2, kBurstIncr, 0,
                         {0xA1, 0xA2, 0xA3, 0xA4}, {0xF, 0xF, 0xF, 0xF});

        auto beats = axi4_read_burst(sim, io, 0x00, 3, 2, kBurstIncr, 5, 4);
        REQUIRE(beats.size() == 4);
        REQUIRE(beats[0].first == 0xA1);
        REQUIRE(beats[1].first == 0xA2);
        REQUIRE(beats[2].first == 0xA3);
        REQUIRE(beats[3].first == 0xA4);
        // rlast 只在末拍
        REQUIRE(beats[0].second == false);
        REQUIRE(beats[1].second == false);
        REQUIRE(beats[2].second == false);
        REQUIRE(beats[3].second == true);
    });

// ============================================================================
// 3. FIXED burst 4 beat → 全部写同一个寄存器
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave burst - FIXED burst writes the same register 4 times",
    "[axi][axi4][burst][write][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        const std::uint64_t bresp = axi4_write_burst(
            sim, io, 0x04, 3, 2, kBurstFixed, 0, {0xB1, 0xB2, 0xB3, 0xB4},
            {0xF, 0xF, 0xF, 0xF});
        REQUIRE(bresp == kRespOkay);

        // 4 个 beat 全部落在 reg1，最后一拍胜出
        REQUIRE(axi4_read_reg(sim, io, 0x04) == 0xB4);
        // 相邻寄存器未被触碰
        REQUIRE(axi4_read_reg(sim, io, 0x00) == 0x00);
        REQUIRE(axi4_read_reg(sim, io, 0x08) == 0x00);
    });

// ============================================================================
// 4. 逐 beat wstrb → 每拍用当拍掩码
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave burst - per-beat wstrb is applied on every beat",
    "[axi][axi4][burst][write][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        // FIXED burst（4 beat 落同一个寄存器）+ 每拍不同的 wstrb：
        //   beat0: wstrb=0xF data=0x11223344 → 0x11223344
        //   beat1: wstrb=0x1 data=0x55       → 0x11223355
        //   beat2: wstrb=0x2 data=0x6600     → 0x11226655
        //   beat3: wstrb=0x8 data=0x77000000 → 0x77226655
        // AW 拍的 wstrb 从未被驱动（= 0）→ 若掩码在 AW 拍锁存则结果完全不同。
        const std::uint64_t bresp = axi4_write_burst(
            sim, io, 0x00, 3, 2, kBurstFixed, 0,
            {0x11223344ull, 0x55ull, 0x6600ull, 0x77000000ull},
            {0xF, 0x1, 0x2, 0x8});
        REQUIRE(bresp == kRespOkay);

        REQUIRE(axi4_read_reg(sim, io, 0x00) == 0x77226655ull);
    });

// ============================================================================
// 5. burst 长度超 MAX_BURST_LEN → SLVERR（不静默截断）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32Max4,
    "Axi4Slave burst - burst longer than MAX_BURST_LEN ends with SLVERR",
    "[axi][axi4][burst][error][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        // MAX_BURST_LEN = 4 → awlen=4（5 beat）超限
        const std::uint64_t bresp = axi4_write_burst(
            sim, io, 0x00, 4, 2, kBurstIncr, 0,
            {0x01, 0x02, 0x03, 0x04, 0x05}, {0xF, 0xF, 0xF, 0xF, 0xF});
        REQUIRE(bresp == kRespSlverr);
    });

// ============================================================================
// 6. WRAP burst → SLVERR 明确拒绝（不静默当作 INCR）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave burst - WRAP burst is rejected with SLVERR",
    "[axi][axi4][burst][error][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        const std::uint64_t bresp = axi4_write_burst(
            sim, io, 0x00, 3, 2, kBurstWrap, 0, {0x01, 0x02, 0x03, 0x04},
            {0xF, 0xF, 0xF, 0xF});
        REQUIRE(bresp == kRespSlverr);
    });

// ============================================================================
// 7. 未映射地址 → DECERR（不静默丢弃写）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave burst - unmapped address returns DECERR",
    "[axi][axi4][burst][error][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        // 0x100 落在 16 个寄存器（0x00..0x3C）之外
        const std::uint64_t bresp = axi4_write_burst(
            sim, io, 0x100, 0, 2, kBurstIncr, 0, {0xDEAD}, {0xF});
        REQUIRE(bresp == kRespDecerr);

        // 已实现寄存器保持原值
        REQUIRE(axi4_read_reg(sim, io, 0x00) == 0x00);
    });

// ============================================================================
// 8. rst_n 复位清零全部状态
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave burst - rst_n clears all state",
    "[axi][axi4][burst][reset][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        axi4_write_burst(sim, io, 0x00, 0, 2, kBurstIncr, 0, {0x5A5A}, {0xF});
        REQUIRE(axi4_read_reg(sim, io, 0x00) == 0x5A5A);

        // 拉低 rst_n 一个时钟沿 → 全部状态寄存器回到初值
        sim.set_input_value(io.rst_n, 0);
        sim.tick();
        sim.set_input_value(io.rst_n, 1);
        sim.tick();

        REQUIRE(axi4_read_reg(sim, io, 0x00) == 0x00);

        // 复位后仍能接受新事务（状态机回到空闲）
        const std::uint64_t bresp = axi4_write_burst(
            sim, io, 0x08, 0, 2, kBurstIncr, 0, {0x1234}, {0xF});
        REQUIRE(bresp == kRespOkay);
        REQUIRE(axi4_read_reg(sim, io, 0x08) == 0x1234);
    });

// ============================================================================
// 10. 流式读形态：`rlast` 在末拍**停留在总线上**时就拉高
//     （master 拉低 `rready` 时末拍仍在总线上 → `rlast` MUST 已经是 1；
//      若用「下一值」形式判定，`rready == 0` 的周期会给出错误的 `rlast = 0`）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave burst - rlast is asserted while the last beat is held on the bus",
    "[axi][axi4][burst][read][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        // reg0 = 0xC0DE, reg1 = 0xFEED
        axi4_write_burst(sim, io, 0x00, 0, 2, kBurstIncr, 0, {0xC0DE}, {0xF});
        axi4_write_burst(sim, io, 0x04, 0, 2, kBurstIncr, 0, {0xFEED}, {0xF});

        // AR：arlen=1 → 2 beat
        sim.set_input_value(io.araddr, 0x00);
        sim.set_input_value(io.arlen, 1);
        sim.set_input_value(io.arsize, 2);
        sim.set_input_value(io.arburst, kBurstIncr);
        sim.set_input_value(io.arvalid, 1);
        sim.set_input_value(io.rready, 0);
        sim.tick(); // AR 握手
        sim.set_input_value(io.arvalid, 0);

        // beat 0 呈现（rready=0 → 停在总线上）
        REQUIRE(wait_high(sim, io.rvalid));
        REQUIRE(pv(sim, io.rdata) == 0xC0DE);
        REQUIRE(pv(sim, io.rlast) == 0);

        // 接受 beat 0 → 末拍（beat 1）呈现
        sim.set_input_value(io.rready, 1);
        sim.tick();
        sim.set_input_value(io.rready, 0);

        // 末拍停留在总线上（master 尚未接受）→ rlast MUST 已拉高
        REQUIRE(pv(sim, io.rvalid) == 1);
        REQUIRE(pv(sim, io.rdata) == 0xFEED);
        REQUIRE(pv(sim, io.rlast) == 1);
    });

// ============================================================================
// 11. `wlast` 与 `awlen` 不符 → SLVERR（提前 / 缺失两种）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full32,
    "Axi4Slave burst - wlast mismatch ends with SLVERR",
    "[axi][axi4][burst][error][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        // 声明 4 beat（awlen=3），但第 2 拍就拉 wlast → 提前 → SLVERR
        std::uint64_t bresp = axi4_write_burst(
            sim, io, 0x00, 3, 2, kBurstIncr, 0, {0x01, 0x02, 0x03, 0x04},
            {0xF, 0xF, 0xF, 0xF}, {0, 1, 0, 0});
        REQUIRE(bresp == kRespSlverr);

        // 已写入的 beat MUST NOT 被回滚（部分写入 + 错误响应是 AXI 允许的行为）
        REQUIRE(axi4_read_reg(sim, io, 0x00) == 0x01);

        // 声明 4 beat（awlen=3），但 4 拍都不带 wlast → 缺失 → SLVERR
        bresp = axi4_write_burst(sim, io, 0x10, 3, 2, kBurstIncr, 0,
                                  {0x11, 0x12, 0x13, 0x14},
                                  {0xF, 0xF, 0xF, 0xF}, {0, 0, 0, 0});
        REQUIRE(bresp == kRespSlverr);
        REQUIRE(axi4_read_reg(sim, io, 0x10) == 0x11);
    });

// ============================================================================
// 9. 64 bit 总线的 4-beat INCR 写
//    ⚠️ 64 bit 总线上 4 字节步长（awsize=2）会让相邻 beat 落到**同一个**
//       64 位寄存器（窄传输的字节道重编号未实现）→ 本用例用 awsize=3
//       （8 字节步长）→ beat 落在 reg0 / reg2 / reg4 / reg6。
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    Full64,
    "Axi4Slave burst - 64-bit bus 4-beat INCR write",
    "[axi][axi4][burst][write][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst_n, 1);

        const std::uint64_t bresp = axi4_write_burst(
            sim, io, 0x00, 3, 3, kBurstIncr, 0,
            {0x1111111111111111ull, 0x2222222222222222ull, 0x3333333333333333ull,
             0x4444444444444444ull},
            {0xFF, 0xFF, 0xFF, 0xFF});
        REQUIRE(bresp == kRespOkay);

        REQUIRE(axi4_read_reg(sim, io, 0x00) == 0x1111111111111111ull);
        REQUIRE(axi4_read_reg(sim, io, 0x08) == 0x2222222222222222ull);
        REQUIRE(axi4_read_reg(sim, io, 0x10) == 0x3333333333333333ull);
        REQUIRE(axi4_read_reg(sim, io, 0x18) == 0x4444444444444444ull);
    });
