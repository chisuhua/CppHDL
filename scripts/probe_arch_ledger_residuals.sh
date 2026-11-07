#!/usr/bin/env bash
# scripts/probe_arch_ledger_residuals.sh
#
# **探针**：层次化 RTL 输出的**架构台账**（`docs/architecture/2026-11-02-hierarchical-rtl-output.md`
# §7「已知残留」）的**陈旧行**探测。
#
# ⚠️ **为什么需要它**（★ 2026-11-07 实测）：
#   台账 §7 用**散文**记录残留的状态 ⇒ ⇒ ⇒ ⇒ 闭口了**不会**自动更新 ⇒ ⇒ ⇒ ⇒
#   实测在 3 个残留项闭口后（2026-11-06 · 残留 5 ⇒ 2026-11-07 · P8 + 残留 1b）
#   ⇒ ⇒ ⇒ 台账里**仍**写着「⚠️ 未决」⇒ ⇒ ⇒ ⇒ ⇒ **陈旧** ⇒ ⇒ ⇒ ⇒ ⇒ 而**没有任何
#   ⇒ ⇒ ⇒ ⇒ 护栏**会发现它（★ scripts/verify_ledger_evidence.sh 只核验 front-matter 的
#   ⇒ ⇒ ⇒ ⇒ evidence 字段 ⇒ ⇒ ⇒ ⇒ **不**核验「状态格 vs 实际闭口」）。
#   ⇒ ⇒ ⇒ ⇒ **这类漂移是静默的** ⇒ ⇒ ⇒ ⇒ 本探针把它**变成可见的**。
#
# ⚠️ **本探针只列事实，不做决定**（★ 与本仓 .rddf/checks/decision-request-*.md
#   的体例一致 ⇒ ⇒ ⇒ ⇒ 裁决权在 architect）。
#
# 用法：bash scripts/probe_arch_ledger_residuals.sh
# 退出码：0 = 探针正常执行（★ **不**代表台账无陈旧行 ⇒ ⇒ ⇒ 陈旧行数在输出里）
set -uo pipefail

cd "$(dirname "$0")/.." || exit 2

# ⚠️ **可选参数** = 台账路径（★ 默认 = 仓内的权威台账 ⇒ ⇒ ⇒ ★ 传参只为**自测**
#    ⇒ ⇒ ⇒ ⇒ 对「已闭口的行」判 OK ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 不传就永远跑真实台账）
LEDGER="${1:-docs/architecture/2026-11-02-hierarchical-rtl-output.md}"
SECTION="7"

if [ ! -f "$LEDGER" ]; then
    echo "FAIL: 台账不存在：$LEDGER"
    exit 2
fi

# ── 残留 id → 闭口证据（★ 显式登记 ⇒ ⇒ ⇒ auditable ⇒ ⇒ ⇒ ⇒ **不是**自动发现）────
# 格式：<台账 §7 的残留 id>|<闭口证据路径（仓内相对路径）>
RESIDUAL_EVIDENCE=(
  "1|openspec/changes/archive/2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11"
  "1b|openspec/changes/archive/2026-11-07-hierarchical-corpus-regression-2026-11"
  "5|scripts/verify_hierarchical_port_naming.sh"
)

# ── 台账 §<SECTION> 的行 → 状态格 ─────────────────────────────────────────
# ⚠️ **MUST 限定在该 section 内** ⇒ ⇒ ⇒ 否则同一个 id 可能在**别的**表里先命中
#    ⇒ ⇒ ⇒ ⇒ （★ 实测踩过：id「1」曾命中另一张表的行 ⇒ ⇒ ⇒ ⇒ 状态格显示成
#    ⇒ ⇒ ⇒ ⇒ 「✅ 编译期 key 类型检查」⇒ ⇒ ⇒ ⇒ ⇒ **完全无关**的内容 ⇒ ⇒ ⇒ ⇒ ⇒
#    ⇒ ⇒ ⇒ ⇒ 结论是**假的** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 必须按 section 过滤）
ledger_status() {  # $1 = 残留 id
    awk -F'|' -v id="$1" -v sec="$SECTION" '
        /^##[ \t]+/ {
            in_sec = ($0 ~ ("^##[ \t]+" sec "[. \t]"))
            next
        }
        in_sec && /^[[:space:]]*\|/ {
            n = split($0, c, "|")
            if (n < 4) next
            id_cell = c[2]
            gsub(/^[ \t`]+|[ \t`]+$/, "", id_cell)
            if (id_cell != id) next
            status = c[4]
            gsub(/^[ \t]+|[ \t]+$/, "", status)
            print status
            exit
        }
    ' "$LEDGER"
}

echo "=== 探针：台账 §${SECTION} 残留行的陈旧性 ==="
echo "台账：$LEDGER"
echo

stale=0
total=0
for entry in "${RESIDUAL_EVIDENCE[@]}"; do
    id="${entry%%|*}"
    evidence="${entry#*|}"
    total=$((total + 1))
    if [ ! -e "$evidence" ]; then
        printf '?? %-4s 证据缺失（%s 不存在）=> 需人工核对\n' "$id" "$evidence"
        continue
    fi
    status="$(ledger_status "$id")"
    if [ -z "$status" ]; then
        printf '?? %-4s 台账里**找不到**该行（表格格式可能变了）=> 需人工核对\n' "$id"
        continue
    fi
    case "$status" in
        *已收口*|*已解决*|*已关闭*|*已落地*|*已完成*)
            printf 'OK %-4s 台账状态格已闭口\n' "$id"
            ;;
        *)
            printf 'STALE %-4s 状态格仍是「%s」=> 但闭口证据已存在 => %s\n' \
                   "$id" "${status:0:50}" "$evidence"
            stale=$((stale + 1))
            ;;
    esac
done

echo
echo "--- 汇总：登记的残留 id $total 个 · **陈旧** $stale 个"
if [ "$stale" -gt 0 ]; then
    echo '=> 修台账 = architect 域（builder MUST NOT 单方面改）'
    echo '=> 且**没有任何护栏**会发现这件事（verify_ledger_evidence.sh 只查 front-matter）'
fi
exit 0
