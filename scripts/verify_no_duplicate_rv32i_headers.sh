# scripts/verify_no_duplicate_rv32i_headers.sh
#
# 护栏：**RV32I 微架构单源化**（ADR-046 `docs/adr/ADR-046-rv32i-canonical-home.md`）
#
# 背景：`include/cpu/` 与 `examples/riscv-mini/src/` 曾是**两套并行架构**的 RV32I，
# 10 组同 basename 头文件靠 `-I` 顺序消歧（**无护栏**）⇒ 调序即**静默换实现**。
# 本护栏把 ADR-046 D1/D4/D7/D9/D11 机械化。
#
# 四条检查：
#   C1  同 basename 双副本：include/cpu/** 与 examples/riscv-mini/src/** 之下
#       同一 basename 的头文件数 MUST ≤ 1
#   C2  反向 -I：根 tests/CMakeLists.txt MUST NOT 出现 examples/riscv-mini/src 的 -I
#   C3  反向 include：根 tests/** 的任何 TU MUST NOT #include examples/** 路径
#   C4  死头：include/cpu/** 与 examples/riscv-mini/src/** 下的头文件 MUST 有
#       includer（零消费者头 MUST 删除，不得「提升回来」）
#
# ⚠️ C4 的判据边界（如实声明，不是「已覆盖」的承诺）：
#   死头判定用**两个独立证据源**取交集 —— ① 源码级 `#include` 反查（路径感知：
#   includer 自身目录 + 全仓 include 目录并集，路径**归一化**后再比对）；
#   ② 编译器依赖文件 `build/**/*.o.d`（若存在）。**两个源都判定无 includer**
#   才算死 ⇒ 只会**漏报**（保守），不会把活头误判成死头。
#   ⚠️ **传递性死**（includer 本身是死的）**不覆盖**。
#
# ⚠️ 允许清单（C4）遵循仓库既有模式（`scripts/verify_example_test_wiring.sh` 的
#   C3 allowlist）：每条豁免 MUST 带**理由 + 失效条件**，且清单 MUST 记
#   **期望条数** ⇒ 豁免项静默增加 / stale 豁免静默残留 ⇒ 护栏失败。
#
# ⚠️ 可移植性：只用 POSIX sh + awk（**不使用** GNU 扩展）。
#    ⇒ 括号表达式内 MUST 用 `[[:space:]]`（POSIX 明确定义），
#      **不可**用 `[ \t]`（POSIX 未定义 ⇒ BSD/BWK awk 可能分叉）。
#    ⚠️ **BSD/BWK awk（macOS 默认）未实测** —— 本机只有 mawk。
#
# Exit codes: 0 = 全部通过, 1 = 至少一处违规。
#
# Usage: bash scripts/verify_no_duplicate_rv32i_headers.sh

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

violations=0
note()      { printf "  %s\n" "$1"; }
violation() { printf "${RED}VIOLATION${NC} %s\n" "$1" >&2; violations=$((violations+1)); }

LIB_DIR="include/cpu"
EX_DIR="examples/riscv-mini/src"

# ---------------------------------------------------------------------------
# 允许清单（两张）
#
# ⚠️ 沿用 `scripts/verify_example_test_wiring.sh` 的 allowlist 模式：
#   * 每条 MUST 带**理由**（+ 失效条件）
#   * 清单 MUST 记**期望条数** ⇒ 豁免项静默增加 ⇒ 护栏失败
#   * 豁免项一旦 stale（条件不再成立）⇒ 护栏失败（不得静默残留）
#   * 每条生效的豁免 MUST 打印 `::notice`（豁免不是静默的）
#
#   ① CH_DUP_ALLOWANCE  —— 同 basename 双副本的豁免
#      格式：<basename>|<owner-change>|<reason>
#      ⚠️ 当前**故意为空表**：ADR-046 D3/D4 收敛完成后，RV32I 微架构组件
#      MUST NOT 有同名双副本（ADR-046 D1）⇒ 「零例外」本身也被护栏守住
#      （任何人新增一个同名对 ⇒ 立刻红）。
#   ② CH_DEAD_ALLOWANCE —— 零消费者头的豁免
#      格式：<路径>|<理由>|<失效条件>
# ---------------------------------------------------------------------------
CH_DUP_ALLOWANCE=""
EXPECTED_DUP_ALLOWANCES=0

CH_DEAD_ALLOWANCE="examples/riscv-mini/src/rv32i_pipeline_top.h	零消费者 —— 其唯一消费者 \`test_pipeline_smoke\` 在 C1（riscv-mini-test-triage-2026-10）被删除（该测试无法写出非空洞断言：端口全是输入，而 ADR-046 D10 冻结了端口列表）	ADR-046 D10 指定它是下游 C3（rv32i-soc-dual-parity-2026-10）的 DUT ⇒ C3 落地时它成为 C3 的 DUT，本条 MUST 移除"
EXPECTED_DEAD_ALLOWANCES=1

echo "=== verify_no_duplicate_rv32i_headers: RV32I 单源化护栏 ==="

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT INT TERM
INC_DIRS="$WORK/inc_dirs.txt"
SRC_LIST="$WORK/src_list.txt"
CONTENTS="$WORK/contents.txt"
CAND="$WORK/candidates.norm"
DEPS="$WORK/deps.norm"

# ---------------------------------------------------------------------------
# 路径归一化（awk 函数 `norm`）—— 把 `a/../b` 与 `a//b` 折叠成规范形式。
# ⚠️ 必须归一化：编译器的 `.d` 里写的是**未归一化**路径
#    （实测：`/…/examples/riscv-mini/tests/../src/rv32i_tcm.h`），
#    而源文件里的 `#include "../../include/cpu/riscv/clint.h"` 从
#    `examples/riscv-mini/tests/` 出发落在 `examples/include/...` ⇒
#    **不归一化就会同时产生假阳性与假阴性**。
# ---------------------------------------------------------------------------
cat > "$WORK/norm.awk" <<'NORM_AWK'
function norm(p,   parts, n, i, c, out, sp, s) {
    n = split(p, parts, "/")
    sp = 0
    for (i = 1; i <= n; i++) {
        c = parts[i]
        if (c == "" || c == ".") continue
        if (c == "..") { if (sp > 0) sp--; continue }
        out[++sp] = c
    }
    s = ""
    for (i = 1; i <= sp; i++) s = (s == "") ? out[i] : s "/" out[i]
    return s
}
NORM_AWK

# ---------------------------------------------------------------------------
# 1) 枚举两个目录下的头文件
# ---------------------------------------------------------------------------
LIB_HEADERS="$(find "$LIB_DIR" -type f -name '*.h' 2>/dev/null | sort)"
EX_HEADERS="$(find "$EX_DIR" -type f -name '*.h' 2>/dev/null | sort)"

lib_n="$(printf '%s\n' "$LIB_HEADERS" | grep -c . || true)"
ex_n="$(printf '%s\n' "$EX_HEADERS" | grep -c . || true)"

if [ -z "$LIB_HEADERS" ] || [ "$lib_n" -eq 0 ]; then
    violation "$LIB_DIR 下未枚举到任何头文件（目录结构异常？）—— 空集合 MUST 视为违规"
fi
if [ -z "$EX_HEADERS" ] || [ "$ex_n" -eq 0 ]; then
    violation "$EX_DIR 下未枚举到任何头文件（目录结构异常？）—— 空集合 MUST 视为违规"
fi

note "include/cpu 头文件数：$lib_n"
note "example/src 头文件数：$ex_n"

# ---------------------------------------------------------------------------
# 2) C1 —— 同 basename 双副本 MUST ≤ 1
# ---------------------------------------------------------------------------
echo ""
note "--- C1: 同 basename 双副本 MUST ≤ 1 ---"
dup_found=0
for exh in $EX_HEADERS; do
    b="$(basename "$exh")"
    libh="$(find "$LIB_DIR" -type f -name "$b" 2>/dev/null | head -1)"
    [ -n "$libh" ] && [ -f "$libh" ] || continue
    # 豁免命中？
    if printf '%s\n' "$CH_DUP_ALLOWANCE" | awk -v b="$b" -F'|' '$1 == b { found = 1 } END { exit !found }'; then
        printf "  ::notice 同 basename 豁免生效：%s（owner=%s）\n" "$b" \
            "$(printf '%s\n' "$CH_DUP_ALLOWANCE" | awk -v b="$b" -F'|' '$1 == b { print $2; exit }')"
        continue
    fi
    violation "同 basename 双副本：$libh 与 $exh 并存 —— 消歧 MUST NOT 依赖 -I 顺序"
    dup_found=1
done
if [ "$dup_found" -eq 0 ]; then
    note "  OK：无同 basename 双副本"
fi

# 期望条数 + stale 检查（豁免的对已消失 ⇒ 豁免失效 ⇒ 失败）
dup_allow_n="$(printf '%s\n' "$CH_DUP_ALLOWANCE" | grep -c . || true)"
if [ "$dup_allow_n" -ne "$EXPECTED_DUP_ALLOWANCES" ]; then
    violation "同 basename 豁免条数为 $dup_allow_n，期望 $EXPECTED_DUP_ALLOWANCES —— 豁免项 MUST 显式增删（防止静默增加）"
fi
printf '%s\n' "$CH_DUP_ALLOWANCE" | while IFS='|' read -r ab owner reason; do
    [ -z "$ab" ] && continue
    still=0
    for exh in $EX_HEADERS; do
        [ "$(basename "$exh")" = "$ab" ] || continue
        libh="$(find "$LIB_DIR" -type f -name "$ab" 2>/dev/null | head -1)"
        [ -n "$libh" ] && [ -f "$libh" ] && still=1
    done
    if [ "$still" -eq 0 ]; then
        printf "${RED}VIOLATION${NC} 同 basename 豁免 %s 已不成立 ⇒ stale，MUST 移除该行\n" "$ab" >&2
    fi
done

# ---------------------------------------------------------------------------
# 3) C2 —— 根 tests/CMakeLists.txt MUST NOT 有 examples/riscv-mini/src 的 -I
# ---------------------------------------------------------------------------
echo ""
note "--- C2: 根 tests/CMakeLists.txt MUST NOT 有反向 -I ---"
c2_bad=0
if [ -f tests/CMakeLists.txt ]; then
    while IFS= read -r ln; do
        [ -z "$ln" ] && continue
        case "$ln" in
            *examples/riscv-mini/src*)
                violation "tests/CMakeLists.txt 有指向 examples 源码的 -I：$ln"
                c2_bad=1
                ;;
        esac
    done <<EOF
$(grep -n 'include_directories' tests/CMakeLists.txt 2>/dev/null || true)
EOF
fi
if [ "$c2_bad" -eq 0 ]; then
    note "  OK：无反向 -I"
fi

# ---------------------------------------------------------------------------
# 4) C3 —— 根 tests/** 的 TU MUST NOT #include examples/** 路径
# ---------------------------------------------------------------------------
echo ""
note "--- C3: 根 tests/** MUST NOT include examples/** 路径 ---"
c3_bad=0
while IFS= read -r hit; do
    [ -z "$hit" ] && continue
    violation "根 tests/ 反向 include examples/ 路径：$hit"
    c3_bad=1
done <<EOF
$(grep -rn '#include[[:space:]]*[\"<][^\">]*examples/' tests/ 2>/dev/null || true)
EOF
if [ "$c3_bad" -eq 0 ]; then
    note "  OK：无反向 include"
fi

# ---------------------------------------------------------------------------
# 5) C4 —— 死头判定（零消费者头 MUST 删除）
# ---------------------------------------------------------------------------
echo ""
note "--- C4: 零消费者头 MUST 删除（不得提升回来）---"

# 5a) include 目录并集（路径相对于仓库根）
{
    printf '%s\n' include include/core include/ast include/abstract include/utils \
                   include/cpu include/cpu/pipeline include/cpu/riscv include/cpu/cache \
                   src tests external/inipp examples/riscv-mini/src examples/riscv-mini/tests
} > "$INC_DIRS"

# 5b) 源文件清单 + 内容流（哨兵行携带文件路径，便于还原 includer 所在目录）
find . -type d \( -name build -o -name .git -o -name third_party -o -name .rddf \
              -o -name node_modules -o -name docs \) -prune -o \
    -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.hh' -o -name '*.cpp' \
            -o -name '*.cc' -o -name '*.cxx' -o -name '*.C' -o -name '*.tpp' \
            -o -name '*.ipp' \) -print 2>/dev/null \
    | sed 's|^\./||' | sort > "$SRC_LIST"

awk 'FNR == 1 { print "<<<FILE " FILENAME ">>>" } { print }' \
    $(cat "$SRC_LIST") > "$CONTENTS" 2>/dev/null || true

# 5c) 证据源①：候选路径集合（includer 自身目录 / X  ∪  include 目录并集 / X）
awk -v incdirs_file="$INC_DIRS" -v out="$CAND" '
    function norm(p,   parts, n, i, c, out2, sp, s) {
        n = split(p, parts, "/")
        sp = 0
        for (i = 1; i <= n; i++) {
            c = parts[i]
            if (c == "" || c == ".") continue
            if (c == "..") { if (sp > 0) sp--; continue }
            out2[++sp] = c
        }
        s = ""
        for (i = 1; i <= sp; i++) s = (s == "") ? out2[i] : s "/" out2[i]
        return s
    }
    FNR == NR { dirs[++nd] = $0; next }
    {
        if ($0 ~ /^<<<FILE /) {
            dir = $0
            sub(/^<<<FILE /, "", dir); sub(/>>>$/, "", dir)
            sub("/[^/]*$", "", dir)
            if (dir == "") dir = "."
            next
        }
        if ($0 ~ /^[ \t]*#[ \t]*include[ \t]*[<"][^>"]+[>"]/) {
            line = $0
            sub(/^[ \t]*#[ \t]*include[ \t]*[<"]/, "", line)
            sub(/[>"].*$/, "", line)
            if (line == "") next
            print norm(dir "/" line) > out
            for (i = 1; i <= nd; i++) print norm(dirs[i] "/" line) > out
        }
    }
' "$INC_DIRS" "$CONTENTS" 2>/dev/null || true
sort -u "$CAND" > "$CAND.sorted" 2>/dev/null || true
mv "$CAND.sorted" "$CAND"

# 5d) 证据源②：编译器的 .d（归一化后）
: > "$DEPS"
deps_n=0
if [ -d build ]; then
    # ⚠️ **陈旧 .d 会撒谎**（ADR-046 §1.6）：已删除 target 的 `.d` 仍留在 build 目录里
    #    ⇒ 会把「死头」误判成「活头」（假阴性）。
    #    ⇒ 只采信**当前 CMake 声明里存在**的 target 的 `.d`。
    CURRENT_TARGETS="$WORK/targets.txt"
    {
        # ⚠️ `\$` 在 ERE 里是**字面 `$`** ⇒ MUST 写成 `(\$?\{?...)`（可选），
        #    否则 `add_catch_test(name ...)`（'(' 后**没有** '$'）一律不匹配。
        grep -rhoE 'add_[a-z_]+\( *(\$?\{?)[A-Za-z_][A-Za-z0-9_]*' \
            --include=CMakeLists.txt . 2>/dev/null \
        | sed -E 's/.*\( *//' | sed -E 's/^\$\{?//' || true
        grep -rhoE 'riscv_mini_catch_test(_noctest)?\( *[A-Za-z_][A-Za-z0-9_]*' \
            --include=CMakeLists.txt . 2>/dev/null \
        | sed -E 's/.*\( *//' || true
    } | sort -u > "$CURRENT_TARGETS"

    : > "$WORK/all.d"
    while IFS= read -r dfile; do
        [ -z "$dfile" ] && continue
        # 从路径里取 target 名：.../CMakeFiles/<target>.dir/...
        tname="$(printf '%s' "$dfile" | sed -E 's|.*/CMakeFiles/([^/]+)\.dir/.*|\1|')"
        if grep -Fx -- "$tname" "$CURRENT_TARGETS" >/dev/null 2>&1; then
            cat "$dfile" >> "$WORK/all.d" 2>/dev/null || true
        fi
    done <<EOF
$(find build -name '*.o.d' -type f 2>/dev/null | sort || true)
EOF
    if [ -s "$WORK/all.d" ]; then
        tr ' ' '\n' < "$WORK/all.d" | grep -E '\.(h|hpp|hh|tpp|ipp)$' | \
            awk -v out="$DEPS" -v root="$REPO_ROOT/" '
            function norm(p,   parts, n, i, c, o, sp, s) {
                n = split(p, parts, "/")
                sp = 0
                for (i = 1; i <= n; i++) {
                    c = parts[i]
                    if (c == "" || c == ".") continue
                    if (c == "..") { if (sp > 0) sp--; continue }
                    o[++sp] = c
                }
                s = ""
                for (i = 1; i <= sp; i++) s = (s == "") ? o[i] : s "/" o[i]
                return s
            }
            # ⚠️ .d 里是**绝对路径** ⇒ MUST 先剥掉仓库根前缀，
            #    否则归一化后仍带前缀 ⇒ 与相对路径的目标比不上。
            { p = $0; sub("^" root, "", p); p = norm(p); if (p != "") print p > out }
            ' 2>/dev/null || true
        sort -u "$DEPS" > "$DEPS.sorted" 2>/dev/null || true
        mv "$DEPS.sorted" "$DEPS"
        deps_n="$(wc -l < "$DEPS" | tr -d ' ')"
    fi
fi

has_source_includer() {
    [ -s "$CAND" ] || return 1
    grep -Fx -- "$1" "$CAND" >/dev/null 2>&1 && return 0
    return 1
}

has_dep_includer() {
    [ "$deps_n" -gt 0 ] || return 1
    grep -Fx -- "$1" "$DEPS" >/dev/null 2>&1 && return 0
    return 1
}

# 5e) 允许清单
allow_list="$CH_DEAD_ALLOWANCE"
allow_count="$(printf '%s\n' "$allow_list" | grep -c . || true)"

if [ "$allow_count" -ne "$EXPECTED_DEAD_ALLOWANCES" ]; then
    violation "零消费者头豁免条数为 $allow_count，期望 $EXPECTED_DEAD_ALLOWANCES —— 豁免项 MUST 显式增删（防止静默增加）"
fi

dead_found=0
for h in $LIB_HEADERS $EX_HEADERS; do
    [ -z "$h" ] && continue
    # 允许清单命中？（取每行第一列 = 路径）
    if printf '%s\n' "$allow_list" | awk -v h="$h" '$1 == h { found = 1 } END { exit !found }'; then
        continue
    fi

    src_has=0
    has_source_includer "$h" && src_has=1
    dep_has=0
    has_dep_includer "$h" && dep_has=1

    if [ "$src_has" -eq 1 ] && { [ "$deps_n" -eq 0 ] || [ "$dep_has" -eq 1 ]; }; then
        continue
    fi
    if [ "$src_has" -eq 1 ] && [ "$deps_n" -gt 0 ] && [ "$dep_has" -eq 0 ]; then
        # 源码有 includer，但没有任何 TU 编译它 ⇒ 该 includer 本身是死的
        dead_found=1
        violation "零消费者头（源码有 includer 但无 TU 编译它）：$h —— MUST 删除或登记豁免"
        continue
    fi
    dead_found=1
    violation "零消费者头：$h —— MUST 删除，不得以「提升回 include/cpu/」保留"
done

if [ "$dead_found" -eq 0 ]; then
    note "  OK：无零消费者头（证据源：源码反查 + ${deps_n} 条归一化 .d 记录）"
fi

# 5f) stale 豁免检查：清单里的头若重新有了消费者 ⇒ 豁免失效 ⇒ 失败
while IFS= read -r a; do
    [ -z "$a" ] && continue
    ah="$(printf '%s' "$a" | awk '{print $1}')"
    [ -z "$ah" ] && continue
    if [ ! -f "$ah" ]; then
        violation "零消费者头豁免里的 $ah 已不存在 ⇒ MUST 移除该行"
        continue
    fi
    if [ "$deps_n" -gt 0 ] && has_dep_includer "$ah"; then
        violation "零消费者头豁免里的 $ah 已有消费者 ⇒ 豁免 stale，MUST 移除该行"
    fi
done <<EOF
$allow_list
EOF

# ---------------------------------------------------------------------------
# 汇总
# ---------------------------------------------------------------------------
echo ""
if [ "$violations" -eq 0 ]; then
    echo "${GREEN}✅ verify_no_duplicate_rv32i_headers: PASS${NC}"
    exit 0
fi
echo "${RED}❌ verify_no_duplicate_rv32i_headers: $violations 处违规${NC}" >&2
exit 1
