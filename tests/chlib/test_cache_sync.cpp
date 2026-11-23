// tests/chlib/test_cache_sync.cpp
//
// ADR-041 Sync/Async variant split — runtime test for ICacheSync / DCacheSync.
// Hits / miss-fill / refilled-hit / write-through paths, driven through
// InterpreterBackend + VerilatorBackend via DUAL_BACKEND_TEST_PAIR.
//
// Scope notes (see openspec/changes/dual-backend-cache-tests):
// - Sync variant ONLY (1-cycle sread, RV32I L1 default). Async variant
//   (0-cycle aread, R6 #3 excluded) is intentionally NOT dual-covered here.
// - Stub headers (cpu/cache/{i,d}_cache_stub.h) are ADR-037 deprecation
//   candidates until 2027-04-17; this file does NOT include them.

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "cpu/cache/d_cache_sync.h"
#include "cpu/cache/i_cache_sync.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace chlib;

// ============================================================================
// Helper: read a ch_out port as uint64_t (consistent width for dual parity)
// ============================================================================

static inline uint64_t out_u64(ch::Simulator &sim,
                               ch::Component &dev,
                               std::function<ch::core::lnodeimpl *(void)> port_fn) {
    (void)dev;
    (void)sim;
    // unused path; placeholder for layout consistency
    return 0;
}

// ============================================================================
// Task 1: ICacheSync cold miss → AXI request (interpreter probe)
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - Cold miss generates AXI request",
    "[cache][sync][icache][cold-miss][dual]",
    [&]() {
        // Cold read: tag_mem/valid_mem initially 0 → hit=0, miss=1
        sim.set_input_value(dev.instance().io().addr, 0x1000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_addr)) == (uint64_t)0x1000);
    });

// ============================================================================
// Task 3: ICacheSync scenarios (5 TEST_CASE_PAIRs total when Task 3 lands)
// ============================================================================

// 2. Miss-fill: cycle 2 axi_resp_valid=1, axi_data=0xDEADBEEF
//    cache should latch the data into way0 (via do_fill path in describe())
DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - AXI resp_valid triggers fill",
    "[cache][sync][icache][fill][dual]",
    [&]() {
        // cycle 1: cold req
        sim.set_input_value(dev.instance().io().addr, 0x2000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        // cycle 2: AXI resp brings data back; valid_mem stays 0 from cycle 1 read
        sim.set_input_value(dev.instance().io().req,  0_h);
        sim.set_input_value(dev.instance().io().axi_ready,    1_h);
        sim.set_input_value(dev.instance().io().axi_resp_valid, 1_h);
        sim.set_input_value(dev.instance().io().axi_data, 0xDEADBEEF_h);
        sim.tick();

        // Fill happens internally; user-observable signal is the next req
        // (checked in scenario 3). Here we sanity-check cycle 2 outputs.
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
    });

// 3. Refilled same addr → hit; data comes from cache
DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - Refilled same addr yields hit",
    "[cache][sync][icache][refilled-hit][dual]",
    [&]() {
        // cycle 1: cold req
        sim.set_input_value(dev.instance().io().addr, 0x2000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        // cycle 2: AXI resp fills cache
        sim.set_input_value(dev.instance().io().req,  0_h);
        sim.set_input_value(dev.instance().io().axi_ready,    1_h);
        sim.set_input_value(dev.instance().io().axi_resp_valid, 1_h);
        sim.set_input_value(dev.instance().io().axi_data, 0xCAFEBABE_h);
        sim.tick();

        // cycle 3: re-request same addr → should hit
        sim.set_input_value(dev.instance().io().addr, 0x2000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().axi_ready, 0_h);
        sim.tick();

        // After fill (cycle 2 → cycle 3 sread), hit=1, miss=0
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 0u);
        // data should be the filled value (NOT axi_data, since cache won)
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().data)) == 0xCAFEBABEu);
    });

// 4. Different addr still miss after one fill
DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - Different addr after fill still misses",
    "[cache][sync][icache][cross-set-miss][dual]",
    [&]() {
        // cycle 1: cold req at 0x3000
        sim.set_input_value(dev.instance().io().addr, 0x3000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        // cycle 2: AXI resp fills 0x3000
        sim.set_input_value(dev.instance().io().req,  0_h);
        sim.set_input_value(dev.instance().io().axi_ready,    1_h);
        sim.set_input_value(dev.instance().io().axi_resp_valid, 1_h);
        sim.set_input_value(dev.instance().io().axi_data, 0x11111111_h);
        sim.tick();

        // cycle 3: new addr → cache_miss (different set, or tag mismatch in same set)
        sim.set_input_value(dev.instance().io().addr, 0x4000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
    });

// 5. Hit-ready reflects AXI ready on miss
DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - Miss ready == axi_ready",
    "[cache][sync][icache][ready-mux][dual]",
    [&]() {
        // cycle 1: cold req
        sim.set_input_value(dev.instance().io().addr, 0x5000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().axi_ready, 0_h);
        sim.tick();

        // miss, ready should be axi_ready = 0
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 0u);
    });

// ============================================================================
// Task 4: DCacheSync scenarios（**2026-11-23 · S3 重写**：AXI 侧改为 full 5 通道
// ⇒ ⇒ 激励从「1 次 resp」改为「**4-beat burst**」；断言端口名同步改）
// ============================================================================

// 6. DCache cold miss read ⇒ 发 AR（4-beat burst 请求）
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - Cold miss read generates AXI burst request",
    "[cache][sync][dcache][cold-miss][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().addr, 0x8000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   0_h);
        // ⚠️ 本拍**不拉** `ar_ready` ⇒ 观察 `ar_valid` 保持（AXI：VALID 不等 READY）
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_addr)) == (uint64_t)0x8000);
        // 手推 burst 参数：ARLEN=3（4 beat）· ARSIZE=2（4 B）· ARBURST=INCR(1)
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_len)) == 3u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_size)) == 2u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_burst)) == 1u);
    });

// 7. DCache refilled read → hit（4 beat 填满 line 后按 lane 读回）
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - Refilled read returns cached data",
    "[cache][sync][dcache][refilled-read][dual]",
    [&]() {
        // cycle 1: cold read ⇒ AR 握手
        sim.set_input_value(dev.instance().io().addr, 0x9000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 1_h);
        sim.tick();

        // cycle 2..5: 4 个 R beat（第 1 个 = 目标 lane 的数据）
        sim.set_input_value(dev.instance().io().req,      0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        for (int b = 0; b < 4; ++b) {
            sim.set_input_value(dev.instance().io().r_valid, 1_h);
            sim.set_input_value(dev.instance().io().r_data,
                                static_cast<uint64_t>(b == 0 ? 0xAABBCCDDull : 0ull));
            sim.set_input_value(dev.instance().io().r_last,
                                b == 3 ? 1_h : 0_h);
            sim.tick();
        }
        sim.set_input_value(dev.instance().io().r_valid, 0_h);
        sim.set_input_value(dev.instance().io().r_last,  0_h);

        // re-read same addr（lane 0）→ hit
        sim.set_input_value(dev.instance().io().addr, 0x9000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 0u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rdata)) == 0xAABBCCDDu);
    });

// 8. DCache hit write ⇒ write-through 单 beat（AW/W）
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - Hit write triggers AXI write-through",
    "[cache][sync][dcache][write-through][dual]",
    [&]() {
        // prefill 0xA000（4 beat）
        sim.set_input_value(dev.instance().io().addr, 0xA000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 1_h);
        sim.tick();
        sim.set_input_value(dev.instance().io().req,      0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        for (int b = 0; b < 4; ++b) {
            sim.set_input_value(dev.instance().io().r_valid, 1_h);
            sim.set_input_value(dev.instance().io().r_data,  static_cast<uint64_t>(0));
            sim.set_input_value(dev.instance().io().r_last,  b == 3 ? 1_h : 0_h);
            sim.tick();
        }
        sim.set_input_value(dev.instance().io().r_valid, 0_h);
        sim.set_input_value(dev.instance().io().r_last,  0_h);

        // 写命中 ⇒ write-through 必须出现在 AXI 写通道上
        sim.set_input_value(dev.instance().io().addr,  0xA000_h);
        sim.set_input_value(dev.instance().io().wdata, 0x12345678_h);
        sim.set_input_value(dev.instance().io().req,   1_h);
        sim.set_input_value(dev.instance().io().we,    1_h);
        sim.set_input_value(dev.instance().io().aw_ready, 1_h);
        sim.set_input_value(dev.instance().io().w_ready,  1_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().aw_valid)) == 1u);
        // 手推：写是**单 beat** ⇒ AWLEN=0
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().aw_len)) == 0u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().w_data)) == 0x12345678u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().aw_addr)) == (uint64_t)0xA000);
    });

// 9. DCache miss write ⇒ 写仍无条件直通 AXI（write-through 的语义）
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - Miss write still forwards to AXI",
    "[cache][sync][dcache][miss-write][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().addr,  0xB000_h);
        sim.set_input_value(dev.instance().io().wdata, 0xDEADF00D_h);
        sim.set_input_value(dev.instance().io().req,   1_h);
        sim.set_input_value(dev.instance().io().we,    1_h);
        sim.set_input_value(dev.instance().io().aw_ready, 1_h);
        sim.set_input_value(dev.instance().io().w_ready,  1_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().aw_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().w_data)) == 0xDEADF00Du);
    });

// ============================================================================
// 10-12. **S3 新增判据**：真 line（4 lane 各自可寻址）+ hit/miss 的 ready 语义
//    判据来源 = `openspec/changes/rv32i-dcache-burst-2026-11/specs/rv32i-dcache-timing/spec.md`
// ============================================================================

// 10. line 的 4 个 lane 各自可寻址（AC1 + AC2 · 「假 line」修复的直接判据）
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - line has 4 independently addressable lanes",
    "[cache][sync][dcache][line-lane][dual]",
    [&]() {
        constexpr uint64_t kBase = 0x4000;
        // fill：4 个 beat 各写一个**不同**的值（lane b = 0x1111_1111 * (b+1)）
        sim.set_input_value(dev.instance().io().addr,     kBase);
        sim.set_input_value(dev.instance().io().req,      1_h);
        sim.set_input_value(dev.instance().io().we,       0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 1_h);
        sim.tick();
        sim.set_input_value(dev.instance().io().req, 0_h);
        for (int b = 0; b < 4; ++b) {
            sim.set_input_value(dev.instance().io().r_valid, 1_h);
            sim.set_input_value(dev.instance().io().r_data,
                                static_cast<uint64_t>(0x11111111ull * (b + 1)));
            sim.set_input_value(dev.instance().io().r_last,
                                b == 3 ? 1_h : 0_h);
            sim.tick();
        }
        sim.set_input_value(dev.instance().io().r_valid, 0_h);
        sim.set_input_value(dev.instance().io().r_last,  0_h);

        // 4 个 offset 各自读回**对应的 lane**
        for (uint64_t lane = 0; lane < 4; ++lane) {
            sim.set_input_value(dev.instance().io().addr,     kBase + lane * 4);
            sim.set_input_value(dev.instance().io().req,      1_h);
            sim.set_input_value(dev.instance().io().we,       0_h);
            sim.set_input_value(dev.instance().io().ar_ready, 0_h);
            sim.tick();
            INFO("lane " << lane);
            REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().miss)) == 0u);
            REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().rdata)) ==
                static_cast<uint64_t>(0x11111111ull * (lane + 1)));
        }
    });

// 11. hit ⇒ ready 与 rdata 同拍（AC3）
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - hit returns ready and data in the same cycle",
    "[cache][sync][dcache][hit-latency][dual]",
    [&]() {
        constexpr uint64_t kBase = 0x6000;
        sim.set_input_value(dev.instance().io().addr,     kBase);
        sim.set_input_value(dev.instance().io().req,      1_h);
        sim.set_input_value(dev.instance().io().we,       0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 1_h);
        sim.tick();
        sim.set_input_value(dev.instance().io().req, 0_h);
        for (int b = 0; b < 4; ++b) {
            sim.set_input_value(dev.instance().io().r_valid, 1_h);
            sim.set_input_value(dev.instance().io().r_data,
                                static_cast<uint64_t>(b == 0 ? 0xABCDEF01ull : 0ull));
            sim.set_input_value(dev.instance().io().r_last, b == 3 ? 1_h : 0_h);
            sim.tick();
        }
        sim.set_input_value(dev.instance().io().r_valid, 0_h);
        sim.set_input_value(dev.instance().io().r_last,  0_h);

        // hit 读：ready 与 rdata **同拍**有效
        sim.set_input_value(dev.instance().io().addr,     kBase);
        sim.set_input_value(dev.instance().io().req,      1_h);
        sim.set_input_value(dev.instance().io().we,       0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 0u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rdata)) == 0xABCDEF01ull);
    });

// 12. miss ⇒ ready 保持 0（AC4 · 反压语义）
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - miss holds ready low until fill completes",
    "[cache][sync][dcache][miss-backpressure][dual]",
    [&]() {
        // 冷地址 ⇒ miss ⇒ ready MUST 为 0（流水线靠 `data_ready` 反压等待）
        sim.set_input_value(dev.instance().io().addr,     0x7000);
        sim.set_input_value(dev.instance().io().req,      1_h);
        sim.set_input_value(dev.instance().io().we,       0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 1_h);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 0u);
        // fill 进行中（beat 0..2）⇒ ready 仍 0
        sim.set_input_value(dev.instance().io().req, 0_h);
        for (int b = 0; b < 3; ++b) {
            sim.set_input_value(dev.instance().io().r_valid, 1_h);
            sim.set_input_value(dev.instance().io().r_data,  static_cast<uint64_t>(0));
            sim.set_input_value(dev.instance().io().r_last,  0_h);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().ready)) == 0u);
        }
        // 最后一个 beat（r_last）⇒ fill 完成那拍 ready 拉高 1 拍
        sim.set_input_value(dev.instance().io().r_last, 1_h);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 1u);
    });
