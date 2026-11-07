#!/usr/bin/env bash
# scripts/verify_arch_ledger_residuals.sh
#
# 护栏：**架构台账 §7「已知残留」的状态 MUST 有唯一真相**（登记表 ⇒ 派生 ⇒ 双向核对）
#
# 判据来源：openspec/changes/arch-residual-registry-2026-11/specs/ledger-residual-registry/spec.md
#          + .rddf/improvements/arch-residual-registry-2026-11.md（12 行判据 · 11 投毒）
#
# ★ 退出码语义（★ **禁**继承探针的恒 exit 0）：
#   0 = 通过 · 1 = 不一致（漂移 / 缺登记 / 证据不存在）· 2 = fail-closed（缺工具 · 缺文件 ·
#   解析 0 行 · 登记表格式非法）⇒ ⇒ ⇒ CI 红 ⇒ ⇒ ⇒ 漂移可见
#
# ★ 本护栏**不接 CI**（⇒ S3 才接）：台账归一化（architect）落地前，判据 ③ 必然红
#   ⇒ 那是**预期状态**，不是缺陷 ⇒ ⇒ MUST NOT 因此回退归一化。
#
# 用法：
#   scripts/verify_arch_ledger_residuals.sh                  # 判定
#   scripts/verify_arch_ledger_residuals.sh --self-test       # 合成 fixture + 11 投毒自检
#   scripts/verify_arch_ledger_residuals.sh --render          # 打印期望规范句（供 copy-paste）
#   scripts/verify_arch_ledger_residuals.sh --check-normalization [--review <file>]
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/ledger_residual.py"
ROOT="$REPO_ROOT"
MODE="check"
EXTRA=()

while [ $# -gt 0 ]; do
    case "$1" in
        --self-test) MODE="selftest"; shift ;;
        --render) MODE="render"; shift ;;
        --check-normalization) MODE="check-normalization"; shift ;;
        --root) ROOT="$2"; shift 2 ;;
        --review) EXTRA+=("--review" "$2"); shift 2 ;;
        --base) EXTRA+=("--base" "$2"); shift 2 ;;
        -h|--help) echo "usage: $0 [--self-test|--render|--check-normalization] [--root DIR] [--review FILE] [--base REF]"; exit 0 ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

if ! command -v python3 >/dev/null 2>&1; then
    echo "FAIL-CLOSED: 缺 python3 ⇒ 无法判定（★ fail-closed）" >&2
    exit 2
fi
if [ ! -f "$ANALYZER" ]; then
    echo "FAIL-CLOSED: 缺判据分析器 $ANALYZER" >&2
    exit 2
fi

python3 "$ANALYZER" "$MODE" --root "$ROOT" ${EXTRA[@]+"${EXTRA[@]}"}
exit $?
