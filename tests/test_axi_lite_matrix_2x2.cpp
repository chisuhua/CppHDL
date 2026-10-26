// tests/test_axi_lite_matrix_2x2.cpp
//
// `AxiLiteMatrix2x2` 的单元测试 —— 双后端（interpreter + verilator）。
//
// 契约来源：`openspec/changes/axi4-interconnect-routing-2026-10/specs/
//            axi-interconnect-routing/spec.md`（该 capability 同时管辖
//            `AxiInterconnect4x4` 与 `AxiLiteMatrix2x2`）
//
// DUT 形态：**testbench 同时扮演 master 与 slave**
//   * master 侧：TB 驱动 `m*_awvalid` / `wvalid` / `bready` / `arvalid` / `rready`，
//     观测 `m*_awready` / `wready` / `bvalid` / `bresp` / `arready` / `rvalid` /
//     `rdata` / `rresp`。
//   * slave 侧：TB 驱动 `s*_awready` / `wready` / `bvalid` / `bresp` / `arready` /
//     `rvalid` / `rdata` / `rresp`，观测 `s*_awaddr` / `awvalid` / `wdata` /
//     `wstrb` / `wvalid` / `bready` / `araddr` / `arvalid` / `rready`。
//   ⇒ MUST NOT 实例化任何 AXI slave 当 fixture。
//
// 超时纪律：**所有握手等待都是有界循环**（≤ 64 拍）+ 超时即 FAIL ——
// 互连缺陷的典型表现是握手**永不完成**（hang），无界等待会挂到 ctest TIMEOUT。
//
// 地址映射：默认映射（2×2 用 2 个 2 GiB 窗口）把地址空间铺满 ⇒ **DECERR 不可触发**。
// 故本文件用 `SparseTestMap`（2 个 4 KiB 窗口，其余地址未映射）⇒ DECERR 可测。

#include "axi4/axi4_lite_matrix.h"

#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

namespace ch_test::axi_mx {

using Sim = ch::Simulator;

// ---------------------------------------------------------------------------
// 测试用地址映射：2 个 4 KiB 窗口连续排在 0x0000..0x1FFF，其余地址**未映射**
// ---------------------------------------------------------------------------
constexpr uint32_t kSlaveBase[2] = {0x0000'0000u, 0x0000'1000u};
constexpr uint32_t kUnmappedAddr = 0x8000'0000u;

struct SparseTestMap {
    static constexpr axi4::AxiAddrWindow windows[2] = {
        {0x0000'0000ull, 0x0000'1000ull},
        {0x0000'1000ull, 0x0000'1000ull},
    };
};

using Dut = axi4::AxiLiteMatrix2x2<32, 32, SparseTestMap>;

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
    set2(sim, s, io.s0_awready, io.s1_awready, v);
}

inline void slv_wready(Sim &sim, auto &io, int s, uint64_t v) {
    set2(sim, s, io.s0_wready, io.s1_wready, v);
}

inline void slv_b(Sim &sim, auto &io, int s, uint64_t valid, uint64_t resp) {
    set2(sim, s, io.s0_bvalid, io.s1_bvalid, valid);
    set2(sim, s, io.s0_bresp, io.s1_bresp, resp);
}

inline void slv_arready(Sim &sim, auto &io, int s, uint64_t v) {
    set2(sim, s, io.s0_arready, io.s1_arready, v);
}

inline void slv_r(Sim &sim, auto &io, int s, uint64_t valid, uint32_t data, uint64_t resp) {
    set2(sim, s, io.s0_rvalid, io.s1_rvalid, valid);
    set2(sim, s, io.s0_rdata, io.s1_rdata, data);
    set2(sim, s, io.s0_rresp, io.s1_rresp, resp);
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
    return get2(sim, s, io.s0_awvalid, io.s1_awvalid);
}
inline uint64_t o_s_awaddr(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_awaddr, io.s1_awaddr);
}
inline uint64_t o_s_awprot(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_awprot, io.s1_awprot);
}
inline uint64_t o_s_wvalid(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_wvalid, io.s1_wvalid);
}
inline uint64_t o_s_wdata(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_wdata, io.s1_wdata);
}
inline uint64_t o_s_wstrb(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_wstrb, io.s1_wstrb);
}
inline uint64_t o_s_bready(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_bready, io.s1_bready);
}
inline uint64_t o_s_arvalid(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_arvalid, io.s1_arvalid);
}
inline uint64_t o_s_araddr(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_araddr, io.s1_araddr);
}
inline uint64_t o_s_arprot(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_arprot, io.s1_arprot);
}
inline uint64_t o_s_rready(Sim &sim, auto &io, int s) {
    return get2(sim, s, io.s0_rready, io.s1_rready);
}

// 2 个 slave 的 awvalid / arvalid 位掩码（bit i = slave i）
inline uint64_t awvalid_mask(Sim &sim, auto &io) {
    return (o_s_awvalid(sim, io, 0) << 0) | (o_s_awvalid(sim, io, 1) << 1);
}
inline uint64_t arvalid_mask(Sim &sim, auto &io) {
    return (o_s_arvalid(sim, io, 0) << 0) | (o_s_arvalid(sim, io, 1) << 1);
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
inline void mx_init(Sim &sim, auto &io) {
    for (int i = 0; i < 2; ++i) {
        drv_aw(sim, io, i, 0, 0);
        drv_w(sim, io, i, 0, 0, 0);
        drv_ar(sim, io, i, 0, 0);
        drv_bready(sim, io, i, 0);
        drv_rready(sim, io, i, 0);
        slv_awready(sim, io, i, 0);
        slv_wready(sim, io, i, 0);
        slv_b(sim, io, i, 0, 0);
        slv_arready(sim, io, i, 0);
        slv_r(sim, io, i, 0, 0, 0);
    }
    sim.set_input_value(io.rst_n, 1);
    sim.tick();
}

// 观测记录（用于断言「数据真的落到了正确的地方」，而非只比握手信号）
struct WriteObs {
    uint64_t awvalid_mask = 0;  // AW 拍：2 个 slave 的 awvalid 位掩码
    uint32_t s_awaddr = 0;      // 目标 slave 看到的地址
    uint32_t s_wdata = 0;       // W 拍：目标 slave 看到的数据
    uint32_t s_wstrb = 0;
    uint32_t bresp = 0;         // B 拍：master 收到的响应
    uint64_t other_bvalid = 0;  // 另一个 master 的 bvalid（串扰探针）
};

struct ReadObs {
    uint64_t arvalid_mask = 0;
    uint32_t s_araddr = 0;
    uint32_t rdata = 0;
    uint32_t rresp = 0;
    uint64_t other_rvalid = 0;  // 另一个 master 的 rvalid（串扰探针）
    uint64_t other_rdata = 0;
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
    sim.tick();                            // AW 握手：锁存路由 + 置 active
    drv_aw(sim, io, m, 0, 0);               // 握手完成即撤 AWVALID
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
    sim.tick();  // W 握手
    drv_w(sim, io, m, 0, 0, 0);
    slv_wready(sim, io, s, 0);

    slv_b(sim, io, s, 1, 0 /*OKAY*/);
    drv_bready(sim, io, m, 1);
    if (!wait_until(sim, [&] { return o_bvalid(sim, io, m) == 1; })) {
        WARN("B timeout: m=" << m << " s=" << s);
        return false;
    }
    if (obs) {
        obs->bresp = static_cast<uint32_t>(o_bresp(sim, io, m));
        obs->other_bvalid = o_bvalid(sim, io, 1 - m);
    }
    sim.tick();  // B 握手
    slv_b(sim, io, s, 0, 0);
    drv_bready(sim, io, m, 0);
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
    sim.tick();                            // AR 握手：锁存读路由
    drv_ar(sim, io, m, 0, 0);
    slv_arready(sim, io, s, 0);

    slv_r(sim, io, s, 1, data, 0 /*OKAY*/);
    drv_rready(sim, io, m, 1);
    if (!wait_until(sim, [&] { return o_rvalid(sim, io, m) == 1; })) {
        WARN("R timeout: m=" << m << " s=" << s);
        return false;
    }
    if (obs) {
        obs->rdata = static_cast<uint32_t>(o_rdata(sim, io, m));
        obs->rresp = static_cast<uint32_t>(o_rresp(sim, io, m));
        obs->other_rvalid = o_rvalid(sim, io, 1 - m);
        obs->other_rdata = o_rdata(sim, io, 1 - m);
    }
    sim.tick();  // R 握手
    slv_r(sim, io, s, 0, 0, 0);
    drv_rready(sim, io, m, 0);
    return true;
}

}  // namespace ch_test::axi_mx

// ===========================================================================
// 用例（14 对 × 2 后端 = 28 个 TEST_CASE）
// ===========================================================================
namespace ch_test::axi_mx {

// --- 1. 4 对全扇出（写）---------------------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 fanout: all 4 (master,slave) write pairs",
                       "[axi][matrix][dual][fanout]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);
    for (int m = 0; m < 2; ++m) {
        for (int s = 0; s < 2; ++s) {
            const uint32_t data = 0xA000u + static_cast<uint32_t>(m * 4 + s);
            WriteObs o;
            INFO("master=" << m << " slave=" << s);
            REQUIRE(write_txn(sim, io, m, s, kSlaveBase[s] + 0x8u, data, 0xFu, &o));
            REQUIRE(o.awvalid_mask == (1ull << s));  // 只有一个 slave 被选中
            REQUIRE(o.s_awaddr == kSlaveBase[s] + 0x8u);
            REQUIRE(o.s_wdata == data);               // **数据断言**（不只比握手）
            REQUIRE(o.s_wstrb == 0xFu);
            REQUIRE(o.bresp == 0u);                   // OKAY
            REQUIRE(o.other_bvalid == 0u);            // 另一个 master 无串扰
        }
    }
});

// --- 2. 4 对全扇出（读）---------------------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 fanout: all 4 (master,slave) read pairs",
                       "[axi][matrix][dual][fanout]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);
    for (int m = 0; m < 2; ++m) {
        for (int s = 0; s < 2; ++s) {
            const uint32_t data = 0xB000u + static_cast<uint32_t>(m * 4 + s);
            ReadObs o;
            INFO("master=" << m << " slave=" << s);
            REQUIRE(read_txn(sim, io, m, s, kSlaveBase[s] + 0x4u, data, &o));
            REQUIRE(o.arvalid_mask == (1ull << s));
            REQUIRE(o.s_araddr == kSlaveBase[s] + 0x4u);
            REQUIRE(o.rdata == data);
            REQUIRE(o.rresp == 0u);
            REQUIRE(o.other_rvalid == 0u);   // 另一个 master 无串扰
            REQUIRE(o.other_rdata == 0u);
        }
    }
});

// --- 3. 写侧路由锁存：AW 握手后改写 AWADDR 不改变落点 ---------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 write route latch: changing AWADDR after handshake",
                       "[axi][matrix][dual][latch]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    drv_aw(sim, io, 0, kSlaveBase[0], 1);
    slv_awready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();

    // 握手后立刻把地址改到 s1 的窗口（AXI 允许：AWADDR 只需保持到握手）
    drv_aw(sim, io, 0, kSlaveBase[1], 0);
    slv_awready(sim, io, 0, 0);

    // 只有 s0 的 wready 拉高 ⇒ 若路由是「活的地址」就会挂死在这里
    drv_w(sim, io, 0, 0xFEEDu, 0xFu, 1);
    slv_wready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 0) == 1; }));
    REQUIRE(o_s_wvalid(sim, io, 0) == 1);  // W 落到**握手时锁存**的 s0
    REQUIRE(o_s_wvalid(sim, io, 1) == 0);  // 而不是改写后的 s1
    REQUIRE(o_s_wdata(sim, io, 0) == 0xFEEDu);
    sim.tick();
});

// --- 4. 读侧路由锁存：AR 握手后改写 ARADDR 不改变来源 ---------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 read route latch: changing ARADDR after handshake",
                       "[axi][matrix][dual][latch]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    drv_ar(sim, io, 0, kSlaveBase[0], 1);
    slv_arready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 0) == 1; }));
    sim.tick();

    // 握手后把读地址改到 s1 的窗口
    drv_ar(sim, io, 0, kSlaveBase[1], 0);
    slv_arready(sim, io, 0, 0);

    // s0 返回数据 ⇒ 必须回到 m0（用**锁存的读路由**）
    slv_r(sim, io, 0, 1, 0x1234u, 0);
    drv_rready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, 0) == 1; }));
    REQUIRE(o_rdata(sim, io, 0) == 0x1234u);
    REQUIRE(o_rresp(sim, io, 0) == 0u);
    sim.tick();
    slv_r(sim, io, 0, 0, 0, 0);
    drv_rready(sim, io, 0, 0);
});

// --- 5. 读响应按**读路由**反查（回归护栏：旧实现用写 grant + 无读锁存）----
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 read response follows the read route, not the write grant",
                       "[axi][matrix][dual][routing]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    // m0 发起一笔**写**，目标是 s1（AW 已握手，W 未到 ⇒ 在途）
    drv_aw(sim, io, 0, kSlaveBase[1], 1);
    slv_awready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();
    drv_aw(sim, io, 0, 0, 0);
    slv_awready(sim, io, 1, 0);

    // m1 发起一笔**读**，目标是 s0；⚠️ 同时把 m1 的**写**地址也指向 s1 ——
    //    旧实现用写通道的 grant 反查 `s0_rready` ⇒ 会把 m1 的读 ready 判给 s1
    //    ⇒ m1 的读响应永不被接受（挂死）；且旧实现无读路由锁存 ⇒
    //    m0 也会看到同一份 rdata（串扰）。
    drv_aw(sim, io, 1, kSlaveBase[1], 0);
    drv_ar(sim, io, 1, kSlaveBase[0], 1);
    slv_arready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 1) == 1; }));
    REQUIRE(arvalid_mask(sim, io) == (1ull << 0));  // AR 只到 s0
    sim.tick();
    drv_ar(sim, io, 1, 0, 0);
    slv_arready(sim, io, 0, 0);

    // s0 返回数据 ⇒ 只有 m1 能看到
    slv_r(sim, io, 0, 1, 0xBEEFu, 0);
    drv_rready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, 1) == 1; }));
    REQUIRE(o_rdata(sim, io, 1) == 0xBEEFu);
    REQUIRE(o_rvalid(sim, io, 0) == 0u);   // m0 没有读事务 ⇒ MUST NOT 看到 rvalid
    REQUIRE(o_rdata(sim, io, 0) == 0u);   // MUST NOT 看到别人的 rdata
    sim.tick();
    slv_r(sim, io, 0, 0, 0, 0);
    drv_rready(sim, io, 1, 0);
});

// --- 6. 在途写事务期间 awready MUST 为 0 ---------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 awready is low while a write transaction is in flight",
                       "[axi][matrix][dual][inflight]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    drv_aw(sim, io, 0, kSlaveBase[0], 1);
    slv_awready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();

    // 仍持有 AWVALID（违规 master 的形态）—— awready MUST 为 0，AW 不得被静默吞掉
    for (int i = 0; i < 4; ++i) {
        INFO("in-flight cycle " << i);
        REQUIRE(o_awready(sim, io, 0) == 0);
        sim.tick();
    }
});

// --- 7. 在途读事务期间 arready MUST 为 0 ---------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 arready is low while a read transaction is in flight",
                       "[axi][matrix][dual][inflight]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    drv_ar(sim, io, 1, kSlaveBase[1], 1);
    slv_arready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 1) == 1; }));
    sim.tick();

    for (int i = 0; i < 4; ++i) {
        INFO("in-flight cycle " << i);
        REQUIRE(o_arready(sim, io, 1) == 0);
        sim.tick();
    }
});

// --- 8. 未映射地址写：DECERR + bvalid 保持到 bready ----------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 DECERR: unmapped write answers with held bvalid",
                       "[axi][matrix][dual][decerr]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    drv_aw(sim, io, 0, kUnmappedAddr, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();  // 未映射 AW 握手
    drv_aw(sim, io, 0, 0, 0);

    drv_w(sim, io, 0, 0x1111u, 0xFu, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 0) == 1; }));
    sim.tick();  // 未映射 W 握手 ⇒ 下一拍 bvalid 置位
    drv_w(sim, io, 0, 0, 0, 0);

    // 没有任何从设备被卷入
    REQUIRE(awvalid_mask(sim, io) == 0);

    // bvalid MUST 是寄存器且**保持**到 bready（BREADY 迟迟不来也不能掉）
    drv_bready(sim, io, 0, 0);
    for (int i = 0; i < 3; ++i) {
        INFO("hold cycle " << i);
        REQUIRE(o_bvalid(sim, io, 0) == 1);
        REQUIRE(o_bresp(sim, io, 0) == 3u);  // DECERR
        sim.tick();
    }
    REQUIRE(o_bvalid(sim, io, 1) == 0);  // 另一个 master 不受影响

    drv_bready(sim, io, 0, 1);
    sim.tick();
    drv_bready(sim, io, 0, 0);
    REQUIRE(o_bvalid(sim, io, 0) == 0);  // 握手后释放
});

// --- 9. 未映射地址读：1 个 DECERR beat（AXI4-Lite 无 arlen）--------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 DECERR: unmapped read answers with one DECERR beat",
                       "[axi][matrix][dual][decerr]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    drv_ar(sim, io, 1, kUnmappedAddr, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 1) == 1; }));
    sim.tick();  // AR 握手
    drv_ar(sim, io, 1, 0, 0);
    REQUIRE(arvalid_mask(sim, io) == 0);  // 不落入任何从设备

    drv_rready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, 1) == 1; }));
    REQUIRE(o_rresp(sim, io, 1) == 3u);  // DECERR
    REQUIRE(o_rdata(sim, io, 1) == 0u);
    sim.tick();
    REQUIRE(o_rvalid(sim, io, 1) == 0);  // AXI4-Lite 单拍 ⇒ 1 个 beat 后即释放
    REQUIRE(o_rvalid(sim, io, 0) == 0);
    drv_rready(sim, io, 1, 0);
});

// --- 10. 读事务不被写请求阻塞（AW / AR 独立仲裁）-------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 read is not blocked by a pending write",
                       "[axi][matrix][dual][arbitration]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    // m0 的写已在途（AW 已握手、W 未到）
    drv_aw(sim, io, 0, kSlaveBase[1], 1);
    slv_awready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();
    drv_aw(sim, io, 0, 0, 0);
    slv_awready(sim, io, 1, 0);

    // m1 只有读请求 ⇒ MUST 能被接受（读仲裁独立于写授权）
    drv_ar(sim, io, 1, kSlaveBase[0], 1);
    slv_arready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 1) == 1; }));
    sim.tick();
    drv_ar(sim, io, 1, 0, 0);
    slv_arready(sim, io, 0, 0);

    slv_r(sim, io, 0, 1, 0x99u, 0);
    drv_rready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, 1) == 1; }));
    REQUIRE(o_rdata(sim, io, 1) == 0x99u);
    sim.tick();
    slv_r(sim, io, 0, 0, 0, 0);
    drv_rready(sim, io, 1, 0);
});

// --- 11. 仲裁不饿死（指针指向无请求的 master 时让位）--------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 arbitration: the requesting master always gets the grant",
                       "[axi][matrix][dual][arbitration]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    // m0 连续完成 3 笔写（每笔完成都会轮转指针）⇒ 指针最终指向 m1。
    // 然后只让 m0 请求 ⇒ m0 MUST 仍然拿到 grant（**让位**逻辑）。
    for (int i = 0; i < 3; ++i) {
        WriteObs o;
        INFO("round " << i);
        REQUIRE(write_txn(sim, io, 0, 0, kSlaveBase[0], 0x10u + i, 0xFu, &o));
        REQUIRE(o.bresp == 0u);
    }
    // m0 再发一笔（此时指针大概率指向 m1 ⇒ m0 必须**让位成功**拿到 grant）
    drv_aw(sim, io, 0, kSlaveBase[0], 1);
    slv_awready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    REQUIRE(awvalid_mask(sim, io) == (1ull << 0));
    sim.tick();
});

// --- 12. WSTRB 完整转发 ---------------------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 WSTRB forwarded verbatim",
                       "[axi][matrix][dual][wstrb]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);
    WriteObs o;
    REQUIRE(write_txn(sim, io, 1, 1, kSlaveBase[1], 0x11223344u, 0x5u, &o));
    REQUIRE(o.s_wstrb == 0x5u);
    REQUIRE(o.s_wdata == 0x11223344u);
});

// --- 13. 中途复位逃生路径（双后端一致）-----------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 mid-transaction reset clears state",
                       "[axi][matrix][dual][reset]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    // 造一笔在途写（AW 已握手，W 未到）
    drv_aw(sim, io, 0, kSlaveBase[0], 1);
    slv_awready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();
    REQUIRE(o_awready(sim, io, 0) == 0);  // 在途标志已置位

    // ⚠️ 必须先撤掉总线上的 VALID/READY，**再**进复位窗口 —— 否则复位释放那一拍
    // `awvalid & awready` 仍然同时为高 ⇒ 会**再握手一次**（在途标志被重新置位）。
    drv_aw(sim, io, 0, 0, 0);
    slv_awready(sim, io, 0, 0);

    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.set_input_value(io.rst_n, 1);
    sim.tick();

    // 复位后 MUST 能接受一笔**全新**事务
    WriteObs o;
    REQUIRE(write_txn(sim, io, 1, 1, kSlaveBase[1], 0x1357u, 0xFu, &o));
    REQUIRE(o.awvalid_mask == (1ull << 1));
    REQUIRE(o.s_wdata == 0x1357u);
    REQUIRE(o.bresp == 0u);
});

// --- 14. 复位清零全部互连状态 -------------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 reset clears all routing state",
                       "[axi][matrix][dual][reset]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);
    // `rst_n` 是必需端口：未接线读出为 0 = 永久复位（见 spec 陷阱条目）
    sim.set_input_value(io.rst_n, 1);

    // 先跑一笔写，让写路由锁存到 s1
    drv_aw(sim, io, 0, kSlaveBase[1], 1);
    slv_awready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();
    drv_aw(sim, io, 0, 0, 0);
    slv_awready(sim, io, 1, 0);

    // 复位窗口（撤掉总线请求 → 复位 → 释放）
    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.set_input_value(io.rst_n, 1);
    sim.tick();

    // 复位后：一笔新的、目标是 s0 的写 MUST 落到 s0（残留的「路由 = s1」会被清零）
    WriteObs o;
    REQUIRE(write_txn(sim, io, 0, 0, kSlaveBase[0], 0x2222u, 0xFu, &o));
    REQUIRE(o.awvalid_mask == (1ull << 0));
    REQUIRE(o.s_awaddr == kSlaveBase[0]);
    REQUIRE(o.s_wdata == 0x2222u);
});

}  // namespace ch_test::axi_mx

// ===========================================================================
// critic SHOULD_FIX #3 的 2x2 补强用例（2026-10-26）：
//   ① 幻影 AW / AR 护栏（2x2 与 4x4 同型缺陷）。
//   ② 复位后 error responder 状态 MUST 被清零。
// ===========================================================================
namespace ch_test::axi_mx_extra {

using namespace ch_test::axi_mx;

// --- ① 幻影 AW / AR 护栏 ----------------------------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 in-flight: the next AW is not forwarded to any slave",
                       "[axi][matrix][dual][inflight]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);

    drv_aw(sim, io, 0, kSlaveBase[0], 1);
    slv_awready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();

    // 在途：m0 合法地举起**下一笔** AW
    drv_aw(sim, io, 0, kSlaveBase[1], 1);
    slv_awready(sim, io, 1, 1);
    for (int i = 0; i < 4; ++i) {
        INFO("in-flight cycle " << i);
        REQUIRE(o_awready(sim, io, 0) == 0);
        // 幻影 AW 护栏：MUST NOT 被转发给任何 slave
        REQUIRE(o_s_awvalid(sim, io, 0) == 0);
        REQUIRE(o_s_awvalid(sim, io, 1) == 0);
        sim.tick();
    }

    // ---- liveness 半面（critic NIT #3）：在途事务完成后，排队的那笔 MUST 最终被接受 ----
    drv_w(sim, io, 0, 0x1111u, 0xFu, 1);            // 在途那笔的 W（m0 → s0）
    slv_wready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 0) == 1; }));
    sim.tick();
    drv_w(sim, io, 0, 0, 0, 0);
    slv_wready(sim, io, 0, 0);

    slv_b(sim, io, 0, 1, 0 /*OKAY*/);
    drv_bready(sim, io, 0, 1);
    REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, 0) == 1; }));
    sim.tick();                                    // B 握手 ⇒ wr_active_m0 清零
    slv_b(sim, io, 0, 0, 0);
    drv_bready(sim, io, 0, 0);

    // 排队的 AW（m0 → s1）此时 MUST 被接受并转发到 s1
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    REQUIRE(o_s_awvalid(sim, io, 1) == 1);
    REQUIRE(o_s_awaddr(sim, io, 1) == kSlaveBase[1]);
    REQUIRE(o_s_awvalid(sim, io, 0) == 0);         // MUST NOT 落到 s0
});

DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 in-flight: the next AR is not forwarded to any slave",
                       "[axi][matrix][dual][inflight]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);

    drv_ar(sim, io, 1, kSlaveBase[1], 1);
    slv_arready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 1) == 1; }));
    sim.tick();

    drv_ar(sim, io, 1, kSlaveBase[0], 1);
    slv_arready(sim, io, 0, 1);
    for (int i = 0; i < 4; ++i) {
        INFO("in-flight cycle " << i);
        REQUIRE(o_arready(sim, io, 1) == 0);
        REQUIRE(o_s_arvalid(sim, io, 0) == 0);
        REQUIRE(o_s_arvalid(sim, io, 1) == 0);
        sim.tick();
    }

    // ---- liveness 半面（critic NIT #3）：在途读完成后，排队的那笔 MUST 最终被接受 ----
    slv_r(sim, io, 1, 1, 0xAAAAu, 0 /*OKAY*/);
    drv_rready(sim, io, 1, 1);
    REQUIRE(wait_until(sim, [&] { return o_rvalid(sim, io, 1) == 1; }));
    sim.tick();                                    // R 握手 ⇒ rd_active_m1 清零
    slv_r(sim, io, 1, 0, 0, 0);
    drv_rready(sim, io, 1, 0);

    REQUIRE(wait_until(sim, [&] { return o_arready(sim, io, 1) == 1; }));
    REQUIRE(o_s_arvalid(sim, io, 0) == 1);         // 排队那笔的目标是 s0
    REQUIRE(o_s_araddr(sim, io, 0) == kSlaveBase[0]);
    REQUIRE(o_s_arvalid(sim, io, 1) == 0);         // MUST NOT 落回 s1
});

// --- ② 复位后 error responder 状态 MUST 被清零 ------------------------------
DUAL_BACKEND_TEST_PAIR(Dut, "AxiLiteMatrix2x2 reset clears the DECERR error responder state",
                       "[axi][matrix][dual][reset]", [&] {
    auto &io = dev.instance().io();
    mx_init(sim, io);

    // 造一笔未映射写（AW + W 握手，但**不**做 B 握手）⇒ err_w_busy / err_w_valid = 1
    drv_aw(sim, io, 0, kUnmappedAddr, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();
    drv_aw(sim, io, 0, 0, 0);
    drv_w(sim, io, 0, 0x1111u, 0xFu, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 0) == 1; }));
    sim.tick();
    drv_w(sim, io, 0, 0, 0, 0);
    REQUIRE(o_bvalid(sim, io, 0) == 1u);
    drv_bready(sim, io, 0, 0);

    sim.set_input_value(io.rst_n, 0);
    sim.tick();
    sim.set_input_value(io.rst_n, 1);
    sim.tick();

    // 复位后：一笔**新的**未映射写 MUST 能被接受
    drv_aw(sim, io, 0, kUnmappedAddr, 1);
    REQUIRE(wait_until(sim, [&] { return o_awready(sim, io, 0) == 1; }));
    sim.tick();
    drv_aw(sim, io, 0, 0, 0);
    drv_w(sim, io, 0, 0x3333u, 0xFu, 1);
    REQUIRE(wait_until(sim, [&] { return o_wready(sim, io, 0) == 1; }));
    sim.tick();
    drv_w(sim, io, 0, 0, 0, 0);
    REQUIRE(wait_until(sim, [&] { return o_bvalid(sim, io, 0) == 1; }));
    REQUIRE(o_bresp(sim, io, 0) == 3u);
});

}  // namespace ch_test::axi_mx_extra
