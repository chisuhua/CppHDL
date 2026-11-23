#!/usr/bin/env bash
# scripts/verify_riscv_soc_core_ports.sh
#
# **核复合体 34 端口契约**护栏（ADR-077 V5 · change `riscv-soc-skeleton-2026-11` AC6）。
#
# ⚠️ **要治的病**：`Rv32iCoreTop` 作为**子 module** 出现在 SoC 产物里 ⇒ 它的端口表
#   **不在** `manifest.port_table[]`（那只覆盖顶层 · ADR-072 D1-c）⇒ ⇒ 仓内**没有**任何
#   判据看过它 ⇒ ⇒ 「核可替换单元」的物证无人核。本护栏补这一格。
#
# 判据：
#   V1  产物里 `Rv32iCoreTop` 的 module 端口表（**名 + 方向 + 位宽**·逐字逐序）
#       == `examples/riscv-mini/AGENTS.md` 的 34 行表（**单一真相源 = 该表**）
#       ⇒ ⇒ 2026-11-23（R3e）：契约表**已归位到核侧** ⇒ ⇒ 契约跟随被描述的对象
#   V2  端口数 == **34** = 框架 2（`default_clock` / `default_reset`）+ 核自有 **32**
#   V3  核自有 32 条（去 `top_core_` 前缀）与 `verify_no_duplicate_rv32i_headers.sh` 的
#       **C6 快照**逐字一致 ⇒ ⇒ 「头文件 → 产物 → 登记表」三方闭环
#   V4  **微架构副本禁入**（ADR-077 D1 的「MUST NOT 复制任何微架构实现」）——
#       `examples/riscv-soc/src/**` 下的头 basename MUST NOT 出现在
#       `include/cpu/**` 或 `examples/riscv-mini/src/**`
#   V5  **两条真兄弟边**（ADR-070 D4 条件③ · change AC8）——
#       产物里 `SocTraceCounter` 实例的两个事件输入 MUST 由 `Rv32iCoreTop` 的
#       输出驱动（**child → child**）⇒ MUST NOT 是顶层 input / 常量 / 未连接
#       ⚠️ 证据形态是**结构化端口连接**（`.top_trace_<port>(top_core_<src>)`），不是散文
#       （⚠️ 这是 ADR-077 R4 缓解的**落点**：把既有护栏的扫描面语义扩到 riscv-soc 侧，
#        而不动那个 470+ 行脚本的 C1/C4 —— 它们的 self-test 期望值不受影响）
#
# 反向判据（`--self-test` 投毒，各自必红）：
#   P1 期望表删一行          ⇒ V1/V2 红
#   P2 期望表改某端口方向     ⇒ V1 红
#   P3 期望表改某端口位宽     ⇒ V1 红
#   P4 期望表改某端口名       ⇒ V1 红
#   P5 期望表的核自有端口与 C6 快照不符 ⇒ V3 红
#   P6 riscv-soc 侧出现与 include/cpu 同 basename 的头 ⇒ V4 红
#   P7 产物里某条兄弟边改接常量            ⇒ V5 红
#
# 退出码：0 = 全成立 · 1 = 违规 · 2 = 用法 / 环境错误（fail-closed）。
#
# 用法：
#   scripts/verify_riscv_soc_core_ports.sh [--self-test]

set -uo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$REPO_ROOT" || exit 2

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
AGENTS_MD="examples/riscv-mini/AGENTS.md"
EMITTER="$BUILD_DIR/examples/riscv-soc/tb_riscv_soc"
HEADER_GUARD="scripts/verify_no_duplicate_rv32i_headers.sh"
CORE_MODULE="Rv32iCoreTop"
PORT_PREFIX="top_core_"

SELF_TEST=0
[ "${1:-}" = "--self-test" ] && SELF_TEST=1

fail_count=0
fail() { echo "❌ $*" >&2; fail_count=$((fail_count + 1)); }
ok()   { echo "✅ $*"; }

TMPROOT="${TMPDIR:-/tmp/opencode}"
mkdir -p "$TMPROOT" || exit 2

# ── 解析：AGENTS.md 的 34 行表 → `name:dir:width`（按表中出现序）──
parse_agents_table() {  # $1 = agents md path
    awk -F'|' '
        /^\| *[0-9]+ *\|/ {
            name=$3; dir=$4; w=$5;
            gsub(/^[ \t]+|[ \t]+$/, "", name);
            gsub(/`/, "", name);
            gsub(/^[ \t]+|[ \t]+$/, "", dir);
            gsub(/^[ \t]+|[ \t]+$/, "", w);
            if (name != "" && dir != "") print name ":" dir ":" w;
        }' "$1"
}

# ── 解析：产物里某个 module 的端口表 → `name:dir:width` ──
parse_product_module() {  # $1 = verilog path · $2 = module name
    awk -v mod="$2" '
        $0 ~ "^module " mod " ?\\(" { inmod = 1; next }
        inmod && /^\);/ { exit }
        inmod {
            line = $0;
            sub(/,.*$/, "", line);
            dir = ""; width = 1; name = "";
            if (match(line, /^[ \t]*input/))  dir = "INPUT";
            if (match(line, /^[ \t]*output/)) dir = "OUTPUT";
            if (dir == "") next;
            if (match(line, /\[[0-9]+:0\]/)) {
                rng = substr(line, RSTART + 1, RLENGTH - 2);
                split(rng, parts, ":");
                width = parts[1] + 1;
            }
            sub(/^[ \t]*(input|output)[ \t]*/, "", line);
            sub(/\[[0-9]+:0\][ \t]*/, "", line);
            gsub(/[ \t]/, "", line);
            name = line;
            if (name != "") print name ":" dir ":" width;
        }' "$1"
}

# ── V1/V2：期望表 vs 产物（逐字·有序）──
compare_tables() {  # $1 = expected file · $2 = actual file · 返回 diff 行数
    if diff -u "$1" "$2" >/dev/null 2>&1; then echo 0; else diff -u "$1" "$2" | wc -l; fi
}

# ── V3：核自有 32 条 vs C6 快照 ──
c6_snapshot() {
    sed -n 's/^SOC_TOP_PORTS="\(.*\)"$/\1/p' "$HEADER_GUARD" | tr ' ' '\n' | awk -F: '
        NF == 3 {
            dir = ($2 == "ch_in") ? "INPUT" : "OUTPUT";
            w = 1;
            if (match($3, /<[0-9]+>/)) { w = substr($3, RSTART + 1, RLENGTH - 2); }
            print "top_core_" $1 ":" dir ":" w;
        }' | sort
}

# ── V4：riscv-soc 侧不得有微架构头的同名副本 ──
check_no_core_copy() {  # $1 = repo root
    local root="$1" h base bad=0 found=""
    [ -d "$root/examples/riscv-soc/src" ] || return 0
    while IFS= read -r h; do
        base="$(basename "$h")"
        # ⚠️ **不得**写 `find ... | grep -q .` —— `set -o pipefail` 下 `grep -q` 早退会让
        #    `find` 收到 SIGPIPE ⇒ 管道状态非零 ⇒ 判据**恒绿**（实测：self-test 的 P6 抓到的）
        found="$(find "$root/include/cpu" "$root/examples/riscv-mini/src" -name "$base" -print -quit 2>/dev/null)"
        if [ -n "$found" ]; then
            fail "V4 微架构头的同名副本出现在 riscv-soc 侧：$base（已在 $found）"
            bad=1
        fi
    done < <(find "$root/examples/riscv-soc/src" -type f -name '*.h' 2>/dev/null)
    if [ "$bad" -eq 0 ]; then
        ok "V4 riscv-soc 侧无微架构头同名副本（ADR-077 D1）"
    fi
}

# ── V5：两条真兄弟边（child → child · ADR-070 D4③）──
check_sibling_edges() {  # $1 = product verilog
    local prod="$1" n=0 port src
    [ -f "$prod" ] || { fail "V5 产物缺失：$prod"; return; }
    for pair in "store_event:top_core_obs_data_write_en" \
                "retire_event:top_core_dbg_wb_write_en"; do
        port="${pair%%:*}"
        src="${pair##*:}"
        # ⚠️ 不用管道（`set -o pipefail` + grep -q 早退会让状态失真 —— V4 踩过）
        if grep -qE "\.top_trace_${port}\( *${src} *\)" "$prod"; then
            n=$((n + 1))
        else
            fail "V5 兄弟边缺失/被改接：SocTraceCounter.${port} 未由 ${src} 驱动"
        fi
    done
    if [ "$n" -eq 2 ]; then
        ok "V5 两条兄弟边成立（child→child · ADR-070 D4③ · AC8）"
    fi
}

run_checks() {  # $1 = agents md · $2 = product verilog
    local agents="$1" prod="$2"

    if [ ! -f "$prod" ]; then
        fail "V1 产物缺失：$prod"
        return
    fi

    local exp act
    exp="$TMPROOT/.soc_core_expected.txt"
    act="$TMPROOT/.soc_core_actual.txt"
    parse_agents_table "$agents" > "$exp"
    parse_product_module "$prod" "$CORE_MODULE" > "$act"

    local n_exp n_act
    n_exp=$(grep -c . "$exp" || true)
    n_act=$(grep -c . "$act" || true)

    # V2：条数
    if [ "$n_exp" -ne 34 ]; then
        fail "V2 期望表条数 = $n_exp ≠ 34（AGENTS.md 的表被改动？）"
    fi
    if [ "$n_act" -ne 34 ]; then
        fail "V2 产物里 $CORE_MODULE 的端口数 = $n_act ≠ 34"
    fi

    # V1：逐字逐序
    local dl
    dl=$(compare_tables "$exp" "$act")
    if [ "$dl" -ne 0 ]; then
        fail "V1 端口表不等（diff $dl 行）："
        diff -u "$exp" "$act" | sed -n '3,20p' >&2
    else
        ok "V1 $CORE_MODULE 端口表 == AGENTS.md 的 34 行（逐字逐序）"
    fi

    # V3：核自有 32 条 vs C6 快照
    local own c6
    own="$TMPROOT/.soc_core_own.txt"
    c6="$TMPROOT/.soc_core_c6.txt"
    grep "^${PORT_PREFIX}" "$exp" | sort > "$own"
    c6_snapshot > "$c6"
    if [ "$(grep -c . "$own" || true)" -ne 32 ]; then
        fail "V3 期望表里核自有端口 = $(grep -c . "$own" || true) ≠ 32"
    elif diff -q "$own" "$c6" >/dev/null 2>&1; then
        ok "V3 核自有 32 条与 C6 快照逐字一致（头文件 → 产物 → 登记表 闭环）"
    else
        fail "V3 核自有端口与 C6 快照不一致："
        diff -u "$c6" "$own" | sed -n '3,20p' >&2
    fi

    if [ "$fail_count" -eq 0 ]; then
        ok "V2 端口数 = 34（框架 2 + 核自有 32）"
    fi
}

# ── 发射产物（fail-closed：发射器缺失 ⇒ 用法错误）──
emit_products() {  # $1 = out dir
    if [ ! -x "$EMITTER" ]; then
        echo "FATAL: 发射器缺失：$EMITTER（先 cmake --build $BUILD_DIR --target tb_riscv_soc）" >&2
        return 2
    fi
    mkdir -p "$1" || return 2
    "$EMITTER" --emit-rtl --hierarchical --out "$1" >"$1/emit.log" 2>&1 || {
        echo "FATAL: 发射失败（见 $1/emit.log）" >&2
        return 2
    }
    [ -f "$1/top.v" ] || { echo "FATAL: 未产出 $1/top.v" >&2; return 2; }
    return 0
}

if [ "$SELF_TEST" -eq 1 ]; then
    OUT="$TMPROOT/.soc_core_selftest"
    rm -rf "$OUT"; mkdir -p "$OUT"
    emit_products "$OUT/emit" || exit 2
    PROD="$OUT/emit/top.v"

    echo "── 基线（期望 PASS）──"
    fail_count=0
    run_checks "$AGENTS_MD" "$PROD"
    check_no_core_copy "$REPO_ROOT"
    check_sibling_edges "$PROD"
    [ "$fail_count" -eq 0 ] || { echo "FATAL: 基线不通过 ⇒ self-test 无意义" >&2; exit 1; }

    # 投毒模板
    BASE_EXP="$OUT/base_exp.txt"
    parse_agents_table "$AGENTS_MD" > "$BASE_EXP"

    poison_case() {  # $1 = 名称 · $2 = 转换脚本(stdin→stdout) · $3 = 期望命中的判据
        local name="$1" xform="$2" want="$3"
        local pf="$OUT/poison.md"
        local mutated="$OUT/poison_exp.txt"
        eval "$xform" < "$BASE_EXP" > "$mutated"
        # 把变异后的期望表写成一个可被 parse_agents_table 读的 md（同名表格形态）
        {
            echo "| # | 端口（产物名） | 方向 | 位宽 | 归属 |"
            echo "|---|---|---|---|---|"
            awk -F: '{ printf "| %d | `%s` | %s | %s | 核自有 |\n", NR, $1, $2, $3 }' "$mutated"
        } > "$pf"

        local before=$fail_count
        fail_count=0
        run_checks "$pf" "$PROD" >/dev/null 2>&1
        local delta=$fail_count
        fail_count=$before
        if [ "$delta" -gt 0 ]; then
            ok "投毒 $name ⇒ 必红（命中 $want）"
        else
            fail "投毒 $name ⇒ **未转红**（判据失效）"
        fi
    }

    echo "── 投毒（各自必红）──"
    poison_case "P1 删一行"            'sed "3d"'                     "V1/V2"
    poison_case "P2 改方向"            'awk -F: "NR==5{\$2=\"OUTPUT\"}1" OFS=:' "V1"
    poison_case "P3 改位宽"            'awk -F: "NR==7{\$3=\$3+1}1" OFS=:'  "V1"
    poison_case "P4 改端口名"          'awk -F: "NR==9{\$1=\$1\"_x\"}1" OFS=:' "V1"
    # P5：破坏 V3 —— 把某个核自有的端口名改成不在 C6 里的名字（条数仍 34）
    poison_case "P5 核自有端口与 C6 不符" 'awk -F: "NR==6{\$1=\"not_in_c6\"}1" OFS=:' "V3"

    # P6：在**临时布局**里放一个与 include/cpu 同名的头 ⇒ V4 必红
    p6="$OUT/p6"
    mkdir -p "$p6/include/cpu" "$p6/examples/riscv-soc/src"
    : > "$p6/include/cpu/hazard_unit.h"
    : > "$p6/examples/riscv-soc/src/hazard_unit.h"
    before6=$fail_count
    fail_count=0
    check_no_core_copy "$p6" >/dev/null 2>&1
    delta6=$fail_count
    fail_count=$before6
    if [ "$delta6" -gt 0 ]; then
        ok "投毒 P6 riscv-soc 侧出现同名微架构头 ⇒ 必红（命中 V4）"
    else
        fail "投毒 P6 ⇒ **未转红**（V4 判据失效）"
    fi

    # P7：产物里把一条兄弟边改接常量 ⇒ V5 必红
    p7="$OUT/p7.v"
    sed 's/\.top_trace_store_event(top_core_obs_data_write_en)/.top_trace_store_event(1'"'"'b0)/' "$PROD" > "$p7"
    before7=$fail_count
    fail_count=0
    check_sibling_edges "$p7" >/dev/null 2>&1
    delta7=$fail_count
    fail_count=$before7
    if [ "$delta7" -gt 0 ]; then
        ok "投毒 P7 兄弟边改接常量 ⇒ 必红（命中 V5）"
    else
        fail "投毒 P7 ⇒ **未转红**（V5 判据失效）"
    fi

    echo
    if [ "$fail_count" -eq 0 ]; then
        echo "✅ verify_riscv_soc_core_ports --self-test: PASS（7 条投毒各自必红）"
        exit 0
    else
        echo "❌ verify_riscv_soc_core_ports --self-test: FAIL（$fail_count 项）" >&2
        exit 1
    fi
fi

# ── 正常模式 ──
OUT="$TMPROOT/.soc_core_run"
rm -rf "$OUT"; mkdir -p "$OUT"
emit_products "$OUT/emit" || exit 2
run_checks "$AGENTS_MD" "$OUT/emit/top.v"
check_no_core_copy "$REPO_ROOT"
check_sibling_edges "$OUT/emit/top.v"

echo
if [ "$fail_count" -eq 0 ]; then
    echo "✅ verify_riscv_soc_core_ports: PASS（34 端口契约 + 无微架构副本 + 两条兄弟边）"
    exit 0
else
    echo "❌ verify_riscv_soc_core_ports: FAIL（$fail_count 项）" >&2
    exit 1
fi
