#!/usr/bin/env bash
# scripts/verify_hierarchical_p8.sh
#
# **P8 守卫**（roadmap「P8」条 · A4 裁决）：T3-005 的等价性 harness MUST 写成
# 「**可公共化**」形态（内部实现与公共包装**分离**），但本 change **MUST NOT 暴露
# 公共 API** ⇒ ⇒ harness MUST NOT 进 `include/`。
#
# 四条判据：
#   R1 `include/` 下**不得**出现 harness 头（文件名含 `equivalence`）⇒ ⇒ 公共 API 面无 harness
#   R2 `include/` 下**不得**出现 harness 的**符号**（实现名 / 内部命名空间）
#      ⇒ ⇒ 「不暴露」不是靠文件名躲，而是真的没写进去
#   R3 harness MUST 存在于 `tests/`（`tests/hier_equivalence.h`）
#      ⇒ ⇒ R1/R2 **不是**「文件还没建」的假绿
#   R4 本 change 的**公开 API 面** MUST ⊆ 已裁决的 4 个公共头
#      （`codegen_verilog.h` · `hierarchy_verilog.h` · `core/verilator_backend.h` ·
#        `core/verilator_port_set.h`）⇒ ⇒ 公开面**不得**继续扩张
#      ⇒ ⚠️ 需要 git 历史（`git diff <base>..HEAD -- include/`）
#      ⇒ ⇒ **base 不可解析时明确 NOTE 跳过**（CI 的浅克隆跑不了这一条 ⇒ 不静默假装绿）
#
# 用法：
#   bash scripts/verify_hierarchical_p8.sh              # 主路径（R1–R4）
#   bash scripts/verify_hierarchical_p8.sh --self-test  # 逐条投毒 ⇒ 必红
#   RDDF_CHANGE_BASE=<ref> bash scripts/verify_hierarchical_p8.sh   # 指定 base
set -uo pipefail

cd "$(dirname "$0")/.." || exit 2
ROOT="$(pwd)"

# ── 已裁决的公开 API 面（T3-005 的四个公共头）────────────────────────────────
ALLOWED_INCLUDE=(
    "include/codegen_verilog.h"
    "include/hierarchy_verilog.h"
    "include/core/verilator_backend.h"
    "include/core/verilator_port_set.h"
)

# ── 本 change 的 base（= T3-005 的第一个 commit 的父）─────────────────────────
# ⚠️ 可用 `RDDF_CHANGE_BASE` 覆盖（例如在别的 checkout 里跑）
DEFAULT_BASE="1453a8f92bb31d16b3e1d7594bd385f7ea835f55"
BASE="${RDDF_CHANGE_BASE:-$DEFAULT_BASE}"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/cpphdl_p8.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

fails=0
ok()   { printf '\033[0;32m✅ %s\033[0m\n' "$*"; }
fail() { printf '\033[0;31m❌ %s\033[0m\n' "$*"; fails=$((fails + 1)); }
note() { printf '  ::notice %s\n' "$*"; }

# ── 检查函数（供主路径与 --self-test **共用同一份** ⇒ 不会漂移）──────────────
# $1 = include/ 下与本 change 相关的文件清单（每行一个）
check_include_set() {
    local list="$1" bad=0 f
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        local allowed=0 a
        for a in "${ALLOWED_INCLUDE[@]}"; do
            [ "$f" = "$a" ] && allowed=1 && break
        done
        if [ "$allowed" -ne 1 ]; then
            fail "R4 公开 API 面越界：'$f' 不在已裁决的 4 个公共头之内"
            bad=1
        fi
    done < "$list"
    [ "$bad" -eq 0 ] && ok "R4 公开 API 面 ⊆ 已裁决的 4 个公共头"
    return $bad
}

run_main() {
    # R1：`include/` 下不得有 harness 头
    local found
    found="$(find include -type f \( -name '*equivalence*' -o -name '*hier_equivalence*' \) 2>/dev/null)"
    if [ -n "$found" ]; then
        fail "R1 include/ 下出现 harness 头："
        printf '%s\n' "$found" | sed 's/^/     /'
    else
        ok "R1 include/ 下无 harness 头"
    fi

    # R2：`include/` 下不得出现 harness 的符号
    if grep -rln -E 'hier_equivalence|run_flat_vs_hier|emit_products|compare_products' \
            include/ 2>/dev/null | grep -q .; then
        fail "R2 include/ 下出现 harness 符号："
        grep -rln -E 'hier_equivalence|run_flat_vs_hier|emit_products|compare_products' \
            include/ 2>/dev/null | sed 's/^/     /'
    else
        ok "R2 include/ 下无 harness 符号"
    fi

    # R3：harness MUST 在 tests/
    if [ -f tests/hier_equivalence.h ]; then
        ok "R3 harness 落在 tests/（tests/hier_equivalence.h）"
    else
        fail "R3 tests/hier_equivalence.h 不存在 ⇒ R1/R2 可能是「文件还没建」的假绿"
    fi

    # R4：公开 API 面（需要 git 历史）
    if ! git rev-parse --verify "$BASE" >/dev/null 2>&1; then
        note "R4 跳过：base '$BASE' 不可解析（CI 的浅克隆没有这段历史）"
        note "     ⇒ 在有完整历史的环境用 RDDF_CHANGE_BASE=<ref> 重跑"
    else
        git diff --name-only "$BASE"..HEAD -- include/ | sort > "$WORK/include_files.txt"
        check_include_set "$WORK/include_files.txt"
    fi
}

run_self_test() {
    local rc=0

    # ① 合法清单 ⇒ 绿
    printf '%s\n' "${ALLOWED_INCLUDE[@]}" > "$WORK/ok.txt"
    check_include_set "$WORK/ok.txt" >/dev/null 2>&1 \
        && ok "self-test: 合法清单 ⇒ 绿" \
        || { fail "self-test: 合法清单竟被判红"; rc=1; }

    # ② 越界清单（新增公共头）⇒ MUST 红
    printf '%s\n' "${ALLOWED_INCLUDE[@]}" "include/core/hier_equivalence_public.h" > "$WORK/bad.txt"
    if check_include_set "$WORK/bad.txt" >/dev/null 2>&1; then
        fail "self-test: 越界清单（新增公共头）竟被判绿"
        rc=1
    else
        ok "self-test: 越界清单 ⇒ 红"
    fi

    # ③ R1/R2/R3 的「假绿」形态：harness 头被搬进 include/ ⇒ MUST 红
    if [ -n "$(find include -type f -name '*equivalence*' 2>/dev/null)" ]; then
        fail "self-test: include/ 下真的存在 harness 头 ⇒ 形态不对"
        rc=1
    else
        ok "self-test: 当前 include/ 下确无 harness 头（R1 的前提成立）"
    fi

    return $rc
}

if [ "${1:-}" = "--self-test" ]; then
    run_self_test
    exit $?
fi

run_main
if [ "$fails" -ne 0 ]; then
    printf '\033[0;31mverify_hierarchical_p8: FAIL（%d 条违规）\033[0m\n' "$fails"
    exit 1
fi
printf '\033[0;32m✅ verify_hierarchical_p8: PASS（R1–R4）\033[0m\n'
exit 0
