#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# measure_new_input_ports.sh —— **实测**：每个 commit **新增**了多少个 `ch_in` 端口
#
# ⚠️ **为什么要有它**（2026-11-03）：
#   「新增组件输入端口 ⇒ 全部实例化点 MUST 显式驱动」这条义务（根 `AGENTS.md`
#   §ANTI-PATTERNS）此前**没有任何自动化护栏** ⇒ 而 2026-10-28 的 spike 结论是
#   「**通用状态型护栏不可行**」⇒ ⇒ ⇒ 剩下的可行路线是**事件驱动（git diff）增量
#   检查** ⇒ ⇒ ⇒ 而那条路线的**成本论证完全依赖一个数字**：「典型 commit 新增几个
#   `ch_in`？」⇒ ⇒ ⇒ **那个数字此前只存在于台账正文**（不可复算）⇒ ⇒ 本脚本把它
#   变成**仓内可复算**的证据。
#
# ⚠️ **口径**（与台账 §1 的历史实测一致）：
#   · 统计范围 = **新增行**（`git show --unified=0` 的 `+` 行）里形如
#     `ch_in<...> name;` / `ch_in<...> name{...}` 的**声明** ⇒ ⇒ 两种声明形态
#     （裸声明与 `__io(...)` 宏块内）**同一正则**覆盖（宏块内的声明也是 `ch_in<...> x;`）。
#   · **不**区分 `ch_in` 与 `ch_out`（`ch_out` 的正则不含 `ch_in` ⇒ 天然不误计）。
#   · 计数单位 = **声明个数**（不是文件数、不是 commit 数）。
#   · 只看**新增**（`+`）行 ⇒ ⇒ 「改名 / 移动」不算新增 ⇒ 与台账口径一致。
#
# ⚠️ **为什么这个数字重要**：它决定「增量检查」这条护栏的成本量级
#   ⇒ ⇒ 典型负载 **0–2** ⇒ ⇒ 成本趋零；唯一的大额是**一次性单源化重构**
#   ⇒ ⇒ 那类 commit 只需要「列出来不判定」（半自动）。
#
# 用法：
#   scripts/measure_new_input_ports.sh              # 最近 100 个 commit
#   scripts/measure_new_input_ports.sh 200          # 最近 200 个 commit
#   scripts/measure_new_input_ports.sh 100 --list   # 额外列出「新增 > 0」的 commit
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
N="${1:-100}"
LIST="${2:-}"

cd "$REPO_ROOT" || exit 2

if ! git rev-parse --git-dir >/dev/null 2>&1; then
    echo "ERROR: 不是 git 仓库 ⇒ 无法测量（fail-closed）" >&2
    exit 2
fi

# 只取**主线** commit（merge commit 的 diff 无意义 ⇒ 用 --no-merges）
mapfile -t shas < <(git log --no-merges --format=%H -n "$N")
if [ "${#shas[@]}" -eq 0 ]; then
    echo "ERROR: 取不到 commit（git 历史不足）⇒ 无法测量（fail-closed）" >&2
    exit 2
fi

declare -A hist=()
declare -A hist_prod=()
outliers=()
zero=0

# 计数一个 commit 的新增 ch_in 声明数；$2 = 额外路径过滤（空 = 全部文件）
count_new() {
    local sha="$1" scope="${2:-}"
    git show --unified=0 --format= "$sha" -- $scope 2>/dev/null \
        | grep -E '^\+' \
        | grep -E '\bch_in\s*<[^;]*>\s*[A-Za-z_][A-Za-z0-9_]*\s*(;|\{)' \
        | wc -l
}

for sha in "${shas[@]}"; do
    n=$(count_new "$sha")
    hist[$n]=$(( ${hist[$n]:-0} + 1 ))
    np=$(count_new "$sha" "include src")
    hist_prod[$np]=$(( ${hist_prod[$np]:-0} + 1 ))
    if [ "$n" -eq 0 ]; then
        zero=$((zero + 1))
    else
        short=$(git show -s --format='%h %ad %s' --date=short "$sha")
        outliers+=("$n|$short")
    fi
done

total=${#shas[@]}
echo "=== 新增 ch_in 端口的分布（最近 ${total} 个非 merge commit）==="
echo "【口径 A】**全部文件**（include + src + tests + examples + samples）"
for k in $(printf '%s\n' "${!hist[@]}" | sort -n); do
    printf "  %3s 个 ⇒ %s 个 commit\n" "$k" "${hist[$k]}"
done
echo
echo "【口径 B】**仅生产代码**（include + src）—— ⇒ 「新增组件端口」的义务只落在这里"
for k in $(printf '%s\n' "${!hist_prod[@]}" | sort -n); do
    printf "  %3s 个 ⇒ %s 个 commit\n" "$k" "${hist_prod[$k]}"
done
echo
echo "  —— 合计：口径 A ${total} 个 commit / 口径 B ${total} 个 commit"
echo "  —— 口径 A 里 0 个（零新增）的 commit 数 = ${zero} ⇒ ⇒ 占比 $(awk -v z=$zero -v t=$total 'BEGIN{printf "%.1f%%", 100*z/t}')"

if [ "$LIST" = "--list" ]; then
    echo
    echo "=== 新增 > 0 的 commit ==="
    for line in "${outliers[@]}"; do
        n="${line%%|*}"
        rest="${line#*|}"
        printf "  %4s 个  %s\n" "$n" "$rest"
    done
fi

echo
echo "⚠️ **口径提醒**：这是**声明个数**（新增行里的 ch_in<...> name; 形态）⇒ ⇒"
echo "   「典型负载」= **0–2 个/commit** ⇒ ⇒ 事件驱动的增量检查成本趋零；"
echo "   大额只出现在**一次性重构**（那种 commit 只需「列出来不判定」）。"
