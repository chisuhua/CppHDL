## ADDED Requirements

### Requirement: perf_main 接收 CMake 自动注入的 verilator 二进制路径

When CppHDL is built with `BUILD_VERILATOR=ON`, the CMake configuration **MUST** automatically inject the Verilator binary path into the `perf_main` / `perf_tests` invocation. Specifically: the `add_test(NAME perf_three_way ...)` and `add_test(NAME perf_tests ...)` commands in `tests/benchmark/CMakeLists.txt` **MUST** append `--verilator=${CPPHDL_VERILATOR_WRAPPER}` to the `perf_tests` command line, AND **MUST** set the `VERILATOR_ROOT` environment variable to `${CPPHDL_VERILATOR_DIR}` via `set_tests_properties(... ENVIRONMENT ...)`. When `BUILD_VERILATOR=OFF`, the CMake configuration **MUST NOT** inject `--verilator=` (allowing `perf_main`'s default PATH lookup to fail naturally and produce `status=UNSUPPORTED` rows).

This requirement eliminates the dependency on `verilator` being discoverable via `$PATH` at ctest runtime, which is a fragile assumption for CI environments where Verilator is built as a CMake `ExternalProject_Add` into `build/verilator-install/bin/`.

#### Scenario: BUILD_VERILATOR=ON 注入 verilator 路径

- **WHEN** `cmake -B build -DBUILD_VERILATOR=ON ... && cmake --build build` succeeds
- **THEN** `${CPPHDL_VERILATOR_WRAPPER}` is set to `${CMAKE_BINARY_DIR}/verilator-install/bin/verilator`
- **AND** `tests/benchmark/perf_three_way` ctest invocation includes `--verilator=${CPPHDL_VERILATOR_WRAPPER}`
- **AND** the `VERILATOR_ROOT` environment variable for the test is `${CPPHDL_VERILATOR_DIR}`

#### Scenario: BUILD_VERILATOR=OFF 不注入 verilator 路径

- **WHEN** `cmake -B build -DBUILD_VERILATOR=OFF ...` is invoked
- **THEN** `${CPPHDL_VERILATOR_WRAPPER}` is empty
- **AND** `tests/benchmark/perf_three_way` ctest invocation does NOT include `--verilator=`
- **AND** `perf_main.cpp`'s default `verilator_bin = "verilator"` falls back to `$PATH` lookup
- **AND** if `verilator` is not on `$PATH`, all verilator backend rows SHALL have `status=UNSUPPORTED` (existing K2 behavior preserved)

#### Scenario: verilator 后端不再全部 UNSUPPORTED（默认 PATH 失败的场景）

- **WHEN** `BUILD_VERILATOR=ON` and `ctest -R perf_three_way` is run
- **THEN** at least one verilator backend row in the resulting `perf_results.json` has `status=PASS` (i.e., the verilator binary was successfully invoked via the injected path)
- **AND** the verilator rows that still have `status=SKIPPED` are only those with documented harness/width gaps (TC-09 / TC-11 per `perf-report-todo.md` W5)

### Requirement: perf_three_way ctest 入口存在且仅跑三路 perf 对比

The CTest configuration **MUST** register a `perf_three_way` test target distinct from the existing `perf_tests` target. The `perf_three_way` target **MUST** be labeled `LABELS "perf;verilator"` to allow filtering via `ctest -L verilator`. The `perf_three_way` target **MUST** invoke `perf_tests --all` with the `--verilator=` injection (per the prior requirement), **MUST** be configured with `TIMEOUT 240` (240 seconds, providing ≥1.5× safety margin over the measured ~150s runtime including the first-build verilator cache cold path), and **MUST** set `WORKING_DIRECTORY` to the project source root (so `perf_main.cpp` finds `tests/benchmark/verilator_harness_tc07.cpp` etc. via relative path).

This provides a fast (~150s) CI gate for Verilator integration health, complementing the full `perf_tests --all` invocation (1800s) which also exercises legacy in-process benchmarks (TC-01/02/04/06) and ch_uint<256> / ch_uint<32> arith chains (TC-09/11).

#### Scenario: perf_three_way 在 ctest 列表中可发现

- **WHEN** `ctest -N` is run in the build directory
- **THEN** the output contains a `perf_three_way` test entry
- **AND** the entry's `LABELS` field includes `verilator`

#### Scenario: perf_three_way 通过 `ctest -L verilator` 单独运行

- **WHEN** `ctest -L verilator --output-on-failure` is run
- **THEN** only `perf_three_way` is executed (not `perf_tests`, not other tests)
- **AND** the test completes within the 240s timeout
- **AND** `perf_results.json` is regenerated under the project root

#### Scenario: TIMEOUT 240s 提供实测余量

- **WHEN** the `perf_three_way` test is invoked on a developer workstation
- **THEN** the wall-clock runtime SHALL be ≤ 150s (1.6× safety margin over the 240s timeout)
- **AND** the test invocation includes both subprocess-isolated TCs (TC-07, TC-08) and in-process TCs (TC-01/02/04/06/10/11)
- **NOTE**: The 150s measurement includes the first invocation's verilator build cache cold path (~60s for TC-07 + TC-08 harness compile). Subsequent invocations with warm cache complete in ~90s.

#### Scenario: verilator 构建缓存跨多次 perf_three_way 复用

- **WHEN** `ctest -R perf_three_way` is run twice in succession
- **THEN** the second invocation's verilator backend uses the SHA-1-cached `obj_dir/Vtop` from the first invocation (per `tests/benchmark/verilator_runner.h` `cache_dir_for_key` mechanism)
- **AND** the second invocation's wall-clock is reduced by ~60s (no verilator cold-build)
