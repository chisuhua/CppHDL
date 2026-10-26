#!/usr/bin/env bash
# scripts/verify_example_assertion_quality.sh
#
# 护栏：**examples 内的测试 MUST 真的断言**（不得只有 `SUCCEED`）。
#
# ⚠️ 与 `scripts/verify_example_test_wiring.sh` 的分工（2026-10-24 拆分）：
#    后者管「**归属守恒**」（测试源有没有被执行到），本脚本管「**断言质量**」
#    （被执行的测试有没有真的断言语义输出）。
#    拆分原因：C1（`riscv-mini-test-triage-2026-10`）在连续三轮对抗审稿
#    （第 4/5/6 轮）里，**全部** BLOCKER 都出在本护栏的静态分析机制上 ——
#    而那些机制**不可能被完全闭合**（任何自研的 C 子集词法分析都有新的绕过形态）。
#    ⇒ 归属守恒（简单、可审、portable）与断言质量（复杂、需长期演进）分开演进。
#
# ⚠️ 本脚本的静态分析**不可能完全闭合** —— 下述「能力边界」是**如实声明**，
#    不是「已覆盖」的承诺。任何扩展 MUST 先补 fixture（见 change
#    `example-assertion-quality-2026-10` 的 tasks）。
#
# ⚠️ **实现与规格共用一份分析器**（2026-10-26，change 的 T1.2）：
#    分析器（`analyze_source`）与 **fail-closed 判定**（`aq_analyze_status` /
#    `aq_violation_reason`）已抽到共享 lib：**scripts/lib/assertion_quality.sh**
#    ⇒ 本护栏与 fixture 自检（`scripts/verify_assertion_quality_fixtures.sh`）
#    不可能分叉。两份实现会漂移 —— 那正是 C1 连续三轮对抗审稿的 BLOCKER 来源。
#    ⇒ **改分析机制 MUST 同时改 lib + 期望表 + fixture**（三处一起）。
#
# Exit codes: 0 = 全部通过, 1 = 至少一处违规。
#
# Usage: bash scripts/verify_example_assertion_quality.sh
#
# ⚠️ 可移植性：只用 POSIX sh + awk（**不使用** GNU 扩展）。
#    ⚠️ **BSD/BWK awk（macOS 默认）未实测** —— 本机只有 mawk。
#    ⇒ 括号表达式内 MUST 用 `[[:space:]]`（POSIX 明确定义），
#      **不可**用 `[ \t]`（POSIX 未定义 ⇒ 各实现可能不同）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

violations=0
note()  { printf "  %s\n" "$1"; }
violation() { printf "${RED}VIOLATION${NC} %s\n" "$1" >&2; violations=$((violations+1)); }

# ═══════════════════════════════════════════════════════════════════════════
# C4 的锚点：examples 测试源文件的**可执行断言数下限**。
# 格式：<源文件>|<该文件保护的语义>|<可执行断言数下限>
#
# ⚠️ **C4 的能力边界（如实声明，勿夸张）**（第四轮对抗审稿 BLOCKER-1）：
#    它防的是**断言被移除**：删掉、被 `//` 注释掉、被 `/* */` 包住、
#    被 `#if 0` 条件编译掉 —— 这些都会让「可执行断言数」下降。
#    ⚠️ 它**不防**「**等量替换**」：把 `REQUIRE(pc == 0x80000004)` 换成
#    `REQUIRE(1 == 1)` 断言数**不变**，任何基于计数的守恒都发现不了
#    （这与 `CTEST_TOTAL` 对「等量替换一个测试」同样无能为力是同型局限）。
#    ⇒ 「等量替换」只能靠 **code review**；本锚点不假装能覆盖它。
#
# ⚠️ 为什么仍需要它：在 C4 之前，本护栏只有**粗粒度**计数（target 数 / 源文件数
#    / 退出码 / error 条数）⇒ 一个文件被删到只剩 1 条断言都不报警（实测：
#    `test_pipeline_exec` 52 → 1，`ctest -N` 仍 199、所有护栏 PASS）。C4 把
#    「每个文件至少有多少条断言」记下来，让**减**变成必须显式改锚点的行为
#    （机制与 `CTEST_TOTAL` 同型）。
#    只在**下调 / 删除条目**时需要解释（那是放宽）；上调无需理由。
# 归位条件：无（这是长期护栏）。新增测试源 MUST 在此登记一行。
# ═══════════════════════════════════════════════════════════════════════════
CH4_ANCHOR_README='C4 锚点：examples 测试源的可执行断言数下限。格式 <源文件>|<语义>|<下限>。下调/删除 MUST 在 commit message 里解释（放宽即需理由）。'
CH4_ANCHOR="examples/riscv-mini/tests/test_cache_smoke.cpp|ICache 冷 miss 透出 + DCache 未驱动端口的当前事实|3
examples/riscv-mini/tests/test_csr_unit.cpp|CSR 寄存器复位/读写/MRET/异常/WFI/中断|42
examples/riscv-mini/tests/test_ifstage_device.cpp|ch_device<IfStage> 的 PC 复位/推进/stall 冻结|2
examples/riscv-mini/tests/test_interrupt_flow.cpp|CLINT 定时器/CSR 中断条件/陷阱入口/MRET|28
examples/riscv-mini/tests/test_pipeline_debug.cpp|5 个 stage + hazard + pipeline 的逐组件行为断言|14
examples/riscv-mini/tests/test_pipeline_exec.cpp|流水线执行（指令级）|52
examples/riscv-mini/tests/test_pipeline_minimal.cpp|Rv32iPipeline 构造 + 复位期周期计数冻结|1
examples/riscv-mini/tests/test_pipeline_wrapper.cpp|Rv32iPipeline + ITCM 装配的取指/周期行为|5
examples/riscv-mini/tests/test_riscv_tests.cpp|riscv-tests 自包含用例 + 仓外二进制条件 SKIP（⚠️ 用 TEST_CASE_FOR_each 宏**生成**的 41 个用例的断言在宏体里 ⇒ 按「宏体整行不计」的保守规则不计入，故下限 = 8 个字面用例的 14）|14
examples/riscv-mini/tests/test_riscv_tests_pipeline.cpp|riscv-tests 经流水线跑（全部依赖仓外二进制）|47
examples/riscv-mini/tests/test_riscv_tests_soc.cpp|MMIO 地址解码（ITCM/DTCM/CLINT/UART/GPIO）|10
examples/riscv-mini/tests/test_rv32i_pipeline.cpp|流水寄存器 latch/stall/flush + hazard 前推（含 EX>MEM 双源优先级）+ 分支检测|23
examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp|SoC 级双路 parity（200 拍 × 27 观测值逐拍全等）+ 3 个 store 的 ISA 语义 golden + 分支/停顿计数 + 指令与 pc 对齐 + 编码器 × 手算常量交叉校验|24
examples/riscv-mini/tests/test_soc_hello_world.cpp|SoC 顶层 hello-world|8
examples/riscv-mini/tests/test_wrapper_ifstage.cpp|IfStage 的 PC 复位/IF-ID 锁存/stall 冻结|4
examples/riscv-mini/tests/test_wrapper_pipeline.cpp|Rv32iPipeline 直接包裹的周期行为|3
examples/riscv-mini/tests/test_wrapper_simple.cpp|HazardUnit 的 stall / EX 前推选择|4"

# ═══════════════════════════════════════════════════════════════════════════
# 源文件静态分析（`analyze_source`）与 **fail-closed 判定**已抽到共享 lib：
#     scripts/lib/assertion_quality.sh
# ⚠️ 抽 lib 的原因（2026-10-26，change `example-assertion-quality-2026-10` 的 T1.2）：
#    护栏与 **fixture 自检（= 规格）** MUST 用**同一份**分析器与同一套 fail-closed
#    判定 ⇒ 否则「实现」与「规格」会漂移（C1 连续三轮对抗审稿的 BLOCKER 全部出在
#    「两处对同一机制的两种理解」上）。分析器的**能力边界**声明也随实现一起搬进 lib，
#    避免「实现」与「边界声明」分处两地。
# ═══════════════════════════════════════════════════════════════════════════
. "$(dirname "$0")/lib/assertion_quality.sh"

# 活代码里的可执行断言总数
real_assert_count() {
    # ⚠️ `|| true`：分析器失败时 awk 无输出 ⇒ `n` 为空 ⇒ 由调用点的
    #    fail-closed 检查报违规（而不是让 `set -e` 在赋值处直接中止）。
    analyze_source "$1" 2>/dev/null | awk '/^A /{print $2; exit}' || true
}
# ---------------------------------------------------------------------------
# 源文件集合（与 verify_example_test_wiring.sh 的口径一致：4 种后缀 ×
# 全部 Catch2 用例宏 ∪ examples/**/tests/ 下的任意文件）
# ---------------------------------------------------------------------------
SRC_LIST="$(mktemp)"
trap 'rm -f "$SRC_LIST"' EXIT INT TERM
{
    grep -rlE '^[[:space:]]*(TEST_CASE|TEST_CASE_METHOD|TEMPLATE_TEST_CASE|TEMPLATE_PRODUCT_TEST_CASE|SCENARIO)[[:space:]]*\(' \
        examples --include='*.cpp' --include='*.cc' --include='*.cxx' --include='*.C' 2>/dev/null || true
    grep -rl 'CATCH_' examples --include='*.cpp' --include='*.cc' --include='*.cxx' --include='*.C' 2>/dev/null || true
    find examples -path '*/tests/*' -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.C' \) 2>/dev/null
} | sed 's|^\./||' | sort -u > "$SRC_LIST"

echo "=== verify_example_assertion_quality: examples 测试断言质量护栏 ==="
note "examples 测试源文件总数：$(wc -l < "$SRC_LIST" | tr -d ' ') 个"
# ⚠️ 源文件集合为空 ⇒ 两个 while 循环都不执行 ⇒ violations=0 ⇒ 假绿。
#    （例如 `examples/` 被改名/移动 ⇒ 枚举不到任何源文件。）
if [ ! -s "$SRC_LIST" ]; then
    violation "未枚举到任何 examples 测试源文件（examples/ 目录结构异常？）—— 空集合 MUST 视为违规"
fi

# ---------------------------------------------------------------------------
# C4 可执行断言数下限（防「断言被删除/注释掉/条件编译掉」；**不防**等量替换）+ C4b 逐用例下限
# ---------------------------------------------------------------------------
echo ""
echo '--- C4: 每个测试源的可执行断言数 MUST ≥ 锚点下限（防「删除/注释掉/#if 0 掉断言」；**不防**「等量替换」，见脚本头注释）---'
printf '%s\n' "$CH4_ANCHOR_README"
while IFS= read -r src; do
    [ -z "$src" ] && continue
    min="$(printf '%s\n' "$CH4_ANCHOR" | awk -F'|' -v f="$src" '$1==f {print $3; exit}')"
    if [ -z "$min" ]; then
        violation "$src 不在 CH4_ANCHOR 中 —— 新增测试源 MUST 登记一行（含真断言数下限）"
        continue
    fi
    as_out_full="$(analyze_source "$src" 2>/dev/null || true)"
    if ! printf '%s\n' "$as_out_full" | grep -q '^E '; then
        violation "$src 的断言分析**未完整执行**（缺少结束标记）—— 分析器中途失败 MUST 视为违规"
        continue
    fi
    n="$(real_assert_count "$src")"
    # ⚠️ MUST **fail-closed**：分析器失败时 `real_assert_count` 会返回**空串**，
    #    而 `[ "" -lt "$min" ]` 会打印 "integer expression expected" 并取**假**分支
    #    ⇒ 走进 else 打印 OK ⇒ violations 仍 0 ⇒ **PASS**（假绿！）。
    #    ⇒ 非数字计数 MUST 报违规（工具坏了 ≠ 没问题）。
    case "$n" in
        ''|*[!0-9]*)
            violation "$src 的可执行断言数统计**失败**（得到 '$n'）—— 分析器出错 MUST 视为违规，不得静默放行"
            continue
            ;;
    esac
    if [ "$n" -lt "$min" ]; then
        violation "$src 的可执行断言数（$n）< 锚点下限（$min）—— 断言被删除 / 注释掉 / 条件编译掉了？（下调下限是**放宽**，MUST 在 commit message 里解释）"
    else
        note "OK  $src → $n ≥ $min"
    fi
done < "$SRC_LIST"

# ---- C4b: 逐用例 MUST 至少 1 条可执行断言（delta 的「**每个**用例」措辞）----
# ⚠️ 为什么需要它（第五轮对抗审稿 BLOCKER-2）：文件级下限会被同文件**其它**用例
#    掩盖 —— 实测往 `test_pipeline_exec.cpp` 追加一个只有 `SUCCEED` 的 TEST_CASE，
#    文件级计数不变 ⇒ 护栏全绿，而 delta 写着「每个用例 MUST 至少断言一个语义输出」。
#    即：delta 里那条 MUST 曾经**零机制**。
# ⚠️ 能力边界：只看得见**字面**的用例宏；宏**生成**的用例（如 `test_riscv_tests.cpp`
#    的 `TEST_CASE_FOR_each`）不可见（其断言在宏体里，已被「宏体整行不计」排除）。
echo ""
echo "--- C4b: 逐用例 MUST ≥ 1 条可执行断言 ---"
while IFS= read -r src; do
    [ -z "$src" ] && continue
    # ⚠️ 只调 `analyze_source` **一次**（原先调 3 次：C4 的 real_assert_count + 这里 2 次）
    as_out="$(analyze_source "$src" 2>/dev/null || true)"
    ncases="$(printf '%s\n' "$as_out" | grep -c '^C ' || true)"
    nskip="$(printf '%s\n' "$as_out" | awk '/^S /{print $2; exit}')"
    zero="$(printf '%s\n' "$as_out" | awk '/^C / && $2 == 0 { $1=""; $2=""; sub(/^ +/, ""); print }')"
    # ⚠️ **fail-closed**（第八轮对抗审稿 BLOCKER-1）：C4b MUST 与 C4 一样，在
    #    **分析器半途失败**时报违规，而不是打出假 OK。
    #    失效形态（实测复现）：① `analyze_source` 返回空 ⇒ `as_out` 为空；
    #      ② awk 在输出 `A`/`S` 之后**出错退出** ⇒ `as_out` 非空但**没有结束标记 `E`**
    #      ⇒ 逐用例扫描根本没跑 ⇒ 原实现会打印「0 个字面用例，每个都 ≥ 1 条断言」
    #      且 `violations` 保持 0 ⇒ **假绿**（16 个文件全绿、exit 0）。
    # ⚠️ fail-closed 判定：**单一实现**（`aq_violation_reason`，与 fixture 自检共用）
    #    —— 空输出 / 缺结束标记 / 逐用例计数与 `E` 不符，三种失效形态都在这里判违规。
    aq_reason="$(aq_violation_reason "$src" "$as_out")"
    if [ -n "$aq_reason" ]; then
        violation "$aq_reason"
        continue
    fi
    if [ -n "$zero" ]; then
        while IFS= read -r z; do
            [ -n "$z" ] && violation "$src 的用例「$z」有 0 条可执行断言（只有 SUCCEED 之类）—— MUST 补断言或删除并登记"
        done <<EOF
$zero
EOF
    else
        note "OK  $src → $ncases 个字面用例，每个都 ≥ 1 条断言"
    fi
    # 被条件编译跳过的区段里有用例宏 ⇒ **点名**（不静默：它没被 C4b 检查到）
    if [ -n "$nskip" ] && [ "$nskip" -gt 0 ]; then
        printf "${YELLOW}NOTICE${NC} %s：有 %s 个用例宏位于被条件编译跳过的区段 ⇒ C4b 未检查它们（见脚本头注释 ②）\n" "$src" "$nskip"
    fi
done < "$SRC_LIST"

# stale：锚点条目对应的文件必须存在
while IFS='|' read -r fsrc fdesc fmin; do
    [ -z "$fsrc" ] && continue
    if [ ! -f "$fsrc" ]; then
        printf "${YELLOW}STALE${NC} CH4_ANCHOR 里的 $fsrc 已不存在 ⇒ MUST 移除该行\n"
        violations=$((violations+1))
    fi
done <<EOF
$CH4_ANCHOR
EOF
# ---------------------------------------------------------------------------
# 汇总
# ---------------------------------------------------------------------------
echo ""
if [ "$violations" -eq 0 ]; then
    echo "${GREEN}✅ verify_example_assertion_quality: PASS${NC}"
    exit 0
fi
echo "${RED}❌ verify_example_assertion_quality: $violations 处违规${NC}" >&2
exit 1
