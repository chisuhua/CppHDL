#!/usr/bin/env bash
# ============================================================================
# benchmark_tracker.sh - Three-way performance history tracker
# ----------------------------------------------------------------------------
# Rewritten 2026-06-11: replaced legacy single-backend tracking (TC-01/02/04)
# with three-way tracking (Interpreter / JIT / Verilator) driven by perf_main.
#
# Output schema (CSV):
#   timestamp,test_name,params,backend,sim_us,median_us,status,ratio_interp_over_jit
#
# - ratio_interp_over_jit = interp_median_us / jit_median_us; empty for
#   non-jit rows. Higher is better.
# - Verilator rows are emitted when supported, else UNSUPPORTED with
#   skip_reason.
# - Use --compare to print a verdict summary instead of (just) appending.
#
# Exit codes:
#   0   success
#   1   build / runtime error
#   64  usage error
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
COMPARE_ONLY=0
if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<EOF
Usage: ./benchmark_tracker.sh [--compare]

Options:
  --compare    Print a JIT-vs-interpreter verdict summary instead of just appending
  --help, -h   Show this help and exit

Output:
  Appends one CSV row per (test, params, backend) to perf_history.csv in the
  repository root. Output schema is:

    timestamp,test_name,params,backend,sim_us,median_us,status,ratio_interp_over_jit

Exit codes:
  0  Success
  1  Build or runtime failure
EOF
    exit 64
fi
if [[ "${1:-}" == "--compare" ]]; then
    COMPARE_ONLY=1
fi

# ---- Configuration ---------------------------------------------------------
HISTORY_FILE="perf_history.csv"
PERF_TESTS_BIN="build/tests/benchmark/perf_tests"
PERF_RESULTS_JSON="perf_results.json"
TIMESTAMP="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"

# ---- Pre-flight: ensure perf_tests exists ----------------------------------
log_info "Stage 1/3: Pre-flight checks..."
if [[ ! -d build ]]; then
    log_info "build directory missing; running cmake -B build -DCMAKE_BUILD_TYPE=Release"
    cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
if [[ ! -x "$PERF_TESTS_BIN" ]]; then
    log_info "Building perf_tests..."
    cmake --build build -j"$(nproc 2>/dev/null || echo 2)" --target perf_tests
fi
if [[ ! -x "$PERF_TESTS_BIN" ]]; then
    log_err "perf_tests binary missing after build at $PERF_TESTS_BIN"
    exit 1
fi
log_ok "perf_tests binary: $PERF_TESTS_BIN"

# ---- Stage 2: run perf_main three-way comparison ---------------------------
# TC-07 (combinational XOR chain) and TC-08 (register chain) cover the two
# regimes we care about: where JIT is expected to win (TC-07) and where JIT
# may not win (TC-08). We intentionally exclude TC-09/11 (arith / wide-reg)
# because their runtime exceeds the 240s budget on this host — perf_main
# appends results incrementally, so killing it mid-run yields a partial or
# empty perf_results.json. Run ./build/tests/benchmark/perf_tests --all
# directly if you need TC-09/11.
#
# perf_main writes perf_results.json via std::ofstream (see
# report_generator.h:114) — its stdout is only progress + summary text,
# so we MUST discard stdout to avoid polluting the JSON file we are about
# to read. The previous version redirected stdout to a tempfile and
# concatenated [PROGRESS] lines into what should have been JSON.
log_info "Stage 2/3: Running three-way perf_main (TC-07 + TC-08)..."
PERF_RC=0
if ! timeout 240 "$PERF_TESTS_BIN" --tc=07 --tc=08 --report=json >/dev/null 2>&1; then
    PERF_RC=$?
    log_warn "perf_main returned rc=$PERF_RC (likely timeout); partial data may be in $PERF_RESULTS_JSON"
fi
if [[ ! -s "$PERF_RESULTS_JSON" ]]; then
    log_err "perf_main produced empty $PERF_RESULTS_JSON"
    exit 1
fi
log_ok "perf_results.json refreshed ($(wc -c <"$PERF_RESULTS_JSON") bytes, rc=$PERF_RC)"

# ---- Stage 3: append to perf_history.csv ----------------------------------
# Extract rows from perf_results.json via python (no jq dependency). For each
# row, also compute ratio_interp_over_jit for the same (test_name, params).
log_info "Stage 3/3: Appending rows to $HISTORY_FILE..."

python3 - "$HISTORY_FILE" "$TIMESTAMP" "$PERF_RESULTS_JSON" <<'PYEOF'
import json, sys, os
history_path, timestamp, json_path = sys.argv[1], sys.argv[2], sys.argv[3]
with open(json_path) as f:
    data = json.load(f)
runs = data.get("runs", [])
from collections import defaultdict
groups = defaultdict(dict)
for r in runs:
    key = (r["test_name"], r["params"])
    groups[key][r.get("backend", "unknown")] = r
new_rows = []
for key, g in groups.items():
    interp = g.get("interpreter", {})
    jit = g.get("jit", {})
    interp_us = interp.get("median_us", 0.0) if interp.get("status") == "PASS" else 0.0
    jit_us = jit.get("median_us", 0.0) if jit.get("status") == "PASS" else 0.0
    ratio = ""
    if interp_us > 0.0 and jit_us > 0.0:
        ratio = f"{interp_us / jit_us:.3f}"
    for backend_name, r in g.items():
        new_rows.append([
            timestamp,
            key[0],
            key[1],
            backend_name,
            f"{r.get('sim_us', 0.0):.3f}",
            f"{r.get('median_us', 0.0):.3f}",
            r.get("status", "MISSING"),
            ratio,
        ])
header = "timestamp,test_name,params,backend,sim_us,median_us,status,ratio_interp_over_jit"
write_header = not os.path.exists(history_path)
with open(history_path, "a") as f:
    if write_header:
        f.write(header + "\n")
    for row in new_rows:
        f.write(",".join(row) + "\n")
print(f"Appended {len(new_rows)} rows to {history_path}")
PYEOF

log_ok "History updated: $HISTORY_FILE"

# ---- Optional: --compare verdict summary -----------------------------------
if [[ "$COMPARE_ONLY" -eq 1 ]]; then
    echo ""
    printf '%s=== JIT vs Interpreter Verdict Summary ===%s\n' "$C_BLU" "$C_RST"
    python3 - "$PERF_RESULTS_JSON" <<'PYEOF'
import json, sys
with open(sys.argv[1]) as f:
    data = json.load(f)
runs = data.get("runs", [])
from collections import defaultdict
groups = defaultdict(dict)
for r in runs:
    key = (r["test_name"], r["params"])
    groups[key][r["backend"]] = r
faster, slower, na = 0, 0, 0
print("| Test | Params | Interp (μs) | JIT (μs) | Ratio | Verdict |")
print("| --- | --- | ---: | ---: | ---: | --- |")
for key in sorted(groups.keys()):
    g = groups[key]
    interp = g.get("interpreter", {})
    jit = g.get("jit", {})
    if interp.get("status") == "PASS" and jit.get("status") == "PASS":
        ratio = interp["median_us"] / jit["median_us"] if jit["median_us"] > 0 else 0
        if ratio >= 1.5:
            verdict, faster = "JIT_FASTER", faster + 1
        elif ratio >= 1.0:
            verdict, faster = "JIT_FASTER (marginal)", faster + 1
        else:
            verdict, slower = "JIT_SLOWER (expected for reg chains)", slower + 1
        print(f"| {key[0]} | {key[1]} | {interp['median_us']:.2f} | {jit['median_us']:.2f} | {ratio:.2f}x | {verdict} |")
    else:
        na += 1
print()
print(f"Faster: {faster} | Slower: {slower} | Skipped: {na}")
PYEOF
fi

echo ""
log_ok "Done."