# Verilator Integration

CppHDL can build and link against the Verilator cycle-accurate simulator
through a git submodule, so three-way performance comparisons
(interpreter / JIT / Verilator) run end-to-end with no manual `apt
install verilator` step.

## Quick start

After a fresh clone:

```bash
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target verilator   # 10-30 min cold
cmake --build build -j$(nproc)
./build/tests/benchmark/perf_tests --tc=07 --report=json
```

The Verilator column in the resulting `perf_results.json` will show
`status: "PASS"` rows instead of `UNSUPPORTED`.

If you cloned without submodules, `./scripts/init-submodules.sh` is
a convenience wrapper that runs the init + a sanity check.

## Host build dependencies

Verilator's build requires the following on the host system:

| Tool    | Required | Why |
|---------|----------|-----|
| `flex`  | YES      | Verilator's CMake uses bison+flex for parser generation |
| `bison` | YES      | Same as flex |
| `perl` ≥ 5.6.1 | YES | The wrapper at `bin/verilator` is a Perl script |
| `g++` ≥ 7      | YES | Compile Verilator's own sources |
| `make`         | YES | (or ninja) |
| `python3` ≥ 3.6 | YES | Verilator's code generation scripts |
| `autoconf`     | optional | Required for some optional features |
| `help2man`     | optional | For man page generation |
| `libsystemd-dev` | optional | For sd_notify support |

CMake's `find_program(... REQUIRED)` aborts the configure step at
the first missing tool with a clear error message. To skip the
Verilator build entirely (e.g. on a constrained CI runner), pass
`-DBUILD_VERILATOR=OFF`.

### Ubuntu

```bash
sudo apt install flex bison build-essential python3 perl \
                autoconf help2man ccache \
                libfl2 libfl-dev zlibc liblzma-dev libsystemd-dev
```

### macOS

```bash
brew install flex bison python3 gperftools readline ccache help2man
```

## Build recipe

```bash
# Step 1: build Verilator (slow, ~10-30 min cold; seconds warm)
cmake --build build -j$(nproc) --target verilator

# Step 2: build CppHDL (fast)
cmake --build build -j$(nproc)

# Combined: both at once
cmake --build build -j$(nproc) --target verilator && \
    cmake --build build -j$(nproc)
```

`cmake --build build` (no target) only builds CppHDL artifacts —
Verilator is `EXCLUDE_FROM_ALL` for `ExternalProject_Add` and must
be targeted explicitly. This avoids re-walking Verilator's
dependency graph on every incremental build.

## Where things live

| Path | Purpose |
|---|---|
| `third_party/verilator/` | Pinned submodule source (read-only) |
| `build/verilator-build/` | Verilator's own CMake out-of-tree build dir |
| `build/verilator-install/` | Verilator's install prefix — what we use at runtime |
| `build/verilator-install/bin/verilator` | Perl wrapper (what `--verilator=` points at) |
| `build/verilator-install/bin/verilator_bin` | C++ binary (invoked by the wrapper) |
| `build/verilator-install/include/` | Configured headers + `verilated.mk` (VerilatorRoot) |

`VERILATOR_ROOT` (set automatically by the wrapper from
`$RealBin/..`, AND explicitly injected by the ctest test
properties as belt-and-suspenders) is `${CMAKE_BINARY_DIR}/verilator-install`.

## Skipping Verilator

If you don't want to build Verilator (e.g. quick local iteration,
no time for the 30-min cold build, or missing host tools):

```bash
cmake -B build -DBUILD_VERILATOR=OFF
cmake --build build
./build/tests/benchmark/perf_tests --tc=07
```

The Verilator column in the report will show `UNSUPPORTED` with
`skip_reason: "verilator not found on PATH"`, exactly as it did
before this integration.

## Re-pin to a newer Verilator

```bash
git ls-remote --tags https://github.com/verilator/verilator.git | \
    grep -E 'refs/tags/v[5-9]\.[0-9]+(\.[0-9]+)?$' | \
    awk -F/ '{print $NF}' | sort -V | tail -1
# Update the submodule pointer
cd third_party/verilator && git fetch && git checkout <TAG> && cd ../..
# Commit
git add third_party/verilator && git commit -m "build: bump verilator to <TAG>"
# Re-build
rm -rf build/verilator-install
cmake --build build -j$(nproc) --target verilator
```

## Troubleshooting

**`cmake --build build --target verilator` fails with "Perl 5.006_001
required":** Your system's perl is too old. Install a newer perl
(Ubuntu: `sudo apt install perl`, macOS: ships with a recent enough
perl by default).

**Build fails with "sh: 1: bison: not found" or "sh: 1: flex: not
found":** Re-run `cmake -B build` after installing the missing
tool — the `find_program(... REQUIRED)` will now pass.

**`perf_tests` shows verilator rows as `UNSUPPORTED` even after a
successful Verilator build:** Check that
`build/verilator-install/bin/verilator` exists and is executable.
Re-run `cmake -B build` to refresh the cached `CPPHDL_VERILATOR_*`
paths.

**Generated `obj_dir/Makefile` fails with "verilated.mk: No such
file or directory":** `VERILATOR_ROOT` is not set to the install
prefix. The ctest test properties set this automatically; manual
invocations need `VERILATOR_ROOT=<build>/verilator-install`.
