// tests/test_axi_lite_crossbar.cpp
//
// `AxiLiteCrossbar<ADDR, DATA, Map>`（2 master × 6 slave）的单元测试 —— 双后端
// （interpreter + verilator）。
//
// 契约来源：`openspec/changes/rv32i-soc-peripherals-interconnect-2026-11/
//            specs/axi-interconnect-routing/spec.md`（ADDED ×5）
//
// DUT 形态：**testbench 同时扮演 master 与 slave**（与 2x2 矩阵测试同型）。
//   * master 侧：驱动 `m*_awvalid/wvalid/bready/arvalid/rready`，观测
//     `m*_awready/wready/bvalid/bresp/arready/rvalid/rdata/rresp`。
//   * slave 侧：驱动 `s*_awready/wready/bvalid/bresp/arready/rvalid/rdata/rresp`，
//     观测 `s*_awaddr/awvalid/wdata/wstrb/wvalid/bready/araddr/arvalid/rready`。
//   ⇒ MUST NOT 实例化任何 AXI slave 当 fixture。
//
// 超时纪律：所有握手等待都是**有界循环**（≤ 64 拍）+ 超时即 FAIL。
//
// 地址映射（`CrossTestMap`，7 个 4 KiB 窗口）：
//   s0..s5 = 0x0000..0x5000 · 0x6000 → `axi4::kErr` · 0x8000_0000 完全未映射
//   ⇒ 两条 DECERR 路径都可测（ERR 窗口 / 未映射）。

#include "axi4/axi_lite_crossbar.h"

#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

namespace ch_test::axil_crossbar {

using Sim = ch::Simulator;

// ---------------------------------------------------------------------------
// 测试用地址映射：6 个真实窗口 + 1 个 ERR 窗口；其余地址未映射
// ---------------------------------------------------------------------------
constexpr uint32_t kSlaveBase[6] = {0x0000'0000u, 0x0000'1000u, 0x0000'2000u,
                                    0x0000'3000u, 0x0000'4000u, 0x0000'5000u};
constexpr uint32_t kErrWindowAddr = 0x0000'6000u;
constexpr uint32_t kUnmappedAddr = 0x8000'0000u;

struct CrossTestMap {
    static constexpr axi4::AxiLiteMapWindow windows[7] = {
        {0x0000'0000ull, 0x0000'1000ull, 0u},
        {0x0000'1000ull, 0x0000'1000ull, 1u},
        {0x0000'2000ull, 0x0000'1000ull, 2u},
        {0x0000'3000ull, 0x0000'1000ull, 3u},
        {0x0000'4000ull, 0x0000'1000ull, 4u},
        {0x0000'5000ull, 0x0000'1000ull, 5u},
        {0x0000'6000ull, 0x0000'1000ull, axi4::kErr},
    };
};

using Dut = axi4::AxiLiteCrossbar<CrossTestMap>;

// ---------------------------------------------------------------------------
// 基础设施
// ---------------------------------------------------------------------------
inline uint64_t pv(Sim &sim, const auto &port) {
    return static_cast<uint64_t>(sim.get_port_value(port));
}

template <typename P>
inline void set2(Sim &sim, int i, const P &a, const P &b, uint64_t v) {
    if (i == 0) sim.set_input_value(a, v);
    else sim.set_input_value(b, v);
}

template <typename P>
inline uint64_t get2(Sim &sim, int i, const P &a, const P &b) {
    return (i == 0) ? pv(sim, a) : pv(sim, b);
}

template <typename P>
inline void set6(Sim &sim, int i, const P &p0, const P &p1, const P &p2, const P &p3,
                 const P &p4, const P &p5, uint64_t v) {
    switch (i) {
    case 0: sim.set_input_value(p0, v); break;
    case 1: sim.set_input_value(p1, v); break;
    case 2: sim.set_input_value(p2, v); break;
    case 3: sim.set_input_value(p3, v); break;
    case 4: sim.set_input_value(p4, v); break;
    default: sim.set_input_value(p5, v); break;
    }
}

template <typename P>
inline uint64_t get6(Sim &sim, int i, const P &p0, const P &p1, const P &p2, const P &p3,
                     const P &p4, const P &p5) {
    switch (i) {
    case 0: return pv(sim, p0);
    case 1: return pv(sim, p1);
    case 2: return pv(sim, p2);
    case 3: return pv(sim, p3);
    case 4: return pv(sim, p4);
    default: return pv(sim, p5);
    }
}

// ---------------- TB 驱动 master 侧输入 ----------------
inline void drv_aw(Sim &sim, auto &io, int m, uint32_t addr, uint64_t valid) {
    set2(sim, m, io.m0_awaddr, io.m1_awaddr, addr);
    set2(sim, m, io.m0_awprot, io.m1_awprot, 0);
    set2(sim, m, io.m0_awvalid, io.m1_awvalid, valid);
}

inline void drv_w(Sim &sim, auto &io, int m, uint32_t data, uint32_t strb, uint64_t valid) {
    set2(sim, m, io.m0_wdata, io.m1_wdata, data);
    set2(sim, m, io.m0_wstrb, io.m1_wstrb, strb);
    set2(sim, m, io.m0_wvalid, io.m1_wvalid, valid);
}

inline void drv_ar(Sim &sim, auto &io, int m, uint32_t addr, uint64_t valid) {
    set2(sim, m, io.m0_araddr, io.m1_araddr, addr);
    set2(sim, m, io.m0_arprot, io.m1_arprot, 0);
    set2(sim, m, io.m0_arvalid, io.m1_arvalid, valid);
}

inline void drv_bready(Sim &sim, auto &io, int m, uint64_t v) {
    set2(sim, m, io.m0_bready, io.m1_bready, v);
}

inline void drv_rready(Sim &sim, auto &io, int m, uint64_t v) {
    set2(sim, m, io.m0_rready, io.m1_rready, v);
}

// ---------------- TB 驱动 slave 侧输入（扮演从设备）----------------
inline void slv_awready(Sim &sim, auto &io, int s, uint64_t v) {
    set6(sim, s, io.s0_awready, io.s1_awready, io.s2_awready, io.s3_awready,
         io.s4_awready, io.s5_awready, v);
}

inline void slv_wready(Sim &sim, auto &io, int s, uint64_t v) {
    set6(sim, s, io.s0_wready, io.s1_wready, io.s2_wready, io.s3_wready, io.s4_wready,
         io.s5_wready, v);
}

inline void slv_b(Sim &sim, auto &io, int s, uint64_t valid, uint64_t resp) {
    set6(sim, s, io.s0_bvalid, io.s1_bvalid, io.s2_bvalid, io.s3_bvalid, io.s4_bvalid,
         io.s5_bvalid, valid);
    set6(sim, s, io.s0_bresp, io.s1_bresp, io.s2_bresp, io.s3_bresp, io.s4_bresp,
         io.s5_bresp, resp);
}

inline void slv_arready(Sim &sim, auto &io, int s, uint64_t v) {
    set6(sim, s, io.s0_arready, io.s1_arready, io.s2_arready, io.s3_arready,
         io.s4_arready, io.s5_arready, v);
}

inline void slv_r(Sim &sim, auto &io, int s, uint64_t valid, uint32_t data, uint64_t resp) {
    set6(sim, s, io.s0_rvalid, io.s1_rvalid, io.s2_rvalid, io.s3_rvalid, io.s4_rvalid,
         io.s5_rvalid, valid);
    set6(sim, s, io.s0_rdata, io.s1_rdata, io.s2_rdata, io.s3_rdata, io.s4_rdata,
         io.s5_rdata, data);
    set6(sim, s, io.s0_rresp, io.s1_rresp, io.s2_rresp, io.s3_rresp, io.s4_rresp,
         io.s5_rresp, resp);
}

// ---------------- 观测 master 侧输出 ----------------
inline uint64_t o_awready(Sim &sim, auto &io, int m) {
    return get2(sim, m, io.m0_awready, io.m1_awready);
}
inline uint64_t o_wready(Sim &sim, auto &io, int m) {
    return get2(sim, m, io.m0_wready, io.m1_wready);
}
inline uint64_t o_bvalid(Sim &sim, auto &io, int m) {
    return get2(sim, m, io.m0_bvalid, io.m1_bvalid);
}
inline uint64_t o_bresp(Sim &sim, auto &io, int m) {
    return get2(sim, m, io.m0_bresp, io.m1_bresp);
}
inline uint64_t o_arready(Sim &sim, auto &io, int m) {
    return get2(sim, m, io.m0_arready, io.m1_arready);
}
inline uint64_t o_rvalid(Sim &sim, auto &io, int m) {
    return get2(sim, m, io.m0_rvalid, io.m1_rvalid);
}
inline uint64_t o_rdata(Sim &sim, auto &io, int m) {
    return get2(sim, m, io.m0_rdata, io.m1_rdata);
}
inline uint64_t o_rresp(Sim &sim, auto &io, int m) {
    return get2(sim, m, io.m0_rresp, io.m1_rresp);
}

// ---------------- 观测 slave 侧输出 ----------------
inline uint64_t o_s_awvalid(Sim &sim, auto &io, int s) {
    return get6(sim, s, io.s0_awvalid, io.s1_awvalid, io.s2_awvalid, io.s3_awvalid,
                io.s4_awvalid, io.s5_awvalid);
}
inline uint64_t o_s_awaddr(Sim &sim, auto &io, int s) {
    return get6(sim, s, io.s0_awaddr, io.s1_awaddr, io.s2_awaddr, io.s3_awaddr,
                io.s4_awaddr, io.s5_awaddr);
}
inline uint64_t o_s_wvalid(Sim &sim, auto &io, int s) {
    return get6(sim, s, io.s0_wvalid, io.s1_wvalid, io.s2_wvalid, io.s3_wvalid,
                io.s4_wvalid, io.s5_wvalid);
}
inline uint64_t o_s_wdata(Sim &sim, auto &io, int s) {
    return get6(sim, s, io.s0_wdata, io.s1_wdata, io.s2_wdata, io.s3_wdata, io.s4_wdata,
                io.s5_wdata);
}
inline uint64_t o_s_wstrb(Sim &sim, auto &io, int s) {
    return get6(sim, s, io.s0_wstrb, io.s1_wstrb, io.s2_wstrb, io.s3_wstrb, io.s4_wstrb,
                io.s5_wstrb);
}
inline uint64_t o_s_bready(Sim &sim, auto &io, int s) {
    return get6(sim, s, io.s0_bready, io.s1_bready, io.s2_bready, io.s3_bready,
                io.s4_bready, io.s5_bready);
}
inline uint64_t o_s_arvalid(Sim &sim, auto &io, int s) {
    return get6(sim, s, io.s0_arvalid, io.s1_arvalid, io.s2_arvalid, io.s3_arvalid,
                io.s4_arvalid, io.s5_arvalid);
}
inline uint64_t o_s_araddr(Sim &sim, auto &io, int s) {
    return get6(sim, s, io.s0_araddr, io.s1_araddr, io.s2_araddr, io.s3_araddr,
                io.s4_araddr, io.s5_araddr);
}
inline uint64_t o_s_rready(Sim &sim, auto &io, int s) {
    return get6(sim, s, io.s0_rready, io.s1_rready, io.s2_rready, io.s3_rready,
                io.s4_rready, io.s5_rready);
}

// 6 个 slave 的 awvalid / arvalid 位掩码（bit i = slave i）
inline uint64_t awvalid_mask(Sim &sim, auto &io) {
    uint64_t m = 0;
    for (int s = 0; s < 6; ++s) m |= (o_s_awvalid(sim, io, s) << s);
    return m;
}
inline uint64_t arvalid_mask(Sim &sim, auto &io) {
    uint64_t m = 0;
    for (int s = 0; s < 6; ++s) m |= (o_s_arvalid(sim, io, s) << s);
    return m;
}

// 有界等待：cond 为真即返回（**不** tick）；超时返回 false。
// ⚠️ 先 `eval_combinational()`：`set_input_value()` 只写数据槽、不重新求值。
template <typename F>
inline bool wait_until(Sim &sim, F cond, int max_cycles = 64) {
    for (int i = 0; i < max_cycles; ++i) {
        sim.eval_combinational();
        if (cond()) return true;
        sim.tick();
    }
    sim.eval_combinational();
    return cond();
}

// 所有 TB 驱动的输入归零 + 放开复位（每个用例 MUST 先调它）
inline void cb_init(Sim &sim, auto &io) {
    for (int i = 0; i < 2; ++i) {
        drv_aw(sim, io, i, 0, 0);
        drv_w(sim, io, i, 0, 0, 0);
        drv_ar(sim, io, i, 0, 0);
        drv_bready(sim, io, i, 0);
        drv_rready(sim, io, i, 0);
    }
    for (int i = 0; i < 6; ++i) {
        slv_awready(sim, io, i, 0);
        slv_wready(sim, io, i, 0);
        slv_b(sim, io, i, 0, 0);
        slv_arready(sim, io, i, 0);
        slv_r(sim, io, i, 0, 0, 0);
    }
    sim.set_input_value(io.rst_n, 1);
    sim.tick();
}

struct WriteObs {
    uint64_t awvalid_mask = 0;
    uint32_t s_awaddr = 0;
    uint32_t s_wdata = 0;
    uint32_t s_wstrb = 0;
    uint32_t bresp = 0;
    uint64_t other_bvalid = 0;
};

struct ReadObs {
    uint64_t arvalid_mask = 0;
    uint32_t s_araddr = 0;
    uint32_t rdata = 0;
    uint32_t rresp = 0;
    uint64_t other_rvalid = 0;
};

// 完整单拍写事务（TB 同时扮演 master 与 slave）
inline bool write_txn(Sim &sim, auto &io, int m, int s, uint32_t addr, uint32_t data,
                      uint32_t strb, WriteObs *obs = nullptr) {
    drv_aw(sim, io, m, addr, 1);
    slv_awready(sim, io, s, 1);
    if (!wait_until(sim, [&] { return o_awready(sim, io, m) == 1; })) {
        WARN("AW timeout: m=" << m << " s=" << s << " addr=0x" << std::hex << addr);
        return false;
    }
    if (obs) {
        obs->awvalid_mask = awvalid_mask(sim, io);
        obs->s_awaddr = static_cast<uint32_t>(o_s_awaddr(sim, io, s));
    }
    sim.tick();                  // AW 握手：锁存路由 + 置 active
    drv_aw(sim, io, m, 0, 0);
    slv_awready(sim, io, s, 0);

    drv_w(sim, io, m, data, strb, 1);
    slv_wready(sim, io, s, 1);
    if (!wait_until(sim, [&] { return o_wready(sim, io, m) == 1; })) {
        WARN("W timeout: m=" << m << " s=" << s);
        return false;
    }
    if (obs) {
        obs->s_wdata = static_cast<uint32_t>(o_s_wdata(sim, io, s));
        obs->s_wstrb = static_cast<uint32_t>(o_s_wstrb(sim, io, s));
    }
    sim.tick();                  // W 握手
    drv_w(sim, io, m, 0, 0, 0);
    slv_wready(sim, io, s, 0);

    slv_b(sim, io, s, 1, 0);     // OKAY
    if (!wait_until(sim, [&] { return o_bvalid(sim, io, m) == 1; })) {
        WARN("B timeout: m=" << m << " s=" << s);
        return false;
    }
    if (obs) {
        obs->bresp = static_cast<uint32_t>(o_bresp(sim, io, m));
        obs->other_bvalid = o_bvalid(sim, io, 1 - m);
    }
    drv_bready(sim, io, m, 1);
    sim.tick();                  // B 握手
    drv_bready(sim, io, m, 0);
    slv_b(sim, io, s, 0, 0);
    return true;
}

// 完整单拍读事务
inline bool read_txn(Sim &sim, auto &io, int m, int s, uint32_t addr, uint32_t data,
                     ReadObs *obs = nullptr) {
    drv_ar(sim, io, m, addr, 1);
    slv_arready(sim, io, s, 1);
    if (!wait_until(sim, [&] { return o_arready(sim, io, m) == 1; })) {
        WARN("AR timeout: m=" << m << " s=" << s << " addr=0x" << std::hex << addr);
        return false;
    }
    if (obs) {
        obs->arvalid_mask = arvalid_mask(sim, io);
        obs->s_araddr = static_cast<uint32_t>(o_s_araddr(sim, io, s));
    }
    sim.tick();                  // AR 握手：锁存路由 + 置 active
    drv_ar(sim, io, m, 0, 0);
    slv_arready(sim, io, s, 0);

    slv_r(sim, io, s, 1, data, 0);   // OKAY
    if (!wait_until(sim, [&] { return o_rvalid(sim, io, m) == 1; })) {
        WARN("R timeout: m=" << m << " s=" << s);
        return false;
    }
    if (obs) {
        obs->rdata = static_cast<uint32_t>(o_rdata(sim, io, m));
        obs->rresp = static_cast<uint32_t>(o_rresp(sim, io, m));
        obs->other_rvalid = o_rvalid(sim, io, 1 - m);
    }
    drv_rready(sim, io, m, 1);
    sim.tick();                  // R 握手
    drv_rready(sim, io, m, 0);
    slv_r(sim, io, s, 0, 0, 0);
    return true;
}

// ===========================================================================
// 用例
// ===========================================================================

// --- ① 路由完备：2×6 每个 (master, slave) 对的写 / 读 ----------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar fanout: all 12 (master,slave) write pairs",
                       "[axi][crossbar][dual][routing]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);
    for (int m = 0; m < 2; ++m) {
        for (int s = 0; s < 6; ++s) {
            INFO("m=" << m << " s=" << s);
            WriteObs obs;
            REQUIRE(write_txn(sim, io, m, s, kSlaveBase[s], 0xA000u + 0x10u * m + s, 0xFu,
                              &obs));
            CHECK(obs.awvalid_mask == (1ull << s));   // 只有目标 slave 看到 AW
            CHECK(obs.s_awaddr == kSlaveBase[s]);     // 绝对地址原样转发（不重定基）
            CHECK(obs.s_wdata == 0xA000u + 0x10u * m + s);
            CHECK(obs.s_wstrb == 0xFu);
            CHECK(obs.bresp == 0u);                   // OKAY
            CHECK(obs.other_bvalid == 0u);            // 无串扰
        }
    }
});

DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar fanout: all 12 (master,slave) read pairs",
                       "[axi][crossbar][dual][routing]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);
    for (int m = 0; m < 2; ++m) {
        for (int s = 0; s < 6; ++s) {
            INFO("m=" << m << " s=" << s);
            ReadObs obs;
            REQUIRE(read_txn(sim, io, m, s, kSlaveBase[s], 0xB000u + 0x10u * m + s, &obs));
            CHECK(obs.arvalid_mask == (1ull << s));
            CHECK(obs.s_araddr == kSlaveBase[s]);
            CHECK(obs.rdata == 0xB000u + 0x10u * m + s);
            CHECK(obs.rresp == 0u);
            CHECK(obs.other_rvalid == 0u);
        }
    }
});

// --- ② 写路由锁存：AW 握手后改 AWADDR ⇒ W/B 仍去原 slave -------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar write route latch: changing AWADDR after handshake",
                       "[axi][crossbar][dual][latch]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);

    const int m = 0;
    const int s = 2;
    drv_aw(sim, io, m, kSlaveBase[s], 1);
    slv_awready(sim, io, s, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, m) == 1; }));
    CHECK(o_s_awaddr(sim, io, s) == kSlaveBase[s]);
    sim.tick();
    drv_aw(sim, io, m, 0, 0);
    slv_awready(sim, io, s, 0);

    // 握手后把地址改成 s0（**不得**影响在途事务）
    drv_aw(sim, io, m, kSlaveBase[0], 1);
    sim.eval_combinational();
    CHECK(awvalid_mask(sim, io) == 0u);   // 第二笔不被转发
    drv_aw(sim, io, m, kSlaveBase[0], 0);

    drv_w(sim, io, m, 0xDEADu, 0xFu, 1);
    slv_wready(sim, io, s, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, m) == 1; }));
    CHECK(o_s_wvalid(sim, io, s) == 1u);    // W 跟随**写路由**
    CHECK(o_s_wvalid(sim, io, 0) == 0u);
    CHECK(o_s_wdata(sim, io, s) == 0xDEADu);
    sim.tick();
    drv_w(sim, io, m, 0, 0, 0);
    slv_wready(sim, io, s, 0);

    slv_b(sim, io, s, 1, 0);
    REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, m) == 1; }));
    CHECK(o_bresp(sim, io, m) == 0u);
    CHECK(o_bvalid(sim, io, 1 - m) == 0u);
    drv_bready(sim, io, m, 1);
    sim.tick();
    drv_bready(sim, io, m, 0);
    slv_b(sim, io, s, 0, 0);
});

// --- ③ 读路由锁存：AR 握手后改 ARADDR ⇒ R 仍来自原 slave -------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar read route latch: changing ARADDR after handshake",
                       "[axi][crossbar][dual][latch]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);

    const int m = 1;
    const int s = 0;
    drv_ar(sim, io, m, kSlaveBase[s], 1);
    slv_arready(sim, io, s, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, m) == 1; }));
    CHECK(o_s_araddr(sim, io, s) == kSlaveBase[s]);
    sim.tick();
    drv_ar(sim, io, m, 0, 0);
    slv_arready(sim, io, s, 0);

    // 握手后改地址 + 另一笔读请求（不得影响在途 R）
    drv_ar(sim, io, m, kSlaveBase[2], 1);
    sim.eval_combinational();
    CHECK(arvalid_mask(sim, io) == 0u);
    drv_ar(sim, io, m, 0, 0);

    slv_r(sim, io, s, 1, 0xC0DEu, 0);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, m) == 1; }));
    CHECK(o_rdata(sim, io, m) == 0xC0DEu);
    CHECK(o_rvalid(sim, io, 1 - m) == 0u);
    drv_rready(sim, io, m, 1);
    sim.tick();
    drv_rready(sim, io, m, 0);
    slv_r(sim, io, s, 0, 0, 0);
});

// --- ④ 在途门控：写事务在途时 awready MUST 为 0，下一笔不丢 ----------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar in-flight: awready low, next AW not swallowed",
                       "[axi][crossbar][dual][inflight]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);

    const int m = 0;
    const int s = 1;
    drv_aw(sim, io, m, kSlaveBase[s], 1);
    slv_awready(sim, io, s, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, m) == 1; }));
    sim.tick();                    // 第一笔 AW 完成 ⇒ active=1
    drv_aw(sim, io, m, 0, 0);
    slv_awready(sim, io, s, 1);    // 保持 slave ready

    // 在途：awready MUST 为 0（同向只允许 1 笔在途）
    drv_aw(sim, io, m, kSlaveBase[s], 1);
    sim.eval_combinational();
    CHECK(o_awready(sim, io, m) == 0u);
    CHECK(awvalid_mask(sim, io) == 0u);   // 没有被转发（不吞、不落）

    // 换一个**空闲** slave 也不行 —— 否则第二笔会覆盖第一笔的锁存路由。
    // ⚠️ s2 的 awready 必须为 1：否则「只靠目标 slave 忙」就能挡住，判据无判别力。
    slv_awready(sim, io, 2, 1);
    drv_aw(sim, io, m, kSlaveBase[2], 1);
    sim.eval_combinational();
    CHECK(o_awready(sim, io, m) == 0u);
    CHECK(awvalid_mask(sim, io) == 0u);
    slv_awready(sim, io, 2, 0);
    drv_aw(sim, io, m, 0, 0);

    // 完成第一笔（W + B），第二笔随后 MUST 被接受
    drv_w(sim, io, m, 0x1111u, 0xFu, 1);
    slv_wready(sim, io, s, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, m) == 1; }));
    sim.tick();
    drv_w(sim, io, m, 0, 0, 0);
    slv_wready(sim, io, s, 0);
    slv_b(sim, io, s, 1, 0);
    REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, m) == 1; }));
    drv_bready(sim, io, m, 1);
    sim.tick();
    drv_bready(sim, io, m, 0);
    slv_b(sim, io, s, 0, 0);

    // 第二笔
    REQUIRE(write_txn(sim, io, m, s, kSlaveBase[s], 0x2222u, 0xFu));
});

// --- ⑤ 仲裁：两 master 同时请求同一 slave ⇒ 轮转（手推 m1,m0,m1,m0）-------
//
// 手推：`round_robin_selector` 的起点 = `(last_grant_idx + 1) % N`；
// `last_grant` 复位为 0 ⇒ start = 1 ⇒ **首轮授予 m1**；之后每次握手把
// `last_grant` 推成刚授予的 one-hot ⇒ 下一轮起点前移 ⇒ 交替。
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar arbitration: grants rotate across masters",
                       "[axi][crossbar][dual][arb]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);

    for (int round = 0; round < 4; ++round) {
        INFO("round=" << round);
        const int expected = 1 - (round % 2);

        drv_ar(sim, io, 0, kSlaveBase[0], 1);
        drv_ar(sim, io, 1, kSlaveBase[0], 1);
        slv_arready(sim, io, 0, 1);
        REQUIRE(wait_until(sim, [&] {
            const uint64_t r0 = o_arready(sim, io, 0);
            const uint64_t r1 = o_arready(sim, io, 1);
            return (r0 ^ r1) == 1;   // 恰好一个被授权
        }));
        CHECK(o_arready(sim, io, expected) == 1u);
        CHECK(o_arready(sim, io, 1 - expected) == 0u);

        // 完成该笔读（R 握手）
        sim.tick();                  // AR 握手
        drv_ar(sim, io, 0, 0, 0);
        drv_ar(sim, io, 1, 0, 0);
        slv_arready(sim, io, 0, 0);
        slv_r(sim, io, 0, 1, 0x5000u + round, 0);
        REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, expected) == 1; }));
        drv_rready(sim, io, expected, 1);
        sim.tick();
        drv_rready(sim, io, expected, 0);
        slv_r(sim, io, 0, 0, 0, 0);
    }

    // 写方向 MUST 同样轮转（AW / AR 各一套指针 ⇒ 读的轮转不证明写的轮转）。
    // `aw_last_grant` 尚未被读过（读方向不碰它）⇒ 手推仍从 m1 起交替。
    for (int round = 0; round < 4; ++round) {
        INFO("write round=" << round);
        const int expected = 1 - (round % 2);

        drv_aw(sim, io, 0, kSlaveBase[0], 1);
        drv_aw(sim, io, 1, kSlaveBase[0], 1);
        slv_awready(sim, io, 0, 1);
        REQUIRE(wait_until(sim, [&] {
            const uint64_t a0 = o_awready(sim, io, 0);
            const uint64_t a1 = o_awready(sim, io, 1);
            return (a0 ^ a1) == 1;
        }));
        CHECK(o_awready(sim, io, expected) == 1u);
        CHECK(o_awready(sim, io, 1 - expected) == 0u);

        sim.tick();                  // AW 握手
        drv_aw(sim, io, 0, 0, 0);
        drv_aw(sim, io, 1, 0, 0);
        slv_awready(sim, io, 0, 0);

        drv_w(sim, io, expected, 0x6000u + round, 0xFu, 1);
        slv_wready(sim, io, 0, 1);
        REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, expected) == 1; }));
        sim.tick();
        drv_w(sim, io, expected, 0, 0, 0);
        slv_wready(sim, io, 0, 0);

        slv_b(sim, io, 0, 1, 0);
        REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, expected) == 1; }));
        drv_bready(sim, io, expected, 1);
        sim.tick();
        drv_bready(sim, io, expected, 0);
        slv_b(sim, io, 0, 0, 0);
    }
});

// --- ⑥ DECERR：未映射写 / 读 MUST 由互连自应答 ----------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar DECERR: unmapped write answers 0b11 and does not hang",
                       "[axi][crossbar][dual][decerr]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);

    drv_aw(sim, io, 0, kUnmappedAddr, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();
    drv_aw(sim, io, 0, 0, 0);
    slv_awready(sim, io, 0, 0);    // 不让任何真实 slave 响应

    drv_w(sim, io, 0, 0xBAD0u, 0xFu, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 0) == 1; }));
    sim.tick();
    drv_w(sim, io, 0, 0, 0, 0);

    REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, 0) == 1; }));
    CHECK(o_bresp(sim, io, 0) == 3u);          // DECERR
    CHECK(o_bvalid(sim, io, 1) == 0u);
    drv_bready(sim, io, 0, 1);
    sim.tick();
    drv_bready(sim, io, 0, 0);

    drv_ar(sim, io, 1, kUnmappedAddr, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 1) == 1; }));
    sim.tick();
    drv_ar(sim, io, 1, 0, 0);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, 1) == 1; }));
    CHECK(o_rresp(sim, io, 1) == 3u);
    CHECK(o_rdata(sim, io, 1) == 0u);
    drv_rready(sim, io, 1, 1);
    sim.tick();
    drv_rready(sim, io, 1, 0);
});

// --- ⑦ ERR 窗口：映射到 kErr 的地址同样 DECERR，且不落到任何真实 slave ----
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar ERR window: DECERR without touching any slave",
                       "[axi][crossbar][dual][decerr]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);

    drv_aw(sim, io, 0, kErrWindowAddr, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.eval_combinational();
    CHECK(awvalid_mask(sim, io) == 0u);   // ERR 路不落到任何真实 slave
    sim.tick();
    drv_aw(sim, io, 0, 0, 0);

    drv_w(sim, io, 0, 0x3333u, 0xFu, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 0) == 1; }));
    sim.tick();
    drv_w(sim, io, 0, 0, 0, 0);
    REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, 0) == 1; }));
    CHECK(o_bresp(sim, io, 0) == 3u);
    drv_bready(sim, io, 0, 1);
    sim.tick();
    drv_bready(sim, io, 0, 0);

    drv_ar(sim, io, 0, kErrWindowAddr, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 0) == 1; }));
    sim.eval_combinational();
    CHECK(arvalid_mask(sim, io) == 0u);
    sim.tick();
    drv_ar(sim, io, 0, 0, 0);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, 0) == 1; }));
    CHECK(o_rresp(sim, io, 0) == 3u);
    CHECK(o_rdata(sim, io, 0) == 0u);
    drv_rready(sim, io, 0, 1);
    sim.tick();
    drv_rready(sim, io, 0, 0);
});

// --- ⑧ W 跟随写路由：AW 与 W 相隔 1 拍（W 在 AW 之后才出现）---------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar W channel follows the latched write route",
                       "[axi][crossbar][dual][routing]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);

    const int m = 1;
    const int s = 2;
    drv_aw(sim, io, m, kSlaveBase[s], 1);
    slv_awready(sim, io, s, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, m) == 1; }));
    sim.tick();                    // AW 先完成，W 尚不 valid
    drv_aw(sim, io, m, 0, 0);
    slv_awready(sim, io, s, 0);

    // W 在**下一拍**出现 ⇒ 只允许 s2 看到
    drv_w(sim, io, m, 0x7777u, 0x3u, 1);
    slv_wready(sim, io, s, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, m) == 1; }));
    CHECK(o_s_wvalid(sim, io, s) == 1u);
    CHECK(o_s_wvalid(sim, io, 0) == 0u);
    CHECK(o_s_wvalid(sim, io, 1) == 0u);
    CHECK(o_s_wdata(sim, io, s) == 0x7777u);
    CHECK(o_s_wstrb(sim, io, s) == 0x3u);
    sim.tick();
    drv_w(sim, io, m, 0, 0, 0);
    slv_wready(sim, io, s, 0);

    slv_b(sim, io, s, 1, 0);
    REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, m) == 1; }));
    CHECK(o_bvalid(sim, io, 1 - m) == 0u);
    drv_bready(sim, io, m, 1);
    sim.tick();
    drv_bready(sim, io, m, 0);
    slv_b(sim, io, s, 0, 0);
});

// --- ⑨ 同一 slave 串行：两 master 同时写同一 slave ⇒ 第二个 awready=0 ------
//
// 手推：`last_grant` 复位 0 ⇒ 首轮授予 m1（同用例 ⑤）。m1 的写完成前，
// m0 的 AW MUST 不被接受，且 **s1 MUST NOT 看到第二次握手**（只掐 ready 不够：
// `awvalid & awready` 同时为 1 就是一次握手）。
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteCrossbar same-slave serialization: second AW waits for the first B",
                       "[axi][crossbar][dual][serialize]", [&] {
    auto &io = dev.instance().io();
    cb_init(sim, io);

    drv_aw(sim, io, 0, kSlaveBase[1], 1);
    drv_aw(sim, io, 1, kSlaveBase[1], 1);
    slv_awready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 1) == 1; }));
    CHECK(o_awready(sim, io, 0) == 0u);
    sim.tick();                    // m1 的 AW 握手
    drv_aw(sim, io, 1, 0, 0);

    // s1 已有在途写 ⇒ m0 的 AW MUST 不被接受，且 s1 看不到任何 valid
    sim.eval_combinational();
    CHECK(o_awready(sim, io, 0) == 0u);
    CHECK(awvalid_mask(sim, io) == 0u);

    // 完成 m1 的写
    drv_w(sim, io, 1, 0xAAAAu, 0xFu, 1);
    slv_wready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 1) == 1; }));
    CHECK(o_s_wvalid(sim, io, 1) == 1u);
    sim.tick();
    drv_w(sim, io, 1, 0, 0, 0);
    slv_wready(sim, io, 1, 0);
    slv_b(sim, io, 1, 1, 0);
    REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, 1) == 1; }));
    drv_bready(sim, io, 1, 1);
    sim.tick();
    drv_bready(sim, io, 1, 0);
    slv_b(sim, io, 1, 0, 0);

    // 第一笔完成后，m0 的 AW MUST 被接受（且落到同一个 s1）
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    CHECK(o_s_awvalid(sim, io, 1) == 1u);
    CHECK(o_s_awaddr(sim, io, 1) == kSlaveBase[1]);
    sim.tick();
    drv_aw(sim, io, 0, 0, 0);
});

}  // namespace ch_test::axil_crossbar
