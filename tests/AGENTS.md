#TN|# tests/AGENTS.md - Test Subsystem Knowledge Base

Parent: [AGENTS.md](../AGENTS.md)

## OVERVIEW

Catch2 v3.7.0 unit tests validating AST, Core, Simulator, CodeGen. 109 test files: 79 base + 30 chlib.

## STRUCTURE

```
tests/
├── test_*.cpp          # 79 base tests (core functionality)
├── chlib/              # 30 chlib component tests
│   ├── test_assert.cpp
│   ├── test_if_stmt.cpp
│   └── test_simulator_trace.cpp
├── catch_amalgamated.cpp   # Catch2 single-header
└── CMakeLists.txt      # Custom add_catch_test() function
```

## KEY FILES

| Category | Files | Purpose |
|----------|-------|---------|
| Core types | test_reg.cpp, test_basic.cpp | Register, width traits |
| Operators | test_operator.cpp, test_operator_advanced.cpp | Arithmetic, bitwise, comparison |
| Bundles | test_bundle.cpp, test_bundle_advanced.cpp | Bundle construction, connections |
| Memory | test_mem.cpp, test_memory_optimization.cpp | Memory operations |
| Streams | test_ch_stream.cpp, test_stream_*.cpp | Stream operators |
| IO | test_simple_io.cpp, test_io_operations.cpp | IO protocols |
| chlib | test_arithmetic.cpp, test_logic.cpp, test_assert.cpp, test_if_stmt.cpp, test_simulator_trace.cpp | Component library (test_assert/test_if_stmt/test_simulator_trace are newly added) |

## CONVENTIONS

- **Include order**: catch_amalgamated.hpp → core headers → ast headers
- **Context pattern**: Always create ch::core::context + ctx_swap for register tests
- **Tags**: `[category][subcategory][feature]` e.g., `[reg][width][basic]`
- **Static tests**: Use STATIC_REQUIRE for compile-time checks
- **Test naming**: test_<feature>.cpp, target = filename without .cpp
- **CMake**: add_catch_test() auto-appends catch_amalgamated.cpp

## ANTI-PATTERNS

- **Empty tests**: Never leave TEST_CASE empty. Use SKIP() or remove.
- **Missing context**: Register operations require ctx_swap or segfault occurs.
- **Hardcoded paths**: Use relative includes from include/ directory.
- **No tags**: Every TEST_CASE needs tags for test selection via ctest -L.
- **Forgotten chlib**: New component tests go in tests/chlib/, tagged [chlib].

## COMMANDS

```bash
# Run base tests only
ctest -L base

# Run chlib tests only
ctest -L chlib

# Run specific tag
ctest -L reg

# Run single test
./build/tests/test_reg
```

## RELATED

- tests/chlib/ : Component library tests
- Catch2 tags: `[basic]`, `[reg]`, `[bundle]`, `[stream]`, `[memory]`, `[io]`, `[chlib]`, `[perf]`

## PERF MEASUREMENT ISOLATION (F2, 2026-06-19)

`tests/benchmark/perf_main.cpp` runs TC-07/08/09 in **separate child processes** (via `tests/benchmark/subprocess_runner.h::run_perf_subprocess`) to avoid the K1 ORC JIT cross-DUT state pollution documented in `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`. Each TC gets a fresh address space; the parent reads the child's `perf_results.json` and merges into the parent's reporter.

Conventions for perf benchmarks:
- **`--direct` flag**: when passed, `perf_main` runs the requested TC **in-process** (no subprocess spawn). Use for: (a) standalone `--tc=NN` invocations, (b) the parent always passes `--direct` when invoking children. Without `--direct`, the child would itself spawn another child (infinite recursion).
- **Subprocess inherits the parent's --workdir** as a per-TC temp dir, so the child's `perf_results.json` is captured in isolation, not written to the project root.
- **No cross-DUT state**: each TC starts with a clean LLVM ORC state. Do not write tests that depend on shared `JitCompiler` state across multiple TC calls.

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**. For tests:
- **No empty TEST_CASE**: use `SKIP()` or delete entirely
- **New feature → new TEST_CASE**: every public API must have test coverage
- **Catch2 tags**: `[category][subcategory]` for `ctest -L` filtering
- **No test dependencies**: each TEST_CASE is independent with its own context
- **Perf subprocess tests** (`tests/benchmark/test_perf_subprocess_*.cpp`): expect ~15-20 minute total runtime because the test invokes `perf_tests --all` (which spawns 9 child processes for TC-07/08/09 × {10,100,1000} plus TC-01/02/04/06/10/11 in-process). Wall-clock is dominated by TC-09 (`ch_uint<32>` arith chain, depth=1000) and TC-11 (`ch_uint<256>` wide reg chain, regs=1000). Set `TIMEOUT 1800` (30 min) in CMake — original TIMEOUT 300 was insufficient and caused false failures.
