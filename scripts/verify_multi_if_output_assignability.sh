#!/usr/bin/env bash
# scripts/verify_multi_if_output_assignability.sh
#
# 护栏：`chlib::multi_if` 的**输出可赋值性**与**条件可接受性**
# （**E1** + **E3**，ADR-052 Non-Goal #6 的两块）。
#
# 它防什么
# --------------------------------------------------------------------------
# **E1**：`multi_if` MUST 能**直接赋给 `ch_out`**。
#      缺陷形态：`ch_out<T>::operator=(const U &)` 走 `get_lnode(value)` ⇒
#      而 `multi_if<T>` **无** `impl()` ⇒ ⇒ **无匹配重载** ⇒ ⇒
#      `io().out = if_then(c, a).else_(b);` **编译不过**
#      （实测：`include/core/io.h:115: error: no matching function for call to
#      'get_lnode(const chlib::multi_if<ch::core::ch_uint<9> >&)'`）
#      ⇒ ⇒ 而 `select` 对同类操作数**可用** ⇒ ⇒ 框架内不一致。
#
# **E3**：`if_then` 的条件 MUST 能是 **`ch_in<ch_bool>` 端口**。
#      缺陷形态：条件形参是 `const ch_bool &` ⇒ ⇒ `ch_in<ch_bool>` **不能**绑
#      ⇒ ⇒ `if_then(io().en, x)` **编译不过** ⇒ ⇒ 而 `_if`（语句块形式）
#      对端口**可用** ⇒ ⇒ 框架内不一致。
#
# ⚠️ **两条修复都在 `chlib` 层** ⇒ ⇒ **`core/` 零改动**。
#
# 四条判据
# --------------------------------------------------------------------------
#   **R0**（fail-closed）：两个 fixture MUST **存在且非空**。
#        ⚠️ **实测踩中的护栏盲区**：文件不存在时编译器报「No such file」⇒ ⇒
#        会被 R1 误判为「编译失败」（**假绿**）⇒ ⇒ 且 R2 也判「对照正确」
#        ⇒ ⇒ **两条判据同时假绿** ⇒ ⇒ 必须显式检查。
#   **R1**（接受面）：`pos_output_assignability.cpp` MUST **编译通过**
#        （**GCC + Clang 各一次**）。
#   **R2**（判别力）：`neg_control.cpp` MUST **编译失败**且错误指向本文件。
#   **R3**（源码级）：`if.h` MUST 同时含 ① `get_lnode(const multi_if<T> &)`
#        （**类型专用**入口 ⇒ **不**违反 ADR-053）与 ② `if_then` 的**模板化**
#        条件参数 ⇒ ⇒ 防止「只实现了两条」或「改回 `const ch_bool &`」。
#
# 用法
# --------------------------------------------------------------------------
#   scripts/verify_multi_if_output_assignability.sh            # 主路径
#   scripts/verify_multi_if_output_assignability.sh --self-test # 判据自身的判别力
#
# 退出码：0 = 通过；1 = 违规。

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURES="$REPO/tests/fixtures/multi_if_output"
IF_H="$REPO/include/chlib/if.h"
INCLUDES=(-I "$REPO/include" -I "$REPO/include/core" -I "$REPO/include/ast"
          -I "$REPO/include/lnode" -I "$REPO/include/utils"
          -I "$REPO/external/inipp")

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
note "编译器：${CXX_LIST[*]}"

compile() {
    local f="$1" c="$2"
    "$c" -std=c++23 -fsyntax-only "${INCLUDES[@]}" "$f" 2>&1
}

# ── R0：fixture MUST 存在（fail-closed）──────────────────────────────────
r0() {
    local f
    for f in pos_output_assignability.cpp neg_control.cpp; do
        if [ ! -s "$FIXTURES/$f" ]; then
            fail "R0 缺 fixture：$FIXTURES/$f（**fail-closed** ⇒ 不继续判定）"
            return 1
        fi
    done
    ok "R0 两个 fixture 均存在且非空"
    return 0
}

# ── R1：接受面 MUST 编译通过 ─────────────────────────────────────────────
r1() {
    local f="$FIXTURES/pos_output_assignability.cpp" c
    for c in "${CXX_LIST[@]}"; do
        local out rc
        out="$(compile "$f" "$c")"; rc=$?
        if [ $rc -ne 0 ]; then
            fail "R1 [$c] pos_output_assignability.cpp 编译失败 ⇒ E1 或 E3 未修复"
            printf '%s\n' "$out" | head -25 | sed 's/^/      /'
            return
        fi
    done
    ok "R1 `multi_if` 可直接赋给 `ch_out` 且条件可为端口（${CXX_LIST[*]} 各一次）"
}

# ── R2：判别力（对照 MUST 编译失败）────────────────────────────────────────
r2() {
    local f="$FIXTURES/neg_control.cpp" c
    for c in "${CXX_LIST[@]}"; do
        local out rc
        out="$(compile "$f" "$c")"; rc=$?
        if [ $rc -eq 0 ]; then
            fail "R2 [$c] neg_control.cpp **编译通过** ⇒ R1 的判据**恒真** ⇒ 护栏无判别力"
            return
        fi
        if ! printf '%s' "$out" | grep -q "neg_control.cpp"; then
            fail "R2 [$c] 编译失败，但错误**不指向** neg_control.cpp ⇒ 判据位置不对"
            printf '%s\n' "$out" | head -20 | sed 's/^/      /'
            return
        fi
    done
    ok "R2 判别力成立（对照 fixture 编译失败且指向本文件，${CXX_LIST[*]} 各一次）"
}

# ── R3：源码级（两条修复 MUST 都在）────────────────────────────────────────
r3() {
    local missing=""
    # ① E1：`get_lnode(const multi_if<T> &)`（**类型专用**入口）
    grep -q "get_lnode(const multi_if<T> &" "$IF_H" \
        || missing="${missing}  E1：`get_lnode(const multi_if<T>&)` 缺失\n"
    grep -q "ch_width_impl<::chlib::multi_if<T>, void>" "$IF_H" \
        || missing="${missing}  E1：`ch_width_impl<multi_if<T>>` 特化缺失\n"
    # ② E3：`if_then` 的条件 MUST 是**模板**参数（不是 `const ch_bool &`）
    grep -q "auto if_then(const C &condition, const U &value)" "$IF_H" \
        || missing="${missing}  E3：`if_then` 的条件未模板化\n"
    grep -q "ch_bool cond(to_operand(condition).impl());" "$IF_H" \
        || missing="${missing}  E3：条件未经 to_operand 归一化\n"
    # ③ 负向：条件 MUST NOT 退回 `const ch_bool &`
    if grep -q "auto if_then(const ch_bool &condition" "$IF_H"; then
        missing="${missing}  E3：条件退回 \`const ch_bool &\` ⇒ 端口又绑不上\n"
    fi
    if [ -n "$missing" ]; then
        fail "R3 include/chlib/if.h 缺少实现："
        printf "$missing" | sed 's/^/      /'
        return
    fi
    ok "R3 E1 + E3 两条实现齐备（类型专用 get_lnode + 模板化条件）"
}

case "${1:-}" in
    --self-test)
        echo "自检：判据自身的判别力"
        for f in pos_output_assignability.cpp neg_control.cpp; do
            if [ ! -s "$FIXTURES/$f" ]; then
                echo "❌ 缺 fixture：$FIXTURES/$f"
                exit 1
            fi
        done
        ok "两个 fixture 均存在"
        TMPF="$(mktemp -d)"
        trap 'rm -rf "$TMPF"' EXIT
        # ① 把正例 fixture **投毒**（把直接赋 `ch_out` 改回「先转 `ch_uint`」）
        #    ⇒ 编译**仍通过** ⇒ ⇒ 故 R1 抓不到它 ⇒ ⇒ 这说明 R1 只能证明
        #    「能编译」⇒ ⇒ 必须配 R3（源码级）⇒ ⇒ 这里验证 R3 会抓。
        cp "$FIXTURES/pos_output_assignability.cpp" "$TMPF/poisoned.cpp"
        python3 - "$TMPF/poisoned.cpp" <<'PYEOF'
import sys, pathlib
p = pathlib.Path(sys.argv[1]); s = p.read_text(encoding='utf-8')
old = "    out9 = if_then<ch_uint<9>>(c, ch_uint<8>(0xA5_h)).else_(ch_uint<4>(0xF_h));"
new = "    ch_uint<9> v_ = if_then<ch_uint<9>>(c, ch_uint<8>(0xA5_h)).else_(ch_uint<4>(0xF_h));\n    out9 = v_;"
if old not in s:
    raise SystemExit("SELF_TEST_FAIL: 找不到投毒锚点")
p.write_text(s.replace(old, new, 1), encoding='utf-8')
PYEOF
        if ! compile "$TMPF/poisoned.cpp" g++ >/dev/null 2>&1; then
            echo "❌ 投毒后编译失败 ⇒ 投毒方式不对（自检本身有问题）"
            exit 1
        fi
        ok "投毒（撤回 E1）后编译仍通过 ⇒ R1 只能证明「能编译」⇒ R3 是必需的"
        # ② R2 的判据必须**真的会红**（把错误 API 改合法 ⇒ MUST 编译通过）
        cp "$FIXTURES/neg_control.cpp" "$TMPF/neg_legal.cpp"
        python3 - "$TMPF/neg_legal.cpp" <<'PYEOF'
import sys, pathlib
p = pathlib.Path(sys.argv[1]); s = p.read_text(encoding='utf-8')
old = "if_then_NO_SUCH_API<ch_uint<9>>"
new = "if_then<ch_uint<9>>"
if old not in s:
    raise SystemExit("SELF_TEST_FAIL: 找不到投毒锚点")
p.write_text(s.replace(old, new, 1), encoding='utf-8')
PYEOF
        if ! compile "$TMPF/neg_legal.cpp" g++ >/dev/null 2>&1; then
            echo "❌ 把 neg_control 的错误 API 改合法后**仍编译失败** ⇒ R2 的判据不是那个错误"
            exit 1
        fi
        ok "R2 的失败确实来自那个不存在的 API ⇒ R2 判据精确"
        echo "✅ 自检通过"
        exit 0
        ;;
    "")
        echo "护栏：multi_if 的输出可赋值性（E1）与条件可接受性（E3）"
        if r0; then r1; r2; r3; else
            echo ""
            echo "❌ verify_multi_if_output_assignability: FAIL"
            exit 1
        fi
        if [ $FAIL -ne 0 ]; then
            echo ""
            echo "❌ verify_multi_if_output_assignability: FAIL"
            exit 1
        fi
        echo ""
        echo "✅ verify_multi_if_output_assignability: PASS"
        exit 0
        ;;
    *)
        echo "用法：$0 [--self-test]"
        exit 2
        ;;
esac
