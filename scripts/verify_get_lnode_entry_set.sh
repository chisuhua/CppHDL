#!/usr/bin/env bash
# scripts/verify_get_lnode_entry_set.sh
#
# 护栏：`get_lnode` 的**入口集合**契约（**ADR-053**，`docs/adr/ADR-053-get-lnode-entry-set.md`）。
#
# 它防什么
# --------------------------------------------------------------------------
# `get_lnode` MUST NOT 存在**无约束的通用**入口（`template <typename T> …(const T&)`
# 且**无** requires 约束）。这样的入口有**两重**危害：
#
#   ① **对有 `impl()` 的类型「死」** —— C++20「约束更严者胜」偏序 ⇒ 受约束版恒胜
#      ⇒ 无约束版**永不选中** ⇒ 纯死代码；
#   ② **对无 `impl()` 的类型「有害」** —— 其返回类型**显式** ⇒
#      `requires { get_lnode(x); }` 只检查重载决议、**不实例化函数体**
#      ⇒ ⇒ 探测返回 **`true`**（**假阳性**的能力探测）；且报错会指向
#      **框架内部**（实测：`bundle_base.h: 'multi_if<…>' has no member named 'impl'`）
#      ⇒ ⇒ 用户误判为框架 bug。
#
# 四条判据
# --------------------------------------------------------------------------
#   **R1**（存在性）：`guard_ok.cpp` MUST **编译通过** —— 它断言「无 `impl()` 的类型
#        无法被 `get_lnode` 解析」⇒ 若存在无约束通用入口 ⇒ 编译失败 ⇒ 违规。
#        ⇒ 该 fixture 在 **GCC 与 Clang 下各编一次**（requires / 约束的行为可能分叉
#        ⇒ 单侧通过 = 假验收）。
#
#   **R2**（判别力）：`guard_poisoned.cpp` MUST **编译失败** —— 它**故意**注入一个
#        无约束通用入口 ⇒ ⇒ 若它**通过** ⇒ 说明 R1 的判据**恒真** ⇒ 护栏无判别力
#        ⇒ **违规**。⇒ 这是「护栏自证」的机制。
#
#   **R3**（诊断质量）：`diagnostic.cpp` MUST **编译失败**，且错误文本
#        MUST 含「impl」（约束不满足的原因被讲清）且 MUST **不含**
#        `bundle_base.h`（错误**没有**指向框架内部）⇒ **GCC 与 Clang 各验一次**
#        （两者的 constraint 诊断文本不同 ⇒ 只验一侧 = 假验收）。
#        ⚠️ **判别性**：无约束入口一旦被加回来 ⇒ 错误会指向 `bundle_base.h`
#        ⇒ ⇒ 无需改源码即可完成对照。
#
#   **R4**（残留清理）：`include/core/bundle/bundle_base.h` MUST NOT 仍含
#        `get_lnode` 的**定义**（那个无约束重载已于 ADR-053 物理删除）。
#
# 用法
# --------------------------------------------------------------------------
#   scripts/verify_get_lnode_entry_set.sh            # 主路径
#   scripts/verify_get_lnode_entry_set.sh --self-test # 自检（判据自身有判别力）
#
# 退出码：0 = 通过；1 = 违规。

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURES="$REPO/tests/fixtures/get_lnode_entry_set"
INCLUDES=(-I "$REPO/include" -I "$REPO/include/core" -I "$REPO/include/ast"
          -I "$REPO/include/lnode" -I "$REPO/include/utils"
          -I "$REPO/external/inipp")

FAIL=0
note() { printf '   %s\n' "$*"; }
fail() { printf '❌ %s\n' "$*"; FAIL=1; }
ok()   { printf '✅ %s\n' "$*"; }

# ── 编译器探测 ──────────────────────────────────────────────────────────────
CXX_LIST=()
for c in g++ clang++; do
    if command -v "$c" >/dev/null 2>&1; then CXX_LIST+=("$c"); fi
done
if [ ${#CXX_LIST[@]} -eq 0 ]; then
    echo "❌ 未找到任何 C++ 编译器（需要 g++ 或 clang++）"
    exit 1
fi
note "编译器：${CXX_LIST[*]}"

# compile <file> <compiler> —— 成功返回 0，失败返回 1，诊断写入 stdout+stderr
compile() {
    local f="$1" c="$2"
    "$c" -std=c++23 -fsyntax-only "${INCLUDES[@]}" "$f" 2>&1
}

# ── R1：存在性 ─────────────────────────────────────────────────────────────
r1() {
    local f="$FIXTURES/guard_ok.cpp" c
    for c in "${CXX_LIST[@]}"; do
        local out rc
        out="$(compile "$f" "$c")"; rc=$?
        if [ $rc -ne 0 ]; then
            fail "R1 [$c] guard_ok.cpp 编译失败 ⇒ 存在无约束的通用 get_lnode 入口"
            printf '%s\n' "$out" | head -25 | sed 's/^/      /'
            return
        fi
    done
    ok "R1 无约束通用入口不存在（${CXX_LIST[*]} 各编译一次，均通过）"
}

# ── R2：判别力（护栏自证）────────────────────────────────────────────────
r2() {
    local f="$FIXTURES/guard_poisoned.cpp" c
    for c in "${CXX_LIST[@]}"; do
        local out rc
        out="$(compile "$f" "$c")"; rc=$?
        if [ $rc -eq 0 ]; then
            fail "R2 [$c] guard_poisoned.cpp 竟然编译通过 ⇒ R1 的判据**恒真** ⇒ 护栏无判别力"
            return
        fi
        if ! printf '%s' "$out" | grep -q "MUTATION_DETECTED"; then
            fail "R2 [$c] guard_poisoned.cpp 编译失败，但错误不是来自那条 static_assert ⇒ 判据位置不对"
            printf '%s\n' "$out" | head -20 | sed 's/^/      /'
            return
        fi
    done
    ok "R2 判别力成立（注入无约束入口 ⇒ static_assert 转红，${CXX_LIST[*]} 各验一次）"
}

# ── R3：诊断质量 ──────────────────────────────────────────────────────────
r3() {
    local f="$FIXTURES/diagnostic.cpp" c
    for c in "${CXX_LIST[@]}"; do
        local out rc
        out="$(compile "$f" "$c")"; rc=$?
        if [ $rc -eq 0 ]; then
            fail "R3 [$c] diagnostic.cpp 竟然编译通过 ⇒ 无匹配时**没有**硬错误 ⇒ 诊断条款无判别力"
            return
        fi
        if ! printf '%s' "$out" | grep -q "impl"; then
            fail "R3 [$c] 错误文本未提及 'impl' ⇒ 约束不满足的原因没被讲清楚"
            printf '%s\n' "$out" | head -20 | sed 's/^/      /'
            return
        fi
        if printf '%s' "$out" | grep -q "bundle_base.h"; then
            fail "R3 [$c] 错误文本指向了框架内部（bundle_base.h）⇒ 违反「诊断 MUST 指向调用点」"
            printf '%s\n' "$out" | head -20 | sed 's/^/      /'
            return
        fi
    done
    ok "R3 诊断指向调用点 + 含约束不满足提示 + 不指框架内部（${CXX_LIST[*]} 各验一次）"
}

# ── R4：通用性扫描（**权威**判据）────────────────────────────────────────
# ⚠️ **为什么必须源码级扫描**（实测踩中的盲区）：R1–R3 是**编译期**判据 ⇒ 它们
#    只能看到 **fixture 自己 include 的**那些头文件 ⇒ ⇒ 缺陷真正的所在地
#    （`core/bundle/bundle_base.h`）若**没被** fixture include ⇒ ⇒ 护栏**漏报**
#    （实测：恢复那个无约束重载后，R1–R3 **仍全绿** ⇒ 只有 R4 抓到）⇒ ⇒ 本条
#    用分析器扫描 `include/**` + `src/**` 的**全部**声明点兜底。
r4() {
    local out rc
    out="$(python3 "$REPO/scripts/lib/get_lnode_entry_set.py" "$REPO" 2>&1)"; rc=$?
    if [ $rc -ne 0 ]; then
        fail "R4 存在无约束的**通用** get_lnode 入口"
        printf '%s\n' "$out" | sed 's/^/      /'
        return
    fi
    ok "R4 源码级扫描：include/** + src/** 无「无约束的通用 get_lnode」入口"
}

case "${1:-}" in
    --self-test)
        echo "自检：逐条确认判据**自身**有判别力"
        # ① R1 的 fixture 存在且非空
        for f in guard_ok.cpp guard_poisoned.cpp diagnostic.cpp; do
            if [ ! -s "$FIXTURES/$f" ]; then
                echo "❌ 缺 fixture：$FIXTURES/$f"
                exit 1
            fi
        done
        ok "三个 fixture 均存在"
        # ② 把 R1 的 fixture 临时**投毒** ⇒ 它 MUST 编译失败 ⇒ 证明 R1 会抓
        TMPF="$(mktemp -d)"
        trap 'rm -rf "$TMPF"' EXIT
        cp "$FIXTURES/guard_ok.cpp" "$TMPF/poisoned_ok.cpp"
        python3 - "$TMPF/poisoned_ok.cpp" <<'PY'
import sys, pathlib
p = pathlib.Path(sys.argv[1]); s = p.read_text(encoding='utf-8')
inject = """
namespace ch::core {
template <typename T> lnode<T> get_lnode(const T &) { return lnode<T>(nullptr); }
} // namespace ch::core
"""
s = s.replace('namespace {', inject + '\nnamespace {', 1)
p.write_text(s, encoding='utf-8')
PY
        if compile "$TMPF/poisoned_ok.cpp" g++ >/dev/null 2>&1; then
            echo "❌ R1 的 fixture 被投毒后**仍然编译通过** ⇒ R1 无判别力"
            exit 1
        fi
        ok "R1 的 fixture 被投毒后编译失败 ⇒ R1 确实会抓"
        # ③ ⚠️ **权威**对照：把**真实头文件**里的无约束入口加回来 ⇒
        #    分析器 MUST 报违规 ⇒ ⇒ 证明 R4 覆盖「fixture include 不到」的地方
        BB="$REPO/include/core/bundle/bundle_base.h"
        cp "$BB" "$TMPF/bb_backup.h"
        python3 "$REPO/scripts/lib/get_lnode_entry_set.py" --inject-unconstrained "$BB"
        if python3 "$REPO/scripts/lib/get_lnode_entry_set.py" "$REPO" >/dev/null 2>&1; then
            cp "$TMPF/bb_backup.h" "$BB"
            echo "❌ 真实头文件被投毒后分析器**仍然通过** ⇒ R4 无判别力"
            exit 1
        fi
        cp "$TMPF/bb_backup.h" "$BB"
        ok "真实头文件被投毒后分析器报违规 ⇒ R4 确实会抓"
        # ④ 确认已还原
        if python3 "$REPO/scripts/lib/get_lnode_entry_set.py" "$REPO" >/dev/null 2>&1; then
            ok "真实头文件已还原"
        else
            echo "❌ 还原失败"
            exit 1
        fi
        echo "✅ 自检通过（R1 / R4 的判别力均已实测）"
        exit 0
        ;;
    "")
        echo "护栏：get_lnode 的入口集合契约（ADR-053）"
        # 分析器自身的判别力（先确认分析器可信，再用它判仓）
        if ! python3 "$REPO/scripts/lib/get_lnode_entry_set.py" --self-test "$REPO" >/dev/null 2>&1; then
            fail "分析器自检失败 ⇒ R4 的判据不可信"
        fi
        r1; r2; r3; r4
        if [ $FAIL -ne 0 ]; then
            echo ""
            echo "❌ verify_get_lnode_entry_set: FAIL"
            exit 1
        fi
        echo ""
        echo "✅ verify_get_lnode_entry_set: PASS"
        exit 0
        ;;
    *)
        echo "用法：$0 [--self-test]"
        exit 2
        ;;
esac
