#!/usr/bin/env bash
# scripts/verify_component_naming.sh
#
# 护栏：组件的**命名模型**契约（capability component-ownership-and-naming）
# —— 「组件实例路径 MUST 无重复前缀」+「组件实例 MUST 提供局部名与模块类型名
# 两个命名维度」。
#
# 它防什么
# --------------------------------------------------------------------------
# **R1（正向 · 编译期）**：CH_HIERARCHY_LAYER("name") **带名字**时 MUST 编译通过
#      ⇒ ⇒ 「成层 + 命名一体」的合并宏可用。
#
# **R2（反向 · 编译期）**：「成层却忘了命名」MUST **编译期不可表达**
#      ⇒ ⇒ CH_HIERARCHY_LAYER（**无参**）MUST 编译失败 ⇒ ⇒ 失败点 MUST 指向
#      本 fixture（否则「fixture 写坏了」会被误读成「判据生效」）。
#
# **R3（反向 · 编译期）**：Component::hierarchical_name() MUST **不存在**
#      ⇒ ⇒ 它是「第二个全路径拼接点」= 两个真源 ⇒ ⇒ 前缀重复会复发。
#
# **R4（源码级 · 独立于 R3）**：include/component.h MUST NOT 仍定义
#      hierarchical_name( ⇒ ⇒ R3 与 R4 **不可互相替代**（R3 靠「调用点编译失败」
#      ⇒ ⇒ 若有人加回该函数但同时改了 fixture，R3 会失去判别力 ⇒ R4 兜底）。
#
# **R0（fail-closed）**：fixture 缺失 ⇒ MUST 报「缺 fixture」，
#      **MUST NOT** 报成「编译失败」（⇒ 本仓已踩过：fixture 缺失被当成编译失败
#      ⇒ ⇒ 两条判据同时假绿）。
#
# 用法
# --------------------------------------------------------------------------
#   scripts/verify_component_naming.sh             # 主路径
#   scripts/verify_component_naming.sh --self-test # 判据自身的判别力
#
# 退出码：0 = 通过；1 = 违规。

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIX="$REPO/tests/fixtures/component_naming"
POS="$FIX/pos_layer_macro.cpp"
NEG_MACRO="$FIX/neg_layer_macro_missing_name.cpp"
NEG_NAME="$FIX/neg_hierarchical_name.cpp"
COMPONENT_H="$REPO/include/component.h"

INCLUDES=(-I "$REPO/include" -I "$REPO/include/core" -I "$REPO/include/ast"
          -I "$REPO/include/abstract" -I "$REPO/include/utils"
          -I "$REPO/external/nipp")

FAIL=0
note() { printf '   %s\n' "$*"; }
fail() { printf '❌ %s\n' "$*"; FAIL=1; }
ok()   { printf '✅ %s\n' "$*"; }

# ⚠️ 编译器探测走**公共 helper**（C++23 能力筛选 + 版本化 clang++ 发现）
#   ⇒ 见 scripts/lib/cxx23_compilers.sh 里记的 CI 假红根因（裸 clang++ 是 clang-14）。
source "$REPO/scripts/lib/cxx23_compilers.sh"
cxx23_probe || exit 1
CXX="${CXX_LIST[0]}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# 编译（只做语法/语义检查 ⇒ 不链接 ⇒ 不需要库）
compile_ok() {
    "$CXX" -std=c++23 -fsyntax-only "${INCLUDES[@]}" "$1" >"$2" 2>&1
}

# ── R0：fixture 存在性（fail-closed） ──────────────────────────────────────
echo "── R0 fixture 存在性 ──"
MISSING=0
for f in "$POS" "$NEG_MACRO" "$NEG_NAME"; do
    if [ ! -f "$f" ]; then
        fail "缺 fixture：$f（MUST NOT 被当成编译失败）"
        MISSING=1
    fi
done
if [ "$MISSING" -ne 0 ]; then
    echo
    echo "❌ verify_component_naming: FAIL（缺 fixture）"
    exit 1
fi
ok "三个 fixture 均存在"

# ── R1：正向 MUST 编译通过 ───────────────────────────────────────────────
echo
echo "── R1 合并宏（带名字）MUST 编译通过 ──"
if compile_ok "$POS" "$WORK/pos.log"; then
    ok "R1：pos_layer_macro.cpp 编译通过"
else
    fail "R1：pos_layer_macro.cpp **编译失败**（带名字的合并宏不可用）"
    tail -8 "$WORK/pos.log" | sed 's/^/      /'
fi

# ── R2：反向（缺名）MUST 编译失败，且失败点指向 fixture ──────────────────
echo
echo "── R2 「成层却忘了命名」MUST 编译期不可表达 ──"
if compile_ok "$NEG_MACRO" "$WORK/neg_macro.log"; then
    fail "R2：缺名的 CH_HIERARCHY_LAYER **编译通过了** ⇒ 「成层却忘了命名」仍可表达"
else
    if grep -q "CH_HIERARCHY_LAYER" "$WORK/neg_macro.log"; then
        ok "R2：缺名形态编译失败（失败点指向宏）"
    else
        fail "R2：编译失败了，但**失败原因不是宏缺参**（⇒ 可能是 fixture 写坏了）"
        tail -5 "$WORK/neg_macro.log" | sed 's/^/      /'
    fi
fi

# ── R3：反向（hierarchical_name）MUST 编译失败，且失败点指向该符号 ──────
echo
echo "── R3 hierarchical_name() MUST 不存在 ──"
if compile_ok "$NEG_NAME" "$WORK/neg_name.log"; then
    fail "R3：hierarchical_name() **编译通过了** ⇒ 第二个全路径拼接点还在"
else
    if grep -q "hierarchical_name" "$WORK/neg_name.log"; then
        ok "R3：调用 hierarchical_name() 编译失败（失败点指向该符号）"
    else
        fail "R3：编译失败了，但**失败原因不是 hierarchical_name 缺失**（⇒ 可能是 fixture 写坏了）"
        tail -5 "$WORK/neg_name.log" | sed 's/^/      /'
    fi
fi

# ── R4：源码级（独立于 R3）──────────────────────────────────────────────
echo
echo "── R4 include/component.h MUST NOT 仍定义 hierarchical_name( ──"
if [ ! -f "$COMPONENT_H" ]; then
    fail "R4：缺 $COMPONENT_H"
elif grep -qE '^\s*std::string[[:space:]]+hierarchical_name[[:space:]]*\(' "$COMPONENT_H"; then
    fail "R4：include/component.h 仍定义 hierarchical_name( ⇒ 第二个真源还在"
    grep -nE '^\s*std::string[[:space:]]+hierarchical_name[[:space:]]*\(' "$COMPONENT_H" | sed 's/^/      /'
else
    ok "R4：include/component.h 无 hierarchical_name 定义"
fi

# ── 自证：中和 R2 / R3 ⇒ 对应判据 MUST 转红 ──────────────────────────────
if [ "${1:-}" = "--self-test" ]; then
    echo
    echo "── 判别力自证（中和 R2 / R3 ⇒ 对应判据 MUST 转红） ──"

    for pair in "neg_macro:$NEG_MACRO:R2" "neg_name:$NEG_NAME:R3"; do
        tag="${pair%%:*}"; rest="${pair#*:}"
        src="${rest%%:*}"; rid="${rest##*:}"
        backup="$WORK/$(basename "$src").bak"
        cp "$src" "$backup"

        # 中和：把「故意失败」的那一行改成合法代码 ⇒ 该 fixture 应当**编译通过**
        #      ⇒ ⇒ 对应判据 MUST **转红**（否则判据恒绿 / 恒红，无判别力）
        case "$tag" in
            neg_macro)
                sed -i 's/CH_HIERARCHY_LAYER;/CH_HIERARCHY_LAYER("neutralized");/' "$src" ;;
            neg_name)
                sed -i 's/c\.hierarchical_name()\.size()/c.name().size()/' "$src" ;;
        esac

        # 中和后 fixture MUST **编译通过** ⇒ ⇒ 对应判据（「MUST 编译失败」）
        # MUST 因此**转红** ⇒ ⇒ 这正是判别力成立的证据。
        # ⚠️ 若中和后**仍**编译失败 ⇒ 中和无效（fixture 另有无关错误）
        #    ⇒ ⇒ 那种情况下判据是**假绿**（失败原因不是被测的那一条）。
        if compile_ok "$src" "$WORK/$tag.neutral.log"; then
            ok "$rid：中和后 fixture 编译通过 ⇒ 对应判据会转红（判别力成立）"
        else
            fail "$rid：中和后 fixture **仍然编译失败** ⇒ 中和无效（fixture 另有无关错误 ⇒ 判据是假绿）"
            tail -4 "$WORK/$tag.neutral.log" | sed 's/^/      /'
        fi

        cp "$backup" "$src"          # 还原（刷新 mtime）
        if cmp -s "$backup" "$src"; then
            ok "$rid：已还原 fixture（逐字节相同）"
        else
            fail "$rid：还原失败 ⇒ fixture 仍处于中和状态，MUST 手工修复"
        fi
    done
fi

echo
if [ "$FAIL" -eq 0 ]; then
    echo "✅ verify_component_naming: PASS"
    exit 0
else
    echo "❌ verify_component_naming: FAIL"
    exit 1
fi
