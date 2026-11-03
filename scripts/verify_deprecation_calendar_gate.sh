#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# verify_deprecation_calendar_gate.sh —— **HDL 逻辑算子弃用的「日历门」**护栏
#
# ⚠️ **为什么要有它**（2026-11-03）：
#   ADR-036 §8.1 写的是「**2027-04-17 前拒绝任何移除 commit**」⇒ ⇒ 但这条
#   **日历锁此前纯靠自觉**（没有任何脚本判它）⇒ ⇒ 有人提前把 8 个 overload 删掉，
#   CI **不会红** ⇒ ⇒ 本护栏把日历锁变成**机械信号**。
#
# ⚠️ **判据**：
#   · 弃用标记仍在位 ⇒ **未移除** ⇒ 绿（并打印最早允许移除日）
#   · 标记已消失 ⇒ 判定**移除日期**：
#       ① 工作树已删、HEAD 仍有 ⇒ **未提交的移除** ⇒ 日期 = 今天
#       ② 否则 ⇒ 在 git 历史里找「标记由在位变为消失」的那个 commit ⇒ 用它的日期
#   · 生效期限 = 最早允许移除日（2027-04-17），
#     **低活动条款**（§8.2）：移除日之前 `[2026-10-16, 移除日]` 的 commit 数 < 5
#     ⇒ 顺延 3 个月 ⇒ 2027-07-17
#   · 移除日期 **早于**生效期限 ⇒ **红**
#
# ⚠️ **fail-closed**：标记已消失但**判定不出移除日期**（git 历史不足 / 浅克隆）
#   ⇒ ⇒ **红**（无法证明合规 ⇒ 不得静默通过）。
#
# ⚠️ **消息里 MUST NOT 含反引号** —— shell 双引号里的反引号会被当**命令替换**
#   ⇒ ⇒ 护栏消息被**静默吞掉**（本仓已踩过）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/deprecation_callsites.py"
REGISTRY="$REPO_ROOT/.rddf/checks/hdl-logical-deprecation-registry.md"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0

ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS+1)); }
note() { printf "  ${YELLOW}::notice %s${RESET}\n" "$*"; }

# ── R0：fail-closed（缺文件 ⇒ MUST NOT 静默通过）──────────────────────────
echo "── R0：必需文件齐备（fail-closed）"
missing=""
[ -f "$ANALYZER" ] || missing="$missing 分析器"
[ -f "$REGISTRY" ] || missing="$missing 登记表"
if [ -n "$missing" ]; then
    fail "R0 缺少必需文件:${missing}"
    exit 1
fi
ok "R0 必需文件齐备（fail-closed）"

# ── R1：日历门判定 ───────────────────────────────────────────────────────
echo "── R1：移除门（日期 vs 生效期限）"
out="$(python3 "$ANALYZER" calendar-check 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
if [ $rc -ne 0 ]; then
    fail "移除门核验未通过（exit=$rc）"
    exit 1
fi
ok "R1 移除门核验通过"

# ─────────────────────────────────────────────────────────────────────────────
# 自检：原地投毒 ⇒ 每个 case 有**明确的期望**（红 / 绿）
#   `calendar-remove`        ⇒ MUST 转红（未提交的移除 ⇒ 早于期限）
#   `calendar-widen-deadline`⇒ MUST 转绿（反向判据：把期限改到过去 ⇒ 合规）
#     ⚠️ 反向判据的价值：证明「红」的成因是**日期**，而不是「标记消失」本身。
if [ "${1:-}" = "--self-test" ]; then
    echo "── 自检：原地投毒（每个 case 有明确期望）"
    declare -A EXPECT=(
        [calendar-remove]=red
        [calendar-widen-deadline]=green
    )
    for c in "${!EXPECT[@]}"; do
        exp="${EXPECT[$c]}"
        if ! python3 "$ANALYZER" poison "$c" >/dev/null 2>&1; then
            fail "自检：投毒 ${c} 失败（⇒ 判据可能无判别力）"
            python3 "$ANALYZER" restore >/dev/null 2>&1 || true
            continue
        fi
        if python3 "$ANALYZER" calendar-check >/dev/null 2>&1; then
            got=green
        else
            got=red
        fi
        if [ "$got" = "$exp" ]; then
            ok "自检：投毒 ${c} ⇒ 核验为 ${got}（与期望一致）"
        else
            fail "自检：投毒 ${c} ⇒ 核验为 ${got}，期望 ${exp} ⇒ 判据行为不符"
        fi
        python3 "$ANALYZER" restore >/dev/null 2>&1 || fail "自检：还原 ${c} 失败"
        if python3 "$ANALYZER" calendar-check >/dev/null 2>&1; then
            ok "自检：还原 ${c} 后核验复绿"
        else
            fail "自检：还原 ${c} 后核验仍为红 ⇒ 还原写坏了文件"
        fi
        if git -C "$REPO_ROOT" diff --quiet -- \
                include/core/operators_logic.h include/core/io_logical.h \
                .rddf/checks/hdl-logical-deprecation-registry.md; then
            ok "自检：${c} 还原后与 HEAD 逐字相同"
        else
            fail "自检：${c} 还原后与 HEAD 不一致 ⇒ 工作树被污染"
        fi
    done
    if [ "$FAILS" -ne 0 ]; then
        fail "自检未全部通过（${FAILS} 项）"
        exit 1
    fi
    ok "自检全部通过（${#EXPECT[@]} 个 case）"
fi

if [ "$FAILS" -ne 0 ]; then
    fail "verify_deprecation_calendar_gate: 有违规"
    exit 1
fi
echo "${GREEN}✅ verify_deprecation_calendar_gate: PASS${RESET}"
