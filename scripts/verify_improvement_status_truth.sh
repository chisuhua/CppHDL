#!/usr/bin/env bash
# verify_improvement_status_truth.sh —— improvement 提案的**状态真实性**护栏
#
# 判据来源 = 2026-11-27 的治理审查：`rddf status` 的 30 条待办里 **21 条是
# 「已交付但 front matter 未回填」的僵尸条目** ⇒ ⇒ 待办池被噪声淹没 ⇒ ⇒
# 把那条不变式**机械化**（判据全部在 `scripts/lib/improvement_status_truth.py`）。
#
# ⚠️ **护栏判什么 / 不判什么**：
#   · **判**：① 每份 improvement 的 front matter MUST 是合法 YAML 且 `---` 闭合
#            ② 「已交付」（同名归档 change **或** 正文 Status 首词命中已完标记）
#              ⇒ ⇒ `状态:` MUST 是终态词 **或** `partial`
#            ③ 索引表每行的链接目标 MUST 存在
#   · **不判**：「标了终态但其实没做」⇒ 那要逐条核对 body 语义，本仓已有 2 份
#     **已登记**的同型漂移（归 planner 的 `rtl-track-followup-list-2026-11.md`）
#     ⇒ ⇒ 强判会把本护栏变成恒红 ⇒ ⇒ 只判**单向**（已交付 ⇒ 必须标终态）。
#
# ⚠️ **可达性前提**（`scripts/verify_guard_reachability.sh` 的纪律 1）：
#   · 依赖 = **python3**（≥ 3.8）+ 仓内文件；**不**依赖网络 / git 历史 /
#     verilator / 编译器 / **仓外产品包**（不 import `skills._lib.*`）；
#   · 调用点 = CI `build-and-test` job（**无 gate** ⇒ 总是执行 ⇒ 排除类型 `none`）；
#   · 自检用**临时 fixture 目录**（`tempfile.mkdtemp`）⇒ ⇒ **不**碰真实仓
#     ⇒ ⇒ 不依赖真实仓历史 ⇒ 不会成为「第一个不可达护栏」。
#
# 用法：
#   scripts/verify_improvement_status_truth.sh              # C1–C3 检查
#   scripts/verify_improvement_status_truth.sh --self-test   # 6 条 fixture（5 类投毒 + 还原）
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PY="$ROOT/scripts/lib/improvement_status_truth.py"
cd "$ROOT"

green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
red()   { printf '\033[0;31m%s\033[0m\n' "$*"; }

# ── fail-closed：缺 python3 / 缺逻辑载体 ⇒ exit 2 ────────────────────────
[ -f "$PY" ] || { red "❌ 缺逻辑载体：$PY"; exit 2; }
command -v python3 >/dev/null 2>&1 || { red "❌ 缺 python3（fail-closed）"; exit 2; }

case "${1:-}" in
    "")           exec python3 "$PY" --check ;;
    --self-test)  exec python3 "$PY" --self-test ;;
    *)            red "未知参数：$1（允许：--self-test）"; exit 2 ;;
esac
