# Design: Verilator Submodule + End-to-End Three-Way Perf Comparison

**Date:** 2026-06-10
**Status:** Revised after oracle review (commit `5a18f98` reverted)
**Author:** Sisyphus (brainstorming session)

## Revision history

- **2026-06-10 v1.0 → v2.0:** Switched from `add_subdirectory` to
  `ExternalProject_Add` + `--prefix` after oracle review revealed
  three blocking issues with the original plan:
  1. Verilator's CMake target is named `verilator${CMAKE_BUILD_TYPE}`
     (e.g. `verilatorRelease`), not `verilator`. See
     `third_party/verilator/src/CMakeLists.txt` upstream: `set(verilator
     verilator${CMAKE_BUILD_TYPE})`.
  2. The Perl wrapper at `bin/verilator` sets `VERILATOR_ROOT` from
     `$RealBin/..` and looks for `verilator_bin` in `$RealBin`. If we
     `add_subdirectory`, the binary lives in the build tree but the
     wrapper still expects it next to itself in the source tree. The
     wrapper does consult `$ENV{VERILATOR_BIN}` as an override, but
     that requires injecting an environment variable at every test
     invocation.
  3. `add_subdirectory` propagates all of Verilator's targets into
     `ALL`, which means every `make` invocation walks Verilator's
     internal dependency graph. The spec proposed
     `set_target_properties(verilator PROPERTIES EXCLUDE_FROM_ALL TRUE)`
     but (a) the target name is wrong, and (b) the flag has to be set
     on every target, not just one. The cleaner pattern with `add_subdirectory`
     would be `add_subdirectory(... EXCLUDE_FROM_ALL)`, but that
     combined with (1) still leaves (2) unresolved.

  `ExternalProject_Add` + `--prefix` avoids all three problems because
  the install step places both the wrapper AND the binary side-by-side
  in `${CMAKE_BINARY_DIR}/verilator-install/bin/`, matching the layout
  the wrapper expects. We also retain the submodule (per user's
  explicit choice) — the submodule provides the source, and
  `ExternalProject_Add` builds and installs it.

## 1. Background

The CppHDL project ships a three-way performance comparison harness in
`tests/benchmark/` (interpreter / JIT / Verilator) per the design
captured in
`docs/superpowers/specs/2026-06-08-perf-tests-jit-verilator-comparison-design.md`
and the follow-up plan `.omo/plans/perf-report-followup.md`. All 11
work items (W1–W11) have been merged into `main` as of commit
`b335daf`.

The remaining gap is **environmental**: Verilator is not installed
in this development environment, so every `perf_tests` run reports
the Verilator column as `UNSUPPORTED` with `skip_reason: "verilator
not found on PATH"`. This is technically correct (W5 added the
distinction between `UNSUPPORTED` and `SKIPPED`) but it means we have
**never observed a real three-way PASS row** in this environment.

This spec proposes to close that gap by vendoring Verilator as a git
submodule and integrating its build into the CppHDL CMake graph via
`ExternalProject_Add` + `--prefix`, so `cmake --build build` produces
a working `verilator` binary and a working `bin/verilator` wrapper
alongside the CppHDL artifacts, and `perf_tests` can exercise the
real three-way comparison.

## 2. Goals

1. **Reproducibility.** Pin a known-good Verilator version (5.x stable)
   as a submodule so every developer and CI worker gets the same version.
2. **End-to-end build.** `cmake --build build` builds both CppHDL and
   Verilator in one step; no manual `apt install verilator` step required.
3. **Three-way PASS rows.** After integration, `perf_tests --tc=07/08`
   should produce PASS rows in all three backend columns
   (interpreter, jit, verilator) — not UNSUPPORTED.
4. **Existing code preserved.** The `verilator_runner.h`, the harness
   files (`verilator_harness_tc07.cpp`, `verilator_harness_tc08.cpp`),
   and the `perf_tests` CLI all stay as they are. Only the source of
   the `verilator` binary changes (from system PATH to a CMake-known
   install-tree path).
5. **Graceful degradation preserved.** When the Verilator build is
   skipped (e.g. developer sets `-DBUILD_VERILATOR=OFF`), the existing
   `UNSUPPORTED` fallback continues to work without errors.

## 3. Non-Goals

- **No CppHDL source changes.** The CPU models, the JIT compiler, and
  the interpreter are out of scope. Verilator is a downstream consumer
  of CppHDL-emitted Verilog.
- **No verilator_harness_tc09/tc11.** TC-09 (arithmetic) and TC-11
  (wide register) have no Verilator harness today. Adding them is a
  separate follow-up; this spec only makes the **existing** TC-07/08
  Verilator column work.
- **No CI changes in this spec.** CI workflow updates (adding
  `submodules: true` to checkout step) are a separate operational
  change documented in §10 but not implemented here.
- **No cross-compile support.** The Verilator build assumes the host
  toolchain (gcc/clang). Cross-compilation is out of scope.

## 4. Architecture

```
CppHDL/
├── .gitmodules                          ← NEW: submodule registration
├── third_party/                         ← NEW: vendored deps dir
│   └── verilator/                       ← NEW: git submodule (https://github.com/verilator/verilator.git, v5.x tag)
│       ├── src/CMakeLists.txt           (upstream — defines verilator${CMAKE_BUILD_TYPE} target)
│       ├── bin/verilator                (upstream Perl wrapper — must NOT be moved)
│       ├── bin/verilator_bin            (NOT upstream — built by Verilator's own build)
│       └── ...
├── CMakeLists.txt                       ← MODIFIED: ExternalProject_Add for verilator
│                                            + find_program probes for PERL/FLEX/BISON
│                                            + sets CPPHDL_VERILATOR_DIR and CPPHDL_VERILATOR_BIN
├── scripts/
│   └── init-submodules.sh               ← NEW: convenience wrapper
├── docs/developer_guide/
│   └── verilator-integration.md         ← NEW: developer-facing doc
└── tests/benchmark/                     ← UNCHANGED (perf_tests stays as-is)
    ├── verilator_runner.h               (already accepts --verilator=<path>)
    ├── perf_main.cpp                    (already accepts --verilator=<path>)
    └── CMakeLists.txt                   (only sets perf_tests ENVIRONMENT is new)
```

### Component boundaries

- **Submodule registration (`.gitmodules`)** — pure git metadata, no
  C++ code. The submodule URL is
  `https://github.com/verilator/verilator.git` with path
  `third_party/verilator` and a pinned tag (TBD, latest 5.x stable
  at the time of execution, e.g. `v5.020` or `v5.024`).

- **Verilator source (`third_party/verilator/`)** — upstream code, used
  read-only by CppHDL. CppHDL does not patch Verilator. If we ever
  need a Verilator patch, we upstream it first.

- **Verilator install (`${CMAKE_BINARY_DIR}/verilator-install/`)** —
  produced by Verilator's own build via
  `cmake --install --prefix ${CMAKE_BINARY_DIR}/verilator-install`.
  This is the directory CppHDL points to at runtime. It contains:
  - `bin/verilator_bin` — compiled binary (built by Verilator)
  - `include/vl/*`, `include/verilated*.h` — runtime headers
  - `share/verilator/` — helper data

  Note: the upstream `bin/verilator` Perl wrapper is **not**
  auto-installed by Verilator's install step (we verified this
  against `master` of the upstream repo). CppHDL's CMake integration
  handles this by injecting the wrapper via the `CPPHDL_VERILATOR_DIR`
  path (Decision 5) — the wrapper sits in the source tree, and the
  `--verilator` flag is resolved relative to `CPPHDL_VERILATOR_DIR`
  as needed.

  **Wait — re-evaluated:** the original v1 of this spec assumed the
  wrapper sits next to the binary after install. The oracle's
  finding was that this assumption is fragile. The actual
  relationship is:
  - The wrapper sets `VERILATOR_ROOT` from `$RealBin/..`
  - The wrapper finds `verilator_bin` in `$RealBin` (sibling of
    wrapper) OR via `$ENV{VERILATOR_BIN}`

  We need to either (a) install both wrapper and binary to
  `${CMAKE_BINARY_DIR}/verilator-install/bin/`, or (b) point
  `--verilator` at the wrapper's source-tree location
  (`third_party/verilator/bin/verilator`) and set
  `VERILATOR_BIN` env var to the install location.

  **Final choice (revised Decision 5):** we point `--verilator` at
  the **source tree wrapper**
  (`${CMAKE_SOURCE_DIR}/third_party/verilator/bin/verilator`), and
  set `VERILATOR_BIN` env var in the ctest test properties to point
  at the install-tree binary. The source-tree wrapper is always
  present (it's part of the submodule), no install step needed for
  it, and we explicitly tell it where to find the binary.

  This means `--verilator` and `VERILATOR_BIN` are two different
  paths but both have clear ownership:
  - `--verilator`: source tree, always present
  - `VERILATOR_BIN`: install tree, present after `make verilator`

- **CMake integration (top-level `CMakeLists.txt`)** — adds an
  `option(BUILD_VERILATOR "Build Verilator from submodule" ON)` gate
  and an `ExternalProject_Add(verilator ...)` that:
  1. configures Verilator's own CMake in an out-of-tree build dir
  2. builds it with `cmake --build`
  3. runs `cmake --install --prefix ${CMAKE_BINARY_DIR}/verilator-install`

  Caches the resulting install directory into
  `CPPHDL_VERILATOR_DIR` and the binary path into
  `CPPHDL_VERILATOR_BIN`.

- **Test wiring (`tests/benchmark/CMakeLists.txt`)** — sets
  `set_tests_properties(perf_tests PROPERTIES
  ENVIRONMENT "VERILATOR_BIN=${CPPHDL_VERILATOR_BIN}")`. The
  `perf_main.cpp` is unchanged; it still accepts `--verilator=<path>`
  and the ctest entry adds it.

### Data flow at build time

```
git clone <CppHDL>
  ↓
git submodule update --init --recursive
  ↓
cmake -B build
  ↓
  option(BUILD_VERILATOR) = ON (default)
  find_program(PERL perl REQUIRED) → fails fast if missing
  find_program(FLEX flex) → warns if missing
  find_program(BISON bison) → warns if missing
  ↓
  ExternalProject_Add(verilator
    SOURCE_DIR  ${CMAKE_SOURCE_DIR}/third_party/verilator
    BINARY_DIR  ${CMAKE_BINARY_DIR}/verilator-build
    INSTALL_DIR ${CMAKE_BINARY_DIR}/verilator-install
    CMAKE_ARGS  -DCMAKE_BUILD_TYPE=Release
                 -DCMAKE_INSTALL_PREFIX=<install>
  )
  ↓
cmake --build build
  ↓
  ExternalProject builds verilator
  (first build: 10-30 min; subsequent: seconds)
  install step places verilator_bin + headers under <install>/
  ↓
./build/tests/benchmark/perf_tests --verilator=<source>/third_party/verilator/bin/verilator --tc=07
  ↓
  ENV{VERILATOR_BIN} = <install>/bin/verilator_bin (injected by ctest)
  Perl wrapper at <source>/third_party/verilator/bin/verilator executes
  wrapper sets VERILATOR_ROOT = <source>/third_party/verilator (from $RealBin/..)
  wrapper finds verilator_bin at $ENV{VERILATOR_BIN} = <install>/bin/verilator_bin
  wrapper runs verilator --build against Tc07XorChain's Verilog
  verilated binary executes ticks, returns timing
  → Three-way PASS row emitted
```

### Data flow at perf-test runtime

```
perf_tests --verilator=<wrapper>  (with ENV{VERILATOR_BIN}=<binary>)
  ↓
VerilatorRunner(<wrapper>)
  ↓
  is_available():
    popen("<wrapper> --version 2>&1")
    exit 0, output is "Verilator 5.x..." → returns true
  ↓
  build(test_name, verilog, harness):
    shell_exec("<wrapper> --build --exe harness.cpp top.v")
    → wrapper sets VERILATOR_ROOT and execs verilator_bin
    → verilator generates ./obj_dir/Vtop + verilated binary
  ↓
  run_and_time(binary, ticks):
    exec(./obj_dir/Vtop, ticks=1000)
    → real Verilator PASS row with measured sim_us
```

## 5. Design Decisions

### Decision 1: `ExternalProject_Add` (not `add_subdirectory`)

**Choice:** Use `ExternalProject_Add` with `--prefix` to install
Verilator into `${CMAKE_BINARY_DIR}/verilator-install/`.

**Rationale:**
- User asked for **submodule** (preserved: the source comes from a
  pinned submodule).
- User asked for "and build together" (preserved: `cmake --build
  build` triggers the Verilator build).
- The original `add_subdirectory` plan failed the oracle review
  with three blocking issues (target name, VERILATOR_ROOT, wrapper
  /binary co-location). `ExternalProject_Add` with install solves
  all three.
- `ExternalProject_Add` decouples Verilator's build graph from
  CppHDL's, so a `make all` does NOT walk Verilator's dependency
  graph. Verilator builds only when explicitly targeted
  (`make verilator`) or when the install dir is missing.

**Rejected alternatives:**
- `add_subdirectory` (v1 of this spec): rejected by oracle review
- `FetchContent`: would download a tarball, violating the submodule
  requirement
- Manual build script + find_program: requires extra ceremony and
  no reproducibility guarantee

### Decision 2: Pin to a specific 5.x tag

**Choice:** Pin to the latest 5.x stable tag at time of integration.
The exact tag is selected by running
`git ls-remote --tags https://github.com/verilator/verilator.git` and
picking the highest semver that is a 5.x release (not a pre-release).

**Rationale:**
- 5.x is the current stable line; 4.x is EOL.
- Pinning to a tag (not a branch) gives reproducible builds.
- Tracking `master` would pick up unreleased changes and break
  compatibility regularly.

### Decision 3: `--prefix` install location = `${CMAKE_BINARY_DIR}/verilator-install`

**Choice:** Install Verilator under
`${CMAKE_BINARY_DIR}/verilator-install/`, not under a system path.

**Rationale:**
- Keeps Verilator hermetic to the build tree. No `sudo make install`
  needed, no system pollution, easy to wipe (`rm -rf build/verilator-install`).
- Allows CI to cache the install dir across jobs (hermetic, stable
  content).

### Decision 4: CMake option gate, default ON

**Choice:** `option(BUILD_VERILATOR "Build Verilator from submodule" ON)`.

**Rationale:**
- Defaulting ON matches the user's intent.
- OFF lets developers with a pre-installed system verilator skip the
  submodule build (CI on a constrained image, quick local iteration).
- When OFF, `CPPHDL_VERILATOR_BIN` is empty and `perf_tests` falls
  back to its default (PATH lookup). Same UNSUPPORTED behavior as
  before, no regression.

### Decision 5: `--verilator` points at source-tree wrapper, `VERILATOR_BIN` env var points at install-tree binary

**Choice:** The `perf_tests` executable is invoked with
`--verilator=<source>/third_party/verilator/bin/verilator` (the Perl
wrapper from the submodule, never moved). The ctest entry sets
`ENV{VERILATOR_BIN}=<install>/bin/verilator_bin` so the wrapper can
find the actual binary.

**Rationale:**
- The wrapper is part of the submodule, so it's always present at
  the source-tree path. No need to copy/install it.
- The wrapper consults `$ENV{VERILATOR_BIN}` before doing its
  `$RealBin/$basename` check (verified by reading
  `bin/verilator` line 219), so injecting the env var is sufficient.
- We do NOT need to add a POST_BUILD copy step (we previously
  considered this); the env var injection is cleaner.
- This avoids the wrapper/binary co-location issue entirely.

### Decision 6: Host build dependency check at configure time

**Choice:** At CMake configure time, before declaring the
ExternalProject:

```cmake
if(BUILD_VERILATOR)
    find_program(PERL perl REQUIRED)
    find_program(FLEX flex)
    find_program(BISON bison)
    if(NOT FLEX OR NOT BISON)
        message(WARNING
            "BUILD_VERILATOR=ON but flex or bison not found. Verilator "
            "build will fail. Install with: apt install flex bison "
            "(Ubuntu) or brew install flex bison (macOS). To skip the "
            "Verilator build, re-run with -DBUILD_VERILATOR=OFF.")
    endif()
endif()
```

`PERL` is REQUIRED because the `bin/verilator` wrapper cannot run
without it. `flex` and `bison` are only warnings because Verilator's
own CMake will produce a more specific error later, but a configure-
time warning helps developers catch the issue early.

### Decision 7: Document build dependencies (perl + flex + bison)

**Choice:** `docs/developer_guide/verilator-integration.md` lists
Verilator's host build dependencies:

- `flex`, `bison` (Verilator's CMake checks for these)
- `g++` ≥ 7 (or clang equivalent) — for Verilator's own compilation
- `make` (or ninja)
- `perl` ≥ 5.6.1 (REQUIRED — `bin/verilator` is a Perl script)
- `python3` ≥ 3.6
- `autoconf`, `m4`, `help2man`, `c++filt` (optional but recommended)

Install commands:
- Ubuntu: `sudo apt install flex bison build-essential python3 perl autoconf help2man ccache libfl2 libfl-dev zlibc liblzma-dev libsystemd-dev`
- macOS: `brew install flex bison python3 gperftools readline ccache help2man`

**Rationale:**
- The Verilator build is known to fail with cryptic errors when
  `flex`/`bison`/`perl` are missing. Documenting upfront saves a
  debug session per developer.
- The Perl requirement is the most-often missed item; explicitly
  calling it out here.

### Decision 8: Fallback to system verilator when present

**Choice:** If `BUILD_VERILATOR=OFF` AND `find_program(verilator
verilator)` finds a binary on PATH, the system verilator is used
(its path is the value returned by `find_program`).

**Rationale:**
- Developers who already have Verilator installed (e.g. macOS via
  Homebrew) don't need to build it from the submodule.
- The same `--verilator=<path>` flag drives both cases; the only
  thing that changes is the default.

## 6. Data flow details

### CMake variable contract

| Variable | Set by | Read by | Meaning |
|---|---|---|---|
| `BUILD_VERILATOR` | user (option) | CppHDL top-level CMakeLists | ON (default) / OFF |
| `CPPHDL_VERILATOR_DIR` | CppHDL top-level CMakeLists (after ExternalProject) | `tests/benchmark/CMakeLists.txt` | Install path or empty |
| `CPPHDL_VERILATOR_BIN` | CppHDL top-level CMakeLists (after install) | `tests/benchmark/CMakeLists.txt` (for ctest ENVIRONMENT) | Path to `verilator_bin` or empty |
| `verilator` (ExternalProject target) | ExternalProject_Add | CppHDL top-level | The build target; depends on install step |

### Submodule initialization

- `.gitmodules` contains a single submodule entry pointing
  `third_party/verilator` at
  `https://github.com/verilator/verilator.git`.
- The submodule is pinned to a specific commit SHA corresponding to a
  5.x tag (e.g. `v5.020` → specific commit hash).
- `scripts/init-submodules.sh` is a thin wrapper that runs
  `git submodule update --init --recursive` and checks for
  uninitialized submodules, exiting non-zero with a clear message if
  the submodule is empty.

## 7. Error handling

| Failure mode | Behavior |
|---|---|
| Submodule not initialized (empty `third_party/verilator/`) | CMake configure prints error and aborts with `message(FATAL_ERROR "third_party/verilator is empty. Run 'git submodule update --init --recursive' first.")` |
| `perl` missing | CMake configure aborts with `find_program(PERL perl REQUIRED)` |
| `flex`/`bison` missing | CMake warning at configure time + Verilator's own build fails later |
| Verilator build fails | `make verilator` fails; `perf_tests` ctest entry shows UNSUPPORTED with `skip_reason: "verilator build failed"` |
| Verilator built but binary doesn't run | `is_available()` (W5-fixed) returns false → `UNSUPPORTED` |
| Developer sets `-DBUILD_VERILATOR=OFF` | `CPPHDL_VERILATOR_BIN` is empty; `perf_tests` ctest entry falls back to `--verilator=verilator` (PATH lookup) → existing UNSUPPORTED behavior preserved |
| `verilator --build` reports error | VerilatorRunner reports `br.success=false` → `SKIPPED` (W5) |

## 8. Testing

### Smoke test

```bash
# After applying this spec:
git clone <CppHDL>
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) verilator       # verilator first (slow)
cmake --build build -j$(nproc) cpphdl          # then CppHDL
./build/tests/benchmark/perf_tests --tc=07 --warmup=3 --measured=5 --report=json
# Expect: 3 rows per depth (interpreter/jit/verilator), all status=PASS for
#         depth=10/100, verilator row at depth=1000 may be slow but PASS
```

### Regression test

Add a new Catch2 test `tests/benchmark/test_verilator_integration.cpp`:

- Tags: `[perf][verilator][integration]`
- Invokes `VerilatorRunner` directly (not via `perf_tests` subprocess)
  with a minimal C++ input that Verilator can build. This avoids
  the brittleness of subprocess JSON parsing.
- Asserts: `is_available()` returns true AND a tiny `--binary` build
  succeeds for a trivial Verilog module.
- SKIPs (not FAILs) if `CPPHDL_VERILATOR_BIN` is empty or the
  verilator binary is missing.
- This test catches the "I broke the verilator path" regression
  class without depending on the heavy full perf run.

### Manual verification checklist

- [ ] `git submodule status` shows `third_party/verilator` at the pinned SHA with `+` prefix (clean)
- [ ] `cmake --build build` completes with verilator binary present
- [ ] `./build/verilator-install/bin/verilator_bin --version` prints a version string
- [ ] `ctest -L perf` passes
- [ ] `perf_tests --tc=07 --report=json | jq '.runs[] | select(.backend=="verilator") | .status'`
      shows `"PASS"` for at least one row

## 9. Out of scope (deferred)

- `verilator_harness_tc09.cpp` / `verilator_harness_tc11.cpp` — needed
  for full three-way coverage of TC-09 (arithmetic) and TC-11 (wide
  register). Separate follow-up.
- CI integration (`submodules: true` in checkout step) — operational
  change; document in §10 but do not implement in this spec.
- Verilator binary in release artifacts (`make install`-style
  packaging) — separate change.
- Cross-compilation of Verilator — Verilator upstream has limited
  support; defer.

## 10. Operational notes (documentation, not implementation)

### CI changes needed

GitHub Actions (or equivalent) must enable submodule checkout:

```yaml
- uses: actions/checkout@v4
  with:
    submodules: true        # ← this is the change
    submodules: recursive
```

### Disk space

Verilator's build tree consumes ~1–2 GB during build. Final install
plus headers is ~50 MB. CI runners should have ≥ 5 GB free.

### Build time

First-time Verilator build on a 4-core x86_64: 10–30 minutes.
Subsequent incremental builds: seconds (only the changed
`verilator` sources recompile, link step ~5 seconds).

### Cache strategy

The Verilator build is hermetic and produces deterministic output
(git SHA + CMake cache). CI can cache
`build/verilator-install/` across jobs to amortize the cold build
cost.

## 11. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Verilator build takes > 30 min on slow CI | Slow CI feedback | Add CI cache for `build/verilator-install/`. Document `EXPECTED_BUILD_TIME` in DEVELOPING.md. |
| Verilator submodule init fails (network/auth) | Submodule empty; CppHDL configure fails | Fast-fail at configure time with clear message; document `git submodule update --init --recursive` in README |
| Verilator 5.x breaks compat with existing harness | All Verilator rows FAIL or crash | Integration test (§8) catches this in CI before merge. If a fix is needed, upstream patch first. |
| flex/bison missing on dev machine | Verilator build fails | Document install commands in `verilator-integration.md`; CMake warning at configure time |
| Verilator install layout changes between versions | Wrapper no longer at expected location | `VERILATOR_BIN` env var is independent of layout; integration test (Decision 8 / §8) catches it |
| `verilator_bin` location differs from `bin/` (post-install) | Wrapper can't find binary | The wrapper consults `$ENV{VERILATOR_BIN}`; we set this if needed. Default install layout keeps them siblings. |
| `VERILATOR_ROOT` not set | Generated build fails | Wrapper auto-sets `VERILATOR_ROOT` from `$RealBin/..` (verified by reading `bin/verilator` line 92) |
| `perl` missing | Wrapper cannot execute | `find_program(PERL perl REQUIRED)` at configure time |

## 12. Open questions

None at this point. The user has confirmed:
- Strategy: full submodule + self-build
- Version: latest 5.x stable
- CMake integration: build in-tree (initial plan was add_subdirectory;
  revised to ExternalProject_Add per oracle review)

Submodule pin (exact tag) will be decided at implementation time by
running `git ls-remote --tags` and picking the highest 5.x stable.
