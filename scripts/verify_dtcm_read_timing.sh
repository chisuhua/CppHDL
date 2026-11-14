#!/usr/bin/env bash
# scripts/verify_dtcm_read_timing.sh
#
# **D-TCM 读写端口的时序结构判据**（`memory-timing-contract` **T4**）
#
# 判据（逐条可复跑 · 失败即 exit 1）：
#   W1 **写口 MUST 是寄存写**（`always_ff`）—— 写是沿上提交，不是组合穿透
#   W2 **写口 MUST 被写使能门控**（块内首条语句是 `if (...)`）且门控 MUST NOT 是常量真
#      ⇒ ⇒ 无门控 = 「每拍都写」⇒ ⇒ 会把**非 store 拍**当成写（地址/数据是垃圾）
#   W3 写口的**地址索引 / 写入数据 MUST 是信号**（MUST NOT 是字面常量）
#      ⇒ ⇒ 常量索引 = tie 死（永远写同一格）；常量数据 = 写入值恒为常数
#   W4 **确定性**（同一二进制两次发射逐字节相同）
#   W5 `--self-test`：两条投毒**各自必红**（去掉门控 / 写口改非寄存）
#
# ⚠️ **本护栏管的是「结构」，不是「相位」** —— 「写使能与被覆盖地址**同拍**」
#   这一条**结构上不可判**（三者都是组合 net）⇒ ⇒ 它的判据是**行为级**：
#   `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` 的
#   `Rv32iSoc - StorePhaseCoherence`（背靠背 store 的 (en, addr, data) **同拍配对**）
#   ⇒ ⇒ 该用例的**判别力**由「写使能错开一拍」的突变实测证明（见 change 的 tasks.md）。
#
# ⚠️ **本护栏不需要 verilator / 网络** ⇒ ⇒ 主 job（BUILD_VERILATOR=OFF）可跑。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 2

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
WORK="${WORK:-/tmp/opencode/dtcm_timing}"
EMITTER="$BUILD_DIR/examples/riscv-mini/emit_rtl_rv32i_soc"

red() { printf '\033[0;31m%s\033[0m\n' "$*"; }
green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
fail() { red "  ❌ $*"; FAILED=1; }
ok() { green "  ✅ $*"; }

MODE="check"
for arg in "$@"; do
    case "$arg" in
    --self-test) MODE="self-test" ;;
    -h | --help)
        sed -n '3,30p' "$0"
        exit 0
        ;;
    *)
        echo "用法：$0 [--self-test]" >&2
        exit 2
        ;;
    esac
done

FAILED=0

echo "── W0：发射器存在（fail-closed）"
if [ ! -x "$EMITTER" ]; then
    red "❌ W0 发射器缺失：$EMITTER"
    echo "     先构建：cmake --build $BUILD_DIR --target emit_rtl_rv32i_soc" >&2
    exit 2
fi
ok "W0 发射器存在"

rm -rf "$WORK"
mkdir -p "$WORK/a" "$WORK/b"

# ── 产物解析（D-TCM 写口块）─────────────────────────────────────────
# 输出（每行一项）：
#   BLOCK <found|missing>
#   EDGE  <always_ff|other>
#   GATE  <cond|none>
#   INDEX <ident|const:...>
#   DATA  <ident|const:...>
parse_write_port() {
    python3 - "$1" <<'PY'
import re
import sys

src = open(sys.argv[1], encoding="utf-8").read()

# ⚠️ **MUST 精确到 D-TCM** —— 产物里的写口块**不止一个**
#   （实测 3 个：`top_pipeline_reg_file` / `top_itcm_itcm_memory` / `top_dtcm_dtcm_memory`）
#   ⇒ ⇒ 用「第一个 Write port」会**静默检查错对象**（那正是本护栏的自检抓出来的 bug）
pat = re.compile(
    r"^(\s*)(always_ff|always_comb|always)\s*@\(([^)]*)\)\s*begin\s*"
    r"//\s*Write port for (\w*dtcm\w*)\s*\n(.*?)^\1end\s*$",
    re.S | re.M)
hits = list(pat.finditer(src))
if not hits:
    print("BLOCK missing")
    raise SystemExit(0)
if len(hits) != 1:
    print("BLOCK ambiguous:%d" % len(hits))
    raise SystemExit(0)
m = hits[0]

print("BLOCK found")
kw = m.group(2)
print("EDGE %s" % ("always_ff" if kw == "always_ff" else "other"))

body = m.group(5)
lines = [ln.strip() for ln in body.splitlines() if ln.strip()]
if not lines:
    print("GATE none")
    print("INDEX const:empty")
    print("DATA const:empty")
    raise SystemExit(0)

first = lines[0]
gm = re.match(r"if\s*\((.*)\)$", first)
if gm:
    cond = gm.group(1).strip()
    print("GATE %s" % ("const:true" if cond in ("1'b1", "1", "1'd1") else cond))
else:
    print("GATE none")

am = re.search(r"\[\s*([^\]]+?)\s*\]\s*<=\s*([^;]+);", body)
if not am:
    print("INDEX const:none")
    print("DATA const:none")
    raise SystemExit(0)

def classify(tok):
    tok = tok.strip()
    return ("const:%s" % tok) if re.match(r"^[0-9]+$|^[0-9]+'[bdh]", tok) else tok

print("INDEX %s" % classify(am.group(1)))
print("DATA %s" % classify(am.group(2)))
PY
}

# check_product <top.v>
check_product() {
    local v="$1" parsed
    parsed="$(parse_write_port "$v")" || {
        fail "解析失败：$v"
        return 1
    }
    local val
    val() { printf '%s\n' "$parsed" | awk -v k="$1" '$1==k{sub(/^[^ ]+ /,"");print; exit}'; }

    if [ "$(val BLOCK)" != "found" ]; then
        fail "W1 D-TCM 的写口块**不存在**（`// Write port for ...dtcm...` 未命中）"
        return 1
    fi
    if [ "$(val EDGE)" != "always_ff" ]; then
        fail "W1 D-TCM 的写口**不是寄存写**（sensitivity/语句形态 = $(val EDGE)）⇒ ⇒ 写会组合穿透"
        return 1
    fi
    local gate
    gate="$(val GATE)"
    case "$gate" in
    none)
        fail "W2 写口**无门控**（块内首条语句不是 \`if (...)\`）⇒ ⇒ 每拍都写"
        return 1
        ;;
    const:*)
        fail "W2 写口的门控是**常量**（$gate）⇒ ⇒ 每拍都写"
        return 1
        ;;
    esac
    case "$(val INDEX)" in
    const:*) fail "W3 写口的**地址索引是常量**（$(val INDEX)）⇒ ⇒ tie 死（永远写同一格）"; return 1 ;;
    esac
    case "$(val DATA)" in
    const:*) fail "W3 写口的**写入数据是常量**（$(val DATA)）"; return 1 ;;
    esac
    return 0
}

echo "── W1–W3 + W4：发射两次并判定"
for d in a b; do
    if ! "$EMITTER" --out "$WORK/$d" >"$WORK/$d/emit.log" 2>&1; then
        fail "发射失败（见 $WORK/$d/emit.log）"
        exit 1
    fi
done

if check_product "$WORK/a/top.v"; then
    ok "W1 写口 = always_ff · W2 有门控且非常量 · W3 索引/数据均为信号"
fi

if cmp -s "$WORK/a/top.v" "$WORK/b/top.v"; then
    ok "W4 两次发射逐字节相同"
else
    fail "W4 两次发射不同（产物不确定）"
fi

if [ "$MODE" = "self-test" ]; then
    echo "── W5：自检（两条投毒各自必红）"
    st=0

    # 投毒 ①：去掉 `if (...)` 门控 ⇒ W2 必红
    poison1="$WORK/poison_nogate"
    rm -rf "$poison1" && mkdir -p "$poison1"
    python3 - "$WORK/a/top.v" "$poison1/top.v" <<'PY'
import re
import sys
# ⚠️ 投毒 MUST 精确到 **D-TCM** 的写口（产物里还有 reg_file / itcm 两个写口）
blk = re.compile(r"(^\s*)(always_ff)\s*@\([^)]*\)\s*begin\s*"
                 r"//\s*Write port for (\w*dtcm\w*)\s*\n(.*?)^\1end\s*$",
                 re.S | re.M)
src = open(sys.argv[1], encoding="utf-8").read()
m = blk.search(src)
if not m:
    sys.exit(1)
body = m.group(4)
gated = re.sub(r"^\s*if\s*\([^\n]*\)\s*\n", "", body, count=1, flags=re.M)
if gated == body:
    sys.exit(1)
src = src[:m.start(4)] + gated + src[m.end(4):]
open(sys.argv[2], "w", encoding="utf-8").write(src)
PY
    if cmp -s "$WORK/a/top.v" "$poison1/top.v"; then
        red "  ❌ 投毒①未生效（门控没删掉）⇒ ⇒ 自检本身无效"
        st=1
    elif check_product "$poison1/top.v" 2>/dev/null; then
        red "  ❌ 去掉门控后仍判绿 ⇒ ⇒ W2 无判别力"
        st=1
    fi
    FAILED=0

    # 投毒 ②：写口改非寄存（always_ff → always_comb）⇒ W1 必红
    poison2="$WORK/poison_comb"
    rm -rf "$poison2" && mkdir -p "$poison2"
    python3 - "$WORK/a/top.v" "$poison2/top.v" <<'PY'
import re
import sys
# ⚠️ 只改 **D-TCM** 那一个写口（其余两个写口 MUST 保持原样 ⇒ 否则投毒不是「单点」）
blk = re.compile(r"^(\s*)(always_ff)\s*@\([^)]*\)\s*begin\s*"
                 r"(//\s*Write port for \w*dtcm\w*)\s*\n(.*?)^\1end\s*$",
                 re.S | re.M)
src = open(sys.argv[1], encoding="utf-8").read()
m = blk.search(src)
if not m:
    sys.exit(1)
repl = "%salways_comb begin %s\n%s^%send" % (m.group(1), m.group(3), m.group(4), m.group(1))
src = src[:m.start()] + repl + src[m.end():]
open(sys.argv[2], "w", encoding="utf-8").write(src)
PY
    if cmp -s "$WORK/a/top.v" "$poison2/top.v"; then
        red "  ❌ 投毒②未生效（写口没改成 always_comb）⇒ ⇒ 自检本身无效"
        st=1
    elif check_product "$poison2/top.v" 2>/dev/null; then
        red "  ❌ 写口改非寄存后仍判绿 ⇒ ⇒ W1 无判别力"
        st=1
    fi
    FAILED=0

    if [ "$st" -ne 0 ]; then
        red "❌ verify_dtcm_read_timing --self-test: FAIL"
        exit 1
    fi
    green "✅ verify_dtcm_read_timing --self-test: PASS（去掉门控 / 改非寄存 各自必红）"
    exit 0
fi

echo
if [ "$FAILED" -ne 0 ]; then
    red "❌ verify_dtcm_read_timing: FAIL"
    exit 1
fi
green "✅ verify_dtcm_read_timing: PASS（W0 发射器 · W1 寄存写 · W2 有门控 · W3 非常量 · W4 确定性）"
