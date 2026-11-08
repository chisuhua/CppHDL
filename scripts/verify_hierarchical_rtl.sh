#!/usr/bin/env bash
# ============================================================================
# verify_hierarchical_rtl.sh
#
# 护栏：**层次化 RTL 发射的产物级判据**（R1–R8）。
#
# ⚠️ **为什么需要它**：`tests/test_hierarchy_verilog.cpp` 的断言只能看见
#    「发射器自己认为产出了什么」（文本子串 + 返回值）。它**看不见** Verilog
#    语义层的两类缺陷：
#      ① 产物根本不是合法 Verilog（未声明标识符 / 端口名不存在 / 宽度不匹配）；
#      ② module 树与设计意图不同构（实例连错模块 ⇒ 计数仍「对」）。
#    ⇒ ⇒ 本护栏**独立地**用 verilator 读产物（结构化提取，MUST NOT 自研解析器）。
#
# 判据（每条都有 `--self-test` 的**原地投毒**对照 ⇒ 必须变红才算有判别力）：
#   R1 lint 门 —— **ADR-060 §2.5 的可执行定义**：
#        命令 = `<verilator> --lint-only --top-module top <file>`；**不加 `-Wall`**
#        （本设计的合法产物会触发 `DECLFILENAME` / `PINCONNECTEMPTY` / `UNUSEDSIGNAL`
#        ⇒ 升墙等于把合法形态判死）；**禁用 `-Wno-fatal`**（它会让 warning 不影响
#        退出码 ⇒ 门失效）；MUST 捕获退出码 + 额外交付「stderr 无 `%Error`」断言；
#        产物内 `/* verilator lint_off X */` 的 X MUST ∈ 白名单
#        （当前仅 `WIDTHEXPAND` / `WIDTHTRUNC`，新增任何一条 MUST 在 change 里登记理由）。
#   R2 实例端口名 ∈ 目标 module 端口集（`PINNOTFOUND` / `PINMISSING` 不得出现）。
#   R3 端口宽度一致 —— 机制 = **剥掉 `lint_off` 后重新 lint**：两条 `lint_off`
#        正是宽度告警的全局消音器 ⇒ 剥掉后任何宽度不匹配都会变成 fatal warning。
#        （⚠️ 这条**不是** R1 的重复：R1 在 lint_off 生效时恒绿，R3 才看得见。）
#   R4 产物确定性 —— 同一二进制两次生成 ⇒ `diff -r` 逐字节相同。
#   R5 跨模块 `ch_mem` 被拒 —— `ok == false` + 错误串含 `cross-module ch_mem`。
#   R6 module 树结构同构 —— 从 `verilator --json-only` **独立提取**
#        （复用 `scripts/lib/verilog_tree_extract.py`）⇒ 逐字节比对期望三元组 +
#        结构不变量（唯一根 / 每 module 恰被实例化一次 / 全部可达 / 无环）。
#   R7 层次化产物路径 ≠ 扁平产物路径 —— **静态扫描仓内调用点**（适用面 = 仓内
#        调用点，不是「扫描所有 .v」⇒ 那样是误报机器）。
#   R8 顶层端口表 == **手推期望集** —— 从 `verilator --json-only` 结构化提取
#        （`isPrimaryIO` + dtype 的 `range` ⇒ 复用提取器的 `--ports` 模式）
#        ⇒ ⇒ 名字 + 方向 + 宽度**逐项**比对 ⇒ ⇒ 同时判掉「成层子树的 `ch_out`
#        跑到顶层」（期望集里没有它们 ⇒ 出现即红）。
#
# 用法：
#   bash scripts/verify_hierarchical_rtl.sh              # 主路径（R1–R8）
#   bash scripts/verify_hierarchical_rtl.sh --self-test   # 逐条投毒 ⇒ 必红 ⇒ 还原复绿
#
# 前置：`build/tests/hier_rtl_products`（生成器）+ 仓内 verilator
#       （`build/verilator-install/bin/verilator`）⇒ **缺一即失败**（fail-closed）。
# ============================================================================
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXTRACTOR="$REPO_ROOT/scripts/lib/verilog_tree_extract.py"
GENERATOR="${HIER_RTL_GENERATOR:-$REPO_ROOT/build/tests/hier_rtl_products}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/logs"

fail() { echo "❌ $1" >&2; }
ok()   { echo "✅ $1"; }

# ── R0：fail-closed（verilator / 生成器 / 提取器都必须可用）──────────────────
VERILATOR=""
for cand in "$REPO_ROOT/build/verilator-install/bin/verilator" "$(command -v verilator || true)"; do
    [ -n "$cand" ] && [ -x "$cand" ] && { VERILATOR="$cand"; break; }
done

require_tools() {
    [ -n "$VERILATOR" ] || {
        fail "R0 verilator 不可用（build/verilator-install/bin/verilator 或 PATH）"
        return 1
    }
    [ -x "$GENERATOR" ] || {
        fail "R0 产物生成器不可用：$GENERATOR（先 cmake --build build --target hier_rtl_products）"
        return 1
    }
    [ -f "$EXTRACTOR" ] || {
        fail "R0 提取器缺失：$EXTRACTOR"
        return 1
    }
    python3 -c "import ast,sys; ast.parse(open(sys.argv[1],encoding='utf-8').read())" "$EXTRACTOR" \
        || { fail "R0 提取器语法错误"; return 1; }
    return 0
}

# ── 工具：生成产物（恰好 3 个 .v）───────────────────────────────────────────
gen_products() {  # $1 = generator, $2 = out dir, $3 = log
    local gen="$1" dir="$2" log="$3"
    rm -rf "$dir" && mkdir -p "$dir"
    "$gen" "$dir" >"$log" 2>&1
    local rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "--- 生成器输出 ---" >&2
        cat "$log" >&2
        fail "生成器失败（rc=$rc）"
        return 1
    fi
    local n
    n="$(find "$dir" -maxdepth 1 -name '*.v' | wc -l)"
    if [ "$n" -ne 3 ]; then
        fail "生成器应产出 3 个 .v，实际 $n（目录：$dir）"
        return 1
    fi
    return 0
}

# ── 工具：跑一次 lint 门（**唯一**的 lint 调用点 ⇒ ADR-060 §2.5）─────────────
LINT_RC=0
lint_file() {  # $1 = file, $2 = log
    "$VERILATOR" --lint-only --top-module top "$1" >"$2" 2>&1
    LINT_RC=$?
}

# R1a：命令行本身 MUST NOT 含 `-Wall` / `-Wno-fatal`（ADR-060 §2.5）
check_lint_flags() {  # $1 = script path
    local script="$1" body
    if [ ! -f "$script" ]; then
        fail "R1 待检查脚本不存在：$script"
        return 1
    fi
    # 只检查 `lint_file` 的函数体（注释里提到这两个开关是合法的文档）
    body="$(awk '/^lint_file\(\)/{f=1} f{print} f&&/^}/{exit}' "$script")"
    if [ -z "$body" ]; then
        fail "R1 找不到 lint_file 的调用体（lint 门 MUST 只有这一个调用点）"
        return 1
    fi
    case "$body" in
    *--lint-only*) ;;
    *)
        fail "R1 lint 调用未使用 --lint-only"
        return 1
        ;;
    esac
    case "$body" in
    *-Wall*)
        fail "R1 lint 调用不得含 -Wall（ADR-060 §2.5：合法产物会触发 DECLFILENAME / PINCONNECTEMPTY / UNUSEDSIGNAL）"
        return 1
        ;;
    esac
    case "$body" in
    *-Wno-fatal*)
        fail "R1 lint 调用不得含 -Wno-fatal（ADR-060 §2.5：它会让 warning 不影响退出码 ⇒ 门失效）"
        return 1
        ;;
    esac
    return 0
}

# ── 工具：产物内 lint_off 白名单 ────────────────────────────────────────────
check_lint_off_whitelist() {  # $1 = file
    local file="$1" off bad=0
    for off in $(grep -oE 'verilator lint_off [A-Z_]+' "$file" | awk '{print $3}' | sort -u); do
        case "$off" in
        WIDTHEXPAND | WIDTHTRUNC) ;;
        *)
            fail "R1 产物含未登记的 lint_off：$off（$file；新增 MUST 在 change 里登记理由）"
            bad=1
            ;;
        esac
    done
    return "$bad"
}

# ── R1：lint 门（ADR-060 §2.5 的可执行定义）─────────────────────────────────
r1_lint_gate() {  # $1 = products dir
    local dir="$1" f name log bad=0
    for f in "$dir"/*.v; do
        [ -f "$f" ] || continue
        name="$(basename "$f" .v)"
        log="$WORK/logs/$name-r1.lint.log"
        lint_file "$f" "$log"
        if [ "$LINT_RC" -ne 0 ]; then
            fail "R1 lint 失败（rc=$LINT_RC）：$(basename "$f")"
            sed -n '1,20p' "$log" >&2
            bad=1
            continue
        fi
        if grep -q '%Error' "$log"; then
            fail "R1 lint 退出码 0 但输出含 %Error：$(basename "$f")"
            grep '%Error' "$log" >&2
            bad=1
        fi
        check_lint_off_whitelist "$f" || bad=1
    done
    [ "$bad" -eq 0 ] && ok "R1 lint 门（无 -Wall / 无 -Wno-fatal / 退出码 0 / 无 %Error / lint_off 白名单）"
    return "$bad"
}

# ── R2：实例端口名 ∈ 目标 module 端口集 ─────────────────────────────────────
r2_pin_names() {  # $1 = products dir
    local dir="$1" f log bad=0
    for f in "$dir"/*.v; do
        [ -f "$f" ] || continue
        log="$WORK/logs/$(basename "$f" .v)-r2.lint.log"
        lint_file "$f" "$log"
        if grep -qE 'PINNOTFOUND|PINMISSING' "$log"; then
            fail "R2 实例端口名不在目标 module 端口集：$(basename "$f")"
            grep -E 'PINNOTFOUND|PINMISSING' "$log" | head -5 >&2
            bad=1
        fi
    done
    [ "$bad" -eq 0 ] && ok "R2 实例端口名 ∈ 目标 module 端口集（无 PINNOTFOUND / PINMISSING）"
    return "$bad"
}

# ── R3：端口宽度一致（剥掉 lint_off 后重新 lint）─────────────────────────────
r3_widths() {  # $1 = products dir
    local dir="$1" f name stripped log bad=0
    mkdir -p "$WORK/strip"
    for f in "$dir"/*.v; do
        [ -f "$f" ] || continue
        name="$(basename "$f")"
        stripped="$WORK/strip/$name"
        sed -e '/verilator lint_off WIDTHEXPAND/d' -e '/verilator lint_off WIDTHTRUNC/d' \
            "$f" >"$stripped"
        log="$WORK/logs/${name%.v}-r3.lint.log"
        lint_file "$stripped" "$log"
        if [ "$LINT_RC" -ne 0 ] || grep -qE '%Warning|%Error' "$log"; then
            fail "R3 剥掉 lint_off 后出现宽度（或其它）告警：$name"
            grep -E '%Warning|%Error' "$log" | head -5 >&2
            bad=1
        fi
    done
    [ "$bad" -eq 0 ] && ok "R3 端口/信号宽度一致（剥掉 lint_off 后仍零告警）"
    return "$bad"
}

# ── R4：产物确定性（两次生成逐字节相同）─────────────────────────────────────
r4_determinism() {  # $1 = generator, $2 = reference dir
    local gen="$1" dir="$2" again="$WORK/determinism"
    gen_products "$gen" "$again" "$WORK/logs/r4-gen.log" || return 1
    if ! diff -r "$dir" "$again" >"$WORK/logs/r4.diff" 2>&1; then
        fail "R4 两次生成的产物不一致："
        sed -n '1,20p' "$WORK/logs/r4.diff" >&2
        return 1
    fi
    ok "R4 产物确定性（两次生成 diff -r 无差异）"
    return 0
}

# ── R5：跨模块 `ch_mem` 被拒 ────────────────────────────────────────────────
r5_reject_mem() {  # $1 = generator, $2 = scratch dir
    local gen="$1" dir="$2" log="$WORK/logs/r5.log" bad=0
    mkdir -p "$dir"
    "$gen" "$dir" --check-reject mem >"$log" 2>&1
    local rc=$?
    if [ "$rc" -ne 0 ]; then
        fail "R5 拒绝判据不成立（生成器 rc=$rc）"
        bad=1
    fi
    if ! grep -q 'cross-module ch_mem' "$log"; then
        fail "R5 拒绝消息未点名 'cross-module ch_mem'"
        sed -n '1,10p' "$log" >&2
        bad=1
    fi
    if [ -f "$dir/reject_mem.v" ]; then
        fail "R5 被拒绝的形态仍写出了文件"
        bad=1
    fi
    [ "$bad" -eq 0 ] && ok "R5 跨模块 ch_mem 被响亮拒绝（ok=false + 点名 cross-module ch_mem）"
    return "$bad"
}

# ── R6：module 树结构同构（独立提取）────────────────────────────────────────
r6_tree() {  # $1 = products dir
    local dir="$1" f name json tsv bad=0
    for f in "$dir"/*.v; do
        [ -f "$f" ] || continue
        name="$(basename "$f" .v)"
        rm -rf "$WORK/json/$name"
        mkdir -p "$WORK/json/$name"
        "$VERILATOR" --json-only --Mdir "$WORK/json/$name" "$f" \
            >"$WORK/logs/$name-r6.json.log" 2>&1
        if [ $? -ne 0 ]; then
            fail "R6 verilator --json-only 失败：$name"
            sed -n '1,20p' "$WORK/logs/$name-r6.json.log" >&2
            bad=1
            continue
        fi
        json="$(find "$WORK/json/$name" -name '*.tree.json' ! -name '*.meta.json' | head -1)"
        if [ -z "$json" ]; then
            fail "R6 未产出 .tree.json：$name"
            bad=1
            continue
        fi
        tsv="$(python3 "$EXTRACTOR" "$json")"
        printf '%s\n' "$tsv" >"$WORK/logs/$name.tsv"
        if ! python3 - "$name" "$WORK/logs/$name.tsv" <<'PY'
import sys

# 期望的 (parent_module, inst_name, module_name) 三元组 —— **由设计意图手写**，
# 不是从发射器导出（否则判据自证）。
EXPECTED = {
    "three_layers": [("m3_unit", "leaf", "l3_unit"), ("top", "mid", "m3_unit")],
    "row1_implicit": [("top", "inner", "inner_row1")],
    "bubble": [("bubble_mid", "leaf", "bubble_leaf"), ("top", "mid", "bubble_mid")],
}

name = sys.argv[1]
rows = [
    tuple(line.split("\t"))
    for line in open(sys.argv[2], encoding="utf-8").read().splitlines()
    if line.strip()
]
expected = EXPECTED.get(name)
if expected is None:
    print(f"R6 未知产物 '{name}' ⇒ 期望三元组表 MUST 同步更新", file=sys.stderr)
    sys.exit(1)

if rows != expected:
    print(f"R6 '{name}' 的 module 树与期望不一致", file=sys.stderr)
    print(f"  期望: {expected}", file=sys.stderr)
    print(f"  实际: {rows}", file=sys.stderr)
    sys.exit(1)

modules = {p for p, _, _ in rows} | {m for _, _, m in rows}
targets = [m for _, _, m in rows]
parents = {p for p, _, _ in rows}

dups = sorted({m for m in targets if targets.count(m) > 1})
if dups:
    print(f"R6 '{name}': module 被重复实例化 {dups}（MUST 每实例一份）", file=sys.stderr)
    sys.exit(1)

roots = parents - set(targets)
if len(roots) != 1:
    print(f"R6 '{name}': 根 module 不唯一 {sorted(roots)}", file=sys.stderr)
    sys.exit(1)
root = roots.pop()

children = {}
for p, _, m in rows:
    children.setdefault(p, []).append(m)

seen, stack = set(), [root]
while stack:
    cur = stack.pop()
    if cur in seen:
        print(f"R6 '{name}': 树中出现环（{cur} 被重复访问）", file=sys.stderr)
        sys.exit(2)
    seen.add(cur)
    stack.extend(children.get(cur, []))

unreached = sorted(modules - seen)
if unreached:
    print(f"R6 '{name}': 存在不可从根到达的 module {unreached}", file=sys.stderr)
    sys.exit(1)
sys.exit(0)
PY
        then
            fail "R6 结构同构判据失败：$name"
            bad=1
        fi
    done
    [ "$bad" -eq 0 ] && ok "R6 module 树结构同构（三元组逐字节 + 唯一根 + 各实例化一次 + 全可达）"
    return "$bad"
}

# ── R7：层次化产物路径 ≠ 扁平产物路径（仓内调用点静态扫描）───────────────────
r7_path_separation() {  # $1 = repo root
    local root="$1"
    python3 - "$root" <<'PY'
import os
import re
import sys

root = sys.argv[1]
HIER = re.compile(r"toVerilogHierarchical\s*\(")
# ⚠️ `toVerilog\s*\(` 天然不匹配 `toVerilogHierarchical(`（中间隔着 `Hier`）
FLAT = re.compile(r"toVerilog\s*\(")
LIT = re.compile(r'"([^"\n]*)"')
SKIP_DIRS = {".git", "build", "build-jit", "build-test", "third_party", "node_modules"}
EXTS = {".cpp", ".cc", ".cxx", ".h", ".hpp"}


def balanced(text, start):
    """从 `start`（指向 '(' 之后）取到匹配的 ')' 之前。"""
    depth, i = 1, start
    while i < len(text) and depth:
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        i += 1
    return text[start : i - 1]


def first_argument(arg_text):
    """按顶层逗号切出第一个实参（跳过字符串与嵌套括号）。"""
    depth, in_str, out = 0, False, []
    for c in arg_text:
        if in_str:
            out.append(c)
            if c == '"':
                in_str = False
            continue
        if c == '"':
            in_str = True
            out.append(c)
            continue
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif c == "," and depth == 0:
            break
        out.append(c)
    return "".join(out)


def literals_of_call(text, pattern):
    found = []
    for m in pattern.finditer(text):
        arg = first_argument(balanced(text, m.end()))
        lits = LIT.findall(arg)
        if lits:
            found.append(lits[-1])
    return found


def key(path_literal):
    """(目录, 文件名) —— 只有两者都相同才是「同一个文件」。"""
    d, _, b = path_literal.rpartition("/")
    return (d, b)


files_with_hier = 0
violations = []
for base, dirs, names in os.walk(root):
    dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith(".")]
    for name in names:
        if os.path.splitext(name)[1] not in EXTS:
            continue
        path = os.path.join(base, name)
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        if not HIER.search(text):
            continue
        files_with_hier += 1
        hier = literals_of_call(text, HIER)
        flat = set(map(key, literals_of_call(text, FLAT)))
        for lit in hier:
            if key(lit) in flat:
                violations.append(
                    f"{os.path.relpath(path, root)}: 层次化产物 '{lit}' 与同文件扁平产物同路径"
                )

if files_with_hier == 0:
    print("R7 扫描不到任何 toVerilogHierarchical( 调用点 ⇒ 判据空转（fail-closed）", file=sys.stderr)
    sys.exit(1)

if violations:
    print("R7 层次化产物路径与扁平产物路径冲突：", file=sys.stderr)
    for v in violations:
        print("  " + v, file=sys.stderr)
    sys.exit(1)

print(f"R7 OK（扫描 {files_with_hier} 个调用文件，无路径冲突）")
sys.exit(0)
PY
    local rc=$?
    [ "$rc" -eq 0 ] && ok "R7 层次化产物路径 ≠ 扁平产物路径（仓内调用点静态扫描）"
    return "$rc"
}

# ── R8：顶层端口表 == 手推期望集（产物级 · 结构化提取）────────────────────────
#    ⚠️ **为什么是「手推」而不是「从实现导出」**：期望集若抄实现输出 ⇒ 判据自证
#       （实现错 ⇒ 期望跟着错 ⇒ 恒绿）⇒ ⇒ 期望集 MUST 由**设计意图**逐条推导。
#    ⚠️ **提取方式** = `verilator --json-only` 的 `isPrimaryIO` + dtype 的 `range`
#       （复用 `scripts/lib/verilog_tree_extract.py --ports`）⇒ ⇒ 结构化提取
#       ⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器。
#    ⚠️ **它同时判掉「移出顶层」**：成层子树里的 `ch_out` MUST NOT 出现在顶层端口表
#       （期望集里没有它们 ⇒ 出现了就红）。
r8_port_set() {  # $1 = products dir
    local dir="$1" f name json bad=0
    for f in "$dir"/*.v; do
        [ -f "$f" ] || continue
        name="$(basename "$f" .v)"
        rm -rf "$WORK/json8/$name"
        mkdir -p "$WORK/json8/$name"
        "$VERILATOR" --json-only --Mdir "$WORK/json8/$name" "$f" \
            >"$WORK/logs/$name-r8.json.log" 2>&1
        if [ $? -ne 0 ]; then
            fail "R8 verilator --json-only 失败：$name"
            sed -n '1,20p' "$WORK/logs/$name-r8.json.log" >&2
            bad=1
            continue
        fi
        json="$(find "$WORK/json8/$name" -name '*.tree.json' ! -name '*.meta.json' | head -1)"
        if [ -z "$json" ]; then
            fail "R8 未产出 .tree.json：$name"
            bad=1
            continue
        fi
        python3 "$EXTRACTOR" "$json" --ports | awk -F'\t' '$1 == "top"' \
            >"$WORK/logs/$name-ports.tsv"
        if ! python3 - "$name" "$WORK/logs/$name-ports.tsv" <<'PY'
import sys

# 顶层端口表 —— **由设计意图手推**（不是抄实现输出 ⇒ 否则判据自证）：
#   顶层端口 = clock → reset → 无 driver 的 input → **根自己**的 ch_out
#   （成层子树里的 `ch_out` MUST NOT 上顶层 —— ADR-061 决策② 的两作用域表）
EXPECTED = {
    "three_layers": [
        ("default_clock", "INPUT", 1),
        ("default_reset", "INPUT", 1),
        ("top_unnamed_input", "INPUT", 8),
        ("top_unnamed_output", "OUTPUT", 8),
    ],
    "row1_implicit": [
        ("default_clock", "INPUT", 1),
        ("default_reset", "INPUT", 1),
        ("top_unnamed_input", "INPUT", 8),
        ("top_unnamed_output", "OUTPUT", 8),
    ],
    "bubble": [
        ("default_clock", "INPUT", 1),
        ("default_reset", "INPUT", 1),
        # ⚠️ 冒泡上来的悬空输入（ADR-060 §2.2）⇒ 顶层**可驱动** ⇒ 名字 = ctx 级全局名
        ("top_mid_leaf_unnamed_input", "INPUT", 8),
        ("top_unnamed_input", "INPUT", 8),
        ("top_unnamed_output", "OUTPUT", 8),
    ],
}

name = sys.argv[1]
rows = [
    (parts[1], parts[2], int(parts[3]))
    for parts in (line.split("\t") for line in
                  open(sys.argv[2], encoding="utf-8").read().splitlines() if line.strip())
    if len(parts) == 4
]
expected = EXPECTED.get(name)
if expected is None:
    print(f"R8 未知产物 '{name}' ⇒ 期望端口表 MUST 同步更新（fail-closed）", file=sys.stderr)
    sys.exit(1)

if rows != expected:
    print(f"R8 '{name}' 的顶层端口表与手推期望不一致", file=sys.stderr)
    print(f"  期望: {expected}", file=sys.stderr)
    print(f"  实际: {rows}", file=sys.stderr)
    sys.exit(1)
sys.exit(0)
PY
        then
            fail "R8 顶层端口表判据失败：$name"
            bad=1
        fi
    done
    [ "$bad" -eq 0 ] && ok "R8 顶层端口表 == 手推期望集（产物级 · verilator --json-only 结构化提取）"
    return "$bad"
}

# ── R9：父侧落点产物（族 ①/③ + 路径 B）过 lint 门 ───────────────────────────
#    ⚠️ **为什么需要这条**：第四态 `Broken`（「`ok=true` 但产物不可编译」）被
#    ADR-066 决策 ①/② 删除后，MUST 有**机械**判据证明「`ok=true` ⇒ 产物可编译」
#    ⇒ ⇒ 这 4 个 fixture 的产物 MUST 过 lint 门（**不是**只断言 `ok`）。
#    ⚠️ 判据强度：lint 门**同时**覆盖「父作用域不可解析」（verilator 报未声明标识符）
#    ⇒ ⇒ 这正是旧 `Broken` 形态的失败方式 ⇒ ⇒ 判据有判别力。
r9_parent_side_lint() {  # $1 = fixture dir
    local dir="$1" f name log bad=0
    local n
    n="$(find "$dir" -maxdepth 1 -name '*.v' | wc -l)"
    if [ "$n" -ne 4 ]; then
        fail "R9 父侧落点 fixture 应为 4 个 .v，实际 $n（目录：$dir）"
        return 1
    fi
    for f in "$dir"/*.v; do
        [ -f "$f" ] || continue
        name="$(basename "$f" .v)"
        log="$WORK/logs/$name-r9.lint.log"
        lint_file "$f" "$log"
        if [ "$LINT_RC" -ne 0 ]; then
            fail "R9 lint 失败（rc=$LINT_RC）：$name"
            sed -n '1,20p' "$log" >&2
            bad=1
            continue
        fi
        if grep -q '%Error' "$log"; then
            fail "R9 lint 退出码 0 但输出含 %Error：$name"
            grep '%Error' "$log" >&2
            bad=1
            continue
        fi
        # ⚠️ **落点网 MUST 真的存在**（防「判据空转」：若 fixture 退化成直通形态，
        #    lint 恒绿 ⇒ ⇒ 判据无效）⇒ ⇒ 父 module (`top`) MUST 声明一根
        #    **以 `top_leaf` 开头**的 `logic` 网（= 驱动端点的 ctx 全局名）。
        if ! awk '/^module top \(/,/^endmodule/' "$f" |
            grep -qE '^    logic .*top_leaf'; then
            fail "R9 产物缺父侧落点网声明（父 module 内无 top_leaf* 的 logic 声明）：$name"
            bad=1
        fi
    done
    [ "$bad" -eq 0 ] && ok "R9 父侧落点产物过 lint 门（族 ①/③ + 路径 B · 零 %Error · 落点网存在）"
    return "$bad"
}

# ── 主路径 ──────────────────────────────────────────────────────────────────
run_all() {
    require_tools || return 1
    check_lint_flags "${BASH_SOURCE[0]}" || return 1
    ok "R0 前置就位（verilator=$("$VERILATOR" --version 2>&1 | head -1)）"

    local products="$WORK/products"
    gen_products "$GENERATOR" "$products" "$WORK/logs/gen.log" || return 1
    ok "R0 产物生成（three_layers.v / row1_implicit.v / bubble.v）"

    r1_lint_gate "$products" || return 1
    r2_pin_names "$products" || return 1
    r3_widths "$products" || return 1
    r4_determinism "$GENERATOR" "$products" || return 1
    r5_reject_mem "$GENERATOR" "$WORK/reject" || return 1
    r6_tree "$products" || return 1
    r7_path_separation "$REPO_ROOT" || return 1
    r8_port_set "$products" || return 1

    # ⚠️ R9：父侧落点 fixture 走**独立目录**（主产物目录 MUST 保持「恰好 3 个 .v」）
    local psdir="$WORK/parent_side"
    rm -rf "$psdir" && mkdir -p "$psdir"
    if ! "$GENERATOR" "$psdir" --parent-side-fixtures "$psdir" \
        >"$WORK/logs/r9-gen.log" 2>&1; then
        fail "R9 父侧落点 fixture 生成失败"
        sed -n '1,20p' "$WORK/logs/r9-gen.log" >&2
        return 1
    fi
    r9_parent_side_lint "$psdir" || return 1
    return 0
}

# ── 自检：逐条原地投毒 ⇒ 必须变红 ⇒ 还原复绿 ────────────────────────────────
# 投毒**只改产物副本**（不改仓内源码）⇒ 每条的「还原」= 重新生成一份干净产物。
self_test() {
    require_tools || return 1
    local pass=0 total=0
    local products="$WORK/products"
    local psdir="$WORK/parent_side"

    reset_products() { gen_products "$GENERATOR" "$products" "$WORK/logs/gen.log" >/dev/null 2>&1; }
    # ⚠️ 父侧落点 fixture 的**独立目录**（主产物目录 MUST 保持「恰好 3 个 .v」）
    reset_parent_side() {
        rm -rf "$psdir" && mkdir -p "$psdir"
        "$GENERATOR" "$psdir" --parent-side-fixtures "$psdir" \
            >"$WORK/logs/r9-gen.log" 2>&1
    }

    expect_green() {  # $1 = label, $2 = function, rest = args
        total=$((total + 1))
        local label="$1"
        shift
        if "$@" >"$WORK/logs/st.out" 2>&1; then
            pass=$((pass + 1))
            echo "  ✅ $label ⇒ 绿"
        else
            echo "  ❌ $label：干净产物上判据未通过 —— 前置无效" >&2
            sed -n '1,10p' "$WORK/logs/st.out" >&2
        fi
    }
    expect_red() {  # $1 = label, $2 = function, rest = args
        total=$((total + 1))
        local label="$1"
        shift
        if "$@" >"$WORK/logs/st.out" 2>&1; then
            echo "  ❌ $label：投毒后判据仍绿 ⇒ 无判别力" >&2
        else
            pass=$((pass + 1))
            echo "  ✅ $label（投毒 ⇒ 红）"
        fi
    }

    # ---- 投毒 1：R1 —— 未声明标识符 -------------------------------------
    echo "[self-test] 1/10 R1：未声明标识符"
    reset_products
    python3 - "$products/three_layers.v" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
s = s.replace("    assign top_mid_leaf_ch_lit = 1'b0;",
              "    assign top_mid_leaf_ch_lit = 1'b0;\n    assign undeclared_wire = 1'b0;", 1)
open(p, "w").write(s)
PY
    expect_red "R1 未声明标识符" r1_lint_gate "$products"

    # ---- 投毒 2：R2 —— 改 pin 名 ---------------------------------------
    echo "[self-test] 2/10 R2：实例 pin 名不在目标端口集"
    reset_products
    python3 - "$products/row1_implicit.v" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
s = s.replace(".__if_0(top_unnamed_input)", ".__if_0_BROKEN(top_unnamed_input)", 1)
open(p, "w").write(s)
PY
    expect_red "R2 pin 名不存在" r2_pin_names "$products"

    # ---- 投毒 3：R3 —— 改端口宽度（R1 必须仍绿 ⇒ 证明 R3 不是 R1 的重复）----
    echo "[self-test] 3/10 R3：端口宽度不一致（lint_off 生效 ⇒ R1 看不见）"
    reset_products
    python3 - "$products/row1_implicit.v" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
s = s.replace("input [7:0] top_inner_unnamed_input", "input [8:0] top_inner_unnamed_input", 1)
open(p, "w").write(s)
PY
    expect_green "R3 投毒后 R1 仍绿（宽度告警被 lint_off 消音）" r1_lint_gate "$products"
    expect_red "R3 剥掉 lint_off 后转红" r3_widths "$products"

    # ---- 投毒 4：R4 —— 改一份产物（确定性）-----------------------------
    echo "[self-test] 4/10 R4：产物被外部篡改"
    reset_products
    printf '\n// tampered\n' >>"$products/bubble.v"
    expect_red "R4 产物不一致" r4_determinism "$GENERATOR" "$products"

    # ---- 投毒 5：R5 —— 假工具输出错消息 --------------------------------
    echo "[self-test] 5/10 R5：生成器假成功（不拒绝跨模块 mem）"
    cat >"$WORK/fake_gen" <<'SH'
#!/usr/bin/env bash
echo "products-ok: pretend"
exit 0
SH
    chmod +x "$WORK/fake_gen"
    expect_red "R5 假成功" r5_reject_mem "$WORK/fake_gen" "$WORK/reject_fake"

    # ---- 投毒 6：R6 —— 删一条实例化语句 --------------------------------
    echo "[self-test] 6/10 R6：删掉一条实例化语句（Verilog 仍合法）"
    reset_products
    python3 - "$products/three_layers.v" <<'PY'
import sys
p = sys.argv[1]
lines = open(p).read().splitlines(keepends=True)
out, i = [], 0
while i < len(lines):
    if lines[i].startswith("    m3_unit mid ("):
        while i < len(lines) and lines[i].rstrip() != "    );":
            i += 1
        i += 1          # 跳过 "    );"
        continue
    out.append(lines[i])
    i += 1
open(p, "w").write("".join(out))
PY
    expect_red "R6 树形同构（少一条边）" r6_tree "$products"

    # ---- 投毒 7：R7 —— 造同路径调用点 ----------------------------------
    echo "[self-test] 7/10 R7：同一文件里层次化产物与扁平产物同路径"
    rm -rf "$WORK/fake_repo"
    mkdir -p "$WORK/fake_repo/tests"
    cat >"$WORK/fake_repo/tests/poison.cpp" <<'CPP'
void f() {
    ch::toVerilog("out.v", ctx);
    ch::toVerilogHierarchical("out.v", root);
}
CPP
    expect_red "R7 同路径调用点" r7_path_separation "$WORK/fake_repo"

    # ---- 投毒 8：R1a —— 在 lint 调用里塞 -Wno-fatal ---------------------
    echo "[self-test] 8/10 R1a：lint 调用含 -Wno-fatal"
    cp "${BASH_SOURCE[0]}" "$WORK/poisoned_guard.sh"
    python3 - "$WORK/poisoned_guard.sh" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
s = s.replace('--lint-only --top-module top "$1"',
              '--lint-only -Wno-fatal --top-module top "$1"', 1)
open(p, "w").write(s)
PY
    expect_red "R1a -Wno-fatal 被拒" check_lint_flags "$WORK/poisoned_guard.sh"

    # ---- 投毒 9：R8 —— 改顶层端口的宽度（Verilog 仍合法 · R1 仍绿）--------
    echo "[self-test] 9/10 R8：顶层端口宽度被改（lint_off 消音 ⇒ R1 看不见）"
    reset_products
    python3 - "$products/three_layers.v" <<'PYX'
import sys
p = sys.argv[1]
s = open(p).read()
s = s.replace("    output [7:0] top_unnamed_output",
              "    output [3:0] top_unnamed_output", 1)
open(p, "w").write(s)
PYX
    expect_green "R8 投毒后 R1 仍绿（宽度告警被 lint_off 消音）" r1_lint_gate "$products"
    expect_red "R8 顶层端口宽度与手推期望不符" r8_port_set "$products"

    # ---- 投毒 10：R9 —— 改掉父侧落点网的**声明名** ⇒ 父作用域不可解析 ⇒ lint 必红
    echo "[self-test] 10/10 R9：父侧落点网被改名（父作用域不可解析）"
    reset_parent_side
    python3 - "$psdir/ps_expr_reg.v" <<'PYX'
import sys
p = sys.argv[1]
s = open(p).read()
# 只改**父 module** 里的那根落点网声明（子 module 里的同名声明不动）
head, sep, tail = s.partition("module top (")
assert sep, "no top module"
tail = tail.replace("    logic [7:0] top_leaf__reg;",
                    "    logic [7:0] top_leaf__reg_renamed;", 1)
open(p, "w").write(head + sep + tail)
PYX
    expect_red "R9 落点网改名后 lint 必红（父作用域不可解析）" r9_parent_side_lint "$psdir"

    echo
    if [ "$pass" -ne "$total" ]; then
        echo "❌ self-test FAIL（$pass/$total）" >&2
        return 1
    fi
    echo "✅ verify_hierarchical_rtl --self-test PASS（$pass/$total 条投毒对照全部生效）"
    return 0
}

case "${1:-}" in
"")
    run_all && echo "✅ verify_hierarchical_rtl: PASS（R1–R8）"
    exit $?
    ;;
--self-test)
    self_test
    exit $?
    ;;
*)
    echo "usage: bash scripts/verify_hierarchical_rtl.sh [--self-test]" >&2
    exit 2
    ;;
esac
