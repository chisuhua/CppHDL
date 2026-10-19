// tests/test_dual_backend_reset.cpp
//
// dual-backend-test-coverage (Phase 1) — default_reset pulse fixture
// (spec R10 §"default_reset pulse fixture" + verilator-reg-init
// spec §R1).
//
// Validates the reset pulse: a ch_reg<ch_uint<N>> with INIT_d != 0
// must report INIT after `initialize()`'s reset pulse, then INIT+1
// after cycle 1. Verifies both backends load the init value
// correctly.
//
// Tagged [dual][reset-init].
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"

using namespace ch;
using namespace ch::core;

namespace {

class CounterInit42 : public ch::Component {
public:
    __io(ch_out<ch_uint<32>> out;)

    CounterInit42(ch::Component *parent = nullptr,
                  const std::string &name = "counter_init_42")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<32>> reg(42_d);
        reg->next = reg + 1_d;
        io().out = reg;
    }
};

} // namespace

// After initialize()'s reset pulse + 1 tick, the register must
// advance from 42 to 43. Verilator's reset pulse loads the init
// value into the always_ff reset branch, then the first tick
// advances to 43. The interpreter loads `42_d` at ch_reg
// construction time; first tick advances to 43.
DUAL_BACKEND_TEST_PAIR(
    CounterInit42,
    "DUAL_BACKEND default_reset pulse - CounterInit42",
    "[dual][reset-init]",
    [&]() {
        // 1 tick: register advances from init 42 → 43.
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().out)) == 43u);
        // 2nd tick: 43 → 44.
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().out)) == 44u);
    });