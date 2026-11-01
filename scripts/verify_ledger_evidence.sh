#!/usr/bin/env bash
# ⚠️ **护栏：台账证据核验**（纪律机制 M · 2026-11-01）
#
# ⚠️ **为什么需要它**（oracle 评审 · 2026-11-01）：
# 本轮出现**三次「自己的结论被自己的探针推翻」** —— builder 的「静默丢字」被推翻、
# XML 判据被负向验证推翻、成本单位被实测推翻 ⇒ ⇒ ⇒ 三次的**共同形状**是
# **「结论先被写进标题 / 根因 / 口径，测量随后才发生」** ⇒ ⇒ ⇒ ⇒ ⇒
# ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
# **⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒**
#
# ⇒ ⇒ **本护栏把「结论复核」从【个人品质】变成【CI 可检的流程】**：
# 台账 front-matter 必须带 `evidence:`（`test:` / `probe:` / `unmeasured`）⇒ ⇒ ⇒
# ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
#
# ⚠️ **fail-closed 的范围**：
#   ① `evidence:` 指向**不存在 / 未在 CMake 注册**的 `test:` ⇒ ⇒ **FAIL**
#   ② `evidence:` 指向**临时目录**（`/tmp/...`）的 `probe:` ⇒ ⇒ **FAIL**
#      （⇒ ⇒ session 结束即消失 ⇒ ⇒ 不可复算）
#   ③ `evidence:` 取值非法 ⇒ ⇒ **FAIL**
#   ④ 标题含「实测 / 已确认」类断言词 **但** `evidence: unmeasured` ⇒ ⇒ **FAIL**
#      （⇒ ⇒ 「未实测却写成实测」= 最危险的一种）
#   ⑤ **缺 `evidence:`** ⇒ ⇒ ⚠️ **WARN + 点名**（⇒ ⇒ 历史台账**不阻塞**
#      ⇒ ⇒ ⇒ ⇒ **⇒ 但缺失是【可见】的** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ M-b 补齐后归零**）
#
# ⚠️ **消息里 MUST NOT 含反引号**（shell 双引号里会被当命令替换 ⇒ ⇒ 消息被吞）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/ledger_evidence.py"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0

ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS+1)); }

# ── R0：fail-closed（分析器缺失 ⇒ MUST NOT 静默通过）──────────────────────
echo "── R0：必需文件齐备（fail-closed）"
if [ ! -f "$ANALYZER" ]; then
    fail "R0 缺少分析器 scripts/lib/ledger_evidence.py"
    exit 1
fi
ok "R0 分析器就位"

# ── R1：证据核验 ────────────────────────────────────────────────────────
echo "── R1：台账 evidence 核验"
out="$(python3 "$ANALYZER" 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
if [ $rc -ne 0 ]; then
    fail "R1 台账证据核验未通过（exit=$rc）"
else
    ok "R1 台账证据核验通过"
fi

# ── 自检：原地投毒（把某个台账的 evidence 指向**不存在**的 test）⇒ MUST 转红 ──
if [ "${1:-}" = "--self-test" ]; then
    echo "── 自检：原地投毒（evidence 指向不存在的源文件 ⇒ MUST 转红）"
    LEDGER="$REPO_ROOT/.rddf/improvements/stream-arbiter-rr-rotation-2026-10.md"
    BACKUP="/tmp/opencode/ledger_evidence.bak"
    if [ ! -f "$LEDGER" ]; then
        fail "自检：找不到用于投毒的台账"
    else
        cp "$LEDGER" "$BACKUP"
        # ⚠️ 还原 MUST 用 cp（刷新 mtime）
        python3 - "$LEDGER" << 'PYEOF'
import io, sys
p = sys.argv[1]
s = io.open(p, encoding="utf-8").read()
old = "evidence: test:tests/chlib/test_stream.cpp#[rotation]"
new = "evidence: test:tests/NO_SUCH_FILE.cpp#nope"
assert old in s, "投毒锚点未命中"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new, 1))
print("  已投毒")
PYEOF
        if python3 "$ANALYZER" >/dev/null 2>&1; then
            fail "自检：投毒后核验**仍绿**（⇒ ⇒ 判据无判别力）"
        else
            ok "自检：投毒 ⇒ 核验转红"
        fi
        cp "$BACKUP" "$LEDGER"
        if python3 "$ANALYZER" >/dev/null 2>&1; then
            ok "自检：还原后核验复绿"
        else
            fail "自检：还原后核验**仍红**（⇒ ⇒ 还原写坏了台账）"
        fi
    fi
fi

if [ "$FAILS" -eq 0 ]; then
    printf "${GREEN}✅ verify_ledger_evidence: PASS${RESET}\n"
    exit 0
fi
printf "${RED}❌ verify_ledger_evidence: FAIL（%d 项）${RESET}\n" "$FAILS"
exit 1
