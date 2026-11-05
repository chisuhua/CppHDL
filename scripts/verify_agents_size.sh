#!/usr/bin/env bash
# scripts/verify_agents_size.sh
#
# 根 `AGENTS.md` 的**体积上限**护栏 —— 防「变更流水回流」。
#
# 背景（2026-11-05 实测）：
#   根 `AGENTS.md` 曾膨胀到 **393,052 B / 1753 行 / 150,663 token**，
#   其中 `## NOTES`（88 条历史变更流水）+ `## 已消除的过时信息`（墓碑）
#   合计占 **88.9%** —— 而根 AGENTS.md 是**每次 agent 会话都注入上下文**的文件。
#   ⇒ 每条历史条目 3–30 KB ⇒ **追加一条流水**就是 3–30 KB 的常驻上下文成本。
#   ⇒ 处置 = 流水归档到 `docs/archive/agents-notes/`，本护栏守住「不许回流」。
#
# 阈值推导（**不要随手改**）：
#   · 2026-11-05 精简后的基线 = **46,193 B**（45.1 KiB）—— 这是「当前态指引」的
#     不可压缩量（CONVENTIONS / ANTI-PATTERNS / KNOWN LIMITATIONS / VERILATOR
#     BACKEND RULES / 5 个 CI 锚点 + 口径注释 + 当前计数表）。
#   · `WARN` = 56 KiB（基线 +24%）⇒ 提前给「在长了」的信号，**不失败**。
#   · `FAIL` = 72 KiB（基线 +60%）⇒ 再长就说明有人在往里塞流水。
#   ⚠️ 曾有人提议把 FAIL 卡在 30 KB —— **低于基线** ⇒ 落地即红 ⇒ 是坏护栏。
#      基线 45.1 KiB 里每一段都是**当前约束**；再压只能删真约束 ⇒ 不可接受。
#
# 用法：
#   scripts/verify_agents_size.sh              # 主路径（CI 挂载）
#   scripts/verify_agents_size.sh --self-test  # 判别力自检（投毒 → 必须红）

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

AGENTS="$REPO_ROOT/AGENTS.md"
WARN_BYTES=$((56 * 1024))    # 57344
FAIL_BYTES=$((72 * 1024))    # 73728

# ── 判定逻辑（主路径与自检**共用同一份** ⇒ 自检不会与实现漂移）────────────
# 输出：0 = 通过 · 1 = 超阈值（红） · 2 = 文件缺失（fail-closed，红）
check_size() {
    local file="$1" sz
    if [ ! -f "$file" ]; then
        echo 2
        return 0
    fi
    sz=$(wc -c < "$file")
    if [ "$sz" -gt "$FAIL_BYTES" ]; then
        echo 1
    else
        echo 0
    fi
}

# ── 自检模式 ────────────────────────────────────────────────────────────────
if [ "${1:-}" = "--self-test" ]; then
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT
    fails=0
    checks=0

    st_check() {  # st_check <label> <expected_code> <actual_code>
        checks=$((checks + 1))
        if [ "$2" != "$3" ]; then
            echo "  FAIL: $1 — expected $2, got $3"
            fails=$((fails + 1))
        else
            echo "  ok: $1 (→ $3)"
        fi
    }

    if [ ! -f "$AGENTS" ]; then
        echo "  FAIL: 根 AGENTS.md 不存在 ⇒ 无法判定（fail-closed）"
        exit 1
    fi
    base_sz=$(wc -c < "$AGENTS")

    # 1. 当前文件 MUST 通过
    st_check "当前 AGENTS.md 在 FAIL 阈值内" 0 "$(check_size "$AGENTS")"

    # 2. 投毒：超过 FAIL 阈值 ⇒ MUST 红
    head -c $((FAIL_BYTES + 1)) /dev/zero | tr '\0' 'x' > "$tmp/big.md"
    st_check "超过 FAIL 阈值 ⇒ 红" 1 "$(check_size "$tmp/big.md")"

    # 3. 投毒：超过 WARN 但未超 FAIL ⇒ MUST **不红**（只是告警）
    head -c $((WARN_BYTES + 1)) /dev/zero | tr '\0' 'x' > "$tmp/warn.md"
    st_check "超过 WARN 阈值 ⇒ 不红（仅告警）" 0 "$(check_size "$tmp/warn.md")"

    # 4. 投毒：文件缺失 ⇒ fail-closed（红）
    st_check "文件缺失 ⇒ 红（fail-closed）" 2 "$(check_size "$tmp/nonexistent.md")"

    # 5. 回归护栏：阈值必须真的卡住「追加一条流水」的量级
    #    （一条历史流水条目实测 3–30 KB ⇒ 基线 +30 KB 必须转红）
    st_check "基线 +30 KB（≈ 一条历史流水）⇒ 红" 1 \
        "$(head -c $((base_sz + 30 * 1024)) /dev/zero | tr '\0' 'x' > "$tmp/one_entry.md"; check_size "$tmp/one_entry.md")"

    echo ""
    if [ "$fails" -ne 0 ]; then
        echo "❌ verify_agents_size --self-test: $fails/$checks 项失败"
        exit 1
    fi
    echo "✅ verify_agents_size --self-test: $checks/$checks 全绿（基线 ${base_sz} B）"
    exit 0
fi

# ── 主路径 ──────────────────────────────────────────────────────────────────
if [ ! -f "$AGENTS" ]; then
    echo "::error::根 AGENTS.md 不存在 ⇒ 体积无法判定（fail-closed）"
    exit 1
fi

size=$(wc -c < "$AGENTS")
kib=$((size / 1024))
rc=$(check_size "$AGENTS")

if [ "$rc" = "1" ]; then
    echo "::error::根 AGENTS.md = ${size} B (${kib} KiB) > FAIL 阈值 ${FAIL_BYTES} B (72 KiB)"
    echo "::error::AGENTS.md 是**每次 agent 会话都注入上下文**的文件 ⇒ 它的体积直接等于常驻 token 成本。"
    echo "::error::正确处置：把内容放进对应段落（CONVENTIONS / ANTI-PATTERNS / KNOWN LIMITATIONS）"
    echo "::error::或归档到 docs/archive/agents-notes/ ⇒ **不要**往本文件追加变更流水。"
    echo "::error::历史流水归档位置：docs/archive/agents-notes/AGENTS-NOTES-2026-11.md"
    exit 1
fi

if [ "$size" -gt "$WARN_BYTES" ]; then
    echo "::warning::根 AGENTS.md = ${size} B (${kib} KiB) 已超 WARN 阈值 ${WARN_BYTES} B (56 KiB)"
    echo "::warning::尚未超 FAIL 阈值（${FAIL_BYTES} B）⇒ 本次不失败，但请确认新增内容是**当前态约束**而非流水。"
fi

echo "✅ verify_agents_size: 根 AGENTS.md = ${size} B (${kib} KiB) ≤ FAIL 阈值 72 KiB"
exit 0
