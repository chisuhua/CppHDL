// tests/test_dual_backend_template.cpp
//
// Self-validation test for the DUAL_BACKEND_TEST_PAIR macro. Registers
// a single fixture (Counter<32>) and runs 5 cycles; the macro
// automatically drives the same body through InterpreterBackend and
// VerilatorBackend.
//
// Tagged [dual][template] so it can be filtered. When verilator is not
// on PATH the verilator half simply returns early with a WARN
// (per spec R4 + dual_backend_fixture.h header); the interpreter half
// always runs.
//
// Reference impls for the macro pattern:
//   - tests/test_verilator_trace_parity.cpp:270-316 (Counter32TwentyCycles)
//   - tests/test_verilator_e2e_harness.cpp (CounterFixture<N> + per-cycle tick)
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "device.h"
#include "simulator.h"

#include "dual_backend_fixture.h"

using namespace ch;
using namespace ch::core;

namespace {

template <unsigned N>
class CounterFixture : public ch::Component {
public:
    __io(ch_out<ch_uint<N>> out;)
    CounterFixture(ch::Component *parent = nullptr,
                   const std::string &name = "counter_template")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<N>> reg(0_d);
        reg->next = reg + 1_d;
        io().out = reg;
    }
};

} // namespace

// ----------------------------------------------------------------------------
// Self-validation: 5-cycle parity check on a ch_uint<32> counter.
// ch_uint<32> wraps at 4 billion cycles — well beyond the 5 we run.
// Both backends must yield cycle-1..5 outputs (1, 2, 3, 4, 5) on `out`.
// ----------------------------------------------------------------------------
DUAL_BACKEND_TEST_PAIR(
    CounterFixture<32>,
    "DUAL_BACKEND template Counter 5-cycle",
    "[dual][template][counter]",
    [&]() {
        for (uint32_t cycle = 1; cycle <= 5; ++cycle) {
            sim.tick();
            uint64_t val = static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().out));
            REQUIRE(val == cycle);
        }
    });