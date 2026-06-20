# perf-test-isolation Specification (delta)

## Purpose

TBD — extends the archived `perf-test-isolation` spec (from `openspec/changes/archive/2026-06-19-fix-perf-subprocess-isolation/`) with operational constraints on CTEST TIMEOUT configuration and `perf_baseline.json` regeneration. The original spec defines the subprocess isolation mechanism (5 REQUIREMENTs); this delta adds 2 REQUIREMENTs that codify the post-F2 TIMEOUT + baseline baseline-regen contract, preventing the false-failure pattern observed in `fix-perf-subprocess-followups` (2026-06-20).

## ADDED Requirements

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