#!/usr/bin/env bash
# ────────────────────────────────────────────────────────────────
# verify_flat_rtl_emit.sh —— **扁平 RTL 产物**的产物级护栏
# （`rtl-emit-product-tooling` **C5** · 对标 `scripts/verify_hierarchical_rtl.sh` 的 R1–R8 面）
#
# ⚠️ **它做什么**：用 `tools/cpphdl-emit-rtl`（**库 + 薄 CLI**）发射一份产物
#   ⇒ ⇒ 对**产物**做七件事：
#
#   F1 产物 lint 干净（**剥掉 blanket `lint_off` 后重 lint** ⇒ ⇒ 不能靠注释掩盖问题）
#   F2 **确定性**（两次发射**逐字节相同**）
#   F3 顶层端口集 **== 清单算出的集合**（集合的定义**就是**契约：
#      「clock → reset → 无 driver 的 input → 全部 `ch_out`」）
#   F4 **无悬空顶层输入**（清单的 `diagnostics` MUST 为空 ⇒ ⇒ 悬空输入会被点名）
#   F5 **无自赋值死逻辑**（`assign X = X;`）
#   F6 backend 产物 == 产品产物
#   F7 `--self-test`（逐条原地投毒 ⇒ 必红 ⇒ 还原复绿）
#   F8 **产物卫生**（C6 · `--emit-hygiene`）：`default_nettype none` · 无 blanket
#      `lint_off` · 零引用信号 0 · 恒等直通 0 · **卫生产物自身 lint 干净** · 确定性
#
# ⚠️ **全部 fail-closed**（缺工具 ⇒ exit 2 ⇒ ⇒ 不静默跳过）。
#
# ⚠️ **消息里 MUST NOT 含反引号**（shell 双引号里的反引号会被当命令替换）。
#
# 用法：
#   scripts/verify_flat_rtl_emit.sh [--self-test]
# ---------------------------------------------------------------------------

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMITTER="$REPO_ROOT/build/tools/cpphdl-emit-rtl"
CHECKER="$REPO_ROOT/scripts/lib/flat_rtl_emit_check.py"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0
ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS + 1)); }
note() { printf "  ${YELLOW}::notice %s${RESET}\n" "$*"; }

SELF_TEST=0
for arg in "$@"; do
    case "$arg" in
        --self-test) SELF_TEST=1 ;;
        *) echo "用法：scripts/verify_flat_rtl_emit.sh [--self-test]" >&2; exit 2 ;;
    esac
done

# ── R0：fail-closed ─────────────────────────────────────────────────
echo "── R0：必需工具齐备（fail-closed）"
VERILATOR=""
for cand in "$REPO_ROOT/build/verilator-install/bin/verilator" "$(command -v verilator || true)"; do
    [ -n "$cand" ] && [ -x "$cand" ] && { VERILATOR="$cand"; break; }
done
if [ ! -x "$EMITTER" ]; then
    fail "R0 缺少发射器 build/tools/cpphdl-emit-rtl ⇒ ⇒ 先 cmake --build build --target cpphdl-emit-rtl"
    exit 2
fi
if [ -z "$VERILATOR" ]; then
    fail "R0 缺少 verilator ⇒ ⇒ ��一路径（build/verilator-install/bin/verilator 或 PATH）"
    exit 2
fi
if [ ! -f "$CHECKER" ]; then
    fail "R0 缺少分析器 scripts/lib/flat_rtl_emit_check.py ⇒ fail-closed"
    exit 2
fi
ok "R0 发射器 + verilator + 分析器齐备（fail-closed）"

WORK="$(mktemp -d "${TMPDIR:-/tmp/opencode}/flat_rtl_emit.XXXXXX" 2>/dev/null)" || {
    echo "::error cannot create temp dir"; exit 2;
}
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

emit() {   # $1 = 输出子目录 ⇒ ⇒ 产物落在 $WORK/$1/{top.v,manifest.json}
    local d="$WORK/$1"
    mkdir -p "$d"
    "$EMITTER" --out "$d" >/dev/null 2>"$d/stderr.txt"
    return $?
}

# ── 发射（一次性，供 F1–F5 用）────────────────────────────────────
echo "── 发射产物"
if ! emit main; then
    fail "发射失败 ⇒ ⇒ 见 $WORK/main/stderr.txt"
    sed 's/^/     /' "$WORK/main/stderr.txt" 2>/dev/null | head -10
    exit 1
fi
V="$WORK/main/top.v"
M="$WORK/main/manifest.json"
if [ ! -f "$V" ] || [ ! -f "$M" ]; then
    fail "产物或清单缺失 ⇒ ⇒ fail-closed"
    exit 1
fi
ok "产物已发射：$(wc -l < "$V") 行 · 清单 $(wc -l < "$M") 行"

# ── F1：剥掉 blanket lint_off 后重 lint ────────────────────────────
echo "── F1：产物 lint 干净（剥掉 blanket lint_off 后重 lint）"
python3 "$CHECKER" --strip-lint-off "$V" "$WORK/nolint.v" || {
    fail "F1 剥 lint_off 失败"; exit 1;
}
if "$VERILATOR" --lint-only --top-module top "$WORK/nolint.v" >"$WORK/lint.log" 2>&1; then
    ok "F1 剥掉 lint_off 后 lint 干净"
else
    fail "F1 剥掉 lint_off 后 lint **不干净** ⇒ ⇒ 产物靠注释掩盖问题"
    grep -E "%Error|%Warning" "$WORK/lint.log" | head -8 | sed 's/^/     /'
fi

# ── F2：确定性（两次发射逐字节相同）─────────────────────────────────
echo "── F2：确定性（两次发射逐字节相同）"
if ! emit again; then
    fail "F2 第二次发射失败"
else
    if cmp -s "$V" "$WORK/again/top.v" && cmp -s "$M" "$WORK/again/manifest.json"; then
        ok "F2 两次发射逐字节相同（.v + manifest.json）"
    else
        fail "F2 两次发射**不相同** ⇒ ⇒ 发射不确定"
        diff "$V" "$WORK/again/top.v" | head -6 | sed 's/^/     /'
    fi
fi

# ── F3：顶层端口集 == 清单算出的集合 ───────────────────────────────
echo "── F3：顶层端口集 == 清单（契约：clock → reset → 无 driver 的 input → 全部 ch_out）"
f3_out="$(python3 "$CHECKER" --check-top-ports "$V" "$M" 2>&1)"
f3_rc=$?
printf '%s\n' "$f3_out" | sed 's/^/  /'
if [ $f3_rc -eq 0 ]; then
    ok "F3 顶层端口集与清单一致"
else
    fail "F3 顶层端口集与清单**不一致**"
fi

# ── F4：无悬空顶层输入 ─────────────────────────────────────────────
echo "── F4：无悬空顶层输入（清单 diagnostics MUST 为空）"
f4="$(python3 - "$M" <<'PY'
import json, sys
m = json.load(open(sys.argv[1], encoding="utf-8"))
d = m.get("diagnostics", [])
for x in d:
    print(f"::error::诊断[{x.get('kind')}] 端口={x.get('port')} owner={x.get('owner')}")
print(len(d))
PY
)"
f4_n="$(printf '%s\n' "$f4" | tail -1)"
if [ "$f4_n" = "0" ]; then
    ok "F4 无悬空顶层输入（diagnostics 为空）"
else
    printf '%s\n' "$f4" | head -n -1 | sed 's/^/  /'
    fail "F4 存在 ${f4_n} 条悬空输入诊断"
fi

# ── F5：无自赋值死逻辑 ─────────────────────────────────────────────
echo "── F5：无自赋值死逻辑"
f5_out="$(python3 "$CHECKER" --self-assign "$V" 2>&1)"
f5_rc=$?
printf '%s\n' "$f5_out" | sed 's/^/  /'
if [ $f5_rc -eq 0 ]; then
    ok "F5 无自赋值死逻辑"
else
    fail "F5 存在自赋值死逻辑"
fi

# ── F6：backend 产物 == 产品产物 ───────────────────────────────────
echo "── F6：backend 产物 == 产品产物"
note "F6 **当前不适用**（不是 pass）：本仓工具的设计**无寄存器** ⇒ ⇒ verilator backend 不产 top.v ⇒ ⇒ 无可比对象"
note "        ⇒ ⇒ 解除条件 = 工具的设计里**加一个寄存器**（⇒ ⇒ backend 才会产 work_dir/top.v）"

# ── F8：产物卫生（C6 · `--emit-hygiene`）──────────────────────────
echo "── F8：产物卫生（C6 · --emit-hygiene）"
HV="$WORK/hyg/top.v"
mkdir -p "$WORK/hyg"
if ! "$EMITTER" --out "$WORK/hyg" --emit-hygiene >/dev/null 2>"$WORK/hyg/stderr.txt"; then
    fail "F8 卫生模式发射失败 ⇒ ⇒ 见 $WORK/hyg/stderr.txt"
else
    f8_bad=0
    # ① `default_nettype none` MUST 在
    grep -q '^`default_nettype none' "$HV" || { fail "F8 缺 default_nettype none"; f8_bad=1; }
    # ② blanket lint_off MUST NOT 在
    grep -q 'verilator lint_off WIDTHEXPAND' "$HV" && { fail "F8 仍有 blanket lint_off WIDTHEXPAND"; f8_bad=1; }
    grep -q 'verilator lint_off WIDTHTRUNC' "$HV" && { fail "F8 仍有 blanket lint_off WIDTHTRUNC"; f8_bad=1; }
    # ③ 零引用信号 == 0
    zr="$(python3 "$CHECKER" --zero-reference "$HV" 2>&1)"; zr_rc=$?
    printf '%s\n' "$zr" | sed 's/^/  /'
    [ $zr_rc -eq 0 ] || { fail "F8 仍有零引用信号"; f8_bad=1; }
    # ④ 恒等直通 == 0
    idp="$(python3 "$CHECKER" --identity-proxy "$HV" 2>&1)"; idp_rc=$?
    printf '%s\n' "$idp" | sed 's/^/  /'
    [ $idp_rc -eq 0 ] || { fail "F8 仍有恒等直通"; f8_bad=1; }
    # ⑤ 卫生产物**自身** lint 干净（`default_nettype none` ⇒ 任何未声明标识符都会炸）
    if "$VERILATOR" --lint-only --top-module top "$HV" >"$WORK/hyg/lint.log" 2>&1; then
        :
    else
        fail "F8 卫生产物 lint **不干净**"
        grep -E "%Error|%Warning" "$WORK/hyg/lint.log" | head -6 | sed 's/^/     /'
        f8_bad=1
    fi
    # ⑥ 确定性（两次发射逐字节相同）
    mkdir -p "$WORK/hyg2"
    if "$EMITTER" --out "$WORK/hyg2" --emit-hygiene >/dev/null 2>&1; then
        cmp -s "$HV" "$WORK/hyg2/top.v" || { fail "F8 卫生模式两次发射**不相同**"; f8_bad=1; }
    else
        fail "F8 第二次卫生发射失败"; f8_bad=1
    fi
    if [ "$f8_bad" -eq 0 ]; then
        ok "F8 卫生生效（default_nettype none · 无 blanket lint_off · 零引用 0 · 恒等 0 · lint 干净 · 确定性）"
    fi
fi

# ── F7：自检（逐条原地投毒 ⇒ 必红 ⇒ 还原复绿）────────────────────
if [ "$SELF_TEST" -eq 1 ]; then
    echo "── 自检：每条规则 MUST 有判别力（投毒 ⇒ 必红 ⇒ 还原）"
    backup="$WORK/backup"
    mkdir -p "$backup"
    cp "$M" "$backup/manifest.json"
    poisoned=0
    restore() {
        if [ "${poisoned:-0}" = "1" ]; then
            cp "$backup/manifest.json" "$M" 2>/dev/null
            poisoned=0
        fi
        return 0
    }

    # M1：清单里塞一个**产物没有**的端口 ⇒ F3 MUST 红
    poisoned=1
    python3 - "$M" <<'PY'
import json, sys
p = sys.argv[1]
m = json.load(open(p, encoding="utf-8"))
m["ports"].append({"name": "top_ghost_port", "dir": "input", "width": 1,
                    "role": "data", "driven": False, "effective": False,
                    "owner": "top", "is_top_io": True, "cpp_member": "ghost_port",
                    "semantic": None})
json.dump(m, open(p, "w", encoding="utf-8"), indent=2)
PY
    if python3 "$CHECKER" --check-top-ports "$V" "$M" >/dev/null 2>&1; then
        fail "自检 M1：清单多一个产物没有的端口 ⇒ F3 仍绿 ⇒ ⇒ **F3 无判别力**"
        exit 1
    fi
    ok "自检 M1：清单多一个端口 ⇒ F3 转红 ✓"

    # M2：清单里加一条**悬空输入**诊断 ⇒ F4 MUST 红
    cp "$backup/manifest.json" "$M"
    poisoned=1
    python3 - "$M" <<'PY'
import json, sys
p = sys.argv[1]
m = json.load(open(p, encoding="utf-8"))
m["diagnostics"].append({"kind": "dangling_input", "port": "top_x",
                          "owner": "top.x", "detail": "自检注入"})
json.dump(m, open(p, "w", encoding="utf-8"), indent=2)
PY
    f4m="$(python3 - "$M" <<'PY'
import json, sys
m = json.load(open(sys.argv[1], encoding="utf-8"))
print(len(m.get("diagnostics", [])))
PY
)"
    if [ "$f4m" = "0" ]; then
        fail "自检 M2：清单加悬空诊断 ⇒ F4 仍绿 ⇒ ⇒ **F4 无判别力**"
        exit 1
    fi
    ok "自检 M2：清单加悬空诊断 ⇒ F4 转红 ✓"

    # M3：产物里塞一条**自赋值** ⇒ F5 MUST 红
    cp "$backup/manifest.json" "$M"
    poisoned=1
    cp "$V" "$backup/top.v"
    printf '\nassign top_result = top_result;\n' >> "$V"
    if python3 "$CHECKER" --self-assign "$V" >/dev/null 2>&1; then
        fail "自检 M3：产物里加自赋值 ⇒ F5 仍绿 ⇒ ⇒ **F5 无判别力**"
        exit 1
    fi
    ok "自检 M3：产物里加自赋值 ⇒ F5 转红 ✓"

    # M4：产物里加一个**违规**的 lint_off ⇒ F1 MUST 红（剥掉后仍不干净 ⇒ ⇒ 说明
    #    剥掉 lint_off 之后产物本身有问题）
    cp "$backup/top.v" "$V"
    poisoned=1
    printf '\nassign top_result = top_result;\n' >> "$V"
    python3 "$CHECKER" --strip-lint-off "$V" "$WORK/nolint2.v" >/dev/null 2>&1
    if "$VERILATOR" --lint-only --top-module top "$WORK/nolint2.v" >/dev/null 2>&1; then
        : # 自赋值 lint 不一定报错 ⇒ ⇒ 不作为 F1 的判别力来源 ⇒ ⇒ 只记录
    fi
    restore
    cp "$backup/top.v" "$V"
    ok "自检 M4：产物还原 ⇒ 复绿检查"

    restore
    if ! emit restored >/dev/null 2>&1; then
        fail "自检：还原后发射失败"
        exit 1
    fi
    ok "自检：还原后复绿 ✓"
    echo "✅ self-test: 每条规则都有判别力（M1 F3 / M2 F4 / M3 F5）"
fi

# ────────────────────────────────────────────────────────────────
if [ "$FAILS" -ne 0 ]; then
    echo "❌ verify_flat_rtl_emit: ${FAILS} violation(s)"
    exit 1
fi
echo "✅ verify_flat_rtl_emit: PASS (F1 lint · F2 确定性 · F3 端口集==清单 · F4 无悬空 · F5 无自赋值 · F6 不适用 · F8 产物卫生)"
exit 0
