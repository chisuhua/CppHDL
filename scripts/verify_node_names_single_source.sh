#!/usr/bin/env bash
# scripts/verify_node_names_single_source.sh
#
# 护栏：**「`node_names_[` 的 62 行 / 64 次无需改动」的机械证据**
#       （phase-6 **P1** · ADR-066 决策 ① · A8 面 ①）
#
# ⚠️ **它在证什么**：ADR-066 决策 ① 规定「父侧落点网名 = **驱动端点的 ctx 全局名**」
#    ⇒ ⇒ 父侧表达式的 RHS 名**本来就是**那个名字
#    ⇒ ⇒ 发射器只要在父 module 里声明同名 `logic` 网 ⇒ ⇒ RHS **自动可解析**
#    ⇒ ⇒ **codegen 侧零改动**（`src/codegen_verilog.cpp` 的 62 个取址点一行不改）。
#    ⇒ ⇒ 但「零改动」是**断言**，不是**证据** ⇒ ⇒ 本护栏把它拆成四条可复跑的判据。
#
# 判据：
#   R1 **grep 清单 + 分母钉死**：`src/codegen_verilog.cpp` 里 `node_names_[` 的
#      **行数**（62）与**出现次**（64）—— ⚠️ 两个数**单位不同**（62 行里有 2 行
#      各出现 2 次）⇒ ⇒ **都钉** ⇒ ⇒ 漂移即红（A8 面 ① 的**分母**变了 ⇒
#      「无需改动」的论证 MUST 重做）。同时**打印完整清单**（行号 + 命中内容）
#      ⇒ ⇒ 可审计。
#   R2 **单写入点**：该文件里 `node_names_` 的**赋值**恰为 **1** 处 ⇒ ⇒ 64 次读
#      **全部**取自同一张表 ⇒ ⇒ 结构上**不可能**出现「某个 site 用了另一套命名」。
#      另：`node_names_` 的**容器级** mutator（`insert` / `emplace` / `erase` /
#      `clear` / `reserve` / `swap`）**零命中** ⇒ ⇒ 那次赋值之后**没人**改过它。
#   R3 **命名真源同源**：`src/codegen_verilog.cpp` 与 `src/hierarchy_verilog.cpp`
#      **都**调用 `ch::build_verilog_name_table(<ctx>->get_eval_list())`
#      （**同一函数** + **同一实参形态**）⇒ ⇒ 落点网名 ≡ 父侧 RHS 名
#      （**同一张表**对**同一个节点**的同一次查询）。
#   R4 **负向注入（承重性）**：把 `print_binary_op` 的 `lhs_name`（**62 处之一**）
#      改成「表里的名字 + 后缀」⇒ ⇒ 父侧 RHS 变成一个**未在父作用域声明**的名字
#      ⇒ ⇒ **`scripts/verify_hierarchical_rtl.sh` 的 R9 必红** ⇒ ⇒ 还原后**必绿**
#      ⇒ ⇒ 证明「同源」是**承重**的（不是「反正都能编译」）。
#      ⚠️ **注入后 MUST 确认仍能编译**（编译失败 ⇒ 跑的是**旧二进制** ⇒ 假红）
#      —— 仓内纪律「还原源码 ≠ 复原（二进制也是状态）」。
#      ⚠️ **注入会改仓内源码** ⇒ ⇒ 只在 `--inject` 模式下做，且 MUST 还原
#      （字节级校验 + 中断保护）。
#
# 用法：
#   bash scripts/verify_node_names_single_source.sh            # R1–R3（只读 · CI 安全）
#   bash scripts/verify_node_names_single_source.sh --inject   # R1–R4（**源码级**注入 ⇒ 需重建）
#   bash scripts/verify_node_names_single_source.sh --self-test # R1–R3 的判别力自证（**只动副本**）
#
# 环境：
#   CPPHDL_BUILD_DIR   构建目录（默认 `build`）
#   CPPHDL_JOBS        重建并行度（默认 `nproc`）
#
# 退出码：0 = 通过 · 1 = 判据失败 · 2 = 用法 / 环境错误
# ============================================================================
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$REPO_ROOT" || exit 2

BUILD_DIR="${CPPHDL_BUILD_DIR:-build}"
JOBS="${CPPHDL_JOBS:-$(nproc 2>/dev/null || echo 4)}"

SRC_CG="$REPO_ROOT/src/codegen_verilog.cpp"
SRC_HV="$REPO_ROOT/src/hierarchy_verilog.cpp"
GENERATOR="$REPO_ROOT/$BUILD_DIR/tests/hier_rtl_products"
HIER_GUARD="$REPO_ROOT/scripts/verify_hierarchical_rtl.sh"

# ⚠️ 本脚本**不自己跑 lint**：R4 的 lint 判据**唯一权威**是既有护栏
#    `scripts/verify_hierarchical_rtl.sh` 的 **R9**（ADR-060 §2.5 的可执行定义）。
#    这里只做**前置探测**（缺 verilator ⇒ R0 报出可行动的错，而不是让 R4 在中途莫名失败）。
VERILATOR=""
for cand in "$REPO_ROOT/$BUILD_DIR/verilator-install/bin/verilator" \
            "$(command -v verilator || true)"; do
    [ -n "$cand" ] && [ -x "$cand" ] && { VERILATOR="$cand"; break; }
done

# A8 面 ① 的**分母**（两个单位都钉：行数 / 出现次）
BASELINE_LINES=62
BASELINE_OCCURRENCES=64

# 负向注入的**唯一锚点**（3 行上下文 ⇒ 在本文件内唯一）
INJECT_ANCHOR_OLD='        std::string lhs_name = node_names_[lhs_node];
        std::string rhs_name = node_names_[rhs_node];
        std::string op_str = get_op_str(node->op());'
INJECT_ANCHOR_NEW='        std::string lhs_name = node_names_[lhs_node] + "__p1_injected";
        std::string rhs_name = node_names_[rhs_node];
        std::string op_str = get_op_str(node->op());'
INJECT_MARKER="__p1_injected"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/cpphdl_nns.XXXXXX")"
mkdir -p "$WORK/logs"

# ⚠️ **中断安全**：注入期间源文件是**被改过**的 ⇒ ⇒ 任何退出路径（含 Ctrl-C /
#    异常）都 MUST 把源码还原 ⇒ ⇒ 否则仓内会留下一份带注入的 `codegen_verilog.cpp`。
SRC_BACKUP=""
ST_ROOT=""
cleanup() {
    if [ -n "$SRC_BACKUP" ] && [ -f "$SRC_BACKUP" ]; then
        cp "$SRC_BACKUP" "$SRC_CG" && echo ":: 已还原 $SRC_CG（中断保护）" >&2
    fi
    [ -n "$ST_ROOT" ] && rm -rf "$ST_ROOT"
    rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

DO_INJECT=0
SELF_TEST=0
case "${1:-}" in
"") ;;
--inject) DO_INJECT=1 ;;
--self-test) SELF_TEST=1 ;;
-h|--help)
    sed -n '2,46p' "${BASH_SOURCE[0]}"
    exit 0 ;;
*)
    echo "❌ unexpected arg: $1（用法：$0 [--inject|--self-test]）" >&2
    exit 2 ;;
esac

fails=0
ok()   { printf '\033[0;32m✅ %s\033[0m\n' "$*"; }
fail() { printf '\033[0;31m❌ %s\033[0m\n' "$*" >&2; fails=$((fails + 1)); }
note() { printf '   :: %s\n' "$*"; }

# ── R1：grep 清单 + 分母钉死 ───────────────────────────────────────────────
r1_grep_manifest() {  # $1 = codegen 源文件（默认仓内真源）
    local cg="${1:-$SRC_CG}" lines occ bad=0
    lines="$(grep -c 'node_names_\[' "$cg")"
    occ="$(grep -o 'node_names_\[' "$cg" | wc -l | tr -d ' ')"
    if [ "$lines" != "$BASELINE_LINES" ]; then
        fail "R1 行数漂移：$cg 的 \`node_names_[\` = $lines 行（基线 $BASELINE_LINES）"
        bad=1
    fi
    if [ "$occ" != "$BASELINE_OCCURRENCES" ]; then
        fail "R1 出现次漂移：$cg 的 \`node_names_[\` = $occ 次（基线 $BASELINE_OCCURRENCES）"
        bad=1
    fi
    if [ "$bad" -eq 0 ]; then
        ok "R1 grep 清单：$lines 行 / $occ 次（分母与基线一致 ⇒ A8 面 ① 的论证仍成立）"
    fi
    # 清单本体（**这就是证据** ⇒ 打印出来 ⇒ 可审计 / 可复跑对照）
    note "清单（$cg · $lines 行 · $occ 次）："
    grep -n 'node_names_\[' "$cg" | sed 's/^/       /'
    return "$bad"
}

# ── R2：`node_names_` 的**单写入点** ───────────────────────────────────────
r2_single_write() {  # $1 = codegen 源文件（默认仓内真源）
    local cg="${1:-$SRC_CG}" writes n mutators bad=0
    writes="$(grep -nE 'node_names_[[:space:]]*=[^=]' "$cg")"
    n="$(printf '%s\n' "$writes" | grep -c . || true)"
    if [ "$n" != "1" ]; then
        fail "R2 \`node_names_\` 的赋值点应恰为 1 处，实际 $n 处："
        printf '%s\n' "$writes" | sed 's/^/     /' >&2
        bad=1
    elif ! printf '%s\n' "$writes" | grep -q 'view_\.names'; then
        fail "R2 唯一的赋值点不是 \`node_names_ = view_.names;\`："
        printf '%s\n' "$writes" | sed 's/^/     /' >&2
        bad=1
    fi
    mutators="$(grep -nE 'node_names_\.(insert|emplace|erase|clear|reserve|swap)\b' "$cg" || true)"
    if [ -n "$mutators" ]; then
        fail "R2 \`node_names_\` 存在容器级 mutator（赋值之后仍有人改表 ⇒ 单写入点不成立）："
        printf '%s\n' "$mutators" | sed 's/^/     /' >&2
        bad=1
    fi
    [ "$bad" -eq 0 ] && ok "R2 单写入点：64 次读全部取自同一张表（赋值后无任何 mutator）"
    return "$bad"
}

# ── R3：命名真源同源（发射器 ↔ codegen）────────────────────────────────────
r3_same_naming_source() {  # $1 = codegen 源 · $2 = emitter 源（默认仓内真源）
    local cg="${1:-$SRC_CG}" hv="${2:-$SRC_HV}" bad=0
    if ! grep -qE 'build_verilog_name_table\([^;]*get_eval_list\(\)' "$cg"; then
        fail "R3 $cg 未调用 \`build_verilog_name_table(...get_eval_list())\`"
        bad=1
    fi
    if ! grep -qE 'build_verilog_name_table\([^;]*get_eval_list\(\)' "$hv"; then
        fail "R3 $hv 未调用 \`build_verilog_name_table(...get_eval_list())\`"
        bad=1
    fi
    if [ "$bad" -eq 0 ]; then
        ok "R3 命名真源同源：两侧都走 \`ch::build_verilog_name_table(ctx->get_eval_list())\`"
        note "codegen：$(grep -nE 'build_verilog_name_table\(' "$cg" | head -1 | cut -d: -f1): $(grep -nE 'build_verilog_name_table\(' "$cg" | head -1 | cut -d: -f2- | sed 's/^ *//')"
        note "emitter：$(grep -nE 'build_verilog_name_table\(' "$hv" | head -1 | cut -d: -f1): $(grep -nE 'build_verilog_name_table\(' "$hv" | head -1 | cut -d: -f2- | sed 's/^ *//')"
    fi
    return "$bad"
}

# ── 工具：跑既有护栏（**R9 的唯一权威**在 `verify_hierarchical_rtl.sh` 里）──
HIER_RC=0
run_hier_guard() {  # $1 = 日志
    bash "$HIER_GUARD" >"$1" 2>&1
    HIER_RC=$?
}

# ── R4：负向注入（源码级）⇒ R9 必红 ⇒ 还原后必绿 ─────────────────────────
r4_negative_injection() {
    local backup="$WORK/codegen_verilog.cpp.bak" bad=0

    [ -f "$HIER_GUARD" ] || { fail "R4 既有护栏缺失：$HIER_GUARD"; return 1; }
    [ -x "$GENERATOR" ] || {
        fail "R0 产物生成器不可用：$GENERATOR（先 cmake --build $BUILD_DIR --target hier_rtl_products）"
        return 1
    }

    cp "$SRC_CG" "$backup" || { fail "R4 备份失败"; return 1; }
    SRC_BACKUP="$backup"

    # 注入点 MUST 是 62 处之一 ⇒ 先确认锚点行本身命中 `node_names_[`
    if ! grep -qE '^[[:space:]]*std::string lhs_name = node_names_\[lhs_node\];' "$SRC_CG"; then
        fail "R4 注入锚点不存在（\`std::string lhs_name = node_names_[lhs_node];\`）"
        bad=1
    fi
    if grep -qF "$INJECT_MARKER" "$SRC_CG"; then
        fail "R4 源文件里已存在注入标记 \`$INJECT_MARKER\` ⇒ 注入的判别力不成立"
        bad=1
    fi
    [ "$bad" -eq 0 ] || return 1

    # ── 基线：注入前既有护栏 MUST 全绿（否则「转红」无从谈起）───────────
    run_hier_guard "$WORK/hier-base.log"
    if [ "$HIER_RC" -ne 0 ]; then
        fail "R4 基线不绿（既有护栏退出码 $HIER_RC）⇒ 注入前的产物本就不合规 ⇒ 判据不成立："
        tail -20 "$WORK/hier-base.log" >&2
        return 1
    fi
    ok "R4 基线：既有护栏（含 R9）全绿（注入前）"

    # ── 注入 ────────────────────────────────────────────────────────────
    python3 - "$SRC_CG" "$INJECT_ANCHOR_OLD" "$INJECT_ANCHOR_NEW" <<'PY' || { fail "R4 注入失败"; bad=1; }
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(path, encoding="utf-8").read()
n = s.count(old)
if n != 1:
    print("anchor count = %d (expected 1)" % n, file=sys.stderr)
    sys.exit(1)
open(path, "w", encoding="utf-8").write(s.replace(old, new, 1))
PY
    [ "$bad" -eq 0 ] || { cp "$backup" "$SRC_CG"; return 1; }

    # ⚠️ **必须重建**（源码改了 ⇒ 旧二进制跑的是旧代码 ⇒ 假红）
    if ! cmake --build "$REPO_ROOT/$BUILD_DIR" --target hier_rtl_products -j"$JOBS" \
        >"$WORK/build-injected.log" 2>&1; then
        fail "R4 注入后**编译失败** ⇒ 跑的是旧二进制 ⇒ 判据不成立："
        grep -E 'error' "$WORK/build-injected.log" | head -6 >&2
        cp "$backup" "$SRC_CG"
        cmake --build "$REPO_ROOT/$BUILD_DIR" --target hier_rtl_products -j"$JOBS" >/dev/null 2>&1
        return 1
    fi

    run_hier_guard "$WORK/hier-injected.log"
    if [ "$HIER_RC" -eq 0 ]; then
        fail "R4 注入后既有护栏**仍然全绿** ⇒ 「父作用域不可解析」这条判据**无判别力**"
        bad=1
    elif ! grep -q '❌ R9' "$WORK/hier-injected.log"; then
        fail "R4 注入后既有护栏转红，但**不是 R9** ⇒ 归因不符："
        tail -20 "$WORK/hier-injected.log" >&2
        bad=1
    else
        grep -E '^%Error' "$WORK/hier-injected.log" | head -3 >&2
        ok "R4 注入后 R9 必红（父作用域不可解析）✅"
    fi

    # 注入 MUST 真的进了产物（否则红的原因不是注入）
    rm -rf "$WORK/ps_injected" && mkdir -p "$WORK/ps_injected"
    if ! "$GENERATOR" "$WORK/ps_injected" --parent-side-fixtures "$WORK/ps_injected" \
        >"$WORK/gen-injected.log" 2>&1; then
        fail "R4 注入后产物生成失败"
        bad=1
    elif ! grep -qF "$INJECT_MARKER" "$WORK/ps_injected"/*.v; then
        fail "R4 产物里找不到注入标记 ⇒ 转红原因不是本次注入"
        bad=1
    else
        ok "R4 注入标记 \`$INJECT_MARKER\` 确实出现在产物中 ⇒ 转红确由注入引起"
        # 父 module MUST **没有**声明那个带后缀的名字 ⇒ 坐实「父作用域不可解析」
        if awk '/^module top \(/,/^endmodule/' "$WORK/ps_injected/ps_expr_out.v" |
            grep -qE "^    logic .*$INJECT_MARKER"; then
            fail "R4 父 module 竟声明了带后缀的名字 ⇒ 不是「父作用域不可解析」"
            bad=1
        else
            note "父 module 内**无** \`$INJECT_MARKER\` 的 logic 声明 ⇒ 确为父作用域不可解析"
        fi
    fi

    # ── 还原 + 重建 ⇒ 必须复绿 ──────────────────────────────────────────
    cp "$backup" "$SRC_CG" || { fail "R4 还原失败（请手工还原 $SRC_CG）"; return 1; }
    if ! cmp -s "$backup" "$SRC_CG"; then
        fail "R4 还原后与备份不一致"
        bad=1
    fi
    SRC_BACKUP=""
    if ! cmake --build "$REPO_ROOT/$BUILD_DIR" --target hier_rtl_products -j"$JOBS" \
        >"$WORK/build-restored.log" 2>&1; then
        fail "R4 还原后重建失败："
        grep -E 'error' "$WORK/build-restored.log" | head -6 >&2
        bad=1
    fi
    if [ "$bad" -eq 0 ]; then
        run_hier_guard "$WORK/hier-restored.log"
        if [ "$HIER_RC" -ne 0 ]; then
            fail "R4 还原后既有护栏**仍然红** ⇒ 还原未生效或基线本就不绿："
            tail -20 "$WORK/hier-restored.log" >&2
            bad=1
        else
            ok "R4 还原后复绿 ✅（源码已字节级还原 + 已重建）"
        fi
    fi
    return "$bad"
}

# ── R0：前置（fail-closed）────────────────────────────────────────────────
r0_preflight() {
    local bad=0
    [ -f "$SRC_CG" ] || { fail "R0 源文件缺失：$SRC_CG"; bad=1; }
    [ -f "$SRC_HV" ] || { fail "R0 源文件缺失：$SRC_HV"; bad=1; }
    if [ "$DO_INJECT" -eq 1 ]; then
        [ -n "$VERILATOR" ] || {
            fail "R0 verilator 不可用（$BUILD_DIR/verilator-install/bin/verilator 或 PATH）"
            bad=1
        }
        [ -x "$GENERATOR" ] || {
            fail "R0 产物生成器不可用：$GENERATOR（先 cmake --build $BUILD_DIR --target hier_rtl_products）"
            bad=1
        }
        command -v cmake >/dev/null 2>&1 || { fail "R0 cmake 不可用"; bad=1; }
        [ -d "$REPO_ROOT/$BUILD_DIR" ] || { fail "R0 构建目录缺失：$REPO_ROOT/$BUILD_DIR"; bad=1; }
    fi
    return "$bad"
}

# ══════════════════════════════════════════════════════════════════════════
# 自检：R1–R3 的判别力（**只动副本** ⇒ 绝不碰仓内源码）
# ══════════════════════════════════════════════════════════════════════════
if [ "$SELF_TEST" -eq 1 ]; then
    echo "=== verify_node_names_single_source: self-test ==="
    st_fail=0
    ST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/cpphdl_nns_st.XXXXXX")"

    st_check() {  # $1 = 描述, $2 = 期望 exit, $3 = 实际 exit, $4 = 日志
        local desc="$1" want="$2" got="$3" log="$4"
        if [ "$want" = "$got" ]; then
            printf "  \033[0;32mOK\033[0m   %s（exit=%s）\n" "$desc" "$got"
        else
            printf "  \033[0;31mFAIL\033[0m %s（期望 exit=%s，实际 exit=%s）\n" "$desc" "$want" "$got"
            [ -n "$log" ] && [ -f "$log" ] && sed 's/^/        /' "$log" | head -8
            st_fail=$((st_fail + 1))
        fi
    }

    # 合成副本（未改动的源文件）
    mk_fixture() {  # $1 = 目录
        rm -rf "$1" && mkdir -p "$1/src"
        cp "$SRC_CG" "$1/src/codegen_verilog.cpp"
        cp "$SRC_HV" "$1/src/hierarchy_verilog.cpp"
    }
    # 只跑 R1–R3（对副本）⇒ 返回 0 = 全绿
    run_static() {  # $1 = 目录
        local d="$1" rc=0
        r1_grep_manifest "$d/src/codegen_verilog.cpp" >/dev/null 2>&1 || rc=1
        r2_single_write "$d/src/codegen_verilog.cpp" >/dev/null 2>&1 || rc=1
        r3_same_naming_source "$d/src/codegen_verilog.cpp" "$d/src/hierarchy_verilog.cpp" \
            >/dev/null 2>&1 || rc=1
        return "$rc"
    }

    # ① 未改动的副本 ⇒ R1–R3 全绿（**对照**：证明下面的红不是「本来就红」）
    F="$ST_ROOT/fx_ok"; mk_fixture "$F"
    run_static "$F" >/dev/null 2>&1; st_check "未改动的副本 MUST 全绿" 0 "$?" ""

    # ② 分母漂移（多一处 `node_names_[`）⇒ R1 必红
    F="$ST_ROOT/fx_count"; mk_fixture "$F"
    python3 - "$F/src/codegen_verilog.cpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
s = s.replace("    node_names_ = view_.names;",
              "    node_names_ = view_.names;\n    const char* probe = nullptr; (void)node_names_[probe];", 1)
open(p, "w", encoding="utf-8").write(s)
PY
    run_static "$F" >/dev/null 2>&1; st_check "分母漂移 ⇒ R1 必红" 1 "$?" ""

    # ③ 出现**第二个**赋值点 ⇒ R2 必红
    F="$ST_ROOT/fx_write"; mk_fixture "$F"
    python3 - "$F/src/codegen_verilog.cpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
s = s.replace("    node_names_ = view_.names;",
              "    node_names_ = view_.names;\n    node_names_ = view_.names;", 1)
open(p, "w", encoding="utf-8").write(s)
PY
    run_static "$F" >/dev/null 2>&1; st_check "第二个赋值点 ⇒ R2 必红" 1 "$?" ""

    # ④ 赋值之后的**容器级** mutator ⇒ R2 必红
    F="$ST_ROOT/fx_mutate"; mk_fixture "$F"
    python3 - "$F/src/codegen_verilog.cpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
s = s.replace("    node_names_ = view_.names;",
              "    node_names_ = view_.names;\n    node_names_.clear();", 1)
open(p, "w", encoding="utf-8").write(s)
PY
    run_static "$F" >/dev/null 2>&1; st_check "赋值后的容器级 mutator ⇒ R2 必红" 1 "$?" ""

    # ⑤ 发射器**不再**走共享命名表 ⇒ R3 必红
    F="$ST_ROOT/fx_source"; mk_fixture "$F"
    python3 - "$F/src/hierarchy_verilog.cpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
s = s.replace("build_verilog_name_table(root.context()->get_eval_list())",
              "build_private_name_table(root.context()->get_eval_list())", 1)
open(p, "w", encoding="utf-8").write(s)
PY
    run_static "$F" >/dev/null 2>&1; st_check "发射器改用私有命名表 ⇒ R3 必红" 1 "$?" ""

    echo ""
    if [ "$st_fail" -ne 0 ]; then
        echo "❌ self-test FAIL（$st_fail 项）" >&2
        exit 1
    fi
    echo "✅ verify_node_names_single_source --self-test: PASS（5/5 条对照全部生效）"
    exit 0
fi

# ── 主路径 ────────────────────────────────────────────────────────────────
run_all() {
    r0_preflight || return 1
    r1_grep_manifest || return 1
    r2_single_write || return 1
    r3_same_naming_source || return 1
    if [ "$DO_INJECT" -eq 1 ]; then
        r4_negative_injection || return 1
    else
        note "R4（负向注入）已跳过 ⇒ 加 \`--inject\` 跑（会改源码并重建 ⇒ 不在 CI 里跑）"
    fi
    return 0
}

if run_all; then
    if [ "$DO_INJECT" -eq 1 ]; then
        echo "✅ verify_node_names_single_source: PASS（R1–R4）"
    else
        echo "✅ verify_node_names_single_source: PASS（R1–R3）"
    fi
    exit 0
fi
echo "❌ verify_node_names_single_source: FAIL" >&2
exit 1
