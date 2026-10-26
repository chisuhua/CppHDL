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
# 源文件静态分析：**活代码提取** + **逐用例断言计数**
#
# 输出三行：
#   `A <n>`            —— 活代码里的可执行断言总数
#   `S <n>`            —— 因条件编译被**跳过**的区段里出现的用例宏数（可见性用）
#   `C <n> <用例名>`   —— 每个用例体内的可执行断言数（每用例一行）
#
# ⚠️ 能力边界（如实声明；第四/五/六轮对抗审稿各纠正过一次，**勿再夸张**）：
#   ① **已覆盖**：整行/行尾/跨行块注释、字符串与字符字面量、
#      宏体（`#define` 行**整行不计**）、词边界（`MYREQUIRE(` / `XCHECK(` 不计）、
#      按**出现次数**计（同一行两条都算）、Catch2 的 throws/nothrow/STATIC 系断言宏。
#      条件编译：`#if <字面常量>` 求值；`#if 0` 区段跳过但**其 `#else` 之后的活代码计入**；
#      `#if 1` 之后的 `#elif` / `#else` **都是死的** ⇒ 跳过（`#elif` 曾漏判，第六轮 BLOCKER-1）；
#      `#if 0 / #elif 1` 的 `#elif` 体**是活的** ⇒ 计入。
#   ② **不覆盖（整组保守跳过）**：`#ifdef` / `#ifndef` / 非常量 `#if` —— **连同其
#      `#elif` / `#else` 一起跳过**（可能不编译 ⇒ 宁可少算）。
#      ⚠️ 第六轮 BLOCKER-2：此前只跳过 `#else` **之前**，恰好把**死**的那一半算进来
#      （`#ifdef X` 在 X 已定义的平台上，`#else` 正是死分支）⇒ 与「不会多算」的声明矛盾。
#      代价：本仓 `test_riscv_tests.cpp` 用 `TEST_CASE_FOR_each(...)` 宏**生成**的
#      41 个用例（宏体里的 `REQUIRE(passed == true)`）不进计数 ⇒ 该文件的
#      锚点是 14（8 个字面用例），不是 55。**这是已知且已声明的局限**。
#      被跳过区段里出现的用例宏由 `S` 计数**点名**（不再完全静默）。
#   ③ **不防「等量替换」**：把 `REQUIRE(pc == X)` 换成 `REQUIRE(1 == 1)` 断言数
#      不变 ⇒ 任何计数守恒都发现不了 ⇒ 由 **code review** 承担。
#      脚本 MUST NOT 声称覆盖它。
#
# ⚠️ 为什么「宏体整行不计」（第五轮对抗审稿 BLOCKER-1）：宏体**可能永不展开** ——
#    实测把一个 52 条 `REQUIRE` 的 `#define DEAD_NEVER_EXPANDED()` 追加到被掏空的
#    文件尾，就能把计数从 0 灌回 52 而护栏全绿。宏体里的断言因此 MUST NOT 计入
#    （保守方向：少算 ⇒ 只可能误报，不会漏报）。
#
# ⚠️ 为什么「逐用例」而不只是「逐文件」（第五轮 BLOCKER-2）：文件级下限会被同文件
#    其它用例掩盖 —— 实测往 `test_pipeline_exec.cpp` 追加一个只有 `SUCCEED` 的
#    TEST_CASE，文件级计数不变 ⇒ 护栏全绿，而 delta 里写着「**每个**用例 MUST
#    至少断言一个语义确定的输出」。
# ═══════════════════════════════════════════════════════════════════════════
analyze_source() {
    awk 'BEGIN {
    SQ = "\047"; inbc = 0; depth = 0; pending = ""; live = ""; nc = 0; sk = 0
    ASM = "(REQUIRE_THROWS_MATCHES|CHECK_THROWS_MATCHES|REQUIRE_THROWS_WITH|CHECK_THROWS_WITH|REQUIRE_THROWS_AS|CHECK_THROWS_AS|REQUIRE_NOTHROW|CHECK_NOTHROW|REQUIRE_THROWS|CHECK_THROWS|REQUIRE_FALSE|CHECK_FALSE|STATIC_REQUIRE|STATIC_CHECK|REQUIRE|CHECK)"
}
function total_skip(   i) {
    for (i = 1; i <= depth; i++) if (gskip[i]) return 1
    return 0
}
function eval_cond(c) {
    sub(/[[:space:]]+$/, "", c)
    if (c ~ /^[-+]?[0-9]+$/) return (c + 0 != 0) ? 1 : 0
    return 2
}
# 在任意文本里数「可执行断言」：断言宏 + 词边界（前驱不是标识符字符）
# ⚠️ 词边界实现：**不**用「交替组里的 `^`」（`(^|[^A-Za-z0-9_])`）——
#    POSIX 对「子表达式内的 `^` 是否仍是锚点」**未定义** ⇒ 不同 awk 实现
#    （尤其 BSD/BWK awk = macOS 默认）可能按字面字符处理 ⇒ 行为分叉。
#    改为在串首**垫一个哨兵空格**（空格不是标识符字符 ⇒ 满足词边界），
#    这样边界判定只需 `[^A-Za-z0-9_]`，全部落在 POSIX 明确定义的范围内。
function count_asserts(str,   cnt, s) {
    cnt = 0; s = " " str
    while (match(s, "[^A-Za-z0-9_]" ASM "[[:space:]]*\\(") > 0) {
        cnt++
        s = substr(s, RSTART + RLENGTH)
    }
    return cnt
}
{
    line = $0
    if (pending != "") line = pending line
    if (line ~ /\\[[:space:]]*$/) { sub(/\\[[:space:]]*$/, "", line); pending = line; next }
    pending = ""

    keep = ""; out = ""; i = 1; n = length(line); instr = 0; inchr = 0
    while (i <= n) {
        c = substr(line, i, 1); two = substr(line, i, 2)
        if (inbc) { if (two == "*/") { inbc = 0; i += 2 } else { i++ }; continue }
        if (instr) {
            if (c == "\\") { keep = keep substr(line, i, 2); i += 2; continue }
            if (c == "\"") instr = 0
            keep = keep c; i++; continue
        }
        if (inchr) {
            if (c == "\\") { keep = keep substr(line, i, 2); i += 2; continue }
            if (c == SQ) inchr = 0
            keep = keep c; i++; continue
        }
        if (two == "/*") { inbc = 1; i += 2; continue }
        if (two == "//") break
        if (c == "\"") { instr = 1; keep = keep c; i++; continue }
        if (c == SQ) { inchr = 1; keep = keep c; i++; continue }
        keep = keep c; out = out c; i++
    }

    # ---- 预处理条件状态机（规则见 guardrail 头注释）----
    if (out ~ /^[[:space:]]*#[[:space:]]*define([[:space:]]|$)/) next
    if (out ~ /^[[:space:]]*#[[:space:]]*if[[:space:]]/) {
        c = out; sub(/^[[:space:]]*#[[:space:]]*if[[:space:]]*/, "", c)
        r = eval_cond(c)
        depth++; gres[depth] = r
        gskip[depth] = (r == 1) ? 0 : 1
        gtaken[depth] = (r == 1)
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*(ifdef|ifndef)/) {
        depth++; gres[depth] = 2; gskip[depth] = 1; gtaken[depth] = 0
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*elif/) {
        if (depth > 0) {
            if (gres[depth] == 2 || gtaken[depth]) {
                gskip[depth] = 1
            } else {
                c = out; sub(/^[[:space:]]*#[[:space:]]*elif[[:space:]]*/, "", c)
                r = eval_cond(c)
                if (r == 1) { gskip[depth] = 0; gtaken[depth] = 1 } else { gskip[depth] = 1 }
            }
        }
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*else/) {
        if (depth > 0) gskip[depth] = (gres[depth] == 2 || gtaken[depth]) ? 1 : 0
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*endif/) { if (depth > 0) depth--; next }

    if (total_skip()) {
        # 被跳过的区段里若有用例宏 ⇒ 记一笔（让「不可见的用例」至少**可见**）
        if (keep ~ /^[[:space:]]*(TEST_CASE|TEST_CASE_METHOD|TEMPLATE_TEST_CASE|TEMPLATE_PRODUCT_TEST_CASE|SCENARIO)[[:space:]]*\(/) sk++
        next
    }

    if (keep ~ /^[[:space:]]*(TEST_CASE|TEST_CASE_METHOD|TEMPLATE_TEST_CASE|TEMPLATE_PRODUCT_TEST_CASE|SCENARIO)[[:space:]]*\(/) {
        if (match(keep, /"[^"]*"/)) {
            nm = substr(keep, RSTART + 1, RLENGTH - 2)
            nc++; casenames[nc] = nm
        }
    }
    live = live "\n" out
}
END {
    printf "A %d\n", count_asserts(live)
    printf "S %d\n", sk

    t = live; m = length(t); pos = 1; inbody = 0; bd = 0; cur = ""; cni = 0; body = ""
    while (pos <= m) {
        if (!inbody) {
            if (substr(t, pos, 9) == "TEST_CASE" || substr(t, pos, 10) == "TEMPLATE_T" || substr(t, pos, 8) == "SCENARIO") {
                cni++
                cur = (cni <= nc) ? casenames[cni] : "?"
                pos += 8; continue
            }
            if (substr(t, pos, 1) == "{" && cur != "") { inbody = 1; bd = 1; body = ""; pos++; continue }
            pos++; continue
        }
        ch = substr(t, pos, 1)
        if (ch == "{") { bd++; body = body ch }
        else if (ch == "}") {
            bd--
            if (bd == 0) { printf "C %d %s\n", count_asserts(body), cur; inbody = 0; cur = ""; body = "" }
            else body = body ch
        } else body = body ch
        pos++
    }
    # ⚠️ 结束标记：证明 awk **完整执行到底**（没在中间出错退出）。
    #    调用方据此判 fail-closed（分析器半途失败 ⇒ 无此标记 ⇒ 报违规）。
    printf "E %d\n", nc
}' "$1"
}

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
    if [ -z "$as_out" ]; then
        violation "$src 的断言分析**没有任何输出** —— 分析器失效 MUST 视为违规，不得静默放行"
        continue
    fi
    if ! printf '%s\n' "$as_out" | grep -q '^E '; then
        violation "$src 的断言分析**未完整执行**（缺少结束标记）—— 分析器中途失败 MUST 视为违规"
        continue
    fi
    # ⚠️ 结束标记**只证明 awk 跑到 END**，不证明**逐用例扫描真的产出了 `C` 行**。
    #    失效形态（第九轮对抗审稿 SHOULD_FIX）：源文件里含 raw string
    #    （`R"( ... )"`）⇒ 其中的裸 `{` / `"` 破坏扫描器的 brace-matching
    #    ⇒ 一行 `C` 都没产出 ⇒ 原实现会打「0 个字面用例，每个都 ≥ 1 条断言」并
    #    以 0 违规退出（假绿）。⇒ 逐用例计数 MUST 与 `E` 行携带的字面用例数**相等**。
    e_nc="$(printf '%s\n' "$as_out" | awk '/^E /{print $2; exit}')"
    if [ "$ncases" != "$e_nc" ]; then
        violation "$src 的逐用例计数（$ncases）与分析器报告的字面用例数（$e_nc）不符 —— 逐用例扫描**未完整执行**（如 raw string 破坏 brace-matching）MUST 视为违规"
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
