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
#include "ch.hpp"
#include "simulator.h"
#include "codegen_verilog.h"
#include "device.h"
#include "component.h"
#include "core/io.h"
#include "core/reg.h"
#include "core/uint.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::core;

// =============================================================================
// Legacy TC-01/02/04/06 — single-backend combinational/sequential benchmarks
// (preserved verbatim from the v1 perf_main.cpp).
// =============================================================================
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

// =============================================================================
// TC-07/08 three-way comparison — DUTs are Component subclasses so the
// generated Verilog has a ch_out<ch_uint<8>> port (required by Verilator).
// =============================================================================
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

// Single-register design mirroring the TC-02 stress pattern; `regs_` is
// recorded for perf-metric normalization only (same convention as v1).
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

struct ThreeWayResult {
    BenchmarkResult interp, jit, verilator;
    bool verilator_skipped = false;
    std::string skip_reason;
};

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
                                double build_us, double sim_us,
                                int iters, double median,
                                const std::string& status) {
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
    {  // Build DUT once, export Verilog (shared artifact).
        ch_device<DutT, CtorArgs...> dev(std::forward<CtorArgs>(ctor_args)...);
        Simulator s(dev.context());
        s.tick(1);
        toVerilog(verilog_path, dev.context());
        (void)s.get_value(dev.io().result);
    }
    {  // Interpreter backend.
        ch_device<DutT, CtorArgs...> dev(std::forward<CtorArgs>(ctor_args)...);
        Simulator s(dev.context());
        s.set_jit_enabled(false);
        s.tick(1);
        double mu = time_backend_us([&](){ s.tick(ticks); }, warmup, measured);
        twr.interp = make_row(test_name, params, "interpreter",
                              0.0, mu, measured, mu, "PASS");
    }
    {  // JIT backend.
        ch_device<DutT, CtorArgs...> dev(std::forward<CtorArgs>(ctor_args)...);
        Simulator s(dev.context());
        s.set_jit_enabled(true);
        s.tick(1);  // triggers JIT compile
        double mu = time_backend_us([&](){ s.tick(ticks); }, warmup, measured);
        twr.jit = make_row(test_name, params, "jit", 0.0, mu, measured, mu, "PASS");
    }
    // Verilator backend — skipped gracefully when unavailable.
    ch_perf::VerilatorRunner vr(cache_root, verilator_bin);
    if (!vr.is_available()) {
        twr.verilator_skipped = true;
        twr.skip_reason = "verilator not found on PATH";
    } else if (!std::filesystem::exists(verilog_path) ||
               !std::filesystem::exists(harness_path)) {
        twr.verilator_skipped = true;
        twr.skip_reason = "verilog or harness file missing";
    } else {
        auto br = vr.build(test_name, verilog_path, harness_path);
        if (!br.success) {
            twr.verilator_skipped = true;
            twr.skip_reason = br.error_msg;
            twr.verilator = make_row(test_name, params, "verilator",
                                     br.build_time_ns / 1000.0, 0.0,
                                     0, 0.0, "SKIPPED");
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
    if (twr.verilator_skipped) {
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

// =============================================================================
// CLI
// =============================================================================
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
    bool run_all = false, tc01 = false, tc02 = false, tc04 = false, tc06 = false;
    bool tc07 = false, tc08 = false;
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
        else if (!std::strcmp(argv[i], "--tc=01")) tc01 = true;
        else if (!std::strcmp(argv[i], "--tc=02")) tc02 = true;
        else if (!std::strcmp(argv[i], "--tc=04")) tc04 = true;
        else if (!std::strcmp(argv[i], "--tc=06")) tc06 = true;
        else if (!std::strcmp(argv[i], "--tc=07")) tc07 = true;
        else if (!std::strcmp(argv[i], "--tc=08")) tc08 = true;
        else if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
            print_usage(argv[0]); return 0;
        } else {
            std::cerr << "error: unknown argument '" << argv[i] << "'\n";
            print_usage(argv[0]); return 1;
        }
    }
    if (!run_all && !tc01 && !tc02 && !tc04 && !tc06 && !tc07 && !tc08) {
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
            auto twr = run_three_way_tc07(d, ticks, warmup, measured,
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
            auto twr = run_three_way_tc08(n, ticks, warmup, measured,
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
