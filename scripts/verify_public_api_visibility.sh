#!/usr/bin/env bash
# scripts/verify_public_api_visibility.sh
#
# Guardrail: a PUBLIC symbol that is removed / renamed MUST be recorded where a
# DOWNSTREAM consumer can find it, and the record MUST stay truthful (the
# recorded symbol must really be gone from the source).
#
# WHY this needs a guardrail (the failure is SILENT for this repo):
#
#   The repo's Zero-Debt rule said "zero consumers => delete immediately". Its
#   evidence source is a REPO-LOCAL `grep` => it is structurally blind to
#   DOWNSTREAM consumers => a public API can disappear and every upstream test
#   stays green. Real occurrence: downstream ChipForge failed to compile on
#   2026-11-08 because `ch_state_machine::is_built()` was removed in `c7b0b28`
#   (ADR-050 D2) with no user-visible notice anywhere (docs/CHANGELOG.md had
#   been stale since 2026-10-15) => upstream issue #28.
#
#   => The fix is a CARRIER (a record a downstream can find) plus a TRUTH
#      requirement (the record must not rot). Both are mechanical.
#
# Rules:
#   R1  every change archived ON/AFTER the mechanism date (2026-11-09) MUST
#       carry a NON-EMPTY `## 用户可见变更` section in one of its own files
#   R2  the registry `.rddf/checks/public-api-surface.md` MUST keep a non-empty
#       "已移除 / 已改名的公开符号" chapter, and every ADR it cites MUST exist
#   R3  root AGENTS.md MUST carry the dead-code verdict boundary sentence
#       ("零「仓内」消费者的「公开」符号不算 dead code") AND the roadmap guard
#       MUST stay green (=> the sentence did not disturb the AUTO block)
#   R4  docs/CHANGELOG.md MUST carry the 2026-11-09 carrier entry with the
#       token list of the 2026-10-15..2026-11-09 user-visible removals
#       (token list, NOT a date check => writing a new date cannot turn it green)
#   R5  every machine probe in `.rddf/checks/public-api-removals.tsv` MUST hold:
#       removed => the symbol is ABSENT from its scope
#       renamed => the OLD name is absent AND the NEW name is present
#       re      => the regex does not match
#       nofile  => the glob matches no file
#       (judged AFTER stripping comments and string literals, otherwise a doc
#        comment that mentions the removed name yields a false RED)
#   R6  shrinkage anchors: the probe count MUST equal EXPECTED_PROBES, and every
#       probe id MUST appear verbatim in the registry markdown
#       (=> deleting the table row alone is RED; deleting row + probe is RED)
#
# Exit codes: 0 = all rules hold, 1 = at least one violation, 2 = usage error.
#
# Usage:
#   scripts/verify_public_api_visibility.sh [--self-test]
#
# ⚠️ PORTABILITY: bash + python3 + grep only (in-place poisoning uses cp
#    backup/restore => no GNU-only `sed -i` flags beyond the portable form).

set -uo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$REPO_ROOT" || exit 2

REGISTRY=".rddf/checks/public-api-surface.md"
PROBES=".rddf/checks/public-api-removals.tsv"
CHANGELOG="docs/CHANGELOG.md"
AGENTS="AGENTS.md"
ARCHIVE_DIR="openspec/changes/archive"
MECHANISM_DATE="2026-11-09"
# 机制生效当日的**存量**归档（早于机制落地 ⇒ 不追溯；⚠️ 该清单 MUST 双向：
# 条目不存在 ⇒ 红（否则它会变成「静默豁免池」））。不追溯的理由 = 存量回填的
# 工作量不可控（145 个历史 change），而「从现在起的每一个」才是可执行的义务。
GRANDFATHERED=("2026-11-09-hierarchical-parent-side-landing-implementation")
# 缩面锚点（精确计数 ⇒ 增删 MUST 同步 ⇒ 机制同 CTEST_TOTAL）：
#   EXPECTED_REMOVAL_ROWS = 登记表「已移除 / 已改名」表的**行数**（分组行，一个符号族一行）
#   EXPECTED_PROBES       = 机器探针的行数（逐符号 / 逐文件族）
EXPECTED_REMOVAL_ROWS=20
EXPECTED_PROBES=34
AGENTS_TOKEN="零「仓内」消费者的「公开」符号不算 dead code"
CHANGELOG_TOKENS=(is_built current_state resolve_hold "noexcept(false)")

WORK="$(mktemp -d)"
FAILURES=()

fail() { FAILURES+=("$1"); }

cleanup() { rm -rf "$WORK"; return 0; }
trap cleanup EXIT

# ---------------------------------------------------------------------------
# shared helpers
# ---------------------------------------------------------------------------

# Strip C/C++ comments and string/char literals, then expose the code text.
# Written once, used by both the probe checker and R1's section scanner is
# not needed (markdown is not C++).
cat > "$WORK/probes.py" <<'PYEOF'
import glob
import os
import re
import sys


def strip_cxx(text: str) -> str:
    """Remove // and /* */ comments plus "..." / '...' literals."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ''
        if c == '/' and nxt == '/':
            j = text.find('\n', i)
            i = n if j < 0 else j
            continue
        if c == '/' and nxt == '*':
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
            continue
        if c in ('"', "'"):
            quote = c
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def scope_files(scope: str):
    if os.path.isdir(scope):
        files = []
        for ext in ('*.h', '*.hpp', '*.tpp', '*.inc'):
            files.extend(glob.glob(os.path.join(scope, '**', ext), recursive=True))
        return sorted(set(files))
    if os.path.isfile(scope):
        return [scope]
    return []


def word_count(text: str, token: str) -> int:
    return len(re.findall(r'(?<![A-Za-z0-9_])' + re.escape(token) + r'(?![A-Za-z0-9_])', text))


def main() -> int:
    tsv = sys.argv[1]
    rows = []
    for raw in open(tsv, encoding='utf-8'):
        line = raw.rstrip('\n')
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        parts = line.split('\t')
        if len(parts) != 5:
            print(f"FAIL R5 探针行不是 5 列（{len(parts)}）：{line}")
            continue
        rows.append(parts)

    if len(rows) != int(sys.argv[2]):
        print(f"FAIL R6 探针数 = {len(rows)}，锚点 EXPECTED_PROBES = {sys.argv[2]}"
              "（增删探针 MUST 同步锚点）")

    for pid, kind, scope, extra, evidence in rows:
        if kind == 'nofile':
            hits = glob.glob(scope)
            if hits:
                print(f"FAIL R5 [{pid}] nofile：仍存在文件 {hits[:3]}")
            continue
        files = scope_files(scope)
        if not files:
            print(f"FAIL R5 [{pid}] 搜索面不存在或为空：{scope}")
            continue
        blob = ''
        for f in files:
            try:
                blob += strip_cxx(open(f, encoding='utf-8', errors='replace').read()) + '\n'
            except OSError:
                pass
        if kind == 'removed':
            if pid in blob:
                print(f"FAIL R5 [{pid}] removed：符号仍存在于 {scope}（应已移除）")
        elif kind == 'renamed':
            if word_count(blob, pid) != 0:
                print(f"FAIL R5 [{pid}] renamed：旧名仍在 {scope}")
            if word_count(blob, extra) == 0:
                print(f"FAIL R5 [{pid}] renamed：新名 `{extra}` 在 {scope} 中不存在")
        elif kind == 're':
            if re.search(extra, blob):
                print(f"FAIL R5 [{pid}] re：`{extra}` 仍有匹配（{scope}）")
        else:
            print(f"FAIL R5 [{pid}] 未知 kind：{kind}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
PYEOF

# ---------------------------------------------------------------------------
# R1: archived changes must carry a non-empty `## 用户可见变更` section
# ---------------------------------------------------------------------------
check_r1_archive_entries() {
    local d name date hits bad=0 g skip
    # 祖父条款 MUST 双向：登记了就必须真的存在（否则是「静默豁免池」）
    for g in "${GRANDFATHERED[@]}"; do
        if [ ! -d "$ARCHIVE_DIR/$g" ]; then
            fail "R1 祖父条款条目不存在：$ARCHIVE_DIR/$g（删掉该条目）"
            bad=1
        fi
    done
    for d in "$ARCHIVE_DIR"/*/; do
        [ -d "$d" ] || continue
        name="$(basename "$d")"
        date="${name:0:10}"
        if [[ "$date" < "$MECHANISM_DATE" ]]; then
            continue
        fi
        skip=0
        for g in "${GRANDFATHERED[@]+"${GRANDFATHERED[@]}"}"; do
            [ "$g" = "$name" ] && skip=1 && break
        done
        [ "$skip" -eq 1 ] && continue
        hits=0
        local f
        for f in "$d"*.md; do
            [ -f "$f" ] || continue
            if awk '
                /^## 用户可见变更[[:space:]]*$/ { inseg = 1; body = 0; next }
                inseg && /^## / { inseg = 0 }
                inseg && NF > 0 && !/^<!--/ { body = 1 }
                END { exit body ? 0 : 1 }
            ' "$f"; then
                hits=1
                break
            fi
        done
        if [ "$hits" -ne 1 ]; then
            fail "R1 归档 change 缺「## 用户可见变更」段或该段为空：$name（机制生效日 = $MECHANISM_DATE）"
            bad=1
        fi
    done
    return $bad
}

# ---------------------------------------------------------------------------
# R2: the registry keeps a non-empty removal chapter, and its ADR citations exist
# ---------------------------------------------------------------------------
check_r2_registry() {
    local bad=0
    if [ ! -f "$REGISTRY" ]; then
        fail "R2 登记表不存在：$REGISTRY"
        return 1
    fi
    if ! grep -q "已移除 / 已改名的公开符号" "$REGISTRY"; then
        fail "R2 登记表缺「已移除 / 已改名的公开符号」章"
        bad=1
    fi
    # the removal table MUST carry exactly EXPECTED_REMOVAL_ROWS removal rows
    # ⚠️ 行首格可能含**多个** backtick（`a` / `b`）⇒ 不能要求「首格 = 单个符号」
    #    （实测：用 `^\| \`[^\`]+\` \|` 判 ⇒ 11 行只数到 **4** 行 ⇒ 判据弱化）
    local rows
    rows="$(awk '/已移除 \/ 已改名的公开符号/,/^## 维护规则/' "$REGISTRY" |
        grep -cE '^\|.*\|[[:space:]]*\*\*(移除|改名|物理删除)\*\*')"
    if [ "${rows:-0}" -ne "$EXPECTED_REMOVAL_ROWS" ]; then
        fail "R2 移除记录表行数 = ${rows:-0}，锚点 EXPECTED_REMOVAL_ROWS = $EXPECTED_REMOVAL_ROWS（增删记录 MUST 同步锚点）"
        bad=1
    fi
    local adr n f found
    for adr in $(awk '/已移除 \/ 已改名的公开符号/,/^## 维护规则/' "$REGISTRY" |
        grep -oE 'ADR-[0-9]{3}' | sort -u); do
        n="${adr#ADR-}"
        found=0
        for f in docs/adr/"$adr"-*.md; do
            [ -f "$f" ] && found=1 && break
        done
        if [ "$found" -ne 1 ]; then
            fail "R2 登记表引用的 $adr 不存在于 docs/adr/（编号 $n）"
            bad=1
        fi
    done
    return $bad
}

# ---------------------------------------------------------------------------
# R3: AGENTS.md carries the verdict boundary and the AUTO block still holds
# ---------------------------------------------------------------------------
check_r3_agents_boundary() {
    local bad=0
    if ! grep -qF "$AGENTS_TOKEN" "$AGENTS"; then
        fail "R3 AGENTS.md 的 ZERO-DEBT 段缺判决边界句：$AGENTS_TOKEN"
        bad=1
    fi
    if ! bash scripts/verify_roadmap_fragments.sh >/dev/null 2>&1; then
        fail "R3 scripts/verify_roadmap_fragments.sh 不绿（AGENTS.md AUTO 块被扰动？）"
        bad=1
    fi
    return $bad
}

# ---------------------------------------------------------------------------
# R4: the CHANGELOG carrier entry with the token list (not a date check)
# ---------------------------------------------------------------------------
check_r4_changelog() {
    local bad=0 t
    if [ ! -f "$CHANGELOG" ]; then
        fail "R4 $CHANGELOG 不存在"
        return 1
    fi
    if ! grep -qE "^## $MECHANISM_DATE" "$CHANGELOG"; then
        fail "R4 $CHANGELOG 缺 $MECHANISM_DATE 的载体条目"
        bad=1
    fi
    for t in "${CHANGELOG_TOKENS[@]}"; do
        if ! grep -qF "$t" "$CHANGELOG"; then
            fail "R4 $CHANGELOG 缺 token：$t"
            bad=1
        fi
    done
    return $bad
}

# ---------------------------------------------------------------------------
# R5 + R6: machine probes hold, and the shrinkage anchors are intact
# ---------------------------------------------------------------------------
check_r5_probes() {
    local bad=0 out
    if [ ! -f "$PROBES" ]; then
        fail "R5 探针文件不存在：$PROBES"
        return 1
    fi
    out="$(python3 "$WORK/probes.py" "$PROBES" "$EXPECTED_PROBES" 2>&1)"
    if [ -n "$out" ]; then
        while IFS= read -r line; do
            [ -n "$line" ] && fail "$line"
        done <<< "$out"
        bad=1
    fi
    # R6: every probe id MUST appear verbatim in the registry markdown
    local id
    while IFS=$'\t' read -r id _rest; do
        case "$id" in ''|'#'*) continue ;; esac
        if ! grep -qF "$id" "$REGISTRY"; then
            fail "R6 探针 id 未出现在登记表里：$id（删表行但留探针 ⇒ 红）"
            bad=1
        fi
    done < <(grep -vE '^[[:space:]]*(#|$)' "$PROBES")
    return $bad
}

run_all_rules() {
    check_r1_archive_entries
    check_r2_registry
    check_r3_agents_boundary
    check_r4_changelog
    check_r5_probes
}

run_rules_quietly() {
    local saved=("${FAILURES[@]+"${FAILURES[@]}"}")
    FAILURES=()
    run_all_rules
    local rc=0
    [ "${#FAILURES[@]}" -gt 0 ] && rc=1
    FAILURES=("${saved[@]+"${saved[@]}"}")
    return $rc
}

# ---------------------------------------------------------------------------
# --self-test: every rule MUST have discriminating power
#    (poison the source IN PLACE => the rule MUST go RED => restore)
# ---------------------------------------------------------------------------
self_test() {
    # ⚠️ `backup` / `poisoned` MUST be GLOBAL: a `local` is invisible to the
    #    EXIT trap => `set -u` turns the restore into "unbound variable" => the
    #    source stays POISONED (learned the hard way in
    #    scripts/verify_chlib_dtor_composability.sh).
    backup="$WORK/backup"
    mkdir -p "$backup"
    poisoned=0

    restore() {
        if [ "${poisoned:-0}" = "1" ]; then
            cp "$backup/AGENTS.md" "$AGENTS" 2>/dev/null
            cp "$backup/CHANGELOG.md" "$CHANGELOG" 2>/dev/null
            cp "$backup/state_machine.h" "$HEADER" 2>/dev/null
            cp "$backup/removals.tsv" "$PROBES" 2>/dev/null
            cp "$backup/registry.md" "$REGISTRY" 2>/dev/null
            poisoned=0
        fi
        return 0
    }
    trap 'restore; cleanup' EXIT

    HEADER="include/chlib/state_machine.h"
    cp "$AGENTS" "$backup/AGENTS.md"
    cp "$CHANGELOG" "$backup/CHANGELOG.md"
    cp "$HEADER" "$backup/state_machine.h"
    cp "$PROBES" "$backup/removals.tsv"
    cp "$REGISTRY" "$backup/registry.md"

    # M1: drop the verdict boundary sentence => R3 MUST go red
    poisoned=1
    python3 - "$AGENTS" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
token = "零「仓内」消费者的「公开」符号不算 dead code"
if token not in s:
    sys.exit(3)
open(p, 'w', encoding='utf-8').write(s.replace(token, "__removed_token__"))
PY
    if [ $? -eq 3 ]; then
        echo "❌ self-test: M1 poisoning did not apply (AGENTS.md token missing?)" >&2
        return 1
    fi
    if run_rules_quietly; then
        echo "❌ self-test: M1 (drop the AGENTS boundary sentence) MUST make R3 red" >&2
        return 1
    fi
    cp "$backup/AGENTS.md" "$AGENTS"

    # M2: drop a CHANGELOG carrier token => R4 MUST go red
    poisoned=1
    python3 - "$CHANGELOG" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
if "resolve_hold" not in s:
    sys.exit(3)
open(p, 'w', encoding='utf-8').write(s.replace("resolve_hold", "__removed_token__"))
PY
    if [ $? -eq 3 ]; then
        echo "❌ self-test: M2 poisoning did not apply (CHANGELOG token missing?)" >&2
        return 1
    fi
    if run_rules_quietly; then
        echo "❌ self-test: M2 (drop a CHANGELOG token) MUST make R4 red" >&2
        return 1
    fi
    cp "$backup/CHANGELOG.md" "$CHANGELOG"

    # M3: put a recorded-as-removed symbol BACK into the header => R5 MUST go red
    poisoned=1
    printf '\nvoid is_built_probe_poison();\nvoid tick_state_probe_poison();\n' >> "$HEADER"
    python3 - "$HEADER" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
# reintroduce the removed symbols as REAL code (not a comment)
s = s.replace("void is_built_probe_poison();", "bool is_built() const;")
open(p, 'w', encoding='utf-8').write(s)
PY
    if run_rules_quietly; then
        echo "❌ self-test: M3 (reintroduce a removed public symbol) MUST make R5 red" >&2
        return 1
    fi
    cp "$backup/state_machine.h" "$HEADER"

    # M4: delete one probe row => R6 anchor MUST go red
    poisoned=1
    grep -v $'^tick_state\t' "$backup/removals.tsv" > "$PROBES"
    if cmp -s "$backup/removals.tsv" "$PROBES"; then
        echo "❌ self-test: M4 poisoning did not apply (tick_state row missing?)" >&2
        return 1
    fi
    if run_rules_quietly; then
        echo "❌ self-test: M4 (delete a probe row) MUST make R6 red" >&2
        return 1
    fi
    cp "$backup/removals.tsv" "$PROBES"

    # M5: delete a row from the human removal table => R2 anchor MUST go red
    poisoned=1
    grep -v 'tick_state()' "$backup/registry.md" > "$REGISTRY"
    if cmp -s "$backup/registry.md" "$REGISTRY"; then
        echo "❌ self-test: M5 poisoning did not apply (tick_state row missing?)" >&2
        return 1
    fi
    if run_rules_quietly; then
        echo "❌ self-test: M5 (delete a removal-table row) MUST make R2 red" >&2
        return 1
    fi
    cp "$backup/registry.md" "$REGISTRY"

    poisoned=0
    echo "✅ self-test: every rule has discriminating power (M1–M5 each turned the guard RED)"
    return 0
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
    echo "❌ verify_public_api_visibility: ${#FAILURES[@]} violation(s)" >&2
    for f in "${FAILURES[@]}"; do
        echo "   - $f" >&2
    done
    exit 1
fi

echo "✅ verify_public_api_visibility: PASS (R1 归档载体 · R2 移除登记 · R3 判决边界 · R4 CHANGELOG 载体 · R5 机械探针 · R6 缩面锚点)"
exit 0
