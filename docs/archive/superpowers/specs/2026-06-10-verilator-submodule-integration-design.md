# Design: Verilator Submodule + End-to-End Three-Way Perf Comparison

**Date:** 2026-06-10
**Status:** v4 — final, pending user approval
**Author:** Sisyphus (brainstorming session)

## Revision history

- **v1 (commit `5a18f98`, reverted):** `add_subdirectory`. Oracle
  found 3 BLOCKING: wrong target name, broken VERILATOR_ROOT, wrapper/
  binary co-location.
- **v2 (commit `f870ca9`, reverted):** `ExternalProject_Add` +
  source-tree wrapper + `VERILATOR_BIN` env var. Oracle found 2
  BLOCKING: false claim about wrapper install, source-tree wrapper's
  `$RealBin/..` points at source root which lacks configured data.
- **v3 (commit `aaa9f89`, reverted):** `ExternalProject_Add` +
  installed wrapper, single-path. Oracle review found 3 remaining
  issues:
  1. Spec said `${prefix}/share/verilator` for VERILATOR_ROOT, but
     the actual install puts `include/verilated_config.h` and
     `include/verilated.mk` at `${prefix}/include/` (verified
     against upstream `CMakeLists.txt:175`). So `VERILATOR_ROOT`
     must be exactly `${prefix}`.
  2. flex/bison should be `REQUIRED`, not just a warning. Upstream
     does `find_package(BISON)` (not REQUIRED); missing tools
     yield silent garbage in `add_custom_command` with empty
     `${BISON_EXECUTABLE}` and the build dies late with cryptic
     `/bin/sh: -d: not found`.
  3. The upstream wrapper does set `VERILATOR_ROOT` (via
     `bin/verilator:101` `$ENV{VERILATOR_ROOT} = $verilator_root;`
     before `system()` on line 199), so the `VERILATOR_ROOT` is
     effectively inherited by the `verilator_bin` subprocess
     started via `popen` from `verilator_runner.h:259`.
     But the v3 spec overstated this as "wrapper auto-sets" without
     being precise about the propagation chain.

- **v4 (current):** Same single-path approach as v3, with three
  precise fixes:
  1. `VERILATOR_ROOT` path: `${prefix}` (NOT `${prefix}/share/verilator`).
  2. flex/bison: `find_program(... REQUIRED)` (with a graceful
     `BUILD_VERILATOR=OFF` fallback).
  3. Explicit `ENVIRONMENT "VERILATOR_ROOT=${CPPHDL_VERILATOR_DIR}"`
     in the ctest test properties as a belt-and-suspenders injection
     — even though the wrapper sets it for its child `verilator_bin`,
     explicit injection ensures the chain works on all paths
     (e.g. if a future `verilator_runner` invocation is ever changed
     to bypass the wrapper and call `verilator_bin` directly).

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
`ExternalProject_Add` + `--prefix`, so `cmake --build build --target
verilator` produces a working Verilator install
(`<install>/bin/verilator` + `<install>/bin/verilator_bin` +
`<install>/include/verilated_*.{h,mk}`) and `perf_tests` can exercise
the real three-way comparison.

## 2. Goals

1. **Reproducibility.** Pin a known-good Verilator version (5.x stable)
   as a submodule so every developer and CI worker gets the same version.
2. **End-to-end build.** `cmake --build build --target verilator`
   builds Verilator; `cmake --build build` builds CppHDL. Both
   invocations are needed; no manual `apt install verilator` step.
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
  produced by Verilator's own `cmake --install --prefix` step. The
  relevant contents (verified against upstream `CMakeLists.txt`):
  - `bin/verilator` — Perl wrapper (installed by
    `install(PROGRAMS bin/${program} TYPE BIN)` for `program` in
    `verilator, verilator_gantt, verilator_ccache_report, ...`).
  - `bin/verilator_bin` — compiled binary (installed by
    `install(TARGETS ${verilator})`).
  - `include/verilated_config.h` and `include/verilated.mk` — installed
    by `install(FILES ... DESTINATION include)`.
  - `include/verilated_*.h`, `include/vl/*` — installed by
    `install(DIRECTORY include TYPE DATA ...)` (filtering via
    `FILES_MATCHING`).

  After install, the wrapper and binary are **siblings** in
  `${prefix}/bin/`. `VERILATOR_ROOT` resolves to `${prefix}` (not
  `${prefix}/share/verilator` — that path does not exist in a CMake
  build; the `verilated.mk` file lives directly under
  `${prefix}/include/`).

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
  `add_test(NAME perf_tests ...)` command, and sets the
  `ENVIRONMENT` property to inject
  `VERILATOR_ROOT=${CPPHDL_VERILATOR_DIR}` for belt-and-suspenders
  correctness (see §11 for why we inject even though the wrapper
  does it for its child).

### Data flow at build time

```
git clone <CppHDL>
  ↓
git submodule update --init --recursive
  ↓
cmake -B build
  ↓
  option(BUILD_VERILATOR) = ON (default)
  find_program(PERL perl REQUIRED)       → fails fast if missing
  find_program(FLEX flex REQUIRED)       → fails fast if missing
  find_program(BISON bison REQUIRED)     → fails fast if missing
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
    <install>/bin/verilator           (Perl wrapper)
    <install>/bin/verilator_bin       (binary)
    <install>/include/verilated_*.h, verilated.mk (configured files)
cmake --build build              # builds CppHDL
  ↓
ctest -L perf
  ↓
  perf_tests invoked with:
    --verilator=<install>/bin/verilator
    ENV{VERILATOR_ROOT} = <install>/
  ↓
  Perl wrapper at <install>/bin/verilator executes
  wrapper sets ENV{VERILATOR_ROOT} = realpath($RealBin/..) = <install>/
  wrapper finds verilator_bin at <install>/bin/verilator_bin (sibling)
  wrapper system()s verilator_bin with ENV{VERILATOR_ROOT} inherited
  verilator_bin generates obj_dir/Makefile with:
    include $(VERILATOR_ROOT)/include/verilated.mk   # RESOLVES
  verilator --build compiles, links, produces Vtop + binary
  → Three-way PASS row emitted
```

### Data flow at perf-test runtime

```
perf_tests --verilator=<install>/bin/verilator
  (with ENV{VERILATOR_ROOT}=<install>/ set by ctest)
  ↓
VerilatorRunner(<install>/bin/verilator)
  ↓
  is_available():
    popen("<install>/bin/verilator --version 2>&1")
    exit 0, output is "Verilator 5.x..." → returns true
  ↓
  build(test_name, verilog, harness):
    shell_exec("<install>/bin/verilator --build --exe harness.cpp top.v")
    → wrapper auto-sets VERILATOR_ROOT=$RealBin/.. for child
    → child verilator_bin finds $VERILATOR_ROOT/include/verilated.mk
    → Verilator's own internal g++/make produces ./obj_dir/Vtop + binary
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
  resolves to `${prefix}` (the install prefix).
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
- `VERILATOR_ROOT` (set by the wrapper from `$RealBin/..`) resolves
  to `${prefix}`, which contains the configured data files
  (`include/verilated_config.h`, `include/verilated.mk`) that the
  generated Makefile in `obj_dir/` needs.
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

### Decision 5: `--verilator` points at the **installed** wrapper; inject `VERILATOR_ROOT` via ctest ENVIRONMENT

**Choice:** The `perf_tests` executable is invoked with
`--verilator=${CPPHDL_VERILATOR_DIR}/bin/verilator` (the wrapper
installed by Verilator's own install step, sitting next to
`verilator_bin`). The ctest test entry ALSO sets
`ENVIRONMENT "VERILATOR_ROOT=${CPPHDL_VERILATOR_DIR}"` for
defense in depth.

**Rationale:**
- The wrapper at `<install>/bin/verilator` (a) sets `VERILATOR_ROOT`
  from `$RealBin/..` = the install prefix for its own child
  `verilator_bin` process (verified by reading upstream
  `bin/verilator:101`), and (b) finds `verilator_bin` as a sibling
  via the default `$RealBin/$basename` lookup. So the wrapper
  alone is sufficient.
- **However**, the propagation chain is non-obvious: the wrapper
  writes to Perl's `%ENV` and then `system()`s `verilator_bin`; if
  `verilator_runner.h` were ever changed to call `verilator_bin`
  directly (bypassing the wrapper), the `VERILATOR_ROOT` setup
  would be lost. The ctest `ENVIRONMENT` injection is
  belt-and-suspenders against future code changes. The cost is
  zero (one env var per test invocation).
- `VERILATOR_ROOT` is `${CPPHDL_VERILATOR_DIR}` (NOT
  `${prefix}/share/verilator` — that path does not exist in a CMake
  install; the `verilated.mk` file is at `${prefix}/include/`).
- `perf_main.cpp`'s default of looking up `verilator` on `PATH` is
  preserved for manual invocations (developer can install Verilator
  system-wide and run `perf_tests` without rebuilding).

### Decision 6: Host build dependencies are REQUIRED (not warnings)

**Choice:** At CMake configure time, before declaring the
ExternalProject:

```cmake
if(BUILD_VERILATOR)
    find_program(PERL perl REQUIRED)
    find_program(FLEX flex REQUIRED)
    find_program(BISON bison REQUIRED)
endif()
```

**Rationale (corrected from v3):**
- v3 made `flex` and `bison` only warnings. That was wrong.
- Upstream's `CMakeLists.txt` does `find_package(BISON)` and
  `find_package(FLEX)` **without** `REQUIRED`. When these are
  missing, `BISON_EXECUTABLE` / `FLEX_EXECUTABLE` are empty strings.
  The subsequent `add_custom_command(... COMMAND ${BISON_EXECUTABLE} ...)`
  produces a silent broken command that fails late with `/bin/sh: -d: not
  found` or a missing-`V3ParseBison.c` error — NOT a clear "install
  bison" message.
- By making them REQUIRED in our outer CMake, we get a clear
  `CMake Error: required flex not found` at configure time,
  which saves a debug session.
- We also REQUIRE `perl` because `bin/verilator` is a Perl script
  and the wrapper cannot execute without it.
- When `BUILD_VERILATOR=OFF`, none of the three checks run
  (`if(BUILD_VERILATOR)` guards the entire block), so a developer
  who doesn't want Verilator can still build CppHDL on a system
  without flex/bison/perl.

### Decision 7: Document build dependencies (perl + flex + bison)

**Choice:** `docs/developer_guide/verilator-integration.md` lists
Verilator's host build dependencies:

- `flex`, `bison` (REQUIRED for Verilator's code generation; see
  Decision 6)
- `g++` ≥ 7 (or clang equivalent) — for Verilator's own compilation
- `make` (or ninja)
- `perl` ≥ 5.6.1 (REQUIRED — `bin/verilator` is a Perl script; declared
  in upstream `bin/verilator:12`: `require 5.006_001;`)
- `python3` ≥ 3.6 (used by Verilator's code generation scripts)
- `autoconf`, `m4`, `help2man`, `c++filt`, `libsystemd-dev`
  (optional but recommended)

Install commands:
- Ubuntu: `sudo apt install flex bison build-essential python3 perl autoconf help2man ccache libfl2 libfl-dev zlibc liblzma-dev libsystemd-dev`
- macOS: `brew install flex bison python3 gperftools readline ccache help2man`

### Decision 8: Two-step build recipe in §10

**Choice:** Document the exact two-step build recipe (Decision 4 also
notes that `cmake --build build` alone does NOT build Verilator —
this is a subtle point).

**Rationale:** ExternalProject_Add is `EXCLUDE_FROM_ALL` by default.
The developer must explicitly invoke the `verilator` target. Without
this document note, developers will run `cmake --build build` and
wonder why Verilator isn't built.

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
| `CPPHDL_VERILATOR_DIR` | CppHDL top-level CMakeLists (after ExternalProject) | diagnostics, ctest ENVIRONMENT | Install path or empty |
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
| `perl`/`flex`/`bison` missing | CMake configure aborts with `find_program(... REQUIRED)` |
| Verilator build fails | `cmake --build build --target verilator` fails; `perf_tests` ctest entry shows UNSUPPORTED with `skip_reason: "verilator build failed"` |
| Verilator built but binary doesn't run | `is_available()` (W5-fixed) returns false → `UNSUPPORTED` |
| Developer sets `-DBUILD_VERILATOR=OFF` | `CPPHDL_VERILATOR_WRAPPER` is empty; `perf_tests` ctest entry falls back to `--verilator=verilator` (PATH lookup). ctest entry uses an `if(CPPHDL_VERILATOR_WRAPPER)` guard so the `--verilator=` flag is only appended when non-empty. → existing UNSUPPORTED behavior preserved |
| `verilator --build` reports error | VerilatorRunner reports `br.success=false` → `SKIPPED` (W5) |
| `VERILATOR_ROOT` mismatch (defense in depth) | ctest ENVIRONMENT sets `VERILATOR_ROOT=${CPPHDL_VERILATOR_DIR}` so even if the wrapper's propagation chain is broken (e.g. future code change bypasses the wrapper), the env var is set explicitly for the test process and its children. |

## 8. Testing

### Smoke test

```bash
git clone <CppHDL>
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target verilator
cmake --build build -j$(nproc)
./build/tests/benchmark/perf_tests --tc=07 --warmup=3 --measured=5 --report=json
# Expect: 3 rows per depth (interpreter/jit/verilator), all status=PASS
#         for depth=10/100, verilator row at depth=1000 may be slow but
#         PASS
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
  class without dependendo on the heavy full perf run.

### Manual verification checklist

- [ ] `git submodule status` shows `third_party/verilator` at the pinned SHA with `+` prefix (clean)
- [ ] `cmake --build build --target verilator` completes
- [ ] `./build/verilator-install/bin/verilator_bin --version` prints a version string
- [ ] `./build/verilator-install/bin/verilator --version` prints a version string
- [ ] `ctest -L perf` passes
- [ ] `perf_tests --tc=07 --report=json | jq '.runs[] | select(.backend=="verilator") | .status'` shows `"PASS"` for at least one row

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

### Build recipe (two steps!)

```bash
# Step 1: build Verilator (slow, ~10-30 min cold)
cmake --build build -j$(nproc) --target verilator

# Step 2: build CppHDL artifacts (fast)
cmake --build build -j$(nproc)

# Combined: build both in one command
cmake --build build -j$(nproc) --target verilator && \
    cmake --build build -j$(nproc)
```

`cmake --build build` (no target) only builds CppHDL; Verilator is
`EXCLUDE_FROM_ALL` by default for `ExternalProject_Add` and must be
targeted explicitly.

### CI changes needed

GitHub Actions (or équivalent) must enable submodule checkout:

```yaml
- uses: actions/checkout@v4
  with:
    submodules: true        # ← this is the change
    submodules: recursive
```

### Disk space

Verilator's build tree consumes ~1–2 GB during build. Final install
plus headers is ~50 MB. CI runners should have ≥ 5 GB free.

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

## 11. Evidence trail (oracle review v1, v2, v3)

For future maintainers wondering "why is the spec so specific about
the install wrapper path and the env var injection", here is the
chain of evidence.

### Upstream `bin/verilator` key lines (v5.020 master, June 2026)

```
90: # WARNING: $verilator_pkgdatadir_relpath is substituted during Verilator 'make install'
91: my $verilator_pkgdatadir_relpath = "..";
92: my $verilator_root = realpath("$RealBin/$verilator_pkgdatadir_relpath");
93: if (defined $ENV{VERILATOR_ROOT}) {
94:     if ((!-d $ENV{VERILATOR_ROOT}) || $verilator_root ne realpath($ENV{VERILATOR_ROOT})) {
95:         warn "%Error: verilator: VERILATOR_ROOT is set to inconsistent path. Suggest leaving it unset.\n";
96:         warn "%Error: VERILATOR_ROOT=$ENV{VERILATOR_ROOT}\n";
97:         exit 1;
98:     }
99: } else {
100:     print "export VERILATOR_ROOT='$verilator_root'\n" if $Debug;
101:     $ENV{VERILATOR_ROOT} = $verilator_root;
102: }
...
199: run(ulimit_stack_unlimited() . aslr(1) . verilator_bin() . " " . join(' ', @quoted_sw));
```

**Reading:**
- Line 92: `$verilator_root` = realpath of `..` from `$RealBin`. When
  the wrapper is at `<install>/bin/verilator`, `$RealBin` is
  `<install>/bin/`, so `$verilator_root` = `<install>/`.
- Lines 93-98: Consistency check. If `VERILATOR_ROOT` is set in
  the environment, it must equal `$verilator_root`. We must set it
  to `<install>/` consistently with `$verilator_root`.
- Lines 99-102: When `VERILATOR_ROOT` is NOT in env, set it
  internally to `$verilator_root`. When the wrapper `system()`s
  `verilator_bin` (line 199), the child inherits this via
  Perl's `%ENV`.
- **This means the wrapper does propagate `VERILATOR_ROOT` to
  its child `verilator_bin`.** But the chain depends on
  `popen()`/`system()` inheriting env, which is true for
  `verilator_runner.h:259` and `popen()` itself. The ctest
  ENVIRONMENT injection is defense in depth.

### Upstream `CMakeLists.txt` install rules (v5.020 master)

```
155: foreach(
156:     program
157:     verilator
158:     verilator_gantt
...
164:     install(PROGRAMS bin/${program} TYPE BIN)
165: endforeach()
...
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/verilated_config.h DESTINATION include)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/verilated.mk DESTINATION include)
install(DIRECTORY include TYPE DATA FILES_MATCHING PATTERN "include/verilated_config.h" ...)
```

**Reading:**
- Lines 155-165 install 6 programs including `verilator` (the wrapper)
  to `<prefix>/bin/`. So the wrapper IS auto-installed (v2 was wrong to
  claim otherwise; v3 corrected this).
- The configured data files (`verilated_config.h`, `verilated.mk`)
  install to `<prefix>/include/` (NOT `<prefix>/share/verilator/`).
- The third install copies the rest of the `include/` tree (excluding
  the `.in` templates) to `<prefix>/include/`.
- So after install, `VERILATOR_ROOT` (which is the wrapper's
  `$RealBin/..` = `<prefix>/`) has `<prefix>/include/verilated.mk`
  exactly where the generated Makefile expects it
  (`include $(VERILATOR_ROOT)/include/verilated.mk` in
  `V3EmitMk.cpp`).

### Oracle review history

- **v1 (commit 5a18f98):** 3 BLOCKING (target name, VERILATOR_ROOT,
  wrapper/binary co-location). All fixed by switching to
  `ExternalProject_Add + --prefix`.
- **v2 (commit f870ca9):** 2 BLOCKING (false "wrapper not installed"
  claim, source-tree wrapper's `$RealBin/..` lacks configured data).
  All fixed by switching to installed-wrapper single-path approach.
- **v3 (commit aaa9f89):** 3 issues:
  1. `VERILATOR_ROOT = ${prefix}/share/verilator` was wrong; actual
     install puts files at `${prefix}/include/`. So
     `VERILATOR_ROOT` must be exactly `${prefix}`.
  2. `flex`/`bison` should be `REQUIRED`, not just warnings.
  3. The v3 spec overclaimed about the wrapper's
     `VERILATOR_ROOT` propagation. We now add explicit ctest
     ENVIRONMENT injection as belt-and-suspenders.

  All three addressed in v4.

## 12. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Verilator build takes > 30 min on slow CI | Slow CI feedback | Add CI cache for `build/verilator-install/`. Document `EXPECTED_BUILD_TIME` in DEVELOPING.md. |
| Verilator submodule init fails (network/auth) | Submodule empty; CppHDL configure fails | Fast-fail at configure time with clear message; document `git submodule update --init --recursive` in README |
| Verilator 5.x breaks compat with existing harness | All Verilator rows FAIL or crash | Integration test (§8) catches this in CI before merge. If a fix is needed, upstream patch first. |
| flex/bison/perl missing on dev machine | CppHDL configure aborts (REQUIRED) | Document install commands in `verilator-integration.md`; skip-Verilator via `BUILD_VERILATOR=OFF` |
| Verilator install layout changes between versions | Wrapper no longer at expected location | The install layout (`bin/verilator` next to `bin/verilator_bin`) is core to how the wrapper itself works; if upstream changes it, they have to update the wrapper too. We're insulated by always using the install layout, not relying on a specific filesystem arrangement. |
| `VERILATOR_ROOT` not set | Generated Makefile fails | (a) Wrapper auto-sets it for child process; (b) ctest ENVIRONMENT injects it explicitly as defense in depth |
| `perl` missing | Wrapper cannot execute | `find_program(PERL perl REQUIRED)` at configure time |
| Verilator upstream drops support for install layout (extremely unlikely) | Wrapper fails to find binary | Would require major upstream redesign; not a near-term risk. If it happens, we fall back to `VERILATOR_BIN` env var injection. |

## 13. Open questions

None at this point. The user has confirmed:
- Strategy: full submodule + self-build
- Version: latest 5.x stable
- CMake integration: build in-tree (initial plan was
  `add_subdirectory`; revised to `ExternalProject_Add` per oracle
  review, then to use the installed wrapper per second oracle
  review, then to add the `VERILATOR_ROOT` env var injection per
  third oracle review)

Submodule pin (exact tag) will be decided at implementation time by
running `git ls-remote --tags` and picking the highest 5.x stable.
