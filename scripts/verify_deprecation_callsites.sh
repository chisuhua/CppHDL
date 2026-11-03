#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# verify_deprecation_callsites.sh —— **HDL 逻辑算子（`&&` / `||`）弃用状态**的
# 常态化护栏（ADR-036 落地后的「0 新增调用点」判据）
#
# ⚠️ **为什么要有它**（2026-11-03）：
#   ADR-036 把 8 个 `ch_bool` / `port<ch_bool>` 的 `&&` / `||` overload 标记为
#   `[[deprecated]]`，并把「**0 新增调用点**」写成 phase-5 的完成条件 ⇒ ⇒ 但
#   **没有任何 CI 判据守它** ⇒ ⇒ 「完成条件」只是一次性人工核对 ⇒ ⇒ 今后新增
#   一处误用，CI **不会红**（只会在编译日志里留一条 deprecation 警告，而当前
#   构建不是 `-Werror`）⇒ ⇒ 本护栏把该条件变成 **CI 可检**。
#
# ⚠️ **判据（委派给 Python 分析器）**：
#   R1 8 个 `[[deprecated]]` 标记仍在位（且两个头里的总数恰好 = 8 ⇒ 漏登记也红）
#   R2 全仓扫描到的弃用调用点 **全部**在登记表的豁免清单里 ⇒ ⇒ **0 新增调用点**
#   R3 登记表里的每条豁免 MUST 仍能在扫描结果里找到 ⇒ ⇒ 双向核对（豁免不得过时）
#
# ⚠️ **判别力方向**：**宁可漏报、不误报** ⇒ 分析器按括号配平取操作数、
#   把「比较表达式」的结果判为 `bool` ⇒ ⇒ 2026-11-03 实测：全仓命中 7 处，
#   全部是登记在案的豁免（3 条 SEGV 复现 + 4 条 mixed nonzero-test 语义覆盖）。
#
# ⚠️ **消息里 MUST NOT 含反引号** —— shell 双引号里的反引号会被当**命令替换**
#   ⇒ ⇒ 护栏消息被**静默吞掉**（本仓已踩过）。
#
# ⚠️ **自检是「原地投毒」**：投毒会改**受版本控制的源文件** ⇒ ⇒ 还原后 MUST
#   确认「文件与 HEAD 逐字相同」（否则下一轮跑的是被投毒的树）。

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
[ -f "$ANALYZER" ]   || missing="$missing 分析器"
[ -f "$REGISTRY" ]   || missing="$missing 登记表"
if [ -n "$missing" ]; then
    fail "R0 缺少必需文件:${missing}"
    exit 1
fi
ok "R0 必需文件齐备（fail-closed）"

# ── R1/R2/R3：判据（委派给 Python 分析器）────────────────────────────────
echo "── R1/R2/R3：弃用标记 / 0 新增调用点 / 豁免登记"
out="$(python3 "$ANALYZER" check 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
if [ $rc -ne 0 ]; then
    fail "弃用调用点核验未通过（exit=$rc）"
    exit 1
fi
ok "R1/R2/R3 弃用调用点核验通过"

# ─────────────────────────────────────────────────────────────────────────────
# 自检：**原地**投毒，每种 MUST 让核验转红；还原后 MUST 再确认一次绿
# ⚠️ 投毒数 MUST **动态**取（`list-poisons` 的行数）⇒ ⇒ 硬编码会漂移
if [ "${1:-}" = "--self-test" ]; then
    echo "── 自检：原地投毒（每种 MUST 转红，还原后 MUST 复绿）"
    mapfile -t cases < <(python3 "$ANALYZER" list-poisons | cut -f1)
    if [ "${#cases[@]}" -eq 0 ]; then
        fail "自检：投毒清单为空 ⇒ 无法自检（fail-closed）"
        exit 1
    fi
    for c in "${cases[@]}"; do
        if ! python3 "$ANALYZER" poison "$c" >/dev/null 2>&1; then
            fail "自检：投毒 ${c} 失败（⇒ 判据可能无判别力）"
            python3 "$ANALYZER" restore >/dev/null 2>&1 || true
            continue
        fi
        if python3 "$ANALYZER" check >/dev/null 2>&1; then
            fail "自检：投毒 ${c} 后核验仍为绿 ⇒ 判据无判别力"
        else
            ok "自检：投毒 ${c} ⇒ 核验转红（判别力成立）"
        fi
        python3 "$ANALYZER" restore >/dev/null 2>&1 || fail "自检：还原 ${c} 失败"
        if python3 "$ANALYZER" check >/dev/null 2>&1; then
            ok "自检：还原 ${c} 后核验复绿"
        else
            fail "自检：还原 ${c} 后核验仍为红 ⇒ 还原写坏了文件"
        fi
        # 还原后 MUST 与 HEAD 逐字相同（否则下一轮跑的是被投毒的树）
        if git -C "$REPO_ROOT" diff --quiet -- \
                include/core/operators_logic.h include/core/io_logical.h \
                tests/test_basic.cpp \
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
    ok "自检全部通过（${#cases[@]} 种投毒）"
fi

if [ "$FAILS" -ne 0 ]; then
    fail "verify_deprecation_callsites: 有违规"
    exit 1
fi
echo "${GREEN}✅ verify_deprecation_callsites: PASS${RESET}"
