#!/usr/bin/env bash
# scripts/probe_arch_ledger_residuals.sh
#
# ★ **本路径 MUST 保留**：它是 `.rddf/checks/decision-request-hierarchical-ledger-residuals-2026-11.md`
#   的 `evidence:` 指针 ⇒ ⇒ ⇒ 改名 ⇒ 证据链断 ⇒ ⇒ ⇒（`verify_ledger_evidence.sh` 会红）
#
# 现在：本脚本 = 护栏 `scripts/verify_arch_ledger_residuals.sh` 的**只读 wrapper**
# ⇒ ⇒ 「状态从哪来」的唯一真相 = 登记表 `.rddf/checks/arch-residual-registry.md`
# ⇒ ⇒ ⇒ 不再有「手工 RESIDUAL_EVIDENCE 清单」⇒ ⇒ ⇒ 那份清单曾与台账**互不校验** ⇒ ⇒ 漂移温床
#
# ⚠️ 退出码与旧版**不同**（★ 有意）：旧版恒 `exit 0`（⇒ 即使检出陈旧也不阻断 CI）
# ⇒ ⇒ 现在统一为护栏语义：`0` 通过 · `1` 不一致 · `2` fail-closed。
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GUARD="$REPO_ROOT/scripts/verify_arch_ledger_residuals.sh"

if [ ! -f "$GUARD" ]; then
    echo "FAIL-CLOSED: 缺护栏 $GUARD" >&2
    exit 2
fi

exec bash "$GUARD" "$@"
