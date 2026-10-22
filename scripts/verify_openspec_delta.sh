#!/usr/bin/env bash
# scripts/verify_openspec_delta.sh
#
# 提案阶段的 **delta 自检**（4 条）—— 把 2026-10-22 两次 change 的四轮
# 对抗式审稿里**反复出现**的同类错误固化成自动化护栏。
#
# 规则（详见 docs/developer_guide/openspec-specs-guide.md 的同名章节）：
#
#   R1  delta 内 MUST NOT 出现 **change 名**
#       （archive 会把 delta **逐字**并入主 spec ⇒ 主 spec 残留 change 名；
#         仓库既有约定：主 spec 内的 change 名引用必须全部改为指向 capability）
#
#   R2  delta 内 MUST NOT 出现**占位符路径**（形如
#       `docs/reports/verification-<change-name>.md`）
#       （占位符在归档后仍是占位符 ⇒ 主 spec 里的路径**不可寻址**）
#       ⚠️ 只匹配**像路径的**占位符 —— C++ 模板语法（`ch_in<ch_bool>`、
#          `bundle_base<Derived>`）是**合法**的，不在此列。
#
#   R3  `## MODIFIED Requirements` 的每条 Requirement **标题 MUST 与主 spec
#       逐字一致**（不一致 ⇒ CLI 匹配不到 ⇒ **静默不合并** —— 最坏的失败
#       模式：看起来归档成功了，主 spec 却没变）
#
#   R4  change 的 tasks.md 里每个 ```bash 块 MUST **语法可达**
#       （`bash -n`）—— 「按字面执行必红」的 guard 是最常见的自欺。
#       ⚠️ `bash -n` 只做**语法**检查（不执行）⇒ 它抓的是「块本身写坏了」
#          那一类；「语义上必红」仍需人工核对。
#
# Exit codes: 0 = 全部通过, 1 = 至少一处违规。
#
# Usage: scripts/verify_openspec_delta.sh [change-name ...]
#        （不给参数 ⇒ 检查 openspec/changes/ 下的**全部** active change）

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

CHANGES_DIR="$REPO_ROOT/openspec/changes"
SPECS_DIR="$REPO_ROOT/openspec/specs"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

violations=0

note() { printf "  %s\n" "$1"; }
violation() {
    printf "${RED}VIOLATION${NC} %s\n" "$1" >&2
    violations=$((violations + 1))
}
if [ ! -d "$CHANGES_DIR" ]; then
    echo "::warning::$CHANGES_DIR not found; nothing to check"
    exit 0
fi

# ---------------------------------------------------------------------------
# 收集待检查的 change
# ---------------------------------------------------------------------------
if [ "$#" -gt 0 ]; then
    CHANGES=("$@")
else
    CHANGES=()
    for d in "$CHANGES_DIR"/*/; do
        name=$(basename "$d")
        [ "$name" = "archive" ] && continue
        [ -d "$d" ] || continue
        CHANGES+=("$name")
    done
fi

if [ "${#CHANGES[@]}" -eq 0 ]; then
    echo "no active OpenSpec change to check ✓"
    exit 0
fi

echo "=========================================="
echo " OpenSpec delta 自检（4 条）"
echo "=========================================="
echo "Changes: ${CHANGES[*]}"
echo ""

for change in "${CHANGES[@]}"; do
    dir="$CHANGES_DIR/$change"
    if [ ! -d "$dir" ]; then
        echo "::error::change '$change' not found under openspec/changes/"
        violations=$((violations + 1))
        continue
    fi

    echo "--- $change ---"

    # ---- R1: delta 内不得出现 change 名 ----------------------------------
    r1_hits=0
    if [ -d "$dir/specs" ]; then
        while IFS= read -r f; do
            [ -n "$f" ] || continue
            # 只看**会被并入主 spec**的部分：## ADDED/MODIFIED/REMOVED/RENAMED
            # 之后的正文（`## Purpose` 不会被合并 ⇒ 不算违规）
            body=$(awk '/^## (ADDED|MODIFIED|REMOVED|RENAMED) Requirements/ {on=1} on' "$f")
            if printf '%s' "$body" | grep -qF "$change"; then
                violation "R1 $f: delta 正文含 change 名 '$change'（归档后会残留进主 spec）"
                r1_hits=$((r1_hits + 1))
            fi
        done < <(find "$dir/specs" -name 'spec.md' 2>/dev/null)
    fi
    [ "$r1_hits" -eq 0 ] && note "R1 ✓ delta 正文无 change 名"

    # ---- R2: delta 内不得出现占位符路径 ----------------------------------
    r2_hits=0
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        # 反引号里含 `<...>` **且**含路径特征（`/` 或常见扩展名）
        while IFS= read -r line; do
            [ -n "$line" ] || continue
            violation "R2 $f: 占位符路径 ⇒ $line"
            r2_hits=$((r2_hits + 1))
        done < <(grep -oE '`[^`]*<[^`>]*>[^`]*`' "$f" 2>/dev/null | \
                 grep -E '/|\.(md|sh|cpp|h|hpp|txt|json|yaml|yml)$' || true)
    done < <(find "$dir" -name 'spec.md' -path '*/specs/*' 2>/dev/null)
    [ "$r2_hits" -eq 0 ] && note "R2 ✓ delta 无占位符路径"

    # ---- R3: MODIFIED 标题 MUST 与主 spec 逐字一致 ------------------------
    r3_hits=0
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        capability=$(basename "$(dirname "$f")")
        main_spec="$SPECS_DIR/$capability/spec.md"
        mod_titles=$(awk '
            /^## MODIFIED Requirements/ {on=1; next}
            /^## (ADDED|REMOVED|RENAMED) Requirements/ {on=0}
            on && /^### Requirement: / {sub(/^### Requirement: /, ""); print}
        ' "$f")
        [ -n "$mod_titles" ] || continue
        if [ ! -f "$main_spec" ]; then
            violation "R3 $capability: 主 spec 不存在（$main_spec）⇒ MODIFIED 无处合并"
            r3_hits=$((r3_hits + 1))
            continue
        fi
        while IFS= read -r title; do
            [ -n "$title" ] || continue
            if ! grep -qxF "### Requirement: $title" "$main_spec"; then
                violation "R3 $capability: MODIFIED 标题与主 spec 不逐字一致 ⇒ CLI 会静默不合并 ⇒「$title」"
                r3_hits=$((r3_hits + 1))
            fi
        done <<< "$mod_titles"
    done < <(find "$dir" -name 'spec.md' -path '*/specs/*' 2>/dev/null)
    [ "$r3_hits" -eq 0 ] && note "R3 ✓ MODIFIED 标题与主 spec 逐字一致"

    # ---- R4: tasks.md 的 bash 块 MUST 语法可达 ----------------------------
    r4_hits=0
    tasks="$dir/tasks.md"
    if [ -f "$tasks" ]; then
        tmp=$(mktemp)
        # 抽出所有 ```bash ... ``` 块，**逐块**用哨兵行分隔
        # ⚠️ tasks.md 的代码块在 markdown 列表里是**缩进**的 ⇒ fence 模式必须
        #    允许前导空白（否则抽不到任何块 ⇒ 本规则变成**零信号**的假绿）。
        awk '
            /^[ \t]*```bash[ \t]*$/ {on=1; next}
            /^[ \t]*```[ \t]*$/ {if (on) {on=0; print "<<<BLOCK_END>>>"}; next}
            on {print}
        ' "$tasks" > "$tmp"
        # fence 数量对照：若文件里有 bash fence 但抽不出内容 ⇒ 抽取逻辑坏了
        fence_count=$(grep -cE '^[ \t]*```bash[ \t]*$' "$tasks" || true)
        if [ "${fence_count:-0}" -gt 0 ] && ! grep -q '<<<BLOCK_END>>>' "$tmp"; then
            violation "R4 $tasks: 有 $fence_count 个 bash 块但**一个都没抽到** ⇒ 抽取逻辑坏了（本规则会假绿）"
            r4_hits=$((r4_hits + 1))
        fi
        n=0
        cur=$(mktemp)
        while IFS= read -r line; do
            if [ "$line" = "<<<BLOCK_END>>>" ]; then
                n=$((n + 1))
                if [ -s "$cur" ] && ! bash -n "$cur" 2>/tmp/opencode_delta_err; then
                    violation "R4 $tasks: 第 $n 个 bash 块语法不可达 ⇒ $(head -1 /tmp/opencode_delta_err)"
                    r4_hits=$((r4_hits + 1))
                fi
                : > "$cur"
            else
                printf '%s\n' "$line" >> "$cur"
            fi
        done < "$tmp"
        rm -f "$cur"
        [ "$n" -gt 0 ] && note "  （R4 检查了 $n 个 bash 块）"
        rm -f "$tmp"
    fi
    [ "$r4_hits" -eq 0 ] && note "R4 ✓ tasks.md 的 bash 块语法可达"

    echo ""
done

if [ "$violations" -gt 0 ]; then
    echo "::error::$violations violation(s) in the OpenSpec deltas"
    exit 1
fi

echo "OpenSpec delta 自检通过（4 条全绿）✓"
exit 0
