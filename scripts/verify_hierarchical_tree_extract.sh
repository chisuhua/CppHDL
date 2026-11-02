#!/usr/bin/env bash
# ============================================================================
# verify_hierarchical_tree_extract.sh
#
# 护栏：**「结构同构」判据的提取器**（层次化 RTL 输出方案 · C0-0 spike 的固化）
#
# ⚠️ **为什么需要它**：ADR-058 §5 的「结构同构」判据 MUST 能**独立于 emitter**
#    读出 RTL 的 module 树 ⇒ ⇒ 否则判据**自证**（分析层算错 ⇒ 两边一起错 ⇒
#    「计数对但树形错」这个 Scenario **永远不可触发**）。
#
# 三条判据：
#   **R0（fail-closed）**：verilator 与提取器 MUST 都可用 ⇒ 缺一即 **失败**
#        （⚠️ 仓内钉的 verilator 是 **5.048**，`--xml-only` 已被删除 ⇒ 用 `--json-only`）
#   **R1（效果）**：在**手写** 3 层 fixture 上 MUST 提出**恰好**期望的
#        `(parent_module, inst_name, module_name)` 三元组（逐字节相等）
#   **R2（判别力 · 负向对照）**：把 fixture 的**实例名**改坏 ⇒ 提取结果 MUST
#        **不再**等于期望 ⇒ ⇒ 证明提取器**不是恒绿**
#
# 用法：bash scripts/verify_hierarchical_tree_extract.sh
# ============================================================================
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURES="$REPO_ROOT/tests/fixtures/hierarchical_tree_extract"
EXTRACTOR="$REPO_ROOT/scripts/lib/verilog_tree_extract.py"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fail() { echo "❌ $1" >&2; exit 1; }
ok()   { echo "✅ $1"; }

# ── R0：fail-closed（verilator 与提取器都必须可用）──────────────────────────
VERILATOR=""
for cand in "$REPO_ROOT/build/verilator-install/bin/verilator" "$(command -v verilator || true)"; do
    [ -n "$cand" ] && [ -x "$cand" ] && { VERILATOR="$cand"; break; }
done
[ -n "$VERILATOR" ] || fail "R0 verilator 不可用（build/verilator-install/bin/verilator 或 PATH）"
[ -f "$EXTRACTOR" ] || fail "R0 提取器缺失：scripts/lib/verilog_tree_extract.py"
[ -f "$FIXTURES/three_layer.v" ] || fail "R0 fixture 缺失：tests/fixtures/hierarchical_tree_extract/three_layer.v"
[ -f "$FIXTURES/expected.tsv" ] || fail "R0 期望结果缺失：tests/fixtures/hierarchical_tree_extract/expected.tsv"

# 提取器必须是可执行的 python 脚本（fail-closed：解析失败即失败，不静默跳过）
python3 -c "import ast,sys; ast.parse(open(sys.argv[1],encoding='utf-8').read())" "$EXTRACTOR" \
    || fail "R0 提取器语法错误"
ok "R0 verilator=$("$VERILATOR" --version 2>&1 | head -1) · 提取器与 fixture 就位"

# ── 提取一次（缓存，供 R1/R2 复用）────────────────────────────────────────
extract_tree() {  # $1 = sv 文件目录 ⇒ 打印 TSV
    local sv_dir="$1" sv_file="$2" out_dir="$1/obj_dir"
    ( cd "$sv_dir" && "$VERILATOR" --json-only --Mdir obj_dir -Wno-fatal "$sv_file" ) \
        >"$sv_dir/verilator.log" 2>&1
    local json
    json="$(find "$out_dir" -name '*.tree.json' ! -name '*.meta.json' | head -1)"
    [ -n "$json" ] || { echo "--- verilator 输出 ---" >&2; cat "$sv_dir/verilator.log" >&2; return 1; }
    python3 "$EXTRACTOR" "$json"
}

# ── R1：效果判据（逐字节相等）──────────────────────────────────────────────
mkdir -p "$WORK/good" && cp "$FIXTURES/three_layer.v" "$WORK/good/"
ACTUAL="$(extract_tree "$WORK/good" three_layer.v)" || fail "R1 提取失败（见 $WORK/good/verilator.log）"
EXPECTED="$(cat "$FIXTURES/expected.tsv")"
if [ "$ACTUAL" != "$EXPECTED" ]; then
    echo "--- 期望 ---" >&2; printf '%s\n' "$EXPECTED" >&2
    echo "--- 实际 ---" >&2; printf '%s\n' "$ACTUAL" >&2
    fail "R1 提取结果与期望不一致"
fi
ok "R1 提取出期望的 2 条 (parent, instance, module) 三元组"

# ── R2：判别力（负向对照：改坏实例名 ⇒ 提取结果 MUST 变化）─────────────────
mkdir -p "$WORK/bad"
sed 's/u_leaf/u_leaf_BROKEN/' "$FIXTURES/three_layer.v" > "$WORK/bad/three_layer.v"
BAD="$(extract_tree "$WORK/bad" three_layer.v)" || fail "R2 负向对照的提取失败（⇒ 前置无效）"
if [ "$BAD" = "$EXPECTED" ]; then
    fail "R2 负向对照未生效（改坏实例名后结果不变 ⇒ 提取器恒绿 ⇒ 无判别力）"
fi
ok "R2 负向对照生效（改坏实例名 ⇒ 提取结果变化 ⇒ 提取器非恒绿）"

echo "✅ verify_hierarchical_tree_extract: PASS（R0/R1/R2）"
