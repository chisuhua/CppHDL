#!/usr/bin/env bash
# verify_openspec_purpose.sh —— openspec 主 spec 的 **Purpose 占位符** + **规范债登记**护栏
#
# 判据来源 = `.rddf/checks/openspec-spec-debt.tsv`（登记表 · 唯一真相）
# 逻辑载体 = `scripts/lib/openspec_purpose_guard.py`（本脚本是薄壳 + --self-test 投毒）
#
# ⚠️ **为什么要有它**：`openspec validate --specs --strict` 虽有「Purpose 仍是占位符」这条规则，
#   但 **CI 不跑 `openspec validate`** ⇒ ⇒ 且 CLI 措辞随时会变 ⇒ ⇒ 不能只靠它。
#   ⇒ ⇒ 本护栏把「占位符集合」与「登记表」**双向**钉住 ⇒ 新建 capability 归档时若又写占位符
#   ⇒ ⇒ 护栏转红（**不等 strict 门**）⇒ ⇒ 同时守住 19+7 条已登记的规范债不被静默遗忘。
#
# 用法：
#   scripts/verify_openspec_purpose.sh              # R0–R3 检查
#   scripts/verify_openspec_purpose.sh --render      # 打印派生式登记表
#   scripts/verify_openspec_purpose.sh --self-test   # 4 类投毒各自必红 + 还原复绿
set -uo pipefail

# ⚠️ trap 里引用的变量必须在**全局**作用域定义（set -u 下未定义会崩）
probe="" ; bak="" ; regbak=""

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PY="$ROOT/scripts/lib/openspec_purpose_guard.py"
REG="$ROOT/.rddf/checks/openspec-spec-debt.tsv"
cd "$ROOT"

green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
red()   { printf '\033[0;31m%s\033[0m\n' "$*"; }

# ── fail-closed：缺 python3 / 缺逻辑载体 / 缺登记表 ⇒ exit 2
[ -f "$PY" ] || { red "❌ 缺逻辑载体：$PY"; exit 2; }
[ -f "$REG" ] || { red "❌ 缺登记表：$REG"; exit 2; }
command -v python3 >/dev/null 2>&1 || { red "❌ 缺 python3（fail-closed）"; exit 2; }

run_check() { python3 "$PY" --check "$ROOT" 2>&1; }

# ── --self-test：4 类投毒**各自**必红 + 还原复绿
self_test() {
    local fails=0
    probe="openspec/specs/verilator-reg-init/spec.md"
    bak="$(mktemp)"; regbak="$(mktemp)"
    cp "$probe" "$bak"; cp "$REG" "$regbak"
    trap 'cp "$bak" "$probe"; cp "$regbak" "$REG"; rm -f "$bak" "$regbak"' EXIT INT TERM

    restore() { cp "$bak" "$probe"; cp "$regbak" "$REG"; }

    expect_red() {   # $1 = 类别名；$2 = 期望规则号（R1/R2/R3）
        local name="$1" rule="$2" out rc
        out="$(run_check)"; rc=$?
        if [ "$rc" -eq 0 ]; then
            red "  ❌ 投毒未抓住：$name（--check 仍绿）"; fails=$((fails+1)); return
        fi
        if printf '%s' "$out" | grep -q "::error::$rule"; then
            green "  ✅ 投毒·$name ⇒ $rule 转红 ✓"
        else
            red "  ❌ 投毒·$name ⇒ 红了但不是 $rule（实际：$(printf '%s' "$out" | grep -oE '::error::R[0-9]' | sort -u | tr '\n' ' ')）"
            fails=$((fails+1))
        fi
    }

    # ① R1：把一个**实质** Purpose 换回占位符（且未登记）⇒ 必须转红
    python3 - "$probe" <<'PY'
import re, sys
p = sys.argv[1]
t = open(p, encoding="utf-8").read()
# ⚠️ MUST 带 re.M：`^` 否则只匹配**文件开头** ⇒ 投毒根本没生效（自检会误报「判据无判别力」）
t = re.sub(r"(^##\s+Purpose[ \t]*\n\n).*?(?=\n\n##\s)",
           r"\1TBD - created by archiving change selftest. Update Purpose after archive.",
           t, count=1, flags=re.S | re.M)
open(p, "w", encoding="utf-8").write(t)
PY
    expect_red "R1 实质 Purpose 被换回占位符" R1
    restore

    # ② R2：登记表登记一个**不存在**的 capability ⇒ 必须转红
    printf 'debt\tDXX\tno-such-capability\t-\t过期\t测试\tplanner\t测试\topen\t-\n' >> "$REG"
    expect_red "R2 登记表指向不存在的 capability" R2
    restore

    # ③ R2：把一行 open 债的 trigger 清空 ⇒ 必须转红
    python3 - "$REG" <<'PY'
import sys
p = sys.argv[1]
lines = open(p, encoding="utf-8").read().split("\n")
for i, ln in enumerate(lines):
    if ln.startswith("debt\tD01\t"):
        c = ln.split("\t"); c[7] = ""; lines[i] = "\t".join(c); break
open(p, "w", encoding="utf-8").write("\n".join(lines))
PY
    expect_red "R2 open 债缺 trigger" R2
    restore

    # ④ R3：登记一个**重复** id ⇒ 必须转红
    printf 'debt\tD01\tverilator-reg-init\t-\t过期\t测试\tplanner\t测试\topen\t-\n' >> "$REG"
    expect_red "R3 登记表 id 重复" R3
    restore

    # ⑤ 还原后必须复绿
    restore
    if run_check >/dev/null 2>&1; then
        green "  ✅ 还原后复绿 ✓"
    else
        red "  ❌ 还原后仍红"; fails=$((fails+1))
    fi

    echo
    if [ "$fails" -ne 0 ]; then red "❌ --self-test: $fails 项未通过"; return 1; fi
    green "✅ verify_openspec_purpose --self-test: PASS（4 类投毒各自转红 · 还原复绿）"
    return 0
}

case "${1:-}" in
    "")           exec python3 "$PY" --check "$ROOT" ;;
    --render)     exec python3 "$PY" --render "$ROOT" ;;
    --self-test)  self_test ;;
    *)            red "未知参数：$1（允许：--render | --self-test）"; exit 2 ;;
esac
