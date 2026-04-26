/**
 * @file perf_timer.h
 * @brief High-precision performance timer for CppHDL benchmarks.
 * @author CppHDL Agent
 * @date 2026-04-25
 */

#ifndef CPPCPPHDL_PERF_TIMER_H
#define CPPCPPHDL_PERF_TIMER_H

#include <chrono>

class PerfTimer {
public:
    using clock = std::chrono::high_resolution_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::duration<double, std::nano>;

    PerfTimer() : start_time_(), elapsed_ns_(0), running_(false) {}

    void start() {
        start_time_ = clock::now();
        running_ = true;
    }

    void stop() {
        if (running_) {
            elapsed_ns_ += std::chrono::duration<double, std::nano>(
                clock::now() - start_time_
            ).count();
            running_ = false;
        }
    }

    void reset() {
        elapsed_ns_ = 0;
        running_ = false;
    }

    double elapsed_ns() const { return elapsed_ns_; }
    double elapsed_us() const { return elapsed_ns_ / 1000.0; }
    double elapsed_ms() const { return elapsed_ns_ / 1000000.0; }
    double elapsed_s() const { return elapsed_ns_ / 1000000000.0; }

    bool is_running() const { return running_; }

private:
    time_point start_time_;
    double elapsed_ns_;
    bool running_;
};

#endif  // CPPCPPHDL_PERF_TIMER_H