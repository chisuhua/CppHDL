#!/usr/bin/env bash
# scripts/verify_example_test_wiring.sh
#
# 护栏：**examples 内的测试 MUST 有运行覆盖归属**。
#
# 背景（2026-10-23 实测）：`examples/riscv-mini/CMakeLists.txt` 注册了 19 个
# Catch2 测试 target，但 `grep -rn "add_test" examples/` 恒为 0 ⇒ 它们
# **每次 CI 都在编译、却从未被执行过**。更隐蔽的是若干**从未被编译**的测试源文件
# （`test_pipeline_debug2` / `test_hazard_minimal`）—— 既不在 CTest 里、也不在
# 任何清单里 ⇒ 「静默腐坏」。
#
# ═══════════════════════════════════════════════════════════════════════════
# ⚠️ 本护栏的三个设计约束（2026-10-23 对抗审稿 REJECT 后重写，勿退化）
#
# ① 判据是 **`ctest -N`（CTest 条目的唯一真相）**，不是解析 CMake 源码。
#    第一版用「解析 CMake + basename 匹配」做判据，被证伪：
#      - `tests/CMakeLists.txt` 里存在**同名**的 `test_rv32i_pipeline.cpp`
#        ⇒ basename 匹配把 example 侧源文件误判成根 tests 的源文件
#        ⇒ 把 example 侧 target 整个注释掉，护栏仍 exit 0；
#      - 只剥整行注释、不剥行尾注释 ⇒ 行尾注释里的文件名被当成「已使用」；
#      - 只认固定 helper 名 ⇒ 不走 helper 的 `add_executable` 形态完全看不见；
#      - 对**自己治的那个病**（`enable_testing()` 排在 `add_subdirectory(examples)`
#        之后 ⇒ examples 子目录的 `add_test()` 是空操作）零感知。
#
# ② 基准是**文件系统上的源文件集合**（外部事实），不是「CMake 里声明了什么」。
#    否则「删掉一条声明」会让「声明集」与「覆盖集」**同步缩小** ⇒ 数量守恒恒等
#    ⇒ 护栏对缩面**无判别力**（实测：注释掉一个 target 后守恒仍然成立）。
#
# ③ 源文件 → target 的映射 MUST 用**精确路径**，不得用 basename。
#    否则 example 侧的 `test_rv32i_pipeline.cpp` 会被根 `tests/` 的同名文件顶替。
#
# 两条检查：
#   C1  源文件归属守恒：每个 `examples/**/tests/test_*.cpp` MUST 被某个
#                       「已进 CTest」或「已被 example 运行护栏跟踪」的 target 编译；
#                       否则 MUST 登记在 `examples/disabled_examples.txt`
#                       （带理由 + 归位条件）。
#   C2  数量守恒：源文件总数 == 已覆盖数 + 已登记（待归位）数（防静默缩面）。
#
# ⚠️ **不检查**「根 tests/ 反向 include examples/ 源码」—— 那是 ADR-046 §D7 与
#    change `rv32i-canonical-convergence-2026-10`（C2）的范围，
#    由 `scripts/verify_no_duplicate_rv32i_headers.sh` 承担。
#
# Exit codes: 0 = 全部通过, 1 = 至少一处违规。
#
# Usage: bash scripts/verify_example_test_wiring.sh [build-dir]
#        （build-dir 默认 `build`；需要**已 configure 且已 build** 的构建目录
#          —— 判据要用 `ctest -N`）
#
# ⚠️ 可移植性：只用 POSIX sh 语法 + awk/grep/sed/find。
#    **不用** `declare -A`（bash 4 专有，macOS 系统 bash 3.2 不支持）
#    与 `mapfile`（同）。CI 的 macos-14 矩阵跑的就是系统 bash。

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
NC='\033[0m'

violations=0
note()  { printf "  %s\n" "$1"; }
violation() { printf "${RED}VIOLATION${NC} %s\n" "$1" >&2; violations=$((violations+1)); }

DISABLED_FILE="examples/disabled_examples.txt"

echo "=== verify_example_test_wiring: examples 测试归属护栏 ==="

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT INT TERM

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
#    这些 target 自带 main()、是「demo 形态的测试」，由 `run_all_ported_tests.sh`
#    覆盖（不是 CTest）⇒ 它们**有**自动化运行归属。
#    解析方式与 `run_all_ported_tests.sh::tracked_examples()` 一致（单一来源）。
# ---------------------------------------------------------------------------
TRACKED_LIST="$TMP/tracked_list"
sed -n '/set(CH_TRACKED_EXAMPLE_TARGETS/,/^)/p' examples/CMakeLists.txt \
    | sed -n 's/^    \([A-Za-z0-9_]*\)$/\1/p' | sort -u > "$TRACKED_LIST"
note "example 运行护栏跟踪的 target：$(wc -l < "$TRACKED_LIST" | tr -d ' ') 个"

# ---------------------------------------------------------------------------
# 3) disabled 清单里的名字（第 1 列，TAB 分隔）
# ---------------------------------------------------------------------------
DISABLED_LIST="$TMP/disabled_list"
: > "$DISABLED_LIST"
if [ -f "$DISABLED_FILE" ]; then
    sed -n 's/^[[:space:]]*//p' "$DISABLED_FILE" \
        | grep -v '^#' | grep -v '^$' | cut -f1 | sort -u > "$DISABLED_LIST"
fi

# ---------------------------------------------------------------------------
# 4) 源文件 → target 映射（跨**所有** CMakeLists，路径**精确**）
#    形态：<fn>(<target> <src> ...) —— fn ∈ {riscv_mini_catch_test,
#         riscv_mini_unit_test, add_executable, add_catch_test, add_test, ...}
#    归一化：剥掉 ${PROJECT_SOURCE_DIR}/ 前缀；相对路径按该 CMakeLists 所在目录解析。
#    ⇒ 根 tests/ 的 `add_catch_test(test_rv32i_pipeline test_rv32i_pipeline.cpp)`
#       解析成 `tests/test_rv32i_pipeline.cpp`，**不会**与 example 侧的
#       `examples/riscv-mini/tests/test_rv32i_pipeline.cpp` 混淆。
# ---------------------------------------------------------------------------
MAP="$TMP/map.tsv"        # <resolved_src>\t<target>\t<declaring CMakeLists>
: > "$MAP"
ALL_CMAKE="$(find . -name CMakeLists.txt -not -path './build/*' -not -path './.rddf/wt/*' -not -path './third_party/*' | sort)"
for f in $ALL_CMAKE; do
    awk -v out="$MAP" '
    {
        line = $0
        # 剥掉行尾注释（CMake 的 # 是注释起始）
        sub(/[ \t]*#.*$/, "", line)
        # 找 "name(" 的位置
        if (match(line, /[A-Za-z0-9_]*\(/) == 0) next
        rest = substr(line, RSTART + RLENGTH)
        # 第一个参数必须是字面量 target 名（排除 ${...} 模板形态）
        if (rest !~ /^[A-Za-z0-9_.-]+[ \t,]/) next
        n = split(rest, a, /[ \t,)]+/)
        target = a[1]
        if (target == "") next
        # 找最后一个 .cpp token 作为源文件
        m = split(rest, b, /[ \t,)]+/)
        src = ""
        for (i = m; i >= 1; i--) {
            if (b[i] ~ /\.cpp$/) { src = b[i]; break }
        }
        if (src == "") next
        # 剥掉 ${PROJECT_SOURCE_DIR}/ 前缀。
        # ⚠️ 用 index() 而非 sub(/^\$\{PROJECT_SOURCE_DIR\}\//) ——
        # ERE 里 `\{` 的转义语义在不同 awk 实现上不一致（实测 gawk 不匹配）。
        # ⚠️ 剥掉之后该路径是**仓库根相对**的 ⇒ 不能再拼 CMakeLists 所在目录
        #    （否则会变成 `tests/examples/riscv-mini/...`）。
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
        # 统一去掉前导 "./"，与 find 的输出对齐
        sub(/^\.\//, "", src)
        print src "\t" target "\t" FILENAME >> out
    }' "$f"
done
sort -u -o "$MAP" "$MAP"
note "源文件→target 映射条目：$(wc -l < "$MAP" | tr -d ' ') 条"

# ---------------------------------------------------------------------------
# 5) 基准集合：文件系统上的 examples 测试源文件（**外部事实**）
# ---------------------------------------------------------------------------
SRC_LIST="$TMP/src_list"
find examples -path '*/tests/test_*.cpp' -type f | sed 's|^\./||' | sort > "$SRC_LIST"
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
# C2 数量守恒
# ---------------------------------------------------------------------------
echo ""
echo "--- C2: 数量守恒（防静默缩面）---"
note "已覆盖：$n_covered ／ 已登记（待归位）：$n_registered ／ 源文件总数：$n_total"
if [ $((n_covered + n_registered)) -ne "$n_total" ]; then
    violation "数量不守恒：$n_covered + $n_registered = $((n_covered+n_registered)) != $n_total"
else
    note "OK  数量守恒"
fi

# ---------------------------------------------------------------------------
# C3 运行期检查：examples 的每个 CTest 条目 MUST 实际跑出 exit 0
#
# ⚠️ 2026-10-23（对抗审稿 B3）：当一个 Catch2 二进制的**全部**用例都因
#    「仓外资源缺失」而被 SKIP 时，Catch2 v3 的退出码是 **4**
#    （= 没有真正跑起来的用例）⇒ **CTest 把它判为 FAILED**。
#    而 `ctest -N` 仍会列出它 ⇒ 「CTest 里认得它」**不等于**「它会绿」。
#    ⇒ 本条**实际运行**每个 examples 的 CTest 条目，要求退出码 0。
#    （代价：约 12 s —— 与 ctest 跑一遍这些条目的耗时同量级。）
# ---------------------------------------------------------------------------
echo ""
echo "--- C3: CTest 条目 MUST 实际跑出 exit 0 ---"
while IFS="$(printf '\t')" read -r msrc mtarget mdecl; do
    [ -z "$mtarget" ] && continue
    grep -qx -- "$mtarget" "$CTEST_LIST" || continue
    # 只跑 **examples 自己声明** 的 target —— 根 `tests/` 的 target
    # （例如 test_csr_unit 的源文件在 examples/ 下，但 target 属于根 tests）
    # 不在本护栏的职责范围内。
    case "$mdecl" in
        ./examples/*) ;;
        *) continue ;;
    esac
    bin="$(find "$BUILD_DIR/examples" -type f -name "$mtarget" -perm -u+x 2>/dev/null | head -1)"
    if [ -z "$bin" ]; then
        violation "CTest 条目 $mtarget 找不到对应可执行文件（构建产物缺失）"
        continue
    fi
    out="$("$bin" 2>&1)"
    rc=$?
    if [ "$rc" -ne 0 ]; then
        violation "CTest 条目 $mtarget 实际运行退出码 $rc（≠ 0）⇒ CTest 会判红。输出末尾：$(printf '%s' "$out" | grep -vE '^\[INFO\]' | tail -2 | tr '\n' ' ')"
    else
        note "OK  $mtarget → exit 0"
    fi
done < "$MAP"

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
