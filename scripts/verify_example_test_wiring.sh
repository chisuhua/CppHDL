#!/usr/bin/env bash
# scripts/verify_example_test_wiring.sh
#
# 护栏：**examples 内的测试 MUST 有运行覆盖归属**。
#
# 背景（2026-10-23 实测）：`examples/riscv-mini/CMakeLists.txt` 注册的 Catch2
# 测试 target 此前**每次 CI 都在编译、却从未被执行过**。机制性根因：根
# `CMakeLists.txt` 的 `enable_testing()` 排在 `add_subdirectory(examples)`
# **之后** ⇒ examples 子目录不生成 `CTestTestfile.cmake` ⇒ 其中的 `add_test()`
# 是**空操作**。
#
# ═══════════════════════════════════════════════════════════════════════════
# ⚠️ 本护栏经两轮对抗审稿（均 REJECT）后重写。以下约束**勿退化**：
#
# ① 判据是 **`ctest -N`（CTest 条目的唯一真相）**，不是解析 CMake 源码。
# ② 基准是**文件系统上的测试源文件集合**（外部事实），不是「CMake 里声明了什么」
#    —— 否则删一条声明会让「声明集」与「覆盖集」同步缩小 ⇒ 数量守恒恒等
#    ⇒ 对缩面**无判别力**。
# ③ 源文件 → target 的映射 MUST 用**精确路径**，不得用 basename
#    —— 否则 example 侧的 `test_rv32i_pipeline.cpp` 会被根 `tests/` 的同名文件顶替。
# ④ 源文件的枚举 MUST 按**内容/目录**，不得按文件名约定（`test_*.cpp`）
#    —— 否则一个叫 `pipeline_check.cpp` 的测试源对护栏完全不可见（静默腐坏）。
# ⑤ 源文件总数 MUST 与 root `AGENTS.md` 的 `EXAMPLES_TEST_SOURCES` 锚点一致
#    —— 否则「源文件与其声明**一起**删除」不会被发现（静默缩面）。
#    锚点机制与 `CTEST_TOTAL` 同型：任何增减 MUST 显式更新锚点（可审计的行为）。
# ⑥ 删除留痕 MUST NOT 与「豁免」混在一个文件里
#    —— 见 `examples/removed_examples.txt` 的头部注释（按名字的永久自动豁免是个坑）。
# ⑦ 「跑起来」还不够：MUST 同时要求**退出码 0** 与**日志无 error/fatal**
#    —— 否则「跑但报错」会被判成 OK。
#
# 检查：
#   C0  源码级：`enable_testing()` MUST 排在 `add_subdirectory(examples)` 之前
#                （陈旧构建目录会掩盖这个问题 ⇒ 不能只靠 `ctest -N`）。
#   C1  源文件归属守恒：每个 examples 测试源 MUST 被某个「已进 CTest」或
#                       「已被 example 运行护栏跟踪」的 target 编译；否则 MUST
#                       登记在 `examples/disabled_examples.txt`（带理由 + 归位条件）。
#   C2  数量守恒：源文件总数 == root `AGENTS.md` 的 `EXAMPLES_TEST_SOURCES` 锚点。
#   C3  运行期：examples 的每个 CTest 条目 MUST 跑出**退出码 0** 且
#              日志**无 error/fatal**（既有例外走显式 allowlist，带 stale 检查）。
#   C4  可执行断言数下限：每个测试源 MUST ≥ 仓库登记的锚点下限
#              （防「断言被删除 / 注释掉 / 条件编译掉」；**不防**等量替换）。
#   C4b 逐用例下限：每个**字面**用例 MUST ≥ 1 条可执行断言
#              （防「某用例只有 SUCCEED」—— 文件级下限会被同文件其它用例掩盖）。
#
# Exit codes: 0 = 全部通过, 1 = 至少一处违规。
#
# Usage: bash scripts/verify_example_test_wiring.sh [build-dir]
#
# ⚠️ 可移植性：只用 POSIX sh 语法 + awk/grep/sed/find。
#    **不用** `declare -A` / `mapfile`（bash 4 专有；CI 的 macos-14 跑系统 bash 3.2）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${1:-build}"
if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: build dir '$BUILD_DIR' not found — 本护栏需要已 configure 的构建目录" >&2
    echo "       （判据是 'ctest -N'，即 CTest 条目的唯一真相）" >&2
    exit 1
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

violations=0
note()  { printf "  %s\n" "$1"; }
violation() { printf "${RED}VIOLATION${NC} %s\n" "$1" >&2; violations=$((violations+1)); }

DISABLED_FILE="examples/disabled_examples.txt"
# ⚠️ `examples/removed_examples.txt`（删除留痕）**刻意不被本护栏读取** ——
#    它只是审计记录。把它当豁免用会退化成「按名字的永久自动豁免」（第二轮 B3）。
AGENTS_MD="AGENTS.md"

# ---------------------------------------------------------------------------
# C3 的 allowlist：**已知**会打印 error 级日志的 target。
# 根因 = 已登记的框架缺陷（`ch_module` 作为 `Component` 成员无法绑 parent ⇒
# 子 stage 挂到祖父节点下 ⇒ `<<=` 的 driver 归属检查失败，`include/core/io.h:385`；
# `CHREQUIRE` 只记日志不中止）。**本 change 不修该缺陷** ⇒ 显式豁免 + stale 检查。
# 归位条件：框架缺陷修复后，这些行 MUST 被移除（否则报 STALE）。
# 格式：<target>|<缺陷编号>
# ---------------------------------------------------------------------------
CHERROR_ALLOWLIST="test_pipeline_minimal|framework-defect-1|3
test_pipeline_debug|framework-defect-1|9
test_pipeline_wrapper|framework-defect-1|9
test_wrapper_pipeline|framework-defect-1|6"


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
examples/riscv-mini/tests/test_riscv_tests.cpp|riscv-tests 自包含用例 + 仓外二进制条件 SKIP（⚠️ 用 `TEST_CASE_FOR_each` 宏**生成**的 41 个用例的断言在宏体里 ⇒ 按「宏体整行不计」的保守规则不计入，故下限 = 9 个字面用例的 14）|14
examples/riscv-mini/tests/test_riscv_tests_pipeline.cpp|riscv-tests 经流水线跑（全部依赖仓外二进制）|47
examples/riscv-mini/tests/test_riscv_tests_soc.cpp|MMIO 地址解码（ITCM/DTCM/CLINT/UART/GPIO）|10
examples/riscv-mini/tests/test_rv32i_pipeline.cpp|流水寄存器 latch/stall/flush + hazard 前推（含 EX>MEM 双源优先级）+ 分支检测|23
examples/riscv-mini/tests/test_soc_hello_world.cpp|SoC 顶层 hello-world|8
examples/riscv-mini/tests/test_wrapper_ifstage.cpp|IfStage 的 PC 复位/IF-ID 锁存/stall 冻结|4
examples/riscv-mini/tests/test_wrapper_pipeline.cpp|Rv32iPipeline 直接包裹的周期行为|3
examples/riscv-mini/tests/test_wrapper_simple.cpp|HazardUnit 的 stall / EX 前推选择|4"

# ═══════════════════════════════════════════════════════════════════════════
# 源文件静态分析：**活代码提取** + **逐用例断言计数**
#
# 输出两行：
#   `A <n>`            —— 活代码里的可执行断言总数
#   `C <n> <用例名>`   —— 每个用例体内的可执行断言数（每用例一行）
#
# ⚠️ 能力边界（如实声明；第四/五轮对抗审稿各纠正过一次，勿再夸张）：
#   ① **已覆盖**：整行/行尾/跨行块注释、字符串与字符字面量、`#if 0` 区段、
#      宏体（`#define` 行**整行不计**）、词边界（`MYREQUIRE(` 不计）、
#      按**出现次数**计（同一行两条都算）。
#   ② **不覆盖（保守跳过）**：`#ifdef` / `#ifndef` / 非常量 `#if` 的**整个区段**
#      一律按「可能不编译」跳过 ⇒ 宁可少算（吵但安全），不会多算。
#      代价：本仓 `test_riscv_tests.cpp` 用 `TEST_CASE_FOR_each(...)` 宏**生成**的
#      41 个用例（宏体里的 `REQUIRE(passed == true)`）不进计数 ⇒ 该文件的
#      锚点是 14（9 个字面用例），不是 55。**这是已知且已声明的局限**。
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
    SQ = "\047"; inbc = 0; depth = 0; pending = ""; live = ""; A = 0; nc = 0
}
function total_skip(   i) {
    for (i = 1; i <= depth; i++) if (gskip[i]) return 1
    return 0
}
{
    line = $0
    if (pending != "") line = pending line
    if (line ~ /\\[ \t]*$/) { sub(/\\[ \t]*$/, "", line); pending = line; next }
    pending = ""

    # 剥注释（两种输出：keep = 保留字符串字面量，out = 连同字符串一起剥掉）
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

    # 预处理指令
    if (out ~ /^[[:space:]]*#[[:space:]]*define([[:space:]]|$)/) next   # 宏体不计（可能永不展开）
    if (out ~ /^[[:space:]]*#[[:space:]]*if[[:space:]]/) {
        cond = out; sub(/^[[:space:]]*#[[:space:]]*if[[:space:]]*/, "", cond); sub(/[[:space:]]+$/, "", cond)
        res = 2                                                # 非常量 ⇒ 保守整组跳过
        if (cond ~ /^[-+]?[0-9]+$/) res = (cond + 0 != 0) ? 1 : 0
        depth++; gskip[depth] = (res == 1 ? 0 : 1); gtaken[depth] = (res == 1)
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*(ifdef|ifndef)/) { depth++; gskip[depth] = 1; gtaken[depth] = 0; next }
    if (out ~ /^[[:space:]]*#[[:space:]]*elif/)            { if (depth > 0 && gtaken[depth] == 0) gskip[depth] = 1; next }
    # ⚠️ `#if 0 ... #else <活代码> ... #endif`：`#else` 之后**是活的** ⇒ 必须放行，
    #    否则「语义等价的重写」会被误报（第五轮对抗审稿 SHOULD_FIX）。
    #    `#if 1 ... #else <死代码> ... #endif`：`#else` 之后是死的 ⇒ 保持跳过。
    if (out ~ /^[[:space:]]*#[[:space:]]*else/) {
        if (depth > 0) gskip[depth] = gtaken[depth] ? 1 : 0
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*endif/)           { if (depth > 0) depth--; next }

    if (total_skip()) next

    # 记录用例名（从**保留字符串**的 keep 里取，注意 out 已把字符串剥空）
    if (keep ~ /^[[:space:]]*(TEST_CASE|TEST_CASE_METHOD|TEMPLATE_TEST_CASE|TEMPLATE_PRODUCT_TEST_CASE|SCENARIO)[[:space:]]*\(/) {
        if (match(keep, /"[^"]*"/)) {
            nm = substr(keep, RSTART + 1, RLENGTH - 2)
            nc++; casenames[nc] = nm
        }
    }
    live = live "\n" out
}
END {
    s = live
    while (length(s) > 0) {
        if (match(s, /(REQUIRE_FALSE|CHECK_FALSE|REQUIRE|CHECK)[[:space:]]*\(/) == 0) break
        pre = (RSTART > 1) ? substr(s, RSTART - 1, 1) : ""
        if (pre == "" || pre ~ /[^A-Za-z0-9_]/) A++
        s = substr(s, RSTART + RLENGTH)
    }
    printf "A %d\n", A

    t = live; m = length(t); pos = 1; inbody = 0; bd = 0; ca = 0; cur = ""; cni = 0
    while (pos <= m) {
        if (!inbody) {
            if (substr(t, pos, 9) == "TEST_CASE" || substr(t, pos, 10) == "TEMPLATE_T" || substr(t, pos, 8) == "SCENARIO") {
                cni++
                cur = (cni <= nc) ? casenames[cni] : "?"
                pos += 8; continue
            }
            if (substr(t, pos, 1) == "{" && cur != "") { inbody = 1; bd = 1; ca = 0; pos++; continue }
            pos++; continue
        }
        ch = substr(t, pos, 1)
        if (ch == "{") bd++
        else if (ch == "}") {
            bd--
            if (bd == 0) { printf "C %d %s\n", ca, cur; inbody = 0; cur = "" }
        } else {
            # ⚠️ 对**任意**字符位置试 match（不能挂在 `ch == "("` 上）
            if (match(substr(t, pos), /^(REQUIRE_FALSE|CHECK_FALSE|REQUIRE|CHECK)[[:space:]]*\(/)) ca++
        }
        pos++
    }
}' "$1"
}

# 活代码里的可执行断言总数
real_assert_count() {
    analyze_source "$1" | awk '/^A /{print $2; exit}'
}

echo "=== verify_example_test_wiring: examples 测试归属护栏 ==="

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT INT TERM

# ---------------------------------------------------------------------------
# C0 源码级：enable_testing() 必须排在 add_subdirectory(examples) 之前
#
# ⚠️ 为什么需要这一条（对抗审稿 S1）：`ctest -N` 反映的是**上次 configure**
#    的结果 —— 在陈旧构建目录下，即使把 `enable_testing()` 移回 examples 之后，
#    `ctest -N` 仍会列出旧条目 ⇒ 只靠运行期判据会**假阴性**。
# ---------------------------------------------------------------------------
echo ""
echo "--- C0: enable_testing() MUST 先于 add_subdirectory(examples) ---"
et_line="$(grep -n '^[[:space:]]*enable_testing()' CMakeLists.txt | head -1 | cut -d: -f1)"
ex_n="$(grep -c '^[[:space:]]*add_subdirectory(examples)' CMakeLists.txt)"
ex_line="$(grep -n '^[[:space:]]*add_subdirectory(examples)' CMakeLists.txt | head -1 | cut -d: -f1)"
if [ "$ex_n" -ne 1 ]; then
    # 若将来出现第 2 处，`head -1` 的判定就不再代表全部 ⇒ 必须显式报出（而非静默只看第一处）
    violation "根 CMakeLists.txt 里 'add_subdirectory(examples…)' 出现 $ex_n 次（期望恰好 1 次）—— C0 的判定会只覆盖第一处"
fi
if [ -z "$et_line" ] || [ -z "$ex_line" ]; then
    violation "在根 CMakeLists.txt 里找不到 enable_testing()（行 $et_line）或 add_subdirectory(examples)（行 $ex_line）"
elif [ "$et_line" -gt "$ex_line" ]; then
    violation "enable_testing() 在第 $et_line 行，晚于 add_subdirectory(examples) 的第 $ex_line 行 —— examples 子目录的 add_test() 会是**空操作**"
else
    note "OK  enable_testing() 在第 $et_line 行 < add_subdirectory(examples) 第 $ex_line 行"
fi

# ---------------------------------------------------------------------------
# 1) 已注册进 CTest 的测试名（唯一真相）
# ---------------------------------------------------------------------------
CTEST_LIST="$TMP/ctest_list"
if ! ctest --test-dir "$BUILD_DIR" -N > "$TMP/ctest_raw" 2>/dev/null; then
    echo "${RED}❌ ctest -N 在 '$BUILD_DIR' 失败（构建目录未 configure？）${NC}" >&2
    exit 1
fi
sed -n 's/^[[:space:]]*Test[[:space:]]*#[0-9][0-9]*:[[:space:]]*//p' "$TMP/ctest_raw" \
    | sort -u > "$CTEST_LIST"
note "CTest 已注册条目：$(wc -l < "$CTEST_LIST" | tr -d ' ') 个"

# ---------------------------------------------------------------------------
# 2) example 运行护栏的跟踪集合（`CH_TRACKED_EXAMPLE_TARGETS`）
# ---------------------------------------------------------------------------
TRACKED_LIST="$TMP/tracked_list"
sed -n '/set(CH_TRACKED_EXAMPLE_TARGETS/,/^)/p' examples/CMakeLists.txt \
    | sed -n 's/^    \([A-Za-z0-9_]*\)$/\1/p' | sort -u > "$TRACKED_LIST"
note "example 运行护栏跟踪的 target：$(wc -l < "$TRACKED_LIST" | tr -d ' ') 个"

# ---------------------------------------------------------------------------
# 3) disabled 清单（**只含存活项**；删除留痕在 removed_examples.txt）
# ---------------------------------------------------------------------------
DISABLED_LIST="$TMP/disabled_list"
: > "$DISABLED_LIST"
if [ -f "$DISABLED_FILE" ]; then
    sed -n 's/^[[:space:]]*//p' "$DISABLED_FILE" \
        | grep -v '^#' | grep -v '^$' | cut -f1 | sort -u > "$DISABLED_LIST"
fi

# ---------------------------------------------------------------------------
# 4) 源文件 → target 映射（跨**所有** CMakeLists，路径**精确**）
# ---------------------------------------------------------------------------
MAP="$TMP/map.tsv"        # <resolved_src>\t<target>\t<declaring CMakeLists>
: > "$MAP"
ALL_CMAKE="$(find . -name CMakeLists.txt -not -path './build/*' -not -path './.rddf/wt/*' -not -path './third_party/*' | sort)"
for f in $ALL_CMAKE; do
    awk -v out="$MAP" '
    {
        line = $0
        sub(/[ \t]*#.*$/, "", line)                       # 剥行尾注释
        if (match(line, /[A-Za-z0-9_]*\(/) == 0) next
        rest = substr(line, RSTART + RLENGTH)
        if (rest !~ /^[A-Za-z0-9_.-]+[ \t,]/) next        # 排除 ${...} 模板
        n = split(rest, a, /[ \t,)]+/)
        target = a[1]
        if (target == "") next
        m = split(rest, b, /[ \t,)]+/)
        src = ""
        for (i = m; i >= 1; i--) {
            if (b[i] ~ /\.cpp$/) { src = b[i]; break }
        }
        if (src == "") next
        # 剥 ${PROJECT_SOURCE_DIR}/ 前缀（用 index() —— ERE 的 \{ 转义不可靠）
        rootrel = 0
        if (index(src, "${PROJECT_SOURCE_DIR}/") == 1) {
            src = substr(src, length("${PROJECT_SOURCE_DIR}/") + 1)
            rootrel = 1
        }
        if (rootrel == 0 && src !~ /^\//) {
            d = FILENAME
            if (sub(/\/[^\/]*$/, "", d) == 0) d = "."
            src = d "/" src
        }
        sub(/^\.\//, "", src)
        print src "\t" target "\t" FILENAME >> out
    }' "$f"
done
sort -u -o "$MAP" "$MAP"

# ---------------------------------------------------------------------------
# 5) 基准集合：examples 下的**测试源文件**（外部事实）
#
# ⚠️ 枚举按**内容/目录**，不按文件名约定：
#    (a) 含 Catch2 `TEST_CASE` 的 `.cpp`（不论文件名、不论目录）
#    (b) `examples/*/tests/*.cpp`（不论内容）
#    (a)∪(b) —— 任一命中即视为「测试源」。
#    只按 `test_*.cpp` 命名枚举会漏掉 `pipeline_check.cpp` 这类源（实测逃逸）。
# ---------------------------------------------------------------------------
SRC_LIST="$TMP/src_list"
# ⚠️ 枚举 MUST 覆盖：
#    ① 任意 C++ 后缀（`.cpp` / `.cc` / `.cxx` / `.C`）—— 只认 `.cpp` 会漏掉
#       `tests/test_x.cc`（实测：`.cc` + TEST_CASE 的源此前完全不可见）；
#    ② Catch2 的**全部**用例宏（不只 `TEST_CASE`）—— `SCENARIO(...)` / 
#       `TEMPLATE_TEST_CASE` 等宏写的测试同样要算；
#    ③ `examples/*/tests/` 下的任意文件（不论内容）—— 兜住「还没写断言」的新测试。
{
    grep -rlE '^[[:space:]]*(TEST_CASE|TEST_CASE_METHOD|TEMPLATE_TEST_CASE|TEMPLATE_PRODUCT_TEST_CASE|SCENARIO)[[:space:]]*\(' \
        examples --include='*.cpp' --include='*.cc' --include='*.cxx' --include='*.C' 2>/dev/null
    grep -rl 'CATCH_' examples --include='*.cpp' --include='*.cc' --include='*.cxx' --include='*.C' 2>/dev/null
    find examples -path '*/tests/*' -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.C' \) 2>/dev/null
} | sed 's|^\./||' | sort -u > "$SRC_LIST"
n_total=$(wc -l < "$SRC_LIST" | tr -d ' ')
note "examples 测试源文件总数：$n_total 个"

# ---------------------------------------------------------------------------
# C1 源文件归属守恒
# ---------------------------------------------------------------------------
echo ""
echo "--- C1: 源文件归属守恒 ---"
n_covered=0
n_registered=0
while IFS= read -r src; do
    [ -z "$src" ] && continue
    base="$(basename "$src")"

    # 该源文件被哪些 target 声明（精确路径匹配）
    owners=""
    while IFS="$(printf '\t')" read -r msrc mtarget mdecl; do
        [ "$msrc" = "$src" ] && owners="$owners $mtarget"
    done < "$MAP"

    covered=0
    for t in $owners; do
        if grep -qx -- "$t" "$CTEST_LIST" || grep -qx -- "$t" "$TRACKED_LIST"; then
            covered=1
            break
        fi
    done

    if [ "$covered" -eq 1 ]; then
        n_covered=$((n_covered+1))
        note "OK  $src →${owners}"
    elif grep -qx -- "${base%.*}" "$DISABLED_LIST"; then
        n_registered=$((n_registered+1))
        note "OK  $src → 已在 disabled 清单登记（待归位）"
    else
        violation "$src 未被任何已覆盖的 target 编译（静默腐坏：编译了但从不执行）—— MUST 修进 CTest 或登记在 $DISABLED_FILE"
    fi
done < "$SRC_LIST"

# ---------------------------------------------------------------------------
# C2 数量守恒 vs root AGENTS.md 的锚点
# ---------------------------------------------------------------------------
echo ""
echo "--- C2: 源文件总数 vs EXAMPLES_TEST_SOURCES 锚点 ---"
anchor="$(sed -n 's/^[[:space:]]*-[[:space:]]*EXAMPLES_TEST_SOURCES:[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$AGENTS_MD" | head -1)"
if [ -z "$anchor" ]; then
    violation "$AGENTS_MD 缺少 'EXAMPLES_TEST_SOURCES: <N>' 锚点（C2 无法判定 ⇒ 缩面不可见）"
else
    note "锚点 EXAMPLES_TEST_SOURCES=$anchor ／ 实测=$n_total"
    if [ "$anchor" -ne "$n_total" ]; then
        violation "examples 测试源文件数（$n_total）≠ 锚点（$anchor）—— 任何增减 MUST 显式更新锚点（防「源文件与其声明一起删除」的静默缩面）"
    else
        note "OK  数量守恒"
    fi
fi

# 声明在 `examples/**/CMakeLists.txt` 里的 target 名（从 $MAP 的 `mdecl` 列派生）。
# 用途：区分「examples 域的 CTest 条目」与「根 `tests/` 等其它域的条目」——
# 前者缺失二进制要报违规，后者不属本护栏职责。
EX_TARGETS="$TMP/ex_targets"
# ⚠️ 两个来源并集，避免只认 `.cpp` 声明行而漏掉非 `.cpp` 源的 target：
#   ① $MAP 的 target 列（精确路径，但只含 `.cpp` 声明）；
#   ② `examples/**/CMakeLists.txt` 的折行文本里「作为任意 `fn(` 首个实参」的名字
#      （覆盖 `.cc`/`.cxx` 源、多行声明）。
{
    awk -F'\t' '$3 ~ /^(\.\/)?examples\// { print $2 }' "$MAP"
    for f in $(find examples -name CMakeLists.txt 2>/dev/null); do
        awk '{printf "%s ", $0}' "$f" \
            | grep -oE '[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\([[:space:]]*[A-Za-z_][A-Za-z0-9_]*' \
            | sed -E 's/.*\([[:space:]]*//' \
            | grep -E '_test$|_demo$|_example$|_smoke$|_prog$|_check$'
    done
} | grep -v '^$' | sort -u > "$EX_TARGETS"
is_declared_example_target() {
    grep -qx -- "$1" "$EX_TARGETS"
}

# ---------------------------------------------------------------------------
# C3 运行期：每个 examples 的 CTest 条目 MUST exit 0 且日志无 error/fatal
# ---------------------------------------------------------------------------
echo ""
echo "--- C3: CTest 条目 MUST 跑出 exit 0 且日志无 error/fatal ---"
# ⚠️ 遍历 **CTest 条目 ∩ `build/examples/` 下的可执行文件**，而**不是**遍历
#    「有 `.cpp` 声明行」的 target —— 否则一个用 `.cc` 源注册进 CTest 的测试
#    永远不被运行期检查（实测：它失败时护栏仍 PASS）。
while IFS= read -r mtarget; do
    [ -z "$mtarget" ] && continue
    bin="$(find "$BUILD_DIR/examples" -type f -name "$mtarget" -perm -u+x 2>/dev/null | head -1)"
    if [ -z "$bin" ]; then
        # ⚠️ 不能一律 `continue`（第四轮对抗审稿 SHOULD_FIX #2）：那会让
        #    「声明了 CTest 条目但**从未构建**」的树**假绿** —— 而「编译了但
        #    从不执行」正是本 change 要治的病，它的孪生形态就是「注册了但
        #    从不构建」。区分两种情形：
        #      ① 该名字声明在 `examples/**/CMakeLists.txt` ⇒ **本护栏的职责**
        #         ⇒ 必须报违规（构建产物缺失）；
        #      ② 否则（根 `tests/` 等其它域）⇒ 不属本护栏职责，跳过。
        if is_declared_example_target "$mtarget"; then
            violation "CTest 条目 $mtarget 声明在 examples/ 域但**找不到可执行文件**（未构建？）—— 注册了却从不执行"
        fi
        continue
    fi
    # ⚠️ 加 timeout：`ctest` 对这些条目设了 TIMEOUT 300，护栏直接跑二进制时
    #    MUST 有等价的保护，否则一个挂死的 target 会把护栏本身也挂死。
    #    （`timeout` 来自 coreutils；macOS 上是 `gtimeout`（homebrew）。）
    if command -v timeout >/dev/null 2>&1; then
        out="$(timeout 300 "$bin" 2>&1)"; rc=$?
    elif command -v gtimeout >/dev/null 2>&1; then
        out="$(gtimeout 300 "$bin" 2>&1)"; rc=$?
    else
        out="$("$bin" 2>&1)"; rc=$?
    fi
    errs="$(printf '%s' "$out" | grep -cE '^\[(ERROR|FATAL)\]')"
    # allowlist 记的是**期望条数**（而不是「整条豁免」）—— 否则往一个已豁免的
    # target 里注入任意多条**无关**错误仍会被判 OK（实测踩过）。
    expected="$(printf '%s\n' "$CHERROR_ALLOWLIST" | awk -F'|' -v t="$mtarget" '$1==t {print $3; exit}')"

    if [ "$rc" -ne 0 ]; then
        violation "CTest 条目 $mtarget 退出码 $rc（≠ 0）⇒ CTest 会判红。输出末尾：$(printf '%s' "$out" | grep -vE '^\[INFO\]' | tail -2 | tr '\n' ' ')"
    elif [ "$errs" -eq 0 ]; then
        # 日志干净 ⇒ 若 allowlist 里仍有它 ⇒ STALE 豁免
        if [ -n "$expected" ]; then
            violation "allowlist 里的 $mtarget 已不再打印 error/fatal ⇒ MUST 从 CHERROR_ALLOWLIST 移除该行（STALE 豁免）"
        else
            note "OK  $mtarget → exit 0，日志干净"
        fi
    elif [ -z "$expected" ]; then
        violation "CTest 条目 $mtarget 退出码 0 但日志有 $errs 条 error/fatal（「跑但报错」≠ 通过）。首条：$(printf '%s' "$out" | grep -E '^\[(ERROR|FATAL)\]' | head -1)"
    elif [ "$errs" -eq "$expected" ]; then
        note "OK  $mtarget → exit 0（$errs 条 error/fatal = allowlist 期望值：framework-defect-1）"
    else
        violation "CTest 条目 $mtarget 的 error/fatal 条数（$errs）≠ allowlist 期望值（$expected）—— **变多** = 新增报错 MUST 单独处置（不得躲在豁免里）；**变少** = 缺陷已被部分修复 ⇒ MUST 把期望值下调"
    fi
done < "$CTEST_LIST"

# allowlist 的 stale 检查：条目对应的 target 若已不再打印 error ⇒ MUST 移除该行
echo ""
echo "--- C3b: allowlist stale 检查 ---"
while IFS='|' read -r atarget adefect aexp; do
    [ -z "$atarget" ] && continue
    # ⚠️ 不在 CTest 里的 allowlist 行 MUST 报 STALE（而不是「跳过」）——
    #    否则拼错的名字 / 已失效的豁免行会**永久残留**且无声。
    if ! grep -qx -- "$atarget" "$CTEST_LIST"; then
        printf "${YELLOW}STALE${NC} allowlist 里的 $atarget 不在 CTest 中 ⇒ 该行永不生效，MUST 移除\n"
        violations=$((violations+1))
        continue
    fi
    note "OK  $atarget 在 CTest 中（$adefect，期望 $aexp 条）"
done <<EOF
$CHERROR_ALLOWLIST
EOF

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
    n="$(real_assert_count "$src")"
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
    zero="$(analyze_source "$src" | awk '/^C / && $2 == 0 { $1=""; $2=""; sub(/^ +/, ""); print }')"
    ncases="$(analyze_source "$src" | grep -c '^C ')"
    if [ -n "$zero" ]; then
        while IFS= read -r z; do
            [ -n "$z" ] && violation "$src 的用例「$z」有 0 条可执行断言（只有 SUCCEED 之类）—— MUST 补断言或删除并登记"
        done <<EOF
$zero
EOF
    else
        note "OK  $src → $ncases 个字面用例，每个都 ≥ 1 条断言"
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
    echo "${GREEN}✅ verify_example_test_wiring: PASS${NC}"
    exit 0
fi
echo "${RED}❌ verify_example_test_wiring: $violations 处违规${NC}" >&2
exit 1
