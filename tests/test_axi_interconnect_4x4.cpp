// tests/test_axi_interconnect_4x4.cpp
//
// `AxiInterconnect4x4` 的单元测试 —— 双后端（interpreter + verilator）。
//
// 契约来源：`openspec/changes/axi4-interconnect-routing-2026-10/specs/
//            axi-interconnect-routing/spec.md`（ADDED × 6）
//
// DUT 形态：**testbench 同时扮演 master 与 slave**
//   * master 侧：DUT 的 `m*_awvalid` / `wvalid` / `bready` / `arvalid` / `rready`
//     是 `ch_in` ⇒ 由 TB 驱动；`m*_awready` / `bvalid` / `rvalid` … 是 `ch_out` ⇒ TB 观测。
//   * slave 侧：DUT 的 `s*_awready` / `wready` / `bvalid` / `arready` / `rvalid` … 是 `ch_in`
//     ⇒ 由 TB 驱动；`s*_awvalid` / `awaddr` / `wdata` / `bready` / `rvalid` … 观测。
//   ⇒ MUST NOT 实例化 `Axi4Slave` 当 fixture（会隐含依赖 burst change 的语义）。
//
// 超时纪律：**所有握手等待都是有界循环**（≤ 64 拍）+ 超时即 FAIL ——
// 互连缺陷的典型表现是握手**永不完成**（hang），无界等待会挂到 ctest TIMEOUT。
//
// 地址映射：默认映射把 32 位地址空间用 4 个 1 GiB 窗口铺满 ⇒ **DECERR 不可触发**。
// 故本文件用 `SparseTestMap`（4 个 4 KiB 窗口，其余地址未映射）⇒ DECERR 可测。

#include "axi4/axi_interconnect_4x4.h"

#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

namespace ch_test::axi_ic {

using Sim = ch::Simulator;

// ---------------------------------------------------------------------------
// 测试用地址映射：4 个 4 KiB 窗口连续排在 0x0000..0x3FFF，其余地址**未映射**
// ---------------------------------------------------------------------------
constexpr uint32_t kSlaveBase[4] = {0x0000'0000u, 0x0000'1000u, 0x0000'2000u, 0x0000'3000u};
constexpr uint32_t kUnmappedAddr = 0x8000'0000u;

struct SparseTestMap {
    static constexpr axi4::AxiAddrWindow windows[4] = {
        {0x0000'0000ull, 0x0000'1000ull},
        {0x0000'1000ull, 0x0000'1000ull},
        {0x0000'2000ull, 0x0000'1000ull},
        {0x0000'3000ull, 0x0000'1000ull},
    };
};

using Dut = axi4::AxiInterconnect4x4<32, 32, 4, SparseTestMap>;

// ---------------------------------------------------------------------------
// 基础设施
// ---------------------------------------------------------------------------
inline uint64_t pv(Sim &sim, const auto &port) {
    return static_cast<uint64_t>(sim.get_port_value(port));
}

template <typename P>
inline void set4(Sim &sim, int i, const P &a, const P &b, const P &c, const P &d, uint64_t v) {
    switch (i) {
    case 0: sim.set_input_value(a, v); break;
    case 1: sim.set_input_value(b, v); break;
    case 2: sim.set_input_value(c, v); break;
    default: sim.set_input_value(d, v); break;
    }
}

template <typename P>
inline uint64_t get4(Sim &sim, int i, const P &a, const P &b, const P &c, const P &d) {
    switch (i) {
    case 0: return pv(sim, a);
    case 1: return pv(sim, b);
    case 2: return pv(sim, c);
    default: return pv(sim, d);
    }
}

// ---------------- TB 驱动 master 侧输入 ----------------
inline void drv_aw(Sim &sim, auto &io, int m, uint32_t addr, uint32_t id, uint32_t len,
                   uint32_t size, uint32_t burst, uint64_t valid) {
    set4(sim, m, io.m0_awaddr, io.m1_awaddr, io.m2_awaddr, io.m3_awaddr, addr);
    set4(sim, m, io.m0_awid, io.m1_awid, io.m2_awid, io.m3_awid, id);
    set4(sim, m, io.m0_awlen, io.m1_awlen, io.m2_awlen, io.m3_awlen, len);
    set4(sim, m, io.m0_awsize, io.m1_awsize, io.m2_awsize, io.m3_awsize, size);
    set4(sim, m, io.m0_awburst, io.m1_awburst, io.m2_awburst, io.m3_awburst, burst);
    set4(sim, m, io.m0_awvalid, io.m1_awvalid, io.m2_awvalid, io.m3_awvalid, valid);
}

inline void drv_w(Sim &sim, auto &io, int m, uint32_t data, uint32_t strb, uint64_t last,
                  uint64_t valid) {
    set4(sim, m, io.m0_wdata, io.m1_wdata, io.m2_wdata, io.m3_wdata, data);
    set4(sim, m, io.m0_wid, io.m1_wid, io.m2_wid, io.m3_wid, 0);
    set4(sim, m, io.m0_wstrb, io.m1_wstrb, io.m2_wstrb, io.m3_wstrb, strb);
    set4(sim, m, io.m0_wlast, io.m1_wlast, io.m2_wlast, io.m3_wlast, last);
    set4(sim, m, io.m0_wvalid, io.m1_wvalid, io.m2_wvalid, io.m3_wvalid, valid);
}

inline void drv_ar(Sim &sim, auto &io, int m, uint32_t addr, uint32_t id, uint32_t len,
                   uint32_t size, uint32_t burst, uint64_t valid) {
    set4(sim, m, io.m0_araddr, io.m1_araddr, io.m2_araddr, io.m3_araddr, addr);
    set4(sim, m, io.m0_arid, io.m1_arid, io.m2_arid, io.m3_arid, id);
    set4(sim, m, io.m0_arlen, io.m1_arlen, io.m2_arlen, io.m3_arlen, len);
    set4(sim, m, io.m0_arsize, io.m1_arsize, io.m2_arsize, io.m3_arsize, size);
    set4(sim, m, io.m0_arburst, io.m1_arburst, io.m2_arburst, io.m3_arburst, burst);
    set4(sim, m, io.m0_arvalid, io.m1_arvalid, io.m2_arvalid, io.m3_arvalid, valid);
}

inline void drv_bready(Sim &sim, auto &io, int m, uint64_t v) {
    set4(sim, m, io.m0_bready, io.m1_bready, io.m2_bready, io.m3_bready, v);
}

inline void drv_rready(Sim &sim, auto &io, int m, uint64_t v) {
    set4(sim, m, io.m0_rready, io.m1_rready, io.m2_rready, io.m3_rready, v);
}

// ---------------- TB 驱动 slave 侧输入（扮演从设备）----------------
inline void slv_awready(Sim &sim, auto &io, int s, uint64_t v) {
    set4(sim, s, io.s0_awready, io.s1_awready, io.s2_awready, io.s3_awready, v);
}

inline void slv_wready(Sim &sim, auto &io, int s, uint64_t v) {
    set4(sim, s, io.s0_wready, io.s1_wready, io.s2_wready, io.s3_wready, v);
}

inline void slv_b(Sim &sim, auto &io, int s, uint64_t valid, uint64_t resp, uint64_t id) {
    set4(sim, s, io.s0_bvalid, io.s1_bvalid, io.s2_bvalid, io.s3_bvalid, valid);
    set4(sim, s, io.s0_bresp, io.s1_bresp, io.s2_bresp, io.s3_bresp, resp);
    set4(sim, s, io.s0_bid, io.s1_bid, io.s2_bid, io.s3_bid, id);
}

inline void slv_arready(Sim &sim, auto &io, int s, uint64_t v) {
    set4(sim, s, io.s0_arready, io.s1_arready, io.s2_arready, io.s3_arready, v);
}

inline void slv_r(Sim &sim, auto &io, int s, uint64_t valid, uint64_t data, uint64_t resp,
                  uint64_t id, uint64_t last) {
    set4(sim, s, io.s0_rvalid, io.s1_rvalid, io.s2_rvalid, io.s3_rvalid, valid);
    set4(sim, s, io.s0_rdata, io.s1_rdata, io.s2_rdata, io.s3_rdata, data);
    set4(sim, s, io.s0_rresp, io.s1_rresp, io.s2_rresp, io.s3_rresp, resp);
    set4(sim, s, io.s0_rid, io.s1_rid, io.s2_rid, io.s3_rid, id);
    set4(sim, s, io.s0_rlast, io.s1_rlast, io.s2_rlast, io.s3_rlast, last);
}

// ---------------- 观测 master 侧输出 ----------------
inline uint64_t o_awready(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_awready, io.m1_awready, io.m2_awready, io.m3_awready);
}
inline uint64_t o_wready(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_wready, io.m1_wready, io.m2_wready, io.m3_wready);
}
inline uint64_t o_bvalid(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_bvalid, io.m1_bvalid, io.m2_bvalid, io.m3_bvalid);
}
inline uint64_t o_bresp(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_bresp, io.m1_bresp, io.m2_bresp, io.m3_bresp);
}
inline uint64_t o_bid(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_bid, io.m1_bid, io.m2_bid, io.m3_bid);
}
inline uint64_t o_arready(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_arready, io.m1_arready, io.m2_arready, io.m3_arready);
}
inline uint64_t o_rvalid(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_rvalid, io.m1_rvalid, io.m2_rvalid, io.m3_rvalid);
}
inline uint64_t o_rdata(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_rdata, io.m1_rdata, io.m2_rdata, io.m3_rdata);
}
inline uint64_t o_rresp(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_rresp, io.m1_rresp, io.m2_rresp, io.m3_rresp);
}
inline uint64_t o_rid(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_rid, io.m1_rid, io.m2_rid, io.m3_rid);
}
inline uint64_t o_rlast(Sim &sim, auto &io, int m) {
    return get4(sim, m, io.m0_rlast, io.m1_rlast, io.m2_rlast, io.m3_rlast);
}

// ---------------- 观测 slave 侧输出 ----------------
inline uint64_t o_s_awvalid(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_awvalid, io.s1_awvalid, io.s2_awvalid, io.s3_awvalid);
}
inline uint64_t o_s_awaddr(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_awaddr, io.s1_awaddr, io.s2_awaddr, io.s3_awaddr);
}
inline uint64_t o_s_awid(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_awid, io.s1_awid, io.s2_awid, io.s3_awid);
}
inline uint64_t o_s_awlen(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_awlen, io.s1_awlen, io.s2_awlen, io.s3_awlen);
}
inline uint64_t o_s_wvalid(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_wvalid, io.s1_wvalid, io.s2_wvalid, io.s3_wvalid);
}
inline uint64_t o_s_wdata(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_wdata, io.s1_wdata, io.s2_wdata, io.s3_wdata);
}
inline uint64_t o_s_wstrb(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_wstrb, io.s1_wstrb, io.s2_wstrb, io.s3_wstrb);
}
inline uint64_t o_s_bready(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_bready, io.s1_bready, io.s2_bready, io.s3_bready);
}
inline uint64_t o_s_arvalid(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_arvalid, io.s1_arvalid, io.s2_arvalid, io.s3_arvalid);
}
inline uint64_t o_s_araddr(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_araddr, io.s1_araddr, io.s2_araddr, io.s3_araddr);
}
inline uint64_t o_s_rready(Sim &sim, auto &io, int s) {
    return get4(sim, s, io.s0_rready, io.s1_rready, io.s2_rready, io.s3_rready);
}

// 4 个 slave 的 awvalid / arvalid 位掩码（bit i = slave i）
inline uint64_t awvalid_mask(Sim &sim, auto &io) {
    return (o_s_awvalid(sim, io, 0) << 0) | (o_s_awvalid(sim, io, 1) << 1) |
           (o_s_awvalid(sim, io, 2) << 2) | (o_s_awvalid(sim, io, 3) << 3);
}
inline uint64_t arvalid_mask(Sim &sim, auto &io) {
    return (o_s_arvalid(sim, io, 0) << 0) | (o_s_arvalid(sim, io, 1) << 1) |
           (o_s_arvalid(sim, io, 2) << 2) | (o_s_arvalid(sim, io, 3) << 3);
}

// 有界等待：cond 为真即返回（**不** tick）；超时返回 false。
//
// ⚠️ 必须先 `eval_combinational()`：`set_input_value()` 只写数据槽、**不**重新求值，
// 因此紧随其后的 `get_port_value()` 读到的是**上一次 tick** 的组合结果。
// 若省略这一步，「握手会在下一次 tick 完成，而测试从未观察到 ready==1」
// ⇒ 所有握手等待都会假超时（实测：awready 恒读出 0 而 active 已是 1）。
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

// 所有 TB 驱动的输入归零 + 放开复位（每个用例 MUST 先调它；
// 未驱动的 `rst_n` 读出为 0 = 永久复位）
inline void ic_init(Sim &sim, auto &io) {
    for (int i = 0; i < 4; ++i) {
        drv_aw(sim, io, i, 0, 0, 0, 0, 0, 0);
        drv_w(sim, io, i, 0, 0, 0, 0);
        drv_ar(sim, io, i, 0, 0, 0, 0, 0, 0);
        drv_bready(sim, io, i, 0);
        drv_rready(sim, io, i, 0);
        slv_awready(sim, io, i, 0);
        slv_wready(sim, io, i, 0);
        slv_b(sim, io, i, 0, 0, 0);
        slv_arready(sim, io, i, 0);
        slv_r(sim, io, i, 0, 0, 0, 0, 0);
    }
    sim.set_input_value(io.rst_n, 1);
    sim.tick();
}

// 提交一次 AW 握手（返回 false = 超时）
inline bool commit_aw(Sim &sim, auto &io, int m) {
    if (!wait_until(sim, [&] { return o_awready(sim, io, m) == 1; })) return false;
    sim.tick();  // 该沿采样 awvalid & awready ⇒ 路由锁存 + active 置位
    return true;
}

inline bool commit_w(Sim &sim, auto &io, int m) {
    if (!wait_until(sim, [&] { return o_wready(sim, io, m) == 1; })) return false;
    sim.tick();
    return true;
}

inline bool commit_ar(Sim &sim, auto &io, int m) {
    if (!wait_until(sim, [&] { return o_arready(sim, io, m) == 1; })) return false;
    sim.tick();
    return true;
}

// 观测记录（用于断言「数据真的落到了正确的地方」，而非只比握手信号）
struct WriteObs {
    uint64_t awvalid_mask = 0;  // AW 拍：4 个 slave 的 awvalid 位掩码
    uint64_t awready_mask = 0;  // AW 拍：4 个 master 的 awready 位掩码
    uint32_t s_awaddr = 0;      // 目标 slave 看到的地址
    uint32_t s_awid = 0;
    uint32_t s_wdata = 0;       // W 拍：目标 slave 看到的数据
    uint32_t s_wstrb = 0;
    uint32_t bresp = 0;         // B 拍：master 收到的响应
    uint32_t bid = 0;
};

struct ReadObs {
    uint64_t arvalid_mask = 0;
    uint32_t s_araddr = 0;
    uint32_t rdata = 0;
    uint32_t rresp = 0;
    uint32_t rid = 0;
    uint64_t rlast = 0;
};

// 完整单拍写事务（TB 同时扮演 master 与 slave）
inline bool write_txn(Sim &sim, auto &io, int m, int s, uint32_t addr, uint32_t data,
                      uint32_t strb, uint32_t id, WriteObs *obs = nullptr) {
    drv_aw(sim, io, m, addr, id, 0, 2, 1, 1);
    slv_awready(sim, io, s, 1);
    if (!wait_until(sim, [&] { return o_awready(sim, io, m) == 1; })) {
        WARN("AW timeout: m=" << m << " s=" << s << " addr=0x" << std::hex << addr
             << " rst_n=" << pv(sim, io.rst_n)
             << " awready=" << o_awready(sim, io, 0) << o_awready(sim, io, 1)
             << o_awready(sim, io, 2) << o_awready(sim, io, 3)
             << " bvalid=" << o_bvalid(sim, io, m));
        return false;
    }
    if (obs) {
        obs->awvalid_mask = awvalid_mask(sim, io);
        obs->s_awaddr = static_cast<uint32_t>(o_s_awaddr(sim, io, s));
        obs->s_awid = static_cast<uint32_t>(o_s_awid(sim, io, s));
        for (int k = 0; k < 4; ++k)
            if (o_awready(sim, io, k)) obs->awready_mask |= (1ull << k);
    }
    sim.tick();                            // AW 握手：锁存路由 + 置 active
    drv_aw(sim, io, m, 0, 0, 0, 0, 0, 0);  // 握手完成即撤 AWVALID
    slv_awready(sim, io, s, 0);

    drv_w(sim, io, m, data, strb, 1, 1);
    slv_wready(sim, io, s, 1);
    if (!wait_until(sim, [&] { return o_wready(sim, io, m) == 1; })) {
        WARN("W timeout: m=" << m << " s=" << s
             << " wready=" << o_wready(sim, io, 0) << o_wready(sim, io, 1)
             << o_wready(sim, io, 2) << o_wready(sim, io, 3));
        return false;
    }
    if (obs) {
        obs->s_wdata = static_cast<uint32_t>(o_s_wdata(sim, io, s));
        obs->s_wstrb = static_cast<uint32_t>(o_s_wstrb(sim, io, s));
    }
    sim.tick();  // W 握手
    drv_w(sim, io, m, 0, 0, 0, 0);
    slv_wready(sim, io, s, 0);

    slv_b(sim, io, s, 1, 0 /*OKAY*/, id);
    drv_bready(sim, io, m, 1);
    if (!wait_until(sim, [&] { return o_bvalid(sim, io, m) == 1; })) return false;
    if (obs) {
        obs->bresp = static_cast<uint32_t>(o_bresp(sim, io, m));
        obs->bid = static_cast<uint32_t>(o_bid(sim, io, m));
    }
    sim.tick();  // B 握手
    slv_b(sim, io, s, 0, 0, 0);
    drv_bready(sim, io, m, 0);
    return true;
}

// 完整单拍读事务
inline bool read_txn(Sim &sim, auto &io, int m, int s, uint32_t addr, uint32_t data,
                     uint32_t id, ReadObs *obs = nullptr) {
    drv_ar(sim, io, m, addr, id, 0, 2, 1, 1);
    slv_arready(sim, io, s, 1);
    if (!wait_until(sim, [&] { return o_arready(sim, io, m) == 1; })) {
        WARN("AR timeout: m=" << m << " s=" << s << " addr=0x" << std::hex << addr
             << " arready=" << o_arready(sim, io, 0) << o_arready(sim, io, 1)
             << o_arready(sim, io, 2) << o_arready(sim, io, 3));
        return false;
    }
    if (obs) {
        obs->arvalid_mask = arvalid_mask(sim, io);
        obs->s_araddr = static_cast<uint32_t>(o_s_araddr(sim, io, s));
    }
    sim.tick();                            // AR 握手
    drv_ar(sim, io, m, 0, 0, 0, 0, 0, 0);
    slv_arready(sim, io, s, 0);

    slv_r(sim, io, s, 1, data, 0 /*OKAY*/, id, 1 /*rlast*/);
    drv_rready(sim, io, m, 1);
    if (!wait_until(sim, [&] { return o_rvalid(sim, io, m) == 1; })) return false;
    if (obs) {
        obs->rdata = static_cast<uint32_t>(o_rdata(sim, io, m));
        obs->rresp = static_cast<uint32_t>(o_rresp(sim, io, m));
        obs->rid = static_cast<uint32_t>(o_rid(sim, io, m));
        obs->rlast = o_rlast(sim, io, m);
    }
    sim.tick();  // R 握手（末拍 ⇒ read_active 清零）
    slv_r(sim, io, s, 0, 0, 0, 0, 0);
    drv_rready(sim, io, m, 0);
    return true;
}

}  // namespace ch_test::axi_ic

// ===========================================================================
// 用例（14 对 × 2 后端 = 28 个 TEST_CASE）
// ===========================================================================
namespace ch_test::axi_ic {

// --- 1. 16 对全扇出（写）---------------------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 fanout: all 16 (master,slave) write pairs",
                       "[axi][interconnect][dual][fanout]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);
    for (int m = 0; m < 4; ++m) {
        for (int s = 0; s < 4; ++s) {
            const uint32_t data = 0xA000u + static_cast<uint32_t>(m * 16 + s);
            WriteObs o;
            INFO("master=" << m << " slave=" << s);
            REQUIRE(write_txn(sim, io, m, s, kSlaveBase[s] + 0x8u, data, 0xFu, 0x5u, &o));
            REQUIRE(o.awvalid_mask == (1ull << s));  // 只有一个 slave 被选中
            REQUIRE(o.awready_mask == (1ull << m));  // 只有一个 master 被授权
            REQUIRE(o.s_awaddr == kSlaveBase[s] + 0x8u);
            REQUIRE(o.s_awid == 0x5u);
            REQUIRE(o.s_wdata == data);              // **数据断言**（不只比握手）
            REQUIRE(o.s_wstrb == 0xFu);
            REQUIRE(o.bresp == 0u);                  // OKAY
            REQUIRE(o.bid == 0x5u);
        }
    }
});

// --- 2. 16 对全扇出（读）---------------------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 fanout: all 16 (master,slave) read pairs",
                       "[axi][interconnect][dual][fanout]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);
    for (int m = 0; m < 4; ++m) {
        for (int s = 0; s < 4; ++s) {
            const uint32_t data = 0xB000u + static_cast<uint32_t>(m * 16 + s);
            ReadObs o;
            INFO("master=" << m << " slave=" << s);
            REQUIRE(read_txn(sim, io, m, s, kSlaveBase[s] + 0x4u, data, 0x7u, &o));
            REQUIRE(o.arvalid_mask == (1ull << s));
            REQUIRE(o.s_araddr == kSlaveBase[s] + 0x4u);
            REQUIRE(o.rdata == data);
            REQUIRE(o.rid == 0x7u);
            REQUIRE(o.rresp == 0u);
            REQUIRE(o.rlast == 1u);
        }
    }
});

// --- 3. 地址映射：窗口内地址**原样转发**（不改写、不串扰）------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 address map: window addresses forwarded verbatim",
                       "[axi][interconnect][dual][addr-map]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);
    for (int s = 0; s < 4; ++s) {
        const uint32_t addr = kSlaveBase[s] + 0x10u;
        WriteObs o;
        INFO("slave=" << s << " addr=0x" << std::hex << addr);
        REQUIRE(write_txn(sim, io, s, s, addr, 0xC0DEu, 0xFu, static_cast<uint32_t>(s), &o));
        REQUIRE(o.s_awaddr == addr);            // 未被改写
        REQUIRE(o.awvalid_mask == (1ull << s)); // 其他窗口不串扰
    }
});

// --- 4. 写事务 AW → W → B 全链路（含数据/响应断言）-------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 write: AW-W-B full chain with data and response",
                       "[axi][interconnect][dual][write]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);
    WriteObs o;
    REQUIRE(write_txn(sim, io, 2, 3, kSlaveBase[3], 0xDEADBEEFu, 0xFu, 0x9u, &o));
    REQUIRE(o.awvalid_mask == (1ull << 3));
    REQUIRE(o.s_wdata == 0xDEADBEEFu);
    REQUIRE(o.bresp == 0u);
    REQUIRE(o.bid == 0x9u);
});

// --- 5. 读事务 AR → R 全链路 ----------------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 read: AR-R full chain with data and response",
                       "[axi][interconnect][dual][read]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);
    ReadObs o;
    REQUIRE(read_txn(sim, io, 1, 2, kSlaveBase[2], 0x1234ABCDu, 0x3u, &o));
    REQUIRE(o.arvalid_mask == (1ull << 2));
    REQUIRE(o.s_araddr == kSlaveBase[2]);
    REQUIRE(o.rdata == 0x1234ABCDu);
    REQUIRE(o.rid == 0x3u);
    REQUIRE(o.rlast == 1u);
});

// --- 6. 未映射地址写：DECERR + bvalid 保持到 bready（不是单拍脉冲）--------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 DECERR: unmapped write answers with held bvalid",
                       "[axi][interconnect][dual][decerr]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);

    drv_aw(sim, io, 1, kUnmappedAddr, 0x6u, 0, 2, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 1) == 1; }));
    sim.tick();  // 未映射 AW 握手
    drv_aw(sim, io, 1, 0, 0, 0, 0, 0, 0);

    drv_w(sim, io, 1, 0x1111u, 0xFu, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 1) == 1; }));
    sim.tick();  // 未映射 W 握手 ⇒ 下一拍 bvalid 置位
    drv_w(sim, io, 1, 0, 0, 0, 0);

    // 没有任何从设备被卷入
    REQUIRE(awvalid_mask(sim, io) == 0);

    // bvalid MUST 是寄存器且**保持**到 bready（BREADY 迟迟不来也不能掉）
    drv_bready(sim, io, 1, 0);
    for (int i = 0; i < 3; ++i) {
        INFO("hold cycle " << i);
        REQUIRE(o_bvalid(sim, io, 1) == 1);
        REQUIRE(o_bresp(sim, io, 1) == 3u);  // DECERR
        sim.tick();
    }
    // 其它 master 的 bvalid 不受影响
    REQUIRE(o_bvalid(sim, io, 0) == 0);
    REQUIRE(o_bvalid(sim, io, 2) == 0);
    REQUIRE(o_bvalid(sim, io, 3) == 0);

    drv_bready(sim, io, 1, 1);
    sim.tick();
    drv_bready(sim, io, 1, 0);
    REQUIRE(o_bvalid(sim, io, 1) == 0);  // 握手后释放
});

// --- 7. 未映射地址读：arlen+1 个 DECERR beat，末拍 rlast -------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 DECERR: unmapped read returns arlen+1 DECERR beats",
                       "[axi][interconnect][dual][decerr]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);

    constexpr uint32_t kLen = 3;  // ⇒ 4 个 beat
    drv_ar(sim, io, 2, kUnmappedAddr, 0x8u, kLen, 2, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 2) == 1; }));
    sim.tick();  // AR 握手
    drv_ar(sim, io, 2, 0, 0, 0, 0, 0, 0);
    REQUIRE(arvalid_mask(sim, io) == 0);  // 不落入任何从设备

    drv_rready(sim, io, 2, 1);
    int beats = 0;
    for (int i = 0; i < static_cast<int>(kLen) + 1; ++i) {
        INFO("beat " << i);
        REQUIRE(o_rvalid(sim, io, 2) == 1);
        REQUIRE(o_rresp(sim, io, 2) == 3u);  // 每拍 DECERR
        REQUIRE(o_rdata(sim, io, 2) == 0u);
        const bool expect_last = (i == static_cast<int>(kLen));
        REQUIRE(o_rlast(sim, io, 2) == (expect_last ? 1u : 0u));
        sim.tick();
        ++beats;
    }
    REQUIRE(beats == static_cast<int>(kLen) + 1);
    drv_rready(sim, io, 2, 0);
    REQUIRE(o_rvalid(sim, io, 2) == 0);  // 末拍握手后释放
});

// --- 8. 写读并发（不同 master）不串扰 -------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 concurrency: write and read on different masters",
                       "[axi][interconnect][dual][concurrency]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);

    // m0 → s2 写（先完成 AW + W，响应暂不返回）
    drv_aw(sim, io, 0, kSlaveBase[2], 0x2u, 0, 2, 1, 1);
    slv_awready(sim, io, 2, 1);
    REQUIRE(commit_aw(sim, io, 0));
    drv_aw(sim, io, 0, 0, 0, 0, 0, 0, 0);
    slv_awready(sim, io, 2, 0);
    drv_w(sim, io, 0, 0x5A5A5A5Au, 0xFu, 1, 1);
    slv_wready(sim, io, 2, 1);
    REQUIRE(commit_w(sim, io, 0));
    drv_w(sim, io, 0, 0, 0, 0, 0);
    slv_wready(sim, io, 2, 0);

    // 同时 m1 读 s3 —— 写事务在途不应阻塞读
    drv_ar(sim, io, 1, kSlaveBase[3], 0x4u, 0, 2, 1, 1);
    slv_arready(sim, io, 3, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 1) == 1; }));
    REQUIRE(arvalid_mask(sim, io) == (1ull << 3));  // 读只到 s3
    sim.tick();
    drv_ar(sim, io, 1, 0, 0, 0, 0, 0, 0);
    slv_arready(sim, io, 3, 0);

    slv_r(sim, io, 3, 1, 0x7777u, 0, 0x4u, 1);
    drv_rready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, 1) == 1; }));
    REQUIRE(o_rdata(sim, io, 1) == 0x7777u);
    REQUIRE(o_rid(sim, io, 1) == 0x4u);
    sim.tick();
    slv_r(sim, io, 3, 0, 0, 0, 0, 0);
    drv_rready(sim, io, 1, 0);

    // 写响应此刻才回来 —— MUST 仍回到 m0，且 s2 的数据没被读事务影响
    slv_b(sim, io, 2, 1, 0, 0x2u);
    drv_bready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, 0) == 1; }));
    REQUIRE(o_bid(sim, io, 0) == 0x2u);
    REQUIRE(o_bvalid(sim, io, 1) == 0);  // 不串到 m1
    sim.tick();
    slv_b(sim, io, 2, 0, 0, 0);
    drv_bready(sim, io, 0, 0);
});

// --- 9. 路由锁存：AW 握手后改写 AWADDR 不改变落点 -------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 route latch: changing AWADDR after handshake",
                       "[axi][interconnect][dual][latch]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);

    drv_aw(sim, io, 0, kSlaveBase[1], 0x1u, 0, 2, 1, 1);
    slv_awready(sim, io, 1, 1);
    REQUIRE(commit_aw(sim, io, 0));

    // 握手后立刻把地址改到 s2 的窗口（AXI 允许：AWADDR 只需保持到握手）
    drv_aw(sim, io, 0, kSlaveBase[2], 0x1u, 0, 2, 1, 0);
    slv_awready(sim, io, 1, 0);

    // 只有 s1 的 wready 拉高 ⇒ 若路由是「活的地址」就会挂死在这里
    drv_w(sim, io, 0, 0xFEEDu, 0xFu, 1, 1);
    slv_wready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 0) == 1; }));
    REQUIRE(o_s_wvalid(sim, io, 1) == 1);  // W 落到**握手时锁存**的 s1
    REQUIRE(o_s_wvalid(sim, io, 2) == 0);  // 而不是改写后的 s2
    REQUIRE(o_s_wdata(sim, io, 1) == 0xFEEDu);
    sim.tick();
});

// --- 10. W 通道不被仲裁轮转改投 + 在途期间不接新 AW -----------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 arbitration: in-flight master keeps grant",
                       "[axi][interconnect][dual][arbitration]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);

    drv_aw(sim, io, 0, kSlaveBase[1], 0x1u, 0, 2, 1, 1);
    slv_awready(sim, io, 1, 1);
    REQUIRE(commit_aw(sim, io, 0));
    drv_aw(sim, io, 0, 0, 0, 0, 0, 0, 0);
    slv_awready(sim, io, 1, 0);

    // m1 此时请求 AW（目标 s2）—— 在途期间 MUST NOT 被接受（AW 静默丢失的防线）
    drv_aw(sim, io, 1, kSlaveBase[2], 0x2u, 0, 2, 1, 1);
    slv_awready(sim, io, 2, 1);
    for (int i = 0; i < 4; ++i) {
        INFO("in-flight cycle " << i);
        REQUIRE(o_awready(sim, io, 1) == 0);
        sim.tick();
    }
    // 在途写仍能被接受（授权未被抢走）
    drv_w(sim, io, 0, 0xABCDu, 0xFu, 1, 1);
    slv_wready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 0) == 1; }));
    REQUIRE(o_s_wvalid(sim, io, 1) == 1);
    REQUIRE(o_s_wdata(sim, io, 1) == 0xABCDu);
    sim.tick();
    drv_w(sim, io, 0, 0, 0, 0, 0);
    slv_wready(sim, io, 1, 0);

    // 收尾：完成 m0 的写响应，然后 m1 的 AW 才应被接受
    slv_b(sim, io, 1, 1, 0, 0x1u);
    drv_bready(sim, io, 0, 1);
    sim.tick();
    slv_b(sim, io, 1, 0, 0, 0);
    drv_bready(sim, io, 0, 0);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 1) == 1; }));
});

// --- 11. WSTRB 完整转发 ---------------------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 WSTRB forwarded verbatim",
                       "[axi][interconnect][dual][wstrb]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);
    WriteObs o;
    REQUIRE(write_txn(sim, io, 3, 0, kSlaveBase[0], 0x11223344u, 0x5u, 0x0u, &o));
    REQUIRE(o.s_wstrb == 0x5u);
    REQUIRE(o.s_wdata == 0x11223344u);
});

// --- 12. 读事务不被写请求阻塞（AW / AR 独立仲裁）-------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 read is not blocked by a pending write",
                       "[axi][interconnect][dual][arbitration]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);

    // m1 的写已在途（AW 已握手、W 未到）
    drv_aw(sim, io, 1, kSlaveBase[2], 0x2u, 0, 2, 1, 1);
    slv_awready(sim, io, 2, 1);
    REQUIRE(commit_aw(sim, io, 1));
    drv_aw(sim, io, 1, 0, 0, 0, 0, 0, 0);
    slv_awready(sim, io, 2, 0);

    // m0 只有读请求 ⇒ MUST 能被接受（读仲裁独立于写授权）
    drv_ar(sim, io, 0, kSlaveBase[3], 0x3u, 0, 2, 1, 1);
    slv_arready(sim, io, 3, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 0) == 1; }));
    sim.tick();
    drv_ar(sim, io, 0, 0, 0, 0, 0, 0, 0);
    slv_arready(sim, io, 3, 0);

    slv_r(sim, io, 3, 1, 0x99u, 0, 0x3u, 1);
    drv_rready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, 0) == 1; }));
    REQUIRE(o_rdata(sim, io, 0) == 0x99u);
    sim.tick();
    slv_r(sim, io, 3, 0, 0, 0, 0, 0);
    drv_rready(sim, io, 0, 0);
});

// --- 13. 在途期间 awready MUST 为 0 --------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 awready is low while a transaction is in flight",
                       "[axi][interconnect][dual][inflight]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);

    drv_aw(sim, io, 2, kSlaveBase[0], 0x1u, 0, 2, 1, 1);
    slv_awready(sim, io, 0, 1);
    REQUIRE(commit_aw(sim, io, 2));

    // 仍持有 AWVALID（违规 master 的形态）—— awready MUST 为 0，AW 不得被静默吞掉
    for (int i = 0; i < 4; ++i) {
        INFO("in-flight cycle " << i);
        REQUIRE(o_awready(sim, io, 2) == 0);
        sim.tick();
    }
});

// --- 14. 中途复位逃生路径（双后端一致）-----------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiInterconnect4x4 mid-transaction reset clears state",
                       "[axi][interconnect][dual][reset]", [&] {
    auto &io = dev.instance().io();
    ic_init(sim, io);

    // 造一笔在途写（AW 已握手，W 未到）
    drv_aw(sim, io, 0, kSlaveBase[0], 0x1u, 0, 2, 1, 1);
    slv_awready(sim, io, 0, 1);
    REQUIRE(commit_aw(sim, io, 0));
    REQUIRE(o_awready(sim, io, 0) == 0);  // 在途标志已置位

    // ⚠️ 必须先撤掉总线上的 VALID/READY，**再**进复位窗口 —— 否则复位释放那一拍
    // `awvalid & awready` 仍然同时为高 ⇒ 会**再握手一次**（在途标志被重新置位）。
    drv_aw(sim, io, 0, 0, 0, 0, 0, 0, 0);
    slv_awready(sim, io, 0, 0);

    // 中途复位 ≥ 1 拍 ⇒ 全部 active 清零
    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.set_input_value(io.rst_n, 1);
    sim.tick();

    // 复位后 MUST 能接受一笔**全新**事务
    // （注意：不能直接断言 `awready==1` —— 那需要 `awvalid=1` 才有 grant）
    WriteObs o;
    REQUIRE(write_txn(sim, io, 1, 3, kSlaveBase[3], 0x1357u, 0xFu, 0x1u, &o));
    REQUIRE(o.awvalid_mask == (1ull << 3));
    REQUIRE(o.s_wdata == 0x1357u);
    REQUIRE(o.bresp == 0u);
});

}  // namespace ch_test::axi_ic
