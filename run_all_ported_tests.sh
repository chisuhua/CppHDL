#!/usr/bin/env bash
# ============================================================
# @file run_all_ported_tests.sh
# @brief 运行所有 SpinalHDL 移植示例的 main() 自测并验证
#
# 用法: ./run_all_ported_tests.sh [build_dir]
#   build_dir: 构建目录，默认为 build
#
# 返回值: 0=全部通过, 非0=有失败
#
# ⚠️ **这是仓库的 PHASE COMPLETION GATE 命令**（见根 AGENTS.md 的
#    「PHASE COMPLETION GATE」表格与 COMMANDS 段）⇒ 它的语义变更会影响
#    每个人的本地验收流程。
#
# ──────────────────────────────────────────────────────────────────────────
# 单一来源（Single source of truth）
# ──────────────────────────────────────────────────────────────────────────
# 被跟踪的 example 集合只有**一处**：`examples/CMakeLists.txt` 的
# `CH_TRACKED_EXAMPLE_TARGETS` ⇒ 它同时驱动
#   ① 构建侧的聚合 target `all-examples`（examples/CMakeLists.txt 末尾）
#   ② 运行侧的本脚本
#
# ⚠️ 为什么要单一来源：曾经本脚本硬编码 28 行 `run_test`、各子目录各自
# `add_executable`、`grep '^int main'` 又给出 32 个源文件 ⇒ **三份清单**。
# 漂移方向不对称：「脚本里有、CMake 里没有」能被「缺失二进制 = 失败」抓到；
# 而「**CMake 里有、脚本里没有**」（新 example 忘了加进护栏）**永远静默**
# ⇒ 那正是本护栏要治的病在护栏自身上的复现。
#
# ──────────────────────────────────────────────────────────────────────────
# 三条守恒规则（与 samples 护栏 scripts/run_all_samples.sh 同构）
# ──────────────────────────────────────────────────────────────────────────
#   C1 (registration consistency): `examples/` 下每个 main() 程序 MUST 要么
#       在 CH_TRACKED_EXAMPLE_TARGETS 里，要么登记在
#       `examples/disabled_examples.txt`（带理由 + 归位条件）。两边都没有
#       ⇒ FAILURE。
#
#   C2 (count conservation): 实际执行的 example 数 MUST 等于跟踪数。
#       不等 ⇒ FAILURE，不是 skip。
#
#   C3 (missing binary): 被跟踪的 example 二进制不存在 ⇒ FAILURE。
#       ⚠️ 缺失**不得中止整轮** —— 中止会让后续 example 从不执行 ⇒
#       受护集合**静默缩面**（比 skip 更坏：skip 至少会出现在汇总里）。
#
# 豁免（KNOWN_FAILING）用于「已知失败」的 example。它们**不被跳过** ——
# 仍被执行，作为 stale 探针：
#   - 它们的失败**不计入**失败总数
#   - 但若被豁免的 example 转绿，则报为 **STALE** 豁免并**计为失败**
#     （豁免不得比它的根因活得更久）
#   - 每条豁免都以 `::notice` 打印 ⇒ 豁免**不是静默的**
#
# 仅使用 bash + sed/grep/find（可移植于 ubuntu-22.04 / macos-14）。
# ============================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${1:-${BUILD_DIR:-build}}"
EXAMPLES_DIR="$REPO_ROOT/examples"
LOG_DIR=""
TIMEOUT_SECONDS="${EXAMPLE_TIMEOUT_SECONDS:-30}"

# ---------------------------------------------------------------------------
# Known-failing examples. Each entry: "<example> <TAB> <root cause> <TAB>
# <related change / issue> <TAB> <invalidation condition>".
#
# The invalidation condition MUST be decidable: whoever fixes the root cause
# MUST remove the line here (the guard will then fail with a STALE exemption
# if the line is left behind after the example goes green).
#
# ℹ️ 2026-10-22: the baseline is 28/28 GREEN (measured on pristine HEAD)
#    ⇒ this list is EMPTY. It is kept as the mechanism, not as data: the
#    moment an example legitimately starts failing, it MUST be registered
#    here (with a root cause and an invalidation condition) rather than
#    silently skipped.
# ---------------------------------------------------------------------------
KNOWN_FAILING=()

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: Build directory '$BUILD_DIR' not found"
    echo "Run: cmake -B build && cmake --build build"
    exit 1
fi
BUILD_DIR_ABS="$(cd "$BUILD_DIR" && pwd)"

if [ ! -d "$BUILD_DIR_ABS/examples" ]; then
    echo "ERROR: example binary directory '$BUILD_DIR_ABS/examples' not found"
    echo "Run: cmake -B build && cmake --build build --target all-examples"
    exit 1
fi

LOG_DIR="$BUILD_DIR_ABS/logs/examples"
mkdir -p "$LOG_DIR"

# ⚠️ The examples write their generated Verilog / waveform dumps into the
# **current working directory** (counter.v, axi4_lite_waveform.vcd, …).
# The script has always run them from the build directory so that those
# artifacts land in build/ and not in the repository root.
# (`.gitignore` covers `*.v` but NOT `*.vcd` ⇒ running from the repo root
# would litter it with untracked waveform files.)
cd "$BUILD_DIR_ABS"

# Portable timeout: GNU `timeout` does not exist on macOS by default
# (coreutils provides `gtimeout` there).
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_BIN="timeout"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_BIN="gtimeout"
else
    echo "ERROR: neither 'timeout' nor 'gtimeout' is available."
    echo "       On macOS: brew install coreutils   (provides gtimeout)."
    echo "       Without a timeout a hanging example would block CI forever."
    exit 1
fi

# ---------------------------------------------------------------------------
# Source of truth: the tracked example set.
#
# Parses `set(CH_TRACKED_EXAMPLE_TARGETS …)` from examples/CMakeLists.txt.
# `# PHASE:` comments inside the block are display-only section markers.
# (Mirrors scripts/run_all_samples.sh:95-99, which parses the same way out
#  of samples/CMakeLists.txt.)
# ---------------------------------------------------------------------------
tracked_examples() {
    sed -n '/set(CH_TRACKED_EXAMPLE_TARGETS/,/^)/p' "$EXAMPLES_DIR/CMakeLists.txt" \
        | sed -n 's/^    # PHASE: \(.*\)$/PHASE|\1/p; s/^    \([A-Za-z0-9_]*\)$/EX|\1/p'
}

# Every example target's binary is named after the target itself
# (CMake's default output name) ⇒ locate it by name under build/examples/.
find_binary() {
    local target="$1" hit
    hit="$(find "$BUILD_DIR_ABS/examples" -type f -name "$target" 2>/dev/null | head -1)"
    if [ -n "$hit" ] && [ -x "$hit" ]; then
        printf '%s' "$hit"
    fi
}

# Every built example program, keyed by its CMake target name (CMake names
# the output after the target ⇒ the binary's basename IS the target name).
#
# ⚠️ Why the *built* set and not the *source* set: the target name and the
# source file name differ in 16 of the 28 tracked examples
# (`counter.cpp` → `counter_example`, `stream_fifo_example.cpp` →
# `fifo_stream_example`, …) ⇒ comparing source names against target names
# would false-positive on every one of them.
built_example_programs() {
    find "$BUILD_DIR_ABS/examples" -type f -perm -u+x 2>/dev/null \
        | xargs -r -n1 basename | sort -u
}

disabled_examples() {
    [ -f "$EXAMPLES_DIR/disabled_examples.txt" ] || return 0
    grep -v '^[[:space:]]*#' "$EXAMPLES_DIR/disabled_examples.txt" \
        | grep -v '^[[:space:]]*$' \
        | cut -f1
}

# 已注册进 CTest 的测试名。
# ⚠️ 2026-10-23 (change `riscv-mini-test-triage-2026-10`): example 内的
# Catch2 测试 target 现在通过 `add_test` 进 CTest => 它们**是构建产物**,
# 但运行覆盖由 **CTest** 提供 (不是本护栏) => 「已构建但未跟踪」检查必须
# 认得 CTest 这个归属, 否则会把 18 个已被 CTest 覆盖的测试误判为「无归属」。
# 判据用 `ctest -N` (CTest 条目的**唯一真相**), 而不是解析 CMake 源码。
# 一个名字是否仍是**已声明**的 CMake target（跨所有 CMakeLists）。
# ⚠️ 用于区分两种「已构建但无归属」：
#   ① target 仍在 CMake 里但没有任何归属 ⇒ **真问题**（UNTRACKED）；
#   ② target 已从 CMake 里**删除**、只是构建目录残留了旧二进制
#      ⇒ **陈旧产物**（STALE ARTIFACT），不是归属问题（清一下 build 目录即可）。
#    2026-10-23（change `riscv-mini-test-triage-2026-10`）新增该区分：
#    删掉若干个 target 后，本地旧构建目录里的孤儿二进制会让 C1 报一串
#    「无归属」，而其中没有一个是真实问题 ⇒ 报告误导。
# 已声明的 target 集合 —— 优先用**构建系统自己的**列表
# （`cmake --build --target help`），它对 CMake 的语法变体免疫：
# 一个**多行**声明 `add_executable(\n  name\n  src)` 用文本正则匹配会漏
# （实测：真实程序被误判成 STALE-ARTIFACT ⇒ **静默掩盖真问题**），
# 而构建系统必然认得它。仅在构建系统不提供该查询时回退到文本匹配。
DECLARED_TARGETS_CACHE=""
declared_targets() {
    if [ -z "$DECLARED_TARGETS_CACHE" ]; then
        DECLARED_TARGETS_CACHE="$(cmake --build "$BUILD_DIR_ABS" --target help 2>/dev/null \
            | sed -n 's/^\.\.\. //p' | sort -u)"
    fi
    printf '%s\n' "$DECLARED_TARGETS_CACHE"
}

is_declared_target() {
    local want="$1" f
    # ① 构建系统认得（权威）
    if [ -n "$DECLARED_TARGETS_CACHE" ] || cmake --build "$BUILD_DIR_ABS" --target help >/dev/null 2>&1; then
        if declared_targets | grep -qx -- "$want"; then
            return 0
        fi
    fi
    # ② 回退：文本匹配（把整个文件折成一行，多行声明也能匹配）。
    #    ⚠️ 回退只会说「declared」⇒ 最坏把陈旧产物报成 UNTRACKED（吵但安全）。
    # ⚠️ 收窄到 examples/ 域：本护栏是 examples 的护栏，而 CTest 的名字集是**全局**的
    #    ⇒ 若不过滤，一个与根 `tests/` 同名的 example 二进制会被判为「已声明/已注册」
    #    而侥幸通过（跨域同名风险）。
    for f in $(find "$EXAMPLES_DIR" -name CMakeLists.txt 2>/dev/null); do
        awk '{printf "%s ", $0}' "$f" 2>/dev/null \
            | grep -qE "[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\\([[:space:]]*${want}([[:space:]]|,|\\))" && return 0
    done
    return 1
}

ctest_registered_tests() {
    ctest --test-dir "$BUILD_DIR_ABS" -N 2>/dev/null \
        | sed -n 's/^[[:space:]]*Test[[:space:]]*#[0-9][0-9]*:[[:space:]]*//p' \
        | sort -u
}

exemption_text() {
    local want="$1" line
    for line in "${KNOWN_FAILING[@]}"; do
        if [ "$(printf '%s' "$line" | cut -f1)" = "$want" ]; then
            printf '%s' "$line" | cut -f2-
            return 0
        fi
    done
    return 1
}

is_exempted() {
    local want="$1" line
    for line in "${KNOWN_FAILING[@]}"; do
        if [ "$(printf '%s' "$line" | cut -f1)" = "$want" ]; then
            return 0
        fi
    done
    return 1
}

PASS=0
FAIL=0
TOTAL=0
EXECUTED=0
MISSING=0
STALE=0
declare -a FAIL_NAMES=()
declare -a STALE_NAMES=()
declare -a MISSING_NAMES=()

echo "=========================================="
echo " SpinalHDL 移植示例验证（运行护栏）"
echo "=========================================="
echo "Build directory : $BUILD_DIR_ABS"
echo "Timeout         : ${TIMEOUT_SECONDS}s per example"
echo "Logs            : $LOG_DIR"
echo ""

TRACKED_RAW="$(tracked_examples)"
TRACKED_NAMES="$(printf '%s\n' "$TRACKED_RAW" | sed -n 's/^EX|//p')"
TRACKED_COUNT="$(printf '%s\n' "$TRACKED_NAMES" | grep -c . || true)"

if [ "$TRACKED_COUNT" -eq 0 ]; then
    echo "${RED}ERROR: no tracked examples parsed from $EXAMPLES_DIR/CMakeLists.txt${NC}"
    echo "       (set(CH_TRACKED_EXAMPLE_TARGETS …) block not found or empty)"
    exit 1
fi

# ---------------------------------------------------------------------------
# C1: registration consistency
# ---------------------------------------------------------------------------
echo "--- C1: 注册一致性（每个已构建的 example 程序必须被跟踪或已登记）---"
CONSISTENCY_FAIL=0
STALE_ARTIFACTS=0
for prog in $(built_example_programs); do
    if printf '%s\n' "$TRACKED_NAMES" | grep -qx "$prog"; then
        continue
    fi
    if disabled_examples | grep -qx "$prog"; then
        continue
    fi
    # 已进 CTest 的测试 target 也有自动化运行归属（CTest）⇒ 不是「无归属」
    # ⚠️ 用 `> /dev/null` 而非 `grep -q`：`grep -q` 提前关管道 ⇒ 上游收
    #    SIGPIPE(141) ⇒ 在 `set -o pipefail` 下整条管道被判失败 ⇒ 误判。
    if ctest_registered_tests | grep -x "$prog" > /dev/null 2>&1; then
        continue
    fi
    # 已从 CMake 里删除的 target ⇒ 构建目录残留的陈旧二进制（非归属问题）
    if ! is_declared_target "$prog"; then
        STALE_ARTIFACTS=$((STALE_ARTIFACTS + 1))
        printf "${YELLOW}  STALE-ARTIFACT  %s${NC}\n" "$prog"
        echo "      该 target 已从 CMake 中删除（不在任何 CMakeLists 里声明）"
        echo "      => 只是旧构建目录残留的二进制；清理 build 目录后即消失，不是归属问题"
        continue
    fi
    printf "${RED}  UNTRACKED  %s${NC}\n" "$prog"
    echo "      既不在 CH_TRACKED_EXAMPLE_TARGETS 里，也不在 examples/disabled_examples.txt 里"
    echo "      => 处置只有两种：① 可运行 => 补进 CH_TRACKED_EXAMPLE_TARGETS；"
    echo "                   ② 不可运行 => 登记进 examples/disabled_examples.txt"
    CONSISTENCY_FAIL=$((CONSISTENCY_FAIL + 1))
done
# 反向：被跟踪集合里若有一个**已登记为禁用** ⇒ 双重登记 ⇒ stale ⇒ 失败
for prog in $(disabled_examples); do
    if printf '%s\n' "$TRACKED_NAMES" | grep -qx "$prog"; then
        printf "${RED}  DOUBLE-REGISTERED  %s${NC}\n" "$prog"
        echo "      它同时出现在 CH_TRACKED_EXAMPLE_TARGETS 与 disabled_examples.txt 里"
        echo "      => MUST 从 disabled_examples.txt 移除该行"
        CONSISTENCY_FAIL=$((CONSISTENCY_FAIL + 1))
    fi
done
if [ "$STALE_ARTIFACTS" -gt 0 ]; then
    echo "  ℹ️  $STALE_ARTIFACTS 个陈旧二进制（target 已删，仅构建目录残留）"
fi
if [ "$CONSISTENCY_FAIL" -eq 0 ]; then
    echo "  OK：全部已构建的 example 程序都有归属（被跟踪 $TRACKED_COUNT）"
else
    FAIL=$((FAIL + CONSISTENCY_FAIL))
    FAIL_NAMES+=("C1-registration-consistency ($CONSISTENCY_FAIL 个 program 无归属)")
fi
echo ""

# ---------------------------------------------------------------------------
# Exemptions are announced, never silent.
# ---------------------------------------------------------------------------
if [ ${#KNOWN_FAILING[@]} -gt 0 ]; then
    echo "--- 豁免清单（豁免项**仍会被执行**，只影响失败计数）---"
    for line in "${KNOWN_FAILING[@]}"; do
        name=$(printf '%s' "$line" | cut -f1)
        echo "::notice example=$name exempted from the examples regression guard"
        printf '%s' "$line" | cut -f2- | tr '\t' '\n' | while read -r part; do
            [ -n "$part" ] && echo "        $part"
        done
    done
    echo ""
fi

# ---------------------------------------------------------------------------
# C2/C3: execute every tracked example
# ---------------------------------------------------------------------------
echo "--- C2/C3: 逐个执行（缺失二进制 = 失败，且**不中止**整轮）---"
while IFS='|' read -r kind value; do
    [ -n "$kind" ] || continue
    if [ "$kind" = "PHASE" ]; then
        echo ""
        echo "  -- $value --"
        continue
    fi
    [ "$kind" = "EX" ] || continue

    name="$value"
    TOTAL=$((TOTAL + 1))
    printf "  %-40s " "$name"

    exe="$(find_binary "$name")"
    if [ -z "$exe" ]; then
        # ⚠️ MUST NOT abort the whole run and MUST NOT `return` non-zero:
        #    under `set -e` a non-zero return from a top-level function call
        #    terminates the script ⇒ the remaining examples would never run
        #    ⇒ the guarded set would shrink SILENTLY (worse than a skip:
        #    a skip at least shows up in the summary).
        printf "${RED}FAILED (binary not found)${NC}\n"
        echo "      找不到二进制 '$name' ⇒ 它**从未被执行** ⇒ 这是构建问题，不是可忽略项"
        echo "      => 请先完整构建： cmake --build build --target all-examples"
        MISSING=$((MISSING + 1))
        FAIL=$((FAIL + 1))
        MISSING_NAMES+=("$name")
        continue
    fi

    EXECUTED=$((EXECUTED + 1))
    set +e
    # The brace group (not a subshell) keeps bash's own "Segmentation fault"
    # job message out of the report: the message is printed by THIS shell to
    # its stderr, so it has to be redirected around the same command.
    { "$TIMEOUT_BIN" "$TIMEOUT_SECONDS" "$exe" > "$LOG_DIR/$name.log" 2>&1; } 2>/dev/null
    rc=$?
    set -e

    if [ "$rc" -eq 0 ]; then
        if is_exempted "$name"; then
            printf "${YELLOW}STALE EXEMPTION (已转绿)${NC}\n"
            echo "      豁免已失效（根因已修复）=> MUST 从脚本的 KNOWN_FAILING 里移除该行"
            STALE=$((STALE + 1))
            FAIL=$((FAIL + 1))
            STALE_NAMES+=("$name")
            FAIL_NAMES+=("$name (stale exemption)")
        else
            printf "${GREEN}PASS${NC}\n"
            PASS=$((PASS + 1))
        fi
    else
        if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
            reason="TIMEOUT (>${TIMEOUT_SECONDS}s)"
        elif [ "$rc" -eq 139 ]; then
            reason="SEGFAULT (exit 139)"
        else
            reason="FAILED (exit code: $rc)"
        fi
        if is_exempted "$name"; then
            printf "${YELLOW}EXEMPTED (${reason})${NC}\n"
            echo "      已豁免：失败不计入失败数；但豁免项**仍被执行**（stale 探针）"
        else
            printf "${RED}%s${NC}\n" "$reason"
            echo "      日志： $LOG_DIR/$name.log"
            FAIL=$((FAIL + 1))
            FAIL_NAMES+=("$name (exit $rc)")
        fi
    fi
done <<< "$TRACKED_RAW"

echo ""

# ---------------------------------------------------------------------------
# C2: count conservation
# ---------------------------------------------------------------------------
echo "--- C2: 数量守恒（实际执行数 == 跟踪数）---"
if [ "$EXECUTED" -ne "$TRACKED_COUNT" ]; then
    printf "${RED}  MISMATCH: 跟踪 %d，实际执行 %d（缺失二进制 %d）${NC}\n" \
        "$TRACKED_COUNT" "$EXECUTED" "$MISSING"
    FAIL=$((FAIL + 1))
    FAIL_NAMES+=("C2-count-conservation (tracked=$TRACKED_COUNT executed=$EXECUTED)")
else
    echo "  OK：跟踪 $TRACKED_COUNT == 实际执行 $EXECUTED"
fi
echo ""

echo "=========================================="
printf "  ${GREEN}PASS: %-4d${NC} ${RED}FAIL: %-4d${NC} ${YELLOW}STALE: %-4d${NC} ${YELLOW}MISSING: %-4d${NC} ${YELLOW}STALE-ARTIFACT: %-4d${NC}\n" \
    "$PASS" "$FAIL" "$STALE" "$MISSING" "${STALE_ARTIFACTS:-0}"
echo "=========================================="

if [ ${#FAIL_NAMES[@]} -gt 0 ]; then
    echo ""
    echo "Failed examples:"
    for name in "${FAIL_NAMES[@]}"; do
        echo "  - $name"
    done
    exit 1
fi

exit 0
