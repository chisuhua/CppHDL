#!/usr/bin/env bash
# scripts/verify_if_stmt_wiring.sh
#
# `_if` 语句块条件赋值的**静态护栏**（ADR-051 D6）。
#
# 为什么需要：
#   条件块内的**普通** `reg->next = v` / `io().x = v` **不会**被条件化
#   （⇒ **无条件**生效 ⇒ **静默**错误）⇒ ⇒ MUST 有静态护栏检出。
#   ⚠️ 这正是 ADR-051 修掉的那个缺陷的**同型**路径（旧实现里条件被彻底忽略）。
#
# 判据（**词法级**，方向为「宁可漏报、不误报」）：
#   R1 条件块分支体（`.then(<lambda>)`）内出现 `->next =` / `.next() =`
#      **且**该语句**未经** `conditional_assign` ⇒ 违规；
#   R2 条件块分支体内出现 `io().x =` / `io().x <<=` ⇒ 违规；
#   R3 被移除的符号（`conditional_reg_assign` / `priority_if` /
#      `priority_if_then`）**不得**再出现在源文件里。
#      ⚠️ 为何不能写成编译期断言：被删除的**自由函数**在 SFINAE 语境下是
#      **硬错误**（名称查找失败**不是** deduction failure）⇒ 同 ADR-050 实测②。
#
# 豁免：
#   * **禁用清单**里的 sample（`samples/disabled_samples.txt`）⇒ 它们**不被构建**
#     ⇒ 无运行期风险 ⇒ 以 `::notice` 点名（豁免**不是静默的**）；
#   * 护栏**自身**的文件（它们必然提到被移除的符号）。
#
# 用法：
#   scripts/verify_if_stmt_wiring.sh             # 主路径
#   scripts/verify_if_stmt_wiring.sh --self-test # fixture 自检（6 项）

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

RED=$'\033[0;31m'
GREEN=$'\033[0;32m'
YELLOW=$'\033[0;33m'
NC=$'\033[0m'

if [ "${1:-}" = "--self-test" ]; then
    exec python3 scripts/lib/if_stmt_wiring.py --self-test
fi

DISABLED_LIST="samples/disabled_samples.txt"

out=$(python3 scripts/lib/if_stmt_wiring.py --root .)
violations=$(echo "$out" | grep -E "^[a-z].*:[0-9]+: \[" || true)

# --- 豁免：禁用清单里的 sample（未被构建 ⇒ 无运行期风险）---
kept=""
notices=""
if [ -n "$violations" ]; then
    while IFS= read -r v; do
        [ -z "$v" ] && continue
        file=$(echo "$v" | cut -d: -f1)
        base=$(basename "$file" .cpp)
        if [ -f "$DISABLED_LIST" ] && grep -q "^${base}[[:space:]]" "$DISABLED_LIST"; then
            notices="${notices}${v}\n"
        else
            kept="${kept}${v}\n"
        fi
    done <<< "$violations"
fi

if [ -n "$notices" ]; then
    echo "::notice::以下违规位于**禁用清单**里的 sample（不被构建 ⇒ 无运行期风险）："
    printf "%b" "$notices" | sed 's/^/        /'
fi

if [ -n "$kept" ]; then
    echo "::error::verify_if_stmt_wiring: 发现违规："
    printf "%b" "$kept" | sed 's/^/        /'
    exit 1
fi

echo "  OK：条件块分支体内的寄存器 / IO 赋值均经由 conditional_assign 捕获；被移除的符号未再出现"
echo "${GREEN}✅ verify_if_stmt_wiring: PASS${NC}"
exit 0
