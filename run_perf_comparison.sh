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
  4. Run perf_tests with --tc=07 --tc=08 + --report=csv/json/md
  5. Copy outputs to build/perf_report.{csv,json,md}
  6. Run perf_jit_vs_interp_ratio structural gate (CI-blocking on failure)
  7. Generate JIT-vs-interpreter ratio summary table

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
log_info "Stage 1/7: Detecting Verilator availability..."
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
log_info "Stage 2/7: Configuring CMake build directory..."
if [[ ! -d build ]]; then
    log_info "Creating build directory (cmake -B build -DCMAKE_BUILD_TYPE=Release)..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
else
    log_ok "build directory already exists, skipping cmake configure"
fi

# ---- Stage 3: build perf_tests -------------------------------------------
log_info "Stage 3/7: Building perf_tests target..."
cmake --build build -j"$(nproc 2>/dev/null || echo 2)" --target perf_tests

PERF_BIN="build/tests/benchmark/perf_tests"
if [[ ! -x "$PERF_BIN" ]]; then
    log_err "perf_tests binary not found at $PERF_BIN after build."
    exit 1
fi
log_ok "perf_tests binary: $PERF_BIN"

# ---- Stage 4: run perf_tests with all three report formats ----------------
# perf_main.cpp accepts multiple --report=<fmt> flags per invocation (it
# accumulates them into a vector and writes each format at exit). One
# invocation therefore produces all three report formats in a single
# benchmark run, instead of three sequential runs as before.
#
# We pass --tc=07 --tc=08 explicitly (no --all) to avoid enabling legacy
# single-backend TC-01/02/04/06 which have no JIT-vs-interpreter meaning
# and inflate the runtime beyond the 240s budget.
log_info "Stage 4/7: Running benchmarks (TC-07 + TC-08, three report formats)..."
if ! timeout 240 "$PERF_BIN" --tc=07 --tc=08 \
        --report=csv --report=json --report=md >/dev/null 2>&1; then
    log_warn "perf_tests returned non-zero (likely timeout); partial data may be present"
fi
# perf_main writes perf_results.json via std::ofstream (see
# report_generator.h:114) so its stdout is only progress + summary text
# and we can safely discard it.

# ---- Stage 5: copy outputs into build/perf_report.{csv,json,md} ----------
log_info "Stage 5/7: Collecting reports into build/perf_report.{csv,json,md}..."

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

# ---- Stage 6: JIT-vs-interpreter sanity gate --------------------------------
# W12 (perf-report-followup.md): run perf_jit_vs_interp_ratio as a CI gate.
# This Catch2 test asserts jit_us < interp_us on depth=500 XOR chain (strict)
# and ratio >= 2.0x on depth=1000. Failure here means the JIT has degraded
# to interpreter-class speed and is the regression we explicitly want to
# catch — propagate the non-zero exit so CI fails loudly.
log_info "Stage 6/7: Running JIT-vs-interpreter structural ratio gate (perf_jit_vs_interp_ratio)..."
JIT_RATIO_BIN="build/tests/perf_jit_vs_interp_ratio"
if [[ ! -x "$JIT_RATIO_BIN" ]]; then
    log_warn "perf_jit_vs_interp_ratio binary not found at $JIT_RATIO_BIN; building..."
    cmake --build build -j"$(nproc 2>/dev/null || echo 2)" --target perf_jit_vs_interp_ratio
fi
JIT_RATIO_RC=0
if [[ -x "$JIT_RATIO_BIN" ]]; then
    if "$JIT_RATIO_BIN" --reporter compact 2>&1 | tail -5; then
        log_ok "perf_jit_vs_interp_ratio PASSED (JIT strictly faster than interpreter)"
    else
        JIT_RATIO_RC=$?
        log_err "perf_jit_vs_interp_ratio FAILED (JIT did not beat interpreter; rc=$JIT_RATIO_RC)"
    fi
else
    log_err "perf_jit_vs_interp_ratio still missing after build; marking gate as failed"
    JIT_RATIO_RC=127
fi

# ---- Stage 7: JIT-vs-interpreter ratio summary table -----------------------
# Read perf_results.json (just regenerated) and produce a side-by-side
# ratio table for every (test_name, params) where both interpreter and jit
# rows have status=PASS. Skips rows that are UNSUPPORTED / SKIPPED.
log_info "Stage 7/7: Generating JIT-vs-interpreter ratio summary..."
RATIO_TABLE_PATH="build/perf_report_ratio.md"
if [[ -f perf_results.json ]]; then
    python3 - "$RATIO_TABLE_PATH" <<'PYEOF' || log_warn "Ratio table generation failed"
import json, sys, os
out_path = sys.argv[1]
src_path = "perf_results.json"
with open(src_path) as f:
    data = json.load(f)
runs = data.get("runs", [])
# group by (test_name, params)
from collections import defaultdict
groups = defaultdict(dict)
for r in runs:
    key = (r["test_name"], r["params"])
    groups[key][r["backend"]] = r
lines = []
lines.append("# JIT vs Interpreter Performance Ratio")
lines.append("")
lines.append(f"Generated by run_perf_comparison.sh at {data.get('timestamp', 'unknown')} (git_sha={data.get('git_sha', 'unknown')})")
lines.append("")
lines.append("Ratio = interp_median_us / jit_median_us.  Higher is better (JIT faster).")
lines.append("Only rows with PASS status on both backends are included.")
lines.append("")
lines.append("| Test | Params | Interpreter (μs) | JIT (μs) | Ratio (interp/jit) | Verdict |")
lines.append("| --- | --- | ---: | ---: | ---: | --- |")
verdict_counts = {"JIT_FASTER": 0, "JIT_SLOWER": 0, "N/A": 0}
for key in sorted(groups.keys()):
    g = groups[key]
    interp = g.get("interpreter", {})
    jit = g.get("jit", {})
    interp_us = interp.get("median_us", 0.0)
    jit_us = jit.get("median_us", 0.0)
    interp_status = interp.get("status", "MISSING")
    jit_status = jit.get("status", "MISSING")
    if interp_status == "PASS" and jit_status == "PASS" and jit_us > 0.0:
        ratio = interp_us / jit_us
        if ratio >= 1.5:
            verdict = "JIT_FASTER"
            verdict_counts["JIT_FASTER"] += 1
        elif ratio >= 1.0:
            verdict = "JIT_FASTER (marginal)"
            verdict_counts["JIT_FASTER"] += 1
        else:
            verdict = "JIT_SLOWER (expected for register-heavy chains)"
            verdict_counts["JIT_SLOWER"] += 1
        lines.append(f"| {key[0]} | {key[1]} | {interp_us:.2f} | {jit_us:.2f} | {ratio:.2f}x | {verdict} |")
    else:
        verdict_counts["N/A"] += 1
lines.append("")
lines.append("## Verdict summary")
lines.append("")
lines.append(f"- Rows where JIT is faster: **{verdict_counts['JIT_FASTER']}**")
lines.append(f"- Rows where JIT is slower (expected for register-heavy TC-08): **{verdict_counts['JIT_SLOWER']}**")
lines.append(f"- Rows missing one backend (skipped): **{verdict_counts['N/A']}**")
lines.append("")
lines.append("## Structural gate (perf_jit_vs_interp_ratio)")
lines.append("")
lines.append("Catch2 test `perf_jit_vs_interp_ratio` runs two dedicated TEST_CASEs:")
lines.append("- depth=500 XOR chain: requires `jit_us < interp_us` (strict)")
lines.append("- depth=1000 XOR chain: requires `interp/jit >= 2.0x`")
lines.append("")
lines.append("Both must pass for the gate to be considered satisfied.")
out_path_final = os.path.abspath(out_path)
os.makedirs(os.path.dirname(out_path_final), exist_ok=True)
with open(out_path_final, "w") as f:
    f.write("\n".join(lines) + "\n")
print(f"Wrote {out_path_final} ({os.path.getsize(out_path_final)} bytes)")
PYEOF
    log_ok "Ratio summary table: $RATIO_TABLE_PATH"
    # Print the ratio table to stdout for the human reader
    if [[ -f "$RATIO_TABLE_PATH" ]]; then
        echo ""
        echo "--- JIT vs Interpreter Ratio Summary ---"
        cat "$RATIO_TABLE_PATH"
    fi
else
    log_warn "perf_results.json missing; skipping ratio summary"
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
echo "  - build/perf_report_ratio.md   (JIT vs Interpreter ratio summary)"

# Print a short summary table extracted from the CSV (best-effort)
if [[ -f build/perf_report.csv ]]; then
    echo ""
    echo "--- CSV preview (first 10 lines) ---"
    head -n 10 build/perf_report.csv
fi

# Final exit code: gate failure wins over success.
if [[ "$JIT_RATIO_RC" -ne 0 ]]; then
    echo ""
    log_err "JIT-vs-interpreter structural gate FAILED (rc=$JIT_RATIO_RC); exiting non-zero"
    exit "$JIT_RATIO_RC"
fi

echo ""
log_ok "Done."