## ADDED Requirements

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
