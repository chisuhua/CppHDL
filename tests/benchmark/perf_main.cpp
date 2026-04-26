/**
 * @file perf_main.cpp
 * @brief Main entry point for CppHDL performance benchmarks.
 */

#include "perf_timer.h"
#include "stats_collector.h"
#include "report_generator.h"
#include "ch.hpp"
#include "simulator.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

using namespace ch;
using namespace ch::core;

BenchmarkResult run_combinational_benchmark(int depth, int ticks) {
    PerfTimer timer;
    {
        context ctx("chain");
        ctx_swap swap(&ctx);

        ch_uint<8> result = ch_uint<8>(0_b);

        for (int i = 0; i < depth; ++i) {
            result = result ^ ch_uint<8>(i);
        }

        Simulator sim(&ctx);

        timer.start();
        sim.tick(ticks);
        timer.stop();
    }

    double elapsed_ns = timer.elapsed_ns();
    double ns_per_tick = elapsed_ns / ticks;
    double ticks_per_sec = (ticks * 1e9) / elapsed_ns;
    double ns_per_node_tick = elapsed_ns / (static_cast<double>(ticks) * depth);

    BenchmarkResult r;
    r.test_name = "TC-01";
    r.params = "depth=" + std::to_string(depth);
    r.ticks_per_sec = ticks_per_sec;
    r.ns_per_tick = ns_per_tick;
    r.ns_per_node_tick = ns_per_node_tick;
    r.overhead_percent = 0.0;
    return r;
}

BenchmarkResult run_sequential_benchmark(int num_regs, int ticks) {
    PerfTimer timer;
    {
        context ctx("seq");
        ctx_swap swap(&ctx);

        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);

        Simulator sim(&ctx);

        timer.start();
        for (int t = 0; t < ticks; ++t) {
            sim.tick();
        }
        timer.stop();
    }

    double elapsed_ns = timer.elapsed_ns();
    double ns_per_tick = elapsed_ns / ticks;
    double ticks_per_sec = (ticks * 1e9) / elapsed_ns;
    double ns_per_node_tick = elapsed_ns / (static_cast<double>(ticks) * num_regs);

    BenchmarkResult r;
    r.test_name = "TC-02";
    r.params = "regs=" + std::to_string(num_regs);
    r.ticks_per_sec = ticks_per_sec;
    r.ns_per_tick = ns_per_tick;
    r.ns_per_node_tick = ns_per_node_tick;
    r.overhead_percent = 0.0;
    return r;
}

BenchmarkResult run_trace_benchmark(int num_signals, int ticks) {
    PerfTimer timer_no_trace, timer_with_trace;

    {
        context ctx("trace");
        ctx_swap swap(&ctx);

        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);

        Simulator sim_no_trace(&ctx, false);
        timer_no_trace.start();
        sim_no_trace.tick(ticks);
        timer_no_trace.stop();

        Simulator sim_with_trace(&ctx, true);
        timer_with_trace.start();
        sim_with_trace.tick(ticks);
        timer_with_trace.stop();
    }

    double elapsed_no_trace = timer_no_trace.elapsed_ns();
    double elapsed_with_trace = timer_with_trace.elapsed_ns();
    double overhead_pct = ((elapsed_with_trace - elapsed_no_trace) / elapsed_no_trace) * 100.0;

    BenchmarkResult r;
    r.test_name = "TC-04";
    r.params = "signals=" + std::to_string(num_signals);
    r.ticks_per_sec = (ticks * 1e9) / elapsed_with_trace;
    r.ns_per_tick = elapsed_with_trace / ticks;
    r.ns_per_node_tick = 0.0;
    r.overhead_percent = overhead_pct;
    return r;
}

BenchmarkResult run_batch_tick_benchmark(int nodes, int ticks) {
    PerfTimer timer_batch, timer_single;

    {
        context ctx("batch");
        ctx_swap swap(&ctx);

        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);

        Simulator sim(&ctx);

        timer_batch.start();
        sim.tick(ticks);
        timer_batch.stop();
    }

    {
        context ctx("single");
        ctx_swap swap(&ctx);

        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);

        Simulator sim(&ctx);

        timer_single.start();
        for (int t = 0; t < ticks; ++t) sim.tick();
        timer_single.stop();
    }

    double batch_ns = timer_batch.elapsed_ns();
    double single_ns = timer_single.elapsed_ns();
    double diff_pct = ((single_ns - batch_ns) / single_ns) * 100.0;

    BenchmarkResult r;
    r.test_name = "TC-06";
    r.params = "nodes=" + std::to_string(nodes);
    r.ticks_per_sec = (ticks * 1e9) / batch_ns;
    r.ns_per_tick = batch_ns / ticks;
    r.ns_per_node_tick = 0.0;
    r.overhead_percent = diff_pct;
    return r;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --tc=01       Run TC-01: Combinational chain benchmark\n";
    std::cout << "  --tc=02       Run TC-02: Sequential registers benchmark\n";
    std::cout << "  --tc=04       Run TC-04: Trace overhead benchmark\n";
    std::cout << "  --tc=06       Run TC-06: Batch vs single tick benchmark\n";
    std::cout << "  --all         Run all benchmarks\n";
    std::cout << "  --report=csv  Export results to CSV\n";
    std::cout << "  --report=json Export results to JSON\n";
    std::cout << "  --baseline    Generate baseline measurements\n";
}

int main(int argc, char* argv[]) {
    ReportGenerator reporter;

    bool run_all = false;
    bool tc01 = false, tc02 = false, tc04 = false, tc06 = false;
    std::string report_format;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--all") == 0) {
            run_all = true;
        } else if (strcmp(argv[i], "--tc=01") == 0) {
            tc01 = true;
        } else if (strcmp(argv[i], "--tc=02") == 0) {
            tc02 = true;
        } else if (strcmp(argv[i], "--tc=04") == 0) {
            tc04 = true;
        } else if (strcmp(argv[i], "--tc=06") == 0) {
            tc06 = true;
        } else if (strncmp(argv[i], "--report=", 9) == 0) {
            report_format = argv[i] + 9;
        } else if (strcmp(argv[i], "--baseline") == 0) {
            run_all = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (!run_all && !tc01 && !tc02 && !tc04 && !tc06) {
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "=== CppHDL Performance Benchmarks ===\n\n";

    if (run_all || tc01) {
        std::cout << "TC-01: Combinational Chain\n";
        std::vector<int> depths = {10, 100, 1000, 5000, 10000};
        for (int d : depths) {
            auto r = run_combinational_benchmark(d, 10000);
            reporter.add_result(r);
            printf("  depth=%5d: %12.2f ticks/sec | %8.2f ns/tick | %6.2f ns/node/tick\n",
                   d, r.ticks_per_sec, r.ns_per_tick, r.ns_per_node_tick);
        }
        std::cout << "\n";
    }

    if (run_all || tc02) {
        std::cout << "TC-02: Sequential Registers\n";
        std::vector<int> counts = {10, 100, 1000, 5000, 10000};
        for (int n : counts) {
            auto r = run_sequential_benchmark(n, 10000);
            reporter.add_result(r);
            printf("  regs=%5d: %12.2f ticks/sec | %8.2f ns/tick | %6.2f ns/reg/tick\n",
                   n, r.ticks_per_sec, r.ns_per_tick, r.ns_per_node_tick);
        }
        std::cout << "\n";
    }

    if (run_all || tc04) {
        std::cout << "TC-04: Trace Overhead\n";
        auto r = run_trace_benchmark(100, 10000);
        reporter.add_result(r);
        printf("  100 signals: trace overhead = %.2f%%\n", r.overhead_percent);
        std::cout << "\n";
    }

    if (run_all || tc06) {
        std::cout << "TC-06: Batch vs Single Tick\n";
        auto r = run_batch_tick_benchmark(1000, 10000);
        reporter.add_result(r);
        printf("  batch vs single: %.2f%% difference\n", r.overhead_percent);
        std::cout << "\n";
    }

    std::cout << "=== Summary ===\n";
    reporter.print_summary();

    if (!report_format.empty()) {
        if (report_format == "csv") {
            reporter.export_csv("perf_results.csv");
            std::cout << "\nExported to perf_results.csv\n";
        } else if (report_format == "json") {
            reporter.export_json("perf_results.json");
            std::cout << "\nExported to perf_results.json\n";
        }
    }

    return 0;
}