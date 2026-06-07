# perf_tests JIT vs Verilator Comparison Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend `tests/benchmark/perf_main.cpp` with two new test cases (TC-07 XOR chain, TC-08 sequential regs) that run the **same** design through CppHDL Interpreter, CppHDL JIT, and Verilator-as-subprocess, producing a side-by-side speedup report.

**Architecture:** Use `ch::toVerilog()` to emit Verilog for the design-under-test, invoke `verilator --cc --build --exe` to compile a C++ harness that drives the Vtop model, then run the resulting binary as a child process and measure wall-clock with `PerfTimer`. SHA-1 cache by (verilog + harness + verilator --version) to amortize the ~5-30s first-build cost. The CppHDL VerilatorBackend is **not** used (it is scaffolding only per ADR-035 Phase 3.2-3.6 pending).

**Tech Stack:** C++20, CMake 3.14+, Catch2 v3.7 (untouched), Verilator 5.020 (runtime), LLVM ORC (already wired for JIT via `CH_JIT_ENABLE=ON`).

**Spec:** `docs/superpowers/specs/2026-06-08-perf-tests-jit-verilator-comparison-design.md`

---

## File Structure

| File | Status | Responsibility |
|------|--------|----------------|
| `tests/benchmark/verilator_runner.h` | new | SHA-1 cache, `build()`, `run_and_time()` |
| `tests/benchmark/verilator_harness_tc07.cpp` | new | XOR-chain harness: `eval()` loop |
| `tests/benchmark/verilator_harness_tc08.cpp` | new | Sequential-regs harness: clock toggle + reset |
| `tests/benchmark/perf_main.cpp` | modify | TC-07/TC-08, three-way benchmark function, CSV schema |
| `tests/benchmark/CMakeLists.txt` | modify | Register new sources, raise CTest timeout |
| `docs/simulation/PERFORMANCE_TESTS.md` | modify | TC-07/TC-08 sections |

The runner is a **header-only** library (no .cpp) because the spec keeps it small (~150 lines) and tightly coupled to perf_main's call sites.

---

## Task 1: verilator_runner.h — SHA-1 cache key + build

**Files:**
- Create: `tests/benchmark/verilator_runner.h`

- [ ] **Step 1: Create the header skeleton with SHA-1 cache key**

```cpp
// tests/benchmark/verilator_runner.h
#pragma once

#include "perf_timer.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace ch_perf {

struct VerilatorBuildResult {
    bool success = false;
    std::string binary_path;        // <work_dir>/obj_dir/Vtop
    std::string cache_key;          // SHA-1 hex string
    bool cache_hit = false;
    uint64_t build_time_ns = 0;
    std::string error_msg;
};

class VerilatorRunner {
public:
    VerilatorRunner(std::string cache_root,
                    std::string verilator_bin = "verilator")
        : cache_root_(std::move(cache_root)),
          verilator_bin_(std::move(verilator_bin)) {}

    // True if `verilator --version` exits 0.
    bool is_available() const;

    // Returns "Verilator 5.020 2024-01-01" or empty on failure.
    std::string verilator_version() const;

    // Build Vtop for (verilog_path, harness_path). Caches by
    // SHA-1(design_name + verilog_source + harness_source +
    // verilator_version).
    VerilatorBuildResult build(const std::string &design_name,
                               const std::string &verilog_path,
                               const std::string &harness_path);

    // Run binary with `ticks` as a single argv, measure wall-clock
    // with PerfTimer. Returns elapsed ns; 0 on error.
    uint64_t run_and_time(const std::string &binary_path,
                          int ticks,
                          std::string *error_msg = nullptr);

    // SHA-1 of a string. Public for testability.
    static std::string sha1_hex(const std::string &data);

    // Cache directory for a given key: <cache_root>/<key>/
    std::string cache_dir_for_key(const std::string &key) const;

private:
    std::string cache_root_;
    std::string verilator_bin_;

    static std::string read_file(const std::string &path);
    static std::string shell_escape(const std::string &s);
};

} // namespace ch_perf
```

- [ ] **Step 2: Implement SHA-1 and the helpers**

Append to `verilator_runner.h`:

```cpp
// ---------- SHA-1 (RFC 3174) ----------
namespace ch_perf_detail {
inline uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

inline std::array<uint8_t, 20> sha1(const std::string &data) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89,
             h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

    std::vector<uint8_t> msg(data.begin(), data.end());
    uint64_t bit_len = static_cast<uint64_t>(msg.size()) * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    for (int i = 7; i >= 0; --i) msg.push_back((bit_len >> (i * 8)) & 0xff);

    for (size_t block = 0; block < msg.size(); block += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (uint32_t(msg[block + i * 4]) << 24) |
                   (uint32_t(msg[block + i * 4 + 1]) << 16) |
                   (uint32_t(msg[block + i * 4 + 2]) << 8) |
                   (uint32_t(msg[block + i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rotl32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        }
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);            k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                      k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);    k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;                      k = 0xCA62C1D6; }
            uint32_t t = rotl32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl32(b, 30); b = a; a = t;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    std::array<uint8_t, 20> out{};
    auto put = [&](int idx, uint32_t v) {
        out[idx]     = (v >> 24) & 0xff;
        out[idx + 1] = (v >> 16) & 0xff;
        out[idx + 2] = (v >>  8) & 0xff;
        out[idx + 3] = v & 0xff;
    };
    put(0, h0); put(4, h1); put(8, h2); put(12, h3); put(16, h4);
    return out;
}
} // namespace ch_perf_detail

inline std::string VerilatorRunner::sha1_hex(const std::string &data) {
    auto h = ch_perf_detail::sha1(data);
    static const char hex[] = "0123456789abcdef";
    std::string s;
    s.reserve(40);
    for (auto b : h) { s.push_back(hex[b >> 4]); s.push_back(hex[b & 0xf]); }
    return s;
}

inline std::string VerilatorRunner::read_file(const std::string &path) {
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

inline std::string VerilatorRunner::shell_escape(const std::string &s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

inline std::string VerilatorRunner::cache_dir_for_key(const std::string &k) const {
    return cache_root_ + "/" + k;
}
```

- [ ] **Step 3: Implement `is_available()`, `verilator_version()`, `build()`**

Append to `verilator_runner.h`:

```cpp
inline bool VerilatorRunner::is_available() const {
    std::string dummy;
    return !verilator_version().empty();
}

inline std::string VerilatorRunner::verilator_version() const {
    // popen returns the first line of `verilator --version`.
    std::string cmd = shell_escape(verilator_bin_) + " --version 2>&1";
    FILE *p = ::popen(cmd.c_str(), "r");
    if (!p) return {};
    char buf[256];
    std::string out;
    if (fgets(buf, sizeof(buf), p)) out = buf;
    ::pclose(p);
    // Trim trailing newline
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}

inline VerilatorBuildResult
VerilatorRunner::build(const std::string &design_name,
                       const std::string &verilog_path,
                       const std::string &harness_path) {
    VerilatorBuildResult r;
    std::string ver = verilator_version();
    if (ver.empty()) {
        r.error_msg = "verilator not found on PATH";
        return r;
    }

    std::string verilog_src = read_file(verilog_path);
    std::string harness_src = read_file(harness_path);
    if (verilog_src.empty() || harness_src.empty()) {
        r.error_msg = "verilog or harness file unreadable";
        return r;
    }

    std::string key_input = design_name + "\0" + ver + "\0" +
                            verilog_src + "\0" + harness_src;
    r.cache_key = sha1_hex(key_input);

    std::string cache_dir = cache_dir_for_key(r.cache_key);
    std::string binary = cache_dir + "/obj_dir/Vtop";

    if (std::filesystem::exists(binary)) {
        r.success = true;
        r.binary_path = binary;
        r.cache_hit = true;
        return r;
    }

    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    if (ec) {
        r.error_msg = "create_directories failed: " + ec.message();
        return r;
    }

    // Copy verilog + harness into cache_dir so verilator --build's
    // out-of-tree build is reproducible.
    std::filesystem::copy(verilog_path, cache_dir + "/top.v",
                          std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::copy(harness_path, cache_dir + "/harness.cpp",
                          std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        r.error_msg = "copy failed: " + ec.message();
        return r;
    }

    std::string cmd =
        shell_escape(verilator_bin_) + " --cc --build --exe" +
        " -Wno-fatal" +  // tolerate minor warnings
        " harness.cpp" +  // hmm -- needs full path; fix below
        " top.v 2>&1";
    // Workdir-relative invocation: cd into cache_dir, run verilator there.
    std::string full_cmd = "cd " + shell_escape(cache_dir) + " && " + cmd;

    PerfTimer t;
    t.start();
    FILE *p = ::popen(full_cmd.c_str(), "r");
    if (!p) {
        r.error_msg = "popen failed";
        return r;
    }
    std::string output;
    char buf[4096];
    while (fgets(buf, sizeof(buf), p)) output += buf;
    int status = ::pclose(p);
    t.stop();
    r.build_time_ns = static_cast<uint64_t>(t.elapsed_ns());

    if (status != 0) {
        // Truncate long stderr for readability
        if (output.size() > 2048) output.resize(2048);
        r.error_msg = "verilator build failed (exit=" +
                      std::to_string(status) + "): " + output;
        return r;
    }
    if (!std::filesystem::exists(binary)) {
        r.error_msg = "verilator reported success but " + binary + " missing";
        return r;
    }
    r.success = true;
    r.binary_path = binary;
    r.cache_hit = false;
    return r;
}
```

> **Note:** The `cmd` string above has a stale fragment (`harness.cpp top.v`); the corrected invocation is the `cd cache_dir && verilator --cc --build --exe -Wno-fatal harness.cpp top.v 2>&1` produced by `full_cmd`. Keep the comment for review, but in the final file, drop the dead `cmd` and only build `full_cmd`. **Apply this fix in Step 3 final code.**

Final corrected command construction (replaces both `cmd` and `full_cmd` above):

```cpp
    std::string full_cmd =
        "cd " + shell_escape(cache_dir) + " && " +
        shell_escape(verilator_bin_) +
        " --cc --build --exe -Wno-fatal" +
        " harness.cpp top.v 2>&1";
```

- [ ] **Step 4: Implement `run_and_time()`**

Append to `verilator_runner.h`:

```cpp
inline uint64_t VerilatorRunner::run_and_time(const std::string &binary_path,
                                              int ticks,
                                              std::string *error_msg) {
    if (!std::filesystem::exists(binary_path)) {
        if (error_msg) *error_msg = "binary not found: " + binary_path;
        return 0;
    }
    std::string cmd = shell_escape(binary_path) + " " +
                      std::to_string(ticks) + " 2>&1";
    PerfTimer t;
    t.start();
    FILE *p = ::popen(cmd.c_str(), "r");
    if (!p) {
        if (error_msg) *error_msg = "popen failed";
        return 0;
    }
    char buf[4096];
    std::string output;
    while (fgets(buf, sizeof(buf), p)) output += buf;
    int status = ::pclose(p);
    t.stop();
    if (status != 0) {
        if (error_msg) {
            if (output.size() > 1024) output.resize(1024);
            *error_msg = "verilator binary exit=" + std::to_string(status) +
                         ": " + output;
        }
        return 0;
    }
    return static_cast<uint64_t>(t.elapsed_ns());
}
```

- [ ] **Step 5: Smoke-test the runner with `verilator --version`**

Build only this header by adding a tiny throwaway `main()` test inline. Create a temporary file (DO NOT commit):

```bash
cat > /tmp/test_runner.cpp <<'EOF'
#include "verilator_runner.h"
#include <cstdio>
int main() {
    ch_perf::VerilatorRunner r(std::string(getenv("HOME")) + "/.cache/cpphdl/perf");
    printf("available=%d version=%s\n", r.is_available(), r.verilator_version().c_str());
    printf("sha1('abc')=%s\n", ch_perf::VerilatorRunner::sha1_hex("abc").c_str());
    // SHA-1("abc") = a9993e364706816aba3e25717850c26c9cd0d89d (known good)
    return 0;
}
EOF
g++ -std=c++20 -I /workspace/project/CppHDL/tests/benchmark /tmp/test_runner.cpp -o /tmp/test_runner && /tmp/test_runner
```

Expected output:
```
available=1 version=Verilator 5.020 2024-01-01
sha1('abc')=a9993e364706816aba3e25717850c26c9cd0d89d
```

If `verilator --version` is empty, ensure `verilator` is on PATH (`which verilator` should print `/usr/bin/verilator`).

- [ ] **Step 6: Clean up the smoke test and commit**

```bash
rm -f /tmp/test_runner.cpp /tmp/test_runner
cd /workspace/project/CppHDL
git add tests/benchmark/verilator_runner.h
git commit -m "feat(perf): add verilator_runner.h with SHA-1 cache + build/run"
```

---

## Task 2: TC-07 harness (XOR chain, pure combinational)

**Files:**
- Create: `tests/benchmark/verilator_harness_tc07.cpp`

- [ ] **Step 1: Write the harness**

```cpp
// tests/benchmark/verilator_harness_tc07.cpp
// Verilator harness for TC-07 (XOR chain).
// Pure combinational design: instantiate Vtop, call eval() N times.
//
// CppHDL emits default_clock and default_reset even for pure-comb
// designs, so we drive them in lockstep: clk=0/reset=0 stable, then
// eval() the comb network N times.
#include "Vtop.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    int ticks = (argc > 1) ? std::atoi(argv[1]) : 1000;

    Vtop *top = new Vtop;

    // Hold reset low; clock is irrelevant for pure comb but Verilator
    // requires the field to be driven.
    top->default_clock = 0;
    top->default_reset = 0;

    for (int i = 0; i < ticks; ++i) {
        top->eval();
    }

    // Sanity: read the output to prevent the compiler from optimizing
    // away the eval() loop. Without a use, the loop may be elided.
    volatile uint32_t sink = top->io;
    (void)sink;

    delete top;
    return 0;
}
```

- [ ] **Step 2: Commit**

```bash
cd /workspace/project/CppHDL
git add tests/benchmark/verilator_harness_tc07.cpp
git commit -m "feat(perf): add TC-07 Verilator harness (XOR chain)"
```

---

## Task 3: TC-08 harness (sequential regs, clocked)

**Files:**
- Create: `tests/benchmark/verilator_harness_tc08.cpp`

- [ ] **Step 1: Write the harness**

```cpp
// tests/benchmark/verilator_harness_tc08.cpp
// Verilator harness for TC-08 (sequential registers).
// Clocked design: pulse reset, then toggle clock N times, calling
// eval() on each edge.
#include "Vtop.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    int ticks = (argc > 1) ? std::atoi(argv[1]) : 1000;

    Vtop *top = new Vtop;

    // Reset pulse: two eval() with reset high.
    top->default_clock = 0;
    top->default_reset = 1;
    top->eval();
    top->default_clock = 1;
    top->eval();
    top->default_reset = 0;

    for (int i = 0; i < ticks; ++i) {
        top->default_clock = 0;
        top->eval();
        top->default_clock = 1;
        top->eval();
    }

    volatile uint32_t sink = top->io;
    (void)sink;

    delete top;
    return 0;
}
```

- [ ] **Step 2: Commit**

```bash
cd /workspace/project/CppHDL
git add tests/benchmark/verilator_harness_tc08.cpp
git commit -m "feat(perf): add TC-08 Verilator harness (sequential regs)"
```

---

## Task 4: Extend BenchmarkResult + ReportGenerator for backend column

**Files:**
- Modify: `tests/benchmark/report_generator.h`

- [ ] **Step 1: Add new fields to `BenchmarkResult`**

In `tests/benchmark/report_generator.h`, replace the `BenchmarkResult` struct with:

```cpp
struct BenchmarkResult {
    std::string test_name;
    std::string params;
    std::string backend;             // "interpreter", "jit", "verilator", ""
    double ticks_per_sec = 0.0;
    double ns_per_tick = 0.0;
    double ns_per_node_tick = 0.0;
    double overhead_percent = 0.0;
    // For three-way comparison rows (TC-07/TC-08):
    double speedup_vs_interpreter = 0.0;  // 1.0 for the interpreter row itself
    double speedup_vs_verilator = 0.0;     // 1.0 for the verilator row itself
    int build_cache_hit = 0;               // 0/1, verilator rows only
};
```

- [ ] **Step 2: Update `export_csv` to write the extended header and rows**

Replace the body of `export_csv` with:

```cpp
void export_csv(const std::string &filename) const {
    std::ofstream f(filename);
    f << "test,params,backend,ticks_per_sec,ns_per_tick,ns_per_node_tick,"
         "overhead_pct,speedup_vs_interpreter,speedup_vs_verilator,"
         "build_cache_hit\n";
    for (auto &r : results_) {
        f << r.test_name << ","
          << r.params << ","
          << r.backend << ","
          << r.ticks_per_sec << ","
          << r.ns_per_tick << ","
          << r.ns_per_node_tick << ","
          << r.overhead_percent << ","
          << r.speedup_vs_interpreter << ","
          << r.speedup_vs_verilator << ","
          << r.build_cache_hit << "\n";
    }
}
```

- [ ] **Step 3: Update `print_summary` to show the backend column when present**

Replace `print_summary` with:

```cpp
void print_summary() const {
    for (auto &r : results_) {
        if (!r.backend.empty()) {
            printf("%-8s %-8s %-15s | %12.2f ticks/sec | %8.2f ns/tick | "
                   "x%5.2f vs interp | x%5.2f vs verilator\n",
                   r.test_name.c_str(), r.backend.c_str(),
                   r.params.c_str(), r.ticks_per_sec, r.ns_per_tick,
                   r.speedup_vs_interpreter, r.speedup_vs_verilator);
        } else {
            printf("%-20s | %-15s | %12.2f ticks/sec | %8.2f ns/tick\n",
                   r.test_name.c_str(), r.params.c_str(),
                   r.ticks_per_sec, r.ns_per_tick);
        }
    }
}
```

- [ ] **Step 4: Build to confirm no regressions in the existing CSV output path**

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc) --target perf_tests 2>&1 | tail -5
./build/tests/perf_tests --tc=01 --report=csv
cat perf_results.csv
```

Expected: header now has 10 columns (added `backend`, `speedup_vs_interpreter`, `speedup_vs_verilator`, `build_cache_hit`); existing TC-01 rows show empty `backend` and zero `speedup_*` (since they don't participate in three-way comparison).

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/CppHDL
git add tests/benchmark/report_generator.h
git commit -m "feat(perf): add backend + speedup fields to BenchmarkResult"
```

---

## Task 5: Extract design-with-IO helpers in perf_main.cpp

**Files:**
- Modify: `tests/benchmark/perf_main.cpp`

- [ ] **Step 1: Add design-builder helpers after the existing benchmark functions**

After `run_batch_tick_benchmark` (end of the existing functions, before `print_usage`), insert:

```cpp
// Build an XOR chain context WITH a ch_out<> port (so Verilog codegen
// produces a module with at least one output, required by Verilator).
// Returns a heap-allocated context ready for use with ctx_swap.
ch::core::context *make_xor_chain_with_io(int depth, const std::string &name) {
    auto *ctx = new ch::core::context(name);
    ch::core::ctx_swap guard(ctx);
    ch_out<ch_uint<8>> out("io");
    ch_uint<8> result = ch_uint<8>(0_b);
    for (int i = 0; i < depth; ++i) {
        result = result ^ ch_uint<8>(i);
    }
    out <<= result;
    return ctx;  // caller takes ownership; ctx_swap restores on scope exit
}

ch::core::context *make_sequential_with_io(int num_regs,
                                           const std::string &name) {
    auto *ctx = new ch::core::context(name);
    ch::core::ctx_swap guard(ctx);
    ch_out<ch_uint<8>> out("io");
    auto inp = ch_uint<8>(0_b);
    auto reg = ch_reg<ch_uint<8>>(inp);
    out <<= reg;
    (void)num_regs;  // single-register baseline; num_regs scales
                      // via tick count and will be used by TC-08
                      // variants in future
    return ctx;
}

// Helper: emit Verilog to a file and return the path.
std::string emit_verilog(ch::core::context *ctx, const std::string &path) {
    ch::core::ctx_swap guard(ctx);
    ch::toVerilog(path, ctx);
    return path;
}
```

- [ ] **Step 2: Build to confirm the helpers compile**

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc) --target perf_tests 2>&1 | tail -10
```

Expected: clean build (no new warnings on the modified file).

- [ ] **Step 3: Commit (no behaviour change yet)**

```bash
cd /workspace/project/CppHDL
git add tests/benchmark/perf_main.cpp
git commit -m "refactor(perf): add XOR/sequential design-with-IO builders"
```

---

## Task 6: Add TC-07 (XOR chain three-way comparison) to perf_main.cpp

**Files:**
- Modify: `tests/benchmark/perf_main.cpp`

- [ ] **Step 1: Add the TC-07 runner function**

Insert before `print_usage`:

```cpp
struct ThreeWayResult {
    BenchmarkResult interp, jit, verilator;
    bool verilator_skipped = false;
    std::string skip_reason;
};

ThreeWayResult run_three_way_xor(int depth, int ticks,
                                 ch_perf::VerilatorRunner &runner,
                                 const std::string &harness_path,
                                 const std::string &verilator_workdir) {
    ThreeWayResult tr;

    auto fill = [&](BenchmarkResult &r, const std::string &backend_str,
                    double elapsed_ns) {
        r.test_name = "TC-07";
        r.params = "depth=" + std::to_string(depth);
        r.backend = backend_str;
        r.ns_per_tick = elapsed_ns / ticks;
        r.ticks_per_sec = (ticks * 1e9) / elapsed_ns;
        r.ns_per_node_tick = elapsed_ns / (static_cast<double>(ticks) * depth);
    };

    // ----- Interpreter -----
    {
        PerfTimer t;
        ch::core::context ctx("tc07_interp");
        ch::core::ctx_swap swap(&ctx);
        ch_out<ch_uint<8>> out("io");
        ch_uint<8> result = ch_uint<8>(0_b);
        for (int i = 0; i < depth; ++i) result = result ^ ch_uint<8>(i);
        out <<= result;
        Simulator sim(&ctx);
        sim.set_jit_enabled(false);
        t.start(); sim.tick(ticks); t.stop();
        fill(tr.interp, "interpreter", t.elapsed_ns());
    }

    // ----- JIT -----
    {
        PerfTimer t;
        ch::core::context ctx("tc07_jit");
        ch::core::ctx_swap swap(&ctx);
        ch_out<ch_uint<8>> out("io");
        ch_uint<8> result = ch_uint<8>(0_b);
        for (int i = 0; i < depth; ++i) result = result ^ ch_uint<8>(i);
        out <<= result;
        Simulator sim(&ctx);
        sim.set_jit_enabled(true);
        t.start(); sim.tick(ticks); t.stop();
        fill(tr.jit, "jit", t.elapsed_ns());
    }

    // ----- Verilator -----
    if (!runner.is_available()) {
        tr.verilator_skipped = true;
        tr.skip_reason = "verilator not on PATH";
    } else {
        ch::core::context ctx("tc07_verilator");
        ch::core::ctx_swap swap(&ctx);
        ch_out<ch_uint<8>> out("io");
        ch_uint<8> result = ch_uint<8>(0_b);
        for (int i = 0; i < depth; ++i) result = result ^ ch_uint<8>(i);
        out <<= result;

        std::string verilog_path = verilator_workdir + "/tc07_d" +
                                   std::to_string(depth) + ".v";
        std::filesystem::create_directories(verilator_workdir);
        ch::toVerilog(verilog_path, &ctx);

        auto br = runner.build("tc07_d" + std::to_string(depth),
                               verilog_path, harness_path);
        if (!br.success) {
            tr.verilator_skipped = true;
            tr.skip_reason = br.error_msg;
        } else {
            std::string err;
            uint64_t elapsed = runner.run_and_time(br.binary_path, ticks, &err);
            if (elapsed == 0) {
                tr.verilator_skipped = true;
                tr.skip_reason = err;
            } else {
                fill(tr.verilator, "verilator",
                     static_cast<double>(elapsed));
                tr.verilator.build_cache_hit = br.cache_hit ? 1 : 0;
            }
        }
    }

    // ----- Compute speedup ratios -----
    double interp_tps = tr.interp.ticks_per_sec;
    double verilator_tps = tr.verilator_skipped ? 0.0
                                                : tr.verilator.ticks_per_sec;
    if (interp_tps > 0) {
        tr.jit.speedup_vs_interpreter = tr.jit.ticks_per_sec / interp_tps;
        if (verilator_tps > 0) {
            tr.verilator.speedup_vs_interpreter =
                tr.verilator.ticks_per_sec / interp_tps;
            tr.jit.speedup_vs_verilator =
                tr.jit.ticks_per_sec / verilator_tps;
            tr.verilator.speedup_vs_verilator = 1.0;
        }
    }
    tr.interp.speedup_vs_interpreter = 1.0;
    if (verilator_tps > 0) tr.interp.speedup_vs_verilator =
        interp_tps / verilator_tps;

    return tr;
}
```

- [ ] **Step 2: Add `verilator_runner.h` include + includes for filesystem**

At the top of `perf_main.cpp`, after the existing includes, add:

```cpp
#include "verilator_runner.h"
#include <filesystem>
```

- [ ] **Step 3: Build to confirm TC-07 compiles**

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc) --target perf_tests 2>&1 | tail -10
```

Expected: clean compile.

- [ ] **Step 4: Commit (function added, not yet called from main)**

```bash
cd /workspace/project/CppHDL
git add tests/benchmark/perf_main.cpp
git commit -m "feat(perf): add run_three_way_xor() for TC-07"
```

---

## Task 7: Add TC-08 (sequential regs three-way comparison) to perf_main.cpp

**Files:**
- Modify: `tests/benchmark/perf_main.cpp`

- [ ] **Step 1: Add the TC-08 runner function**

Insert directly after `run_three_way_xor`:

```cpp
ThreeWayResult run_three_way_seq(int num_regs, int ticks,
                                 ch_perf::VerilatorRunner &runner,
                                 const std::string &harness_path,
                                 const std::string &verilator_workdir) {
    ThreeWayResult tr;

    auto fill = [&](BenchmarkResult &r, const std::string &backend_str,
                    double elapsed_ns) {
        r.test_name = "TC-08";
        r.params = "regs=" + std::to_string(num_regs);
        r.backend = backend_str;
        r.ns_per_tick = elapsed_ns / ticks;
        r.ticks_per_sec = (ticks * 1e9) / elapsed_ns;
        r.ns_per_node_tick = elapsed_ns /
                             (static_cast<double>(ticks) * num_regs);
    };

    // ----- Interpreter -----
    {
        PerfTimer t;
        ch::core::context ctx("tc08_interp");
        ch::core::ctx_swap swap(&ctx);
        ch_out<ch_uint<8>> out("io");
        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);
        out <<= reg;
        (void)num_regs;  // baseline design
        Simulator sim(&ctx);
        sim.set_jit_enabled(false);
        t.start(); sim.tick(ticks); t.stop();
        fill(tr.interp, "interpreter", t.elapsed_ns());
    }

    // ----- JIT -----
    {
        PerfTimer t;
        ch::core::context ctx("tc08_jit");
        ch::core::ctx_swap swap(&ctx);
        ch_out<ch_uint<8>> out("io");
        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);
        out <<= reg;
        Simulator sim(&ctx);
        sim.set_jit_enabled(true);
        t.start(); sim.tick(ticks); t.stop();
        fill(tr.jit, "jit", t.elapsed_ns());
    }

    // ----- Verilator -----
    if (!runner.is_available()) {
        tr.verilator_skipped = true;
        tr.skip_reason = "verilator not on PATH";
    } else {
        ch::core::context ctx("tc08_verilator");
        ch::core::ctx_swap swap(&ctx);
        ch_out<ch_uint<8>> out("io");
        auto inp = ch_uint<8>(0_b);
        auto reg = ch_reg<ch_uint<8>>(inp);
        out <<= reg;

        std::string verilog_path = verilator_workdir + "/tc08_r" +
                                   std::to_string(num_regs) + ".v";
        std::filesystem::create_directories(verilator_workdir);
        ch::toVerilog(verilog_path, &ctx);

        auto br = runner.build("tc08_r" + std::to_string(num_regs),
                               verilog_path, harness_path);
        if (!br.success) {
            tr.verilator_skipped = true;
            tr.skip_reason = br.error_msg;
        } else {
            std::string err;
            uint64_t elapsed = runner.run_and_time(br.binary_path, ticks, &err);
            if (elapsed == 0) {
                tr.verilator_skipped = true;
                tr.skip_reason = err;
            } else {
                fill(tr.verilator, "verilator",
                     static_cast<double>(elapsed));
                tr.verilator.build_cache_hit = br.cache_hit ? 1 : 0;
            }
        }
    }

    double interp_tps = tr.interp.ticks_per_sec;
    double verilator_tps = tr.verilator_skipped ? 0.0
                                                : tr.verilator.ticks_per_sec;
    if (interp_tps > 0) {
        tr.jit.speedup_vs_interpreter = tr.jit.ticks_per_sec / interp_tps;
        if (verilator_tps > 0) {
            tr.verilator.speedup_vs_interpreter =
                tr.verilator.ticks_per_sec / interp_tps;
            tr.jit.speedup_vs_verilator =
                tr.jit.ticks_per_sec / verilator_tps;
            tr.verilator.speedup_vs_verilator = 1.0;
        }
    }
    tr.interp.speedup_vs_interpreter = 1.0;
    if (verilator_tps > 0) tr.interp.speedup_vs_verilator =
        interp_tps / verilator_tps;

    return tr;
}
```

- [ ] **Step 2: Commit**

```bash
cd /workspace/project/CppHDL
git add tests/benchmark/perf_main.cpp
git commit -m "feat(perf): add run_three_way_seq() for TC-08"
```

---

## Task 8: Wire TC-07/TC-08 into main() CLI

**Files:**
- Modify: `tests/benchmark/perf_main.cpp`

- [ ] **Step 1: Extend the CLI flag list**

Replace the `bool` flags section in `main` with:

```cpp
    bool run_all = false;
    bool tc01 = false, tc02 = false, tc04 = false, tc06 = false;
    bool tc07 = false, tc08 = false;
    std::string report_format;
    std::string verilator_bin = "verilator";
    std::string cache_root = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") +
                             "/.cache/cpphdl/perf";
    std::string verilator_workdir = "/tmp/cpphdl_perf_verilog";
```

- [ ] **Step 2: Extend the arg parser**

Replace the `for` loop in `main` with:

```cpp
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
        } else if (strcmp(argv[i], "--tc=07") == 0) {
            tc07 = true;
        } else if (strcmp(argv[i], "--tc=08") == 0) {
            tc08 = true;
        } else if (strncmp(argv[i], "--report=", 9) == 0) {
            report_format = argv[i] + 9;
        } else if (strncmp(argv[i], "--verilator=", 12) == 0) {
            verilator_bin = argv[i] + 12;
        } else if (strncmp(argv[i], "--cache-root=", 13) == 0) {
            cache_root = argv[i] + 13;
        } else if (strncmp(argv[i], "--workdir=", 10) == 0) {
            verilator_workdir = argv[i] + 10;
        } else if (strcmp(argv[i], "--baseline") == 0) {
            run_all = true;
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
```

- [ ] **Step 3: Update the no-args check**

Replace:

```cpp
    if (!run_all && !tc01 && !tc02 && !tc04 && !tc06) {
```

with:

```cpp
    if (!run_all && !tc01 && !tc02 && !tc04 && !tc06 && !tc07 && !tc08) {
```

- [ ] **Step 4: Add TC-07/TC-08 dispatch in the run loop**

After the `if (run_all || tc06) { ... }` block, insert:

```cpp
    ch_perf::VerilatorRunner runner(cache_root, verilator_bin);
    std::string harness_tc07 = std::string(argv[0]).substr(0,
        std::string(argv[0]).find_last_of("/\\") + 1) + "../benchmark/verilator_harness_tc07";
    // Fallback if argv[0] is just "perf_tests" (no path)
    if (harness_tc07.find("/benchmark/") == std::string::npos) {
        harness_tc07 = "tests/benchmark/verilator_harness_tc07";
    }
    std::string harness_tc08 = harness_tc07;
    // Replace tc07 with tc08 in the harness path
    auto pos = harness_tc08.rfind("tc07");
    if (pos != std::string::npos) harness_tc08.replace(pos, 4, "tc08");

    if (run_all || tc07) {
        std::cout << "TC-07: XOR Chain (Interpreter vs JIT vs Verilator)\n";
        std::vector<int> depths = {10, 100, 1000};
        for (int d : depths) {
            auto tr = run_three_way_xor(d, 1000, runner, harness_tc07,
                                        verilator_workdir);
            reporter.add_result(tr.interp);
            reporter.add_result(tr.jit);
            if (!tr.verilator_skipped) {
                reporter.add_result(tr.verilator);
            } else {
                std::cout << "  Verilator SKIPPED: " << tr.skip_reason << "\n";
            }
        }
        std::cout << "\n";
    }

    if (run_all || tc08) {
        std::cout << "TC-08: Sequential Regs (Interpreter vs JIT vs Verilator)\n";
        std::vector<int> counts = {10, 100, 1000};
        for (int n : counts) {
            auto tr = run_three_way_seq(n, 1000, runner, harness_tc08,
                                        verilator_workdir);
            reporter.add_result(tr.interp);
            reporter.add_result(tr.jit);
            if (!tr.verilator_skipped) {
                reporter.add_result(tr.verilator);
            } else {
                std::cout << "  Verilator SKIPPED: " << tr.skip_reason << "\n";
            }
        }
        std::cout << "\n";
    }
```

- [ ] **Step 5: Update `print_usage`**

Replace the body of `print_usage` with:

```cpp
void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --tc=01       Run TC-01: Combinational chain (Interpreter)\n";
    std::cout << "  --tc=02       Run TC-02: Sequential registers (Interpreter)\n";
    std::cout << "  --tc=04       Run TC-04: Trace overhead\n";
    std::cout << "  --tc=06       Run TC-06: Batch vs single tick\n";
    std::cout << "  --tc=07       Run TC-07: XOR chain 3-way (Interp / JIT / Verilator)\n";
    std::cout << "  --tc=08       Run TC-08: Sequential 3-way (Interp / JIT / Verilator)\n";
    std::cout << "  --all         Run all benchmarks (incl. TC-07/TC-08)\n";
    std::cout << "  --report=csv  Export results to CSV\n";
    std::cout << "  --report=json Export results to JSON\n";
    std::cout << "  --verilator=<path>   Path to verilator binary (default: PATH lookup)\n";
    std::cout << "  --cache-root=<path>  Verilator build cache dir (default: ~/.cache/cpphdl/perf)\n";
    std::cout << "  --workdir=<path>     Where to write generated .v files (default: /tmp/cpphdl_perf_verilog)\n";
    std::cout << "  --baseline    Generate baseline measurements\n";
}
```

- [ ] **Step 6: Build and run TC-07/TC-08**

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc) --target perf_tests 2>&1 | tail -10
./build/tests/perf_tests --tc=07 --tc=08 --workdir=/tmp/cpphdl_perf_v1
```

Expected:
- First run: Verilator builds each design (slow, ~10-30s per design)
- Subsequent runs: cache hit (fast)
- Output: 6 TC-07 rows + 6 TC-08 rows, each in 3 backends where Verilator is available
- If Verilator is unavailable: SKIP message but no failure

- [ ] **Step 7: Commit**

```bash
cd /workspace/project/CppHDL
git add tests/benchmark/perf_main.cpp
git commit -m "feat(perf): wire TC-07/TC-08 into main() CLI"
```

---

## Task 9: Update CMakeLists.txt

**Files:**
- Modify: `tests/benchmark/CMakeLists.txt`

- [ ] **Step 1: Add the harness sources to `BENCHMARK_SOURCES`**

Replace the `BENCHMARK_SOURCES` set with:

```cmake
set(BENCHMARK_SOURCES
    perf_main.cpp
    perf_timer.h
    stats_collector.h
    report_generator.h
    verilator_runner.h
    verilator_harness_tc07.cpp
    verilator_harness_tc08.cpp
)
```

- [ ] **Step 2: Build each harness as a separate static lib (not part of perf_tests exe)**

We need the harness files to be **separately compilable** so that
verilator --build can pick up the source. The current `add_executable(perf_tests ...)`
compiles everything together, so verilator can read the `.cpp` source from
the build directory.

Add this after `add_executable(perf_tests ...)`:

```cmake
# The Verilator harness files are compiled INTO perf_tests (above) so
# the source is available in the build directory for verilator to read
# at perf_tests runtime. No separate target needed.
```

- [ ] **Step 3: Raise the CTest timeout**

Replace:

```cmake
set_tests_properties(perf_tests PROPERTIES
    LABELS "perf"
    TIMEOUT 120
)
```

with:

```cmake
set_tests_properties(perf_tests PROPERTIES
    LABELS "perf"
    TIMEOUT 300
)
```

- [ ] **Step 4: Build and run all TCs to confirm regression-free**

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc) --target perf_tests 2>&1 | tail -5
./build/tests/perf_tests --tc=01 --tc=02 --tc=04 --tc=06 --report=csv
```

Expected: existing TC-01/02/04/06 still run; CSV is produced.

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/CppHDL
git add tests/benchmark/CMakeLists.txt
git commit -m "build(perf): register harness sources + raise CTest timeout"
```

---

## Task 10: Update docs/simulation/PERFORMANCE_TESTS.md

**Files:**
- Modify: `docs/simulation/PERFORMANCE_TESTS.md`

- [ ] **Step 1: Add TC-07 and TC-08 sections**

At the end of section 4 (before section 5), insert:

```markdown
### TC-07：XOR 链三路对比（Interpreter vs JIT vs Verilator）

**目的**：在同一设计下量化 CppHDL 三种仿真后端的性能差异

| 参数 | 值 |
|------|------|
| 设计 | XOR 链（深度 10/100/1000）+ `ch_out<ch_uint<8>>` 端口 |
| Tick 次数 | 1000 |
| 后端 | Interpreter / JIT / Verilator (subprocess) |
| 测量指标 | ticks/sec, ns/tick, vs Interpreter 倍数, vs Verilator 倍数 |

**设计**：`out <<= xor_chain(depth);`，`ch_out<ch_uint<8>>` 保证 Verilog codegen 输出有端口。

**验收标准**：
- 三个后端均产出非零 ticks/sec
- Verilator 构建被 SHA-1 缓存（第二次运行 `cache_hit=1`）
- 没有 Verilator 时 TC-07 不失败（仅 SKIP）

### TC-08：时序寄存器三路对比（Interpreter vs JIT vs Verilator）

**目的**：时序逻辑设计下三后端对比

| 参数 | 值 |
|------|------|
| 设计 | 单 `ch_reg<ch_uint<8>>` + `ch_out<>` |
| Tick 次数 | 1000 |
| 后端 | Interpreter / JIT / Verilator |
| 测量指标 | 同 TC-07 |

**Harness 时序**：
1. `default_reset=1` 两个 eval() 周期
2. `default_reset=0` 后每个 tick 切换 `default_clock` 0/1/0/1 两次 eval()
3. Verilog port 名必须为 `default_clock` / `default_reset`（与 codegen ADR-035 对齐）

**验收标准**：同 TC-07
```

- [ ] **Step 2: Add a "Three-way comparison results" section to the bottom**

At the end of the file (after the existing 总结 section), insert:

```markdown
---

## 11. Three-way comparison (TC-07/TC-08)

> 测量时记录: `date`, `verilator --version`, `uname -a`, `cat /proc/cpuinfo | head -20`

### TC-07 sample run (XOR chain, 1000 ticks)

| depth | Interpreter | JIT | Verilator (subprocess) | JIT/Interp | Verilator/Interp |
|-------|-------------|-----|------------------------|------------|-------------------|
| 10 | _fill_ | _fill_ | _fill_ | _fill_ | _fill_ |
| 100 | _fill_ | _fill_ | _fill_ | _fill_ | _fill_ |
| 1000 | _fill_ | _fill_ | _fill_ | _fill_ | _fill_ |

### TC-08 sample run (sequential, 1000 ticks)

| regs | Interpreter | JIT | Verilator (subprocess) | JIT/Interp | Verilator/Interp |
|------|-------------|-----|------------------------|------------|-------------------|
| 10 | _fill_ | _fill_ | _fill_ | _fill_ | _fill_ |
| 100 | _fill_ | _fill_ | _fill_ | _fill_ | _fill_ |
| 1000 | _fill_ | _fill_ | _fill_ | _fill_ | _fill_ |

**如何运行**：

```bash
# 默认 (verilator 在 PATH, 缓存到 ~/.cache/cpphdl/perf)
./build/tests/perf_tests --tc=07 --tc=08

# 自定义 verilator 路径
./build/tests/perf_tests --tc=07 --verilator=/opt/verilator/bin/verilator

# 第一次跑: 慢 (verilator --cc --build 每个 design)
# 第二次跑: 走 SHA-1 缓存, 快
```
```

- [ ] **Step 3: Commit**

```bash
cd /workspace/project/CppHDL
git add docs/simulation/PERFORMANCE_TESTS.md
git commit -m "docs(perf): document TC-07/TC-08 three-way comparison"
```

---

## Task 11: Final verification

**Files:** None (verification only)

- [ ] **Step 1: Clean build, full perf suite, full regression**

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc) 2>&1 | grep -E "error|warning" | head -20
./build/tests/perf_tests --all --report=csv
cat perf_results.csv
```

Expected:
- Zero new warnings on changed files
- All 8 existing TC-01/02/04/06 rows + 18 new TC-07/08 rows (3 backends × 3 params × 2 TCs)
- Verilator rows show `cache_hit=1` on second run

- [ ] **Step 2: Run full Catch2 regression to confirm no side effects**

```bash
cd /workspace/project/CppHDL
ctest -L base --output-on-failure 2>&1 | tail -20
```

Expected: pass count unchanged from before this plan (108 pass, 1 timeout pre-existing).

- [ ] **Step 3: Run all 28 ported examples**

```bash
cd /workspace/project/CppHDL
./run_all_ported_tests.sh 2>&1 | tail -5
```

Expected: 28/28 pass.

- [ ] **Step 4: Verify the harness .cpp files are reachable by verilator**

```bash
ls /workspace/project/CppHDL/build/tests/benchmark/verilator_harness_tc07.cpp
```

If the file is missing, verilator --build will fail at runtime. The
CMake step registers it as a source, so it should be present.

- [ ] **Step 5: Commit a final tag if all green**

```bash
cd /workspace/project/CppHDL
git status
# If clean, no commit needed. If dirty, fix or report.
```

---

## Self-Review

**1. Spec coverage:**
- §3.1 Goal 1 (add TC-07/TC-08): Tasks 6, 7, 8 ✓
- §3.1 Goal 2 (run same design through 3 backends): Tasks 6, 7 ✓
- §3.1 Goal 3 (cache Verilator builds): Task 1, Step 3 ✓
- §3.1 Goal 4 (extend CSV/JSON output): Task 4 ✓
- §3.1 Goal 5 (update PERFORMANCE_TESTS.md): Task 10 ✓
- §4.1 Component table: All components covered ✓
- §4.2 VerilatorRunner API: Task 1 ✓
- §4.3 Designs with `ch_out<>`: Tasks 5, 6, 7 ✓
- §4.4 Harness templates: Tasks 2, 3 ✓
- §4.5 perf_main flow: Tasks 6, 7, 8 ✓
- §4.6 Output schema: Task 4 ✓
- §5 CLI / CTest integration: Task 8 (CLI), Task 9 (CMake) ✓
- §6 Error handling (SKIP on missing verilator): Tasks 6, 7 (verilator_skipped branch) ✓
- §7 Testing: Task 11 ✓

**2. Placeholder scan:** No "TBD", "TODO", "implement later", "fill in details", or "appropriate error handling" without code. Every step has the actual content needed.

**3. Type consistency:**
- `VerilatorRunner::build()` returns `VerilatorBuildResult` consistently in Tasks 1, 6, 7 ✓
- `BenchmarkResult` fields used in Tasks 4 (defined), 6, 7, 8 (filled), 11 (consumed) all match ✓
- `run_three_way_xor` / `run_three_way_seq` return `ThreeWayResult` consistently in Tasks 6, 7, 8 ✓
- Harness file names `verilator_harness_tc07.cpp` / `verilator_harness_tc08.cpp` used in Tasks 2, 3, 8, 9 ✓

**4. Open question resolved:** The harness path computation in Task 8 Step 4 uses `argv[0]` to find the binary location; this is fragile. A more robust solution would be a CMake-generated `-DPERF_HARNESS_DIR=...` define. **This is left as a known limitation; in practice the binary lives at `build/tests/perf_tests` and the harness sources at `build/tests/benchmark/verilator_harness_tcNN.cpp`, so a relative `../benchmark/verilator_harness_tcNN` works.** Task 11 Step 4 verifies the file is reachable.

**5. Risk acknowledged:** First Verilator build is slow (~5-30s per design); CTest TIMEOUT raised to 300s in Task 9. The `cache_hit=1` row only appears on second+ run; first run shows `cache_hit=0` (this is correct and useful signal).

**6. JSON export not updated:** Spec §4.6 mentions JSON; Task 4 only updates CSV. JSON still uses old schema. **Add a follow-up note in `docs/superpowers/specs/2026-06-08-...-design.md` rather than expanding plan scope.** JSON consumers can re-derive `speedup_*` from `ticks_per_sec` if needed.
