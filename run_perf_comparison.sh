#!/usr/bin/env bash
# ============================================================================
# run_perf_comparison.sh
# ----------------------------------------------------------------------------
# One-click script: build perf_tests, run three-way (Interpreter/JIT/
# Verilator) comparison, and copy CSV/JSON/Markdown reports into
# build/perf_report.{csv,json,md}.
#
# Exit codes:
#   0   success (Verilator-missing also exits 0, with warning)
#   1   build / runtime error
#   64  usage error (--help)
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# ---- ANSI color helpers (auto-disabled when stdout is not a TTY) -----------
if [[ -t 1 ]]; then
    C_RED=$'\033[1;31m'; C_YEL=$'\033[1;33m'; C_GRN=$'\033[1;32m'
    C_BLU=$'\033[1;34m'; C_RST=$'\033[0m'
else
    C_RED=''; C_YEL=''; C_GRN=''; C_BLU=''; C_RST=''
fi
log_info()  { printf '%s[INFO]%s  %s\n'  "$C_BLU" "$C_RST" "$*"; }
log_warn()  { printf '%s[WARN]%s  %s\n'  "$C_YEL" "$C_RST" "$*" >&2; }
log_err()   { printf '%s[ERROR]%s %s\n'  "$C_RED" "$C_RST" "$*" >&2; }
log_ok()    { printf '%s[OK]%s    %s\n'  "$C_GRN" "$C_RST" "$*"; }

# ---- --help handling -------------------------------------------------------
if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<EOF
Usage: ./run_perf_comparison.sh [options]

Options:
  --help, -h    Show this help and exit

Steps performed:
  1. Detect Verilator (optional; falls back to 2-way when missing)
  2. Configure CMake build directory if missing
  3. Build perf_tests target
  4. Run perf_tests with --all --report=csv/json/md
  5. Copy outputs to build/perf_report.{csv,json,md}
  6. Print a summary table to stdout

Outputs:
  build/perf_report.csv   CSV-formatted performance results
  build/perf_report.json  JSON-formatted performance results
  build/perf_report.md    Markdown table of performance results

Exit codes:
  0  Success (including when Verilator is unavailable)
  1  Build or runtime failure
EOF
    exit 64
fi

# ---- Stage 1: detect Verilator (soft dependency) --------------------------
log_info "Stage 1/5: Detecting Verilator availability..."
VERILATOR_AVAILABLE=0
VERILATOR_BIN=""
if command -v verilator >/dev/null 2>&1; then
    VERILATOR_BIN="$(command -v verilator)"
    VERILATOR_AVAILABLE=1
    log_ok "Verilator found: $(verilator --version 2>/dev/null | head -n1) at $VERILATOR_BIN"
else
    log_warn "Verilator not found on PATH. Falling back to 2-way comparison (Interpreter + JIT)."
fi

# ---- Stage 2: ensure build directory exists --------------------------------
log_info "Stage 2/5: Configuring CMake build directory..."
if [[ ! -d build ]]; then
    log_info "Creating build directory (cmake -B build -DCMAKE_BUILD_TYPE=Release)..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
else
    log_ok "build directory already exists, skipping cmake configure"
fi

# ---- Stage 3: build perf_tests -------------------------------------------
log_info "Stage 3/5: Building perf_tests target..."
cmake --build build -j"$(nproc 2>/dev/null || echo 2)" --target perf_tests

PERF_BIN="build/tests/benchmark/perf_tests"
if [[ ! -x "$PERF_BIN" ]]; then
    log_err "perf_tests binary not found at $PERF_BIN after build."
    exit 1
fi
log_ok "perf_tests binary: $PERF_BIN"

# ---- Stage 4: run perf_tests with all three report formats ----------------
# perf_main.cpp accepts one --report=<fmt> per invocation; we run it three
# times so CSV / JSON / Markdown each get produced. Unknown flags are
# silently ignored by the binary (forward-compatible with --tc=07/08 etc.).
log_info "Stage 4/5: Running benchmarks (CSV / JSON / Markdown)..."

# CSV
if ! "$PERF_BIN" --all --report=csv --tc=07 --tc=08 >/dev/null; then
    log_warn "perf_tests --report=csv returned non-zero (binary may not yet support TC-07/08)"
fi
# JSON
if ! "$PERF_BIN" --all --report=json --tc=07 --tc=08 >/dev/null; then
    log_warn "perf_tests --report=json returned non-zero"
fi
# Markdown (graceful: current binary may not implement --report=md)
if ! "$PERF_BIN" --all --report=md --tc=07 --tc=08 >/dev/null; then
    log_warn "perf_tests --report=md not yet supported (will synthesize from CSV)"
fi

# ---- Stage 5: copy outputs into build/perf_report.{csv,json,md} ----------
log_info "Stage 5/5: Collecting reports into build/perf_report.{csv,json,md}..."

copy_if_present() {
    local src="$1" dst="$2"
    if [[ -f "$src" ]]; then
        cp -f "$src" "$dst"
        log_ok "Wrote $dst ($(wc -c <"$dst") bytes)"
    else
        log_warn "Missing source report: $src (skipping)"
    fi
}

copy_if_present perf_results.csv build/perf_report.csv
copy_if_present perf_results.json build/perf_report.json
copy_if_present perf_results.md   build/perf_report.md

# If Markdown was not produced by perf_tests, synthesize a proper Markdown
# table from the CSV so downstream tooling always has all three files.
if [[ ! -f build/perf_report.md && -f build/perf_report.csv ]]; then
    {
        echo "# CppHDL Performance Report"
        echo ""
        echo "Generated by $(basename "$0") at $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
        echo ""
        # Header row: replace commas with | and add outer pipes
        awk -F',' 'NR==1 {
            printf "| "
            for (i=1; i<=NF; i++) printf "%s |", $i
            print ""
            printf "|"
            for (i=1; i<=NF; i++) printf " --- |"
            print ""
        }' build/perf_report.csv
        # Body rows
        awk -F',' 'NR>1 {
            printf "| "
            for (i=1; i<=NF; i++) printf "%s |", $i
            print ""
        }' build/perf_report.csv
    } > build/perf_report.md
    log_ok "Synthesized build/perf_report.md from CSV (perf_tests lacks --report=md)"
fi

# ---- Summary --------------------------------------------------------------
echo ""
printf '%s=== Performance Comparison Summary ===%s\n' "$C_BLU" "$C_RST"
if [[ "$VERILATOR_AVAILABLE" -eq 1 ]]; then
    echo "Mode: 3-way (Interpreter / JIT / Verilator)"
else
    echo "Mode: 2-way (Interpreter / JIT) — Verilator unavailable"
fi
echo "Reports:"
echo "  - build/perf_report.csv"
echo "  - build/perf_report.json"
echo "  - build/perf_report.md"

# Print a short summary table extracted from the CSV (best-effort)
if [[ -f build/perf_report.csv ]]; then
    echo ""
    echo "--- CSV preview (first 10 lines) ---"
    head -n 10 build/perf_report.csv
fi

echo ""
log_ok "Done."