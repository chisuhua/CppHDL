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
// Helper: 驱动一次完整的 4-beat AXI4-full burst fill（**I 侧专用**）
//
// 拍序手推（design.md §5.2）：
//   N     AR 握手（`ar_valid & ar_ready`）⇒ 下一拍 `fill_active` 置位
//   N+1..N+4  4 个 R beat（第 b 个落 lane b；`r_last=1` 在 N+4 收尾 = `fill_complete`）
//   N+5   line 已写入 · `ready`（`fill_done_r`）与 `data`（`miss_data_r`）**同拍**有效
//
// ⚠️ **I 侧是 `sread`（寄存器读）**：D 侧（`aread` 组合读）那套「fill 完 ⇒ tick 一次
//    就读到数据」在 I 侧**不成立** ⇒ ⇒ 本 helper 把「呈现地址（不握手，让 `miss` 与
//    addr 对齐）+ 握手 + 4 beat」全部走完，返回时处于 N+5 拍。
// ⚠️ 返回值（`ready` / `data`）**不用**断言，由调用方按自己的判据断言。
// ============================================================================

template <typename Dut>
static void icache_axi_burst_fill(ch::Simulator &sim, Dut &ic, uint32_t addr,
                                  uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3) {
    const uint32_t words[4] = {w0, w1, w2, w3};

    // ① 呈现地址 · 本拍**不**握手（`miss` 来自 1 拍 sread ⇒ 先让它与 addr 对齐）
    sim.set_input_value(ic.io().addr, static_cast<uint64_t>(addr));
    sim.set_input_value(ic.io().req, 1u);
    sim.set_input_value(ic.io().ar_ready, 0u);
    sim.tick();

    // ② AR 握手（本拍 `miss` 已反映 addr）⇒ 下一拍 `fill_active` 置位
    sim.set_input_value(ic.io().ar_ready, 1u);
    sim.tick();

    // ③ 4 个 R beat（第 b 个落 lane b）· 第 4 个 `r_last=1` 收尾
    sim.set_input_value(ic.io().req, 0u);
    sim.set_input_value(ic.io().ar_ready, 0u);
    for (int b = 0; b < 4; ++b) {
        sim.set_input_value(ic.io().r_valid, 1u);
        sim.set_input_value(ic.io().r_data, static_cast<uint64_t>(words[b]));
        sim.set_input_value(ic.io().r_last, b == 3 ? 1u : 0u);
        sim.tick();
    }
    sim.set_input_value(ic.io().r_valid, 0u);
    sim.set_input_value(ic.io().r_last, 0u);
    sim.set_input_value(ic.io().r_data, 0u);
}

// ============================================================================
// I 侧 1: 冷 miss ⇒ 发出 **4-beat INCR burst** 的 AR（AC2）
//
// ⚠️ 2026-11-28（S4c-1a）：AXI 侧由 5 根简化握手改为 **AXI4-full 读通道** ⇒
//    激励从「1 次 resp」改为 **4-beat burst**，断言端口同步改名。
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - Cold miss generates AXI4 burst request",
    "[cache][sync][icache][cold-miss][dual]",
    [&]() {
        // 冷读：initial tag_mem/valid_mem = 0 ⇒ hit=0, miss=1
        // ⚠️ 本拍**不拉** `ar_ready` ⇒ 观察 `ar_valid` 保持（AXI：VALID 不等 READY）
        sim.set_input_value(dev.instance().io().addr, 0x1000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_addr)) == (uint64_t)0x1000);
        // 手推 burst 参数：ARLEN=3（4 beat）· ARSIZE=2（4 B）· ARBURST=INCR(1)
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_len)) == 3u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_size)) == 2u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_burst)) == 1u);
    });

// ============================================================================
// I 侧 2: burst fill 完成后 `ready` 与 `data` **同拍**有效（miss 路径的数据延迟）
//
// ⚠️ 用**寄存器** `fill_done_r`（K1）：组合的 `fill_complete` 只活 1 拍 ⇒
//    tick 之后读永远 0 ⇒ 本判据在那种实现下**恒红**。
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - AXI burst fill completes with ready and data in the same cycle",
    "[cache][sync][icache][fill][dual]",
    [&]() {
        icache_axi_burst_fill(sim, dev.instance(), 0x2000,
                              0xDEADBEEFu, 0u, 0u, 0u);

        // 填充完成后那一拍：`ready` = 1（`fill_done_r`）· `data` = 请求 lane 的字
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().data)) == 0xDEADBEEFu);
    });

// ============================================================================
// I 侧 3: fill 后同地址再读 ⇒ hit（数据来自 cache）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - Refilled same addr yields hit",
    "[cache][sync][icache][refilled-hit][dual]",
    [&]() {
        icache_axi_burst_fill(sim, dev.instance(), 0x2000,
                              0xCAFEBABEu, 0u, 0u, 0u);

        // 同地址再读（关闭 AR）⇒ 1 拍后 hit
        sim.set_input_value(dev.instance().io().addr, 0x2000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 0u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 1u);
        // 数据来自 cache（`r_data` 已清 0 ⇒ 若读 AXI 数据就必须是 0）
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().data)) == 0xCAFEBABEu);
        // hit ⇒ 不再向 AXI 发请求
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_valid)) == 0u);
    });

// ============================================================================
// I 侧 4: fill 后换地址仍 miss
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - Different addr after fill still misses",
    "[cache][sync][icache][cross-set-miss][dual]",
    [&]() {
        icache_axi_burst_fill(sim, dev.instance(), 0x3000,
                              0x11111111u, 0u, 0u, 0u);

        // 新地址 ⇒ tag 不匹配 ⇒ miss + AR
        sim.set_input_value(dev.instance().io().addr, 0x4000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ar_valid)) == 1u);
    });

// ============================================================================
// I 侧 5: fill 期间 `ready` 保持 0（I 侧 ready =「请求已被接受」· 不照搬 D 侧组合 ready）
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - Miss holds ready low until fill completes",
    "[cache][sync][icache][miss-backpressure][dual]",
    [&]() {
        // 4 个 beat 的手推值（lane 0 是被请求的 lane ⇒ 完成拍 data 取它）
        const uint32_t kBeat0 = 0x0BADF00Du;
        const uint32_t kBeat1 = 0u;
        const uint32_t kBeat2 = 0u;
        const uint32_t kBeat3 = 0u;

        // cycle 0 就握手（`ar_ready=1`）⇒ fill_active 在 cycle 1 置位
        sim.set_input_value(dev.instance().io().addr, 0x7000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().ar_ready, 1_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 0u);

        // beat 0..2 ⇒ ready 仍 0
        sim.set_input_value(dev.instance().io().req, 0_h);
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        sim.set_input_value(dev.instance().io().r_valid, 1u);
        sim.set_input_value(dev.instance().io().r_data, static_cast<uint64_t>(kBeat0));
        sim.set_input_value(dev.instance().io().r_last, 0_h);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 0u);

        sim.set_input_value(dev.instance().io().r_data, static_cast<uint64_t>(kBeat1));
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 0u);

        sim.set_input_value(dev.instance().io().r_data, static_cast<uint64_t>(kBeat2));
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 0u);

        // 最后一个 beat（`r_last`）⇒ fill 完成 ⇒ **下一拍** ready 与 data 同拍有效
        sim.set_input_value(dev.instance().io().r_data, static_cast<uint64_t>(kBeat3));
        sim.set_input_value(dev.instance().io().r_last, 1_h);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().data)) == 0x0BADF00Du);
    });

// ============================================================================
// I 侧 6 (**AC1**): line 的 4 个 lane 各自可寻址（「假 line」修复的直接判据）
//
// ⚠️ 修复前 = `bits<31,0>(cache_data)` ⇒ 同一 line 的 4 个地址**恒读首字**
//    ⇒ 本用例 lane 1/2/3 必红。
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - line has 4 independently addressable lanes",
    "[cache][sync][icache][line-lane][dual]",
    [&]() {
        constexpr uint64_t kBase = 0x4000;
        // 手推值：lane b 的字 = 0x(b+1) 重复 8 次
        const uint32_t kWord0 = 0x11111111u;
        const uint32_t kWord1 = 0x22222222u;
        const uint32_t kWord2 = 0x33333333u;
        const uint32_t kWord3 = 0x44444444u;
        icache_axi_burst_fill(sim, dev.instance(), static_cast<uint32_t>(kBase),
                              kWord0, kWord1, kWord2, kWord3);

        // 4 个 offset 各自读回**对应的 lane**（每 lane 只需 1 拍 ⇒ sread 语义）
        for (uint64_t lane = 0; lane < 4; ++lane) {
            const uint64_t expected = (lane == 0) ? static_cast<uint64_t>(kWord0)
                                    : (lane == 1) ? static_cast<uint64_t>(kWord1)
                                    : (lane == 2) ? static_cast<uint64_t>(kWord2)
                                                  : static_cast<uint64_t>(kWord3);
            sim.set_input_value(dev.instance().io().addr, kBase + lane * 4);
            sim.set_input_value(dev.instance().io().req, 1_h);
            sim.set_input_value(dev.instance().io().ar_ready, 0_h);
            sim.tick();
            INFO("lane " << lane);
            REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().miss)) == 0u);
            REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().data)) == expected);
        }
    });

// ============================================================================
// I 侧 7 (**AC3**): 命中后 **1 拍**数据就绪（`sread` 语义不变）
//
// ⚠️ 判别力：读的是 **lane 2**（不是 fill 时锁存的 lane 0）⇒ 若命中路径需要
//    2 拍，本拍读到的是**上一拍的 `miss_data_r`**（= lane 0 的字）⇒ 必红。
// ============================================================================

DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - hit returns ready and data one cycle after the address",
    "[cache][sync][icache][hit-latency][dual]",
    [&]() {
        constexpr uint64_t kBase = 0x6000;
        const uint32_t kWord0 = 0xAAAAAAA1u;
        const uint32_t kWord1 = 0xAAAAAAA2u;
        const uint32_t kWord2 = 0xAAAAAAA3u;
        const uint32_t kWord3 = 0xAAAAAAA4u;
        icache_axi_burst_fill(sim, dev.instance(), static_cast<uint32_t>(kBase),
                              kWord0, kWord1, kWord2, kWord3);

        // **恰好 1 拍**后：hit · ready · lane 2 的字
        sim.set_input_value(dev.instance().io().addr, kBase + 8);
        sim.set_input_value(dev.instance().io().req, 1_h);
        sim.set_input_value(dev.instance().io().ar_ready, 0_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 0u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().ready)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().data)) == 0xAAAAAAA3u);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
            sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
        sim.set_input_value(dev.instance().io().strbe, 0xF_h);
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
