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

# ---------------------------------------------------------------------------
# 锚点解析 helper（ADR-078 · S4c-0：锚点 MUST **按产品线分组**）
#
# 格式：`- <KEY>: <group>=<N>[, <group>=<M>]*`  例 `CTEST_TOTAL: lib=283, mini=2, full=1`
#
# ⚠️ **为什么分组**（ADR-078 AC3）：单值锚点下「加一条产品线」MUST 手改全局数字 ⇒ ⇒
#    成本随线数线性增长；分组后「加线只影响自己那组」⇒ ⇒ 且**和**仍被校验 ⇒ 不丢判别力。
#
# 判据（两层，都是**等式**）：
#   ① **和** == 实测总数（分组漏一组 / 加错 ⇒ 红）
#   ② **每线** == 该线的实际数（由 `scripts/lib/product_lines.py --counts` 派生的
#      target 清单 ∩ 实测清单 ⇒ ⇒ 组的归属错 ⇒ 红）
# ⚠️ 组名 `lib` = 库层测试（根 `tests/**`），**不属任何产品线** ⇒ 它是**余项**（派生），
#    但仍 MUST 登记（否则「和」可被随意调整）。
# ---------------------------------------------------------------------------
anchor_value() {  # $1=KEY  →  打印该锚点的分组串（无锚点则空）
    sed -nE "s/^[[:space:]]*-[[:space:]]*$1:[[:space:]]*(.+)$/\1/p" "$AGENTS" | head -1
}
anchor_sum() {    # $1=KEY  →  打印各组之和（⚠️ 不用 bc —— 精简容器里没有它）
    anchor_value "$1" | tr ',' '\n' | sed -nE 's/^[[:space:]]*[a-z-]+=([0-9]+)[[:space:]]*$/\1/p' \
        | awk '{ s += $1 } END { if (NR > 0) printf "%d\n", s }'
}
anchor_group() {  # $1=KEY $2=group  →  打印该组的值
    anchor_value "$1" | tr ',' '\n' | sed -nE "s/^[[:space:]]*$2=([0-9]+)[[:space:]]*$/\1/p" | head -1
}
pl_line_count() { # $1=line $2=实测清单文件  →  该线的实际数
    # ⚠️ `awk -F'\t'` 在 mawk 下**不**把 `\t` 解释为 TAB（实测：整行不分割 ⇒ 恒 0）
    #    ⇒ ⇒ MUST 用 `printf '\t'` 生成真 TAB。
    local line_targets
    line_targets="$(mktemp)"
    python3 scripts/lib/product_lines.py --counts 2>/dev/null \
        | awk -F"$(printf '\t')" -v L="$1" '$1 == L { print $2 }' | sort > "$line_targets"
    comm -12 "$line_targets" "$2" | wc -l
    rm -f "$line_targets"
}

# 1. CTest total — anchored on the grouped "CTEST_TOTAL: <group>=<N>, ..." line.
if ! anchor_value CTEST_TOTAL | grep -qE '[a-z-]+=[0-9]+'; then
  echo "::warning::No grouped CTEST_TOTAL anchor in AGENTS.md (add 'CTEST_TOTAL: lib=<N>, mini=<N>, full=<N>')"
  exit 0
fi
documented_total="$(anchor_sum CTEST_TOTAL)"
if [ -z "$documented_total" ]; then
  echo "::error::CTEST_TOTAL 锚点的分组串无法解析：$(anchor_value CTEST_TOTAL)"
  exit 1
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
  echo "::error::AGENTS.md CTEST_TOTAL 的分组之和=$documented_total but ctest -N reports $actual_total"
  echo "        (锚点：$(anchor_value CTEST_TOTAL))"
  echo "        Update root AGENTS.md NOTES §Test counts to match."
  exit 1
fi

# ① 和一致 ⇒ 继续 ② **每线**一致
ALL_LIST="$(mktemp)"
ctest --test-dir "$BUILD_DIR" -N 2>/dev/null \
    | sed -nE 's/^ *Test +#([0-9]+): +(.+)$/\2/p' | sort > "$ALL_LIST"
for grp in $(anchor_value CTEST_TOTAL | tr ',' '\n' | sed -nE 's/^[[:space:]]*([a-z-]+)=.*/\1/p'); do
    [ "$grp" = "lib" ] && continue      # 库层 = 余项（不属任何产品线）
    doc="$(anchor_group CTEST_TOTAL "$grp")"
    act="$(pl_line_count "$grp" "$ALL_LIST")"
    if [ "$doc" != "$act" ]; then
        echo "::error::CTEST_TOTAL 的组 '$grp' 登记 $doc 但实际 $act（target 归属或数值漂移）"
        exit 1
    fi
    echo "  CTEST_TOTAL[$grp] = $act ✓"
done
rm -f "$ALL_LIST"

echo "AGENTS.md CTEST_TOTAL ($(anchor_value CTEST_TOTAL) = $documented_total) matches ctest -N ($actual_total) ✓"

# 2. `verilator_e2e` 标签下的测试数 —— **独立**锚点。
#    ⚠️ 为什么需要独立锚点（2026-10-28 新增）：`CTEST_TOTAL` 只统计**注册进
#    CTest 的测试数**；而给一个**已注册**的 target 加/删 `verilator_e2e` 标签
#    **不改变** `CTEST_TOTAL`（target 数不变）⇒ 只改标签的话，`CTEST_TOTAL`
#    护栏**完全看不见** ⇒ 双后端覆盖会**静默缩面**。
#    ⇒ 锚点格式：`VERILATOR_E2E_TOTAL: <N>`。
if anchor_value VERILATOR_E2E_TOTAL | grep -qE '[a-z-]+=[0-9]+'; then
    documented_vl="$(anchor_sum VERILATOR_E2E_TOTAL)"
    actual_vl=$(ctest --test-dir "$BUILD_DIR" -N -L verilator_e2e 2>/dev/null | tail -1 | grep -oE '[0-9]+' || echo "")
    if [ -z "$actual_vl" ]; then
        echo "::warning::ctest -N -L verilator_e2e produced no parseable total; skipping check"
    elif [ "$actual_vl" != "$documented_vl" ]; then
        echo "::error::AGENTS.md VERILATOR_E2E_TOTAL 的分组之和=$documented_vl but ctest -N -L verilator_e2e reports $actual_vl"
        echo "        (锚点：$(anchor_value VERILATOR_E2E_TOTAL))"
        exit 1
    else
        VL_LIST="$(mktemp)"
        ctest --test-dir "$BUILD_DIR" -N -L verilator_e2e 2>/dev/null \
            | sed -nE 's/^ *Test +#([0-9]+): +(.+)$/\2/p' | sort > "$VL_LIST"
        for grp in $(anchor_value VERILATOR_E2E_TOTAL | tr ',' '\n' | sed -nE 's/^[[:space:]]*([a-z-]+)=.*/\1/p'); do
            [ "$grp" = "lib" ] && continue
            doc="$(anchor_group VERILATOR_E2E_TOTAL "$grp")"
            act="$(pl_line_count "$grp" "$VL_LIST")"
            if [ "$doc" != "$act" ]; then
                echo "::error::VERILATOR_E2E_TOTAL 的组 '$grp' 登记 $doc 但实际 $act"
                exit 1
            fi
            echo "  VERILATOR_E2E_TOTAL[$grp] = $act ✓"
        done
        rm -f "$VL_LIST"
        echo "AGENTS.md VERILATOR_E2E_TOTAL ($(anchor_value VERILATOR_E2E_TOTAL) = $documented_vl) matches ctest -N -L verilator_e2e ($actual_vl) ✓"
    fi
fi

# 3. Catch2 source file count — informational check for chlib directory.
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
