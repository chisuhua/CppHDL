#!/usr/bin/env bash
# scripts/verify_multi_if_width.sh
#
# 护栏：`chlib::multi_if` 的**接受面**契约（**ADR-052**）—— 「要么语义正确、
# 要么响亮拒绝」，不允许第三态。
#
# 它防什么
# --------------------------------------------------------------------------
# `multi_if` 的五种「框架放行了却无法正确执行」的形态：
#
#   **A** 首分支窄、后续宽  ⇒ 编译通过、**静默丢高位**（`bits<7:0>` 截断节点）
#   **B** 单分支（无 else_）  ⇒ 编译通过、**条件被彻底忽略**（零 mux ⇒ 恒等于该分支）
#   **C** `T` 推导成 port    ⇒ 编译失败（模板深渊）—— 但 `select` 对 port **可用**
#                              ⇒ 框架内不一致
#   **D** `T` = 纯算术      ⇒ ctor 放行、转换处才失败（**接受但不可用**死路）
#   **E** 显式 `T` 过窄     ⇒ 编译通过、**静默截断**
#
# ⚠️ 其中 A / E 是**静默数值损坏** ⇒ 且截断发生在**进入 mux 之前** ⇒ 两后端
#    **同构** ⇒ 双后端 parity **结构性失明**；Verilog 侧 `WIDTHTRUNC` 又被
#    **全局关闭** ⇒ ⇒ 唯一能「变响」的位置是 **C++ 前端** ⇒ ⇒ 本护栏。
#
# 四条判据
# --------------------------------------------------------------------------
#   **R1**（拒绝）：每个「拒绝」fixture MUST **编译失败**（GCC + Clang 各一次）。
#   **R2**（中性化对照）：把实现里**对应**的 `static_assert` **中性化** ⇒ 同一
#        fixture MUST **编译通过** ⇒ ⇒ 证明「拒」来自该断言（而非别的编译错误）。
#        ⚠️ **必须**做这一步：实施期实测踩中过一次 ——
#        `checked_value<T>(...)` 把显式实参绑到了**函数**模板参数 `U`
#        ⇒ 更宽的分支被**隐式窄化** ⇒ **宽度判据根本没跑到** ⇒ 而当时的
#        「拒绝」fixture **竟然编译通过**（⇒ 假绿）。
#   **R3**（正向）：每个「正向」fixture MUST **编译通过** ⇒ 防**过度实现**
#        （把「更宽 ⇒ 拒绝」误写成「不等 ⇒ 拒绝」）。
#   **R4**（源码级）：实现 MUST 同时含三条判据 —— 宽度 `static_assert` ·
#        单分支拒绝 · 值域检查 ⇒ 防止「只实现了两条」。
#
# 用法
# --------------------------------------------------------------------------
#   scripts/verify_multi_if_width.sh            # 主路径
#   scripts/verify_multi_if_width.sh --self-test # 判据自身的判别力
#
# 退出码：0 = 通过；1 = 违规。

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURES="$REPO/tests/fixtures/multi_if_width"
IF_H="$REPO/include/chlib/if.h"
UINT_TPP="$REPO/include/lnode/uint.tpp"
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

# ── 中性化（把实现里的某条 static_assert 改成恒真）────────────────────────
#
# ⚠️ **「宽度」判据有两处**（实测）：
#   ① `include/chlib/if.h` 的 `static_assert(ch_width_v<VT> <= ch_width_v<T>)`
#      （带**可行动**的逃生口提示）；
#   ② `include/lnode/uint.tpp` 的 `static_assert(W <= N, "Literal width must not
#      exceed target uint width")`（框架自带的**兜底**，对 `ch_literal_impl` 分支
#      也会触发）。
# ⇒ ⇒ 中性化「宽度」时**两处都要**处理，否则 sized-literal 的拒绝 fixture
#     **仍会**编译失败 ⇒ ⇒ R2 会误报「拒绝并非来自该断言」。
#
# ⚠️ 替换 MUST **保留原有的分号与消息结构**（否则文件语法被破坏 ⇒ 后续判据
#    全部误报 —— 实测踩中过）。
#
# kind: width | hardware
neutralize() {
    python3 - "$IF_H" "$UINT_TPP" "$1" <<'PYEOF'
import sys, pathlib
if_h, uint_tpp, kind = sys.argv[1], sys.argv[2], sys.argv[3]
NEUT = 'static_assert(true /* NEUTRALIZED BY GUARD */, "");'
# ⚠️ if.h 的两处**只换条件部分**（原消息串 + `);` 保留在后面 ⇒ 语法不被破坏）；
#    uint.tpp 那处是**整条**断言（含消息与分号）⇒ 必须整体替换。
PATS = {
    "width": [
        (if_h, "static_assert(ch_width_v<VT> <= ch_width_v<T>,",
              "static_assert(true /* NEUTRALIZED BY GUARD */,"),
        (if_h, "static_assert(ch_width_v<UU> <= ch_width_v<T>,",
              "static_assert(true /* NEUTRALIZED BY GUARD */,"),
        (uint_tpp, 'static_assert(W <= N, "Literal width must not exceed target uint width");',
                   'static_assert(true /* NEUTRALIZED BY GUARD */, "neutralized");'),
    ],
    "hardware": [
        (if_h, "static_assert(ch::core::HardwareType<T>,",
                 "static_assert(true /* NEUTRALIZED BY GUARD */,"),
    ],
}[kind]
hits = 0
for path, old, new in PATS:
    p = pathlib.Path(path)
    s = p.read_text(encoding="utf-8")
    if old in s:
        p.write_text(s.replace(old, new, 1), encoding="utf-8")
        hits += 1
if hits == 0:
    raise SystemExit("NEUTRALIZE_FAIL: 一个判据都没找到")
PYEOF
}

REJECT_FIXTURES=(reject_wide_branch reject_narrow_explicit_T
                 reject_arithmetic_T reject_wide_literal)
POS_FIXTURES=(pos_basic pos_port pos_bool1)

# ── 备份 / 还原（两个可能被中性化的文件）─────────────────────────────────
BAK_DIR=""
backup() {
    BAK_DIR="$(mktemp -d)"
    cp "$IF_H" "$BAK_DIR/if.h.bak"
    cp "$UINT_TPP" "$BAK_DIR/uint.tpp.bak"
}
restore() {
    [ -n "$BAK_DIR" ] || return 0
    cp "$BAK_DIR/if.h.bak" "$IF_H" 2>/dev/null
    cp "$BAK_DIR/uint.tpp.bak" "$UINT_TPP" 2>/dev/null
}

# ── R1：拒绝 fixture MUST 编译失败 ───────────────────────────────────────
r1() {
    local f c name
    for name in "${REJECT_FIXTURES[@]}"; do
        f="$FIXTURES/$name.cpp"
        for c in "${CXX_LIST[@]}"; do
            if compile "$f" "$c" >/dev/null 2>&1; then
                fail "R1 [$c] $name **编译通过** ⇒ 该形态未被拒绝（静默损坏！）"
                return
            fi
        done
    done
    ok "R1 ${#REJECT_FIXTURES[@]} 个拒绝 fixture 均编译失败（${CXX_LIST[*]} 各一次）"
}

# ── R2：中性化对照 ───────────────────────────────────────────────────────
r2() {
    backup
    trap 'restore; [ -n "$BAK_DIR" ] && rm -rf "$BAK_DIR"' EXIT

    local pairs=("width:reject_wide_branch" "width:reject_narrow_explicit_T"
                 "width:reject_wide_literal" "hardware:reject_arithmetic_T")
    local pair kind name c
    for pair in "${pairs[@]}"; do
        kind="${pair%%:*}"; name="${pair##*:}"
        restore
        if ! neutralize "$kind"; then
            fail "R2 无法中性化判据 '$kind' ⇒ 对照无法进行"
            return
        fi
        for c in "${CXX_LIST[@]}"; do
            if ! compile "$FIXTURES/$name.cpp" "$c" >/dev/null 2>&1; then
                fail "R2 [$c] 中性化 '$kind' 后 $name **仍编译失败** ⇒ 拒绝并非来自该断言"
                return
            fi
        done
    done
    restore
    ok "R2 中性化对照：每个拒绝都**确实**来自对应断言（${CXX_LIST[*]} 各一次）"
}

# ── R3：正向 fixture MUST 编译通过 ───────────────────────────────────────
r3() {
    local f c name out
    for name in "${POS_FIXTURES[@]}"; do
        f="$FIXTURES/$name.cpp"
        for c in "${CXX_LIST[@]}"; do
            out="$(compile "$f" "$c" 2>&1)"
            if [ $? -ne 0 ]; then
                fail "R3 [$c] $name 编译失败 ⇒ 过度实现（把合法的写法也拒了）"
                printf '%s\n' "$out" | head -20 | sed 's/^/      /'
                return
            fi
        done
    done
    ok "R3 ${#POS_FIXTURES[@]} 个正向 fixture 均编译通过（防过度实现）"
}

# ── R4：源码级 —— 三条判据 MUST 都在 ─────────────────────────────────────
r4() {
    local missing=""
    grep -q "ch_width_v<VT> <= ch_width_v<T>" "$IF_H" || missing="${missing}  宽度 static_assert（硬件分支）\n"
    grep -q "ch_width_v<UU> <= ch_width_v<T>" "$IF_H" || missing="${missing}  宽度 static_assert（sized literal）\n"
    grep -q "branches_.size() < 2" "$IF_H" || missing="${missing}  单分支拒绝\n"
    grep -q "does not fit into the" "$IF_H" || missing="${missing}  整数字面量值域检查\n"
    grep -q "negative literal" "$IF_H" || missing="${missing}  负数字面量拒绝\n"
    if [ -n "$missing" ]; then
        fail "R4 include/chlib/if.h 缺少判据："
        printf "$missing" | sed 's/^/      /'
        return
    fi
    ok "R4 实现含全部判据（宽度 · 单分支 · 值域/负数）"
}

case "${1:-}" in
    --self-test)
        echo "自检：判据自身的判别力"
        for name in "${REJECT_FIXTURES[@]}" "${POS_FIXTURES[@]}"; do
            if [ ! -s "$FIXTURES/$name.cpp" ]; then
                echo "❌ 缺 fixture：$FIXTURES/$name.cpp"
                exit 1
            fi
        done
        ok "全部 fixture 存在"
        backup
        if ! neutralize width; then
            restore
            echo "❌ 无法中性化宽度判据"
            exit 1
        fi
        if ! compile "$FIXTURES/reject_wide_branch.cpp" g++ >/dev/null 2>&1; then
            restore
            echo "❌ 中性化后 reject_wide_branch 仍编译失败 ⇒ R2 的对照无意义"
            exit 1
        fi
        restore
        ok "中性化宽度判据 ⇒ reject_wide_branch 编译通过 ⇒ R2 的对照有判别力"
        rm -rf "$BAK_DIR"; BAK_DIR=""
        echo "✅ 自检通过"
        exit 0
        ;;
    "")
        echo "护栏：multi_if 的接受面契约（ADR-052）"
        r1; r2; r3; r4
        restore
        if [ $FAIL -ne 0 ]; then
            echo ""
            echo "❌ verify_multi_if_width: FAIL"
            exit 1
        fi
        echo ""
        echo "✅ verify_multi_if_width: PASS"
        exit 0
        ;;
    *)
        echo "用法：$0 [--self-test]"
        exit 2
        ;;
esac
