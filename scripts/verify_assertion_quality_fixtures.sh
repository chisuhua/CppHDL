#!/usr/bin/env bash
# scripts/verify_assertion_quality_fixtures.sh
#
# **fixture 自检**：`assertion quality` 护栏的**规格**（change
# `example-assertion-quality-2026-10` 的 Phase 1 / T1.2）。
#
# ⚠️ 为什么需要它（2026-10-24 C1 拆分时的原始理由）：
#    `scripts/verify_example_assertion_quality.sh` 的静态分析器**手写**，
#    而 C1 在连续三轮对抗审稿（第 4/5/6 轮）里**全部** BLOCKER 都出在它身上：
#      第 4 轮：用「`REQUIRE` 词频」冒充「可执行断言」（数的是行、只剥整行注释）
#      第 5 轮：`#define` 宏体 / `#if 1` 死分支可**灌计数**（未建模预处理指令）
#      第 6 轮：`#elif` 死分支被计入；`#ifdef` 的 `#else` 被当活代码
#    ⇒ 护栏进了 CI 却**没有任何可执行的期望值** ⇒ 「它到底认什么」只有散文描述。
#    ⇒ 本脚本把每条「机制 → 期望计数」**固化成可执行的期望值**。
#
# ⚠️ **单一实现**：分析与 fail-closed 判定都在 `scripts/lib/assertion_quality.sh`
#    （护栏与本脚本共用）⇒ 「护栏行为」与「规格」不可能分叉。
#
# 用法：bash scripts/verify_assertion_quality_fixtures.sh
# Exit codes: 0 = 全部通过, 1 = 至少一处违规。
#
# ⚠️ 可移植性：POSIX sh 习惯 + bash 数组 avoided；awk 仅用 POSIX 明确定义的构造。

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FIXTURE_DIR="$REPO_ROOT/tests/fixtures/assertion_quality"

RED=''
GREEN=''
YELLOW=''
NC=''
if [ -t 1 ]; then
    RED=$'\033[0;31m'; GREEN=$'\033[0;32m'; YELLOW=$'\033[0;33m'; NC=$'\033[0m'
fi

violations=0
note() { printf "  %s\n" "$1"; }
violation() { printf "${RED}VIOLATION${NC} %s\n" "$1" >&2; violations=$((violations + 1)); }

if [ ! -f "$SCRIPT_DIR/lib/assertion_quality.sh" ]; then
    printf "${RED}VIOLATION${NC} 共享 lib 缺失：%s\n" "$SCRIPT_DIR/lib/assertion_quality.sh" >&2
    exit 1
fi
# shellcheck source=scripts/lib/assertion_quality.sh
. "$SCRIPT_DIR/lib/assertion_quality.sh"

# ═══════════════════════════════════════════════════════════════════════════
# 期望值表（**规格本体**）
# 格式：<fixture>|<A 活代码断言总数>|<S 被跳过区段的用例宏数>|<E 字面用例数>|<逐用例断言数(逗号分隔)>
# ⚠️ 每一行都 MUST 与 fixture 文件头的「期望」注释**逐字一致** ⇒ 两处不一致即违规。
# ═══════════════════════════════════════════════════════════════════════════
EXPECTED="live_assert.cpp|1|0|1|1
commented_out.cpp|0|0|1|0
dead_macro.cpp|0|0|1|0
dead_elif.cpp|0|1|1|0
dead_ifdef_else.cpp|0|2|0|
live_else.cpp|1|1|1|1
live_elif.cpp|1|1|1|1
word_boundary.cpp|0|0|1|0
throws_only.cpp|1|0|1|1
succeed_only.cpp|0|0|1|0
identifier_lookalike.cpp|2|0|2|1,1"

echo "=== verify_assertion_quality_fixtures: 断言质量护栏的 fixture 自检 ==="

if [ ! -d "$FIXTURE_DIR" ]; then
    violation "fixture 目录缺失：$FIXTURE_DIR"
    printf "${RED}❌ verify_assertion_quality_fixtures: %s 处违规${NC}\n" "$violations" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# C1：逐个 fixture 的实际分析结果 MUST 与期望值一致
# ---------------------------------------------------------------------------
echo ""
echo '--- C1: 每个 fixture 的分析结果 MUST 与期望值一致（机制 → 期望计数）---'
while IFS='|' read -r fname exp_a exp_s exp_e exp_c; do
    [ -z "${fname:-}" ] && continue
    path="$FIXTURE_DIR/$fname"
    if [ ! -f "$path" ]; then
        violation "期望表里的 $fname 不存在（fixture 被删？MUST 同时改期望表）"
        continue
    fi
    out="$(analyze_source "$path" 2>/dev/null || true)"
    status="$(aq_analyze_status "$out")"
    if [ "$status" != "ok" ]; then
        violation "$fname 的分析**未完整执行**（状态 = $status）—— fixture 本身有问题"
        continue
    fi
    got_a="$(printf '%s\n' "$out" | awk '/^A /{print $2; exit}')"
    got_s="$(printf '%s\n' "$out" | awk '/^S /{print $2; exit}')"
    got_e="$(printf '%s\n' "$out" | awk '/^E /{print $2; exit}')"
    got_c="$(printf '%s\n' "$out" | awk '/^C /{printf "%s%s", sep, $2; sep=","}')"
    if [ "$got_a" != "$exp_a" ] || [ "$got_s" != "$exp_s" ] || [ "$got_e" != "$exp_e" ] || [ "$got_c" != "$exp_c" ]; then
        violation "$fname：期望 A=$exp_a S=$exp_s E=$exp_e C=[$exp_c]，实得 A=$got_a S=$got_s E=$got_e C=[$got_c]"
    else
        note "OK  $fname → A=$got_a S=$got_s E=$got_e C=[$got_c]"
    fi
done <<EOF
$EXPECTED
EOF

# ---------------------------------------------------------------------------
# C2：fixture 目录里**不得有**期望表未登记的文件（防止「新增 fixture 忘了写期望」）
# ---------------------------------------------------------------------------
echo ""
echo '--- C2: fixture 目录 MUST 无「期望表未登记」的文件 ---'
for f in "$FIXTURE_DIR"/*.cpp; do
    [ -e "$f" ] || continue
    base="$(basename "$f")"
    if ! printf '%s\n' "$EXPECTED" | grep -q "^${base}|"; then
        violation "$base 在 fixture 目录里但**未登记**期望值 —— 新增 fixture MUST 同时登记期望"
    fi
done

# ---------------------------------------------------------------------------
# C3：fail-closed 判定（分析器失效 MUST 报违规，不得打出假 OK）
#     —— change `example-assertion-quality-2026-10` 的 T3.5.1
# ---------------------------------------------------------------------------
echo ""
echo '--- C3: 分析器失效的两种形态 MUST 被判违规（fail-closed）---'
# 形态 ①：analyze_source 返回**空**（分析器完全没跑起来）
st_empty="$(aq_analyze_status '')"
if [ "$st_empty" != "empty" ]; then
    violation "空输出的状态判定错误：期望 empty，实得 '$st_empty'"
else
    reason="$(aq_violation_reason 'fixture-dummy.cpp' '')"
    if [ -z "$reason" ]; then
        violation "空输出 MUST 报违规，但 aq_violation_reason 返回空串（假绿路径）"
    else
        note "OK  空输出 → 状态 empty → 报违规"
    fi
fi
# 形态 ②：awk 输出 A/S 之后**出错退出** ⇒ 输出非空但**缺结束标记 E**
st_partial="$(aq_analyze_status 'A 3
S 0')"
if [ "$st_partial" != "no_end_marker" ]; then
    violation "缺结束标记的状态判定错误：期望 no_end_marker，实得 '$st_partial'"
else
    reason="$(aq_violation_reason 'fixture-dummy.cpp' 'A 3
S 0')"
    if [ -z "$reason" ]; then
        violation "缺结束标记 MUST 报违规，但 aq_violation_reason 返回空串（假绿路径）"
    else
        note "OK  缺结束标记 → 状态 no_end_marker → 报违规"
    fi
fi
# 对照：正常输出 MUST **不**被判违规（防止 fail-closed 变成「一律违规」）
st_ok="$(aq_analyze_status 'A 1
S 0
C 1 some case
E 1')"
if [ "$st_ok" != "ok" ]; then
    violation "正常输出的状态判定错误：期望 ok，实得 '$st_ok'"
else
    reason="$(aq_violation_reason 'fixture-dummy.cpp' 'A 1
S 0
C 1 some case
E 1')"
    if [ -n "$reason" ]; then
        violation "正常输出 MUST 不报违规，但得到：$reason"
    else
        note "OK  正常输出 → 状态 ok → 不报违规"
    fi
fi

# ---------------------------------------------------------------------------
# 汇总
# ---------------------------------------------------------------------------
echo ""
if [ "$violations" -eq 0 ]; then
    echo "${GREEN}✅ verify_assertion_quality_fixtures: PASS${NC}"
    exit 0
fi
echo "${RED}❌ verify_assertion_quality_fixtures: $violations 处违规${NC}" >&2
exit 1
