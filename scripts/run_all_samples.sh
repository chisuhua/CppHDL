#!/usr/bin/env bash
# scripts/run_all_samples.sh
#
# Regression guard for `samples/` - the demos that double as living
# documentation. Before this script existed, samples had ZERO automated
# coverage: `grep -c 'samples/' tests/CMakeLists.txt` returned 1 (a comment
# line) and `grep -c 'samples/' run_all_ported_tests.sh` returned 0. Two
# samples had been segfaulting for months without anybody noticing.
#
# The guard is deliberately stronger than `run_all_ported_tests.sh`, which
# used to SKIP a missing binary. A missing binary is exactly how a sample
# would silently disappear from the guarded set (the same failure mode as
# `samples/fifo_bundle` rotting away unnoticed), so here it is a FAILURE.
#
# ℹ️ 2026-10-22 correction: the sentence above was itself WRONG.
# `run_all_ported_tests.sh` did not "skip" a missing binary — under
# `set -euo pipefail` its `run_test()` returned 2, which **aborted the whole
# script** at the first missing binary (exit 2, summary never reached). That
# is worse than a skip: a skip at least shows up in the summary, whereas an
# abort truncates the report and silently shrinks the guarded set.
# `examples-runtime-coverage-2026-10` fixed it: the missing-binary branch is
# now structurally identical to the failure branch (counts as FAIL, continues)
# plus an explicit EXECUTED-vs-tracked count conservation check.
# This file's own guard has always had the correct behaviour (C3 below).
#
# Three conservation rules keep the guarded set from shrinking silently:
#
#   C1 (registration consistency): every `samples/*.cpp` MUST be either
#       registered in `CH_SAMPLE_TARGETS` (samples/CMakeLists.txt) or listed
#       in `samples/disabled_samples.txt` with a reason and a condition for
#       coming back. A sample in neither state is a FAILURE.
#
#   C2 (count conservation): the number of samples actually executed MUST
#       equal the number of registered samples. A mismatch is a FAILURE,
#       not a skip.
#
#   C3 (missing binary): a registered sample whose binary is not in
#       build/samples/ is a FAILURE.
#
# Exemptions (KNOWN_FAILING below) are for samples that are known to fail.
# They are NOT skipped - they are still executed, as a stale probe:
#   - their failure does not count towards the failure total
#   - but if an exempted sample goes GREEN, that is reported as a STALE
#     exemption and IS a failure (an exemption must not outlive its cause)
#   - every exemption is printed as a `::notice` so it is never silent
#
# Exit codes: 0 = all good, 1 = at least one failure.
#
# Usage: scripts/run_all_samples.sh [build_dir]
#
# Only bash + sed/grep/comm are used (portable across the ubuntu-22.04 /
# macos-14 CI matrix; no python dependency).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${1:-${BUILD_DIR:-build}}"
SAMPLES_DIR="$REPO_ROOT/samples"
SAMPLE_BIN_DIR="$BUILD_DIR/samples"
TIMEOUT_SECONDS="${SAMPLE_TIMEOUT_SECONDS:-60}"

# ---------------------------------------------------------------------------
# Known-failing samples. Each entry: "<sample> <TAB> <root cause> <TAB>
# <related change / issue> <TAB> <invalidation condition>".
#
# The invalidation condition MUST be decidable: whoever fixes the root cause
# MUST remove the line here (and the guard will then fail with a STALE
# exemption if the line is left behind after the sample goes green).
#
# ℹ️ 2026-10-22: the last entry (`nested_bundle_demo`) was removed when
#    `bundle-ecosystem-hardening-2026-10` fixed the root cause (the base
#    constructor no longer calls a virtual function on a not-yet-constructed
#    nested bundle member). The guard reported it as a STALE exemption
#    first — which is exactly that mechanism working as designed.
# ---------------------------------------------------------------------------
KNOWN_FAILING=()

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

if [ ! -d "$SAMPLE_BIN_DIR" ]; then
    echo "ERROR: sample binary directory '$SAMPLE_BIN_DIR' not found"
    echo "Run: cmake -B build && cmake --build build"
    exit 1
fi

# Portable timeout: GNU `timeout` does not exist on macOS by default
# (coreutils provides `gtimeout` there).
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_BIN="timeout"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_BIN="gtimeout"
else
    echo "ERROR: neither 'timeout' nor 'gtimeout' is available."
    echo "       On macOS: brew install coreutils   (provides gtimeout)."
    echo "       Without a timeout a hanging sample would block CI forever."
    exit 1
fi

# ---------------------------------------------------------------------------
# Source of truth: the registered sample set.
# ---------------------------------------------------------------------------
registered_samples() {
    sed -n '/set(CH_SAMPLE_TARGETS/,/^)/p' "$SAMPLES_DIR/CMakeLists.txt" \
        | sed -n 's/^    \([A-Za-z0-9_]*\)$/\1/p'
}

# Samples that are deliberately not registered (with a reason and a
# condition for coming back). File format: name <TAB> reason <TAB> condition.
disabled_samples() {
    [ -f "$SAMPLES_DIR/disabled_samples.txt" ] || return 0
    grep -v '^[[:space:]]*#' "$SAMPLES_DIR/disabled_samples.txt" \
        | grep -v '^[[:space:]]*$' \
        | cut -f1
}

all_sample_sources() {
    ls "$SAMPLES_DIR"/*.cpp 2>/dev/null | xargs -n1 basename | sed 's/\.cpp$//'
}

# Exemption lookup: name -> the "reason | change | invalidation" text.
exemption_text() {
    local want="$1" line
    for line in "${KNOWN_FAILING[@]}"; do
        if [ "$(printf '%s' "$line" | cut -f1)" = "$want" ]; then
            printf '%s' "$line" | cut -f2-
            return 0
        fi
    done
    return 1
}

is_exempted() {
    local want="$1" line
    for line in "${KNOWN_FAILING[@]}"; do
        if [ "$(printf '%s' "$line" | cut -f1)" = "$want" ]; then
            return 0
        fi
    done
    return 1
}

PASS=0
FAIL=0
EXECUTED=0
MISSING=0
STALE=0
declare -a FAIL_NAMES=()
declare -a STALE_NAMES=()
declare -a MISSING_NAMES=()

echo "=========================================="
echo " samples 回归护栏"
echo "=========================================="
echo "Build directory : $SAMPLE_BIN_DIR"
echo "Timeout         : ${TIMEOUT_SECONDS}s per sample"
echo ""

# ---------------------------------------------------------------------------
# C1: registration consistency
# ---------------------------------------------------------------------------
mapfile -t REGISTERED < <(registered_samples)
mapfile -t DISABLED  < <(disabled_samples)
mapfile -t SOURCES   < <(all_sample_sources)

REG_COUNT=${#REGISTERED[@]}
echo "--- C1: 注册一致性（每个 sample 必须已注册或在禁用清单里）---"
CONSISTENCY_FAIL=0
for src in "${SOURCES[@]}"; do
    if printf '%s\n' "${REGISTERED[@]}" | grep -qx "$src"; then
        continue
    fi
    if printf '%s\n' "${DISABLED[@]}" | grep -qx "$src"; then
        continue
    fi
    printf "${RED}  UNREGISTERED  %s${NC}\n" "$src"
    echo "      既不在 CH_SAMPLE_TARGETS 里，也不在 samples/disabled_samples.txt 里"
    echo "      => 处置只有两种：① 能编译 => 补进 CH_SAMPLE_TARGETS；"
    echo "                   ② 不能编译 => 登记进禁用清单（带理由 + 归位条件）"
    CONSISTENCY_FAIL=$((CONSISTENCY_FAIL + 1))
done
if [ "$CONSISTENCY_FAIL" -eq 0 ]; then
    echo "  OK：${#SOURCES[@]} 个 sample 源文件全部有归属（已注册 ${REG_COUNT} / 已禁用 ${#DISABLED[@]}）"
else
    FAIL=$((FAIL + CONSISTENCY_FAIL))
    FAIL_NAMES+=("C1-registration-consistency (${CONSISTENCY_FAIL} 个 sample 无归属)")
fi
echo ""

# ---------------------------------------------------------------------------
# Exemptions are announced, never silent.
# ---------------------------------------------------------------------------
if [ ${#KNOWN_FAILING[@]} -gt 0 ]; then
    echo "--- 豁免清单（豁免项**仍会被执行**，只影响失败计数）---"
    for line in "${KNOWN_FAILING[@]}"; do
        name=$(printf '%s' "$line" | cut -f1)
        echo "::notice sample=$name exempted from the samples regression guard"
        printf '%s' "$line" | cut -f2- | tr '\t' '\n' | while read -r part; do
            [ -n "$part" ] && echo "        $part"
        done
    done
    echo ""
fi

# ---------------------------------------------------------------------------
# C2/C3: execute every registered sample
# ---------------------------------------------------------------------------
echo "--- C2/C3: 逐个执行（缺失二进制 = 失败）---"
for name in "${REGISTERED[@]}"; do
    exe="$SAMPLE_BIN_DIR/$name"
    printf "  %-40s " "$name"

    if [ ! -f "$exe" ] || [ ! -x "$exe" ]; then
        printf "${RED}MISSING BINARY${NC}\n"
        MISSING=$((MISSING + 1))
        FAIL=$((FAIL + 1))
        MISSING_NAMES+=("$name")
        continue
    fi

    EXECUTED=$((EXECUTED + 1))
    set +e
    # The brace group (not a subshell) keeps bash's own "Segmentation fault"
    # job message out of the report: the message is printed by THIS shell to
    # its stderr, so it has to be redirected around the same command.
    { "$TIMEOUT_BIN" "$TIMEOUT_SECONDS" "$exe" >/dev/null 2>&1; } 2>/dev/null
    rc=$?
    set -e

    if [ "$rc" -eq 0 ]; then
        if is_exempted "$name"; then
            printf "${YELLOW}STALE EXEMPTION (已转绿)${NC}\n"
            echo "      豁免已失效（根因已修复）=> MUST 从脚本的 KNOWN_FAILING 里移除该行"
            STALE=$((STALE + 1))
            FAIL=$((FAIL + 1))
            STALE_NAMES+=("$name")
            FAIL_NAMES+=("$name (stale exemption)")
        else
            printf "${GREEN}PASS${NC}\n"
            PASS=$((PASS + 1))
        fi
    else
        if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
            reason="TIMEOUT (>${TIMEOUT_SECONDS}s)"
        elif [ "$rc" -eq 139 ]; then
            reason="SEGFAULT (exit 139)"
        else
            reason="FAILED (exit code: $rc)"
        fi
        if is_exempted "$name"; then
            printf "${YELLOW}EXEMPTED (${reason})${NC}\n"
            echo "      已豁免：失败不计入失败数；但豁免项**仍被执行**（stale 探针）"
        else
            printf "${RED}%s${NC}\n" "$reason"
            FAIL=$((FAIL + 1))
            FAIL_NAMES+=("$name (exit $rc)")
        fi
    fi
done
echo ""

# ---------------------------------------------------------------------------
# C2: count conservation
# ---------------------------------------------------------------------------
echo "--- C2: 数量守恒（实际执行数 == 已注册数）---"
if [ "$EXECUTED" -ne "$REG_COUNT" ]; then
    printf "${RED}  MISMATCH: 已注册 %d，实际执行 %d（缺失二进制 %d）${NC}\n" \
        "$REG_COUNT" "$EXECUTED" "$MISSING"
    FAIL=$((FAIL + 1))
    FAIL_NAMES+=("C2-count-conservation (registered=$REG_COUNT executed=$EXECUTED)")
else
    echo "  OK：已注册 $REG_COUNT == 实际执行 $EXECUTED"
fi
echo ""

echo "=========================================="
printf "  ${GREEN}PASS: %-4d${NC} ${RED}FAIL: %-4d${NC} ${YELLOW}STALE: %-4d${NC} ${YELLOW}MISSING: %-4d${NC}\n" \
    "$PASS" "$FAIL" "$STALE" "$MISSING"
echo "=========================================="

if [ ${#MISSING_NAMES[@]} -gt 0 ]; then
    echo ""
    echo "Missing binaries (build them: cmake --build build):"
    for name in "${MISSING_NAMES[@]}"; do
        echo "  - $name"
    done
fi

if [ ${#STALE_NAMES[@]} -gt 0 ]; then
    echo ""
    echo "Stale exemptions (sample went green - remove the KNOWN_FAILING entry):"
    for name in "${STALE_NAMES[@]}"; do
        echo "  - $name"
    done
fi

if [ ${#FAIL_NAMES[@]} -gt 0 ]; then
    echo ""
    echo "Failed samples:"
    for name in "${FAIL_NAMES[@]}"; do
        echo "  - $name"
    done
    exit 1
fi

echo ""
echo "samples 回归护栏全绿 ✓"
exit 0
