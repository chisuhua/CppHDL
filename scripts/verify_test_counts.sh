#!/usr/bin/env bash
# scripts/verify_test_counts.sh
#
# Cross-check root AGENTS.md's documented test counts against actual
# filesystem state. Run from repo root. Exits 0 on match, 1 on drift.
#
# Used by .github/workflows/ci.yml to prevent the historical pattern
# where the documented CTest count (142/161) silently fell behind the
# actual registered count (159). See openspec/changes/cleanup-orphan-code
# for the audit trail.
#
# Parsing rules: AGENTS.md must contain a literal line of the form
# `CTEST_TOTAL: <N>` (the CI anchor). The value is compared to
# `ctest -N | tail -1`. If `ctest` cannot run (e.g. no build dir), the
# script prints a warning and exits 0 — CI's "build then verify" order
# guarantees the build dir exists when this runs there.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Allow callers to override the build dir (CI uses `build`).
BUILD_DIR="${BUILD_DIR:-build}"
AGENTS="$REPO_ROOT/AGENTS.md"

if [ ! -f "$AGENTS" ]; then
  echo "::warning::$AGENTS not found"
  exit 0
fi

# 1. CTest total — anchored on the literal "CTEST_TOTAL: <N>" line.
if grep -qE 'CTEST_TOTAL: [0-9]+' "$AGENTS"; then
  documented_total=$(grep -oE 'CTEST_TOTAL: [0-9]+' "$AGENTS" | head -1 | grep -oE '[0-9]+$')
else
  echo "::warning::No CTEST_TOTAL anchor in AGENTS.md (add a line 'CTEST_TOTAL: <N>' under NOTES)"
  exit 0
fi

if [ ! -f "$BUILD_DIR/CTestTestfile.cmake" ]; then
  echo "::warning::$BUILD_DIR/CTestTestfile.cmake not present; skipping CTest total check"
  exit 0
fi

actual_total=$(ctest --test-dir "$BUILD_DIR" -N 2>/dev/null | tail -1 | grep -oE '[0-9]+' || echo "")

if [ -z "$actual_total" ]; then
  echo "::warning::ctest -N produced no parseable total; skipping check"
  exit 0
fi

if [ "$actual_total" != "$documented_total" ]; then
  echo "::error::AGENTS.md says CTEST_TOTAL=$documented_total but ctest -N reports $actual_total"
  echo "        Update root AGENTS.md NOTES §Test counts to match."
  exit 1
fi

echo "AGENTS.md CTEST_TOTAL ($documented_total) matches ctest -N ($actual_total) ✓"

# 2. Catch2 source file count — informational only, not gated.
#    The "94 root / 27 chlib / 3 jit" counts in AGENTS.md are tracked by
#    `ls | wc -l` and would drift whenever a test is added. We do not
#    gate this; the CTest total above is the authoritative check.

exit 0
