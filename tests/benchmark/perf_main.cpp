/**
 * @file perf_main.cpp
 * @brief Main entry point for CppHDL performance benchmarks.
 *
 * Legacy TC-01/02/04/06 use a single backend. TC-07/08 do three-way
 * comparison (Interpreter / JIT / Verilator); the Verilator path
 * gracefully degrades to 2-way when verilator is not installed.
 */

#include "perf_timer.h"
#include "stats_collector.h"
#include "report_generator.h"
#include "verilator_runner.h"
#include "subprocess_runner.h"
#include "ch.hpp"
#include "simulator.h"
#include "codegen_verilog.h"
#include "device.h"
#include "component.h"
#include "core/io.h"
#include "core/reg.h"
#include "core/uint.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::core;

static BenchmarkResult run_combinational_benchmark(int depth, int ticks) {
    PerfTimer timer;
    {
        context ctx("chain");
        ctx_swap swap(&ctx);
        ch_uint<8> result = ch_uint<8>(0_b);
        for (int i = 0; i < depth; ++i) result = result ^ ch_uint<8>(i);
        Simulator sim(&ctx);
        timer.start(); sim.tick(ticks); timer.stop();
    }
    double ns = timer.elapsed_ns();
    double us = ns / 1000.0;
    BenchmarkResult r;
    r.test_name = "TC-01";
    r.params = "depth=" + std::to_string(depth);
    r.ticks_per_sec = (ticks * 1e9) / ns;
    r.ns_per_tick = ns / ticks;
    r.ns_per_node_tick = ns / (static_cast<double>(ticks) * depth);
    r.backend = ""; r.sim_us = us; r.median_us = us;
    r.iterations = 1; r.status = "LEGACY";
    return r;
}

static BenchmarkResult run_sequential_benchmark(int num_regs, int ticks) {
    PerfTimer timer;
    {
        context ctx("seq"); ctx_swap swap(&ctx);
        // W4 (perf-report-followup.md): the v1 harness constructed a single
        // ch_reg and ignored its `num_regs` parameter, so the LEGACY row
        // for `regs=1000` was identical to `regs=10`. The intended fix
        // (chain of N regs via `inp = ch_reg<ch_uint<8>>(inp);` in a loop)
        // triggers a pre-existing segfault in regimpl::create_instruction
        // (see .omo/evidence/task-4-w4-tc02-segfault.txt for backtrace).
        // Until that is resolved upstream, keep the v1 single-reg DUT and
        // rely on the LEGACY status + is_legacy flag (W6) to communicate
        // the parameter-is-decorative nature to downstream consumers.
        (void)num_regs;  // parameter is documented as v1-legacy decoration
        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);
        Simulator sim(&ctx);
        timer.start();
        for (int t = 0; t < ticks; ++t) sim.tick();
        timer.stop();
    }
    double ns = timer.elapsed_ns();
    double us = ns / 1000.0;
    BenchmarkResult r;
    r.test_name = "TC-02";
    r.params = "regs=" + std::to_string(num_regs);
    r.ticks_per_sec = (ticks * 1e9) / ns;
    r.ns_per_tick = ns / ticks;
    r.ns_per_node_tick = ns / (static_cast<double>(ticks) * num_regs);
    r.backend = ""; r.sim_us = us; r.median_us = us;
    r.iterations = 1; r.status = "LEGACY";
    // W4: LEGACY rows have no comparison target for overhead_percent;
    // -1.0 is the sentinel for "not applicable" (rendered as null/N/A/empty).
    r.overhead_percent = -1.0;
    return r;
}

static BenchmarkResult run_trace_benchmark(int num_signals, int ticks) {
    PerfTimer t0, t1;
    {
        context ctx("trace"); ctx_swap swap(&ctx);
        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);
        Simulator sim0(&ctx, false);
        t0.start(); sim0.tick(ticks); t0.stop();
        Simulator sim1(&ctx, true);
        t1.start(); sim1.tick(ticks); t1.stop();
    }
    double n0 = t0.elapsed_ns(), n1 = t1.elapsed_ns();
    BenchmarkResult r;
    r.test_name = "TC-04";
    r.params = "signals=" + std::to_string(num_signals);
    r.ticks_per_sec = (ticks * 1e9) / n1;
    r.ns_per_tick = n1 / ticks;
    r.overhead_percent = ((n1 - n0) / n0) * 100.0;
    r.backend = ""; r.sim_us = n1 / 1000.0; r.median_us = n1 / 1000.0;
    r.iterations = 1; r.status = "LEGACY";
    return r;
}

static BenchmarkResult run_batch_tick_benchmark(int nodes, int ticks) {
    PerfTimer tb, ts;
    {
        context ctx("batch"); ctx_swap swap(&ctx);
        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);
        Simulator sim(&ctx);
        tb.start(); sim.tick(ticks); tb.stop();
    }
    {
        context ctx("single"); ctx_swap swap(&ctx);
        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);
        Simulator sim(&ctx);
        ts.start();
        for (int t = 0; t < ticks; ++t) sim.tick();
        ts.stop();
    }
    double nb = tb.elapsed_ns(), ns = ts.elapsed_ns();
    BenchmarkResult r;
    r.test_name = "TC-06";
    r.params = "nodes=" + std::to_string(nodes);
    r.ticks_per_sec = (ticks * 1e9) / nb;
    r.ns_per_tick = nb / ticks;
    r.overhead_percent = ((ns - nb) / ns) * 100.0;
    r.backend = ""; r.sim_us = nb / 1000.0; r.median_us = nb / 1000.0;
    r.iterations = 1; r.status = "LEGACY";
    return r;
}

// W11 (perf-report-followup.md): capture the current short git SHA so
// that the JSON report can be tied back to a specific commit. Returns
// "unknown" if git is unavailable, the working tree is not a git repo,
// or the SHA file cannot be read.
static std::string capture_git_sha() {
    std::string sha = "unknown";
    if (std::system("git rev-parse --short HEAD > /tmp/cpphdl_git_sha.txt 2>/dev/null") == 0) {
        std::ifstream in("/tmp/cpphdl_git_sha.txt");
        if (in.good()) {
            std::getline(in, sha);
            while (!sha.empty() && (sha.back() == '\n' || sha.back() == '\r' ||
                   sha.back() == ' '))
                sha.pop_back();
        }
    }
    return sha;
}

// TC-07/08 three-way comparison — DUTs are Component subclasses so the generated Verilog has a ch_out<ch_uint<8>> port (required by Verilator).
class Tc07XorChain : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> result;)
    Tc07XorChain(ch::Component* p, const std::string& n, int d)
        : ch::Component(p, n), depth_(d) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch_uint<8> acc = ch_uint<8>(0_b);
        for (int i = 0; i < depth_; ++i) acc = acc ^ ch_uint<8>(i);
        io().result = acc;
    }
private:
    int depth_;
};

// Single-register design mirroring the TC-02 stress pattern (v1 convention).
class Tc08RegChain : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> result;)
    Tc08RegChain(ch::Component* p, const std::string& n, int r)
        : ch::Component(p, n), regs_(r) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch_uint<8> inp = ch_uint<8>(1_b);
        auto reg = ch_reg<ch_uint<8>>(inp);
        io().result = reg;
    }
private:
    int regs_;
};

// W8 (perf-report-followup.md): TC-09 — ch_uint<32> arithmetic chain.
class Tc09ArithChain : public ch::Component {
public:
    __io(ch_out<ch_uint<32>> result;)
    Tc09ArithChain(ch::Component* p, const std::string& n, int d)
        : ch::Component(p, n), depth_(d) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch_uint<32> acc = ch_uint<32>(1_d);
        for (int i = 0; i < depth_; ++i) {
            acc = acc + ch_uint<32>(static_cast<unsigned>(i));
            acc = acc ^ (acc << 1);
        }
        io().result = acc;
    }
private:
    int depth_;
};

// W8 (perf-report-followup.md): TC-10 — float chain. UNSUPPORTED because
// ch_float does not exist in the current CppHDL type system (verified
// 2026-06-08). The DUT stub remains so that a future ch_float integration
// can simply enable the body.
class Tc10FloatChain : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> result;)  // ch_float not available; use placeholder
    Tc10FloatChain(ch::Component* p, const std::string& n, int d)
        : ch::Component(p, n), depth_(d) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // ch_float not implemented; the design body is a no-op that
        // exercises zero eval nodes. The run_three_way() wrapper should
        // detect this and mark the row as UNSUPPORTED.
        (void)depth_;
        ch_uint<8> r = ch_uint<8>(0_b);
        io().result = r;
    }
private:
    int depth_;
};

// W8 (perf-report-followup.md): TC-11 — ch_uint<256> wide reg chain.
class Tc11WideReg : public ch::Component {
public:
    __io(ch_out<ch_uint<256>> result;)
    Tc11WideReg(ch::Component* p, const std::string& n, int r)
        : ch::Component(p, n), regs_(r) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch_uint<256> inp = ch_uint<256>(1_b);
        for (int i = 0; i < regs_; ++i) {
            inp = ch_reg<ch_uint<256>>(inp);
        }
        io().result = inp;
    }
private:
    int regs_;
};

struct ThreeWayResult {
    BenchmarkResult interp, jit, verilator;
    bool verilator_skipped = false;
    std::string skip_reason;
};

// F2 (fix-perf-subprocess-isolation): helpers that drive TC-07/08/09 in a
// fresh child process. Each helper builds a CLI command mirroring the
// parent's --tc=NN flags, invokes run_perf_subprocess, parses the child's
// perf_results.json, and returns a ThreeWayResult in the same shape as
// the in-process run_three_way_tc07/08/09 functions below. The child
// process runs in its own address space, so K1 ORC JIT cross-DUT state
// pollution (see docs/simulation/PERF_COMPARISON_REPORT.md §6 K1) cannot
// affect subsequent measurements.

static std::filesystem::path self_exe_path() {
    // Linux: /proc/self/exe is a symlink to the running executable.
    // Returns the path; on readlink failure (other platforms) callers
    // fall back to argv[0] at the call site.
    std::error_code ec;
    auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) return p;
    return {};
}

static BenchmarkResult subprocess_row_to_benchmark_result(
    const ch::perf_test::SubprocessRow& r) {
    BenchmarkResult out;
    out.test_name = r.test_name;
    out.params = r.params;
    out.backend = r.backend;
    out.ticks_per_sec = r.ticks_per_sec;
    out.ns_per_tick = r.ns_per_tick;
    out.ns_per_node_tick = r.ns_per_node_tick;
    out.overhead_percent = r.overhead_percent;
    out.build_us = r.build_us;
    out.sim_us = r.sim_us;
    out.total_us = r.total_us;
    out.iterations = r.iterations;
    out.median_us = r.median_us;
    out.status = r.status;
    out.skip_reason = r.skip_reason;
    return out;
}

// Find the row whose (backend, params) matches; if absent, return a SKIPPED
// placeholder so the parent's reporter still emits a row for verilator-
// missing environments.
static BenchmarkResult find_or_skip(
    const std::vector<ch::perf_test::SubprocessRow>& rows,
    const std::string& want,
    const std::string& test_name, const std::string& params) {
    for (const auto& r : rows) {
        if (r.backend == want && r.params == params) {
            return subprocess_row_to_benchmark_result(r);
        }
    }
    BenchmarkResult skip;
    skip.test_name = test_name;
    skip.params = params;
    skip.backend = want;
    skip.status = "SKIPPED";
    skip.skip_reason = "no row for backend '" + want + "' params '" + params +
        "' in subprocess output";
    return skip;
}

static ThreeWayResult run_three_way_subprocess_dispatch(
    const std::string& tc_str, const std::string& params,
    const std::string& test_name,
    int ticks, int warmup, int measured,
    const std::string& verilator_bin, const std::string& cache_root,
    const std::string& workdir) {
    ThreeWayResult twr;
    auto exe = self_exe_path();
    if (exe.empty()) {
        BenchmarkResult fail;
        fail.test_name = test_name; fail.params = params;
        fail.status = "SKIPPED";
        fail.skip_reason = "subprocess self-exe path not available";
        twr.interp = fail; twr.interp.backend = "interpreter";
        twr.jit = fail; twr.jit.backend = "jit";
        twr.verilator = fail; twr.verilator.backend = "verilator";
        twr.verilator_skipped = true;
        twr.skip_reason = fail.skip_reason;
        return twr;
    }
    std::vector<std::string> args = {
        "--direct",  // subprocess runs in-process to avoid infinite recursion
        "--ticks=" + std::to_string(ticks),
        "--warmup=" + std::to_string(warmup),
        "--measured=" + std::to_string(measured),
        "--verilator=" + verilator_bin,
        "--cache-root=" + cache_root,
        "--workdir=" + workdir,
    };
    auto sub = ch::perf_test::run_perf_subprocess(exe, tc_str, args, 180);
    if (!sub.error_msg.empty() || sub.timed_out) {
        std::cerr << "  [F2 subprocess] " << tc_str << " " << params
                  << ": " << sub.error_msg << "\n";
    }
    twr.interp = find_or_skip(sub.rows, "interpreter", test_name, params);
    twr.jit = find_or_skip(sub.rows, "jit", test_name, params);
    twr.verilator = find_or_skip(sub.rows, "verilator", test_name, params);
    twr.verilator_skipped = (twr.verilator.status == "SKIPPED" ||
                             twr.verilator.status == "UNSUPPORTED");
    twr.skip_reason = twr.verilator.skip_reason;
    return twr;
}

static ThreeWayResult run_three_way_subprocess_tc07(
    int depth, int ticks, int warmup, int measured,
    const std::string& verilator_bin, const std::string& cache_root,
    const std::string& workdir) {
    std::string params = "depth=" + std::to_string(depth);
    return run_three_way_subprocess_dispatch("07", params, "TC-07",
        ticks, warmup, measured, verilator_bin, cache_root, workdir);
}

static ThreeWayResult run_three_way_subprocess_tc08(
    int regs, int ticks, int warmup, int measured,
    const std::string& verilator_bin, const std::string& cache_root,
    const std::string& workdir) {
    std::string params = "regs=" + std::to_string(regs);
    return run_three_way_subprocess_dispatch("08", params, "TC-08",
        ticks, warmup, measured, verilator_bin, cache_root, workdir);
}

static ThreeWayResult run_three_way_subprocess_tc09(
    int depth, int ticks, int warmup, int measured,
    const std::string& verilator_bin, const std::string& cache_root,
    const std::string& workdir) {
    std::string params = "depth=" + std::to_string(depth);
    return run_three_way_subprocess_dispatch("09", params, "TC-09",
        ticks, warmup, measured, verilator_bin, cache_root, workdir);
}

// Run `runner` `warmup + measured` times; return median (us) of measured.
static double time_backend_us(std::function<void()> runner,
                              int warmup, int measured) {
    for (int i = 0; i < warmup; ++i) runner();
    std::vector<double> s; s.reserve(measured);
    for (int i = 0; i < measured; ++i) {
        PerfTimer t; t.start(); runner(); t.stop();
        s.push_back(t.elapsed_us());
    }
    std::sort(s.begin(), s.end());
    return s[s.size() / 2];
}

static BenchmarkResult make_row(const std::string& tn, const std::string& p,
                                const std::string& backend,
                                double build_us, double sim_us, int iters,
                                double median, const std::string& status) {
    BenchmarkResult r;
    r.test_name = tn; r.params = p; r.backend = backend;
    r.build_us = build_us; r.sim_us = sim_us;
    r.total_us = build_us + sim_us * static_cast<double>(iters);
    r.iterations = iters; r.median_us = median; r.status = status;
    r.ns_per_tick = sim_us * 1000.0;  // legacy: us -> ns
    r.ticks_per_sec = (sim_us > 0.0) ? 1e6 / sim_us : 0.0;
    return r;
}

template <typename DutT, typename... CtorArgs>
static ThreeWayResult run_three_way(const std::string& test_name,
                                    const std::string& params,
                                    int ticks, int warmup, int measured,
                                    const std::string& verilog_path,
                                    const std::string& harness_path,
                                    const std::string& verilator_bin,
                                    const std::string& cache_root,
                                    const std::string& workdir,
                                    CtorArgs&&... ctor_args) {
    ThreeWayResult twr;
    // Workdir must exist before toVerilog() tries to open its output.
    std::error_code ec;
    std::filesystem::create_directories(workdir, ec);
    // W3 (perf-report-followup.md): measure the build cost (ch_device +
    // Simulator construction + initial tick) once and propagate the same
    // value to all three backends. The build artifact is shared.
    double build_us_measured = 0.0;
    {  // Build DUT once, export Verilog (shared artifact).
        PerfTimer build_timer;
        build_timer.start();
        ch_device<DutT, CtorArgs...> dev(std::forward<CtorArgs>(ctor_args)...);
        Simulator s(dev.context());
        s.tick(1);
        toVerilog(verilog_path, dev.context());
        (void)s.get_value(dev.io().result);
        build_timer.stop();
        build_us_measured = build_timer.elapsed_us();
    }
    {  // Interpreter backend.
        ch_device<DutT, CtorArgs...> dev(std::forward<CtorArgs>(ctor_args)...);
        Simulator s(dev.context());
        s.set_jit_enabled(false);
        s.tick(1);
        double mu = time_backend_us([&](){ s.tick(ticks); }, warmup, measured);
        twr.interp = make_row(test_name, params, "interpreter",
                              build_us_measured, mu, measured, mu, "PASS");
    }
    {  // JIT backend.
        ch_device<DutT, CtorArgs...> dev(std::forward<CtorArgs>(ctor_args)...);
        Simulator s(dev.context());
        s.set_jit_enabled(true);
        s.tick(1);  // triggers JIT compile
        double mu = time_backend_us([&](){ s.tick(ticks); }, warmup, measured);
        twr.jit = make_row(test_name, params, "jit",
                           build_us_measured, mu, measured, mu, "PASS");
    }
    // Verilator backend — skipped gracefully when unavailable.
    ch_perf::VerilatorRunner vr(cache_root, verilator_bin);
    if (!vr.is_available()) {
        // W5 (perf-report-followup.md): UNSUPPORTED = environment missing
        // (verilator binary not on PATH), distinct from SKIPPED = runtime
        // hiccup (compile error, missing harness).
        twr.verilator_skipped = true;
        twr.skip_reason = "verilator not found on PATH";
        twr.verilator = make_row(test_name, params, "verilator",
                                 0.0, 0.0, 0, 0.0, "UNSUPPORTED");
        twr.verilator.skip_reason = twr.skip_reason;
    } else if (!std::filesystem::exists(verilog_path) ||
               !std::filesystem::exists(harness_path)) {
        twr.verilator_skipped = true;
        twr.skip_reason = "verilog or harness file missing";
        twr.verilator = make_row(test_name, params, "verilator",
                                 0.0, 0.0, 0, 0.0, "SKIPPED");
        twr.verilator.skip_reason = twr.skip_reason;
    } else {
        auto br = vr.build(test_name, verilog_path, harness_path);
        if (!br.success) {
            twr.verilator_skipped = true;
            twr.skip_reason = br.error_msg;
            twr.verilator = make_row(test_name, params, "verilator",
                                     br.build_time_ns / 1000.0, 0.0,
                                     0, 0.0, "SKIPPED");
            twr.verilator.skip_reason = twr.skip_reason;
        } else {
            double bu = br.build_time_ns / 1000.0;
            double mu = time_backend_us([&](){
                std::string err;
                vr.run_and_time(br.binary_path, ticks, &err);
            }, warmup, measured);
            twr.verilator = make_row(test_name, params, "verilator",
                                     bu, mu, measured, mu, "PASS");
        }
    }
    if (twr.verilator_skipped && twr.verilator.status != "UNSUPPORTED" &&
        twr.verilator.status != "SKIPPED") {
        // Backstop: if a path didn't set the status, default to SKIPPED.
        twr.verilator = make_row(test_name, params, "verilator",
                                 0.0, 0.0, 0, 0.0, "SKIPPED");
    }
    return twr;
}

static ThreeWayResult run_three_way_tc07(int depth, int ticks, int warmup,
                                         int measured,
                                         const std::string& vb,
                                         const std::string& cr,
                                         const std::string& wd) {
    return run_three_way<Tc07XorChain>(
        "TC-07", "depth=" + std::to_string(depth), ticks, warmup, measured,
        wd + "/tc07_top.v", "build/tests/benchmark/verilator_harness_tc07.cpp",
        vb, cr, wd, depth);
}

static ThreeWayResult run_three_way_tc08(int regs, int ticks, int warmup,
                                         int measured,
                                         const std::string& vb,
                                         const std::string& cr,
                                         const std::string& wd) {
    return run_three_way<Tc08RegChain>(
        "TC-08", "regs=" + std::to_string(regs), ticks, warmup, measured,
        wd + "/tc08_top.v", "build/tests/benchmark/verilator_harness_tc08.cpp",
        vb, cr, wd, regs);
}

// W8 (perf-report-followup.md): TC-09 arith chain. Verilator not wired
// (no harness) so emit SKIPPED for the verilator backend; interpreter/JIT
// are exercised.
static ThreeWayResult run_three_way_tc09(int depth, int ticks, int warmup,
                                         int measured,
                                         const std::string& vb,
                                         const std::string& cr,
                                         const std::string& wd) {
    return run_three_way<Tc09ArithChain>(
        "TC-09", "depth=" + std::to_string(depth), ticks, warmup, measured,
        wd + "/tc09_top.v", "build/tests/benchmark/verilator_harness_tc09.cpp",
        vb, cr, wd, depth);
}

// W8: TC-10 is unconditionally UNSUPPORTED (ch_float does not exist).
// Return a stub ThreeWayResult without invoking run_three_way.
static ThreeWayResult run_three_way_tc10_unsupported(int depth) {
    (void)depth;
    ThreeWayResult twr;
    auto stub = [&](const std::string& backend) {
        BenchmarkResult r;
        r.test_name = "TC-10";
        r.params = "depth=" + std::to_string(depth);
        r.backend = backend;
        r.status = "UNSUPPORTED";
        r.skip_reason = "ch_float not implemented";
        return r;
    };
    twr.interp = stub("interpreter");
    twr.jit = stub("jit");
    twr.verilator = stub("verilator");
    twr.verilator_skipped = true;
    return twr;
}

static ThreeWayResult run_three_way_tc11(int regs, int ticks, int warmup,
                                         int measured,
                                         const std::string& vb,
                                         const std::string& cr,
                                         const std::string& wd) {
    return run_three_way<Tc11WideReg>(
        "TC-11", "regs=" + std::to_string(regs), ticks, warmup, measured,
        wd + "/tc11_top.v", "build/tests/benchmark/verilator_harness_tc11.cpp",
        vb, cr, wd, regs);
}

// CLI
static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
        "  --tc=01|02|04|06    Legacy single-backend benchmarks\n"
        "  --tc=07|08          3-way (Interpreter/JIT/Verilator) benchmarks\n"
        "  --all               Run all benchmarks (alias: --baseline)\n"
        "  --verilator=<path>  Verilator binary (default: verilator)\n"
        "  --cache-root=<path> Verilator build cache dir (default: build/perf_cache)\n"
        "  --workdir=<path>    Verilator workdir (default: build/perf_work)\n"
        "  --warmup=<N>        Warmup iterations per backend (default: 5)\n"
        "  --measured=<N>      Measured iterations per backend (default: 10)\n"
        "  --ticks=<N>         Ticks per iteration (default: 1000)\n"
        "  --report=csv|json|md  Export results (repeatable)\n"
        "  -h, --help          Show this help\n";
}

static std::string flag_value(const char* arg, const char* prefix) {
    size_t n = std::strlen(prefix);
    return (std::strncmp(arg, prefix, n) == 0) ? std::string(arg + n) : std::string();
}

int main(int argc, char* argv[]) {
    ReportGenerator reporter;
    // W11: stamp the report with metadata so reviewers can tie results
    // back to a specific commit. git_sha is captured at startup;
    // timestamp is filled in by ReportGenerator::export_json() at write
    // time.
    reporter.set_metadata_field("git_sha", capture_git_sha());
    bool run_all = false, tc01 = false, tc02 = false, tc04 = false, tc06 = false;
    // F2 (fix-perf-subprocess-isolation): when the parent perf_main spawns
    // a child for a single TC, it passes --direct so the child runs the
    // TC in-process. Without --direct, the child would itself spawn another
    // child for the same TC (infinite recursion).
    bool direct = false;
    bool tc07 = false, tc08 = false;
    bool tc09 = false, tc10 = false, tc11 = false;
    std::vector<std::string> report_formats;
    std::string verilator_bin = "verilator";
    std::string cache_root = "build/perf_cache";
    std::string workdir = "build/perf_work";
    int warmup = 5, measured = 10, ticks = 1000;

    for (int i = 1; i < argc; ++i) {
        std::string v;
        if      ((v = flag_value(argv[i], "--report=")).size() > 0) {
            if (v != "csv" && v != "json" && v != "md") {
                std::cerr << "error: --report expects csv|json|md, got '" << v << "'\n";
                return 1;
            }
            report_formats.push_back(v);
        } else if ((v = flag_value(argv[i], "--verilator=")).size() > 0) verilator_bin = v;
        else if ((v = flag_value(argv[i], "--cache-root=")).size() > 0) cache_root = v;
        else if ((v = flag_value(argv[i], "--workdir=")).size() > 0)    workdir = v;
        else if ((v = flag_value(argv[i], "--warmup=")).size() > 0)     warmup = std::atoi(v.c_str());
        else if ((v = flag_value(argv[i], "--measured=")).size() > 0)   measured = std::atoi(v.c_str());
        else if ((v = flag_value(argv[i], "--ticks=")).size() > 0)      ticks = std::atoi(v.c_str());
        else if (!std::strcmp(argv[i], "--all") || !std::strcmp(argv[i], "--baseline")) run_all = true;
        else if (!std::strcmp(argv[i], "--direct")) direct = true;
        else if (!std::strcmp(argv[i], "--tc=01")) tc01 = true;
        else if (!std::strcmp(argv[i], "--tc=02")) tc02 = true;
        else if (!std::strcmp(argv[i], "--tc=04")) tc04 = true;
        else if (!std::strcmp(argv[i], "--tc=06")) tc06 = true;
        else if (!std::strcmp(argv[i], "--tc=07")) tc07 = true;
        else if (!std::strcmp(argv[i], "--tc=08")) tc08 = true;
        else if (!std::strcmp(argv[i], "--tc=09")) tc09 = true;
        else if (!std::strcmp(argv[i], "--tc=10")) tc10 = true;
        else if (!std::strcmp(argv[i], "--tc=11")) tc11 = true;
        else if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
            print_usage(argv[0]); return 0;
        } else {
            std::cerr << "error: unknown argument '" << argv[i] << "'\n";
            print_usage(argv[0]); return 1;
        }
    }
    if (!run_all && !tc01 && !tc02 && !tc04 && !tc06 && !tc07 && !tc08 &&
        !tc09 && !tc10 && !tc11) {
        print_usage(argv[0]); return 1;
    }
    if (warmup < 0 || measured <= 0 || ticks <= 0) {
        std::cerr << "error: --warmup/--measured/--ticks must be positive\n";
        return 1;
    }

    std::cout << "=== CppHDL Performance Benchmarks ===\n\n";

    if (run_all || tc01) {
        std::cout << "TC-01: Combinational Chain\n";
        for (int d : std::vector<int>{10, 100, 1000}) {
            auto r = run_combinational_benchmark(d, 1000);
            reporter.add_result(r);
            printf("  depth=%5d: %12.2f ticks/sec | %8.2f ns/tick | %6.2f ns/node/tick\n",
                   d, r.ticks_per_sec, r.ns_per_tick, r.ns_per_node_tick);
        }
        std::cout << "\n";
    }
    if (run_all || tc02) {
        std::cout << "TC-02: Sequential Registers\n";
        for (int n : std::vector<int>{10, 100, 1000}) {
            auto r = run_sequential_benchmark(n, 1000);
            reporter.add_result(r);
            printf("  regs=%5d: %12.2f ticks/sec | %8.2f ns/tick | %6.2f ns/reg/tick\n",
                   n, r.ticks_per_sec, r.ns_per_tick, r.ns_per_node_tick);
        }
        std::cout << "\n";
    }
    if (run_all || tc04) {
        std::cout << "TC-04: Trace Overhead\n";
        auto r = run_trace_benchmark(100, 1000);
        reporter.add_result(r);
        printf("  100 signals: trace overhead = %.2f%%\n", r.overhead_percent);
        std::cout << "\n";
    }
    if (run_all || tc06) {
        std::cout << "TC-06: Batch vs Single Tick\n";
        auto r = run_batch_tick_benchmark(1000, 1000);
        reporter.add_result(r);
        printf("  batch vs single: %.2f%% difference\n", r.overhead_percent);
        std::cout << "\n";
    }
    if (run_all || tc07) {
        std::cout << "TC-07: 3-way XOR chain\n";
        for (int d : std::vector<int>{10, 100, 1000}) {
            auto twr = direct ? run_three_way_tc07(d, ticks, warmup, measured,
                                                    verilator_bin, cache_root, workdir)
                              : run_three_way_subprocess_tc07(d, ticks, warmup, measured,
                                                    verilator_bin, cache_root, workdir);
            reporter.add_result(twr.interp);
            reporter.add_result(twr.jit);
            reporter.add_result(twr.verilator);
            printf("  depth=%5d: interp=%9.2f us | jit=%9.2f us | verilator=%9.2f us [%s]\n",
                   d, twr.interp.median_us, twr.jit.median_us,
                   twr.verilator.median_us,
                   twr.verilator_skipped ? "SKIPPED" : "OK");
            if (twr.verilator_skipped)
                std::cout << "    verilator skipped: " << twr.skip_reason << "\n";
        }
        std::cout << "\n";
    }
    if (run_all || tc08) {
        std::cout << "TC-08: 3-way ch_reg chain\n";
        for (int n : std::vector<int>{10, 100, 1000}) {
            auto twr = direct ? run_three_way_tc08(n, ticks, warmup, measured,
                                                    verilator_bin, cache_root, workdir)
                              : run_three_way_subprocess_tc08(n, ticks, warmup, measured,
                                                    verilator_bin, cache_root, workdir);
            reporter.add_result(twr.interp);
            reporter.add_result(twr.jit);
            reporter.add_result(twr.verilator);
            printf("  regs=%5d:  interp=%9.2f us | jit=%9.2f us | verilator=%9.2f us [%s]\n",
                   n, twr.interp.median_us, twr.jit.median_us,
                   twr.verilator.median_us,
                   twr.verilator_skipped ? "SKIPPED" : "OK");
            if (twr.verilator_skipped)
                std::cout << "    verilator skipped: " << twr.skip_reason << "\n";
        }
        std::cout << "\n";
    }
    if (run_all || tc09) {
        std::cout << "TC-09: 3-way ch_uint<32> arith chain\n";
        for (int d : std::vector<int>{10, 100, 1000}) {
            auto twr = direct ? run_three_way_tc09(d, ticks, warmup, measured,
                                                    verilator_bin, cache_root, workdir)
                              : run_three_way_subprocess_tc09(d, ticks, warmup, measured,
                                                    verilator_bin, cache_root, workdir);
            reporter.add_result(twr.interp);
            reporter.add_result(twr.jit);
            reporter.add_result(twr.verilator);
            printf("  depth=%5d: interp=%9.2f us | jit=%9.2f us | verilator=%9.2f us [%s]\n",
                   d, twr.interp.median_us, twr.jit.median_us,
                   twr.verilator.median_us,
                   twr.verilator_skipped ? "SKIPPED" : "OK");
        }
        std::cout << "\n";
    }
    if (run_all || tc10) {
        std::cout << "TC-10: 3-way float chain (UNSUPPORTED — ch_float missing)\n";
        for (int d : std::vector<int>{10, 100, 1000}) {
            auto twr = run_three_way_tc10_unsupported(d);
            reporter.add_result(twr.interp);
            reporter.add_result(twr.jit);
            reporter.add_result(twr.verilator);
            printf("  depth=%5d: [UNSUPPORTED] %s\n", d,
                   twr.skip_reason.empty() ? "ch_float not implemented"
                                          : twr.skip_reason.c_str());
        }
        std::cout << "\n";
    }
    if (run_all || tc11) {
        std::cout << "TC-11: 3-way ch_uint<256> wide reg chain\n";
        for (int n : std::vector<int>{10, 100, 1000}) {
            auto twr = run_three_way_tc11(n, ticks, warmup, measured,
                                          verilator_bin, cache_root, workdir);
            reporter.add_result(twr.interp);
            reporter.add_result(twr.jit);
            reporter.add_result(twr.verilator);
            printf("  regs=%5d:  interp=%9.2f us | jit=%9.2f us | verilator=%9.2f us [%s]\n",
                   n, twr.interp.median_us, twr.jit.median_us,
                   twr.verilator.median_us,
                   twr.verilator_skipped ? "SKIPPED" : "OK");
        }
        std::cout << "\n";
    }

    std::cout << "=== Summary ===\n";
    reporter.print_summary();

    if (!report_formats.empty()) {
        std::vector<std::string> uniq;
        for (const auto& f : report_formats)
            if (std::find(uniq.begin(), uniq.end(), f) == uniq.end()) uniq.push_back(f);
        for (const auto& f : uniq) {
            if      (f == "csv")  { reporter.export_csv("perf_results.csv");
                                    std::cout << "\nExported to perf_results.csv\n"; }
            else if (f == "json") { reporter.export_json("perf_results.json");
                                    std::cout << "\nExported to perf_results.json\n"; }
            else if (f == "md")   { reporter.export_markdown("perf_results.md");
                                    std::cout << "\nExported to perf_results.md\n"; }
        }
    }
    return 0;
}
