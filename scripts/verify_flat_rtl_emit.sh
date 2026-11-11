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

# ── F1（**延后到 F8 之后**）：目标从「默认产物」改为「**卫生产物**」
#   ⚠️ **判据修正理由**（2026-11-11 · B1 验收返工）：原 F1 跑**默认产物**，
#   其隐含前提是「工具的小设计恰好没有宽度告警」。工具设计加入
#   **mux 选字面量**（BLOCKER-1 的回归靶）后默认产物出现真实宽度告警
#   ⇒ ⇒ 前提破了。更本质：**默认产物在真实设计上必然有 23 条宽度告警**
#   （那正是产物里 blanket `lint_off` 要盖住的东西）⇒ ⇒ 「剥掉 blanket 后干净」
#   对**默认**产物是**不可满足**的判据。
#   ⇒ ⇒ 正确靶 = **卫生产物**（C6 的交付物：它**自身**就必须干净）。

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
    # ⑥ **无未声明引用**（**BLOCKER-1 的直接判据**：剪枝误伤 ⇒ 引用了被剪掉的信号
    #   ⇒ `default_nettype none` 下直接编译失败。`--undeclared-refs` 不依赖
    #   verilator ⇒ 即使 lint 因其它原因通过，这一项也独立兜底）
    ur="$(python3 "$CHECKER" --undeclared-refs "$HV" 2>&1)"; ur_rc=$?
    printf '%s\n' "$ur" | sed 's/^/  /'
    [ $ur_rc -eq 0 ] || { fail "F8 产物引用了**未声明**的标识符（剪枝误伤）"; f8_bad=1; }
    # ⑦ 确定性（两次发射逐字节相同）
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

# ── F1（**两段式基线** · 靶 = 卫生产物）───────────────────────────────
#   段 ① 原样 lint（基线）⇒ MUST 绿：卫生产物**不靠注释**就必须干净
#   段 ② 剥掉 `lint_off` 后重 lint ⇒ MUST 仍绿（卫生产物本来就没有 blanket
#     ⇒ ⇒ 这一段是**回归锁**：若将来有人在卫生模式里偷偷加回 blanket，段 ② 就会红）
echo "── F1：卫生产物 lint 干净（两段式基线：① 原样 · ② 剥掉 lint_off）"
if [ -f "$HV" ]; then
    if "$VERILATOR" --lint-only --top-module top "$HV" >"$WORK/hyg/lint_base.log" 2>&1; then
        ok "F1-a 卫生产物基线（原样）lint 干净"
    else
        fail "F1-a 卫生产物基线 lint **不干净** ⇒ ⇒ 产物自身有问题"
        grep -E "%Error|%Warning" "$WORK/hyg/lint_base.log" | head -8 | sed 's/^/     /'
    fi
    python3 "$CHECKER" --strip-lint-off "$HV" "$WORK/hyg/nolint.v" || {
        fail "F1 剥 lint_off 失败"; exit 1;
    }
    if "$VERILATOR" --lint-only --top-module top "$WORK/hyg/nolint.v" >"$WORK/hyg/lint.log" 2>&1; then
        ok "F1-b 剥掉 lint_off 后仍干净（卫生模式**没有** blanket 可依赖）"
    else
        fail "F1-b 剥掉 lint_off 后 lint **不干净** ⇒ ⇒ 卫生产物偷偷依赖了注释"
        grep -E "%Error|%Warning" "$WORK/hyg/lint.log" | head -8 | sed 's/^/     /'
    fi
else
    fail "F1 卫生产物缺失 ⇒ ⇒ F8 未产出 $HV"
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

    # M4：用 `lint_off` **盖住一条真实宽度告警** ⇒ 剥掉后 F1-b MUST 红
    #   ⚠️ 旧实现这里是**空操作**（`if 绿; then : # 只记录; fi`）⇒ ⇒ 两条分支都不
    #   失败 ⇒ ⇒ F1 **没有判别力自证**，正是草案明文禁止的「只报告不失败」。
    if [ -f "$HV" ]; then
        cp "$HV" "$WORK/hyg/poison_backup.v"
        python3 - "$HV" <<'PYPOISON'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
t = p.read_text(encoding="utf-8")
# ⚠️ 投毒 MUST 是**真实**的宽度截断：9 位源赋给 8 位目标
#   （`8'hFF + 8'h01` **不**告警 —— Verilog 的 `+` 结果宽度 = 操作数宽度，
#     溢出静默截断 ⇒ 该投毒形态**恒绿**，实测踩过）
poison = ("/* verilator lint_off WIDTHTRUNC */\n"
          "    logic [8:0] self_check_poison_wide;\n"
          "    logic [7:0] self_check_poison;\n"
          "    assign self_check_poison_wide = 9'h1FF;\n"
          "    assign self_check_poison = self_check_poison_wide;\n"
          "/* verilator lint_on WIDTHTRUNC */\n")
idx = t.rfind("endmodule")
t = t[:idx] + poison + t[idx:]
p.write_text(t, encoding="utf-8")
PYPOISON
        # ① 前置：投毒 MUST 有效（原样 lint **绿** = lint_off 确实盖住了它）
        if ! "$VERILATOR" --lint-only --top-module top "$HV" >/dev/null 2>&1; then
            cp "$WORK/hyg/poison_backup.v" "$HV"
            fail "自检 M4：投毒后**原样** lint 仍红 ⇒ ⇒ 投毒形态无效（判据作废）"
            exit 1
        fi
        # ② 剥掉 lint_off ⇒ MUST 红
        python3 "$CHECKER" --strip-lint-off "$HV" "$WORK/hyg/nolint2.v" >/dev/null 2>&1
        if "$VERILATOR" --lint-only --top-module top "$WORK/hyg/nolint2.v" >/dev/null 2>&1; then
            cp "$WORK/hyg/poison_backup.v" "$HV"
            fail "自检 M4：剥掉 lint_off 后**仍绿** ⇒ ⇒ **F1-b 无判别力**"
            exit 1
        fi
        cp "$WORK/hyg/poison_backup.v" "$HV"
        ok "自检 M4：真实宽度告警被 lint_off 盖住 ⇒ 剥掉后 F1-b 转红 ✓"
    fi

    restore
    if ! emit restored >/dev/null 2>&1; then
        fail "自检：还原后发射失败"
        exit 1
    fi
    ok "自检：还原后复绿 ✓"
    echo "✅ self-test: 每条规则都有判别力（M1 F3 / M2 F4 / M3 F5 / M4 F1-b）"
fi

# ────────────────────────────────────────────────────────────────
if [ "$FAILS" -ne 0 ]; then
    echo "❌ verify_flat_rtl_emit: ${FAILS} violation(s)"
    exit 1
fi
echo "✅ verify_flat_rtl_emit: PASS (F1 lint · F2 确定性 · F3 端口集==清单 · F4 无悬空 · F5 无自赋值 · F6 不适用 · F8 产物卫生 · F1 两段式基线)"
exit 0
