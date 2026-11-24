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
#   ⇒ ⇒ 端口名 + 方向 + 宽度 + 顺序**逐字逐序相同**（实测 **53** · S4a 起 +19 根 `axil_*`）
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

# ⚠️ **发射器 = TB 自身**（`--emit-rtl` 模式）⇒ ⇒ 三个薄壳
#   （`emit_rtl.cpp` / `emit_rtl_fw_init.cpp` / `emit_rtl_hier.cpp`）**已删**
#   ⇒ ⇒ 扁平 / 层次化两种形态**共用同一个二进制**，靠**模式标记**区分。
EMITTER_BIN="$(find_emitter tb_rv32i_soc)"
if [ -z "$EMITTER_BIN" ]; then
    fail "缺发射器 tb_rv32i_soc ⇒ 先 cmake --build build --target tb_rv32i_soc"
    exit 2
fi
# ⚠️ **层次化 = 库层 CLI 的既有选项**（`--hierarchical`）⇒ ⇒ 透传给发射器
FLAT_EMITTER="$EMITTER_BIN"
HIER_EMITTER="$EMITTER_BIN"

mkdir -p "$OUT_DIR"
echo "════ RV32I RTL 交付 + Verilator 仿真环境 ════"
echo "  输出目录：$OUT_DIR"
echo "  verilator：$VERILATOR"
echo "  独立 TB ：$TB_REL"
echo

run_one() {  # $1 = 形态（flat|hier）；$2 = 发射器；$3 = 发射器附加参数
    local form="$1" emitter="$2" eargs="${3:-}"
    local work="$OUT_DIR/$form"
    rm -rf "$work"
    mkdir -p "$work"

    info "［$form］① 生成 RTL 产物"
    # ⚠️ **参数不加引号**（要按空格拆成多个 token）⇒ ⇒ 无参时为空
    if ! "$emitter" --emit-rtl $eargs --out "$work" --emit-hygiene >"$work/emit.log" 2>&1; then
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
    run_one flat "$FLAT_EMITTER" "" || RC=1
fi
if [ "$FORM" = "hier" ] || [ "$FORM" = "both" ]; then
    run_one hier "$HIER_EMITTER" "--hierarchical" || RC=1
fi

echo "════ 产物清单 ════"
for form in flat hier; do
    [ -d "$OUT_DIR/$form" ] || continue
    echo "  $OUT_DIR/$form/"
    echo "      top.v          （RTL 产物）"
    echo "      manifest.json  （端口清单 · port_table[] = 产品端口面）"
    echo "      lint.log / compile.log / run.log（证据）"
done

# ── 人工检查摘要（含行号坐标 / 端口全表 / 结构统计）─────────────────────────
# ⚠️ **为什么固化进脚本**：产物目录是**一次性**的（`--out` 换位置即重建）
#   ⇒ ⇒ 摘要若靠人手生成，重建后就丢了 ⇒ ⇒ 而「人工检查」最需要的正是它。
write_summary() {
    python3 - "$OUT_DIR" <<'PY' >"$OUT_DIR/SUMMARY.md"
import json, re, pathlib, sys
root = pathlib.Path(sys.argv[1])
out = ["# RV32I 核 RTL 交付 —— 人工检查摘要\n",
       f"> 生成自 `scripts/rv32i_rtl_sim_env.sh --out {root}`\n"]
for form in ('flat', 'hier'):
    d = root / form
    if not d.is_dir():
        continue
    v = d / 'top.v'
    t = v.read_text(encoding='utf-8')
    man = json.loads((d / 'manifest.json').read_text(encoding='utf-8'))
    lines = t.split('\n')
    out += [f"\n## 形态 `{form}`\n",
            f"- 产物：`{v}`（**{len(lines)} 行** · {len(t)} 字节）",
            f"- manifest：`{d/'manifest.json'}`（schema `{man['schema']}`）",
            f"- `ports[]`（全量，含后代）= **{len(man['ports'])}** · "
            f"`port_table[]`（产品端口面）= **{len(man['port_table'])}**",
            f"- 诊断 = **{len(man['diagnostics'])}** 条",
            "\n### module 清单（含行号）\n", "| 行号 | module |", "|---:|---|"]
    for i, l in enumerate(lines, 1):
        m = re.match(r'^\s*module\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(', l)
        if m:
            out.append(f"| {i} | `{m.group(1)}` |")
    ff = len(re.findall(r'always_ff', t))
    asg = len(re.findall(r'^\s*assign\s', t, re.M))
    ini = len(re.findall(r'^\s*initial\b', t, re.M))
    mem = re.findall(r'logic\s*\[31:0\]\s*(\w*memory\w*)\s*\[([^\]]+)\]', t)
    lo = re.findall(r'/\*\s*verilator\s+lint_off\s+([A-Za-z0-9_]+)\s*\*/', t)
    out += ["\n### 结构统计\n", "| 项 | 值 |", "|---|---:|",
            f"| `always_ff`（寄存器） | {ff} |",
            f"| `assign`（组合） | {asg} |",
            f"| `initial`（初值块） | {ini}（⚠️ 可能为空 = `ch_mem` 初值位，非缺陷） |",
            f"| `default_nettype none` | {'✅ 有' if 'default_nettype none' in t else '❌ 无'} |",
            f"| 末尾 `default_nettype wire` | {'✅ 有' if 'default_nettype wire' in t else '❌ 无'} |",
            f"| blanket `lint_off` | {len(lo)} 处{'（零 ✅）' if not lo else ' ' + str(lo)} |"]
    if mem:
        out.append("| 内存数组 | " + " · ".join(f"`{n} [{r}]`" for n, r in mem) + " |")
    out += [f"\n### 顶层端口表（{len(man['port_table'])} 个 · 逐字逐序 == manifest `port_table[]`）\n",
            "| # | 方向 | 端口 | 位宽 | 角色 |", "|---:|---|---|---:|---|"]
    for i, e in enumerate(man['port_table'], 1):
        out.append(f"| {i} | {e['dir']} | `{e['name']}` | {e['width']} | {e['role']} |")
    out += ["\n### 人工核对点（可直接 grep）\n", "```bash",
            f"grep -n '^module top' {v}                       # 顶层 module 头 + 端口表",
            f"python3 scripts/lib/flat_rtl_emit_check.py --top-ports {v} --module top | wc -l   # 期望 53",
            f"python3 scripts/lib/flat_rtl_emit_check.py --modules {v}                        # module 树",
            f"grep -c 'always_ff' {v}; grep -c '^ *assign ' {v}                              # 规模",
            f"grep -n 'prog_we\\|prog_addr\\|prog_data' {v} | head                            # 装载口（只接 I-TCM）",
            f"grep -n 'lint_off' {v} || echo '零命中 ✅'                                       # 卫生产物",
            "```"]
sys.stdout.write('\n'.join(out) + '\n')
PY
    echo "  $OUT_DIR/SUMMARY.md   （人工检查摘要：module 行号 / 端口全表 / 结构统计 / grep 坐标）"
}
write_summary

if [ "$RC" -ne 0 ]; then
    echo
    echo "❌ rv32i_rtl_sim_env: 失败"
    exit 1
fi
echo
echo "✅ rv32i_rtl_sim_env: PASS"
exit 0
