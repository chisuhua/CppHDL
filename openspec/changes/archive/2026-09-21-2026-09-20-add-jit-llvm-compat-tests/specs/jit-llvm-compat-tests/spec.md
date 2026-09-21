## ADDED Requirements

### Requirement: LLVM version compatibility red-line test guards JitCompiler initialization

The test suite MUST contain `tests/jit/test_jit_llvm_version.cpp` that verifies the LLVM-version compatibility contract of `JitCompiler::is_available()`. The test MUST be registered in `tests/CMakeLists.txt` using the existing **flat registration pattern** (`add_catch_test(test_jit_llvm_version jit/test_jit_llvm_version.cpp)`), NOT via a new `tests/jit/CMakeLists.txt`. The test MUST be discoverable via `ctest --test-dir build -R test_jit_llvm_version` and tagged `[jit][llvm-compat]`.

The test body MUST:
1. Print `LLVM_VERSION_MAJOR` and `LLVM_VERSION_MINOR` at runtime (for diagnostic visibility)
2. Assert `JitCompiler::is_available() == true` when `LLVM_VERSION_MAJOR` is in `[17, 22]`
3. SUCCEED (not fail) when `LLVM_VERSION_MAJOR` is outside `[17, 22]` — future versions require explicit opt-in, not silent failure
4. Compile to `SUCCEED("JIT not enabled - skipping")` when `CH_JIT_ENABLED` is not defined (i.e., when `CH_JIT_ENABLE=OFF` is configured); the test must NOT link against `JitCompiler` in this configuration

#### Scenario: Test passes on LLVM 17-22

- **WHEN** `cmake -B build -DCH_JIT_ENABLE=ON` produces a build with LLVM 17, 18, 19, 20, 21, or 22 detected
- **THEN** `ctest -R test_jit_llvm_version --output-on-failure` reports the test as PASSED
- **AND** the runtime output includes the detected LLVM version (e.g., "LLVM version: 22.1") for diagnostic purposes

#### Scenario: Test SUCCEEDs on unsupported LLVM version

- **WHEN** `LLVM_VERSION_MAJOR` is `16` (below project minimum) or `23` (beyond tested range)
- **THEN** the test outputs a SUCCEED message naming the version
- **AND** `ctest` reports the test as PASSED (SUCCEED counts as pass)
- **AND** the rest of the test suite is unaffected

#### Scenario: Test SUCCEEDs under CH_JIT_ENABLE=OFF

- **WHEN** the project is configured with `cmake -B build -DCH_JIT_ENABLE=OFF`
- **THEN** `tests/jit/test_jit_llvm_version.cpp` compiles with `CH_JIT_ENABLED` undefined
- **AND** the test body uses `#else SUCCEED("JIT not enabled - skipping");` branch
- **AND** `ctest -R test_jit_llvm_version` reports the test as PASSED (skipping is success), not FAILED
- **AND** CI PR-feedback matrix (which uses `CH_JIT_ENABLE=OFF` per AGENTS.md) does not break

#### Scenario: Test detects future LLVM regression

- **WHEN** a future LLVM version (≥ 23) removes `llvm-c/Target.h`'s `static inline LLVMInitializeNative*` family, OR changes the `llvm::InitializeNative*` API signatures, OR otherwise breaks `JitCompiler` initialization
- **THEN** either the test target fails to compile (unresolved-identifier error), OR `is_available() == true` assertion fails
- **AND** the diagnostic output names the LLVM version, enabling rapid diagnosis

### Requirement: JitCompiler initialization smoke test guards non-crash contract

The test suite MUST contain `tests/jit/test_jit_init_succeeds.cpp` that verifies the `JitCompiler` constructor does not crash and reports a sensible availability status. The test MUST be registered in `tests/CMakeLists.txt` using the flat registration pattern (`add_catch_test(test_jit_init_succeeds jit/test_jit_init_succeeds.cpp)`).

The test body MUST:
1. Construct a `ch::jit::JitCompiler` instance
2. Use `CHECK` (not `REQUIRE`) for availability assertion — `CHECK` allows the test to report a partial failure rather than abort, supporting future opt-in to systems where native init legitimately fails
3. Compile to `SUCCEED("JIT not enabled - skipping")` when `CH_JIT_ENABLED` is not defined

#### Scenario: Smoke test passes on supported LLVM

- **WHEN** `cmake -B build -DCH_JIT_ENABLE=ON` produces a build with LLVM 17-22 detected
- **THEN** `JitCompiler compiler;` succeeds without crash
- **AND** `compiler.is_available()` returns `true`
- **AND** `ctest -R test_jit_init_succeeds` reports the test as PASSED

#### Scenario: Smoke test SUCCEEDs under CH_JIT_ENABLE=OFF

- **WHEN** the project is configured with `cmake -B build -DCH_JIT_ENABLE=OFF`
- **THEN** `tests/jit/test_jit_init_succeeds.cpp` compiles with `CH_JIT_ENABLED` undefined
- **AND** the test body uses `#else SUCCEED("JIT not enabled - skipping");` branch
- **AND** CI PR-feedback matrix does not break

### Requirement: LLVM compatibility tests preserve existing build matrix behavior

The new tests MUST NOT introduce any change to the existing build or CI matrix beyond:
- Adding `tests/jit/test_jit_llvm_version.cpp` source file
- Adding `tests/jit/test_jit_init_succeeds.cpp` source file
- Adding 2 lines to `tests/CMakeLists.txt` (the `add_catch_test` registrations)
- Adding a documentation entry to `docs/adr/ADR-027-llvm-version-detection.md`

The new tests MUST NOT require:
- Changes to `src/jit/jit_compiler.cpp` (current implementation works on LLVM 17-22, empirically verified 2026-09-20)
- Changes to `src/jit/jit_finalize.cpp` (LLJIT API stable across LLVM 16-22)
- Changes to `CMakeLists.txt` (current `find_package(LLVM REQUIRED)` correctly resolves)
- Changes to `.github/workflows/ci.yml` (current `LLVM_DIR` discovery is standard CMake package lookup, not a workaround)
- A re-generated `perf_baseline.json` (current baseline remains valid; LLVM 22 measurably faster than baseline)

#### Scenario: No source code changes required

- **WHEN** `git diff --stat HEAD~1 HEAD` is run after this change is committed
- **THEN** the diff shows ONLY changes to:
  - `tests/jit/test_jit_llvm_version.cpp` (new file, ~80 lines)
  - `tests/jit/test_jit_init_succeeds.cpp` (new file, ~50 lines)
  - `tests/CMakeLists.txt` (2 lines added, ~10 lines of comment)
  - `docs/adr/ADR-027-llvm-version-detection.md` (v2.0 section appended, ~30 lines)
- **AND** no changes appear under `src/`, `include/`, `CMakeLists.txt`, `.github/workflows/`, or `openspec/specs/`

#### Scenario: Full test suite remains green

- **WHEN** the change is committed and `ctest --test-dir build --output-on-failure` is run
- **THEN** all tests pass — including the new `test_jit_llvm_version` and `test_jit_init_succeeds`
- **AND** `perf_regression` reports 0 regression (LLVM 22 JIT performance is 1.47-1.75x faster than baseline, well within the 1.6x jit threshold)
- **AND** the `CH_JIT_ENABLE=OFF` PR-feedback matrix continues to pass without breaking on the new tests