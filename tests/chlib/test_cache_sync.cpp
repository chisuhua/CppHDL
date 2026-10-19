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
// Task 4: DCacheSync scenarios (4 TEST_CASE_PAIRs)
// ============================================================================

// 6. DCache cold miss read
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - Cold miss read generates AXI request",
    "[cache][sync][dcache][cold-miss][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().addr, 0x8000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   0_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_addr)) == (uint64_t)0x8000);
    });

// 7. DCache refilled read → hit
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - Refilled read returns cached data",
    "[cache][sync][dcache][refilled-read][dual]",
    [&]() {
        // cycle 1: cold read
        sim.set_input_value(dev.instance().io().addr, 0x9000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   0_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        // cycle 2: AXI resp brings data
        sim.set_input_value(dev.instance().io().req,  0_h);
        sim.set_input_value(dev.instance().io().axi_ready,    1_h);
        sim.set_input_value(dev.instance().io().axi_resp_valid, 1_h);
        sim.set_input_value(dev.instance().io().axi_rdata, 0xAABBCCDD_h);
        sim.tick();

        // cycle 3: re-read same addr → hit, rdata from cache
        sim.set_input_value(dev.instance().io().addr, 0x9000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   0_h);
        sim.set_input_value(dev.instance().io().axi_ready, 0_h);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 0u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().rdata)) == 0xAABBCCDDu);
    });

// 8. DCache hit write write-through
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - Hit write triggers AXI write-through",
    "[cache][sync][dcache][write-through][dual]",
    [&]() {
        // Pre-fill 0xA000 via read
        sim.set_input_value(dev.instance().io().addr, 0xA000_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   0_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        sim.set_input_value(dev.instance().io().req,  0_h);
        sim.set_input_value(dev.instance().io().axi_ready,    1_h);
        sim.set_input_value(dev.instance().io().axi_resp_valid, 1_h);
        sim.set_input_value(dev.instance().io().axi_rdata, 0x0_h);
        sim.tick();

        // cycle 3: write to cache line (hit case after fill)
        sim.set_input_value(dev.instance().io().addr, 0xA000_h);
        sim.set_input_value(dev.instance().io().wdata, 0x12345678_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   1_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        // Write-through must show AXI side
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_we)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_wdata)) == 0x12345678u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_addr)) == (uint64_t)0xA000);
    });

// 9. DCache miss write (write-allocate)
DUAL_BACKEND_TEST_PAIR(
    DCacheSync,
    "DCacheSync - Miss write still forwards to AXI",
    "[cache][sync][dcache][miss-write][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().addr, 0xB000_h);
        sim.set_input_value(dev.instance().io().wdata, 0xDEADF00D_h);
        sim.set_input_value(dev.instance().io().req,  1_h);
        sim.set_input_value(dev.instance().io().we,   1_h);
        sim.set_input_value(dev.instance().io().axi_ready, 1_h);
        sim.tick();

        // Even on miss, write must propagate
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_we)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_wdata)) == 0xDEADF00Du);
    });
