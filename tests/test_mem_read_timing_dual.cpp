// tests/test_mem_read_timing_dual.cpp
//
// Cross-backend parity for the memory read port timing contract
// (capability `mem-read-timing`).
//
// The claim under test: the Simulator's `ch_mem::aread` is 0-cycle, exactly
// like the generated Verilog `assign proxy = mem[addr];` — so the interpreter
// and the Verilator backend MUST observe the SAME value for a register-driven
// read address within the same cycle.
//
// Before the `sim-mem-read-timing-align-2026-10` change this could not be
// tested: the Simulator's aread lagged one cycle behind the Verilog `assign`,
// so parity failed *by design* and aread-based designs were excluded from
// the dual-backend matrix (see the `dual-backend-test-coverage` capability's
// exclusion list).
//
// The DUT exposes the read data through top-level `ch_out` ports — the
// Verilator backend cannot observe deep-internal memory / reg-file state
// through its dlopen accessor table.

#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include <cstdint>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// A memory whose read address is driven by a register (the FIFO `raddr`
// pattern) — the shape that used to expose the 1-cycle aread skew.
class RegAddrMem : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> next_addr; ch_out<ch_uint<8>> rdata_async;
         ch_out<ch_uint<8>> rdata_sync;)

        RegAddrMem(ch::Component *parent = nullptr,
                   const std::string &name = "reg_addr_mem")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        std::vector<uint8_t> init = {0xA1, 0xB2, 0xC3, 0xD4,
                                     0xE5, 0xF6, 0x07, 0x18,
                                     0x29, 0x3A, 0x4B, 0x5C,
                                     0x6D, 0x7E, 0x8F, 0x90};
        ch_mem<ch_uint<8>, 16> mem(init, "dual_read_mem");

        // The address passes through a `ch_reg`; its value proxy is refreshed
        // in the pre-edge combinational pass.
        ch_reg<ch_uint<4>> addr(0_d, "addr");
        addr->next = io().next_addr;

        // aread: 0-cycle (Verilog `assign proxy = mem[addr];`).
        auto async_port =
            mem.aread(ch_uint<4>(addr.impl()), "dual_async_port");
        io().rdata_async = ch_uint<8>(async_port.impl());

        // sread: 1-cycle (Verilog `always_ff`).
        auto sync_port = mem.sread(ch_uint<4>(addr.impl()), ch_bool(true),
                                   "dual_sync_port");
        io().rdata_sync = ch_uint<8>(sync_port.impl());
    }
};

}  // anonymous namespace

DUAL_BACKEND_TEST_PAIR(
    RegAddrMem,
    "mem read port timing: aread is 0-cycle, sread is 1-cycle",
    "[mem][dual][read-timing]",
    [&]() {
        auto &io = dev.instance().io();

        // Tick 1: the address register loads 4 (mem[4] = 0xE5).
        sim.set_input_value(io.next_addr, 4);
        sim.tick();

        // aread is 0-cycle: the new address is reflected in the SAME tick.
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(io.rdata_async)) == 0xE5u);
        // sread is 1-cycle: it still observes the pre-edge address (0).
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(io.rdata_sync)) == 0xA1u);

        // Tick 2: the address register loads 3 (mem[3] = 0xD4).
        sim.set_input_value(io.next_addr, 3);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(io.rdata_async)) == 0xD4u);
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(io.rdata_sync)) == 0xE5u);

        // Tick 3: settle — the sync port catches up with the address
        // committed during tick 2.
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(io.rdata_async)) == 0xD4u);
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(io.rdata_sync)) == 0xD4u);
    });
