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
REMOVED_FILE="examples/removed_examples.txt"
AGENTS_MD="AGENTS.md"

# ---------------------------------------------------------------------------
# C3 的 allowlist：**已知**会打印 error 级日志的 target。
# 根因 = 已登记的框架缺陷（`ch_module` 作为 `Component` 成员无法绑 parent ⇒
# 子 stage 挂到祖父节点下 ⇒ `<<=` 的 driver 归属检查失败，`include/core/io.h:385`；
# `CHREQUIRE` 只记日志不中止）。**本 change 不修该缺陷** ⇒ 显式豁免 + stale 检查。
# 归位条件：框架缺陷修复后，这些行 MUST 被移除（否则报 STALE）。
# 格式：<target>|<缺陷编号>
# ---------------------------------------------------------------------------
CHERROR_ALLOWLIST="test_pipeline_minimal|framework-defect-1
test_pipeline_debug|framework-defect-1
test_pipeline_wrapper|framework-defect-1
test_wrapper_pipeline|framework-defect-1"

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
ex_line="$(grep -n '^[[:space:]]*add_subdirectory(examples)' CMakeLists.txt | head -1 | cut -d: -f1)"
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
{
    grep -rl 'TEST_CASE' examples --include='*.cpp' 2>/dev/null
    find examples -path '*/tests/*.cpp' -type f 2>/dev/null
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
    elif grep -qx -- "${base%.cpp}" "$DISABLED_LIST"; then
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

# ---------------------------------------------------------------------------
# C3 运行期：每个 examples 的 CTest 条目 MUST exit 0 且日志无 error/fatal
# ---------------------------------------------------------------------------
echo ""
echo "--- C3: CTest 条目 MUST 跑出 exit 0 且日志无 error/fatal ---"
while IFS="$(printf '\t')" read -r msrc mtarget mdecl; do
    [ -z "$mtarget" ] && continue
    grep -qx -- "$mtarget" "$CTEST_LIST" || continue
    case "$mdecl" in
        ./examples/*) ;;
        *) continue ;;   # 根 tests/ 的 target 不在本护栏职责内
    esac
    bin="$(find "$BUILD_DIR/examples" -type f -name "$mtarget" -perm -u+x 2>/dev/null | head -1)"
    if [ -z "$bin" ]; then
        violation "CTest 条目 $mtarget 找不到对应可执行文件（构建产物缺失）"
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
    allowed=0
    printf '%s\n' "$CHERROR_ALLOWLIST" | grep -qx -- "$mtarget|framework-defect-1" && allowed=1

    if [ "$rc" -ne 0 ]; then
        violation "CTest 条目 $mtarget 退出码 $rc（≠ 0）⇒ CTest 会判红。输出末尾：$(printf '%s' "$out" | grep -vE '^\[INFO\]' | tail -2 | tr '\n' ' ')"
    elif [ "$errs" -gt 0 ] && [ "$allowed" -eq 0 ]; then
        violation "CTest 条目 $mtarget 退出码 0 但日志有 $errs 条 error/fatal（「跑但报错」≠ 通过）。首条：$(printf '%s' "$out" | grep -E '^\[(ERROR|FATAL)\]' | head -1)"
    elif [ "$errs" -gt 0 ]; then
        note "OK  $mtarget → exit 0（$errs 条 error/fatal 走 allowlist：framework-defect-1）"
    else
        note "OK  $mtarget → exit 0，日志干净"
    fi
done < "$MAP"

# allowlist 的 stale 检查：条目对应的 target 若已不再打印 error ⇒ MUST 移除该行
echo ""
echo "--- C3b: allowlist stale 检查 ---"
while IFS='|' read -r atarget adefect; do
    [ -z "$atarget" ] && continue
    grep -qx -- "$atarget" "$CTEST_LIST" || { note "（$atarget 不在 CTest 中，跳过）"; continue; }
    abin="$(find "$BUILD_DIR/examples" -type f -name "$atarget" -perm -u+x 2>/dev/null | head -1)"
    [ -z "$abin" ] && continue
    aerrs="$("$abin" 2>&1 | grep -cE '^\[(ERROR|FATAL)\]')"
    if [ "$aerrs" -eq 0 ]; then
        printf "${YELLOW}STALE${NC} allowlist 里的 $atarget 已不再打印 error/fatal ⇒ MUST 从 CHERROR_ALLOWLIST 移除该行\n"
        violations=$((violations+1))
    else
        note "OK  $atarget 仍打印 $aerrs 条（$adefect）"
    fi
done <<EOF
$CHERROR_ALLOWLIST
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
