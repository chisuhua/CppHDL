#!/usr/bin/env bash
# verify_improvement_index.sh —— 改进提案索引的**状态列一致性**护栏
#
# 判据来源 = `.rddf/roadmap/phase-6-rtl-productization.md` §4 ·「索引表状态列的自动生成」
#   （登记原文：现状 = **人工维护** ⇒ ⇒ 必然漂 ⇒ ⇒ 根本修法 = **从 frontmatter
#   `状态:` 生成** + **一致性护栏**）
# 逻辑载体 = `scripts/lib/improvement_index.py`（本脚本是薄壳 + fixture 自检）
#
# ⚠️ **护栏判什么 / 不判什么**：
#   · **判**：索引表每一行的**状态词** == 对应 improvement 的 frontmatter `状态:` 的词
#     ⇒ ⇒ 「frontmatter 改了、索引表没跟上」与「索引表手改了、frontmatter 没跟上」
#     **两个方向**都转红；
#   · **不判**：每个 `.rddf/improvements/*.md` 都在索引表里 ⇒ ⇒ 索引表是**提案池**
#     （实施 / 归档后**从表移除**）⇒ ⇒ 那样判必假红（仓内 70+ 份 · 表里 16 份）。
#
# ⚠️ **可达性前提**（`scripts/verify_guard_reachability.sh` 的纪律 1）：
#   · 依赖 = **python3**（≥ 3.8）+ 仓内文件；**不**依赖网络 / git 历史 /
#     verilator / 编译器 / 任何仓外东西；
#   · 调用点 = CI `build-and-test` job（**无 gate** ⇒ ⇒ 总是执行 ⇒ 排除类型 `none`）；
#   · 自检用**临时 fixture 目录**（`tempfile.mkdtemp`）⇒ ⇒ **不**碰真实仓
#     ⇒ ⇒ 不依赖真实仓历史 ⇒ ⇒ 不会成为「第一个不可达护栏」。
#
# 用法：
#   scripts/verify_improvement_index.sh              # 状态列一致性检查
#   scripts/verify_improvement_index.sh --render      # 打印渲染后的索引表（刷新状态词）
#   scripts/verify_improvement_index.sh --self-test   # 8 条 fixture（含 5 类投毒）
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PY="$ROOT/scripts/lib/improvement_index.py"
cd "$ROOT"

green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
red()   { printf '\033[0;31m%s\033[0m\n' "$*"; }

# ── fail-closed：缺 python3 / 缺逻辑载体 ⇒ exit 2 ────────────────────────
[ -f "$PY" ] || { red "❌ 缺逻辑载体：$PY"; exit 2; }
command -v python3 >/dev/null 2>&1 || { red "❌ 缺 python3（fail-closed）"; exit 2; }

case "${1:-}" in
    "")           exec python3 "$PY" --check ;;
    --render)     exec python3 "$PY" --render ;;
    --self-test)  exec python3 "$PY" --self-test ;;
    *)            red "未知参数：$1（允许：--render | --self-test）"; exit 2 ;;
esac
