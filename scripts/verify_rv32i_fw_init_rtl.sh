#!/usr/bin/env bash
# scripts/verify_rv32i_fw_init_rtl.sh
#
# **RV32I 固件「产物自带初值」**的判据（`firmware-load-chain` **L2 = 路线 A** / **L6**）
#
# 判据（逐条可复跑 · 失败即 exit 1）：
#   F0 两个发射器存在（缺 ⇒ **fail-closed**，不是 skip）
#   F1 两次发射都成功，产物 `top.v` 存在
#   F2 **零端口变更** —— 带初值产物与基线产物的**顶层端口表逐字相同**
#      ⇒ ⇒ 这条钉住 L2 的核心承诺：「装载不进端口面」
#   F3 **正向内容** —— 带初值产物的 `initial` 块携带**手推**的固件字：
#      · I-TCM：`[0]=0x10002083`（`lw x1,0x100(x0)`）· `[1]=0x00108113`（`addi x2,x1,1`）
#        · `[2]=0x10202223`（`sw x2,0x104(x0)`）· `[3]=0x0000006f`（`jal x0,0`）
#      · D-TCM：`[64]=0x5a`（`.data` 的 `.word 0x5A` ⇒ 字节地址 0x100 ÷ 4）
#   F4 **反向内容** —— 基线产物（不带初值）的 `initial` 块**一条赋值都没有**
#      ⇒ ⇒ 证明 F3 的内容**只能**来自 `ch_mem::init_data`（不是发射器里写死的常量）
#   F5 **确定性** —— 带初值产物连发两次**逐字节相同**
#   F6 `--self-test`：把 F3 的正反两面**互换**喂给判定函数 ⇒ ⇒ **各自必红**
#      （只查「有值」不够：常量写死也能绿；必须证明**判据对内容敏感**）
#
# ⚠️ **为什么是产物级而不是 C++ 级**：C++ 侧的 `init_words_` 只证明「构造时传了参数」
#   ⇒ ⇒ 它到底落成了 Verilog `initial`（下游能用的形态）还是被 codegen 吞掉，
#   **只有产物能回答**。
#   ⇒ ⇒ 行为面的证据在 `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`
#   的 `FirmwareInitDataLoadedFromConstruction`（**零 `prog_*` 写** + verilator 后端真跑）。
#
# ⚠️ **期望值全部手推**（禁抄发射器/仿真输出）：见 `examples/riscv-mini/src/rv32i_fw_demo.h`
#   的指令表与本文件的 F3 注释。
#
# ⚠️ **本护栏不需要 verilator / 网络** ⇒ ⇒ 主 job（BUILD_VERILATOR=OFF）可跑。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 2

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
WORK="${WORK:-/tmp/opencode/rv32i_fw_init_rtl}"
EMIT_PLAIN="$BUILD_DIR/examples/riscv-mini/emit_rtl_rv32i_soc"
EMIT_INIT="$BUILD_DIR/examples/riscv-mini/emit_rtl_rv32i_soc_fw_init"

red() { printf '\033[0;31m%s\033[0m\n' "$*"; }
green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
fail() { red "  ❌ $*"; FAILED=1; }
ok() { green "  ✅ $*"; }

MODE="check"
for arg in "$@"; do
    case "$arg" in
    --self-test) MODE="self-test" ;;
    -h | --help)
        sed -n '3,40p' "$0"
        exit 0
        ;;
    *)
        echo "用法：$0 [--self-test]" >&2
        exit 2
        ;;
    esac
done

FAILED=0

echo "── F0：发射器存在（fail-closed）"
for bin in "$EMIT_PLAIN" "$EMIT_INIT"; do
    if [ ! -x "$bin" ]; then
        red "❌ F0 发射器缺失：$bin"
        echo "     先构建：cmake --build $BUILD_DIR --target emit_rtl_rv32i_soc emit_rtl_rv32i_soc_fw_init" >&2
        exit 2
    fi
done
ok "F0 两个发射器存在"

rm -rf "$WORK"
mkdir -p "$WORK/plain" "$WORK/init" "$WORK/init2"

echo "── F1：发射（基线 / 带初值 / 带初值复跑）"
for pair in "plain:$EMIT_PLAIN:$WORK/plain" "init:$EMIT_INIT:$WORK/init" \
    "init2:$EMIT_INIT:$WORK/init2"; do
    IFS=: read -r _tag bin out <<<"$pair"
    if ! "$bin" --out "$out" >"$out/emit.log" 2>&1; then
        fail "F1 发射失败（$bin，见 $out/emit.log）"
        exit 1
    fi
    [ -f "$out/top.v" ] || {
        fail "F1 产物缺失：$out/top.v"
        exit 1
    }
done
ok "F1 三份产物均生成"

# ── 产物解析（端口表 + 内存 initial 赋值）─────────────────────────────
# 输出：第一行 `PORTS <n>`，随后每行 `PORT <decl>`；
#       再每行 `MEM <mem_name> <idx> <value_hex>`
parse_product() {
    python3 - "$1" <<'PY'
import re
import sys

src = open(sys.argv[1], encoding="utf-8").read()

# ── 顶层端口表（module top (...) 内按出现顺序）──
m = re.search(r"^module top\s*\((.*?)^\);", src, re.S | re.M)
if not m:
    print("ERROR no module top")
    sys.exit(3)
ports = re.findall(r"\b(input|output)\s+(?:\[[^\]]*\]\s*)?([A-Za-z_][\w$]*)",
                   m.group(1))
print("PORTS %d" % len(ports))
for kind, name in ports:
    print("PORT %s %s" % (kind, name))

# ── 内存 initial 赋值（只收 `mem[idx] = 32'h...;` 形态）──
# ⚠️ 必须**逐个 initial 块**处理：只在 `initial begin … end` 里的赋值才算
for blk in re.finditer(r"initial begin(.*?)^\s*end\s*$", src, re.S | re.M):
    for am in re.finditer(
            r"^\s*([A-Za-z_][\w$]*)\[(\d+)\]\s*=\s*32'h([0-9a-fA-F]+);",
            blk.group(1), re.M):
        print("MEM %s %s %s" % (am.group(1), am.group(2),
                                am.group(3).lower()))
PY
}

# 手推期望（**字节地址 ÷ 4 = 字索引**；见 rv32i_fw_demo.h）
readonly ITCM_MEM="top_itcm_itcm_memory"
readonly DTCM_MEM="top_dtcm_dtcm_memory"
readonly -a EXPECT_ITCM=("0:10002083" "1:108113" "2:10202223" "3:6f")
readonly EXPECT_DTCM="64:5a"

# check_product <dir> <expect_init: 0|1>
#   expect_init=1 ⇒ 必须有 **恰好** 上面那些赋值，且无其它
#   expect_init=0 ⇒ 必须 **一条赋值都没有**
check_product() {
    local dir="$1" expect_init="$2"
    local parsed
    parsed="$(parse_product "$dir/top.v")" || {
        fail "解析失败：$dir/top.v"
        return 1
    }

    local nports
    nports="$(printf '%s\n' "$parsed" | awk '/^PORTS /{print $2}')"
    if [ -z "$nports" ] || [ "$nports" -le 0 ]; then
        fail "顶层端口表为空（$dir）"
        return 1
    fi

    local mem_lines
    mem_lines="$(printf '%s\n' "$parsed" | grep '^MEM ' || true)"
    local nmem=0
    [ -n "$mem_lines" ] && nmem="$(printf '%s\n' "$mem_lines" | wc -l | tr -d ' ')"

    if [ "$expect_init" -eq 0 ]; then
        # ⚠️ 反向：基线产物 MUST **没有**任何 init 赋值
        if [ "$nmem" -ne 0 ]; then
            fail "基线产物含 $nmem 条 init 赋值（期望 0）⇒ ⇒ 「初值只来自 init_data」不成立"
            printf '%s\n' "$mem_lines" | sed 's/^/       /'
            return 1
        fi
        return 0
    fi

    # ⚠️ 正向：**恰好**这些赋值（多一条也不接受 ⇒ ⇒ 常量泄漏会被抓到）
    local want_count=$(( ${#EXPECT_ITCM[@]} + 1 ))
    if [ "$nmem" -ne "$want_count" ]; then
        fail "init 赋值条数 $nmem ≠ 期望 $want_count（$dir）"
        printf '%s\n' "$mem_lines" | sed 's/^/       /'
        return 1
    fi

    local item idx want got
    for item in "${EXPECT_ITCM[@]}"; do
        idx="${item%%:*}"
        want="${item#*:}"
        got="$(printf '%s\n' "$mem_lines" |
            awk -v m="$ITCM_MEM" -v i="$idx" '$2==m && $3==i {print $4}')"
        if [ "$got" != "$want" ]; then
            fail "I-TCM[$idx] = '0x${got:-缺}' ⇒ ⇒ 期望 '0x$want'"
            return 1
        fi
    done
    idx="${EXPECT_DTCM%%:*}"
    want="${EXPECT_DTCM#*:}"
    got="$(printf '%s\n' "$mem_lines" |
        awk -v m="$DTCM_MEM" -v i="$idx" '$2==m && $3==i {print $4}')"
    if [ "$got" != "$want" ]; then
        fail "D-TCM[$idx] = '0x${got:-缺}' ⇒ ⇒ 期望 '0x$want'（.data 未落盘）"
        return 1
    fi
    return 0
}

if [ "$MODE" = "self-test" ]; then
    echo "── F6：自检（把正反两面互换 ⇒ ⇒ 各自必红）"
    st=0
    if check_product "$WORK/plain" 1 2>/dev/null; then
        red "  ❌ 基线产物喂「期望有初值」⇒ ⇒ 仍然判绿（判据对内容不敏感）"
        st=1
    fi
    if check_product "$WORK/init" 0 2>/dev/null; then
        red "  ❌ 带初值产物喂「期望无初值」⇒ ⇒ 仍然判绿（判据对内容不敏感）"
        st=1
    fi
    # 投毒：**值**改一位（.data 0x5a → 0x5b）⇒ ⇒ 必须红
    # ⚠️ 先确认投毒**真的落盘**（否则跑的是未改动的副本 ⇒ 假绿，同「变异必须能改变可观测行为」）
    poisoned="$WORK/poisoned"
    rm -rf "$poisoned" && mkdir -p "$poisoned"
    sed "s/${DTCM_MEM}\[64\] = 32'h5a;/${DTCM_MEM}[64] = 32'h5b;/" \
        "$WORK/init/top.v" >"$poisoned/top.v"
    if cmp -s "$WORK/init/top.v" "$poisoned/top.v"; then
        red "  ❌ 投毒未生效（sed 没命中）⇒ ⇒ 自检本身无效"
        st=1
    elif check_product "$poisoned" 1 2>/dev/null; then
        red "  ❌ 值投毒（0x5a→0x5b）后仍判绿 ⇒ ⇒ 判据只查「有位」不查「值」"
        st=1
    fi
    FAILED=0  # 自检里上面的 fail 输出是**预期的**，不计入
    if [ "$st" -ne 0 ]; then
        red "❌ verify_rv32i_fw_init_rtl --self-test: FAIL"
        exit 1
    fi
    green "✅ verify_rv32i_fw_init_rtl --self-test: PASS（正反互换各自必红）"
    exit 0
fi

echo "── F2：零端口变更（带初值 vs 基线）"
if ! diff <(parse_product "$WORK/plain/top.v" | grep '^PORT ') \
    <(parse_product "$WORK/init/top.v" | grep '^PORT ') >"$WORK/ports.diff" 2>&1; then
    fail "顶层端口表**变了** ⇒ ⇒ L2 的「装载不进端口面」不成立（见 $WORK/ports.diff）"
    head -20 "$WORK/ports.diff" | sed 's/^/       /'
else
    ok "F2 端口表逐字相同（$(parse_product "$WORK/init/top.v" | awk '/^PORTS /{print $2}') 个）"
fi

echo "── F3：带初值产物携带**手推**的固件字"
if check_product "$WORK/init" 1; then
    ok "F3 I-TCM[0..3] + D-TCM[64] 与手推值一致"
fi

echo "── F4：基线产物**没有** init 赋值（反向判据）"
if check_product "$WORK/plain" 0; then
    ok "F4 基线产物零 init 赋值 ⇒ ⇒ F3 的内容只可能来自 init_data"
fi

echo "── F5：确定性（连发两次逐字节相同）"
if cmp -s "$WORK/init/top.v" "$WORK/init2/top.v"; then
    ok "F5 两次发射逐字节相同"
else
    fail "两次发射不同（产物不确定 ⇒ ⇒ 下游无法做内容哈希缓存/比对）"
fi

echo
if [ "$FAILED" -ne 0 ]; then
    red "❌ verify_rv32i_fw_init_rtl: FAIL"
    exit 1
fi
green "✅ verify_rv32i_fw_init_rtl: PASS（F0 发射器 · F1 三份产物 · F2 零端口变更 · F3 手推初值 · F4 反向零赋值 · F5 确定性）"
