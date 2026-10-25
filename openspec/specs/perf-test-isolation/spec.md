# perf-test-isolation Specification

## Purpose

规定 perf 基准测量的**可复现性与防失明**：`perf_main` 把每个 TC 放进独立子进程
（避免 K1 记录的 ORC JIT 跨 DUT 状态污染），子进程超时 / 清理 / 失败必须优雅
处理，verilator 二进制与 harness 路径由 CMake 注入（父进程 absolute-ize 后再
分发），per-backend 阈值按各自噪声分布设定。

另规定 **`perf_baseline.json` 重生成的守恒**：`perf_regression` 的判据是
`if (base <= 0.0) continue;` ⇒ baseline 里 `median_us == 0` 的行会**永久退出
比较集合**（该行的未来真回归不可见）⇒ 重生成 MUST 满足行集守恒、锚点守恒
（含「恢复」标记只能是**取值**逐字相等）与「变松须披露（含数值）」；
判定阈值 MUST 从 `perf_regression.cpp` 的常量读出（不得用脚本内硬编码副本），
权威源不可解析时 **fail-closed**。

## Requirements

### Requirement: perf_main runs each TC in a separate child process

When `perf_main` is invoked with `--all` (or any combination of `--tc=07/08/09`), the `perf_main` executable MUST spawn a separate child process for each (test_name, params) pair under measurement, such that no two TC measurements share a process address space. Each child process MUST be a fresh invocation of the `perf_tests` executable with the `--tc=NN` flag set to the corresponding TC identifier. The parent process MUST wait for the child to complete, capture its stdout JSON output, and merge the results into a single `perf_results.json` via the existing `report_generator.h` `add_result()` API.

#### Scenario: --all mode runs 9 child processes

- **WHEN** `perf_main --all` is invoked
- **THEN** the parent process spawns 9 separate child processes (TC-07 depth=10/100/1000, TC-08 regs=10/100/1000, TC-09 depth=10/100/1000)
- **AND** each child process executes in its own fresh process address space (no shared LLVM ORC JIT state)
- **AND** the parent process collects the JSON output from each child and merges into a single `perf_results.json`

#### Scenario: --tc=NN mode runs a single child process

- **WHEN** `perf_main --tc=08` is invoked
- **THEN** the parent process spawns 1 child process (`perf_tests --tc=08`)
- **AND** the child process executes the TC-08 measurement independently
- **AND** the parent process collects the JSON output and writes `perf_results.json`

### Requirement: subprocess isolation eliminates cross-DUT ORC state pollution

The subprocess isolation mechanism MUST guarantee that two consecutive TC measurements (e.g., TC-07 followed by TC-08) executed via `perf_main --all` produce performance numbers equivalent to running each TC standalone in a fresh process (e.g., `perf_tests --tc=08` alone). The measured JIT `sim_us` for TC-08 in `--all` mode MUST be no more than 1.5× the measured JIT `sim_us` for TC-08 in standalone `--tc=08` mode. This property MUST hold across at least 3 consecutive runs to control for measurement noise.

#### Scenario: TC-08 JIT in --all mode matches standalone performance

- **WHEN** `perf_main --all` is run and produces a TC-08 JIT `sim_us` value
- **THEN** the value is no more than 1.5× the TC-08 JIT `sim_us` value produced by running `perf_tests --tc=08` alone in a fresh process
- **AND** this property holds for all 3 sizes (regs=10, regs=100, regs=1000)
- **AND** the test `test_perf_subprocess_isolation` Catch2 test asserts this property

#### Scenario: K1 pollution is no longer observable

- **WHEN** the K1 ORC JIT cross-DUT state pollution (documented in `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`) is examined
- **THEN** the pollution signature (TC-08 JIT `sim_us` increasing by 100×+ when run after TC-07 in the same process) is no longer observable
- **AND** `perf_main --all` TC-08 JIT measurements are equivalent to standalone TC-08 JIT measurements within the 1.5× tolerance

### Requirement: subprocess_runner handles timeouts and failures gracefully

The subprocess invocation mechanism (implemented in `tests/benchmark/subprocess_runner.h`) MUST handle subprocess failures and timeouts without crashing the parent process. The parent process MUST impose a wall-clock timeout (180 seconds recommended per TC) on each child process. If a child process exceeds the timeout, the parent MUST send `SIGTERM` to the child and record the corresponding TC as `SKIPPED` with reason `"subprocess timeout"`. If a child process exits with a non-zero status, the parent MUST record the TC as `SKIPPED` with reason `"subprocess failed (exit code N)"` and continue processing subsequent TCs.

#### Scenario: subprocess timeout does not crash the parent

- **WHEN** a child process exceeds the 180-second timeout
- **THEN** the parent sends `SIGTERM` to the child
- **AND** the parent records the TC as `SKIPPED` with reason `"subprocess timeout"`
- **AND** the parent continues to process subsequent TCs

#### Scenario: subprocess non-zero exit does not crash the parent

- **WHEN** a child process exits with non-zero status (e.g., LLVM backend segfault)
- **THEN** the parent captures the exit code
- **AND** the parent records the TC as `SKIPPED` with reason `"subprocess failed (exit code N)"`
- **AND** the parent continues to process subsequent TCs

### Requirement: subprocess_runner cleans up temporary files

The subprocess invocation mechanism MUST clean up any temporary files (e.g., child stdout JSON dumps) it creates during the run, regardless of whether the child completes successfully, times out, or fails. Temporary file names MUST be unique per child process (e.g., include PID + timestamp) to avoid conflicts when multiple children run concurrently or sequentially in the same parent.

#### Scenario: successful child cleans up its temp file

- **WHEN** a child process completes successfully and the parent reads its JSON output
- **THEN** the parent removes the temporary file before processing the next child

#### Scenario: timed-out child cleans up its temp file

- **WHEN** a child process times out and is sent `SIGTERM`
- **THEN** the parent removes the temporary file associated with that child

### Requirement: perf_regression threshold restored to 1.6x after subprocess isolation

After the subprocess isolation mechanism is in place, the `perf_regression` test threshold for the JIT backend MUST be restored from the temporary 4.0× value (set by F1 on 2026-06-19 to absorb the K1 pollution) back to the original 1.6× value. The temporary comment block in `tests/benchmark/perf_regression.cpp:139-156` documenting the K1 pollution MUST be removed. The `perf_baseline.json` MUST be regenerated in the subprocess-isolated environment to reflect the new clean-state baseline.

#### Scenario: perf_regression passes with 1.6x JIT threshold

- **WHEN** the subprocess isolation mechanism is enabled and `perf_regression` is run
- **THEN** the JIT threshold check uses 1.6× (not 4.0×)
- **AND** all rows in `perf_results.json` are within 1.6× of the regenerated `perf_baseline.json`
- **AND** `perf_regression` exits with status 0

#### Scenario: perf_baseline.json reflects subprocess-isolated measurements

- **WHEN** `perf_baseline.json` is regenerated after the subprocess isolation mechanism is in place
- **THEN** the baseline contains measurements produced by `perf_main --all` running each TC in a separate child process
- **AND** the baseline does NOT contain measurements produced by the in-process sequential execution model
- **AND** the baseline's `note` field is updated to document the new measurement methodology

### Requirement: perf_main CTEST TIMEOUT accommodates wall-clock with safety margin

The CTEST `TIMEOUT` property for any CTest that invokes `perf_tests --all` or `perf_tests --tc=NN` MUST be set to at least `1.5 × measured_wall_clock_seconds + 300` seconds, where `measured_wall_clock_seconds` is the wall-clock runtime of the test on the reference developer workstation (per the most recent `tests/AGENTS.md` perf subprocess test documentation). Any commit that changes `perf_main.cpp` runtime characteristics (e.g., adds new TC, changes subprocess timeout, enables Verilator build, removes in-process TCs from subprocess dispatch) MUST include a CTEST TIMEOUT re-evaluation in the same commit, AND the test's wall-clock MUST be re-measured on the reference workstation and documented in the test's CMake `set_tests_properties()` comment block.

#### Scenario: TIMEOUT covers slowest measured wall-clock

- **WHEN** a developer workstation measures `perf_tests --all` at `t_meas` seconds (with three consecutive runs averaged; outliers beyond 2× median discarded)
- **THEN** the CTEST `TIMEOUT` for that test SHALL be ≥ `ceil(1.5 × t_meas + 300)` seconds
- **AND** the CMake comment block (3-5 lines above `set_tests_properties(... TIMEOUT ...)`) SHALL record `t_meas`, the measurement date, and the F2/methodology context (e.g., "F2 subprocess-isolated, BUILD_VERILATOR=OFF")

#### Scenario: TIMEOUT re-evaluated on perf_main change

- **WHEN** a commit to `tests/benchmark/perf_main.cpp` or `tests/benchmark/subprocess_runner.h` adds new TC, changes subprocess timeout (`run_perf_subprocess(..., int timeout_sec)`), or otherwise alters wall-clock runtime by ≥10%
- **THEN** that commit SHALL also update the affected CTest's `TIMEOUT` property
- **AND** the commit message SHALL include a "perf TIMEOUT re-evaluated: <old> → <new> (measured <t_meas>s)" footer
- **AND** the commit SHALL re-measure wall-clock on the reference workstation before pushing

#### Scenario: TC-09 subprocess timeout known limitation

- **WHEN** `ch_uint<32>` arithmetic chain TC-09 (`perf_main.cpp:744-759`, `Tc09ArithChain`) is invoked with `depth=1000` via subprocess (per F2 mechanism)
- **THEN** the subprocess wall-clock is ~165s (interpreter, 1000 ticks × 15 iter) + ~5-50s (JIT compile 2000 nodes) + ~5s (build)
- **AND** the `subprocess_runner.h:168` default `timeout_sec = 180` MAY be insufficient for this TC
- **AND** TC-09 `depth=1000` rows in `perf_results.json` MAY have `status="SKIPPED"` and `median_us=0` due to subprocess timeout
- **AND** the `tests/benchmark/perf_baseline.json` `note` field SHALL document this TC-09 SKIPPED status with the root cause (180s subprocess timeout insufficient)
- **AND** this limitation is acceptable as long as both baseline AND current `perf_results.json` have TC-09 rows with `median_us=0` (so `perf_regression` skips the comparison — no false-positive)

### Requirement: perf_baseline.json regenerated on measurement methodology change

`tests/benchmark/perf_baseline.json` MUST be regenerated whenever a commit changes `perf_main`'s measurement methodology. "Measurement methodology change" is defined as: any commit that (a) modifies `tests/benchmark/perf_main.cpp` to alter which TCs run in-process vs subprocess, (b) modifies `tests/benchmark/subprocess_runner.h` timeout / capture mechanism, (c) modifies `tests/benchmark/verilator_runner.h` Verilator dispatch, (d) enables/disables Verilator build (`BUILD_VERILATOR=ON/OFF`), or (e) modifies `tests/benchmark/perf_regression.cpp` per-backend threshold. The regenerated baseline MUST use `git_sha` matching the commit hash and MUST update the `note` field to document the methodology and date. After regeneration, `ctest -R "^perf_regression$"` MUST pass with 0 detected regressions.

#### Scenario: Baseline regenerated in same commit as methodology change

- **WHEN** a commit changes `perf_main.cpp` or related measurement machinery (subprocess / Verilator dispatch / regression thresholds)
- **THEN** that commit SHALL also regenerate `tests/benchmark/perf_baseline.json` by copying the current `perf_results.json` (post-commit, post-run) into the baseline
- **AND** the baseline's `git_sha` SHALL match the commit's `git_sha`
- **AND** the baseline's `note` field SHALL be updated to include the methodology change description (e.g., "F2 subprocess isolation baseline" / "BUILD_VERILATOR=ON" / "JIT threshold restored to 1.6x")
- **AND** the commit message SHALL include a "baseline regenerated: git_sha=<hash> timestamp=<iso>" footer

#### Scenario: Stale baseline detected by spec validation

- **WHEN** a commit changes measurement methodology but does NOT regenerate baseline (i.e., baseline's `git_sha` lags behind code HEAD by >1 commit on `tests/benchmark/perf_main.cpp` or related files)
- **THEN** the comparison between `perf_results.json` (new methodology) and `perf_baseline.json` (old methodology) will show ratio outliers (e.g., 50-100×) for affected TCs (especially TC-08 JIT in pre/post-F2 transition)
- **AND** `perf_regression` will report those outliers as "No regressions" only because current < baseline (lower is better for speed) — the test loses its true-positive regression detection capability
- **AND** this state SHALL be detected by CI / code review / spec validation as a methodology mismatch
- **AND** the commit SHALL be rejected or require baseline regeneration as a follow-up before archive

#### Scenario: perf_regression passes against current baseline

- **WHEN** `tests/benchmark/perf_baseline.json` has been regenerated in the most recent methodology change
- **AND** `perf_results.json` is produced by `perf_tests --all` on the same commit's `git_sha`
- **THEN** `ctest -R "^perf_regression$"` SHALL pass with 0 detected regressions (compared rows / threshold breaches = 0)
- **AND** future commits that introduce a real JIT regression (>1.6× for JIT backend, >2.5× for interpreter, >1.3× for verilator) will be detected by `perf_regression` (true-positive failure)

### Requirement: perf_main 接收 CMake 自动注入的 verilator 二进制路径

When CppHDL is built with `BUILD_VERILATOR=ON`, the CMake configuration **MUST** automatically inject the Verilator binary path into the `perf_main` / `perf_tests` invocation. Specifically: the `add_test(NAME perf_three_way ...)` command in `tests/benchmark/CMakeLists.txt` **MUST** append `--verilator=${CPPHDL_VERILATOR_WRAPPER}` to the `perf_tests` command line when `CPPHDL_VERILATOR_WRAPPER` is non-empty. When `BUILD_VERILATOR=OFF`, the CMake configuration **MUST NOT** inject `--verilator=` (allowing `perf_main`'s default PATH lookup to fail naturally and produce `status=UNSUPPORTED` rows).

This requirement eliminates the dependency on `verilator` being discoverable via `$PATH` at ctest runtime, which is a fragile assumption for CI environments where Verilator is built as a CMake `ExternalProject_Add` into `build/verilator-install/bin/`.

Note: The legacy `perf_tests` ctest entry already injects `VERILATOR_ROOT=${CPPHDL_VERILATOR_DIR}` via `set_tests_properties(... ENVIRONMENT ...)` as belt-and-suspenders. The new `perf_three_way` entry **does not** need this injection because the upstream Verilator Perl wrapper self-sets `VERILATOR_ROOT` when unset (`third_party/verilator/bin/verilator:101`); explicit injection in `perf_three_way` would risk triggering the wrapper's consistency-check `%Error` if `${CPPHDL_VERILATOR_DIR}` does not match the wrapper's `realpath($RealBin/..)`.

#### Scenario: BUILD_VERILATOR=ON 注入 verilator 路径

- **WHEN** `cmake -B build -DBUILD_VERILATOR=ON ... && cmake --build build` succeeds
- **THEN** `${CPPHDL_VERILATOR_WRAPPER}` is set to `${CMAKE_BINARY_DIR}/verilator-install/bin/verilator`
- **AND** `tests/benchmark/perf_three_way` ctest invocation includes `--verilator=${CPPHDL_VERILATOR_WRAPPER}`

#### Scenario: BUILD_VERILATOR=OFF 不注入 verilator 路径

- **WHEN** `cmake -B build -DBUILD_VERILATOR=OFF ...` is invoked
- **THEN** `${CPPHDL_VERILATOR_WRAPPER}` is empty
- **AND** `tests/benchmark/perf_three_way` ctest invocation does NOT include `--verilator=`
- **AND** `perf_main.cpp`'s default `verilator_bin = "verilator"` falls back to `$PATH` lookup
- **AND** if `verilator` is not on `$PATH`, all verilator backend rows SHALL have `status=UNSUPPORTED` (existing K2 behavior preserved)

#### Scenario: verilator 后端不再全部 UNSUPPORTED（默认 PATH 失败的场景）

- **WHEN** `BUILD_VERILATOR=ON` and `ctest -R perf_three_way` is run
- **THEN** at least one verilator backend row in the resulting `perf_results.json` has `status=PASS` (i.e., the verilator binary was successfully invoked via the injected path)
- **AND** the verilator rows that still have `status=SKIPPED` are only those with documented harness/width gaps (TC-09 / TC-11 per `perf-report-todo.md` W5), which are out of scope for the `perf_three_way` subset

### Requirement: perf_main 接收 CMake 注入的 harness 绝对路径

When CppHDL is built, the CMake configuration **MUST** inject an absolute harness directory path into the `perf_tests` target via `target_compile_definitions(perf_tests PRIVATE CPPHDL_PERF_HARNESS_DIR="...")`. The macro value **MUST** resolve to the build-tree directory where `configure_file()` copies the `verilator_harness_tc{07,08}.cpp` sources — i.e. `${CMAKE_CURRENT_BINARY_DIR}` of `tests/benchmark/CMakeLists.txt`. The macro **MUST** be defined when `perf_tests` is built via cmake; if a hand-built binary compiles without the cmake reconfigure path, `perf_main.cpp` **MUST** fall back to a relative string `"build/tests/benchmark"` so the legacy relative form still resolves under the standard project-root CWD.

Rationale: `subprocess_runner.h` chdirs the child process to a per-TC temp directory before fork() (`subprocess_runner.h:186`). Any relative path the child receives — harness_path, cache_root, workdir — then resolves against the wrong CWD. Without an absolute harness directory, `std::filesystem::exists(harness_path)` returns false and `run_three_way()` records a `status=SKIPPED` row with reason `"verilog or harness file missing"`, masking which file is actually missing. The fix unifies both sites: (a) harness_path is absolute at compile time, (b) cache_root / workdir are absolute-ized at `main()` start against the parent's CWD (which ctest sets to `${CMAKE_SOURCE_DIR}` via `WORKING_DIRECTORY`).

#### Scenario: harness 路径解析独立于子进程 cwd

- **WHEN** `ctest -R perf_three_way` spawns 6 child perf_tests processes (TC-07/08 × {10,100,1000}), each chdir'd into a unique temp dir
- **THEN** every child's `std::filesystem::exists(harness_path)` returns true for the configured absolute path
- **AND** the corresponding verilator backend rows for TC-07 / TC-08 SHALL have `status=PASS` with non-zero `median_us`
- **AND** verilator backend rows for TC-09 / TC-11 (no harness file exists) SHALL have `status=SKIPPED` with `skip_reason="verilator harness not implemented: <absolute harness path>"`

#### Scenario: 缺失文件分支给出独立 reason

- **WHEN** `run_three_way()` cannot find either `verilog_path` or `harness_path`
- **THEN** the `skip_reason` SHALL distinguish the two cases:
  - missing verilog → `"verilog file missing: <verilog_path>"`
  - missing harness → `"verilator harness not implemented: <harness_path>"`
- **AND** the legacy merged reason `"verilog or harness file missing"` SHALL NOT appear in any row

#### Scenario: 手工编译无 cmake 注入仍能工作

- **WHEN** a developer hand-compiles `perf_main.cpp` without re-running cmake configure (so `CPPHDL_PERF_HARNESS_DIR` is undefined)
- **THEN** the `#ifndef CPPHDL_PERF_HARNESS_DIR` fallback in `perf_main.cpp` provides `"build/tests/benchmark"`
- **AND** the binary still resolves harness paths correctly when run from the project root CWD

### Requirement: perf_main 接受相对 cache_root / workdir 并在 main() 入口 absolute-ize

The `perf_main.cpp` command-line defaults for `--cache-root=` (`build/perf_cache`) and `--workdir=` (`build/perf_work`) **MAY** remain relative for ergonomic CLI use. The `main()` entry **MUST** call `std::filesystem::absolute()` on both before any subprocess dispatch so the resolved absolute paths survive the child process's `chdir` to a per-TC temp dir. The absolute-ization **MUST** anchor against the parent's CWD (which ctest sets to `${CMAKE_SOURCE_DIR}`); if `absolute()` fails for any reason, `main()` **MUST** exit 1 with a diagnostic on stderr.

Rationale: the subprocess runner writes `verilator_runner.h::cache_dir_for_key()` results into `cache_root`. If `cache_root` is relative and the child has been chdir'd to a temp dir, the cache is written under the temp dir, which `remove_all()` deletes on subprocess exit. Subsequent subprocess runs cannot find the cached `libVtop.so` and recompile from scratch every time — risking TIMEOUT 360 on `perf_three_way` and defeating the SHA-1 cache invariant documented in `verilator_runner.h:6`.

#### Scenario: 子进程 verilator cache 跨 subprocess 命中

- **WHEN** `ctest -R perf_three_way` runs TC-07 depth=100 after TC-07 depth=10 has already built the cache
- **THEN** the TC-07 depth=100 subprocess finds the cache hit and `verilator_runner.h::build()` returns with `cache_hit=true` and `build_time_ns` near zero
- **AND** the resulting row's `build_us` is 0 (cache hit), not the cold-compile cost

### Requirement: perf_three_way ctest 入口存在且仅跑 TC-07/08 三路 perf 对比

The CTest configuration **MUST** register a `perf_three_way` test target distinct from the existing `perf_tests` target. The `perf_three_way` target **MUST** invoke `perf_tests --tc=07 --tc=08` (two separate `--tc=` flags, NOT `--all` and NOT the comma syntax `--tc=07,08` which `perf_main` does not parse) with the `--verilator=` injection per the prior requirement, **MUST** be labeled `LABELS "perf;verilator"` to allow filtering via `ctest -L verilator`, **MUST** be configured with `TIMEOUT 360` (360 seconds, providing ≥1.24× safety margin over the measured 291s TC-07/08 subset runtime including the first-build verilator cache cold path), and **MUST** set `WORKING_DIRECTORY` to the project source root.

This provides a fast (~150s) CI gate for Verilator integration health, complementing the full `perf_tests --all` invocation (1800s) which also exercises legacy in-process benchmarks (TC-01/02/04/06) and ch_uint<256> / ch_uint<32> arith chains (TC-09/11) which lack verilator harnesses.

#### Scenario: perf_three_way 在 ctest 列表中可发现

- **WHEN** `ctest -N` is run in the build directory
- **THEN** the output contains a `perf_three_way` test entry
- **AND** the entry's `LABELS` field includes `verilator`

#### Scenario: perf_three_way 通过 `ctest -L verilator` 单独运行

- **WHEN** `ctest -L verilator --output-on-failure` is run
- **THEN** only `perf_three_way` is executed (not `perf_tests`, not other tests)
- **AND** the test completes within the 360s timeout
- **AND** `perf_results.json` is regenerated under the project root

#### Scenario: TIMEOUT 360s 提供实测余量

- **WHEN** the `perf_three_way` test is invoked on a developer workstation
- **THEN** the wall-clock runtime SHALL be ≤ 291s (1.24× safety margin over the 360s timeout)
- **AND** the test invocation includes only TC-07 and TC-08 subprocess-isolated TCs (each with sizes 10, 100, 1000), NOT TC-01/02/04/06/10/11
- **NOTE**: The 291s measurement includes the first invocation's verilator build cache cold path (~60s for TC-07 + TC-08 harness compile). Subsequent invocations with warm cache complete in ~230s.

#### Scenario: verilator 构建缓存跨多次 perf_three_way 复用

- **WHEN** `ctest -R perf_three_way` is run twice in succession
- **THEN** the second invocation's verilator backend uses the SHA-1-cached `obj_dir/Vtop` from the first invocation (per `tests/benchmark/verilator_runner.h` `cache_dir_for_key` mechanism)
- **AND** the second invocation's wall-clock is reduced by ~60s (no verilator cold-build)

### Requirement: perf baseline 重生成 MUST 守恒（行集 / 锚点 / 变松披露）

- **WHEN** `tests/benchmark/perf_baseline.json` 被重生成
- **THEN** 新 baseline 的 `(test_name, params, backend)` 行集 MUST 等于基准的行集
- **AND** 基准里 `status=PASS` 且 `median_us > 0` 的锚点 MUST 仍存在**且仍 `> 0`**
  （`perf_regression` 的判据是 `if (base <= 0.0) continue;` ⇒ 锚点降级为 0
  = 该行的未来回归**永久不可见**）
- **AND** 比基准更慢的行 MUST 在 baseline 的 `note` 字段里被**点名**
  （披露的形式、数值、阈值来源、逃逸口禁令见下方 Scenario）
- **AND** 该守恒检查 MUST 由 `scripts/verify_perf_baseline_anchors.sh` 执行
- **AND** 该脚本 MUST 自带 `--self-test`（断言每条判据各自有判别力）

#### Scenario: 锚点被降级为 0 MUST 被抓

- **WHEN** 基准里某行 `status=PASS` 且 `median_us > 0`，而新 baseline 把该行
  降级为 `median_us = 0`（SKIPPED）
- **THEN** 守恒检查 MUST 报违规
- **AND** 违规 MUST 让脚本以**非零**退出码结束（只打印不算 —— CI 见到的是退出码）

#### Scenario: 行消失 MUST 被抓

- **WHEN** 新 baseline 缺少基准里的某一行
- **THEN** 守恒检查 MUST 报违规

#### Scenario: 变松未披露 MUST 被抓

- **WHEN** 某行比基准更慢（超过该 backend 的回归阈值）**且** 未在 `note` 的披露表里
  以 `<test> <params> <backend> old= new= ratio=` 的**行**被点名
- **THEN** 守恒检查 MUST 报违规
- **AND** 判定 MUST **不**接受「三个 token 在 `note` 里各自出现过」
  （`note` 里别的句子会提到同名 token ⇒ 那样判会让该行恒被判为已披露
  ⇒ 披露表漏写它也不会被发现）
- **AND** 披露行里的 `old=` / `new=` / `ratio=` 数值 MUST 与实际计算值一致
  （数值不符 ⇒ 报违规：披露的价值在于「披露了**多大**的变松」）
- **AND** 「更慢」的判据阈值 MUST 从**权威源**（`perf_regression.cpp` 的
  per-backend 阈值常量）读出 ⇒ MUST NOT 用脚本内的**硬编码副本**
  （副本会漂移，且漂移方向可能是**不安全**的：有人放宽 C++ 侧阈值，
  脚本仍按旧值判 ⇒ 放过未披露的变松）
- **AND** 权威源不可读 / 不可解析时 MUST **fail-closed**（报违规）⇒ MUST NOT
  回落到脚本内置的默认值（回落是 **fail-open**：权威源被改写时护栏会静默用
  旧阈值判 ⇒ 恰好复现本条想消除的漂移）
- **AND** 权威阈值表 MUST 覆盖 baseline 里出现的**所有** backend
  （否则那些行的「变松」永远不会被披露）

#### Scenario: 「恢复」标记 MUST NOT 成为降级逃逸口

- **WHEN** 新 baseline 的某行带「恢复」标记（`note_restored`）**且**其
  `median_us` 被降级为 0
- **THEN** 守恒检查 MUST 报违规（标记不是许可证）
- **AND** 带该标记的行 MUST 断言其 `median_us` 与基准**逐字相等**
  （不等 ⇒ 报违规：那不是「恢复」，是偷偷改值）

#### Scenario: 守恒检查自身的失效 MUST 被断言钉住

- **WHEN** 守恒检查的实现被改坏（例如违规只打印不返回非零退出码，或披露判定退回
  子串匹配）
- **THEN** `--self-test` MUST 失败
- **AND** CI MUST 在跑守恒检查**之前**先跑 `--self-test`
  （未测试的闸门 = 形同虚设）

#### Scenario: 基准 ref 解析不到 MUST 显式告警

- **WHEN** 基准 ref（如 CI 的 base branch）无法解析
- **THEN** 守恒检查 MUST 跳过（不阻断 PR）
- **AND** MUST 打印**显式告警**（不得静默跳过 ⇒ 闸门会长期形同虚设）
