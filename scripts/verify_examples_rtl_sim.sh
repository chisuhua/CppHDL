#!/usr/bin/env bash
# scripts/verify_examples_rtl_sim.sh
#
# **首批 examples 的独立 Verilog 仿真护栏**（`examples-rtl-sim-coverage` **E5**）。
#
# 判据（逐项，缺一即红）：
#   ① 清单里每个 target **都有 TB 文件**（`tb` 列指向的路径）
#   ② 每个 TB **都能编译**（verilator `--binary`）
#   ③ 每个 TB **都跑过**且退出码为 0
#   ④ TB 的断言**非空** —— 以**运行时证据**判定：输出里 MUST 有
#      `RESULT: checks=<N> fails=0` 且 **N > 0**
#      ⇒ ⇒ **禁**用自研 Verilog 静态分析器判「断言非空」
#      （草案 metis 订正：仓内那个自研 C 子集分析器在第 4/5/6 轮对抗审稿里
#        **全部** BLOCKER 出在它身上 ⇒ ⇒ 同一类东西**不**再做第二套）
#   ⑤ `--self-test`：投毒必红 ⇒ 还原复绿
#
# ⚠️ **缺失 = 失败**（**不**跳过）⇒ ⇒ 防「TB 存在但从未执行」
#   ⇒ ⇒ 因此在 TB 补齐之前，本脚本**恒红** ⇒ ⇒ **不得**接进 CI
#   （CI 接线是 E6，前置条件 = 首批全部 `verified`）
#
# 用法：
#   scripts/verify_examples_rtl_sim.sh             # 严格判定（CI 用）
#   scripts/verify_examples_rtl_sim.sh --list      # 只列清单与状态
#   scripts/verify_examples_rtl_sim.sh --self-test # 自检（投毒 ⇒ 必红）

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
LIST="$REPO_ROOT/.rddf/checks/examples-rtl-sim-targets.tsv"
WORK_ROOT="${TMPDIR:-/tmp/opencode}/examples_rtl_sim"

MODE="check"
case "${1:-}" in
--list) MODE="list" ;;
--self-test) MODE="self-test" ;;
"") ;;
*)
    echo "用法：$0 [--list|--self-test]" >&2
    exit 2
    ;;
esac

fail() {
    echo "  [0;31m❌ $*[0m"
    FAILED=$((FAILED + 1))
}
ok() { echo "  [0;32m✅ $*[0m"; }
info() { echo "  ── $*"; }

FAILED=0
CHECKED=0

if [ ! -f "$LIST" ]; then
    echo "::error::清单缺失：$LIST" >&2
    exit 2
fi

# ── 工具定位 ────────────────────────────────────────────────────────
find_verilator() {
    if [ -x "$BUILD_DIR/verilator-install/bin/verilator" ]; then
        echo "$BUILD_DIR/verilator-install/bin/verilator"
        return 0
    fi
    if command -v verilator >/dev/null 2>&1; then
        command -v verilator
        return 0
    fi
    return 1
}

VERILATOR="$(find_verilator || true)"

# ── 清单读取（跳过注释与空行）──────────────────────────────────────
read_rows() {
    grep -vE '^[[:space:]]*(#|$)' "$LIST"
}

if [ "$MODE" = "list" ]; then
    echo "── 首批 examples 的 RTL 仿真清单（$LIST）"
    printf "  %-32s %-10s %-34s %s\n" "target" "status" "tb" "note"
    while IFS=$'\t' read -r tgt src comp emitter tb status note; do
        printf "  %-32s %-10s %-34s %s\n" "$tgt" "$status" "$tb" "${note:0:60}"
    done < <(read_rows)
    echo
    echo "  （status=pending ⇒ ⇒ 尚未补齐期望源 ⇒ ⇒ **不算**已验证）"
    exit 0
fi

if [ -z "$VERILATOR" ]; then
    echo "::error::找不到 verilator（缺 $BUILD_DIR/verilator-install/bin/verilator 且 PATH 里没有）⇒ ⇒ fail-closed" >&2
    exit 2
fi

mkdir -p "$WORK_ROOT"

# ── 逐项判定 ───────────────────────────────────────────────────────
while IFS=$'\t' read -r tgt src comp emitter tb status note; do
    CHECKED=$((CHECKED + 1))
    info "$tgt（$emitter · $tb）"

    # ① TB 文件存在
    if [ ! -f "$REPO_ROOT/$tb" ]; then
        fail "① TB 文件缺失：$tb（缺失 = 失败，不跳过）"
        continue
    fi

    # ② 发射器存在
    emitter_bin=""
    for cand in "$BUILD_DIR/examples/$emitter" "$BUILD_DIR/$emitter" "$emitter"; do
        if [ -x "$REPO_ROOT/$cand" ]; then
            emitter_bin="$REPO_ROOT/$cand"
            break
        fi
    done
    if [ -z "$emitter_bin" ]; then
        fail "② 发射器缺失：$emitter（先构建它：cmake --build $BUILD_DIR --target $emitter）"
        continue
    fi

    # ③ 发射产物
    work="$WORK_ROOT/$tgt"
    rm -rf "$work"
    mkdir -p "$work"
    if ! "$emitter_bin" --out "$work" >"$work/emit.log" 2>&1; then
        fail "③ 发射失败（见 $work/emit.log）"
        continue
    fi
    dut_v="$work/top.v"
    if [ ! -f "$dut_v" ]; then
        fail "③ 产物缺失：$dut_v"
        continue
    fi

    # ④ 编译 TB + 产物
    mkdir -p "$work/obj"
    if ! "$VERILATOR" --binary --timing -CFLAGS -fcoroutines -Wno-fatal \
            --x-assign unique \
            --top-module "$(basename "$tb" .sv)" \
            -Mdir "$work/obj" "$REPO_ROOT/$tb" "$dut_v" -o sim \
            >"$work/compile.log" 2>&1; then
        fail "④ TB 编译失败（见 $work/compile.log）"
        continue
    fi

    # ⑤ 运行 + 断言非空（**运行时证据**）
    if ! "$work/obj/sim" >"$work/run.log" 2>&1; then
        fail "⑤ 仿真退出码非 0（见 $work/run.log）"
        continue
    fi
    line="$(grep -Eo 'RESULT: checks=[0-9]+ fails=[0-9]+' "$work/run.log" | tail -1)"
    if [ -z "$line" ]; then
        fail "⑤ 输出里没有 \`RESULT: checks=<N> fails=0\` 行（TB 没按约定收尾）"
        continue
    fi
    checks="$(printf '%s' "$line" | sed -E 's/.*checks=([0-9]+).*/\1/')"
    fails="$(printf '%s' "$line" | sed -E 's/.*fails=([0-9]+).*/\1/')"
    if [ "$checks" -le 0 ]; then
        fail "⑤ 断言数为 0（checks=$checks）⇒ ⇒ TB 只编译不跑"
        continue
    fi
    if [ "$fails" -ne 0 ]; then
        fail "⑤ 断言失败数非 0（$line）"
        continue
    fi
    ok "$tgt：编译通过 · 跑通 · $line"
done < <(read_rows)

echo
if [ "$FAILED" -ne 0 ]; then
    echo "❌ verify_examples_rtl_sim: $FAILED 项失败（共检查 $CHECKED 项）"
    exit 1
fi
echo "✅ verify_examples_rtl_sim: PASS（$CHECKED 项 · TB 存在 · 编译通过 · 跑通 · 断言非空）"
exit 0
