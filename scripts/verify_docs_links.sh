#!/usr/bin/env bash
# verify_docs_links.sh —— 文档链接与引用护栏（C1–C5）
#
# 判据来源 = `docs/DOCUMENT-ARCHITECTURE.md` §9（治理口径正本）
# 逻辑载体 = `scripts/lib/docs_link_scope.py`（本脚本是薄壳 + C4 自检投毒）
#
# 用法：
#   scripts/verify_docs_links.sh              # C1–C5 检查
#   scripts/verify_docs_links.sh --render      # 打印派生式分级登记表
#   scripts/verify_docs_links.sh --self-test   # C4：4 类投毒各自必红 + 还原复绿
set -uo pipefail

# ⚠️ trap 里引用的变量必须在**全局**作用域定义（set -u 下未定义会崩）
probe="" ; bak=""

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PY="$ROOT/scripts/lib/docs_link_scope.py"
cd "$ROOT"

green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
red()   { printf '\033[0;31m%s\033[0m\n' "$*"; }

# ── fail-closed：缺 python3 / 缺逻辑载体 ⇒ exit 2
[ -f "$PY" ] || { red "❌ 缺派生器：$PY"; exit 2; }
command -v python3 >/dev/null 2>&1 || { red "❌ 缺 python3（fail-closed）"; exit 2; }

# ── C4 --self-test：4 类投毒**各自**必红 + 还原复绿
self_test() {
    local fails=0
    probe="docs/usage_guide/README.md"            # L1 文件（投毒目标）
    bak="$(mktemp)"
    cp "$probe" "$bak"
    trap 'cp "$bak" "$probe"; rm -f "$bak"' EXIT INT TERM

    restore() { cp "$bak" "$probe"; }

    expect_red() {   # $1 = 类别名；$2 = 期望的规则号（C1/C2/C3/C5）
        local name="$1" rule="$2" out rc
        out="$(python3 "$PY" --check 2>&1)"; rc=$?
        if [ "$rc" -eq 0 ]; then
            red "  ❌ 投毒未抓住：$name（--check 仍绿）"; fails=$((fails+1)); return
        fi
        if printf '%s' "$out" | grep -q "::error::$rule"; then
            green "  ✅ 投毒·$name ⇒ $rule 转红 ✓"
        else
            red "  ❌ 投毒·$name ⇒ 红了但不是 $rule（实际：$(printf '%s' "$out" | grep -oE '::C[0-9]::' | sort -u | tr '\n' ' ')）"
            fails=$((fails+1))
        fi
    }

    # ① C1 死链
    printf '\n[x](NO_SUCH_FILE_FOR_SELFTEST.md)\n' >> "$probe"
    expect_red "C1 死链" C1
    restore

    # ② C2 引用行号越界（引一个存在文件的不可能行号）
    printf '\n`src/simulator.cpp:99999`\n' >> "$probe"
    expect_red "C2 行号越界" C2
    restore

    # ③ C3 本机绝对路径
    printf '\n本机路径 `/workspace/should-not-appear.txt`\n' >> "$probe"
    expect_red "C3 本机路径" C3
    restore

    # ④ C5 分级覆盖：新建一个无法归级的 .md（放在未被任何前缀覆盖的目录）
    local orphan="tmp_selftest_orphan/note.md"
    mkdir -p "$(dirname "$orphan")"
    printf '# orphan\n' > "$orphan"
    expect_red "C5 无法归级" C5
    rm -rf "$(dirname "$orphan")"

    # ⑤ 还原后必须复绿
    restore
    if python3 "$PY" --check >/dev/null 2>&1; then
        green "  ✅ 还原后复绿 ✓"
    else
        red "  ❌ 还原后仍红"; fails=$((fails+1))
    fi

    echo
    if [ "$fails" -ne 0 ]; then red "❌ --self-test: $fails 项未通过"; return 1; fi
    green "✅ verify_docs_links --self-test: PASS（4 类投毒各自转红 · 还原复绿）"
    return 0
}

case "${1:-}" in
    "")           exec python3 "$PY" --check ;;
    --render)     exec python3 "$PY" --render ;;
    --self-test)  self_test ;;
    --list-l1)    exec python3 "$PY" --list-l1 ;;
    *)            red "未知参数：$1（允许：--render | --self-test | --list-l1）"; exit 2 ;;
esac
