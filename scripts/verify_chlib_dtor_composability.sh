#!/usr/bin/env bash
# scripts/verify_chlib_dtor_composability.sh
#
# Guardrail: the COMPOSABLE form of `ch_state_machine` MUST stay composable,
# and the "throwing destructor => the type is not composable" contract MUST
# stay documented at every place that can hand a user a non-composable type.
#
# WHY this needs a guardrail (the failure is SILENT for this repo):
#
#   `ch_state_machine`'s destructor is `noexcept(false)` in the DEFAULT form
#   (ADR-050 D5: "contributions that were registered but never resolved MUST be
#   detected *catchably* at destruction"). A member with a potentially-throwing
#   destructor makes the ENCLOSING class's implicit destructor
#   `noexcept(false)` as well => if the base class has a virtual destructor
#   (the normal shape of an abstract plugin / interface base), the derived
#   destructor overrides a `noexcept` virtual destructor with a LOOSER
#   exception specification => HARD COMPILE ERROR:
#       "looser exception specification on overriding virtual function"
#
#   => The error surfaces in the DOWNSTREAM project, never in this repo
#      => this repo's own test suite stays GREEN => zero upstream signal.
#      (Real occurrence: downstream ChipForge, issue #28, 2026-11-08.)
#
#   NOTE the trigger is WIDER than "the base declares `noexcept`": a
#   `virtual ~Base() = default;` is IMPLICITLY noexcept => it triggers too.
#   And it is not an exotic usage: `ch::Component` (include/component.h) has
#   `virtual ~Component();` => promoting a describe()-local FSM to a component
#   member does not compile.
#
# ADR-067 ruling (docs/adr/ADR-067-dtor-exception-spec-composability.md):
#   - the DEFAULT form stays byte-identical (ADR-050 D5 preserved);
#   - an explicit `dtor_detection::explicit_only` form has a `noexcept`
#     destructor => the type IS composable => detection moves to an explicit
#     `check_unresolved()` (catchable) + a terminating backstop (fail-closed).
#
# Rules:
#   R1: the composability fixture MUST compile
#       (its `static_assert`s encode: default form unchanged / composable form
#        noexcept / the `std::optional` gap pinned / a `Component`-derived
#        class holding the FSM BY VALUE is noexcept)
#   R2: the header MUST expose `dtor_detection` (both enumerators) and
#       `check_unresolved()`
#   R3: the usage contract MUST be documented in BOTH headers
#       (`include/chlib/state_machine.h` AND `include/chlib/if_stmt.h`)
#   R4: the usage contract MUST be in the capability specs
#       (the main spec OR a pending change's delta under openspec/changes/)
#
# Exit codes: 0 = all rules hold, 1 = at least one violation, 2 = usage error.
#
# Usage:
#   scripts/verify_chlib_dtor_composability.sh [--self-test]
#
# ⚠️ PORTABILITY: bash + a C++ compiler only (no GNU-only `sed -i`; the
#    in-place poisoning uses `cp` for backup/restore) => identical on
#    ubuntu-22.04 / macos-14.

set -uo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$REPO_ROOT" || exit 2

CXX_BIN="${CXX:-g++}"
FIXTURE="tests/test_chlib_composability_api.cpp"
FSM_HEADER="include/chlib/state_machine.h"
IF_STMT_HEADER="include/chlib/if_stmt.h"
SPEC_FSM="openspec/specs/chlib-state-machine-semantics/spec.md"
SPEC_COND="openspec/specs/chlib-conditional-semantics/spec.md"

# The contract wording token (ADR-067 decision 4). Kept short and stable so
# that the guard does not depend on the exact prose.
CONTRACT_TOKEN="有虚析构"

INCLUDES=(
    -I tests
    -I include
    -I include/core
    -I include/ast
    -I include/abstract
    -I include/utils
    -I external/inipp
)

FAILURES=()

fail() { FAILURES+=("$1"); }

# ---------------------------------------------------------------------------
# R1: the fixture MUST compile (the compile-time half of the contract)
# ---------------------------------------------------------------------------
check_r1_fixture_compiles() {
    if [ ! -f "$FIXTURE" ]; then
        fail "R1: fixture missing: $FIXTURE"
        return
    fi
    local out
    out="$("$CXX_BIN" -std=c++23 -fsyntax-only "${INCLUDES[@]}" "$FIXTURE" 2>&1)"
    local rc=$?
    if [ $rc -ne 0 ]; then
        fail "R1: the composability fixture MUST compile (rc=$rc) => the composable form is broken:"
        printf '%s\n' "$out" | grep -E "error:|static assertion failed" | head -5 |
            sed 's/^/      /'
    fi
}

# ---------------------------------------------------------------------------
# R2: the API surface MUST exist
# ---------------------------------------------------------------------------
check_r2_api_surface() {
    if ! grep -q "enum class dtor_detection" "$FSM_HEADER"; then
        fail "R2: $FSM_HEADER MUST expose 'enum class dtor_detection'"
    fi
    if ! grep -q "automatic" "$FSM_HEADER"; then
        fail "R2: $FSM_HEADER MUST expose the 'automatic' enumerator"
    fi
    if ! grep -q "explicit_only" "$FSM_HEADER"; then
        fail "R2: $FSM_HEADER MUST expose the 'explicit_only' enumerator"
    fi
    # ⚠️ 匹配 `check_unresolved()`（**带括号**）⇒ ⇒ 改名成 `check_unresolved_RENAMED()`
    #    **不会**蒙混过关（⇒ ⇒ 自检 M3 才有判别力）
    if ! grep -q "check_unresolved()" "$FSM_HEADER"; then
        fail "R2: $FSM_HEADER MUST expose 'check_unresolved()'"
    fi
}

# ---------------------------------------------------------------------------
# R3: the usage contract MUST be in both headers
# ---------------------------------------------------------------------------
check_r3_header_contract() {
    local f
    for f in "$FSM_HEADER" "$IF_STMT_HEADER"; do
        if [ ! -f "$f" ]; then
            fail "R3: header missing: $f"
            continue
        fi
        if ! grep -q "$CONTRACT_TOKEN" "$f"; then
            fail "R3: $f MUST document the usage contract (token: '$CONTRACT_TOKEN')"
        fi
    done
}

# ---------------------------------------------------------------------------
# R4: the usage contract MUST be in the capability specs
#   (the main spec OR a pending change's delta => green during the change,
#    green after the archive merge, red if the main spec regresses)
# ---------------------------------------------------------------------------
check_r4_spec_contract() {
    local hit=0
    local f
    for f in "$SPEC_FSM" "$SPEC_COND"; do
        if [ -f "$f" ] && grep -q "$CONTRACT_TOKEN" "$f"; then
            hit=1
        fi
    done
    if [ $hit -eq 0 ]; then
        # pending change deltas
        for f in openspec/changes/*/specs/*/spec.md; do
            [ -f "$f" ] || continue
            if grep -q "$CONTRACT_TOKEN" "$f"; then
                hit=1
            fi
        done
    fi
    if [ $hit -eq 0 ]; then
        fail "R4: the usage contract MUST be in the capability specs (token: '$CONTRACT_TOKEN')"
    fi
}

run_all_rules() {
    check_r1_fixture_compiles
    check_r2_api_surface
    check_r3_header_contract
    check_r4_spec_contract
}

# ---------------------------------------------------------------------------
# --self-test: every rule MUST have discriminating power
#   (poison the source in place => the rule MUST go RED => restore => GREEN)
# ---------------------------------------------------------------------------
self_test() {
    # ⚠️ `backup` / `poisoned` MUST 是**全局**（bash 的 `local` 在函数作用域内
    #    ⇒ ⇒ EXIT trap 里的 `restore()` **看不到**它们 ⇒ ⇒ `set -u` 下会
    #    「unbound variable」⇒ ⇒ **还原失败 ⇒ 源文件被留在投毒态** ⇒ ⇒ 真实踩过）
    backup="$(mktemp -d)"
    poisoned=0

    restore() {
        if [ "${poisoned:-0}" = "1" ]; then
            cp "$backup/state_machine.h" "$FSM_HEADER"
            cp "$backup/if_stmt.h" "$IF_STMT_HEADER"
            poisoned=0
        fi
        [ -n "${backup:-}" ] && rm -rf "$backup"
        return 0
    }
    trap restore EXIT

    cp "$FSM_HEADER" "$backup/state_machine.h"
    cp "$IF_STMT_HEADER" "$backup/if_stmt.h"

    # M1: break the composable form (make the destructor throwing again)
    #     => R1 MUST go red (the fixture's `static_assert` fires)
    poisoned=1
    sed -i 's/noexcept(kDetectAtDtor ? false : true)/noexcept(false)/' "$FSM_HEADER"
    if grep -q "noexcept(kDetectAtDtor" "$FSM_HEADER"; then
        echo "❌ self-test: M1 poisoning did not apply (the header text changed?)" >&2
        return 1
    fi
    if run_rules_quietly; then
        echo "❌ self-test: M1 (break the composable form) MUST make R1 red" >&2
        return 1
    fi
    cp "$backup/state_machine.h" "$FSM_HEADER"

    # M2: remove the usage contract from the conditional-block header
    #     => R3 MUST go red
    poisoned=1
    sed -i "s/${CONTRACT_TOKEN}/__removed_contract_token__/" "$IF_STMT_HEADER"
    if grep -q "$CONTRACT_TOKEN" "$IF_STMT_HEADER"; then
        echo "❌ self-test: M2 poisoning did not apply" >&2
        return 1
    fi
    if run_rules_quietly; then
        echo "❌ self-test: M2 (drop the contract from if_stmt.h) MUST make R3 red" >&2
        return 1
    fi
    cp "$backup/if_stmt.h" "$IF_STMT_HEADER"

    # M3: rename the explicit check => R2 MUST go red
    poisoned=1
    sed -i 's/check_unresolved()/check_unresolved_RENAMED()/' "$FSM_HEADER"
    if grep -q "check_unresolved()" "$FSM_HEADER"; then
        echo "❌ self-test: M3 poisoning did not apply" >&2
        return 1
    fi
    if run_rules_quietly; then
        echo "❌ self-test: M3 (rename check_unresolved) MUST make R2 red" >&2
        return 1
    fi
    cp "$backup/state_machine.h" "$FSM_HEADER"

    poisoned=0
    echo "✅ self-test: every rule has discriminating power (M1/M2/M3 each turned the guard RED)"
    return 0
}

# run the rules without touching FAILURES (used by --self-test)
run_rules_quietly() {
    local saved_failures=("${FAILURES[@]+"${FAILURES[@]}"}")
    FAILURES=()
    run_all_rules
    local rc=0
    if [ "${#FAILURES[@]}" -gt 0 ]; then
        rc=1
    fi
    FAILURES=("${saved_failures[@]+"${saved_failures[@]}"}")
    return $rc
}

case "${1:-}" in
    --self-test)
        self_test
        exit $?
        ;;
    "")
        ;;
    *)
        echo "usage: $0 [--self-test]" >&2
        exit 2
        ;;
esac

run_all_rules

if [ "${#FAILURES[@]}" -gt 0 ]; then
    echo "❌ verify_chlib_dtor_composability: ${#FAILURES[@]} violation(s)" >&2
    for f in "${FAILURES[@]}"; do
        echo "   - $f" >&2
    done
    exit 1
fi

echo "✅ verify_chlib_dtor_composability: PASS (R1 fixture compiles · R2 API surface · R3 header contract · R4 spec contract)"
exit 0
