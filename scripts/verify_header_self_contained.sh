#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# verify_header_self_contained.sh —— **每个头文件 MUST 能被单独编译**的护栏
#
# ⚠️ **为什么要有它**：根 AGENTS.md 的 CONVENTIONS 明写「Headers
#    self-contained」⇒ ⇒ 但该约定**从未被机械核验** ⇒ ⇒ 于是「靠 `-I` 巧合
#    才能编译」的头**长期存在**，且：
#    ⚠️ **`#include "chlib.h"`（`include/AGENTS.md` 记载的**公共入口**）
#       单独编译失败**（2026-11-03 实测）—— 仓内**零消费者** include 它 ⇒ ⇒
#       ⇒ 这个入口**从未被验证过** ⇒ ⇒ 「公共 API 可用」这件事是**未经验证**的。
#
# ⚠️ **豁免只允许「分片头」**（被聚合头 include 后才有意义的设计）⇒ ⇒ 公共
#    入口头 MUST NOT 豁免 ⇒ ⇒ 豁免名单由 R2 **双向**核对（已过时 ⇒ 红；
#    未登记 ⇒ 红）⇒ ⇒ 豁免不会变成「万能借口」。
#
# ⚠️ **消息里 MUST NOT 含反引号** —— shell 双引号里的反引号会被当**命令替换**
#    ⇒ ⇒ 护栏消息被**静默吞掉**（本仓已踩过）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/header_self_contained.py"
EXEMPT="$REPO_ROOT/scripts/lib/header_self_contained_exempt.txt"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0

ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS+1)); }
note() { printf "  ${YELLOW}::notice %s${RESET}\n" "$*"; }

# ── R0：fail-closed（缺文件 ⇒ MUST NOT 静默通过）──────────────────────────
echo "── R0：必需文件齐备（fail-closed）"
missing=""
[ -f "$ANALYZER" ] || missing="$missing 分析器"
[ -f "$EXEMPT" ]   || missing="$missing 豁免名单"
if [ -n "$missing" ]; then
    fail "R0 缺少必需文件:${missing}"
    exit 1
fi
ok "R0 必需文件齐备（fail-closed）"

# ── R1/R2：判据（委派给 Python 分析器）────────────────────────────────────
echo "── R1/R2：每个头 MUST 能单独编译（分片头按设计豁免）"
out="$(python3 "$ANALYZER" check 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
if [ $rc -ne 0 ]; then
    fail "头文件自包含性核验未通过（exit=$rc）"
    exit 1
fi
ok "R1/R2 头文件自包含性核验通过"

# ─────────────────────────────────────────────────────────────────────────────
# 自检：**原地**投毒，每种 MUST 让核验转红；还原后 MUST 再确认一次绿
if [ "${1:-}" = "--self-test" ]; then
    echo "── 自检：原地投毒（每种 MUST 转红 ⇒ ⇒ 否则判别力为零）"
    cases=()
    while IFS= read -r line; do
        [ -n "$line" ] && cases+=("$line")
    done < <(python3 "$ANALYZER" list-poisons)

    for c in "${cases[@]}"; do
        python3 "$ANALYZER" restore >/dev/null 2>&1 || true
        if ! python3 "$ANALYZER" poison "$c" >/dev/null 2>&1; then
            fail "自检：投毒 [$c] 失败（模式未找到 ⇒ fail-closed）"
            exit 1
        fi
        if python3 "$ANALYZER" check >/dev/null 2>&1; then
            fail "自检：投毒 [$c] 后核验**仍绿** ⇒ ⇒ 判别力为零"
            python3 "$ANALYZER" restore >/dev/null 2>&1 || true
            exit 1
        fi
        ok "自检：投毒 [$c] ⇒ 核验转红"
        python3 "$ANALYZER" restore >/dev/null 2>&1 || true
        if ! python3 "$ANALYZER" check >/dev/null 2>&1; then
            fail "自检：还原后核验**仍红** ⇒ ⇒ 还原写坏了文件"
            exit 1
        fi
        ok "自检：还原后核验复绿（投毒 [$c]）"
    done

    local_declared="$(python3 "$ANALYZER" list-poisons | wc -l | tr -d ' ')"
    if [ "${#cases[@]}" -ne "$local_declared" ]; then
        fail "自检：自检清单 ${#cases[@]} 种 ≠ 分析器声明 $local_declared 种"
        exit 1
    fi
    ok "自检：${#cases[@]} 种投毒全部被抓住，且还原后复绿"
fi

python3 "$ANALYZER" restore >/dev/null 2>&1 || true

if [ $FAILS -ne 0 ]; then
    printf "\n${RED}❌ verify_header_self_contained: FAIL（%s 项）${RESET}\n" "$FAILS"
    exit 1
fi
printf "\n${GREEN}✅ verify_header_self_contained: PASS${RESET}\n"
