#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# verify_chlib_naming_honesty.sh —— 契约 `chlib-stream-arbitration` 的
# **命名诚实性登记表**护栏（B8）
#
# ⚠️ **为什么要有它**：本仓反复出现「**名字/注释承诺了不存在的行为**」这一族缺陷
#    （`fwft_fifo` 承诺 FWFT 实则不是 · `stream_arbiter_sequence` 注释承诺
#    round-robin 实则 `priority_encoder` · `stream_arbiter_round_robin` 承诺轮转
#    实则恒传 0）⇒ ⇒ ⇒ **每次都靠「临时普查」发现 ⇒ ⇒ 下次还会漏**。
#    ⇒ ⇒ 本护栏把「登记表」与「实现 / 测试」**机械对齐** ⇒ ⇒ ⇒ 退化可被检出。
#
# ⚠️ **覆盖面声明**：登记表只覆盖「**被改动过 / 名字说过谎**」的组件
#    ⇒ ⇒ **不是**全仓普查 ⇒ ⇒ 「新增组件忘了登记」由 capability 的
#    **per-change 义务** + R3（最近的 change MUST 声明它触及的组件）抓。
#
# ⚠️ **消息里 MUST NOT 含反引号** —— shell 双引号里的反引号会被当**命令替换**
#    ⇒ ⇒ 护栏消息被**静默吞掉**（本仓已踩过）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/chlib_naming_honesty.py"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0

ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS+1)); }
note() { printf "  ${YELLOW}::notice %s${RESET}\n" "$*"; }

# ── R0：fail-closed（缺文件 ⇒ MUST NOT 静默通过）──────────────────────────
echo "── R0：必需文件齐备（fail-closed）"
missing=""
[ -f "$ANALYZER" ] || missing="$missing 分析器"
[ -f "$REPO_ROOT/.rddf/checks/chlib-naming-honesty-registry.md" ] \
    || missing="$missing 登记表"
if [ -n "$missing" ]; then
    fail "R0 缺少必需文件:${missing}"
    exit 1
fi
ok "R0 必需文件齐备（fail-closed）"

# ── R1/R2/R3：判据（委派给 Python 分析器）────────────────────────────────
echo "── R1/R2/R3：登记表 ↔ 实现 / 测试 的机械对齐"
out="$(python3 "$ANALYZER" check 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
if [ $rc -ne 0 ]; then
    fail "命名诚实性登记表核验未通过（exit=$rc）"
    exit 1
fi
ok "R1/R2/R3 命名诚实性登记表核验通过"

# ─────────────────────────────────────────────────────────────────────────────
# 自检：**原地**投毒，每种 MUST 让核验转红；还原后 MUST 再确认一次绿
# ⚠️ 投毒数 MUST **动态**取（`list-poisons` 的行数）⇒ ⇒ 硬编码会漂移
# ⚠️ 登记表是**纯数据** ⇒ 「编译成功」这条判据**不适用** ⇒ ⇒ 改为「还原后复绿」
if [ "${1:-}" = "--self-test" ]; then
    echo "── 自检：原地投毒（纯数据 ⇒ 改为「还原后复绿」）"
    cases=()
    while IFS= read -r line; do
        [ -n "$line" ] && cases+=("$line")
    done < <(python3 "$ANALYZER" list-poisons)

    for c in "${cases[@]}"; do
        # ⚠️ 开头这次 restore **可能失败**（无投毒快照 ⇒ ⇒ 拒绝）⇒ ⇒ 忽略退出码
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
    printf '%s\n' "$FAILS" > /tmp/opencode/chlib_naming_honesty.fails
    printf "\n${RED}❌ verify_chlib_naming_honesty: FAIL（%s 项）${RESET}\n" "$FAILS"
    exit 1
fi
printf "\n${GREEN}✅ verify_chlib_naming_honesty: PASS${RESET}\n"
