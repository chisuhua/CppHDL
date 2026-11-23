#!/usr/bin/env bash
# scripts/verify_verif_migration_state.sh —— `.verif` 迁移态自洽性护栏（R13 闭口探针）
#
# 判据来源 = ADR-074 §7.6 的 **AC-R13′-1** + 蓝图 §7 **R13**
# （2026-11-20 用户追认：闭口探针 MUST 做并挂 CI）
# 逻辑载体 = `scripts/lib/verif_migration_state.py`（本脚本是薄壳 + --self-test 投毒）
#
# ⚠️ **为什么不是「必须已迁」**：R13 的闭合判据要求 SoC 行 `generated=yes`，而 SoC **尚未**
#   迁移 `.verif` ⇒ ⇒ 直接判 ⇒ CI 恒红 ⇒ ⇒ 本护栏判**两态自洽**：
#   · 态 A（未迁）= `generated=no` ⇒ `.verif` MUST NOT 存在
#   · 态 B（已迁）= `generated=yes` ⇒ `.verif` MUST 存在 + `interp_checks == min_checks`
#   ⇒ ⇒ 红只发生在**不自洽**时（半迁态 / 回潮 / 迁移不完整）。
#   ⚠️ 「宣称已闭却未迁」这一格**不可机械判**（「已闭」无可判载体）⇒ ⇒ 提供
#   **`--require-closed`** 作**归档门**（归档 SoC 迁移 change 时跑一次 ⇒ 那时必须已迁）。
#
# ⚠️ **逐行判**（2026-11-23 · 用户裁决）：本护栏判**每个** `kind=soc` 单元 ⇒ ⇒ **逐行**判。
#   ⚠️ **为什么改**：原实现写死「`kind=soc` 的行**恰好 1 条**」⇒ ⇒ 那是「仓里只有 1 个
#   SoC 单元」的**过时实现假设**⇒ ⇒ S1 登记 `tb_riscv_soc`（第 2 条）后变成 fail-closed
#   ⇒ ⇒ **覆盖面归零**（2 个单元一个都没判）⇒ ⇒ `--self-test` 同样崩溃。
#   ⇒ ⇒ **防「删行缩面」不在本护栏**：行数守恒由 `scripts/verify_examples_rtl_sim.sh` 的
#   `REGISTERED_SOC` 锚点负责 ⇒ ⇒ 本护栏**不另造**行数锚点（避免两处清单漂移）。
#   ⚠️ **归档门在多单元下的语义**：只判 `generated=yes`（已宣称闭口）的单元；`generated=no`
#   的单元**跳过**（并列出）⇒ ⇒ **已知边界**：单行时代归档门会拦住「停在未迁态就归档」，
#   逐行后该格退化为**文档纪律**；而「宣称已迁却没迁」仍由**主路径**抓住（回潮 ⇒ 红）。
#
# ⚠️ ⑥' 新鲜度**不在本护栏重复判**（双权威禁令）⇒ ⇒ 由
#   `scripts/verify_examples_rtl_sim.sh` 的 ⑥' 负责。
#
# 用法：
#   scripts/verify_verif_migration_state.sh                    # 逐行两态自洽（CI 用）
#   scripts/verify_verif_migration_state.sh --self-test        # 5 类投毒 + 2 条必绿
#   scripts/verify_verif_migration_state.sh --require-closed   # 归档门（已宣称闭口的单元必须已迁）

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PY="$ROOT/scripts/lib/verif_migration_state.py"
cd "$ROOT"

GREEN='\033[0;32m'; RED='\033[0;31m'; RESET='\033[0m'
FAILS=0

# ── fail-closed：缺 python3 / 缺逻辑载体 ⇒ exit 2 ──────────────────────
command -v python3 >/dev/null 2>&1 || { echo "${RED}❌ 缺 python3（fail-closed）${RESET}"; exit 2; }
[ -f "$PY" ] || { echo "${RED}❌ 缺逻辑载体：$PY${RESET}"; exit 2; }

case "${1:-}" in
    --self-test)
        python3 "$PY" --self-test
        exit $?
        ;;
    --require-closed)
        python3 "$PY" --check --require-closed
        exit $?
        ;;
    "" | --check)
        python3 "$PY" --check
        exit $?
        ;;
    -h | --help)
        sed -n '3,24p' "$0"
        exit 0
        ;;
    *)
        echo "用法：$0 [--check|--self-test|--require-closed]" >&2
        exit 2
        ;;
esac
