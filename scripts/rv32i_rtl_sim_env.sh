#!/usr/bin/env bash
# ============================================================================
# scripts/rv32i_rtl_sim_env.sh
#
# **RV32I 核的 RTL 交付 + Verilator 仿真环境**（一条命令跑通）。
#
# ⚠️ **它做什么**（四步 · 每步都有产物证据）：
#   ① **生成 RTL 产物**（扁平 / 层次化 / 两者）⇒ 每种形态一个目录
#      （`top.v` + `manifest.json` ⇒ manifest 的 `port_table[]` 是产品端口面的唯一权威）
#   ② **lint**（`verilator --lint-only --top-module top`）⇒ ⇒ 退出码 0 + 零 `%Error`
#   ③ **编译**独立 Verilog TB（`verilator --binary --timing`）
#   ④ **运行** ⇒ ⇒ TB 收尾 MUST 打印 `RESULT: checks=<N> fails=0`
#
# ⚠️ **为什么两种形态能共用同一个 TB**：
#   扁平与层次化产物的**顶层端口表同源**（都走 `make_flat_module_view`）
#   ⇒ ⇒ 端口名 + 方向 + 宽度 + 顺序**逐字逐序相同**（实测 34）
#   ⇒ ⇒ 顶层 module 名同为 `top` ⇒ ⇒ **同一个 TB 连线逐字相同** ⇒ ⇒
#   ⇒ ⇒ 跑出**同一组判据** ⇒ ⇒ 「层次化 ≡ 扁平」在**产品路径**上得到证据
#   （不只在 `tests/test_hierarchy_equivalence.cpp` 的用例路径上）。
#
# ⚠️ **fail-closed**（缺工具 / 缺产物 / 编译失败 / 断言失败 ⇒ ⇒ 非零退出）：
#   · 缺 verilator ⇒ exit 2
#   · 缺发射器（未构建）⇒ exit 2
#   · 发射失败 ⇒ exit 1
#   · lint 非 0 或含 `%Error` ⇒ exit 1
#   · TB 编译失败 / 跑挂 / 断言失败 ⇒ exit 1
#
# 用法：
#   scripts/rv32i_rtl_sim_env.sh                     # 两种形态都跑（默认）
#   scripts/rv32i_rtl_sim_env.sh --form hier         # 只跑层次化
#   scripts/rv32i_rtl_sim_env.sh --out /tmp/my-rtl   # 换输出目录
#   scripts/rv32i_rtl_sim_env.sh --tb path/to/my.sv  # 换独立 TB
# ============================================================================
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
OUT_DIR="$BUILD_DIR/rv32i-rtl-sim"
FORM="both"
TB_REL="examples/riscv-mini/tb_rv32i_soc.sv"
TB_MODULE="tb_rv32i_soc"

usage() {
    sed -n '3,40p' "$0"
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT_DIR="$2"; shift 2 ;;
        --form) FORM="$2"; shift 2 ;;
        --tb) TB_REL="$2"; shift 2 ;;
        -h | --help) usage ;;
        *) echo "::error::未知选项：$1" >&2; exit 2 ;;
    esac
done

case "$FORM" in
    flat | hier | both) ;;
    *) echo "::error::--form 只允许 flat | hier | both" >&2; exit 2 ;;
esac

fail() { echo "❌ $1" >&2; }
info() { echo "── $1"; }
ok()   { echo "✅ $1"; }

# ── R0：fail-closed（verilator / 发射器 / TB / 分析器都必须可用）───────────
VERILATOR=""
for cand in "$BUILD_DIR/verilator-install/bin/verilator" "$(command -v verilator || true)"; do
    [ -n "$cand" ] && [ -x "$cand" ] && { VERILATOR="$cand"; break; }
done
[ -n "$VERILATOR" ] || {
    fail "找不到 verilator（$BUILD_DIR/verilator-install/bin/verilator 或 PATH）⇒ fail-closed"
    exit 2
}

CHECKER="$REPO_ROOT/scripts/lib/flat_rtl_emit_check.py"
[ -f "$CHECKER" ] || { fail "缺分析器：$CHECKER ⇒ fail-closed"; exit 2; }

TB_PATH="$REPO_ROOT/$TB_REL"
[ -f "$TB_PATH" ] || { fail "缺独立 TB：$TB_REL ⇒ fail-closed"; exit 2; }

find_emitter() {  # $1 = target 名
    find "$BUILD_DIR" -type f -name "$1" -perm -u+x 2>/dev/null | sort | head -1
}

FLAT_EMITTER="$(find_emitter emit_rtl_rv32i_soc)"
HIER_EMITTER="$(find_emitter emit_rtl_rv32i_soc_hier)"
if [ "$FORM" != "hier" ] && [ -z "$FLAT_EMITTER" ]; then
    fail "缺扁平发射器 emit_rtl_rv32i_soc ⇒ 先 cmake --build build --target emit_rtl_rv32i_soc"
    exit 2
fi
if [ "$FORM" != "flat" ] && [ -z "$HIER_EMITTER" ]; then
    fail "缺层次化发射器 emit_rtl_rv32i_soc_hier ⇒ 先 cmake --build build --target emit_rtl_rv32i_soc_hier"
    exit 2
fi

mkdir -p "$OUT_DIR"
echo "════ RV32I RTL 交付 + Verilator 仿真环境 ════"
echo "  输出目录：$OUT_DIR"
echo "  verilator：$VERILATOR"
echo "  独立 TB ：$TB_REL"
echo

run_one() {  # $1 = 形态（flat|hier）；$2 = 发射器
    local form="$1" emitter="$2"
    local work="$OUT_DIR/$form"
    rm -rf "$work"
    mkdir -p "$work"

    info "［$form］① 生成 RTL 产物"
    if ! "$emitter" --out "$work" --emit-hygiene >"$work/emit.log" 2>&1; then
        fail "[$form] 发射失败（见 $work/emit.log）"; tail -5 "$work/emit.log" >&2; return 1
    fi
    local dut_v="$work/top.v"
    local man="$work/manifest.json"
    [ -f "$dut_v" ] && [ -f "$man" ] || { fail "[$form] 产物缺失"; return 1; }

    local mods n_mods
    mods="$(python3 "$CHECKER" --modules "$dut_v")"
    n_mods="$(printf '%s\n' "$mods" | grep -c '[A-Za-z_]' || true)"
    local n_ports
    n_ports="$(python3 "$CHECKER" --top-ports "$dut_v" --module top | grep -c . || true)"
    local n_pt
    n_pt="$(python3 -c "import json,sys;print(len(json.load(open(sys.argv[1]))['port_table']))" "$man")"
    echo "   module 数：$n_mods（$(printf '%s' "$mods" | tr '\n' ' '))"
    echo "   顶层端口：$n_ports（manifest port_table[] = $n_pt）"
    if [ "$n_ports" -ne "$n_pt" ]; then
        fail "[$form] 产物端口表（$n_ports）≠ manifest port_table[]（$n_pt）"; return 1
    fi

    info "［$form］② lint（--lint-only --top-module top）"
    if ! "$VERILATOR" --lint-only --top-module top "$dut_v" >"$work/lint.log" 2>&1; then
        fail "[$form] lint 非零退出（见 $work/lint.log）"; return 1
    fi
    if grep -q '%Error' "$work/lint.log"; then
        fail "[$form] lint 输出含 %Error（见 $work/lint.log）"; return 1
    fi
    ok "[$form] lint 干净（退出码 0 · 零 %Error）"

    info "［$form］③ 编译独立 TB"
    local flags="--binary --timing -CFLAGS -fcoroutines --x-assign unique"
    if ! "$VERILATOR" $flags --top-module "$TB_MODULE" \
            -Mdir "$work/obj" "$TB_PATH" "$dut_v" -o sim >"$work/compile.log" 2>&1; then
        fail "[$form] TB 编译失败（见 $work/compile.log）"; tail -5 "$work/compile.log" >&2; return 1
    fi

    info "［$form］④ 运行"
    if ! timeout 300 "$work/obj/sim" >"$work/run.log" 2>&1; then
        fail "[$form] 仿真失败或超时（见 $work/run.log）"; return 1
    fi
    local line
    line="$(grep -Eo 'RESULT: checks=[0-9]+ fails=[0-9]+' "$work/run.log" | tail -1)"
    [ -n "$line" ] || { fail "[$form] 输出里没有 'RESULT: checks=<N> fails=0' 行"; return 1; }
    case "$line" in
        *"fails=0") ok "[$form] $line" ;;
        *) fail "[$form] $line"; return 1 ;;
    esac
    echo
    return 0
}

RC=0
if [ "$FORM" = "flat" ] || [ "$FORM" = "both" ]; then
    run_one flat "$FLAT_EMITTER" || RC=1
fi
if [ "$FORM" = "hier" ] || [ "$FORM" = "both" ]; then
    run_one hier "$HIER_EMITTER" || RC=1
fi

echo "════ 产物清单 ════"
for form in flat hier; do
    [ -d "$OUT_DIR/$form" ] || continue
    echo "  $OUT_DIR/$form/"
    echo "      top.v          （RTL 产物）"
    echo "      manifest.json  （端口清单 · port_table[] = 产品端口面）"
    echo "      lint.log / compile.log / run.log（证据）"
done

if [ "$RC" -ne 0 ]; then
    echo
    echo "❌ rv32i_rtl_sim_env: 失败"
    exit 1
fi
echo
echo "✅ rv32i_rtl_sim_env: PASS"
exit 0
