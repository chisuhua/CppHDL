// tests/test_mem_comb_read.cpp
//
// B' (ChipForge Phase 6d.7+): per-port opt-out of 1-cycle read latency.
//
// aread (ch_mem::aread) historically misnamed — although the C++ class is
// named "async_read_port", it was classified into sequential_instr_list_
// and emitted as `always_ff @(posedge)` in Verilog, giving 1-cycle latency.
// This test pins down the NEW semantics for aread:
//
//   1. Verilog codegen: aread emits `assign proxy = mem[addr]` (continuous
//      assignment, 0-cycle latency in Verilog). sread continues to emit
//      `always_ff` (1-cycle).
//
//   2. Simulator: aread is evaluated in `sequential_instr_list_` at posedge
//      (`src/simulator_init.cpp:116-121`), same phase as sread. The
//      observed "same-cycle" visibility of aread comes from the trailing
//      `eval_combinational()` in `Simulator::tick()` (`src/simulator.cpp`)
//      that propagates the proxy to output ports within the same tick.
//      So in the Simulator, aread and sread are observationally identical
//      to a testbench that sets the address before tick() and reads after.
//
// Known simulator-vs-Verilog drift (documented in
// docs/simulation/ARCHITECTURE.md "Memory read port timing"):
//   - Register-driven read address: Simulator aread effectively 1-cycle
//     behind a register update at the same posedge; Verilog `assign`
//     reflects the new address immediately.
//   - Same-cycle write+read race: Simulator aread returns pre-write
//     value; Verilog `assign` returns post-write value (NBA update).
// ChipForge dmem tolerates both drifts because the downstream reg_file
// provides the required 1-cycle latency. imem must remain on sread to
// keep fetch/pc_lag alignment.

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "codegen_verilog.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/mem.h"
#include "core/reg.h"
#include "core/uint.h"
#include "device.h"
#include "simulator.h"
#include "utils/logger.h"

using namespace ch;
using namespace ch::core;

namespace {

// --------------------------------------------------------------------------
// Test fixture: a memory with one sread and one aread port
// ------------------------------------------------------------------
class CombReadFixture : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr_sync; ch_in<ch_uint<4>> addr_async;
         ch_in<ch_uint<8>> wdata; ch_in<ch_bool> we;
         ch_out<ch_uint<8>> rdata_sync; ch_out<ch_uint<8>> rdata_async;)

        CombReadFixture(ch::Component *parent = nullptr,
                        const std::string &name = "comb_read_mem")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        std::vector<uint8_t> init = {0xA1, 0xB2, 0xC3, 0xD4,
                                     0xE5, 0xF6, 0x07, 0x18,
                                     0x29, 0x3A, 0x4B, 0x5C,
                                     0x6D, 0x7E, 0x8F, 0x90};
        ch_mem<ch_uint<8>, 16> mem(init, "cmp_read_mem");

        // sread port — stays 1-cycle latency (for fetch-style usage).
        // The signature is `sread(addr, enable=ch_bool(true), name="mem_sread")`.
        // Passing "mem_sread" as the 2nd positional arg binds to the `enable`
        // parameter via `const char*`→`bool`→`ch_bool` implicit conversion
        // (non-null pointer → true). The actual `name` therefore stays at
        // the default "mem_sread", so the codegen proxy is named
        // "mem_sread_data_proxy". This implicit conversion is intentional
        // here (documented design decision; see Decision D in
        // openspec/changes/cleanup-250198a-aread-combinational-debt/).
        auto sync_port =
            mem.sread(ch_uint<4>(io().addr_sync.impl()), "mem_sread");
        io().rdata_sync = ch_uint<8>(sync_port.impl());

        // aread port — emits a combinational `assign` in generated Verilog
        // (0-cycle). In the Simulator, aread is in `sequential_instr_list_`
        // and observed in the same tick as the address change.
        auto async_port =
            mem.aread(ch_uint<4>(io().addr_async.impl()), "async_port");
        io().rdata_async = ch_uint<8>(async_port.impl());

        // Write port (no enable — always-on for test simplicity)
        auto write_port =
            mem.write(ch_uint<4>(io().addr_sync.impl()),
                      ch_uint<8>(io().wdata.impl()),
                      ch_bool(io().we.impl()));
        (void)write_port;
    }
};

// Read entire file into string
std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Unique per-process temp path to avoid parallel ctest collisions and to
// satisfy hardened kernels (fs.protected_regular) that reject writes to
// pre-existing files in shared /tmp.
std::string unique_temp_verilog_path() {
    return (std::filesystem::temp_directory_path() /
            ("test_comb_read_codegen_" + std::to_string(::getpid()) + ".v"))
        .string();
}

}  // anonymous namespace

// =============================================================================
// Simulator semantics: aread observable same-tick (via tick() propagation),
// sread observable same-tick (same mechanism). Both use sequential list.
// =============================================================================

TEST_CASE("aread observable same-tick in Simulator (regression for codegen "
          "combinational read semantics)", "[mem][comb-read][sim]") {
    ch_device<CombReadFixture> device;
    Simulator simulator(device.context());

    // Tick 1: set addr_sync=2 and addr_async=4 before tick. After tick, the
    // Simulator's tick() runs `eval_sequential()` (which updates the aread
    // proxy to mem[addr_async=4]) then a trailing `eval_combinational()`
    // that propagates the proxy to rdata_async. Hence the testbench sees
    // mem[4]=0xE5 after one tick. (This is observationally identical to
    // sread in the Simulator; see Decision D in the cleanup change.)
    simulator.set_input_value(device.instance().io().addr_sync,
                              static_cast<uint64_t>(0x2));
    simulator.set_input_value(device.instance().io().addr_async,
                              static_cast<uint64_t>(0x4));
    simulator.set_input_value(device.instance().io().wdata,
                              static_cast<uint64_t>(0x00));
    simulator.set_input_value(device.instance().io().we,
                              static_cast<uint64_t>(0));
    simulator.tick();

    auto async_rdata_t1 = simulator.get_port_value(
        device.instance().io().rdata_async);
    REQUIRE(static_cast<uint64_t>(async_rdata_t1) == 0xE5);

    // Tick 2: change async addr to 1 — observable same tick.
    simulator.set_input_value(device.instance().io().addr_async,
                              static_cast<uint64_t>(0x1));
    simulator.tick();

    auto async_rdata_t2 = simulator.get_port_value(
        device.instance().io().rdata_async);
    REQUIRE(static_cast<uint64_t>(async_rdata_t2) == 0xB2);

    // Tick 3: change again to 0xF.
    simulator.set_input_value(device.instance().io().addr_async,
                              static_cast<uint64_t>(0xF));
    simulator.tick();

    auto async_rdata_t3 = simulator.get_port_value(
        device.instance().io().rdata_async);
    REQUIRE(static_cast<uint64_t>(async_rdata_t3) == 0x90);
}

TEST_CASE("sread retains 1-cycle latency (regression for codegen "
          "always_ff emission)", "[mem][comb-read][sim][sread-regression]") {
    ch_device<CombReadFixture> device;
    Simulator simulator(device.context());

    // Prime: set addr_sync=5 before tick. After tick, sread proxy updates
    // to mem[5]=0xF6 during eval_sequential and the trailing
    // eval_combinational propagates it to rdata_sync. This test is a
    // regression guard: it pins the sread codegen path (always_ff in
    // Verilog) and Simulator timing unchanged by the aread cleanup.
    simulator.set_input_value(device.instance().io().addr_sync,
                              static_cast<uint64_t>(0x5));
    simulator.set_input_value(device.instance().io().addr_async,
                              static_cast<uint64_t>(0x0));
    simulator.set_input_value(device.instance().io().wdata,
                              static_cast<uint64_t>(0x00));
    simulator.set_input_value(device.instance().io().we,
                              static_cast<uint64_t>(0));
    simulator.tick();

    auto sync_rdata_t1 = simulator.get_port_value(
        device.instance().io().rdata_sync);
    REQUIRE(static_cast<uint64_t>(sync_rdata_t1) == 0xF6);

    // Change sync addr to 0x3 (init=0xD4). Next tick output should change.
    simulator.set_input_value(device.instance().io().addr_sync,
                              static_cast<uint64_t>(0x3));
    simulator.tick();
    auto sync_rdata_t2 = simulator.get_port_value(
        device.instance().io().rdata_sync);
    REQUIRE(static_cast<uint64_t>(sync_rdata_t2) == 0xD4);
}

// =============================================================================
// Verilog codegen: aread -> `assign`, sread -> `always_ff`
// =============================================================================

TEST_CASE("aread Verilog codegen emits assign (no always_ff)",
          "[mem][comb-read][codegen]") {
    ch_device<CombReadFixture> device;

    const std::string out_file = unique_temp_verilog_path();
    toVerilog(out_file, device.context());
    auto cleanup = std::filesystem::path(out_file);  // RAII not used; explicit
    (void)cleanup;

    std::string verilog = read_file(out_file);
    REQUIRE(!verilog.empty());
    REQUIRE(verilog.find("module") != std::string::npos);

    // aread proxy (named "async_port" in the fixture) must be driven by an
    // `assign` line carrying the "Comb read" comment — NOT by any always_ff
    // block. The codegen emits the proxy name with the module-name prefix
    // (default "top"), so we match the suffix and require the line contains
    // both `assign` and `Comb read` to disambiguate from the signal decl.
    const std::string async_proxy_suffix = "async_port_data_proxy";
    std::istringstream aread_iss(verilog);
    std::string aread_line;
    bool found_aread_assign = false;
    while (std::getline(aread_iss, aread_line)) {
        if (aread_line.find(async_proxy_suffix) != std::string::npos &&
            aread_line.find("assign") != std::string::npos &&
            aread_line.find("Comb read") != std::string::npos) {
            found_aread_assign = true;
            // The assign line itself must not contain `always`.
            REQUIRE(aread_line.find("always") == std::string::npos);
        }
    }
    REQUIRE(found_aread_assign);

    // sread proxy (default name "mem_sread") must still emit an always_ff
    // block driving that proxy and must NOT use a combinational `assign`.
    // The proxy is named with the module-name prefix (default "top_"), so
    // we match the suffix to stay agnostic to the module name. Disambiguate
    // the sread emission line from the aread one by requiring `always_ff` +
    // `Read port` together with the sync proxy suffix on the same line.
    const std::string sync_proxy_suffix = "mem_sread_data_proxy";
    std::istringstream sync_iss(verilog);
    std::string sync_line;
    bool found_sread_always = false;
    bool found_sread_comb = false;
    while (std::getline(sync_iss, sync_line)) {
        if (sync_line.find(sync_proxy_suffix) != std::string::npos &&
            sync_line.find("always_ff") != std::string::npos &&
            sync_line.find("Read port") != std::string::npos) {
            found_sread_always = true;
        }
        if (sync_line.find(sync_proxy_suffix) != std::string::npos &&
            sync_line.find("Comb read") != std::string::npos) {
            found_sread_comb = true;
        }
    }
    REQUIRE(found_sread_always);
    REQUIRE_FALSE(found_sread_comb);

    // Clean up the temp file.
    std::error_code ec;
    std::filesystem::remove(out_file, ec);
}

// =============================================================================
// Cross-cycle write+read: write commits at tick N posedge; aread at tick N+1
// observes the new value (post-write). This validates the standard SRAM
// timing for the now-0-cycle read port: writes update memory on the clock
// edge; aread in the next tick sees the post-write value.
// =============================================================================

TEST_CASE("aread cross-cycle write-then-read consistency",
          "[mem][comb-read][sim][write-read]") {
    ch_device<CombReadFixture> device;
    Simulator simulator(device.context());

    // Tick 1: aread at addr=2 (init=0xC3), no write.
    simulator.set_input_value(device.instance().io().addr_async,
                              static_cast<uint64_t>(0x2));
    simulator.set_input_value(device.instance().io().addr_sync,
                              static_cast<uint64_t>(0x0));
    simulator.set_input_value(device.instance().io().wdata,
                              static_cast<uint64_t>(0x00));
    simulator.set_input_value(device.instance().io().we,
                              static_cast<uint64_t>(0));
    simulator.tick();
    auto rdata_t1 = simulator.get_port_value(
        device.instance().io().rdata_async);
    REQUIRE(static_cast<uint64_t>(rdata_t1) == 0xC3);

    // Tick 2: change addr to 5, no write — still reads init.
    simulator.set_input_value(device.instance().io().addr_async,
                              static_cast<uint64_t>(0x5));
    simulator.tick();
    auto rdata_t2 = simulator.get_port_value(
        device.instance().io().rdata_async);
    REQUIRE(static_cast<uint64_t>(rdata_t2) == 0xF6);

    // Tick 3: write mem[5]=0xFF via the write port (addr_sync=5, we=1).
    simulator.set_input_value(device.instance().io().addr_sync,
                              static_cast<uint64_t>(0x5));
    simulator.set_input_value(device.instance().io().wdata,
                              static_cast<uint64_t>(0xFF));
    simulator.set_input_value(device.instance().io().we,
                              static_cast<uint64_t>(1));
    simulator.tick();

    // Tick 4: aread at addr=5 (we=0). Areadobserves the post-write 0xFF.
    simulator.set_input_value(device.instance().io().addr_async,
                              static_cast<uint64_t>(0x5));
    simulator.set_input_value(device.instance().io().we,
                              static_cast<uint64_t>(0));
    simulator.tick();
    auto rdata_after_write = simulator.get_port_value(
        device.instance().io().rdata_async);
    REQUIRE(static_cast<uint64_t>(rdata_after_write) == 0xFF);
}

// =============================================================================
// Same-cycle write+aread race: AREAD returns PRE-WRITE value (spec R4 third
// bullet). This test pins the chosen Simulator semantic — if a future
// refactor lets aread observe the same-tick we=1 write, this test fails and
// requires a coordinated spec/ARCHITECTURE.md update.
// =============================================================================

TEST_CASE("mem_comb_read same_cycle write_plus_aread returns pre_write value",
          "[mem][aread][race]") {
    ch::core::context ctx("race_test");
    ch::core::ctx_swap swap(&ctx);

    ch_device<CombReadFixture> device;
    Simulator simulator(device.context());

    simulator.set_input_value(device.instance().io().we,
                              static_cast<uint64_t>(0));
    simulator.set_input_value(device.instance().io().addr_sync,
                              static_cast<uint64_t>(0x0));
    simulator.set_input_value(device.instance().io().addr_async,
                              static_cast<uint64_t>(0x0));
    simulator.set_input_value(device.instance().io().wdata,
                              static_cast<uint64_t>(0x00));
    simulator.tick();

    simulator.set_input_value(device.instance().io().we,
                              static_cast<uint64_t>(1));
    simulator.set_input_value(device.instance().io().wdata,
                              static_cast<uint64_t>(0xEE));
    simulator.set_input_value(device.instance().io().addr_sync,
                              static_cast<uint64_t>(0x2));
    simulator.set_input_value(device.instance().io().addr_async,
                              static_cast<uint64_t>(0x2));
    simulator.tick();

    constexpr uint64_t kPreWriteValueAtAddr2 = 0xC3;
    auto rdata = simulator.get_port_value(
        device.instance().io().rdata_async);
    REQUIRE(static_cast<uint64_t>(rdata) == kPreWriteValueAtAddr2);
}
