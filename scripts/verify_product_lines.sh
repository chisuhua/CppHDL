#!/usr/bin/env bash
# scripts/verify_product_lines.sh
#
# **产品线元护栏**（ADR-078 · change `rv32i-full-product-line-2026-11` · S4c-0）
#
# ---------------------------------------------------------------------------
# 判据（`scripts/lib/product_lines.py` 实现 · 本文件是入口 + 投毒自证）
#
#   C1  发现集 == 登记集 —— marker 缺失 / 多出 / **生成物陈旧** ⇒ 红
#   C2  marker 完备性 —— 必填字段 / `layer` == ip / `line` == 目录名 /
#       `members` 每个目录存在 / `adrs` 的 ADR 文件存在 / `specs` 的 capability 存在
#       / **归属完备性**（`examples/` 下每个产品线成员目录 MUST 有归属）
#   C3  覆盖完备性 —— 每条产品线 MUST ≥1 个 ctest target；`examples/**` 下每个
#       ctest target MUST 归属**恰好一条**产品线
#   ⇒ ⇒ 「登记了但不覆盖」/「新 target 忘了归线」/「新建产品线却零测试」⇒ **当场红**
#
#   C4  `--self-test` —— 5 条投毒，**每条 MUST 转红**（否则判据无判别力）
#
# 为什么这样设计（`openspec/changes/rv32i-full-product-line-2026-11/design.md` §1）：
#   纯自动发现会「**静默不覆盖新线**」（本仓最恐惧的失败族）；手写中央登记表会在
#   多会话并行下冲突 ⇒ ⇒ marker（单点真相在各自目录）+ 本元护栏（发现集/归属/覆盖
#   三重闭合）两者都避开。
# ---------------------------------------------------------------------------
#
# 用法：
#   scripts/verify_product_lines.sh              # 判据（CI 用）
#   scripts/verify_product_lines.sh --self-test  # 5 条投毒自证

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'

MODE="check"
for arg in "$@"; do
    case "$arg" in
        --self-test) MODE="self-test" ;;
        -h|--help) sed -n '3,28p' "$0"; exit 0 ;;
        *) echo "用法：$0 [--self-test]" >&2; exit 2 ;;
    esac
done

LIB="scripts/lib/product_lines.py"
GEN="scripts/gen_product_lines.py"

run_judgement() {
    python3 "$LIB"
}

# ---------------------------------------------------------------------------
# 投毒自证（C4）
#
# ⚠️ 每条投毒 + 期望的**判别归属**：
#   投毒后 MUST **重生成登记面** —— 否则 C1 会先红，掩盖 C2/C3 的判别力
#   （⇒ ⇒ 投毒验证的是**该条判据**，不是「生成物陈旧」那条）。
# ⚠️ 还原 MUST 无论成败都执行（trap），且还原后 MUST 复测绿。
# ---------------------------------------------------------------------------
# ⚠️ 备份目录 MUST 是**脚本级**变量：EXIT trap 在 self_test 返回后执行，
#    而 `local` 变量那时已销毁 ⇒ ⇒ `set -u` 下会报 `unbound variable`
#    （实测：功能仍正确，但那是错误用法，会掩盖真正的 trap 故障）。
SELFTEST_BACKUP_DIR=""
SELFTEST_MARKER="examples/riscv-mini/.rddf-line.yaml"

self_test() {
    local fail=0
    SELFTEST_BACKUP_DIR="$(mktemp -d)"
    local mini_marker="$SELFTEST_MARKER"

    cleanup() {
        [ -n "${SELFTEST_BACKUP_DIR:-}" ] && cp -f "$SELFTEST_BACKUP_DIR/mini_marker" "$SELFTEST_MARKER" 2>/dev/null
        [ -n "${SELFTEST_BACKUP_DIR:-}" ] && rm -rf "$SELFTEST_BACKUP_DIR"
    }
    trap cleanup EXIT

    cp -f "$mini_marker" "$SELFTEST_BACKUP_DIR/mini_marker"

    poison() {  # $1=名字 $2=期望出现在输出里的关键字 $3=改动命令
        local name="$1" expect="$2" mutate="$3"
        bash -c "$mutate"
        python3 "$GEN" >/dev/null 2>&1 || true
        local out rc
        out="$(run_judgement 2>&1)"; rc=$?
        cleanup_marker
        python3 "$GEN" >/dev/null 2>&1 || true
        if [ "$rc" -eq 0 ]; then
            printf "${RED}❌ C4 投毒「%s」未转红（判据无判别力）${NC}\n" "$name" >&2
            fail=$((fail+1)); return
        fi
        if ! printf '%s' "$out" | grep -qF -- "$expect"; then
            printf "${RED}❌ C4 投毒「%s」转红但**原因不符**（期望含 '%s'）${NC}\n" "$name" "$expect" >&2
            printf '%s\n' "$out" | head -4 | sed 's/^/      /' >&2
            fail=$((fail+1)); return
        fi
        printf "${GREEN}✅ C4 投毒「%s」⇒ 红（含 '%s'）${NC}\n" "$name" "$expect"
    }

    cleanup_marker() { cp -f "$SELFTEST_BACKUP_DIR/mini_marker" "$mini_marker"; }

    # ① 改 line（≠ 目录名）
    poison "改 line" "!= 所在目录名" \
        "sed -i 's/^line: riscv-mini\$/line: riscv-mini-TAMPER/' $mini_marker"

    # ② 改 layer（MUST 是 ip）
    poison "改 layer" "MUST 是 \`ip\`" \
        "sed -i 's/^layer: ip\$/layer: soc/' $mini_marker"

    # ③ 断 members（指向不存在的目录）
    poison "断 members" "members\` 里的目录不存在" \
        "sed -i 's|^  - examples/riscv-soc\$|  - examples/no-such-dir|' $mini_marker"

    # ④ 断 adrs（指向不存在的 ADR）
    poison "断 adrs" "ADR 文件不存在" \
        "sed -i 's/^  - ADR-035\$/  - ADR-999/' $mini_marker"

    # ⑤ 删 marker ⇒ 发现集 != 登记集（C1）且该目录归属缺失（C2）
    poison "删 marker" "归属缺失" \
        "rm -f $mini_marker"

    # 基线复测
    if run_judgement >/dev/null 2>&1; then
        printf "${GREEN}✅ 还原后基线复测绿${NC}\n"
    else
        printf "${RED}❌ 还原后基线仍红（投毒未干净回滚）${NC}\n" >&2
        run_judgement 2>&1 | head -5 | sed 's/^/      /' >&2
        fail=$((fail+1))
    fi

    echo ""
    if [ "$fail" -eq 0 ]; then
        printf "${GREEN}✅ verify_product_lines --self-test: PASS（5 条投毒全部转红 + 还原复绿）${NC}\n"
        return 0
    fi
    printf "${RED}❌ verify_product_lines --self-test: %d 处失败${NC}\n" "$fail" >&2
    return 1
}

if [ "$MODE" = "self-test" ]; then
    self_test
    exit $?
fi

echo "--- 产品线元护栏（C1 发现集 / C2 marker 完备性 + 归属 / C3 覆盖完备性）---"
if run_judgement; then
    printf "${GREEN}✅ verify_product_lines: PASS${NC}\n"
    exit 0
fi
printf "${RED}❌ verify_product_lines: 见上方 VIOLATION${NC}\n" >&2
exit 1
