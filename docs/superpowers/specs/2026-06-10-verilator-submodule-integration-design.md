# Design: Verilator Submodule + End-to-End Three-Way Perf Comparison

**Date:** 2026-06-10
**Status:** v3 — final, pending user approval
**Author:** Sisyphus (brainstorming session)

## Revision history

- **v1 (commit `5a18f98`, reverted):** Used `add_subdirectory(third_party/verilator)`.
  Oracle review found 3 blocking issues: wrong CMake target name, broken
  VERILATOR_ROOT setup, wrapper/binary co-location.
- **v2 (commit `f870ca9`, reverted):** Switched to `ExternalProject_Add +
  --prefix` and pointed `--verilator` at the source-tree wrapper while
  injecting `VERILATOR_BIN` as a ctest env var. Oracle review found
  2 new blocking issues: (a) the v2 spec falsely claimed Verilator does
  NOT auto-install the wrapper — in fact, Verilator's top-level
  `CMakeLists.txt` explicitly does `install(PROGRAMS bin/${program} TYPE BIN)`
  for `verilator` (and 5 other helper programs); (b) the source-tree
  wrapper sets `VERILATOR_ROOT = $RealBin/..` which points at the
  source root, but `verilated_config.h` and `verilated.mk` (the
  `configure_file()` outputs) only exist in the build tree and
  install tree — the source root only has `.in` templates. The
  generated Makefile in `obj_dir/` would fail to find
  `$VERILATOR_ROOT/include/verilated.mk`.
- **v3 (current):** Uses `ExternalProject_Add + --prefix` and points
  `--verilator` at the **installed** wrapper
  (`<install>/bin/verilator`). Because the wrapper sits in the same
  directory as `verilator_bin` after install, `$RealBin/..` is the
  install prefix (which DOES have configured data files), the
  wrapper finds `verilator_bin` as a sibling via the default
  `$RealBin/verilator_bin` lookup, and `VERILATOR_ROOT` is set to
  the install prefix. **No `VERILATOR_BIN` env var is needed.**

The full chain of evidence (lines, snippets) is preserved below in §11.

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
2. **End-to-end build.** `cmake --build build` plus a single follow-up
   `cmake --build build --target verilator` builds both CppHDL and
   Verilator; no manual `apt install verilator` step required.
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
│       ├── bin/verilator                (upstream Perl wrapper)
│       ├── bin/verilator_bin            (NOT upstream — built by Verilator's own build)
│       └── include/                      (upstream headers + verilated_*.in templates)
├── CMakeLists.txt                       ← MODIFIED: ExternalProject_Add for verilator
│                                            + find_program probes for PERL/FLEX/BISON
│                                            + sets CPPHDL_VERILATOR_DIR/WRAPPER/BIN
├── scripts/
│   └── init-submodules.sh               ← NEW: convenience wrapper
├── docs/developer_guide/
│   └── verilator-integration.md         ← NEW: developer-facing doc
└── tests/benchmark/                     ← UNCHANGED (perf_tests stays as-is)
    ├── verilator_runner.h               (already accepts --verilator=<path>)
    ├── perf_main.cpp                    (already accepts --verilator=<path>)
    └── CMakeLists.txt                   (only --verilator= flag injection is new)
```

### Component boundaries

- **Submodule registration (`.gitmodules`)** — pure git metadata, no
  C++ code. The submodule URL is
  `https://github.com/verilator/verilator.git` with path
  `third_party/verilator` and a pinned tag (TBD, latest 5.x stable at
  the time of execution, e.g. `v5.020` or `v5.024`).

- **Verilator source (`third_party/verilator/`)** — upstream code, used
  read-only by CppHDL. CppHDL does not patch Verilator. If we ever
  need a Verilator patch, we upstream it first.

- **Verilator install (`${CMAKE_BINARY_DIR}/verilator-install/`)** —
  produced by Verilator's own `cmake --install --prefix` step. Contains:
  - `bin/verilator` — Perl wrapper (auto-installed via
    `install(PROGRAMS bin/${program} TYPE BIN)` in Verilator's top-level
    `CMakeLists.txt`, line 164, for the loop containing
    `verilator, verilator_gantt, verilator_ccache_report,
    verilator_difftree, verilator_profcfunc, verilator_includer`).
  - `bin/verilator_bin` — compiled binary (the actual C++ executable).
  - `include/verilated_config.h` and `include/verilated.mk` —
    `configure_file()` outputs, both present in the install tree.
  - `share/verilator/` — helper data.

  After install, the wrapper and binary are **siblings** in
  `${prefix}/bin/`. This matches the layout Verilator's own
  developers use, and is exactly what `bin/verilator` line 221
  expects when it does `if (-x "$RealBin/$basename")`.

- **CMake integration (top-level `CMakeLists.txt`)** — adds an
  `option(BUILD_VERILATOR "Build Verilator from submodule" ON)` gate
  and an `ExternalProject_Add(verilator ...)` that:
  1. configures Verilator's own CMake in an out-of-tree build dir
     (`${CMAKE_BINARY_DIR}/verilator-build`)
  2. builds it with `cmake --build`
  3. runs `cmake --install --prefix ${CMAKE_BINARY_DIR}/verilator-install`

  Caches the resulting install directory into
  `CPPHDL_VERILATOR_DIR`, the wrapper path into
  `CPPHDL_VERILATOR_WRAPPER`, and the binary path into
  `CPPHDL_VERILATOR_BIN`.

- **Test wiring (`tests/benchmark/CMakeLists.txt`)** — adds
  `--verilator=${CPPHDL_VERILATOR_WRAPPER}` to the
  `add_test(NAME perf_tests ...)` command. No new env vars needed.

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
                 -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
  )
  ↓
cmake --build build --target verilator
  ↓
  ExternalProject builds verilator
  (first build: 10-30 min cold; seconds warm)
  install step places:
    <install>/bin/verilator        (Perl wrapper, auto-installed)
    <install>/bin/verilator_bin    (binary)
    <install>/include/verilated_*.h, verilated.mk (configured files)
cmake --build build              # builds CppHDL
  ↓
./build/tests/benchmark/perf_tests --verilator=<install>/bin/verilator --tc=07
  ↓
  Perl wrapper at <install>/bin/verilator executes
  wrapper sets VERILATOR_ROOT = <install>/ (from $RealBin/..)
  wrapper finds verilator_bin at <install>/bin/verilator_bin (sibling, via default $RealBin/$basename)
  wrapper sets VERILATOR_BIN internally to verilator_bin path (or just execs it directly)
  wrapper runs verilator --build against Tc07XorChain's Verilog
  verilated binary executes ticks, returns timing
  → Three-way PASS row emitted
```

### Data flow at perf-test runtime

```
perf_tests --verilator=<install>/bin/verilator
  ↓
VerilatorRunner(<install>/bin/verilator)
  ↓
  is_available():
    popen("<install>/bin/verilator --version 2>&1")
    exit 0, output is "Verilator 5.x..." → returns true
  ↓
  build(test_name, verilog, harness):
    shell_exec("<install>/bin/verilator --build --exe harness.cpp top.v")
    → wrapper auto-sets VERILATOR_ROOT, finds sibling binary, runs
      Verilator's own internal g++/make, produces ./obj_dir/Vtop + binary
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
  build --target verilator` triggers the Verilator build; running
  `cmake --build build` without target only builds CppHDL artifacts).
- The original `add_subdirectory` plan failed the oracle review
  with three blocking issues (target name, VERILATOR_ROOT, wrapper
  /binary co-location).
- `ExternalProject_Add` with install solves all three: install
  places wrapper and binary side-by-side, and `VERILATOR_ROOT`
  resolves to `${prefix}/share/verilator` naturally.
- `ExternalProject_Add` decouples Verilator's build graph from
  CppHDL's, so a plain `make all` does NOT walk Verilator's
  dependency graph. Verilator builds only when explicitly targeted.

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
  needed, no system pollution, easy to wipe
  (`rm -rf build/verilator-install`).
- Aligns wrapper and binary: `${prefix}/bin/verilator` and
  `${prefix}/bin/verilator_bin` are siblings, which is what
  upstream's install does and what the wrapper expects
  (`$RealBin/$basename`).
- `VERILATOR_ROOT` is auto-derivable as `${prefix}/share/verilator`
  (matches upstream's install layout).
- Allows CI to cache the install dir across jobs (hermetic, stable
  content).

### Decision 4: CMake option gate, default ON

**Choice:** `option(BUILD_VERILATOR "Build Verilator from submodule" ON)`.

**Rationale:**
- Defaulting ON matches the user's intent.
- OFF lets developers with a pre-installed system verilator skip the
  submodule build (CI on a constrained image, quick local iteration).
- When OFF, `CPPHDL_VERILATOR_WRAPPER` is empty and `perf_tests` falls
  back to its default (PATH lookup). Same UNSUPPORTED behavior as
  before, no regression.

### Decision 5: `--verilator` points at the **installed** wrapper, no env var

**Choice:** The `perf_tests` executable is invoked with
`--verilator=${CPPHDL_VERILATOR_DIR}/bin/verilator` (the wrapper
installed by Verilator's own install step, sitting next to
`verilator_bin`).

**Rationale:**
- The wrapper at `<install>/bin/verilator` sets `VERILATOR_ROOT` from
  `$RealBin/..` = the install prefix. The install prefix contains the
  configured data files (`include/verilated_config.h`,
  `include/verilated.mk`) that the generated Makefile in `obj_dir/`
  needs to find.
- The wrapper's default `$RealBin/verilator_bin` lookup
  (line 221 of upstream `bin/verilator`) finds the binary as a
  sibling in `<install>/bin/`.
- **No `VERILATOR_BIN` env var needed** (the v2 approach that needed
  it was the workaround for using the source-tree wrapper, which
  is no longer the approach).
- `perf_main.cpp`'s default of looking up `verilator` on `PATH` is
  preserved for manual invocations (developer can install Verilator
  system-wide and run `perf_tests` without rebuilding).

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
without it. `flex` and `bison` are warnings (not REQUIRED) because
Verilator's own CMake uses `find_package(BISON)` and `find_package(FLEX)`
internally, and a missing-bison error from Verilator's own build is
already a clear message. We just give a heads-up at configure time.

### Decision 7: Document build dependencies (perl + flex + bison)

**Choice:** `docs/developer_guide/verilator-integration.md` lists
Verilator's host build dependencies:

- `flex`, `bison` (Verilator's CMake uses these for code generation)
- `g++` ≥ 7 (or clang equivalent) — for Verilator's own compilation
- `make` (or ninja)
- `perl` ≥ 5.6.1 (REQUIRED — `bin/verilator` is a Perl script;
  declared in upstream `bin/verilator` line 12: `require 5.006_001;`)
- `python3` ≥ 3.6 (used by Verilator's code generation scripts)
- `autoconf`, `m4`, `help2man`, `c++filt`, `libsystemd-dev`
  (optional but recommended)

Install commands:
- Ubuntu: `sudo apt install flex bison build-essential python3 perl autoconf help2man ccache libfl2 libfl-dev zlibc liblzma-dev libsystemd-dev`
- macOS: `brew install flex bison python3 gperftools readline ccache help2man`

**Rationale:**
- The Verilator build is known to fail with cryptic errors when
  `flex`/`bison`/`perl` are missing. Documenting upfront saves a
  debug session per developer.
- The Perl requirement is the most-often missed item; explicitly
  calling it out here.

### Decision 8: Top-level convenience targets

**Choice:** Add two convenience targets at the top of
`CMakeLists.txt`:

```cmake
add_custom_target(cpphdl DEPENDS cpphdl)   # alias for clarity in logs
add_custom_target(perf_test_bench DEPENDS
    ${CPPHDL_PERF_TESTS_EXE}
    ${CPPHDL_PERF_REGRESSION_EXE}
)
```

These are no-ops; they exist purely so the developer can write
`cmake --build build --target verilator` to build Verilator and
`cmake --build build` to build CppHDL, and have a clear mental
model. The main work happens at the ExternalProject level
(`make verilator`).

### Decision 9: Fallback to system verilator when present

**Choice:** If `BUILD_VERILATOR=OFF` AND `find_program(verilator
verilator)` finds a binary on PATH, the system verilator is used.
The ctest test entry then defaults `--verilator=verilator` and the
system binary is found by `verilator_runner.h`.

**Rationale:**
- Developers who already have Verilator installed (e.g. macOS via
  Homebrew) don't need to build it from the submodule.
- The same `--verilator=<path>` flag drives both cases; the only
  thing that changes is the default path.

## 6. Data flow details

### CMake variable contract

| Variable | Set by | Read by | Meaning |
|---|---|---|---|
| `BUILD_VERILATOR` | user (option) | CppHDL top-level CMakeLists | ON (default) / OFF |
| `CPPHDL_VERILATOR_DIR` | CppHDL top-level CMakeLists (after ExternalProject) | diagnostics | Install path or empty |
| `CPPHDL_VERILATOR_WRAPPER` | CppHDL top-level CMakeLists | `tests/benchmark/CMakeLists.txt` | `<dir>/bin/verilator` or empty |
| `CPPHDL_VERILATOR_BIN` | CppHDL top-level CMakeLists | diagnostics (printed in CMake summary) | `<dir>/bin/verilator_bin` or empty |
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
| Verilator build fails | `cmake --build build --target verilator` fails; `perf_tests` ctest entry shows UNSUPPORTED with `skip_reason: "verilator build failed"` |
| Verilator built but binary doesn't run | `is_available()` (W5-fixed) returns false → `UNSUPPORTED` |
| Developer sets `-DBUILD_VERILATOR=OFF` | `CPPHDL_VERILATOR_WRAPPER` is empty; `perf_tests` ctest entry falls back to `--verilator=verilator` (PATH lookup) → existing UNSUPPORTED behavior preserved |
| `verilator --build` reports error | VerilatorRunner reports `br.success=false` → `SKIPPED` (W5) |

## 8. Testing

### Smoke test

```bash
# After applying this spec:
git clone <CppHDL>
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target verilator
cmake --build build -j$(nproc)
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
- SKIPs (not FAILs) if `CPPHDL_VERILATOR_DIR` is empty or the
  verilator binary is missing.
- This test catches the "I broke the verilator path" regression
  class without depending on the heavy full perf run.

### Manual verification checklist

- [ ] `git submodule status` shows `third_party/verilator` at the pinned SHA with `+` prefix (clean)
- [ ] `cmake --build build --target verilator` completes
- [ ] `./build/verilator-install/bin/verilator_bin --version` prints a version string
- [ ] `./build/verilator-install/bin/verilator --version` prints a version string (with VERILATOR_ROOT correctly set)
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

> **⚠️ First-time build time: 10–30 minutes on a 4-core x86_64
> machine.** Subsequent incremental builds: seconds. CI should
> cache `build/verilator-install/` to amortize the cold cost.

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

First-time Verilator build on a 4-core x86_64: **10–30 minutes**.
Subsequent incremental builds: seconds (only the changed
`verilator` sources recompile, link step ~5 seconds).

### Cache strategy

The Verilator build is hermetic and produces deterministic output
(git SHA + CMake cache). CI can cache
`build/verilator-install/` across jobs to amortize the cold build
cost.

### Skip-Verilator escape hatch

If a developer cannot wait 30 minutes, or their build env lacks
`flex`/`bison`/`perl`, they can opt out:

```bash
cmake -B build -DBUILD_VERILATOR=OFF
cmake --build build    # builds CppHDL only
./build/tests/benchmark/perf_tests --tc=07
# Verilator column will show UNSUPPORTED, exactly as before this spec
```

This is a documented design feature, not a workaround.

## 11. Evidence trail (oracle review v1 and v2)

For future maintainers wondering "why is the spec so specific about
the install wrapper path", here is the chain of evidence:

### Oracle review v1 (commit `5a18f98`)

Found 3 BLOCKING issues in the `add_subdirectory` approach:

1. **CMake target name.** Upstream `src/CMakeLists.txt` line 286:
   `set(verilator verilator${CMAKE_BUILD_TYPE})`. The v1 spec used
   `set_target_properties(verilator ...)` which would target a
   non-existent target.

2. **`VERILATOR_ROOT` setup.** Upstream `bin/verilator` line 92:
   `my $verilator_root = realpath("$RealBin/$verilator_pkgdatadir_relpath")`.
   Wrapper derives root from `$RealBin/..`. With `add_subdirectory`,
   the binary lands in the build tree but the wrapper is in the
   source tree → wrong root.

3. **Wrapper/binary co-location.** Upstream `bin/verilator` line 221:
   `if (-x "$RealBin/$basename" || -x "$RealBin/$basename.exe")`. Wrapper
   looks for binary next to itself. With `add_subdirectory`, the
   binary is not next to the wrapper.

### Oracle review v2 (commit `f870ca9`)

Found 2 new BLOCKING issues in the "source-tree wrapper + VERILATOR_BIN
env var" approach:

1. **Factually wrong claim.** v2 spec said the upstream wrapper is
   not auto-installed. In fact, upstream `CMakeLists.txt` line 156
   loops over `(verilator, verilator_gantt, verilator_ccache_report,
   verilator_difftree, verilator_profcfunc, verilator_includer)` and
   line 164 does `install(PROGRAMS bin/${program} TYPE BIN)` for
   each. The wrapper IS auto-installed.

2. **VERILATOR_ROOT inconsistency check.** Upstream `bin/verilator`
   lines 95–100 enforce that if `$ENV{VERILATOR_ROOT}` is set, it
   must match `realpath("$RealBin/..")`. Pointing `--verilator` at
   the source-tree wrapper while setting `VERILATOR_BIN` to the
   install-tree binary is internally inconsistent: the wrapper's
   `$RealBin/..` is the source root, but the binary is in the
   install tree. The generated Makefile in `obj_dir/` looks for
   `$VERILATOR_ROOT/include/verilated.mk` (the `configure_file()`
   output), which only exists in the build tree and the install
   tree — not in the source tree.

### v3 resolution

Use the **installed** wrapper. The install step places the wrapper
and the binary in the same directory (`<prefix>/bin/`), matching
what the wrapper's `$RealBin/$basename` check expects, and
`$RealBin/..` is the install prefix which contains the configured
data files. No env var needed.

## 12. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Verilator build takes > 30 min on slow CI | Slow CI feedback | Add CI cache for `build/verilator-install/`. Document `EXPECTED_BUILD_TIME` in DEVELOPING.md. |
| Verilator submodule init fails (network/auth) | Submodule empty; CppHDL configure fails | Fast-fail at configure time with clear message; document `git submodule update --init --recursive` in README |
| Verilator 5.x breaks compat with existing harness | All Verilator rows FAIL or crash | Integration test (§8) catches this in CI before merge. If a fix is needed, upstream patch first. |
| flex/bison missing on dev machine | Verilator build fails | Document install commands in `verilator-integration.md`; CMake warning at configure time |
| Verilator install layout changes between versions | Wrapper no longer at expected location | The install layout (`bin/verilator` next to `bin/verilator_bin`) is core to how the wrapper itself works; if upstream changes it, they have to update the wrapper too. We're insulated by always using the install layout, not relying on a specific filesystem arrangement. |
| `VERILATOR_ROOT` not set | Generated build fails | Wrapper auto-sets `VERILATOR_ROOT` from `$RealBin/..` (verified by reading `bin/verilator` line 92) |
| `perl` missing | Wrapper cannot execute | `find_program(PERL perl REQUIRED)` at configure time |
| Verilator upstream drops support for install layout (extremely unlikely) | Wrapper fails to find binary | Would require major upstream redesign; not a near-term risk. If it happens, we fall back to `VERILATOR_BIN` env var injection. |

## 13. Open questions

None at this point. The user has confirmed:
- Strategy: full submodule + self-build
- Version: latest 5.x stable
- CMake integration: build in-tree (initial plan was
  `add_subdirectory`; revised to `ExternalProject_Add` per oracle
  review, then to use the installed wrapper per second oracle
  review)

Submodule pin (exact tag) will be decided at implementation time by
running `git ls-remote --tags` and picking the highest 5.x stable.
