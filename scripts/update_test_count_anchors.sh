#!/usr/bin/env bash
# scripts/update_test_count_anchors.sh
#
# **计数锚点的单一更新入口**（`examples-rtl-sim-coverage` **E8**）。
#
# ⚠️ **本脚本只做「同步」，不做「推导」**：
#   锚点（root `AGENTS.md` 的 `CTEST_TOTAL:` 等 5 行）本身是**静默缩面**的防线
#   —— 它们必须**留在文件里、可被人 review**。本脚本把「手工改 5 个数」
#   变成「跑一条命令」，但**不**把锚点改成自动推导（那会让缩面再次不可见）。
#
# ⚠️ **计数一律**复用既有脚本的口径（**不重算**）：
#   · CTEST_TOTAL / VERILATOR_E2E_TOTAL ⇒ `ctest -N` / `ctest -N -L verilator_e2e`
#   · DUAL_BACKEND_PARITY_TOTAL / PAIRS ⇒ `scripts/lib/dual_backend_parity.py`
#   · EXAMPLES_TEST_SOURCES ⇒ `scripts/lib/example_test_wiring.py`
#   （重算 = 第二套实现 ⇒ 必然与护栏漂移）
#
# 用法：
#   scripts/update_test_count_anchors.sh            # 就地更新 + 打印 diff
#   scripts/update_test_count_anchors.sh --check    # 只检查（CI 用）⇒ 不一致则 exit 1
#   scripts/update_test_count_anchors.sh --dry-run  # 只打印将改成什么
#
# 依赖：`build/`（ctest 计数）⇒ 不存在时**跳过**并 exit 0（CI 的 build-then-verify 顺序保证）

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
AGENTS="$REPO_ROOT/AGENTS.md"

MODE="apply"
case "${1:-}" in
--check) MODE="check" ;;
--dry-run) MODE="dry-run" ;;
"") ;;
*)
    echo "用法：$0 [--check|--dry-run]" >&2
    exit 2
    ;;
esac

if [ ! -f "$AGENTS" ]; then
    echo "::error::$AGENTS 不存在" >&2
    exit 2
fi

# -- 取实际值（复用既有口径）------------------------------------------------
actual_ctest=""
actual_ve2e=""
if [ -f "$BUILD_DIR/CTestTestfile.cmake" ]; then
    actual_ctest="$(ctest --test-dir "$BUILD_DIR" -N 2>/dev/null | tail -1 | grep -oE '[0-9]+' || echo "")"
    actual_ve2e="$(ctest --test-dir "$BUILD_DIR" -N -L verilator_e2e 2>/dev/null | tail -1 | grep -oE '[0-9]+' || echo "")"
else
    # ⚠️ **fail-closed**（旧实现静默 exit 0 ⇒ 「静默跳过」形态，与仓内
    #   `verify_arch_ledger_residuals.sh` 的「结算 fail-closed」纪律相反）
    if [ "$MODE" = "check" ]; then
        echo "::error::$BUILD_DIR 不存在 ⇒ \`--check\` 无法判定 CTest 锚点 ⇒ fail-closed" >&2
        exit 2
    fi
    echo "::warning::$BUILD_DIR 不存在 ⇒ 跳过 CTest 计数（apply/dry-run 模式允许）" >&2
fi

actual_parity_files=""
actual_parity_pairs=""
if [ -f "$REPO_ROOT/scripts/lib/dual_backend_parity.py" ]; then
    # 口径由那份库定义（护栏 `verify_dual_backend_parity.sh` 也用它）
    # ⇒ ⇒ 它打印 `PARITY_COUNT=<n>` / `PARITY_PAIRS=<n>`（**不**另立 CLI 选项）
    _parity_out="$(python3 "$REPO_ROOT/scripts/lib/dual_backend_parity.py" 2>/dev/null || true)"
    actual_parity_files="$(printf '%s\n' "$_parity_out" | grep -oE '^PARITY_COUNT=[0-9]+' | cut -d= -f2 || true)"
    actual_parity_pairs="$(printf '%s\n' "$_parity_out" | grep -oE '^PARITY_PAIRS=[0-9]+' | cut -d= -f2 || true)"
fi

actual_example_sources=""
if [ -x "$REPO_ROOT/scripts/verify_example_test_wiring.sh" ]; then
    # 口径由那个护栏定义 ⇒ ⇒ 解析它的「examples 测试源文件总数：<n> 个」
    _wiring_out="$(bash "$REPO_ROOT/scripts/verify_example_test_wiring.sh" 2>/dev/null || true)"
    actual_example_sources="$(printf '%s\n' "$_wiring_out" | grep -oE '测试源文件总数：[0-9]+' | head -1 | grep -oE '[0-9]+' || true)"
fi

# -- 逐锚点比较 --------------------------------------------------------------
drift=0
report=()
update_one() {
    local key="$1" actual="$2"
    if [ -z "$actual" ]; then
        report+=("  $key: (跳过 —— 取不到实际值)")
        return 0
    fi
    local documented
    documented="$(grep -oE "^[-*] ${key}: [0-9]+" "$AGENTS" | head -1 | grep -oE '[0-9]+$' || echo "")"
    if [ -z "$documented" ]; then
        report+=("  $key: (AGENTS.md 里没有该锚点 ⇒ 跳过)")
        return 0
    fi
    if [ "$documented" = "$actual" ]; then
        report+=("  $key: $documented ✓")
        return 0
    fi
    drift=1
    report+=("  $key: $documented → $actual")
    if [ "$MODE" = "apply" ]; then
        # ⚠️ 只改**那一行**的数字（锚点行格式固定 ⇒ 逐行精确替换，不用全局 sed）
        python3 - "$AGENTS" "$key" "$actual" <<'PY'
import re, sys, pathlib
path, key, actual = sys.argv[1], sys.argv[2], sys.argv[3]
p = pathlib.Path(path)
text = p.read_text(encoding="utf-8")
pattern = re.compile(r"^([-*] " + re.escape(key) + r": )(\d+)$", re.M)
new_text, n = pattern.subn(lambda m: m.group(1) + actual, text)
if n == 0:
    sys.exit(1)
p.write_text(new_text, encoding="utf-8")
PY
    fi
}

update_one CTEST_TOTAL "$actual_ctest"
update_one VERILATOR_E2E_TOTAL "$actual_ve2e"
update_one DUAL_BACKEND_PARITY_TOTAL "$actual_parity_files"
update_one DUAL_BACKEND_PARITY_PAIRS "$actual_parity_pairs"
update_one EXAMPLES_TEST_SOURCES "$actual_example_sources"

echo "── 计数锚点（脚本：$0 · 模式：$MODE）"
printf '%s\n' "${report[@]}"

if [ "$drift" -eq 0 ]; then
    echo "✅ 全部锚点与实际一致"
    exit 0
fi

if [ "$MODE" = "apply" ]; then
    echo "✅ 已更新（请 review diff 后提交）"
    exit 0
fi

if [ "$MODE" = "check" ]; then
    echo "::error::锚点与实际不一致 ⇒ 跑 \`scripts/update_test_count_anchors.sh\` 同步" >&2
    exit 1
fi

echo "(dry-run：未改动任何文件)"
exit 0
