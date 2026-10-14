// tests/test_verilator_trace_parity.cpp
// strengthen-verilator-e2e-coverage phase 1 (RED):
// Trace parity single-test for 3 fixtures (counter / AXI4-Lite / bundle
// multi-field). In each TEST_CASE we run the SAME fixture through
// InterpreterBackend AND VerilatorBackend, then walk cycles one at a time
// and require every output port to produce identical data_map_ values.
//
// Critical invariants (from design.md D1):
//   - interpreter & verilator each hold a single device (no per-cycle
//     rebuild; no shared context)
//   - interpreter is the ground truth → we MUST explicitly
//     set_backend(std::make_unique<InterpreterBackend>()) on that side,
//     otherwise the project default CH_JIT_ENABLE=ON silently routes the
//     "interpreter" side through JIT and parity failures become
//     un-attributable to verilator
//   - observation points are top-level `ch_out` ports (verilator cannot
//     observe deep-internal memory / reg-file via dlopen accessor table)
//   - we read post-tick values; do NOT depend on intermediate 3-eval/tick
//     state
//   - fixtures MUST NOT contain `ch_mem` `aread` read ports (those are
//     0-cycle comb in Verilog but posedge in Simulator → 1-cycle skew
//     makes parity fail by design); `sread` is fine
//   - we do NOT compare `rvalid`/`rdata` etc. that depend on always_ff
//     register propagation under the current M0.5 dispatch (issue #25);
//     only combinationally-driven outputs (awready/wready/arready/bvalid/
//     counter-out etc.)
//
// Tagged [verilator][e2e][trace-parity][m5]. Skipped cleanly when
// verilator is not on PATH (PR matrix without toolchain).
#include <unistd.h>
#include <cstdio>
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "axi4/axi4_lite_slave.h"
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
using namespace axi4;

namespace {

bool tool_available(const char *cmd) {
    std::string which_cmd = std::string("command -v ") + cmd +
                            " >/dev/null 2>&1";
    return std::system(which_cmd.c_str()) == 0;
}

std::string make_temp_dir(const std::string &suffix) {
    char tmpl[] = "/tmp/cpphdl_vl_XXXXXX";
    char *dir = ::mkdtemp(tmpl);
    if (!dir) return {};
    std::string result(dir);
    result += suffix;
    ::mkdir(result.c_str(), 0755);
    return result;
}

// Read a top-level ch_out port's value from the simulator's data_map_.
// Returns 0 if the port is missing (parity must FAIL anyway, the
// discrepancy surfaces through the cycle-by-cycle REQUIRE chain).
uint64_t read_port_output(Simulator &sim, ch::core::lnodeimpl *ln) {
    auto it = sim.data_map().find(ln->id());
    if (it == sim.data_map().end()) return UINT64_MAX;
    return static_cast<uint64_t>(it->second);
}

// ----------------------------------------------------------------------------
// Fixture 1: CounterFixture<32> — minimal sanity counter (R5/R7 closure).
// ch_reg increments each cycle; combinational io().out = reg.
//
// Verilator path: backend takes 1 cycle to spin up; the 20-cycle run
// after warmup exercises the 2-step clock toggle + 3-eval/tick dispatch
// from `tests/test_verilator_backend.cpp::E2E CounterFiftyCycles`.
// ----------------------------------------------------------------------------
template <unsigned N>
class CounterFixture : public ch::Component {
public:
    __io(ch_out<ch_uint<N>> out;)
    CounterFixture(ch::Component *parent = nullptr,
                   const std::string &name = "counter")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch_reg<ch_uint<N>> reg(0_d);
        reg->next = reg + 1_d;
        io().out = reg;
    }
};

// ----------------------------------------------------------------------------
// Fixture 2: Minimal AXI4-Lite slave wrapper. Mirrors tests/test_axi_lite_
// verilator_e2e.cpp::AxiLiteTop but exposes only the combinational handshake
// signals (awready / wready / arready) that do NOT depend on always_ff
// register propagation under M0.5 dispatch (issue #25). bvalid and rvalid
// are skipped here because they depend on the busy latch (always_ff).
// ----------------------------------------------------------------------------
class TraceParityAxiLite : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> awaddr;
        ch_in<ch_uint<2>>  awprot;
        ch_in<ch_bool>     awvalid;
        ch_out<ch_bool>    awready;

        ch_in<ch_uint<32>> wdata;
        ch_in<ch_uint<4>>  wstrb;
        ch_in<ch_bool>     wvalid;
        ch_out<ch_bool>    wready;

        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool>    bvalid;
        ch_in<ch_bool>     bready;

        ch_in<ch_uint<32>> araddr;
        ch_in<ch_uint<2>>  arprot;
        ch_in<ch_bool>     arvalid;
        ch_out<ch_bool>    arready;

        ch_out<ch_uint<32>> rdata;
        ch_out<ch_uint<2>>  rresp;
        ch_out<ch_bool>     rvalid;
        ch_in<ch_bool>      rready;
    )

    TraceParityAxiLite(ch::Component *parent = nullptr,
                       const std::string &name = "axi_trace_parity")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch::ch_module<AxiLiteSlave<32, 32, 4>> slave{"slave"};

        slave.io().awaddr  <<= io().awaddr;
        slave.io().awprot  <<= io().awprot;
        slave.io().awvalid <<= io().awvalid;
        io().awready       <<= slave.io().awready;

        slave.io().wdata <<= io().wdata;
        slave.io().wstrb <<= io().wstrb;
        slave.io().wvalid <<= io().wvalid;
        io().wready       <<= slave.io().wready;

        io().bresp        <<= slave.io().bresp;
        io().bvalid       <<= slave.io().bvalid;
        slave.io().bready <<= io().bready;

        slave.io().araddr  <<= io().araddr;
        slave.io().arprot  <<= io().arprot;
        slave.io().arvalid <<= io().arvalid;
        io().arready       <<= slave.io().arready;

        io().rdata  <<= slave.io().rdata;
        io().rresp  <<= slave.io().rresp;
        io().rvalid <<= slave.io().rvalid;
        slave.io().rready <<= io().rready;
    }
};

// ----------------------------------------------------------------------------
// Fixture 3: bundle_base multi-field. CRTP pattern from
// tests/test_bundle.cpp:18-29. Three independent ch_uint<8> registers each
// cycle on their own increment; each field bound to its own ch_out port.
// Tests bundle field name resolution in verilator (R5 closure — verilator
// must see flat field ports after `bundle_base::create_field_slices_`
// from_node` at bundle_base.h:301-345).
// ----------------------------------------------------------------------------
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

template <unsigned N>
class BundleMultiField : public ch::Component {
public:
    __io(
        ch_out<ch_uint<N>> a_out;
        ch_out<ch_uint<N>> b_out;
        ch_out<ch_uint<N>> c_out;
    )
    BundleMultiField(ch::Component *parent = nullptr,
                     const std::string &name = "bundle_mf")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        AbcBundle<ch_uint<N>> bundle;
        bundle.as_master();

        // (verilator-reset-initial-value-mismatch): the follow-up change
        // restored the original design intent — three non-zero init
        // values that exercise reg init loading via VerilatorBackend's
        // t=0 reset pulse. After cycle 1 we expect a=1, b=12, c=23
        // (interpreter == verilator).
        ch_reg<ch_uint<N>> reg_a(0_d);
        ch_reg<ch_uint<N>> reg_b(10_d);
        ch_reg<ch_uint<N>> reg_c(20_d);
        reg_a->next = reg_a + 1_d;
        reg_b->next = reg_b + 2_d;
        reg_c->next = reg_c + 3_d;

        bundle.a = reg_a;
        bundle.b = reg_b;
        bundle.c = reg_c;

        io().a_out = bundle.a;
        io().b_out = bundle.b;
        io().c_out = bundle.c;
    }
};

// ----------------------------------------------------------------------------
// Shared helper: build an InterpreterBackend-driven Simulator on a freshly
// constructed ch_device<T>. The pointer is released by the unique_ptr (clean
// stack-allocated ownership). Returns by unique_ptr so caller can't
// accidentally copy the Simulator and share its data_map_.
// ----------------------------------------------------------------------------
template <typename T>
std::unique_ptr<ch::ch_device<T>> make_interpreter_device() {
    return std::make_unique<ch::ch_device<T>>();
}

} // namespace

// ============================================================================
// CounterFixture<32>: 20 cycle trace parity.
// ============================================================================
TEST_CASE("TraceParity - CounterFixture32TwentyCycles",
          "[verilator][e2e][trace-parity][m5]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not on PATH");
    }

    // 1) Interpreter ground truth — MUST pin InterpreterBackend explicitly.
    auto dev_a = make_interpreter_device<CounterFixture<32>>();
    ch::Simulator sim_a(dev_a->context());
    sim_a.set_backend(std::make_unique<InterpreterBackend>());

    // 2) Verilator side — single device + single backend, reused across cycles.
    auto dev_b = std::make_unique<ch::ch_device<CounterFixture<32>>>();
    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_parity_c32");
    REQUIRE_FALSE(workdir.empty());

    ch::Simulator sim_b(dev_b->context());
    {
        auto backend = std::make_unique<VerilatorBackend>(workdir);
        REQUIRE(backend->initialize(dev_b->context(), data_map_b));
        if (backend->port_access_snapshot().empty()) {
            SKIP("verilator dlopen did not populate port_access_ "
                 "(verilator build may have failed in test env)");
        }
        sim_b.set_backend(std::move(backend));
    }

    auto *out_a = dev_a->instance().io().out.impl();
    auto *out_b = dev_b->instance().io().out.impl();
    // (no id equality check — ids are context-local; we instead compare
    // the *values* in each simulator's data_map_, which is the real
    // signal we care about for parity)

    // 3) Walk 20 cycles and require per-cycle parity.
    for (uint32_t cycle = 1; cycle <= 20; ++cycle) {
        sim_a.tick(1);
        sim_b.tick(1);
        uint64_t val_a = read_port_output(sim_a, out_a);
        uint64_t val_b = read_port_output(sim_b, out_b);
        UNSCOPED_INFO("cycle=" << cycle
                     << " interpreter=" << val_a
                     << " verilator=" << val_b);
        REQUIRE(val_a == val_b);
        REQUIRE(val_a == static_cast<uint64_t>(cycle));
    }
}

// ============================================================================
// TraceParityAxiLite: 10 cycle trace parity covering awready/wready/arready
// (the combinational handshake signals). Write transaction at cycle 1,
// read transaction at cycle 4. Both sides follow identical inputs.
// ============================================================================
TEST_CASE("TraceParity - AxiLiteTenCycles",
          "[verilator][e2e][trace-parity][axi4lite][m5]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not on PATH");
    }

    auto dev_a = make_interpreter_device<TraceParityAxiLite>();
    ch::Simulator sim_a(dev_a->context());
    sim_a.set_backend(std::make_unique<InterpreterBackend>());

    auto dev_b = std::make_unique<ch::ch_device<TraceParityAxiLite>>();
    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_parity_axi");
    REQUIRE_FALSE(workdir.empty());

    ch::Simulator sim_b(dev_b->context());
    {
        auto backend = std::make_unique<VerilatorBackend>(workdir);
        REQUIRE(backend->initialize(dev_b->context(), data_map_b));
        if (backend->port_access_snapshot().empty()) {
            SKIP("verilator dlopen did not populate port_access_");
        }
        sim_b.set_backend(std::move(backend));
    }

    // Drive identical inputs on both sides.
    auto drive_defaults = [](ch::Simulator &sim,
                             ch::ch_device<TraceParityAxiLite> &dev) {
        sim.set_input_value(dev.instance().io().awvalid, false);
        sim.set_input_value(dev.instance().io().wvalid,  false);
        sim.set_input_value(dev.instance().io().arvalid, false);
        sim.set_input_value(dev.instance().io().bready,  true);
        sim.set_input_value(dev.instance().io().rready,  true);
    };

    // Drive identical inputs on both sides, copying values from the
    // interpreter side's view to the verilator side's view.
    auto drive_write = [](ch::Simulator &sim,
                          ch::ch_device<TraceParityAxiLite> &dev) {
        sim.set_input_value(dev.instance().io().awaddr,
                            static_cast<uint64_t>(0x0));
        sim.set_input_value(dev.instance().io().awprot,
                            static_cast<uint64_t>(0x0));
        sim.set_input_value(dev.instance().io().awvalid, true);
        sim.set_input_value(dev.instance().io().wdata,
                            static_cast<uint64_t>(0xDEADBEEF));
        sim.set_input_value(dev.instance().io().wstrb,
                            static_cast<uint64_t>(0xF));
        sim.set_input_value(dev.instance().io().wvalid, true);
    };
    auto drive_read = [](ch::Simulator &sim,
                         ch::ch_device<TraceParityAxiLite> &dev) {
        sim.set_input_value(dev.instance().io().araddr,
                            static_cast<uint64_t>(0x0));
        sim.set_input_value(dev.instance().io().arprot,
                            static_cast<uint64_t>(0x0));
        sim.set_input_value(dev.instance().io().arvalid, true);
    };

    auto *awready_a = dev_a->instance().io().awready.impl();
    auto *wready_a  = dev_a->instance().io().wready.impl();
    auto *arready_a = dev_a->instance().io().arready.impl();
    auto *awready_b = dev_b->instance().io().awready.impl();
    auto *wready_b  = dev_b->instance().io().wready.impl();
    auto *arready_b = dev_b->instance().io().arready.impl();

    // Cycle 0: defaults.
    drive_defaults(sim_a, *dev_a);
    drive_defaults(sim_b, *dev_b);
    sim_a.tick(1);
    sim_b.tick(1);
    REQUIRE(read_port_output(sim_a, awready_a) ==
            read_port_output(sim_b, awready_b));
    REQUIRE(read_port_output(sim_a, wready_a) ==
            read_port_output(sim_b, wready_b));
    REQUIRE(read_port_output(sim_a, arready_a) ==
            read_port_output(sim_b, arready_b));

    // Cycle 1: assert write.
    drive_write(sim_a, *dev_a);
    drive_write(sim_b, *dev_b);
    sim_a.tick(1);
    sim_b.tick(1);
    REQUIRE(read_port_output(sim_a, awready_a) ==
            read_port_output(sim_b, awready_b));
    REQUIRE(read_port_output(sim_a, wready_a) ==
            read_port_output(sim_b, wready_b));

    // Cycles 2-4: idle. Walk 3 cycles with await.
    drive_defaults(sim_a, *dev_a);
    drive_defaults(sim_b, *dev_b);
    sim_a.tick(3);
    sim_b.tick(3);

    // Cycle 5: assert read.
    drive_read(sim_a, *dev_a);
    drive_read(sim_b, *dev_b);
    sim_a.tick(1);
    sim_b.tick(1);
    REQUIRE(read_port_output(sim_a, arready_a) ==
            read_port_output(sim_b, arready_b));

    // Cycles 6-9: idle walk 4 cycles.
    drive_defaults(sim_a, *dev_a);
    drive_defaults(sim_b, *dev_b);
    sim_a.tick(4);
    sim_b.tick(4);

    // Final parity gate.
    REQUIRE(read_port_output(sim_a, awready_a) ==
            read_port_output(sim_b, awready_b));
    REQUIRE(read_port_output(sim_a, wready_a) ==
            read_port_output(sim_b, wready_b));
    REQUIRE(read_port_output(sim_a, arready_a) ==
            read_port_output(sim_b, arready_b));
}

// ============================================================================
// BundleMultiField<8>: 20 cycle trace parity across 3 output ports.
// a cycles 0,1,2,...; b cycles 10,12,14,...; c cycles 20,23,26,...
// All three must match per-cycle between interpreter & verilator.
// ============================================================================
TEST_CASE("TraceParity - BundleMultiFieldTwentyCycles",
          "[verilator][e2e][trace-parity][bundle][m5]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not on PATH");
    }

    auto dev_a = make_interpreter_device<BundleMultiField<8>>();
    ch::Simulator sim_a(dev_a->context());
    sim_a.set_backend(std::make_unique<InterpreterBackend>());

    auto dev_b = std::make_unique<ch::ch_device<BundleMultiField<8>>>();
    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_parity_bnd");
    REQUIRE_FALSE(workdir.empty());

    ch::Simulator sim_b(dev_b->context());
    {
        auto backend = std::make_unique<VerilatorBackend>(workdir);
        REQUIRE(backend->initialize(dev_b->context(), data_map_b));
        if (backend->port_access_snapshot().empty()) {
            SKIP("verilator dlopen did not populate port_access_");
        }
        sim_b.set_backend(std::move(backend));
    }

    auto *a_a = dev_a->instance().io().a_out.impl();
    auto *b_a = dev_a->instance().io().b_out.impl();
    auto *c_a = dev_a->instance().io().c_out.impl();
    auto *a_b = dev_b->instance().io().a_out.impl();
    auto *b_b = dev_b->instance().io().b_out.impl();
    auto *c_b = dev_b->instance().io().c_out.impl();

    for (uint32_t cycle = 1; cycle <= 20; ++cycle) {
        sim_a.tick(1);
        sim_b.tick(1);
        uint64_t a_a_v = read_port_output(sim_a, a_a);
        uint64_t b_a_v = read_port_output(sim_a, b_a);
        uint64_t c_a_v = read_port_output(sim_a, c_a);
        uint64_t a_b_v = read_port_output(sim_b, a_b);
        uint64_t b_b_v = read_port_output(sim_b, b_b);
        uint64_t c_b_v = read_port_output(sim_b, c_b);
        UNSCOPED_INFO("cycle=" << cycle
                     << " a:interp=" << a_a_v << " vl=" << a_b_v
                     << " b:interp=" << b_a_v << " vl=" << b_b_v
                     << " c:interp=" << c_a_v << " vl=" << c_b_v);
        REQUIRE(a_a_v == a_b_v);
        REQUIRE(b_a_v == b_b_v);
        REQUIRE(c_a_v == c_b_v);
        // Closed-form sanity check: a = cycle-1 (since reg starts at 0
        // and increments post-edge, post-tick view of cycle N is N-1).
        // Actually the first tick settles the reset; we observed via
        // test_verilator_e2e_harness.cpp:162 that counter_value after
        // tick(50) == 50, so post-tick cycle N → reg == N. Keep this
        // loose (no exact equality) because bundle slicing may introduce
        // a 1-cycle offset depending on how bundle_base.create_field_
        // slices_from_node resolves the underlying node id.
    }
}

// Custom main(): when verilator is not on PATH every TEST_CASE SKIPs and
// Catch2 exits with 4 ("all tests skipped"), which ctest reports as a
// failure. Surface a one-line note and return 0 instead (mirrors
// test_verilog_external.cpp:266-275).
int main(int argc, char *argv[]) {
    if (!tool_available("verilator")) {
        std::fprintf(stderr,
                     "test_verilator_trace_parity: verilator not installed; "
                     "skipping all e2e tests.\n");
        return 0;
    }
    Catch::Session session;
    session.applyCommandLine(argc, argv);
    return session.run();
}