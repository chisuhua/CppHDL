#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# verify_jit_golden_coverage.sh —— JIT golden 覆盖下限（phase-6 **P8** / 路线 **R4.3**）
#
# ⚠️ **为什么要有它**：删掉 `set_ab_verification` 之后，运行期**没有** interpreter-vs-JIT
#   双跑校验了 ⇒ ⇒ JIT 的误编译只能靠 **golden 测试**抓（`tests/jit/test_jit_golden.cpp`
#   以**手推期望值**逐值断言）⇒ ⇒ 所以「golden 覆盖了哪些 JitOp」MUST 是**可判的**，
#   否则「覆盖」这件事只在人脑子里。
#
# ⚠️ **判据是集合，不是计数**：`5 / 39` 这种计数判据**恒绿** —— 把已覆盖的 `SELECT`
#   换成 `MUL` ⇒ 计数仍是 5 ⇒ 覆盖面漂移了而判据看不见。⇒ ⇒ 基线
#   `.rddf/checks/jit-golden-coverage.md` 记的是**集合**，比对**逐元素相等**。
#   ⇒ ⇒ 自检 **M2** 就是这条的判别力证明（计数不变、集合变 ⇒ 必红）。
#
# ⚠️ **全部 fail-closed**：派生不出结果 ⇒ exit 2（**不得**静默判绿）。
# ⚠️ **消息里 MUST NOT 含反引号** —— shell 双引号里的反引号会被当命令替换。
#
# 用法：
#   scripts/verify_jit_golden_coverage.sh [--self-test]
# ---------------------------------------------------------------------------

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/jit_golden_coverage.py"
ENUM_HDR="$REPO_ROOT/include/jit/jit_ir.h"
TEST_SRC="$REPO_ROOT/tests/jit/test_jit_golden.cpp"
BASELINE="$REPO_ROOT/.rddf/checks/jit-golden-coverage.md"

SELF_TEST=0
for arg in "$@"; do
    case "$arg" in
        --self-test) SELF_TEST=1 ;;
        *) echo "用法：scripts/verify_jit_golden_coverage.sh [--self-test]" >&2; exit 2 ;;
    esac
done

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0
ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS + 1)); }
note() { printf "  ${YELLOW}::notice %s${RESET}\n" "$*"; }

# ── R0：fail-closed ───────────────────────────────────────────────────────
echo "── R0：必需文件齐备（fail-closed）"
r0_bad=0
for f in "$ANALYZER" "$ENUM_HDR" "$TEST_SRC" "$BASELINE"; do
    if [ ! -f "$f" ]; then
        fail "R0 缺少 ${f#"$REPO_ROOT"/} ⇒ fail-closed"
        r0_bad=1
    fi
done
[ "$r0_bad" -eq 0 ] || exit 2
ok "R0 分析器 + 枚举头 + golden 源 + 基线 齐备"

# check_baseline <baseline-path> ⇒ 0 一致 / 1 不一致 / 2 解析失败
check_baseline() {
    python3 "$ANALYZER" --enum "$ENUM_HDR" --tests "$TEST_SRC" --baseline "$1"
}

# ── R1：与基线比对 ────────────────────────────────────────────────────────
echo "── R1：覆盖集合 vs 基线（逐元素相等）"
out="$(check_baseline "$BASELINE" 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
case "$rc" in
    0) ok "R1 覆盖集合与基线一致（分母 / 已覆盖集 / 未覆盖集）" ;;
    2) fail "R1 派生失败（exit=2）⇒ ⇒ **判据本身坏了**（fail-closed，不得判绿）" ;;
    *) fail "R1 覆盖集合与基线**不一致**（exit=$rc）⇒ ⇒ 见上方逐条差异" ;;
esac

# ── R2：自检（逐个投毒 ⇒ 必红）───────────────────────────────────────────
if [ "$SELF_TEST" -eq 1 ]; then
    echo "── R2：自检（投毒 ⇒ 必红）"
    WORK="$(mktemp -d "${TMPDIR:-/tmp/opencode}/jitcov.XXXXXX" 2>/dev/null)" || {
        echo "::error cannot create a temp dir (mktemp failed)"; exit 2;
    }
    trap 'rm -rf "$WORK"' EXIT INT TERM

    # poison <case> <python-mutation-code> ⇒ 生成 $WORK/<case>.md
    #   ⚠️ 投毒 MUST 后置断言「确实变了」⇒ 否则投毒无效 = 判据恒绿
    poison() {
        local case="$1" code="$2"
        cp "$BASELINE" "$WORK/$case.md"
        python3 - "$WORK/$case.md" <<PY
import re, sys
p = sys.argv[1]
t = open(p, encoding="utf-8").read()
before = t
$code
assert t != before, "投毒没有改变文件内容 ⇒ 投毒形态无效"
open(p, "w", encoding="utf-8").write(t)
PY
    }

    # poisons <case> <描述> —— 该 case MUST 判红
    poisons() {
        local case="$1" what="$2"
        if check_baseline "$WORK/$case.md" >/dev/null 2>&1; then
            fail "自检 $case：$what ⇒ 判据**仍绿** ⇒ ⇒ 无判别力"
        else
            ok "自检 $case：$what ⇒ 判据转红 ✓"
        fi
    }

    # M1：未覆盖集少一行（缺口被悄悄抹掉）
    poison m1_drop 't = re.sub(r", CALL_EXTERNAL, NOP", ", NOP", t)'
    poisons m1_drop "未覆盖集删掉一项"

    # M2：**计数不变**的替换（把已覆盖的 SELECT 换成未覆盖的 MUL）
    #     ⇒ 分母 39 / 已覆盖 5 / 未覆盖 34 全部不变 ⇒ 只有**集合**变了
    #     ⇒ ⇒ 计 数判据会恒绿；本判据 MUST 红
    poison m2_swap 't = t.replace("covered: LOAD_CONST, LOAD_DATA, REG_NEXT, SELECT, STORE_DATA",
            "covered: LOAD_CONST, LOAD_DATA, REG_NEXT, MUL, STORE_DATA")'
    poisons m2_swap "计数不变但已覆盖集被换掉一项（SELECT→MUL）"

    # M3：谎报覆盖（把一个实际没有 golden 的 op 写进 covered）
    poison m3_fake 't = t.replace("covered: LOAD_CONST, LOAD_DATA, REG_NEXT, SELECT, STORE_DATA",
            "covered: LOAD_CONST, LOAD_DATA, REG_NEXT, SELECT, STORE_DATA, MUL")'
    poisons m3_fake "谎报覆盖（covered 多一项 MUL）"

    # M4：分母漂移（枚举新增/删除 op 却没同步基线）
    poison m4_denom 't = t.replace("denominator: 39", "denominator: 38")'
    poisons m4_denom "分母漂移（39→38）"

    # M5：**派生面** fail-closed（不是基线投毒）——喂一个没有 TEST_CASE 的源文件
    #     ⇒ MUST exit 2（退出码 2 = 判据本身坏了），不得当成「一致」而判绿
    printf '// no test cases here\nint x;\n' > "$WORK/empty.cpp"
    python3 "$ANALYZER" --enum "$ENUM_HDR" --tests "$WORK/empty.cpp" --baseline "$BASELINE" >/dev/null 2>&1
    rc5=$?
    if [ "$rc5" -eq 2 ]; then
        ok "自检 M5：无 TEST_CASE 的源 ⇒ 派生 fail-closed（exit 2）✓"
    else
        fail "自检 M5：无 TEST_CASE 的源 ⇒ exit=$rc5（应为 2）⇒ ⇒ 派生面没有 fail-closed"
    fi

    if [ "$FAILS" -eq 0 ]; then
        echo "✅ self-test: 5 条投毒全部判别（M1 缺口 / M2 集合 / M3 谎报 / M4 分母 / M5 fail-closed）"
    fi
fi

# ─────────────────────────────────────────────────────────────────────────
if [ "$FAILS" -ne 0 ]; then
    echo "❌ verify_jit_golden_coverage: ${FAILS} violation(s)"
    exit 1
fi
echo "✅ verify_jit_golden_coverage: PASS (R0 fail-closed · R1 集合逐元素相等)"
exit 0
