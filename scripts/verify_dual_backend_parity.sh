#!/usr/bin/env bash
# scripts/verify_dual_backend_parity.sh
#
# 双后端 parity 覆盖计数护栏（2026-10-30，planner 判据修订配套）。
#
# 为什么需要：
#   `objective-test-coverage-100pct` 的完成判据由「`ctest -L verilator_e2e` ≥ 100」
#   改为「**双后端 parity 源文件数 ≥ 60**」⇒ ⇒ 该判据 MUST 有**可机械核验**的锚点
#   ⇒ ⇒ 本脚本把「parity 源文件数」与根 `AGENTS.md` 的
#   `DUAL_BACKEND_PARITY_TOTAL: <N>` 行比对。
#
# 它防的是什么（与 `VERILATOR_E2E_TOTAL` 锚点**同型但更严**）：
#   * `VERILATOR_E2E_TOTAL` 防「带标签的条目数」静默缩面 —— 但**标签 ≠ parity**
#     （9 个契约锁定的**单后端**参考实现也带该标签）⇒ ⇒ 它**看不见** parity 缩面；
#   * 本脚本数的**是**真正带双后端 parity 的源文件 ⇒ ⇒ parity 缩面**看得见**。
#
# 判据的权威定义见 `.rddf/roadmap/objectives/objective-test-coverage-100pct.md` §2；
# 分析器与 fixture 自检**共用同一份**（`scripts/lib/dual_backend_parity.py`）⇒ 不会漂移。
#
# 用法：
#   scripts/verify_dual_backend_parity.sh            # 主路径
#   scripts/verify_dual_backend_parity.sh --self-test # fixture 自检（10 项）
#
# 退出码：0 = 通过；1 = 违规（计数漂移 / fail-closed 探针命中）。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
AGENTS="$REPO_ROOT/AGENTS.md"
ANALYZER="scripts/lib/dual_backend_parity.py"

if [ "${1:-}" = "--self-test" ]; then
    exec python3 "$ANALYZER" --self-test
fi

if [ ! -f "$AGENTS" ]; then
    echo "::warning::$AGENTS not found"
    exit 0
fi

if ! grep -qE 'DUAL_BACKEND_PARITY_TOTAL: [0-9]+' "$AGENTS"; then
    echo "::error::No DUAL_BACKEND_PARITY_TOTAL anchor in AGENTS.md"
    echo "::error::(add a line 'DUAL_BACKEND_PARITY_TOTAL: <N>' under NOTES §Test counts)"
    exit 1
fi

# 两个锚点：**广度**（源文件数）与**深度**（macro 调用数）
# ⚠️ 为什么两个都要：只钉「文件数」的话，**从有 3 个 dual pair 的文件里删掉
#    1 个 pair** ⇒ 文件仍被计数 ⇒ **护栏看不见**（实测：变异 M1 恒绿）
#    ⇒ ⇒ 深度锚点补上这个盲区。
declare -A ANCHORS=(
    [DUAL_BACKEND_PARITY_TOTAL]=PARITY_COUNT
    [DUAL_BACKEND_PARITY_PAIRS]=PARITY_PAIRS
)

out=$(python3 "$ANALYZER" --root . --build-dir "$BUILD_DIR" --check)
echo "$out"

violations=$(echo "$out" | grep -E '^VIOLATION ' || true)
if [ -n "$violations" ]; then
    echo "::error::dual-backend parity guard found violations:"
    echo "$violations" | sed 's/^/        /'
    exit 1
fi

rc=0
for anchor in DUAL_BACKEND_PARITY_TOTAL DUAL_BACKEND_PARITY_PAIRS; do
    metric=${ANCHORS[$anchor]}
    documented=$(grep -oE "$anchor: [0-9]+" "$AGENTS" | head -1 | grep -oE '[0-9]+$')
    if [ -z "$documented" ]; then
        echo "::warning::No $anchor anchor in AGENTS.md"
        continue
    fi
    actual=$(echo "$out" | grep -oE "^$metric=[0-9]+" | head -1 | grep -oE '[0-9]+$')
    if [ -z "$actual" ]; then
        echo "::error::analyzer produced no parseable $metric"
        rc=1
        continue
    fi
    if [ "$actual" != "$documented" ]; then
        echo "::error::AGENTS.md says $anchor=$documented but the analyzer reports $actual"
        echo "        Update root AGENTS.md NOTES §Test counts to match."
        rc=1
        continue
    fi
    echo "AGENTS.md $anchor ($documented) matches the analyzer ($actual) ✓"
done

exit $rc
