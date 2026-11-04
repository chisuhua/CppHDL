#!/usr/bin/env bash
# scripts/verify_header_odr.sh
#
# 护栏：聚合头（`include/ch.hpp`）闭包的 **ODR 安全性**。
#
# 它防什么
# --------------------------------------------------------------------------
# **R1（链接期）**：两个独立的 TU 各自 `#include "ch.hpp"` ⇒ ⇒ **链接 MUST 成功**。
#
#      ⚠️ 缺陷形态（修复前）：`include/module.h` 的 `build_hierarchical_name`
#      是**非 inline 的自由函数定义**，而该头经 `ch.hpp` 进入聚合入口
#      ⇒ ⇒ 每个包含它的 TU 都发出一份**外部定义** ⇒ ⇒ 2+ TU 链接期
#      `multiple definition`。
#
#      ⚠️ **为什么至今没暴露（不是没问题，是侥幸）**：`src/**` 的 **0 个** TU 包含
#      `ch.hpp`（⇒ `libcpphdl.a` 里没有该符号），且 Catch2 测试是**一个源文件一个
#      二进制** ⇒ ⇒ 每个二进制只有 **1 个** TU 包含 `ch.hpp`。
#      ⇒ ⇒ 条件一出现「multi-source target」就炸（下游 ChipForge 就是这种形态）。
#
#      ⇒ ⇒ **判据 MUST 是链接期，不是文本扫描**：本仓已两次证明「自研 C++ 子集分析器
#      不可能被完全闭合」（`verify_example_assertion_quality.sh` ·
#      `verify_no_duplicate_rv32i_headers.sh`）。链接器执行的是**真**判据
#      ⇒ ⇒ 它覆盖**整个 `ch.hpp` 闭包**，不止这一个函数。
#
# **R2（判别力）**：`--self-test` 原地把 `inline` 去掉 ⇒ ⇒ R1 MUST **转红**，
#      且失败原因 MUST 是 `multiple definition`（**MUST NOT** 是「编译失败」）
#      ⇒ ⇒ 还原后 R1 MUST 复绿。
#
# **R0（fail-closed）**：fixture 缺失 ⇒ MUST 报「缺 fixture」，
#      **MUST NOT** 报成「链接失败」（⇒ 本仓已踩过：fixture 缺失被当成编译失败
#      ⇒ ⇒ 两条判据同时假绿）。
#
# 用法
# --------------------------------------------------------------------------
#   scripts/verify_header_odr.sh             # 主路径
#   scripts/verify_header_odr.sh --self-test # 判据自身的判别力
#
# 退出码：0 = 通过；1 = 违规。

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURE_DIR="$REPO/tests/fixtures/header_odr_link"
TU_A="$FIXTURE_DIR/tu_a.cpp"
TU_B="$FIXTURE_DIR/tu_b.cpp"
MODULE_H="$REPO/include/module.h"
ANCHOR_RE='^inline std::string build_hierarchical_name'

INCLUDES=(-I "$REPO/include" -I "$REPO/include/core" -I "$REPO/include/ast"
          -I "$REPO/include/abstract" -I "$REPO/include/utils"
          -I "$REPO/external/inipp")

# ⚠️ `-ffunction-sections -fdata-sections` + `--gc-sections` 是**承重**的，不是优化：
#    修复后（有 `inline`）两个「空 TU」仍会各自 emit 一些**未被使用**的 inline 体
#    （典型：`context::create_node<proxyimpl, ...>`）⇒ ⇒ 它们引用库里的
#    `context::next_node_id()` / `debug_context_lifetime()` ⇒ ⇒ 不 GC 就变成
#    **undefined reference** ⇒ ⇒ 判据会报成「链接失败但非 multiple definition」。
#    ⇒ ⇒ 而这里要的是**纯 ODR 判据** ⇒ ⇒ fixture MUST 不依赖 `libcpphdl`。
#    ⚠️ 已验证 GC **不会**顺手丢掉重复定义（实测：投毒后仍报 1 条 multiple definition）
#    —— 因为非 inline 的定义是**强符号**（非 COMDAT），在 GC 之前就已冲突。
COMPILE_FLAGS=(-std=c++23 -ffunction-sections -fdata-sections)
LINK_FLAGS=(-Wl,--gc-sections)

FAIL=0
note() { printf '   %s\n' "$*"; }
fail() { printf '❌ %s\n' "$*"; FAIL=1; }
ok()   { printf '✅ %s\n' "$*"; }

CXX_LIST=()
for c in g++ clang++; do
    if command -v "$c" >/dev/null 2>&1; then CXX_LIST+=("$c"); fi
done
if [ ${#CXX_LIST[@]} -eq 0 ]; then
    echo "❌ 未找到任何 C++ 编译器（需要 g++ 或 clang++）"
    exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# ── 底层探针：编译两个 TU 再链接。**只返回结论，不打印裁定** ─────────────
# 返回值（stdout，单行）：
#   OK                 — 编译 + 链接均成功
#   COMPILE_FAIL:<tu>  — 某个 TU **编译**失败（⇒ 与 ODR 无关，必须先修编译）
#   MULTIPLE_DEF       — 链接失败且原因含 `multiple definition`（= ODR 违规）
#   LINK_FAIL          — 链接失败但**不是** multiple definition
probe() {
    local cxx="$1" dir="$2"
    mkdir -p "$dir"
    local a_obj="$dir/tu_a.o" b_obj="$dir/tu_b.o" bin="$dir/fixture"

    "$cxx" "${COMPILE_FLAGS[@]}" "${INCLUDES[@]}" -c "$TU_A" -o "$a_obj" >"$dir/compile_a.log" 2>&1 \
        || { printf 'COMPILE_FAIL:tu_a'; return; }
    "$cxx" "${COMPILE_FLAGS[@]}" "${INCLUDES[@]}" -c "$TU_B" -o "$b_obj" >"$dir/compile_b.log" 2>&1 \
        || { printf 'COMPILE_FAIL:tu_b'; return; }
    if "$cxx" "${LINK_FLAGS[@]}" "$a_obj" "$b_obj" -o "$bin" >"$dir/link.log" 2>&1; then
        printf 'OK'
    elif grep -q "multiple definition" "$dir/link.log" 2>/dev/null; then
        printf 'MULTIPLE_DEF'
    else
        printf 'LINK_FAIL'
    fi
}

show_log_tail() {
    local f="$1"
    [ -f "$f" ] && tail -5 "$f" | sed 's/^/      /'
}

# 把探针结论翻译成「通过 / 不通过」，并打印证据。
# $1=结论 $2=目录 $3=标签
judge_expect_ok() {
    local verdict="$1" dir="$2" label="$3"
    case "$verdict" in
        OK)
            ok "$label：两 TU 编译 + 链接成功"
            return 0 ;;
        COMPILE_FAIL:tu_a)
            fail "$label：**编译**失败（tu_a）⇒ 不是 ODR 问题"
            show_log_tail "$dir/compile_a.log"; return 1 ;;
        COMPILE_FAIL:tu_b)
            fail "$label：**编译**失败（tu_b）⇒ 不是 ODR 问题"
            show_log_tail "$dir/compile_b.log"; return 1 ;;
        MULTIPLE_DEF)
            fail "$label：链接失败 —— **multiple definition**（ODR 违规）"
            grep -m2 "multiple definition" "$dir/link.log" | sed 's/^/      /'
            return 1 ;;
        LINK_FAIL)
            fail "$label：链接失败，但**不是** multiple definition"
            show_log_tail "$dir/link.log"; return 1 ;;
        *)
            fail "$label：未预期的探针结论 '$verdict'"; return 1 ;;
    esac
}

# ── R0：fixture 存在性（fail-closed） ──────────────────────────────────────
echo "── R0 fixture 存在性 ──"
MISSING=0
for f in "$TU_A" "$TU_B"; do
    if [ ! -f "$f" ]; then
        fail "缺 fixture：$f（MUST NOT 被当成链接失败）"
        MISSING=1
    fi
done
if [ "$MISSING" -ne 0 ]; then
    echo
    echo "❌ verify_header_odr: FAIL（缺 fixture）"
    exit 1
fi
ok "两个 fixture TU 均存在"

# ── R1：两 TU 编译 + 链接 ─────────────────────────────────────────────────
echo
echo "── R1 两 TU 编译 + 链接（$(basename "${CXX_LIST[0]}")） ──"
V="$(probe "${CXX_LIST[0]}" "$WORK/main")"
judge_expect_ok "$V" "$WORK/main" "R1" || FAIL=1

# ── R2：--self-test（原地投毒 ⇒ 必须转红且原因正确） ──────────────────────
if [ "${1:-}" = "--self-test" ]; then
    echo
    echo "── R2 判别力自证（原地去掉 inline ⇒ R1 必须转红） ──"

    if [ ! -f "$MODULE_H" ]; then
        fail "R2：缺 $MODULE_H"
    elif ! grep -q "$ANCHOR_RE" "$MODULE_H"; then
        # fail-closed：锚点不存在 ⇒ 报告「无法投毒」，MUST NOT 报成「判别力为零」
        fail "R2：投毒锚点不存在（module.h 里找不到 '$ANCHOR_RE'）"
        note "⇒ 若实现已改用别的 inline 形态，MUST 同步更新本脚本的锚点"
    elif [ "$V" != "OK" ]; then
        # R1 本来就不绿 ⇒ 投毒对比无意义（不谎报判别力）
        fail "R2：R1 已失败 ⇒ 无法判定判别力（先让 R1 变绿）"
    else
        BACKUP="$WORK/module.h.bak"
        cp "$MODULE_H" "$BACKUP"

        # 原地投毒：普通 s///（⚠️ MUST NOT 把 ANCHOR_RE 当 sed **地址**用 ——
        # 它是基础正则，`^inline ...` 作为地址会报 "unknown command" ⇒
        # ⇒ 投毒静默失败 ⇒ 然后整个 R2 会谎报「判别力为零」）
        sed 's/^inline std::string build_hierarchical_name/std::string build_hierarchical_name/' \
            "$BACKUP" > "$WORK/module.h.poisoned"
        POISONED="$WORK/module.h.poisoned"

        # fail-closed：投毒产物必须是「非空 + 锚点已消失 + 函数名仍在」
        # （⇒ 三个条件缺任一 ⇒ 报「投毒失败」并作废本次判定，MUST NOT 判「无判别力」）
        if [ ! -s "$POISONED" ] \
           || grep -q "$ANCHOR_RE" "$POISONED" \
           || ! grep -q 'std::string build_hierarchical_name' "$POISONED"; then
            fail "R2：投毒失败（产物非空=$([ -s "$POISONED" ] && echo yes || echo no) · 锚点残留=$(grep -c "$ANCHOR_RE" "$POISONED")）⇒ 本次判定**作废**"
            cp "$BACKUP" "$MODULE_H"
        else
            cp "$POISONED" "$MODULE_H"

            if grep -q "$ANCHOR_RE" "$MODULE_H"; then
                fail "R2：投毒未生效（文件未被替换）⇒ 本次判定**作废**"
            else
                PV="$(probe "${CXX_LIST[0]}" "$WORK/poison")"
                case "$PV" in
                    MULTIPLE_DEF)
                        ok "R2：去掉 inline ⇒ 链接期 multiple definition（判别力成立）" ;;
                    OK)
                        fail "R2：去掉 inline 后 R1 **仍然通过** ⇒ 判据无判别力" ;;
                    COMPILE_FAIL:*)
                        fail "R2：去掉 inline 后变成**编译失败**（${PV#COMPILE_FAIL:}）⇒ 该红与本判据无关" ;;
                    *)
                        fail "R2：去掉 inline 后失败，但**不是** multiple definition（$PV）⇒ 该红与本判据无关"
                        show_log_tail "$WORK/poison/link.log" ;;
                esac
            fi
        fi

        # 还原：用 cp（刷新 mtime）⇒ 杜绝「还原了但读到旧内容」
        cp "$BACKUP" "$MODULE_H"
        if grep -q "$ANCHOR_RE" "$MODULE_H"; then
            ok "R2：已还原 inline"
        else
            fail "R2：还原失败 —— module.h 仍处于投毒状态，MUST 手工修复"
        fi

        if cmp -s "$BACKUP" "$MODULE_H"; then
            ok "R2：还原后与备份逐字节相同"
        else
            fail "R2：还原后与备份**不一致**"
        fi

        RV="$(probe "${CXX_LIST[0]}" "$WORK/restored")"
        judge_expect_ok "$RV" "$WORK/restored" "R2(还原后)" || FAIL=1
    fi
fi

echo
if [ "$FAIL" -eq 0 ]; then
    echo "✅ verify_header_odr: PASS"
    exit 0
else
    echo "❌ verify_header_odr: FAIL"
    exit 1
fi
