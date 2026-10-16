#!/usr/bin/env bash
# scripts/verify_test_counts.sh
#
# Cross-check root AGENTS.md's documented test counts against actual
# filesystem state. Run from repo root. Exits 0 on match, 1 on drift.
#
# Used by .github/workflows/ci.yml to prevent the historical pattern
# where the documented CTest count (142/161) silently fell behind the
# actual registered count (159). See openspec/changes/archive/2026-10-15-cleanup-orphan-code
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

# 2. Catch2 source file count — informational check for chlib directory.
#    The chlib count in AGENTS.md is line-anchored as
#    '**27 Catch2 test source files** at `tests/chlib/test_*.cpp`'
#    and we verify it by an explicit `ls | wc -l`. Drift here means the
#    documentation diverged from the filesystem (e.g. someone added a
#    new test but forgot to update AGENTS.md).
if grep -qE '\*\*([0-9]+) Catch2 test source files\*\* at `tests/chlib/test_\*\.cpp`' "$AGENTS"; then
  documented_chlib=$(grep -oE '\*\*[0-9]+ Catch2 test source files\*\* at `tests/chlib/test_\*\.cpp`' "$AGENTS" | head -1 | grep -oE '[0-9]+' | head -1)
  actual_chlib=$(ls tests/chlib/test_*.cpp 2>/dev/null | wc -l)
  if [ "$documented_chlib" = "$actual_chlib" ]; then
    echo "AGENTS.md chlib test source count ($actual_chlib) matches filesystem ✓"
  else
    echo "::warning::AGENTS.md says $documented_chlib chlib tests but filesystem has $actual_chlib"
  fi
fi

# 3. Informational only — root / jit / samples / main-bearing examples counts.
#    These are documented with 'ls | wc -l' / 'grep -rl' commands rather
#    than literal numbers, so a drift is visible by running the command.
#    We print them to give the operator a quick spot-check.
echo ""
echo "--- Informational spot-checks (drift visible by re-running each command) ---"
echo "root tests/test_*.cpp         : $(ls tests/test_*.cpp 2>/dev/null | wc -l)"
echo "tests/chlib/test_*.cpp        : $(ls tests/chlib/test_*.cpp 2>/dev/null | wc -l)"
echo "tests/jit/test_*.cpp          : $(ls tests/jit/test_*.cpp 2>/dev/null | wc -l)"
echo "samples/*.cpp                 : $(ls samples/*.cpp 2>/dev/null | wc -l)"
echo "examples main()-bearing .cpp  : $(grep -rl '^int main' examples --include='*.cpp' 2>/dev/null | wc -l)"

exit 0
