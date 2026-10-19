// tests/test_dual_backend_64_bit.cpp
//
// dual-backend-test-coverage (Phase 1) — 64-bit port boundary fixture
// (spec R10 §"64-bit port boundary" + R5 width cap).
//
// Two cases, both using DUAL_BACKEND_TEST_PAIR so the macro path
// itself is exercised (per review feedback 2026-10-19: raw halves
// don't exercise the macro's R5 pre-flight width scan).
//
//   1. CounterWide64 — ch_out<ch_uint<64>>: AT the cap. Both halves
//      run when verilator on PATH; interpreter half always runs.
//   2. CounterWide96 — ch_out<ch_uint<96>>: EXCEEDS the cap. The
//      macro's pre-flight width scan detects this and WARN-skips
//      the verilator half (interpreter still runs).
//
// Tagged [dual][width-cap]. Registered via add_dual_backend_test in
// tests/CMakeLists.txt so both inherit the verilator_e2e label +
// RESOURCE_LOCK + _vl_path_env injection.
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"

using namespace ch;
using namespace ch::core;

namespace {

template <unsigned N>
class CounterWide : public ch::Component {
public:
    __io(ch_out<ch_uint<N>> out;)

    CounterWide(ch::Component *parent = nullptr,
                const std::string &name = "counter_wide")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<N>> reg(0_d);
        reg->next = reg + 1_d;
        io().out = reg;
    }
};

} // namespace

// CounterWide<64>: exactly AT the ADR-035 cap. Both halves run
// (verilator compiles the design since ch_out<64> fits the cap).
DUAL_BACKEND_TEST_PAIR(
    CounterWide<64>,
    "DUAL_BACKEND 64-bit cap - CounterWide64 dual",
    "[dual][width-cap]",
    [&]() {
        for (uint32_t cycle = 1; cycle <= 50; ++cycle) {
            sim.tick();
            REQUIRE(static_cast<uint64_t>(
                        sim.get_port_value(dev.instance().io().out)) ==
                    cycle);
        }
    });

// CounterWide<96>: EXCEEDS the 64-bit cap. The macro's pre-flight
// width scan detects this BEFORE calling try_initialize and
// WARN-skips the verilator half (saves the minutes-long verilator
// compile attempt). Interpreter half still runs to completion.
// This TEST_CASE is the R5 macro-path coverage (vs the legacy
// non-macro `WideSignalRejection` in test_verilator_backend.cpp
// which uses raw halves).
DUAL_BACKEND_TEST_PAIR(
    CounterWide<96>,
    "DUAL_BACKEND 64-bit cap - CounterWide96 verilator skips",
    "[dual][width-cap][verilator-skip-wide]",
    [&]() {
        // Interpreter half always runs (ch_out ch_uint<96> > 64 cap
        // is fine for InterpreterBackend).
        for (uint32_t cycle = 1; cycle <= 10; ++cycle) {
            sim.tick();
            REQUIRE(static_cast<uint64_t>(
                        sim.get_port_value(dev.instance().io().out)) ==
                    cycle);
        }
    });