#!/usr/bin/env bash
# scripts/verify_no_duplicate_rv32i_headers.sh
#
# 护栏：**RV32I 微架构单源化**（ADR-046 `docs/adr/ADR-046-rv32i-canonical-home.md`）
#
# 背景：`include/cpu/` 与 `examples/riscv-mini/src/` 曾是**两套并行架构**的 RV32I，
# 10 组同 basename 头文件靠 `-I` 顺序消歧（**无护栏**）⇒ 调序即**静默换实现**。
# 本护栏把 ADR-046 D1/D4/D7/D9/D10/D11 机械化。
#
# 六条检查：
#   C1  同 basename 双副本：include/cpu/** 与 examples/riscv-mini/src/** 之下
#       同一 basename 的头文件数 MUST ≤ 1
#   C2  反向 -I：根 tests/CMakeLists.txt MUST NOT 出现 examples/riscv-mini/src 的 -I
#       （**跨行续行也 MUST 抓到** —— 仓库自身风格就是跨行的）
#   C3  反向 include：根 tests/** 的任何 TU MUST NOT #include examples/** 路径
#   C4  硬死头：include/cpu/** 与 examples/riscv-mini/src/** 下的头文件若
#       **两个独立证据源都判定无 includer** ⇒ MUST 删除，不得「提升回来」
#   C5  文档一致：include/cpu/AGENTS.md 的 STRUCTURE 表 MUST 与磁盘一致
#       （无幽灵条目 + 自述计数与实测一致）—— ADR-046 D9
#   C6  SoC 顶层端口冻结：Rv32iPipelineTop 的端口集合 MUST 与登记快照一致
#       —— ADR-046 D10
#
# ⚠️ **判据边界（如实声明，不是「已覆盖」的承诺）**：
#   ① C4 的硬死头判定用**两个独立证据源**：① 源码级 `#include` 反查（路径感知：
#      includer 自身目录 + 全仓 include 目录并集，路径**归一化**后再比对）；
#      ② 编译器依赖文件 `build/**/*.o.d`（若存在，且**过滤已删除 target 的陈旧 `.d`**）。
#      **两个源都判定无 includer** 才判死 ⇒ 只会**漏报**（保守）。
#      ⇒ 「源码有 includer 但无 TU 编译它」这一形态**只警告、不违规**
#        （它可能是传递性死，也可能是 target 名抽取盲区 ⇒ 误报会让 agent
#          删掉**活头** ⇒ 方向危险 ⇒ 不计入违规）。
#   ② **传递性死**（includer 本身是死的）**不覆盖**（同上，只警告）。
#   ③ C2 只覆盖 **CMake 的 `include_directories` / `target_include_directories`
#      调用**（含跨行续行）；`set(CMAKE_INCLUDE_PATH ...)`、
#      `target_include_system_directories` 等其它注入形式**不覆盖**。
#   ④ C4 的 `.d` 证据依赖 `build/` 存在；不存在时（`deps_n == 0`）退化为
#      **纯源码反查**。
#   ⑤ 目标名抽取依赖 CMakeLists 里的**字面量** target 名；用变量拼出的
#      target（如 `add_executable(${target} ...)`）抽不到 ⇒ 其 `.d` 被排除
#      ⇒ 只能靠 ① 判定 ⇒ 与 ① 的保守性一致（不误报）。
#
# ⚠️ 允许清单（C1 / C4）遵循仓库既有模式（`scripts/verify_example_test_wiring.sh`
#   的 allowlist）：每条豁免 MUST 带**理由**（+ 失效条件），且清单 MUST 记
#   **期望条数** ⇒ 豁免项静默增加 / stale 豁免静默残留 ⇒ 护栏失败。
#   每条生效的豁免 MUST 打印 `::notice`（豁免不是静默的）。
#
# ⚠️ 可移植性：只用 POSIX sh + awk（**不使用** GNU 扩展）。
#    ⇒ 括号表达式内 MUST 用 `[[:space:]]`（POSIX 明确定义），
#      **不可**用 `[ \t]`（POSIX 未定义 ⇒ BSD/BWK awk 可能分叉）。
#    ⚠️ **BSD/BWK awk（macOS 默认）未实测** —— 本机只有 mawk。
#
# Exit codes: 0 = 全部通过, 1 = 至少一处违规。
#
# Usage: scripts/verify_no_duplicate_rv32i_headers.sh [--self-test]
#
#        --self-test  构造合成树，断言本护栏的 6 条判据**各自有判别力**
#                     （每个变异 MUST 被抓到；干净树 MUST 通过）。
#                     ⇒ 护栏自身的回归测试（否则同类缺陷会重演）。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

violations=0
note()      { printf "  %s\n" "$1"; }
warning()   { printf "${YELLOW}::warning${NC} %s\n" "$1"; }
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
#   ② CH_DEAD_ALLOWANCE —— 硬死头的豁免
#      格式：<路径>|<理由>|<失效条件>
# ---------------------------------------------------------------------------
CH_DUP_ALLOWANCE=""
EXPECTED_DUP_ALLOWANCES=0

CH_DEAD_ALLOWANCE="examples/riscv-mini/src/rv32i_pipeline_top.h	零消费者 —— 其唯一消费者 \`test_pipeline_smoke\` 在 C1（riscv-mini-test-triage-2026-10）被删除（该测试无法写出非空洞断言：端口全是输入，而 ADR-046 D10 冻结了端口列表）	ADR-046 D10 指定它是下游 C3（rv32i-soc-dual-parity-2026-10）的 DUT ⇒ C3 落地时它成为 C3 的 DUT，本条 MUST 移除"
EXPECTED_DEAD_ALLOWANCES=1

# ---------------------------------------------------------------------------
# C6 —— SoC 顶层端口快照（ADR-046 D10）
#
# ⚠️ 端口列表**冻结**：C2 允许改内部装配，MUST NOT 改端口。
#    快照在此登记 ⇒ 任何增删 MUST 显式改本表（可审计的行为）。
# ---------------------------------------------------------------------------
SOC_TOP_HEADER="examples/riscv-mini/src/rv32i_pipeline_top.h"
SOC_TOP_PORTS="ext_rst ext_clk ext_instr_data ext_instr_ready ext_data_read_data ext_data_ready"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT INT TERM

# ---------------------------------------------------------------------------
# 路径归一化（awk 函数 `norm`）—— 把 `a/../b` 与 `a//b` 折叠成规范形式。
# ⚠️ 必须归一化：编译器的 `.d` 里写的是**未归一化**路径
#    （实测：`/…/examples/riscv-mini/tests/../src/rv32i_tcm.h`），
#    而源文件里的 `#include "../../include/cpu/riscv/clint.h"` 从
#    `examples/riscv-mini/tests/` 出发落在 `examples/include/...` ⇒
#    **不归一化就会同时产生假阳性与假阴性**。
# ---------------------------------------------------------------------------

# ===========================================================================
# --self-test：护栏自身的回归测试
# ===========================================================================
if [ "${1:-}" = "--self-test" ]; then
    echo "=== verify_no_duplicate_rv32i_headers: self-test ==="
    st_fail=0
    st_check() {
        local desc="$1" expect="$2" actual="$3"
        if [ "$expect" = "$actual" ]; then
            printf "  %sOK%s   %s（exit=%s）\n" "$GREEN" "$NC" "$desc" "$actual"
        else
            printf "  %sFAIL%s %s（期望 exit=%s，实际 %s）\n" "$RED" "$NC" "$desc" "$expect" "$actual"
            st_fail=$((st_fail+1))
        fi
    }

    # 构造一个**合成仓库**（含最小 CMakeLists + 源文件 + build 目录）
    mk_fixture() {
        local root="$1"
        rm -rf "$root"; mkdir -p "$root/scripts" "$root/include/cpu/pipeline" \
                                "$root/examples/riscv-mini/src" "$root/tests"
        cp "$REPO_ROOT/scripts/verify_no_duplicate_rv32i_headers.sh" "$root/scripts/"
        cat > "$root/include/cpu/AGENTS.md" <<'AGEOF'
## STRUCTURE
```
include/cpu/
└── pipeline/
    └── alpha.h
```
## OVERVIEW
**1 headers, 1 lines**
AGEOF
        cat > "$root/examples/riscv-mini/src/rv32i_pipeline_top.h" <<'TOPEOF'
#pragma once
#include "ch.hpp"
class Rv32iPipelineTop : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      ext_rst;
        ch_in<ch_bool>      ext_clk;
        ch_in<ch_uint<32>>  ext_instr_data;
        ch_in<ch_bool>      ext_instr_ready;
        ch_in<ch_uint<32>>  ext_data_read_data;
        ch_in<ch_bool>      ext_data_ready;
    )
};
TOPEOF
        cat > "$root/include/cpu/pipeline/alpha.h" <<'ALEOF'
#pragma once
ALEOF
        cat > "$root/tests/use_alpha.cpp" <<'USEOF'
#include "cpu/pipeline/alpha.h"
int main() { return 0; }
USEOF
        cat > "$root/tests/CMakeLists.txt" <<'CMEOF'
add_catch_test(test_alpha use_alpha.cpp)
CMEOF
        cat > "$root/tests/test_alpha.cpp" <<'TEOF'
TEST_CASE("x", "[x]") { REQUIRE(1 == 1); }
TEOF
        # build/ 目录：给一个**当前 target** 的 .d（alpha.h 是活的）
        mkdir -p "$root/build/tests/CMakeFiles/test_alpha.dir"
        { printf 'build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o: '
          printf '%s/tests/use_alpha.cpp %s/include/cpu/pipeline/alpha.h\n' "$root" "$root"
        } > "$root/build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o.d"
    }

    run_guard() { ( cd "$1" && bash scripts/verify_no_duplicate_rv32i_headers.sh >/dev/null 2>&1 ) && echo 0 || echo $?; }

    # ① 干净树 MUST 通过
    F="$WORK/fx_clean"; mk_fixture "$F"
    st_check "干净树 MUST 通过" 0 "$(run_guard "$F")"

    # ② 同名副本 MUST 被抓到
    F="$WORK/fx_dup"; mk_fixture "$F"
    cp "$F/include/cpu/pipeline/alpha.h" "$F/examples/riscv-mini/src/alpha.h"
    st_check "同名双副本 MUST 被抓" 1 "$(run_guard "$F")"

    # ③ 反向 -I（**跨行续行**形式 —— 仓库自身风格）MUST 被抓到
    F="$WORK/fx_i_multi"; mk_fixture "$F"
    cat >> "$F/tests/CMakeLists.txt" <<'CMEOF'
target_include_directories(test_alpha PRIVATE
    ${PROJECT_SOURCE_DIR}/examples/riscv-mini/src
)
CMEOF
    st_check "跨行反向 -I MUST 被抓" 1 "$(run_guard "$F")"

    # ④ 反向 include MUST 被抓到
    F="$WORK/fx_inc"; mk_fixture "$F"
    printf '#include "../examples/riscv-mini/src/rv32i_pipeline_top.h"\n' >> "$F/tests/use_alpha.cpp"
    st_check "反向 include MUST 被抓" 1 "$(run_guard "$F")"

    # ⑤ 硬死头（两个源都无 includer）MUST 被抓到
    F="$WORK/fx_dead"; mk_fixture "$F"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/ghost.h"
    st_check "硬死头 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑥ 豁免表条数不符 MUST 被抓到
    F="$WORK/fx_allow_n"; mk_fixture "$F"
    sed -i 's|^EXPECTED_DEAD_ALLOWANCES=1$|EXPECTED_DEAD_ALLOWANCES=2|' "$F/scripts/verify_no_duplicate_rv32i_headers.sh"
    st_check "豁免条数不符 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑦ C1 豁免 stale MUST 被抓到（**且必须真的 exit 非 0**）
    F="$WORK/fx_dup_stale"; mk_fixture "$F"
    sed -i 's|^CH_DUP_ALLOWANCE=""$|CH_DUP_ALLOWANCE="ghost_dup.h\|owner\|reason"|' "$F/scripts/verify_no_duplicate_rv32i_headers.sh"
    sed -i 's|^EXPECTED_DUP_ALLOWANCES=0$|EXPECTED_DUP_ALLOWANCES=1|' "$F/scripts/verify_no_duplicate_rv32i_headers.sh"
    st_check "C1 豁免 stale MUST 被抓" 1 "$(run_guard "$F")"

    # ⑧ C4 豁免 stale MUST 被抓到
    F="$WORK/fx_dead_stale"; mk_fixture "$F"
    sed -i 's|^CH_DEAD_ALLOWANCE="examples/riscv-mini/src/rv32i_pipeline_top.h.*|CH_DEAD_ALLOWANCE="include/cpu/pipeline/alpha.h\\tx\\ty"|' "$F/scripts/verify_no_duplicate_rv32i_headers.sh"
    st_check "C4 豁免 stale MUST 被抓" 1 "$(run_guard "$F")"

    # ⑨ 源码反查失效 MUST fail-closed（不得静默放行）
    F="$WORK/fx_ana"; mk_fixture "$F"
    sed -i 's|sort -u "\$CAND" > "\$CAND.sorted" 2>/dev/null \|\| true|sort -u /dev/null > "$CAND.sorted" 2>/dev/null \|\| true|' "$F/scripts/verify_no_duplicate_rv32i_headers.sh"
    st_check "分析器失效 MUST fail-closed" 1 "$(run_guard "$F")"

    # ⑩ C5 文档幽灵条目 MUST 被抓到
    F="$WORK/fx_doc"; mk_fixture "$F"
    sed -i 's|    └── alpha.h|    └── ghost_doc.h|' "$F/include/cpu/AGENTS.md"
    st_check "AGENTS.md 幽灵条目 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑪ C6 SoC 顶层端口变化 MUST 被抓到
    F="$WORK/fx_ports"; mk_fixture "$F"
    sed -i 's|        ch_in<ch_bool>      ext_clk;||' "$F/examples/riscv-mini/src/rv32i_pipeline_top.h"
    st_check "SoC 顶层端口变化 MUST 被抓" 1 "$(run_guard "$F")"

    echo ""
    if [ "$st_fail" -eq 0 ]; then
        echo "${GREEN}✅ self-test: 11/11 通过${NC}"
        exit 0
    fi
    echo "${RED}❌ self-test: $st_fail 项失败${NC}" >&2
    exit 1
fi

# ===========================================================================
# 正式运行
# ===========================================================================
echo "=== verify_no_duplicate_rv32i_headers: RV32I 单源化护栏 ==="

# ---------------------------------------------------------------------------
# 1) 枚举两个目录下的头文件
# ---------------------------------------------------------------------------
LIB_HEADERS="$(find "$LIB_DIR" -type f -name '*.h' 2>/dev/null | sort || true)"
EX_HEADERS="$(find "$EX_DIR" -type f -name '*.h' 2>/dev/null | sort || true)"

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
    libh="$(find "$LIB_DIR" -type f -name "$b" 2>/dev/null | head -1 || true)"
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
# ⚠️ MUST 用 **heredoc**（不是管道）：管道 = 子 shell ⇒ 计数改动会丢失
#    ⇒ 会「打印 VIOLATION 但 exit 0」（第一轮对抗审稿 BLOCKER-3）。
while IFS='|' read -r ab owner reason; do
    [ -z "$ab" ] && continue
    still=0
    for exh in $EX_HEADERS; do
        [ "$(basename "$exh")" = "$ab" ] || continue
        libh="$(find "$LIB_DIR" -type f -name "$ab" 2>/dev/null | head -1 || true)"
        [ -n "$libh" ] && [ -f "$libh" ] && still=1
    done
    if [ "$still" -eq 0 ]; then
        violation "同 basename 豁免 $ab 已不成立 ⇒ stale，MUST 移除该行"
    fi
done <<EOF
$CH_DUP_ALLOWANCE
EOF

# ---------------------------------------------------------------------------
# 3) C2 —— 根 tests/CMakeLists.txt MUST NOT 有 examples/riscv-mini/src 的 -I
#
# ⚠️ MUST **跨行感知**：CMake 的 `target_include_directories(...)` 常写成
#    「首行 + 续行」，路径在续行上 ⇒ 只匹配含 `include_directories` 的那一行
#    会**漏检**（第一轮对抗审稿 BLOCKER-2）。
#    ⇒ 先把「以 `\` 续行的命令」拼成一条**逻辑行**，再判定。
# ---------------------------------------------------------------------------
echo ""
note "--- C2: 根 tests/CMakeLists.txt MUST NOT 有反向 -I ---"
# ⚠️ CMake 的多行命令**默认不用反斜杠续行** —— 它靠**括号配平**判定命令结束
#    （仓库自身风格正是如此：`target_include_directories(${TEST_NAME} PRIVATE`
#      + 续行 + `)`）⇒ MUST 按括号深度拼接，而不是按反斜杠。
c2_hits="$(awk '
    {
        if (buf == "") start = FNR
        buf = (buf == "") ? $0 : buf " " $0
        o = gsub(/\(/, "&")
        c = gsub(/\)/, "&")
        depth += o - c
        cont = ($0 ~ /\\[ \t]*$/)
        if (depth > 0 || cont) next
        # 一条完整命令
        if (buf ~ /include_directories/ && buf ~ /examples\/riscv-mini\/src/) {
            print FILENAME ":" start ": " buf
        }
        buf = ""; depth = 0
    }
    END {
        if (buf != "" && buf ~ /include_directories/ && buf ~ /examples\/riscv-mini\/src/) {
            print FILENAME ":" start ": " buf
        }
    }
' tests/CMakeLists.txt 2>/dev/null || true)"
if [ -n "$c2_hits" ]; then
    while IFS= read -r hit; do
        [ -z "$hit" ] && continue
        violation "tests/CMakeLists.txt 有指向 examples 源码的 -I：$hit"
    done <<EOF
$c2_hits
EOF
else
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
# 5) C4 —— 硬死头判定（两个证据源都判定无 includer 才判死）
# ---------------------------------------------------------------------------
echo ""
note "--- C4: 硬死头 MUST 删除（不得提升回来）---"

INC_DIRS="$WORK/inc_dirs.txt"
SRC_LIST="$WORK/src_list.txt"
CONTENTS="$WORK/contents.txt"
CAND="$WORK/candidates.norm"
DEPS="$WORK/deps.norm"

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
            sub("/[^/]*$/", "", dir)
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

# 5d) 证据源②：编译器的 .d（归一化 + 剥仓库根前缀 + 过滤陈旧 target）
: > "$DEPS"
deps_n=0
if [ -d build ]; then
    CURRENT_TARGETS="$WORK/targets.txt"
    {
        # ⚠️ `\$` 在 ERE 里是**字面 `$`** ⇒ MUST 写成 `(\$?\{?...)`（可选），
        #    否则 `add_catch_test(name ...)`（'(' 后**没有** '$'）一律不匹配。
        grep -rhoE 'add_[a-z_]+\( *(\$?\{?)[A-Za-z_][A-Za-z0-9_]*' \
            --include=CMakeLists.txt . 2>/dev/null \
        | sed -E 's/.*\( *//' | sed -E 's/^\$\{?//' || true
        grep -rhoE 'riscv_mini_(catch_test(_noctest)?|unit_test)\( *[A-Za-z_][A-Za-z0-9_]*' \
            --include=CMakeLists.txt . 2>/dev/null \
        | sed -E 's/.*\( *//' || true
    } | sort -u > "$CURRENT_TARGETS"

    : > "$WORK/all.d"
    while IFS= read -r dfile; do
        [ -z "$dfile" ] && continue
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
        printf "  ::notice 零消费者头豁免生效：%s\n" "$h"
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
        # ⚠️ **只警告、不违规**：这一形态可能是传递性死，也可能是 target 名抽取
        #    盲区（用变量拼出的 target 抽不到）⇒ 误报会让 agent 删掉**活头**
        #    ⇒ 方向危险 ⇒ 不计入违规（见脚本头部「判据边界」②）。
        warning "零消费者头（源码有 includer 但无 TU 编译它，可能是传递性死或 target 名抽取盲区）：$h —— 请人工确认后再删"
        continue
    fi
    dead_found=1
    violation "硬死头：$h —— MUST 删除，不得以「提升回 include/cpu/」保留"
done

if [ "$dead_found" -eq 0 ]; then
    note "  OK：无硬死头（证据源：源码反查 + ${deps_n} 条归一化 .d 记录）"
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
# 6) C5 —— include/cpu/AGENTS.md MUST 与磁盘一致（ADR-046 D9）
# ---------------------------------------------------------------------------
echo ""
note "--- C5: include/cpu/AGENTS.md MUST 与磁盘一致 ---"
CPU_AGENTS="$LIB_DIR/AGENTS.md"
if [ ! -f "$CPU_AGENTS" ]; then
    violation "$CPU_AGENTS 不存在 —— 组件文档 MUST 存在（ADR-046 D9）"
else
    # 6a) STRUCTURE 代码块里的每个 .h 都必须在磁盘上存在
    struct_block="$(awk '
        /^## STRUCTURE/ { inb = 1; next }
        inb && /^```/ { code = !code; if (!code) exit; next }
        inb && code { print }
    ' "$CPU_AGENTS" 2>/dev/null || true)"
    # ⚠️ 文档里有多���**简写形式**：`stages/{if,id,ex,mem,wb}_stage.h`（花括号展开）
    #    与 `i_cache_sync.h / d_cache_sync.h`（斜杠并列）⇒ MUST 先**展开花括号**，
    #    再只取 **basename** 与磁盘比对（否则会抽出 `wb}_stage.h` 这类垃圾）。
    struct_names="$(printf '%s\n' "$struct_block" | awk '
        {
            line = $0
            while (match(line, /\{[^{}]*\}/)) {
                pre  = substr(line, 1, RSTART - 1)
                inner = substr(line, RSTART + 1, RLENGTH - 2)
                post = substr(line, RSTART + RLENGTH)
                n = split(inner, parts, ",")
                out = ""
                for (i = 1; i <= n; i++) out = out (i > 1 ? " " : "") pre parts[i] post
                line = out
            }
            print line
        }
    ' | grep -oE '[A-Za-z0-9_*]+\.h' | sort -u || true)"
    struct_names="$(printf '%s\n' "$struct_names" | grep -E '\.h$' | sort -u || true)"
    if [ -z "$struct_names" ]; then
        violation "$CPU_AGENTS 的 STRUCTURE 块里未解析出任何 .h 条目 —— 解析器失效 MUST 视为违规"
    fi
    while IFS= read -r nm; do
        [ -z "$nm" ] && continue
        # ⚠️ 文档里允许 **glob** 写法（如 `*_sync.h`）⇒ `find -name` 天然支持
        #    （`-name` 的模式由 shell glob 语法解释）。
        if ! find "$LIB_DIR" -type f -name "$nm" 2>/dev/null | grep -q .; then
            violation "$CPU_AGENTS 的 STRUCTURE 表列出磁盘上不存在的文件：$nm"
        fi
    done <<EOF
$struct_names
EOF

    # 6b) 自述计数与实测一致
    claimed="$(grep -oE '\*\*[0-9]+ headers?,\*\* *[0-9,]+ lines|[0-9]+ headers?, *[0-9,]+ lines' "$CPU_AGENTS" 2>/dev/null | head -1 || true)"
    if [ -n "$claimed" ]; then
        # ⚠️ MUST 先去掉**千分位逗号**（"5,520" ⇒ 5520），否则数字被逗号切碎
        c_hdr="$(printf '%s' "$claimed" | tr -d ',' | grep -oE '[0-9]+' | head -1 || true)"
        c_loc="$(printf '%s' "$claimed" | tr -d ',' | grep -oE '[0-9]+' | sed -n '2p' || true)"
        actual_hdr="$lib_n"
        actual_loc="$(find "$LIB_DIR" -type f -name '*.h' -exec cat {} + 2>/dev/null | wc -l | tr -d ' ' || echo 0)"
        if [ -n "$c_hdr" ] && [ "$c_hdr" != "$actual_hdr" ]; then
            violation "$CPU_AGENTS 自述头文件数 $c_hdr ≠ 实测 $actual_hdr"
        fi
        if [ -n "$c_loc" ] && [ "$c_loc" != "$actual_loc" ]; then
            violation "$CPU_AGENTS 自述行数 $c_loc ≠ 实测 $actual_loc"
        fi
    else
        warning "$CPU_AGENTS 里未找到「N headers, M lines」形式的自述计数 —— 无法校验计数"
    fi
fi

# ---------------------------------------------------------------------------
# 7) C6 —— SoC 顶层端口集合 MUST 与快照一致（ADR-046 D10）
# ---------------------------------------------------------------------------
echo ""
note "--- C6: SoC 顶层端口集合 MUST 与快照一致（ADR-046 D10）---"
if [ ! -f "$SOC_TOP_HEADER" ]; then
    violation "$SOC_TOP_HEADER 不存在 —— SoC 顶层头 MUST 存在（ADR-046 D10）"
else
    actual_ports="$(awk '
        /__io\(/ { inb = 1; next }
        inb && /\)/ { exit }
        inb {
            line = $0
            # ⚠️ 贪心 `.*>`：要剥掉**嵌套**泛型（`ch_in<ch_uint<32>>`）——
            #    `[^>]*` 会在第一个 `>` 处停下 ⇒ 残留 `>ext_instr_data`。
            sub(/^[ \t]*ch_(in|out)<.*>[ \t]*/, "", line)
            sub(/;.*$/, "", line)
            gsub(/[ \t]/, "", line)
            if (line != "") print line
        }
    ' "$SOC_TOP_HEADER" 2>/dev/null | sort | tr '\n' ' ' | sed 's/ *$//' || true)"
    expected_ports="$(printf '%s\n' $SOC_TOP_PORTS | sort | tr '\n' ' ' | sed 's/ *$//' || true)"
    if [ -z "$actual_ports" ]; then
        violation "未能从 $SOC_TOP_HEADER 解析出端口集合 —— 解析器失效 MUST 视为违规"
    elif [ "$actual_ports" != "$expected_ports" ]; then
        violation "SoC 顶层端口集合与快照不一致：\n      期望：$expected_ports\n      实际：$actual_ports\n    ⇒ ADR-046 D10 冻结端口列表 ⇒ 任何增删 MUST 显式改脚本里的 SOC_TOP_PORTS"
    else
        note "  OK：端口集合与快照一致（$(printf '%s\n' $SOC_TOP_PORTS | wc -l | tr -d ' ') 个）"
    fi
fi

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
