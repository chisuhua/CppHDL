# 变更规格：perf baseline 重采集时的 TC-09 锚点口径

## MODIFIED Requirements

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
- **AND** this limitation MUST NOT produce a **false positive**, which admits exactly
      two shapes:
  - **Shape A（锚点保留）**：the baseline side MUST keep the TC-09 anchors
        `median_us > 0`（逐字保留自上一份 baseline，并打 `note_restored` 标记）
        while the current results have `median_us = 0`
        ⇒ the computed ratio is 0 ⇒ `perf_regression` reports **no** regression;
  - **Shape B（两侧皆空）**：both the baseline AND the current `perf_results.json`
        have the TC-09 rows with `median_us = 0`
        ⇒ the comparison is skipped entirely ⇒ no regression is reported.
- **AND** Shape A is **REQUIRED** whenever the previous baseline held TC-09 rows
      with `status=PASS` and `median_us > 0`
      ⇒ 否则那些锚点会**静默退出** `perf_regression` 的比较集合
      ⇒ 未来的 TC-09 真回归**永久不可见**
      （该义务的裁决条款见「perf baseline 重生成 MUST 守恒」Requirement）
- **NOTE**: 本条款的**旧措辞**（「both baseline AND current have TC-09 rows with
      `median_us=0`」）只覆盖 Shape B，写于 F2 期（2026-06/07）⇒ 早于
      2026-10-25 确立的「锚点守恒 + 逐字恢复」决策 ⇒ 与该决策**互斥**
      ⇒ 本次重采集把它改写为**两种形态都合法、且明确各自的义务**，
      以免契约与已提交的实现互相矛盾。
