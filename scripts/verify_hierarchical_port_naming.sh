#!/usr/bin/env bash
# scripts/verify_hierarchical_port_naming.sh
#
# **端口命名契约的静态护栏**（残留 5 · capability `hierarchical-port-naming-guard`）
#
# 它防什么
# --------------------------------------------------------------------------
# **N1**：**根** module MUST NOT 有**框架合成端口**（`__if_<n>` / `__x_<n>` /
#      `__up_<n>`）⇒ ⇒ 顶层端口名 MUST 与扁平产物逐字相同（主 spec 的既有契约）。
# **N2a**：语料 MUST **确实**含可被判的合成端口（**存在量词**）⇒ ⇒ 否则 N1 恒绿。
# **N2b**：`{__if_, __x_, __up_}` **每个** MUST 至少有一个**真实产物**覆盖
#      ⇒ ⇒ 缺失即红并**点名缺哪个**（覆盖矩阵 ⇒ 防「只覆盖一种前缀」的半绿）。
# **N3**：根 MUST 由**结构**判定（`declared − instantiated` 恰好 1）
#      ⇒ ⇒ 根名**无需配置**（结构判定使「配置根名」不必要 ⇒ 且**不硬编码** `top` /
#      任何文件名 —— 发射器的根名旋钮是 `top_module_name`，本脚本**不依赖**它）。
# **N0**：fail-closed —— 缺工具 / 生成器失败 / 无 `.tree.json` ⇒ 明确报错 + 非 0 退出。
#
# ⚠️⚠️ **三条假阳性禁令**（判据只认「module 的一级端口声明」）：
#   ① 根**体内**合法的 `__up_<n>` **wire**（`direction == 'NONE'`）
#   ② 根**实例端口映射**里合法的 `.__if_0(...)` / `.__x_0(...)` / `.__up_0(...)`
#   ③ 用户自取的前缀名（合成名的确切形态恒为 `prefix + 十进制序号`）
# ⇒ ⇒ 本脚本**只**消费 `scripts/lib/verilog_tree_extract.py` 的结构化输出
#      ⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器（也不 grep `.v` 文本找端口）。
#
# ⚠️ **端口提取 MUST 用 `--module-ports`（不是 `--ports`）**：
#    verilator 的 `isPrimaryIO` 语义 = **顶层 primary IO** ⇒ ⇒ 非顶层 module 的
#    端口 `isPrimaryIO` 是 `None` ⇒ ⇒ 用 `--ports` 判「非根 module 有没有合成端口」
#    会**恒绿**（假绿）。`--module-ports` 用「一级 VAR + direction ∈ {INPUT,OUTPUT}」
#    ⇒ ⇒ 精确对应 module 头的端口声明行。
#
# 用法：
#   bash scripts/verify_hierarchical_port_naming.sh              # 主路径（N0–N3）
#   bash scripts/verify_hierarchical_port_naming.sh --self-test  # 逐条投毒 ⇒ 必红
#
# 退出码：0 = 通过 · 1 = 判据失败 · 2 = 前置失败（N0）
# ============================================================================
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXTRACTOR="${PNG_EXTRACTOR:-$REPO_ROOT/scripts/lib/verilog_tree_extract.py}"
GENERATOR="${HIER_RTL_GENERATOR:-$REPO_ROOT/build/tests/hier_rtl_products}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT          # ⚠️ **自己的** $WORK ⇒ 不共享别人的
mkdir -p "$WORK/logs"

fail() { echo "❌ $1" >&2; }
ok()   { echo "✅ $1"; }
note() { echo "ℹ️  $1" >&2; }

SELF_TEST=0
[ "${1:-}" = "--self-test" ] && SELF_TEST=1

# 合成端口的**精确**形态（不是「以 `__if_` 开头」）
# ⚠️ 用户可合法自取 `__if_xxx` 这类名字 ⇒ 「开头」会假阳性
SYNTH_PREFIXES=(__if_ __x_ __up_)

# ═══════════════════════════════════════════════════════════════════════════
# N0：fail-closed（缺工具 / 前置失败 ⇒ 退出码 2）
# ═══════════════════════════════════════════════════════════════════════════
VERILATOR=""
for cand in "$REPO_ROOT/build/verilator-install/bin/verilator" "$(command -v verilator || true)"; do
    [ -n "$cand" ] && [ -x "$cand" ] && { VERILATOR="$cand"; break; }
done
[ -n "$VERILATOR" ] || { fail "N0-MISSING-VERILATOR：找不到 verilator（build/verilator-install/bin/verilator 或 PATH）"; exit 2; }
[ -x "$GENERATOR" ] || { fail "N0-MISSING-GENERATOR：产物生成器不可用（$GENERATOR）⇒ 先 cmake --build build --target hier_rtl_products"; exit 2; }
[ -f "$EXTRACTOR" ]  || { fail "N0-MISSING-EXTRACTOR：提取器缺失（$EXTRACTOR）"; exit 2; }
command -v python3 >/dev/null 2>&1 || { fail "N0-MISSING-PYTHON：找不到 python3"; exit 2; }
python3 -c "import ast,sys; ast.parse(open(sys.argv[1],encoding='utf-8').read())" "$EXTRACTOR" \
    || { fail "N0 提取器语法错误（$EXTRACTOR）"; exit 2; }

# ── 产物生成（主 3 个 + 覆盖 fixture 2 个 ⇒ **两个独立目录**）──────────────
#    ⚠️ `verify_hierarchical_rtl.sh` 强制它自己的产物目录里**恰好 3 个** `.v`
#    （R6/R8 对未知产物名 fail-closed）⇒ ⇒ fixture MUST NOT 写进那个目录。
MAIN_DIR="$WORK/main"
FIX_DIR="$WORK/fixtures"
"$GENERATOR" "$MAIN_DIR" >"$WORK/logs/gen_main.log" 2>&1 \
    || { fail "N0-GENERATOR-FAILED：主产物生成失败（见 $WORK/logs/gen_main.log）"; exit 2; }
"$GENERATOR" "$WORK/ignored" --prefix-fixtures "$FIX_DIR" >"$WORK/logs/gen_fix.log" 2>&1 \
    || { fail "N0-GENERATOR-FAILED：fixture 生成失败（见 $WORK/logs/gen_fix.log）"; exit 2; }

MAIN_COUNT=$(find "$MAIN_DIR" -maxdepth 1 -name '*.v' | wc -l | tr -d ' ')
FIX_COUNT=$(find "$FIX_DIR" -maxdepth 1 -name '*.v' | wc -l | tr -d ' ')
[ "$MAIN_COUNT" = "3" ] || { fail "N0-PRODUCT-COUNT：主产物数 = $MAIN_COUNT（期望 3）"; exit 2; }
[ "$FIX_COUNT" = "2" ] || { fail "N0-PRODUCT-COUNT：fixture 数 = $FIX_COUNT（期望 2）"; exit 2; }

# ═══════════════════════════════════════════════════════════════════════════
# 数据收集（结构化提取 ⇒ 不解析 Verilog 文本）
# ═══════════════════════════════════════════════════════════════════════════
# 用法：collect_data <产物目录...> ⇒ 填充 ROOT_OF / DECL_FILE / INST_FILE / PORTS_FILE
collect_data() {
    local v name json
    for v in "$@"; do
        name="$(basename "$v" .v)"
        json="$WORK/json/$name"
        mkdir -p "$json"
        "$VERILATOR" --json-only -Wno-fatal --Mdir "$json" "$v" >"$WORK/logs/$name-json.log" 2>&1
        if [ $? -ne 0 ]; then
            fail "N0 产物 '$name' 的 verilator --json-only 失败（见 $WORK/logs/$name-json.log）"
            return 1
        fi
        json="$(find "$json" -name '*.tree.json' ! -name '*.meta.json' | head -1)"
        [ -n "$json" ] || { fail "N0-NO-TREE-JSON：产物 '$name' 未产出 .tree.json"; return 1; }
        DECL_FILE[$name]="$WORK/decl_$name.txt"
        INST_FILE[$name]="$WORK/inst_$name.txt"
        PORTS_FILE[$name]="$WORK/ports_$name.tsv"
        # ⚠️ **三条提取 MUST 检查退出码** —— 否则运行时异常会退化成「空输入」
        #    ⇒ ⇒ 失败分类会从 N0（前置失败 · 退出码 2）漂到「判据失败」（退出码 1）
        if ! python3 "$EXTRACTOR" "$json" --modules | sort -u >"${DECL_FILE[$name]}"; then
            fail "N0 提取失败：产物 '$name' 的 --modules 提取异常"
            return 1
        fi
        if ! python3 "$EXTRACTOR" "$json" | cut -f3 | sort -u >"${INST_FILE[$name]}"; then
            fail "N0 提取失败：产物 '$name' 的实例化集提取异常"
            return 1
        fi
        if ! python3 "$EXTRACTOR" "$json" --module-ports >"${PORTS_FILE[$name]}"; then
            fail "N0 提取失败：产物 '$name' 的 --module-ports 提取异常"
            return 1
        fi
        ROOT_OF[$name]="$(comm -23 "${DECL_FILE[$name]}" "${INST_FILE[$name]}" | tr '\n' ' ' | sed 's/ *$//')"
    done
    return 0
}

# ═══════════════════════════════════════════════════════════════════════════
# 判定（N3 判根 → N1 → N2a → N2b）⇒ 退出码 0/1
# ═══════════════════════════════════════════════════════════════════════════
declare -A ROOT_OF=()
declare -A DECL_FILE=()
declare -A INST_FILE=()
declare -A PORTS_FILE=()

judge() {
    local name root roots n_decl n_inst n_root hits c missing p
    local fail_root=0 fail_n1=0 fail_n2a=0 fail_n2b=0

    # ── N3：结构判根（4 种情形 ⇒ 文本可区分）─────────────────────────────
    for name in "${!ROOT_OF[@]}"; do
        roots="${ROOT_OF[$name]}"
        n_decl=$(wc -l <"${DECL_FILE[$name]}" | tr -d ' ')
        n_inst=$(wc -l <"${INST_FILE[$name]}" | tr -d ' ')
        if [ "$n_decl" = "1" ] && [ "$n_inst" = "0" ]; then
            ok "N3 产物 '$name'：单 module 产物（零 CELL）⇒ 根 = $(cat "${DECL_FILE[$name]}")"
            continue
        fi
        n_root=$(printf '%s\n' $roots | grep -c . || true)
        if [ "$n_root" != "1" ]; then
            if [ "$n_root" = "0" ]; then
                fail "N3 判根失败：产物 '$name' 的根被实例化（自实例化 / 重复例化）⇒ 声明集=${n_decl} 被实例化集=${n_inst}"
            else
                fail "N3 判根失败：产物 '$name' 有 $n_root 个根（多棵独立树 / 孤儿 module）⇒ 声明集=${n_decl} 被实例化集=${n_inst}"
            fi
            fail_root=1
            continue
        fi
        ok "N3 产物 '$name'：根 = $roots（结构判定）"
    done

    # ── N1：根 module MUST NOT 有框架合成端口 ────────────────────────────
    local skipped_root=0
    for name in "${!ROOT_OF[@]}"; do
        root="$(printf '%s\n' ${ROOT_OF[$name]} | head -1)"
        if [ -z "$root" ]; then
            # ⚠️ **判根失败 ⇒ N1 MUST NOT 静默跳过**（否则「找不到根」= N1 恒绿 = 假绿）
            fail "N1 无法判定：产物 '$name' 的根未能确定（判根失败 ⇒ N1 不可跳过）"
            fail_n1=1
            skipped_root=1
            continue
        fi
        hits="$(awk -F'\t' -v r="$root" '$1 == r && $2 ~ /^__(if|x|up)_[0-9]+$/ {print $2}' "${PORTS_FILE[$name]}")"
        if [ -n "$hits" ]; then
            fail "N1 根 module '$root'（产物 '$name'）含框架合成端口：$(printf '%s' "$hits" | tr '\n' ' ')"
            fail_n1=1
        fi
    done
    [ "$fail_n1" = 0 ] && ok "N1 所有产物的根 module 均无框架合成端口"
    [ "$skipped_root" = 1 ] && note "N1 有产物因判根失败而无法判定（已计入失败）"

    # ── N2a：全局存在量词（语料 MUST 确实含可被判的合成端口）────────────
    SYNTH_ALL="$WORK/synth_all.tsv"
    : >"$SYNTH_ALL"
    for name in "${!ROOT_OF[@]}"; do
        root="$(printf '%s\n' ${ROOT_OF[$name]} | head -1)"
        [ -n "$root" ] || continue
        awk -F'\t' -v r="$root" '$1 != r && $2 ~ /^__(if|x|up)_[0-9]+$/ {print $1"\t"$2}' "${PORTS_FILE[$name]}" \
            >>"$SYNTH_ALL"
    done
    n_synth=$(wc -l <"$SYNTH_ALL" | tr -d ' ')
    if [ "$n_synth" -lt 1 ]; then
        fail "N2a 语料中**没有任何**可被判的合成端口 ⇒ N1 恒绿（无判别力）"
        fail_n2a=1
    else
        ok "N2a 语料含 $n_synth 个非根 module 的合成端口（N1 非恒绿）"
    fi

    # ── N2b：前缀覆盖矩阵（三个前缀各至少一个真实产物覆盖）──────────────
    missing=""
    for p in "${SYNTH_PREFIXES[@]}"; do
        c=$(awk -F'\t' -v p="$p" '$2 ~ ("^" p "[0-9]+$")' "$SYNTH_ALL" | wc -l | tr -d ' ')
        [ "$c" -lt 1 ] && missing="$missing $p"
    done
    if [ -n "$missing" ]; then
        fail "N2b 前缀覆盖矩阵缺:$missing（每个前缀 MUST 至少一个真实产物覆盖）"
        fail_n2b=1
    else
        ok "N2b 前缀覆盖矩阵完整（${SYNTH_PREFIXES[*]} 各至少一个真实产物）"
    fi

    [ "$fail_root" = 1 ] && return 1
    [ "$fail_n1" = 1 ] && return 1
    [ "$fail_n2a" = 1 ] && return 1
    [ "$fail_n2b" = 1 ] && return 1
    return 0
}

# ═══════════════════════════════════════════════════════════════════════════
# 主路径
# ═══════════════════════════════════════════════════════════════════════════
if [ "$SELF_TEST" = 0 ]; then
    PRODUCTS=()
    while IFS= read -r f; do PRODUCTS+=("$f"); done < <(find "$MAIN_DIR" "$FIX_DIR" -maxdepth 1 -name '*.v' | sort)
    collect_data "${PRODUCTS[@]}" || exit 2
    judge || exit 1
    ok "verify_hierarchical_port_naming: PASS（N0–N3）"
    exit 0
fi

# ═══════════════════════════════════════════════════════════════════════════
# --self-test：逐条投毒 ⇒ 必红（预期红）/ 必绿（反向对照）⇒ 还原复绿
# ═══════════════════════════════════════════════════════════════════════════
PRODUCTS=()
while IFS= read -r f; do PRODUCTS+=("$f"); done < <(find "$MAIN_DIR" "$FIX_DIR" -maxdepth 1 -name '*.v' | sort)

echo "=== self-test：基线 MUST 绿 ==="
BASE_DECL=(); BASE_INST=(); BASE_PORTS=(); BASE_ROOT=()
collect_data "${PRODUCTS[@]}" || exit 2
BASE_DECL=("${DECL_FILE[@]}"); BASE_INST=("${INST_FILE[@]}")
BASE_PORTS=("${PORTS_FILE[@]}"); BASE_ROOT=("${ROOT_OF[@]}")
if ! judge; then
    fail "self-test：基线**不绿** ⇒ 判据在干净语料上就失败 ⇒ 投毒无效"
    exit 1
fi
ok "self-test：基线绿"

# 投毒工具：改产物文本（**不改判据实现** ⇒ 负向对照的有效性）
# ⚠️ **必须按「结构判出的根名」定位** —— 产物里 module 的**声明顺序**里
#    **根不一定在前**（实测 `synth_x.v` 的第一个 module 是 `synth_x_child`）
#    ⇒ ⇒ 投到「第一个 module」上会打在**非根**上 ⇒ ⇒ N1（只查根）不会红。
poison() { # $1 = 源 .v · $2 = 目标 .v · $3 = 投毒种类 · $4 = 根 module 名
    python3 - "$1" "$2" "$3" "$4" <<'PY'
import re, sys
src, dst, kind, root = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
text = open(src, encoding='utf-8').read()
if kind in ("root_if", "root_up", "rename_root_if"):
    port = "__up_9" if kind == "root_up" else "__if_9"
    # ⚠️ 只在**根** module 的头里插端口（`module <root> (`）
    pat = r'(?m)^(module\s+' + re.escape(root) + r'\s*\(\n)'
    text2, n = re.subn(pat, r'\1    input [7:0] ' + port + r',\n', text, count=1)
    if n != 1:
        sys.exit(3)
    text = text2
    if kind == "rename_root_if":
        # ⚠️ **改名 + 插端口** ⇒ 判据 MUST **跟着改名后的根走**并转红
        #    ⇒ 否则「护栏在改名产物上因找不到根而恒绿」这条假绿就存在
        text2, n = re.subn(r'(?m)^module\s+' + re.escape(root) + r'\s*\(',
                            r'module my_dut (', text, count=1)
        if n != 1:
            sys.exit(3)
        text = text2
elif kind == "orphan":
    # 追加一个**未被实例化**的 module ⇒ 声明集 +1 ⇒ 多根
    text += ("\nmodule orphan_root (\n    input default_clock\n);\n"
             "endmodule\n")
elif kind == "rename_root":
    # 根改名 ⇒ **结构判根** MUST 仍判对（不依赖名字）⇒ 期望仍绿
    text2, n = re.subn(r'(?m)^module\s+' + re.escape(root) + r'\s*\(',
                        r'module my_dut (', text, count=1)
    if n != 1:
        sys.exit(3)
    text = text2
else:
    sys.exit(2)
open(dst, 'w', encoding='utf-8').write(text)
PY
}

run_poison() { # $1 = 种类 · $2 = 期望（red|green）· $3 = 描述
    local kind="$1" expect="$2" desc="$3"
    # ⚠️ **先恢复基线数据** ⇒ 投毒用的根名 MUST 是**基线**的根
    #    （否则上一条投毒（如 orphan ⇒ 多根）会把 ROOT_OF 污染 ⇒ 后续投毒打错模块）
    ROOT_OF=(); DECL_FILE=(); INST_FILE=(); PORTS_FILE=()
    if ! collect_data "${PRODUCTS[@]}" >/dev/null 2>&1; then
        fail "self-test[$kind]：基线数据收集失败"
        return 1
    fi
    local pdir="$WORK/poison/$kind"
    rm -rf "$pdir"; mkdir -p "$pdir"
    local v name root
    for v in "${PRODUCTS[@]}"; do
        name="$(basename "$v" .v)"
        root="$(printf '%s\n' ${ROOT_OF[$name]} | head -1)"
        poison "$v" "$pdir/$name.v" "$kind" "$root" || {
            fail "self-test[$kind]：投毒脚本失败（无法改写 $name.v · 根='$root'）"
            return 1
        }
    done
    # ⚠️ **投毒后 MUST 仍能被 verilator 解析** ⇒ 否则是**无效变异**（如实登记）
    local poisoned=()
    for v in "$pdir"/*.v; do
        name="$(basename "$v" .v)"
        local jd="$WORK/pjson/$kind/$name"
        mkdir -p "$jd"
        "$VERILATOR" --json-only -Wno-fatal --Mdir "$jd" "$v" >"$WORK/logs/poison-$kind-$name.log" 2>&1
        if [ $? -ne 0 ]; then
            fail "self-test[$kind]：**无效变异** —— 投毒后 '$name' 无法被 verilator 解析"
            return 1
        fi
        find "$jd" -name '*.tree.json' ! -name '*.meta.json' | grep -q . || {
            fail "self-test[$kind]：**无效变异** —— 投毒后 '$name' 未产出 .tree.json"
            return 1
        }
        poisoned+=("$v")
    done
    # 用**投毒后的产物**重跑判据
    ROOT_OF=(); DECL_FILE=(); INST_FILE=(); PORTS_FILE=()
    if ! collect_data "${poisoned[@]}" >/dev/null 2>&1; then
        fail "self-test[$kind]：投毒后产物收集失败 ⇒ 判据未真正跑在投毒产物上"
        return 1
    fi
    local out rc
    out="$(judge 2>&1)"; rc=$?
    printf '%s\n' "$out" | grep -qE '❌' || true
    if [ "$expect" = "red" ]; then
        if [ "$rc" != 0 ]; then
            ok "self-test[$kind]：$desc ⇒ 判据**转红** ✓"
        else
            fail "self-test[$kind]：$desc ⇒ 判据**仍绿** ⇒ 判据无判别力"
            return 1
        fi
    else
        if [ "$rc" = 0 ]; then
            ok "self-test[$kind]：$desc ⇒ 判据**仍绿**（反向对照：不依赖名字）✓"
        else
            fail "self-test[$kind]：$desc ⇒ 判据**转红**（不该红）"
            printf '%s\n' "$out" | grep -E '❌' >&2 || true
            return 1
        fi
    fi
    return 0
}

echo
echo "=== self-test：逐条投毒 ==="
run_poison root_if   red   "根 module 插入 __if_9 端口 ⇒ N1 MUST 转红" || exit 1
run_poison root_up   red   "根 module 插入 __up_9 端口 ⇒ N1 MUST 转红（补全区）" || exit 1
run_poison orphan    red   "追加未被实例化的 module ⇒ N3 MUST 转红（多根）" || exit 1
run_poison rename_root green "根改名 my_dut ⇒ 结构判根 MUST 仍判对（不依赖名字）" || exit 1
run_poison rename_root_if red "根改名 **且** 插 __if_9 ⇒ N1 MUST 跟着改名后的根**转红**" || exit 1

echo
echo "=== self-test：N0 fail-closed（缺提取器 ⇒ 退出码 2 + 明说缺什么）==="
PNG_EXTRACTOR="$WORK/nonexistent_extractor.py" bash "$REPO_ROOT/scripts/verify_hierarchical_port_naming.sh" \
    >"$WORK/logs/n0.log" 2>&1
n0rc=$?
if [ "$n0rc" = 2 ] && grep -q "N0-MISSING-EXTRACTOR" "$WORK/logs/n0.log"; then
    ok "self-test[N0]：缺提取器 ⇒ 退出码 2 + 文本点名 ✓"
else
    fail "self-test[N0]：缺提取器 ⇒ 退出码 $n0rc（期望 2）/ 文本未点名 N0-MISSING-EXTRACTOR"
    exit 1
fi

echo
echo "=== self-test：还原复绿 ==="
ROOT_OF=(); DECL_FILE=(); INST_FILE=(); PORTS_FILE=()
collect_data "${PRODUCTS[@]}" >/dev/null 2>&1 || { fail "还原后重新收集失败"; exit 1; }
judge >/dev/null 2>&1 && ok "self-test：还原后复绿 ✓" || { fail "self-test：还原后仍红"; exit 1; }

echo
echo "✅ verify_hierarchical_port_naming: PASS（--self-test）"
exit 0
