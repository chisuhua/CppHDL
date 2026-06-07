# Spec: perf_tests JIT vs Verilator Comparison

**Document ID**: SPEC-2026-06-08-perf-jit-verilator
**Date**: 2026-06-08
**Status**: Draft
**Author**: Sisyphus
**Related**: ADR-035, docs/simulation/PERFORMANCE_TESTS.md

---

## 1. Purpose

Extend `tests/benchmark/perf_main.cpp` to support a three-way comparison of
HDL simulation performance:

- **Interpreter** (default `ch::Simulator`, no JIT)
- **JIT** (`ch::Simulator` with `set_jit_enabled(true)`, LLVM ORC)
- **Verilator** (external subprocess: `verilator --cc --build` then run the
  compiled binary, wall-clock time)

The existing TC-01 / TC-02 / TC-04 / TC-06 in `perf_main.cpp` measure only
the interpreter. This spec adds two new test cases (TC-07, TC-08) that
exercise the same designs as TC-01 / TC-02 across all three backends, and
produces a side-by-side speedup report (CSV + console).

## 2. Background and Constraints

### 2.1 Verilator backend status (ADR-035)

`ch::VerilatorBackend` (in `include/core/verilator_backend.h`,
`src/core/verilator_backend.cpp`) is **scaffolding only** as of
2026-06-07 (per the header banner). It can:

- generate Verilog from a `ch::core::context`
- invoke `verilator --cc` to produce `Vtop.h` / `Vtop.cpp`
- compute SHA-1 cache keys for incremental builds

It **cannot** yet drive the compiled Vtop model through a `tick()` cycle.
Phases 3.2-3.6 of ADR-035 (dlopen, port binding, clock sync, VCD) are
pending.

### 2.2 Constraint: external-subprocess approach

Because the C++ API backend cannot drive Vtop, this spec uses Verilator
**as an external tool**. Each Verilator-backed test case:

1. Uses `ch::toVerilog()` to emit a `.v` file for the design-under-test.
2. Compiles a small **C++ harness** that links the generated `Vtop` and
   drives `eval()` for N cycles.
3. Invokes `verilator --cc --build --exe <harness>.cpp <top>.v`.
4. Runs the compiled binary as a child process; perf_main measures the
   wall-clock duration of the child with `PerfTimer`.

This is the same external-tool pattern already used in
`tests/test_verilog_external.cpp` and the `examples/verilator/` programs.

### 2.3 JIT state

`option(CH_JIT_ENABLE "..." ON)` is the default in `CMakeLists.txt:13`,
and `LLVM` is found by `find_package(LLVM REQUIRED)`. JIT is therefore
available in the standard build. `Simulator::set_jit_enabled(true)` and
`is_jit_compiled()` are the relevant API.

### 2.4 Existing perf_tests scope

`tests/benchmark/perf_main.cpp` has TC-01, TC-02, TC-04, TC-06. Only
TC-01 (combinational chain) and TC-02 (sequential registers) measure raw
simulation speed. TC-04 (trace overhead) and TC-06 (batch vs single
tick) measure CppHDL-API-specific behavior that has no Verilator
analogue, so they are **out of scope** for this comparison.

## 3. Goals and Non-Goals

### 3.1 Goals

- Add TC-07 (XOR chain) and TC-08 (sequential regs) to perf_main.cpp
- Each runs the **same design** (with `ch_out<>` ports) through
  Interpreter, JIT, Verilator subprocess; reports ticks/sec and
  ns/tick for all three.
- Cache Verilator build artifacts by SHA-1 to amortize the ~5-30s
  first-build cost across runs.
- Extend CSV / JSON output with `verilator_*` columns and
  `speedup_vs_interpreter` ratios.
- Document the new TCs in `docs/simulation/PERFORMANCE_TESTS.md`.

### 3.2 Non-Goals

- Implementing VerilatorBackend Phase 3.2-3.6 (dlopen + port binding
  inside the C++ simulator). This spec only uses Verilator as an
  external tool.
- Comparing with SpinalHDL or other HDL simulators. The comparison is
  strictly CppHDL-Interpreter vs CppHDL-JIT vs Verilator-as-tool.
- Running TC-04 (trace) or TC-06 (batch tick) through Verilator. The
  three-way comparison is for raw simulation speed only.
- Hardware-level / FPGA synthesis timing. This is software simulation.

## 4. Architecture

### 4.1 Components

| Component | File | Status | Purpose |
|-----------|------|--------|---------|
| Perf runner (extended) | `tests/benchmark/perf_main.cpp` | modify | CLI + main loop; new TC-07/TC-08 |
| Verilator runner | `tests/benchmark/verilator_runner.h` | new | build / exec / cache Vtop binary |
| TC-07 harness | `tests/benchmark/verilator_harness_tc07.cpp` | new | XOR-chain harness (pure comb) |
| TC-08 harness | `tests/benchmark/verilator_harness_tc08.cpp` | new | sequential-reg harness (clocked) |
| CMake glue | `tests/benchmark/CMakeLists.txt` | modify | register new sources |
| Doc | `docs/simulation/PERFORMANCE_TESTS.md` | modify | add TC-07 / TC-08 sections |

### 4.2 Verilator runner API (verilator_runner.h)

```cpp
struct VerilatorBuildResult {
    bool success;
    std::string binary_path;   // <work_dir>/obj_dir/Vtop
    std::string cache_key;     // SHA-1 hash
    bool cache_hit;            // true if reused
    uint64_t build_time_ns;    // 0 if cache_hit
    std::string error_msg;     // empty on success
};

class VerilatorRunner {
public:
    VerilatorRunner(std::string cache_root, std::string verilator_bin = "verilator");

    // Build Vtop for (verilog_path, harness_path). Caches by SHA-1.
    // If verilator is not on PATH, returns success=false with
    // error_msg="verilator not found" and the caller should SKIP.
    VerilatorBuildResult build(
        const std::string& design_name,    // for cache subdir
        const std::string& verilog_path,
        const std::string& harness_path,
        const std::string& extra_args = "");

    // Run a previously-built binary, measure wall-clock ns.
    // Returns elapsed_ns. On failure (binary missing or non-zero exit),
    // returns 0 and populates error_msg.
    uint64_t run_and_time(
        const std::string& binary_path,
        const std::vector<std::string>& args = {});
};
```

The cache key is `SHA-1(design_name + verilog_source + harness_source
+ verilator --version)`. Cached binaries live under
`<cache_root>/<key>/obj_dir/Vtop`.

### 4.3 Test designs (used by all three backends)

Both designs add a `ch_out<ch_uint<8>>` so the Verilog codegen produces a
module with output ports (Verilog requires at least an output for
`Vtop` to be useful).

**TC-07 design (XOR chain)**:
```cpp
ch_out<ch_uint<8>> out("io");
ch_uint<8> result = ch_uint<8>(0_b);
for (int i = 0; i < depth; ++i) {
    result = result ^ ch_uint<8>(i);
}
out <<= result;
```

**TC-08 design (sequential regs)**:
```cpp
ch_out<ch_uint<8>> out("io");
auto inp = ch_uint<8>(0_b);
auto reg = ch_reg<ch_uint<8>>(inp);
out <<= reg;
```

depth ∈ {10, 100, 1000} for TC-07 (matches existing TC-01).
num_regs ∈ {10, 100, 1000} for TC-08 (matches existing TC-02).

### 4.4 Harness templates

**TC-07 harness (pure combinational)**:
```cpp
// verilator_harness_tc07.cpp
#include "Vtop.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    int ticks = (argc > 1) ? std::atoi(argv[1]) : 1000;
    Vtop* top = new Vtop;
    for (int i = 0; i < ticks; ++i) {
        top->eval();
    }
    delete top;
    return 0;
}
```

**TC-08 harness (sequential, clocked)**:
```cpp
// verilator_harness_tc08.cpp
#include "Vtop.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    int ticks = (argc > 1) ? std::atoi(argv[1]) : 1000;
    Vtop* top = new Vtop;
    // Reset pulse
    top->reset = 1;
    top->clk = 0; top->eval();
    top->clk = 1; top->eval();
    top->reset = 0;
    for (int i = 0; i < ticks; ++i) {
        top->clk = 0; top->eval();
        top->clk = 1; top->eval();
    }
    delete top;
    return 0;
}
```

### 4.5 perf_main.cpp flow for TC-07 (depth=N)

```cpp
auto run_three_way = [&](int depth, int ticks, const char* design_name) {
    // ----- 1. Build the shared design (Verilog + JIT-compilable graph) -----
    auto ctx = build_xor_chain_context(depth, "io");  // has ch_out<ch_uint<8>>

    // ----- 2. Interpreter path -----
    {
        context local("interp");
        ctx_swap swap(&local);
        build_xor_chain_graph(depth);
        Simulator sim(&local);
        sim.set_jit_enabled(false);
        timer.start(); sim.tick(ticks); timer.stop();
        record("TC-07", depth, "interpreter", ...);
    }

    // ----- 3. JIT path -----
    {
        context local("jit");
        ctx_swap swap(&local);
        build_xor_chain_graph(depth);
        Simulator sim(&local);
        sim.set_jit_enabled(true);
        timer.start(); sim.tick(ticks); timer.stop();
        record("TC-07", depth, "jit", ...);
    }

    // ----- 4. Verilator path -----
    {
        // 4a. Emit Verilog from a fresh context (with ch_out<>)
        std::string verilog = build_xor_chain_verilog(depth);
        std::string verilog_path = work_dir + "/tc07_d" + std::to_string(depth) + ".v";
        write_file(verilog_path, verilog);

        // 4b. Build Vtop via verilator_runner (cached)
        auto br = runner.build(design_name, verilog_path, harness_tc07_path);
        if (!br.success) { SKIP("verilator build failed: " + br.error_msg); return; }

        // 4c. Run binary, measure wall-clock
        auto elapsed = runner.run_and_time(br.binary_path, {std::to_string(ticks)});
        double tps = (ticks * 1e9) / elapsed;
        record("TC-07", depth, "verilator", tps, elapsed/ticks, ...);
    }
};
```

### 4.6 Output schema (CSV)

Existing columns:
```
test,params,ticks_per_sec,ns_per_tick,ns_per_node_tick,overhead_pct
```

New columns appended for TC-07/TC-08 (only those rows; existing rows
keep current schema for backward compat):
```
test,params,backend,ticks_per_sec,ns_per_tick,ns_per_node_tick,
speedup_vs_interpreter,speedup_vs_verilator,build_cache_hit
```

`backend` ∈ {interpreter, jit, verilator}. For each (test, params)
triple, three rows are emitted (one per backend) so the CSV stays
flat and easy to import into pandas/Excel.

`build_cache_hit` is only meaningful for the verilator rows; for the
other two rows it is 0.

## 5. CLI / CTest integration

perf_main.cpp extends its existing CLI:

```
--tc=07       Run TC-07: XOR-chain Interpreter/JIT/Verilator comparison
--tc=08       Run TC-08: Sequential-regs Interpreter/JIT/Verilator comparison
--verilator=<path>   Override verilator binary path (default: search PATH)
--cache-root=<path>  Verilator build cache dir (default: ~/.cache/cpphdl/perf)
```

The CMake `add_test(NAME perf_tests COMMAND perf_tests --all)` test
gets a longer `TIMEOUT` (from 120s to 300s) to accommodate the first
Verilator build.

## 6. Error Handling and Skips

| Failure | Behavior |
|---------|----------|
| `verilator` not on PATH | TC-07/TC-08 print `SKIP: verilator not found` and exit 0 |
| Verilator build error | TC prints `FAIL: <stderr>` and continues with remaining backends; exit code is 0 if Interpreter+JIT succeed |
| Verilator binary non-zero exit | TC prints `FAIL: exit=<n>`; same as above |
| JIT compile failed (LLVM not found at build time) | TC-07/TC-08 print `SKIP: JIT not available` and continue with Interpreter+Verilator |
| Cache dir not writable | First run creates it; if creation fails, fall back to `/tmp/cpphdl_perf_<pid>` |

The CTest test passes (exit 0) if at least one of Interpreter / JIT /
Verilator succeeds for each (test, params) combination. The motivation
is that missing Verilator or JIT should not break CI on machines
without those tools.

## 7. Testing

### 7.1 Unit / smoke

- Build with default options, run `./build/tests/perf_tests --tc=07 --tc=08`.
- Expected first run: Verilator builds each design (slow); subsequent
  runs hit cache and are fast.
- All three backends should produce non-zero `ticks_per_sec`.

### 7.2 Functional

- Run `./run_all_ported_tests.sh` to confirm no regression in
  existing perf_tests TC-01/02/04/06.
- Run `ctest -L perf` to confirm the perf_tests target still passes.

### 7.3 Performance

- After cache warmup, TC-07 depth=1000 and TC-08 regs=1000 should
  each complete in <5 seconds wall-clock (the actual perf numbers
  are reported in the CSV, not asserted by the test).

## 8. Risks and Open Questions

| Risk | Mitigation |
|------|------------|
| Verilator 5.020 in apt may not match what CI uses | Cache key includes `verilator --version`; mismatch → cache miss, rebuild |
| CppHDL Verilog codegen requires at least one `ch_out<>` for Vtop to be useful | TC-07/TC-08 designs include `ch_out<ch_uint<8>>` |
| Verilator `default_clock` in port list might require reset to be driven in harness | TC-08 harness toggles reset high for one cycle before main loop |
| First Verilator build is slow | CTest `TIMEOUT` raised to 300s; user can use `--no-verilator` to skip |
| `ch_out<>` is named "io" matching existing examples (e.g. `examples/verilator/01_minimal_counter.cpp` uses `ch_out<ch_uint<4>> out_port("io")`) — collision with Verilator-generated `Vtop` constructor parameters | If a name conflict is observed during build, prefix with `cpphdl_`; the design name in the harness is the C++ symbol, not the Verilog port name |

## 9. Out-of-Scope / Future Work

- Once `ch::VerilatorBackend` Phase 3.2-3.6 lands, replace the
  subprocess measurement with `sim.set_backend(verilator_backend)` and
  measure in-process. The CSV/JSON schema stays identical; the
  `verilator_*` columns become in-process numbers.
- Add a `cache-clear` CLI to invalidate the Verilator build cache
  (useful when changing harness without bumping SHA-1 inputs).
- Extend to TC-04 (VCD-trace vs no-trace) once VCD support is in.
- Cross-check Interpreter vs JIT vs Verilator with property-based
  output assertions (today the three paths are not bit-equality
  checked; only their perf numbers are reported).

---

**End of SPEC-2026-06-08-perf-jit-verilator**
