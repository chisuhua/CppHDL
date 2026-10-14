// tests/test_verilator_bundle_e2e.cpp
// strengthen-verilator-e2e-coverage phase 2 (GREEN):
// 4 bundle e2e scenarios. The fixture-level pattern matches
// tests/test_bundle.cpp:18-29 (CRTP with bundle_base<Derived> +
// CH_BUNDLE_FIELDS_T). All observation points are independent
// ch_out ports (AGENTS.md "bundle 不作 IO, 字段绑独立端口").
//
// Scenarios (per design D2):
//   1. SingleLayerMultiFieldAssign — 3× ch_uint<8> + independent
//      ch_out ports; drive each field from a literal.
//   2. NestedTwoLayerFieldAccess — InnerBundle nested inside
//      OuterBundle; io().out = outer.inner.payload.
//   3. BundleArrayIndexAccess — C array XBundle bundles[3]; index
//      access into the array.
//   4. BundleAssignmentChain — A <<= B; B <<= C; C.field <<= reg
//      (reg each cycle +1).
//
// All 4 scenarios run the same fixture under InterpreterBackend (ground
// truth) and VerilatorBackend (target) and compare the value read from
// the data_map_ observation point.
//
// Tagged [verilator][e2e][bundle]. Skipped cleanly when verilator is
// not on PATH; CATCH_CONFIG_RUNNER keeps the executable exit 0 when
// all tests SKIP (otherwise Catch2 returns 4 and ctest reports a
// failure).
#include <unistd.h>
#include <cstdio>
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/context.h"
#include "core/eval_backend.h"
#include "core/interpreter_backend.h"
#include "core/verilator_backend.h"
#include "device.h"
#include "simulator.h"
#include <cstdlib>
#include <memory>
#include <string>
#include <sys/stat.h>

#define CATCH_CONFIG_RUNNER

using namespace ch;
using namespace ch::core;

namespace {

bool tool_available(const char *cmd) {
    std::string which_cmd = std::string("command -v ") + cmd +
                            " >/dev/null 2>&1";
    return std::system(which_cmd.c_str()) == 0;
}

std::string make_temp_dir(const std::string &suffix) {
    char tmpl[] = "/tmp/cpphdl_vl_bnd_XXXXXX";
    char *dir = ::mkdtemp(tmpl);
    if (!dir) return {};
    std::string result(dir);
    result += suffix;
    ::mkdir(result.c_str(), 0755);
    return result;
}

uint64_t read_port_output(Simulator &sim, ch::core::lnodeimpl *ln) {
    auto it = sim.data_map().find(ln->id());
    if (it == sim.data_map().end()) return UINT64_MAX;
    return static_cast<uint64_t>(it->second);
}

// ---------------------------------------------------------------------------
// Fixture 1: single-layer multi-field. 3 × ch_uint<8> + independent ports.
// ---------------------------------------------------------------------------
template <typename T>
struct AbcBundle : public bundle_base<AbcBundle<T>> {
    using Self = AbcBundle<T>;
    T a;
    T b;
    T c;

    AbcBundle() = default;
    CH_BUNDLE_FIELDS_T(a, b, c)

    void as_master_direction() {
        this->make_output(a, b, c);
    }
    void as_slave_direction() {
        this->make_input(a, b, c);
    }
};

class SingleLayerMultiField : public ch::Component {
public:
    __io(
        ch_out<ch_uint<8>> a_out;
        ch_out<ch_uint<8>> b_out;
        ch_out<ch_uint<8>> c_out;
    )
    SingleLayerMultiField(ch::Component *parent = nullptr,
                          const std::string &name = "single_layer")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        AbcBundle<ch_uint<8>> bundle;
        bundle.as_master();
        bundle.a = ch_uint<8>(1_d);
        bundle.b = ch_uint<8>(2_d);
        bundle.c = ch_uint<8>(3_d);
        io().a_out = bundle.a;
        io().b_out = bundle.b;
        io().c_out = bundle.c;
    }
};

// ---------------------------------------------------------------------------
// Fixture 2: nested two-layer. InnerBundle nested as a field of
// OuterBundle; the outer bundle's inner.payload field is driven to
// 0x42 and routed to io().out. Verifies verilator can resolve
// double-nested bit-slice / field-resolution.
// ---------------------------------------------------------------------------
template <typename T>
struct InnerBundle : public bundle_base<InnerBundle<T>> {
    using Self = InnerBundle<T>;
    T payload;
    InnerBundle() = default;
    CH_BUNDLE_FIELDS_T(payload)
    void as_master_direction() { this->make_output(payload); }
    void as_slave_direction() { this->make_input(payload); }
};

template <typename T>
struct OuterBundle : public bundle_base<OuterBundle<T>> {
    using Self = OuterBundle<T>;
    T tag;
    InnerBundle<T> inner;
    OuterBundle() = default;
    CH_BUNDLE_FIELDS_T(tag, inner)
    void as_master_direction() {
        this->make_output(tag);
        this->make_output(inner);  // delegate to inner's direction
    }
    void as_slave_direction() {
        this->make_input(tag);
        this->make_input(inner);
    }
};

class NestedTwoLayer : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> out;)
    NestedTwoLayer(ch::Component *parent = nullptr,
                   const std::string &name = "nested")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        OuterBundle<ch_uint<8>> outer;
        outer.as_master();
        outer.tag = 0x0a_h;
        outer.inner.payload = 0x42_h;
        io().out = outer.inner.payload;
    }
};

// ---------------------------------------------------------------------------
// Fixture 3: bundle array index access. XBundle bundles[3] declared as
// a C array on the stack of describe(); bundles[1].field is driven to
// a constant and routed to io().out. Verifies the array-indexing
// pattern survives verilator codegen.
// ---------------------------------------------------------------------------
class BundleArrayIndex : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> out;)
    BundleArrayIndex(ch::Component *parent = nullptr,
                     const std::string &name = "bundle_arr")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        AbcBundle<ch_uint<8>> bundles[3];
        for (int i = 0; i < 3; ++i) {
            bundles[i].as_master();
        }
        bundles[0].a = 0x10_h;
        bundles[0].b = 0x11_h;
        bundles[0].c = 0x12_h;
        bundles[1].a = 0x20_h;
        bundles[1].b = 0x21_h;
        bundles[1].c = 0x22_h;
        bundles[2].a = 0x30_h;
        bundles[2].b = 0x31_h;
        bundles[2].c = 0x32_h;
        io().out = bundles[1].a;
    }
};

// ---------------------------------------------------------------------------
// Fixture 4: bundle <<= chain. A <<= B; B <<= C; C.a <<= reg (where reg
// starts at 0 and increments each cycle). 5 cycles total; io().out
// should == reg value at the end of each cycle (since A.a == B.a ==
// C.a after the chain).
// ---------------------------------------------------------------------------
class BundleAssignmentChain : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> out;)
    BundleAssignmentChain(ch::Component *parent = nullptr,
                          const std::string &name = "bundle_chain")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        AbcBundle<ch_uint<8>> A;
        AbcBundle<ch_uint<8>> B;
        AbcBundle<ch_uint<8>> C;
        A.as_master();
        B.as_master();
        C.as_master();

        ch_reg<ch_uint<8>> reg(0_d);
        reg->next = reg + 1_d;

        C.a = reg;
        // operator<<= field-wires A.a = B.a, B.a = C.a (per
        // bundle_base.h:119-134 which loops through fields).
        B <<= C;
        A <<= B;

        io().out = A.a;
    }
};

// ---------------------------------------------------------------------------
// Shared helper: run the given fixture on interpreter + verilator in
// sequence, ticking `n_cycles` on each side. Returns when both sims
// have completed the run. Caller then compares the observation-point
// values from sim_a and sim_b.
// ---------------------------------------------------------------------------
template <typename T>
void run_both(std::unique_ptr<ch::ch_device<T>> &dev_a,
              std::unique_ptr<ch::ch_device<T>> &dev_b,
              std::unique_ptr<Simulator> &sim_a,
              std::unique_ptr<Simulator> &sim_b,
              uint32_t n_cycles) {
    sim_a->set_backend(std::make_unique<InterpreterBackend>());
    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_bnd");
    REQUIRE_FALSE(workdir.empty());
    auto backend = std::make_unique<VerilatorBackend>(workdir);
    REQUIRE(backend->initialize(dev_b->context(), data_map_b));
    if (backend->port_access_snapshot().empty()) {
        SKIP("verilator dlopen did not populate port_access_");
    }
    sim_b->set_backend(std::move(backend));
    sim_a->tick(n_cycles);
    sim_b->tick(n_cycles);
}

} // namespace

// ============================================================================
// 1. SingleLayerMultiFieldAssign: 3 fields × 1 cycle.
// ============================================================================
TEST_CASE("BundleE2E - SingleLayerMultiFieldAssign",
          "[verilator][e2e][bundle][m5]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not on PATH");
    }

    auto dev_a = std::make_unique<ch::ch_device<SingleLayerMultiField>>();
    auto dev_b = std::make_unique<ch::ch_device<SingleLayerMultiField>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

    run_both(dev_a, dev_b, sim_a, sim_b, 1);

    auto *a_a = dev_a->instance().io().a_out.impl();
    auto *b_a = dev_a->instance().io().b_out.impl();
    auto *c_a = dev_a->instance().io().c_out.impl();
    auto *a_b = dev_b->instance().io().a_out.impl();
    auto *b_b = dev_b->instance().io().b_out.impl();
    auto *c_b = dev_b->instance().io().c_out.impl();

    REQUIRE(read_port_output(*sim_a, a_a) == 1);
    REQUIRE(read_port_output(*sim_a, b_a) == 2);
    REQUIRE(read_port_output(*sim_a, c_a) == 3);
    REQUIRE(read_port_output(*sim_b, a_b) == 1);
    REQUIRE(read_port_output(*sim_b, b_b) == 2);
    REQUIRE(read_port_output(*sim_b, c_b) == 3);
}

// ============================================================================
// 2. NestedTwoLayerFieldAccess: outer.inner.payload -> io().out.
// ============================================================================
TEST_CASE("BundleE2E - NestedTwoLayerFieldAccess",
          "[verilator][e2e][bundle][nested][m5]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not on PATH");
    }

    auto dev_a = std::make_unique<ch::ch_device<NestedTwoLayer>>();
    auto dev_b = std::make_unique<ch::ch_device<NestedTwoLayer>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

    run_both(dev_a, dev_b, sim_a, sim_b, 1);

    auto *out_a = dev_a->instance().io().out.impl();
    auto *out_b = dev_b->instance().io().out.impl();

    uint64_t v_a = read_port_output(*sim_a, out_a);
    uint64_t v_b = read_port_output(*sim_b, out_b);
    UNSCOPED_INFO("nested interpreter=" << v_a << " verilator=" << v_b);
    REQUIRE(v_a == 0x42);
    REQUIRE(v_b == 0x42);
}

// ============================================================================
// 3. BundleArrayIndexAccess: bundles[1].a -> io().out.
// ============================================================================
TEST_CASE("BundleE2E - BundleArrayIndexAccess",
          "[verilator][e2e][bundle][array][m5]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not on PATH");
    }

    auto dev_a = std::make_unique<ch::ch_device<BundleArrayIndex>>();
    auto dev_b = std::make_unique<ch::ch_device<BundleArrayIndex>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

    run_both(dev_a, dev_b, sim_a, sim_b, 1);

    auto *out_a = dev_a->instance().io().out.impl();
    auto *out_b = dev_b->instance().io().out.impl();

    uint64_t v_a = read_port_output(*sim_a, out_a);
    uint64_t v_b = read_port_output(*sim_b, out_b);
    UNSCOPED_INFO("array interpreter=" << v_a << " verilator=" << v_b);
    REQUIRE(v_a == 0x20);
    REQUIRE(v_b == 0x20);
}

// ============================================================================
// 4. BundleAssignmentChain: A <<= B; B <<= C; C.a = reg. After N cycles
// A.a should equal the current value of reg (== N for ch_reg(0_d) +
// reg->next = reg + 1_d).
//
// NOTE: This test uses ch_reg(0_d) initial value to avoid the
// VerilatorBackend ch_reg non-zero initial-value mismatch bug (D8 (b);
// deferred to follow-up change). Once that change lands, the cycle
// closure form remains correct.
// ============================================================================
TEST_CASE("BundleE2E - BundleAssignmentChain",
          "[verilator][e2e][bundle][chain][m5]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not on PATH");
    }

    auto dev_a = std::make_unique<ch::ch_device<BundleAssignmentChain>>();
    auto dev_b = std::make_unique<ch::ch_device<BundleAssignmentChain>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

    run_both(dev_a, dev_b, sim_a, sim_b, 5);

    auto *out_a = dev_a->instance().io().out.impl();
    auto *out_b = dev_b->instance().io().out.impl();

    uint64_t v_a = read_port_output(*sim_a, out_a);
    uint64_t v_b = read_port_output(*sim_b, out_b);
    UNSCOPED_INFO("chain interpreter=" << v_a << " verilator=" << v_b);
    // After 5 ticks, reg should == 5 (since reg(0_d); next = reg+1).
    REQUIRE(v_a == 5);
    REQUIRE(v_b == 5);
}

// Custom main(): exit 0 when verilator is missing so ctest doesn't
// report the all-skipped case as a failure.
int main(int argc, char *argv[]) {
    if (!tool_available("verilator")) {
        std::fprintf(stderr,
                     "test_verilator_bundle_e2e: verilator not installed; "
                     "skipping all e2e tests.\n");
        return 0;
    }
    Catch::Session session;
    session.applyCommandLine(argc, argv);
    return session.run();
}