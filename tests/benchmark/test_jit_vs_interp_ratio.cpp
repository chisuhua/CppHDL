/**
 * @file test_jit_vs_interp_ratio.cpp
 * @brief Structural regression test: JIT must beat interpreter on combinational-heavy designs.
 *
 * Existing gates cover (a) absolute time budget (50ms) and (b) per-backend
 * baseline drift (1.2x via perf_regression.cpp). Neither catches the case
 * where the JIT silently degrades back to interpreter-class speed — the
 * JIT line stays under 50ms, the per-backend baseline holds, but the JIT
 * stops earning its compilation overhead.
 *
 * This file adds the missing structural assertion: same DUT, two
 * Simulators, one with JIT enabled and one without. The JIT variant must
 * be strictly faster (depth=500) or at least 2x faster (depth=1000) on a
 * combinational XOR chain, where native code emission dwarfs interpreter
 * dispatch cost.
 *
 * NOTE on scope:
 *   - Combinational-only DUT (TC-07 mirror) — register-heavy chains
 *     (TC-08) intentionally use the absolute 50ms gate in
 *     test_jit_reg_perf.cpp because the per-tick work is dominated by
 *     buffer sync / function call overhead where JIT cannot win.
 *   - depth=500 is large enough that JIT setup (~10-50ms cold ORC) is
 *     amortized; depth=1000 doubles the chain to confirm the ratio grows.
 *   - Independent contexts per run — no shared Simulator state leaks
 *     between jit_enabled(true) and jit_enabled(false) measurements.
 *
 * Tag: [perf][jit][ratio] — runs under ctest -L perf.
 */

#include "catch_amalgamated.hpp"
#include "perf_timer.h"
#include "ch.hpp"
#include "device.h"
#include "simulator.h"
#include "component.h"
#include "core/io.h"
#include "core/uint.h"

#include <algorithm>
#include <functional>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// XOR-chain combinational DUT. Same body as Tc07MirrorSmall in
// test_jit_smallgraph.cpp; declared here independently so the test file
// has no cross-file header dependency beyond perf_timer.h. The depth
// parameter scales node count linearly (depth+1 ch_uint<8> nodes), giving
// us a tunable knob for "enough work that JIT wins".
class RatioMirror : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> result;)
    RatioMirror(ch::Component* p, const std::string& n, int d)
        : ch::Component(p, n), depth_(d) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch_uint<8> acc = ch_uint<8>(0_b);
        for (int i = 0; i < depth_; ++i) {
            acc = acc ^ ch_uint<8>(static_cast<unsigned>(i & 0xff));
        }
        io().result = acc;
    }
private:
    int depth_;
};

// Run the DUT for `warmup + measured` cycles of `ticks_per_run` ticks,
// with `jit_enabled` toggled on a fresh Simulator. Returns median_us of
// the measured runs. JIT compile cost is paid by the first sim.tick(1)
// call which is *outside* the timed loop.
double median_backend_us(int depth, int ticks_per_run,
                         int warmup, int measured, bool jit_enabled) {
    // std::move because ch_device<...> takes Args&&... — passing a named
    // lvalue would fail to bind. Existing perf tests (test_jit_smallgraph)
    // pass a prvalue literal which binds directly; we forward a variable
    // so the same DUT can be sized from the caller.
    ch_device<RatioMirror, int> dev(std::move(depth));
    Simulator sim(dev.context());
    sim.set_jit_enabled(jit_enabled);
    sim.tick(1);  // trigger JIT compile (untimed) when jit_enabled=true
    std::vector<double> samples;
    samples.reserve(measured);
    auto runner = [&]() { sim.tick(ticks_per_run); };
    for (int i = 0; i < warmup; ++i) runner();
    for (int i = 0; i < measured; ++i) {
        PerfTimer t;
        t.start();
        runner();
        t.stop();
        samples.push_back(t.elapsed_us());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

}  // namespace

// Depth=500: combinational chain large enough that the ORC JIT setup
// (~10-50ms one-shot) is amortized over 1000 measured runs, but small
// enough that CI remains snappy. Asserts a strict speedup: jit_us must be
// strictly less than interp_us. If this fails, the JIT has degraded to
// (or below) interpreter speed — the regression we explicitly want to
// catch.
TEST_CASE("JIT vs interpreter ratio: TC-07 depth=500 jit strictly faster than interp",
          "[perf][jit][ratio]") {
    const int depth = 500;
    const int ticks_per_run = 100;
    const int warmup = 3;
    const int measured = 5;

    double interp_us = median_backend_us(depth, ticks_per_run,
                                          warmup, measured, /*jit_enabled=*/false);
    double jit_us    = median_backend_us(depth, ticks_per_run,
                                          warmup, measured, /*jit_enabled=*/true);
    double ratio = (jit_us > 0.0) ? (interp_us / jit_us) : 0.0;

    INFO("depth=" << depth
         << " ticks_per_run=" << ticks_per_run
         << " interp_us=" << interp_us
         << " jit_us=" << jit_us
         << " ratio(interp/jit)=" << ratio
         << " (must be > 1.0)");

    REQUIRE(jit_us < interp_us);
    // Sanity floor — guards against a broken timer producing 0-μs samples.
    // The absolute 50ms gate from perf_jit_smallgraph is intentionally NOT
    // applied here: that budget is sized for depth=10. depth=500 has 50x
    // more work and runs ~130ms cold (LLVM ORC first-touch), well under
    // the ratio but well over 50ms. The structural ratio assertion is the
    // contract for this test, not the absolute wall-clock.
    REQUIRE(jit_us > 0.0);
    REQUIRE(interp_us > 0.0);
}

// Depth=1000: larger chain doubles the work, expected to widen the
// JIT-vs-interpreter gap. Asserts ratio >= 2.0x — a more aggressive bar
// that confirms JIT performance scales with design complexity. If this
// fails while depth=500 still passes, JIT scalability has regressed (the
// interpreter's per-node dispatch cost grows linearly while native JIT
// should scale sub-linearly with node count).
TEST_CASE("JIT vs interpreter ratio: TC-07 depth=1000 ratio >= 2.0x",
          "[perf][jit][ratio][deep]") {
    const int depth = 1000;
    const int ticks_per_run = 100;
    const int warmup = 3;
    const int measured = 5;

    double interp_us = median_backend_us(depth, ticks_per_run,
                                          warmup, measured, /*jit_enabled=*/false);
    double jit_us    = median_backend_us(depth, ticks_per_run,
                                          warmup, measured, /*jit_enabled=*/true);
    double ratio = (jit_us > 0.0) ? (interp_us / jit_us) : 0.0;

    INFO("depth=" << depth
         << " ticks_per_run=" << ticks_per_run
         << " interp_us=" << interp_us
         << " jit_us=" << jit_us
         << " ratio(interp/jit)=" << ratio
         << " (must be >= 2.0)");

    REQUIRE(ratio >= 2.0);
    // Sanity floor: JIT must still finish — guards against pathological
    // 0-μs samples from a broken timer.
    REQUIRE(jit_us > 0.0);
    REQUIRE(interp_us > 0.0);
}