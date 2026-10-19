// tests/test_dual_backend_clock.cpp
//
// dual-backend-test-coverage (Phase 1) — 2-step clock toggle fixture
// (spec R10 §"2-step clock toggle fixture").
//
// Validates the verilator-backend-e2e spec §"3-step clock toggle"
// contract: counter-out after 1 cycle should equal 1 in both
// InterpreterBackend and VerilatorBackend. This proves the
// `eval_sequential` 2-step clock toggle (clk=1 → eval_fn_ → clk=0
// → eval_fn_) fires `always_ff @(posedge clk)` correctly.
//
// Tagged [dual][clock]. Uses a ch_uint<32> counter (wraps at
// 4 billion cycles — far beyond what we run).
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"

using namespace ch;
using namespace ch::core;

namespace {

template <unsigned N>
class CounterClock : public ch::Component {
public:
    __io(ch_out<ch_uint<N>> out;)

    CounterClock(ch::Component *parent = nullptr,
                 const std::string &name = "counter_clock")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<N>> reg(0_d);
        reg->next = reg + 1_d;
        io().out = reg;
    }
};

} // namespace

// 1-cycle check: counter increments to 1 in both halves.
DUAL_BACKEND_TEST_PAIR(
    CounterClock<32>,
    "DUAL_BACKEND 2-step clock toggle - CounterClock 1-cycle",
    "[dual][clock]",
    [&]() {
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().out)) == 1u);
    });