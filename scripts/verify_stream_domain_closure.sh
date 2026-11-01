#!/usr/bin/env bash
# ⚠️ **护栏：stream 域收口清单**（`.rddf/checks/stream-domain-closure.md`）
#
# ⚠️ **为什么需要它**（本仓登记的病症之一）：
#   「未收口项散落多处 ⇒ ⇒ 无法宣告收官」—— 实测：stream 域的未兑现项分散在
#   **4 处**（capability Purpose 的「范围限定」段 + 多个 improvement 台账 +
#   ADR 的「不在本 ADR 范围」段）⇒ ⇒ ⇒ **没人能一眼看出「还欠什么」**。
#
# ⚠️⚠️ **本护栏的核心设计**：清单的「状态」列**不是手写的事实声明**
#   ⇒ ⇒ **由「契约 / 判据 / 护栏」三个指针的集合推导**：
#   - ✅ ⇒ MUST 同时有 spec + test + guard（**三者对齐**）
#   - ❌ ⇒ MUST 有 debt（欠账落到真实文件）且 MUST NOT 同时有 test + guard
#   ⇒ ⇒ ⇒ **清单不可能自相矛盾**（「说谎」在结构上不可达）
#
# ⚠️ **消息里 MUST NOT 含反引号** —— shell 双引号里的反引号会被当**命令替换**
#    ⇒ ⇒ 护栏消息被**静默吞掉**（本仓已踩过）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/stream_domain_closure.py"
CHECKLIST="$REPO_ROOT/.rddf/checks/stream-domain-closure.md"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0

ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS+1)); }
note() { printf "  ${YELLOW}::notice %s${RESET}\n" "$*"; }

# ── R0：fail-closed（清单 / 分析器缺失 ⇒ ⇒ MUST NOT 静默通过）────────────────
echo "── R0：必需文件齐备（fail-closed）"
missing=""
[ -f "$CHECKLIST" ] || missing="$missing 收口清单"
[ -f "$ANALYZER" ]  || missing="$missing 分析器"
if [ -n "$missing" ]; then
    fail "R0 缺少必需文件:${missing}"
    printf '%s\n' "$FAILS" > /tmp/opencode/stream_domain_closure.fails
    exit 1
fi
ok "R0 必需文件齐备（fail-closed）"

# ── R1 / R2：清单核验（委派给 Python 分析器）──────────────────────────────
echo "── R1/R2：清单指针可解析 + 状态与指针集合一致"
out="$(python3 "$ANALYZER" check 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
if [ $rc -ne 0 ]; then
    fail "R1/R2 收口清单核验未通过（exit=$rc）"
    printf '%s\n' "$FAILS" > /tmp/opencode/stream_domain_closure.fails
    exit 1
fi
ok "R1/R2 收口清单核验通过"

# ─────────────────────────────────────────────────────────────────────────────
# 自检：4 种**原地**投毒，每种 MUST 让核验转红；还原后 MUST 再确认一次绿
# ─────────────────────────────────────────────────────────────────────────────
if [ "${1:-}" = "--self-test" ]; then
    echo "── 自检：4 种原地投毒（清单是纯数据 ⇒ 「编译成功」不适用 ⇒ 改为「还原后复绿」）"

    # ⚠️⚠️ **还原 MUST 刷新 mtime** —— 否则「还原」后核验读的是陈旧内容
    #    ⇒ ⇒ 复绿判定失真。分析器的 restore() 用 shutil.copyfile（刷新 mtime）。
    trap 'python3 "$ANALYZER" restore >/dev/null 2>&1 || true' EXIT

    poisons="$(python3 "$ANALYZER" list-poisons)"
    n=0
    caught=0
    for p in $poisons; do
        n=$((n+1))
        if ! python3 "$ANALYZER" poison "$p" >/dev/null 2>&1; then
            fail "自检：投毒 [$p] 注入失败（⇒ ⇒ fail-closed ⇒ ⇒ 绝不静默跳过）"
            continue
        fi
        if python3 "$ANALYZER" check >/dev/null 2>&1; then
            fail "自检：投毒 [$p] 后核验**仍绿**（⇒ ⇒ 判据无判别力）"
        else
            ok "自检：投毒 [$p] ⇒ 核验转红"
            caught=$((caught+1))
        fi
        if ! python3 "$ANALYZER" restore >/dev/null 2>&1; then
            fail "自检：投毒 [$p] 还原失败"
            continue
        fi
        if python3 "$ANALYZER" check >/dev/null 2>&1; then
            ok "自检：还原后核验复绿（投毒 [$p]）"
        else
            fail "自检：还原后核验**仍红**（⇒ ⇒ 还原写坏了清单 ⇒ ⇒ 假失败）"
        fi
    done
    trap - EXIT
    # ⚠️⚠️⚠️ **汇总 MUST 以「**全部**投毒都被抓住」为条件**
    #    ⇒ ⇒ **只数数量** ⇒ ⇒ 会出现「3 种注入失败 + 1 种未转红 ⇒ 仍报
    #    『全部被抓住』」的**假绿** ⇒ ⇒ **本护栏自己就假绿过一次**
    #    ⇒ ⇒ ⇒ ⇒ **⇒ 计数 MUST 与「被抓住」数同时校验**
    if [ "$n" -ne 4 ]; then
        fail "自检：投毒数为 $n ≠ 4（⇒ ⇒ 投毒清单与实现漂移）"
    fi
    if [ "$caught" -ne 4 ]; then
        fail "自检：仅 $caught/4 种投毒被抓住（⇒ ⇒ **判据无判别力**）"
    else
        ok "自检：4 种投毒全部被抓住，且还原后复绿"
    fi
fi

printf '%s\n' "$FAILS" > /tmp/opencode/stream_domain_closure.fails
if [ "$FAILS" -eq 0 ]; then
    printf "${GREEN}✅ verify_stream_domain_closure: PASS${RESET}\n"
    exit 0
fi
printf "${RED}❌ verify_stream_domain_closure: FAIL（%d 项）${RESET}\n" "$FAILS"
exit 1
