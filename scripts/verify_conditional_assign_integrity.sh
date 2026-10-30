#!/usr/bin/env bash
# scripts/verify_conditional_assign_integrity.sh
#
# 护栏：`chlib::conditional_assign` 的**接受面**与**完整性**契约。
#
# 它防什么
# --------------------------------------------------------------------------
# **R1**（接受面）：`conditional_assign(target).next() = <value>` MUST 接受
#      **裸 literal**（`0_d` / `0xAA_h`）⇒ ⇒ 编译期 fixture MUST **编译通过**。
#
#      ⚠️ 缺陷形态（修复前）：`conditional_assignment::assign()` 的 else 分支
#      （**块外直写**）写的是 `(*out_) <<= value;` ⇒ 它走 `operator<<=` ⇒
#      用 `get_lnode(driver)` ⇒ ⚠️ `ch_literal_impl` **没有** `get_lnode`
#      重载 ⇒ ⇒ **裸 literal 编译不过** ⇒ ⇒ 连 `if_stmt.h` **自己的头注释
#      示例**（`conditional_assign(count).next() = 0_d;`）都编译不过。
#      ⇒ ⇒ 该分支**总被实例化**（`if` / `else` 都是模板代码）⇒ ⇒ 即使运行期
#      走不到 ⇒ 也会编译失败 ⇒ ⇒ 「能不能编译」与「走哪条分支」**无关**。
#
# **R2**（判别力）：`neg_control.cpp` MUST **编译失败**（故意用不存在的 API）
#      ⇒ ⇒ 证明 R1 的「编译通过」不是因为编译器**根本没检查**这段代码。
#      ⇒ 失败点 MUST 指向**本 fixture**（⇒ 错误文本含 `neg_control.cpp`）。
#
# **R3**（源码级）：`if_stmt.h` 的块外直写分支 MUST 经 `to_operand` 归一化
#      ⇒ ⇒ 防止「只修了一半」（例如只修 `ch_reg` 路径、漏了 `ch_out` 路径）。
#
# 用法
# --------------------------------------------------------------------------
#   scripts/verify_conditional_assign_integrity.sh            # 主路径
#   scripts/verify_conditional_assign_integrity.sh --self-test # 判据自身的判别力
#
# 退出码：0 = 通过；1 = 违规。

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURES="$REPO/tests/fixtures/conditional_assign_literal"
IF_STMT="$REPO/include/chlib/if_stmt.h"
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

# ── R0：fixture MUST **存在**（**fail-closed**）────────────────────────────
# ⚠️⚠️ **实测踩中的护栏盲区**：本护栏初版**没有**这条 ⇒ ⇒ 当 fixture
#    **文件不存在**时，编译器报「No such file」⇒ ⇒ R1 会判「编译失败」
#    （**假绿**：其实是 fixture 缺失）⇒ ⇒ 而 R2 也会判「对照正确」
#    ⇒ ⇒ **两条判据同时假绿** ⇒ ⇒ 必须显式检查 fixture 存在。
r0() {
    local f
    for f in pos_literal.cpp neg_control.cpp; do
        if [ ! -s "$FIXTURES/$f" ]; then
            fail "R0 缺 fixture：$FIXTURES/$f（**fail-closed** ⇒ 不继续判定）"
            return 1
        fi
    done
    ok "R0 两个 fixture 均存在且非空"
    return 0
}

# ── R1：接受面（裸 literal MUST 编译通过）─────────────────────────────────
r1() {
    local f="$FIXTURES/pos_literal.cpp" c
    for c in "${CXX_LIST[@]}"; do
        local out rc
        out="$(compile "$f" "$c")"; rc=$?
        if [ $rc -ne 0 ]; then
            fail "R1 [$c] pos_literal.cpp 编译失败 ⇒ `conditional_assign` **不接受裸 literal**"
            printf '%s\n' "$out" | head -25 | sed 's/^/      /'
            return
        fi
    done
    ok "R1 裸 literal 被接受（${CXX_LIST[*]} 各编译一次，均通过）"
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

# ── R3：源码级（块外直写 MUST 经 to_operand 归一化）────────────────────────
r3() {
    local missing=""
    grep -q "to_operand(value)" "$IF_STMT" \
        || missing="${missing}  块外直写分支未经 to_operand 归一化\n"
    # ⚠️ MUST 同时覆盖 `ch_reg` 与 `ch_out` 两条路径（只修一半会被抓）
    grep -q "(\*reg_)->next = value;" "$IF_STMT" \
        || missing="${missing}  ch_reg 路径缺失\n"
    grep -q "(\*out_) <<= ch_uint<ch_width_v<T>>(to_operand(value).impl());" "$IF_STMT" \
        || missing="${missing}  ch_out 路径未归一化（仍走裸 value ⇒ literal 编译不过）\n"
    if [ -n "$missing" ]; then
        fail "R3 include/chlib/if_stmt.h 的接受面实现不完整："
        printf "$missing" | sed 's/^/      /'
        return
    fi
    ok "R3 块外直写分支已归一化（ch_reg + ch_out 两条路径）"
}

case "${1:-}" in
    --self-test)
        echo "自检：判据自身的判别力"
        for f in pos_literal.cpp neg_control.cpp; do
            if [ ! -s "$FIXTURES/$f" ]; then
                echo "❌ 缺 fixture：$FIXTURES/$f"
                exit 1
            fi
        done
        ok "两个 fixture 均存在"
        # ① 把正例 fixture 临时**投毒**（把裸 literal 换成不存在的 API）
        #    ⇒ 它 MUST 编译失败 ⇒ 证明 R1 的判据**真的**在检查这段代码
        TMPF="$(mktemp -d)"
        trap 'rm -rf "$TMPF"' EXIT
        cp "$FIXTURES/pos_literal.cpp" "$TMPF/poisoned.cpp"
        python3 - "$TMPF/poisoned.cpp" <<'PYEOF'
import sys, pathlib
p = pathlib.Path(sys.argv[1]); s = p.read_text(encoding='utf-8')
old = "conditional_assign(reg).next() = 0xCC_h;"
new = "conditional_assign_NO_SUCH_API(reg).next() = 0xCC_h;"
if old not in s:
    raise SystemExit("SELF_TEST_FAIL: 找不到投毒锚点")
p.write_text(s.replace(old, new, 1), encoding='utf-8')
PYEOF
        if compile "$TMPF/poisoned.cpp" g++ >/dev/null 2>&1; then
            echo "❌ R1 的 fixture 被投毒后**仍然编译通过** ⇒ R1 无判别力"
            exit 1
        fi
        ok "R1 的 fixture 被投毒后编译失败 ⇒ R1 确实会抓"
        # ② R2 的判据必须**真的会红**（临时把 neg_control 的错误 API 改合法）
        cp "$FIXTURES/neg_control.cpp" "$TMPF/neg_legal.cpp"
        python3 - "$TMPF/neg_legal.cpp" <<'PYEOF'
import sys, pathlib
p = pathlib.Path(sys.argv[1]); s = p.read_text(encoding='utf-8')
old = "conditional_assign_NO_SUCH_API(reg).next() = 0xBB_h;"
new = "conditional_assign(reg).next() = 0xBB_h;"
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
        echo "护栏：conditional_assign 的接受面契约"
        if r0; then r1; r2; r3; else
            echo ""
            echo "❌ verify_conditional_assign_integrity: FAIL"
            exit 1
        fi
        if [ $FAIL -ne 0 ]; then
            echo ""
            echo "❌ verify_conditional_assign_integrity: FAIL"
            exit 1
        fi
        echo ""
        echo "✅ verify_conditional_assign_integrity: PASS"
        exit 0
        ;;
    *)
        echo "用法：$0 [--self-test]"
        exit 2
        ;;
esac
