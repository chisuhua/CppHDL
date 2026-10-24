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
#       （**跨行续行也 MUST 抓到** —— CMake 的多行命令靠**括号配平**判定结束）
#   C3  反向 include：根 tests/** 的任何 TU MUST NOT #include examples/** 路径
#   C4  硬死头：include/cpu/** 与 examples/riscv-mini/src/** 下的头文件若
#       **两个独立证据源都判定无 includer** ⇒ MUST 删除，不得「提升回来」
#   C5  文档一致：include/cpu/AGENTS.md 的 STRUCTURE / KEY FILES 表 MUST 与磁盘一致
#       （无幽灵条目 + **每一处**自述计数与实测一致）—— ADR-046 D9
#   C6  SoC 顶层端口冻结：Rv32iPipelineTop 的端口集合（**名 + 方向 + 位宽**）
#       MUST 与登记快照一致 —— ADR-046 D10
#
# ⚠️ **判据边界（如实声明，不是「已覆盖」的承诺）**：
#   ① C4 的**硬死头**判定要求**两个独立证据源都判定无 includer**：
#      ① 源码级 `#include` 反查（路径感知 + 归一化）；② 编译器依赖文件
#      `build/**/*.o.d`（若存在，且**过滤已删除 target 的陈旧 `.d`**）。
#      ⇒ **任一源说「有 includer」就不判死** ⇒ 只会**漏报**，**绝不**把活头
#        误判成死头（这条是**安全属性**，实现 MUST 保持）。
#        特别地：源码级反查**不认识**的 include 形态（续行 `#include \`、
#        宏 `#include HDR`、仓库根相对路径）⇒ 源①会说「无 includer」⇒
#        此时**必须**由源②（`.d`，编译器真相）兜底 ⇒ 这就是「两源一致」的实现。
#   ② 「源码有 includer 但无 TU 编译它」这一形态**只警告、不违规**（它可能是
#      传递性死，也可能是 target 名抽取盲区 ⇒ 误报会让 agent 删掉**活头**）。
#      **传递性死**本身不覆盖。
#   ③ C2 只覆盖 **CMake 的 `include_directories` / `target_include_directories`
#      调用**（含跨行）；**变量间接**（`set(X ...)` + `${X}`）与
#      `target_include_system_directories` 等**不覆盖**。
#   ④ C4 的 `.d` 证据依赖 `build/` 存在；不存在时（`deps_n == 0`）退化为
#      **纯源码反查**。
#   ⑤ 目标名抽取依赖 CMakeLists 里的**字面量** target 名；用变量拼出的
#      target 抽不到 ⇒ 其 `.d` 被排除 ⇒ 只能靠源①判定（保守，不误报）。
#
# ⚠️ 允许清单（C1 / C4）遵循仓库既有模式（`scripts/verify_example_test_wiring.sh`
#   的 allowlist）：每条豁免 MUST 带**理由**（+ 失效条件），且清单 MUST 记
#   **期望条数** ⇒ 豁免项静默增加 / stale 豁免静默残留 ⇒ 护栏失败。
#   每条生效的豁免 MUST 打印 `::notice`（豁免不是静默的）。
#
# ⚠️ 可移植性：**bash**（用 `local`、数组、`[[ ]]`）+ **POSIX awk**。
#    ⇒ **不使用 GNU 扩展**：特别地 sed 的 `-i`（**不带后缀**）是 GNU-only
#      （BSD sed 要求 `-i ''` 或 `-i.bak`）⇒ 一律用 `sed_inplace()` 助手。
#    ⇒ 空白类一律用 POSIX 明确定义的 `[[:space:]]`；`\t` 在**方括号表达式内**
#      POSIX 未定义（各实现可能分叉：有的当 tab、有的当字面 't'）
#      ⇒ 代码里**不出现** `[[:space:]]` 这种写法。
#    ⚠️ **BSD/BWK awk（macOS 默认）与 BSD sed 未实测** —— 本机只有 mawk + GNU sed。
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

# ---------------------------------------------------------------------------
# 可移植性助手：BSD sed 的 `-i` 需要后缀参数 ⇒ 一律走「写临时文件 + mv」
# ---------------------------------------------------------------------------
sed_inplace() {
    local file="$1"; shift
    sed "$@" "$file" > "$file.cpptmp" && mv "$file.cpptmp" "$file"
}

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
#      MUST NOT 有同名双副本（ADR-046 D1）⇒ 「零例外」本身也被护栏守住。
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
#    快照格式 = <名>:<方向>:<位宽>（**方向与位宽也在冻结范围内** ——
#    对下游 DUT 而言，方向翻转的破坏力等同改名）。
# ---------------------------------------------------------------------------
SOC_TOP_HEADER="examples/riscv-mini/src/rv32i_pipeline_top.h"
SOC_TOP_PORTS="ext_clk:ch_in:ch_bool ext_data_read_data:ch_in:ch_uint<32> ext_data_ready:ch_in:ch_bool ext_instr_data:ch_in:ch_uint<32> ext_instr_ready:ch_in:ch_bool ext_rst:ch_in:ch_bool"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT INT TERM

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

    mk_fixture() {
        local root="$1"
        rm -rf "$root"; mkdir -p "$root/scripts" "$root/include/cpu/pipeline" \
                                "$root/examples/riscv-mini/src" "$root/tests" \
                                "$root/build/tests/CMakeFiles/test_alpha.dir"
        cp "$REPO_ROOT/scripts/verify_no_duplicate_rv32i_headers.sh" "$root/scripts/"
        cat > "$root/include/cpu/AGENTS.md" <<'AGEOF'
## STRUCTURE
```
include/cpu/
└── pipeline/
    └── alpha.h
```
## KEY FILES
| Task | File |
|------|------|
| Alpha | `pipeline/alpha.h` |
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

    # ③b 正常的**跨行** include_directories（指向 include/）MUST NOT 误报
    F="$WORK/fx_i_ok"; mk_fixture "$F"
    cat >> "$F/tests/CMakeLists.txt" <<'CMEOF'
target_include_directories(test_alpha PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/cpu/pipeline
)
CMEOF
    st_check "正常跨行 -I MUST NOT 误报" 0 "$(run_guard "$F")"

    # ④ 反向 include MUST 被抓到
    F="$WORK/fx_inc"; mk_fixture "$F"
    printf '#include "../examples/riscv-mini/src/rv32i_pipeline_top.h"\n' >> "$F/tests/use_alpha.cpp"
    st_check "反向 include MUST 被抓" 1 "$(run_guard "$F")"

    # ⑤ 硬死头（两个源都无 includer）MUST 被抓到
    F="$WORK/fx_dead"; mk_fixture "$F"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/ghost.h"
    st_check "硬死头 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑥ **B1 回归**：活头的 include 形态源级反查不认识（续行 `#include \`）
    #    ⇒ 源①说「无 includer」⇒ MUST 由源②（.d）兜底 ⇒ MUST NOT 报「MUST 删除」
    F="$WORK/fx_cont"; mk_fixture "$F"
    printf '#include \\\n  "cpu/pipeline/alpha.h"\nint main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    st_check "续行 include 的活头 MUST NOT 被判死" 0 "$(run_guard "$F")"

    # ⑥b **B1 回归**：宏形式 `#include HDR`
    F="$WORK/fx_macro"; mk_fixture "$F"
    printf '#define HDR "cpu/pipeline/alpha.h"\n#include HDR\nint main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    st_check "宏形式 include 的活头 MUST NOT 被判死" 0 "$(run_guard "$F")"

    # ⑦ 豁免表条数不符 MUST 被抓到
    F="$WORK/fx_allow_n"; mk_fixture "$F"
    sed_inplace "$F/scripts/verify_no_duplicate_rv32i_headers.sh" 's|^EXPECTED_DEAD_ALLOWANCES=1$|EXPECTED_DEAD_ALLOWANCES=2|'
    st_check "豁免条数不符 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑧ C1 豁免 stale MUST 被抓到（**且必须真的 exit 非 0**）
    F="$WORK/fx_dup_stale"; mk_fixture "$F"
    sed_inplace "$F/scripts/verify_no_duplicate_rv32i_headers.sh" 's|^CH_DUP_ALLOWANCE=""$|CH_DUP_ALLOWANCE="ghost_dup.h\|owner\|reason"|'
    sed_inplace "$F/scripts/verify_no_duplicate_rv32i_headers.sh" 's|^EXPECTED_DUP_ALLOWANCES=0$|EXPECTED_DUP_ALLOWANCES=1|'
    st_check "C1 豁免 stale MUST 被抓" 1 "$(run_guard "$F")"

    # ⑨ C4 豁免 stale MUST 被抓到
    F="$WORK/fx_dead_stale"; mk_fixture "$F"
    sed_inplace "$F/scripts/verify_no_duplicate_rv32i_headers.sh" 's|^CH_DEAD_ALLOWANCE="examples/riscv-mini/src/rv32i_pipeline_top.h.*|CH_DEAD_ALLOWANCE="include/cpu/pipeline/alpha.h\\tx\\ty"|'
    st_check "C4 豁免 stale MUST 被抓" 1 "$(run_guard "$F")"

    # ⑩ **分析器整体失效 MUST fail-closed**：源码反查的候选集合**为空**
    #     （整个仓库一条 include 都没解析出来 ⇒ 绝无可能 ⇒ 分析器坏了）
    F="$WORK/fx_ana"; mk_fixture "$F"
    sed_inplace "$F/scripts/verify_no_duplicate_rv32i_headers.sh" 's|sort -u "\$CAND" > "\$CAND.sorted" 2>/dev/null \|\| true|sort -u /dev/null > "$CAND.sorted" 2>/dev/null \|\| true|'
    st_check "分析器失效 MUST fail-closed" 1 "$(run_guard "$F")"

    # ⑪ C5 文档幽灵条目 MUST 被抓到
    F="$WORK/fx_doc"; mk_fixture "$F"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|    └── alpha.h|    └── ghost_doc.h|'
    st_check "AGENTS.md STRUCTURE 幽灵条目 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑫ C5 KEY FILES 幽灵条目 MUST 被抓到
    F="$WORK/fx_doc_key"; mk_fixture "$F"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|`pipeline/alpha.h`|`pipeline/ghost_keyfile.h`|'
    st_check "AGENTS.md KEY FILES 幽灵条目 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑬ C5 自述计数错 MUST 被抓到
    F="$WORK/fx_doc_cnt"; mk_fixture "$F"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**9 headers, 1 lines**|'
    st_check "AGENTS.md 自述计数错 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑭ C6 SoC 顶层端口变化 MUST 被抓到
    F="$WORK/fx_ports"; mk_fixture "$F"
    sed_inplace "$F/examples/riscv-mini/src/rv32i_pipeline_top.h" 's|        ch_in<ch_bool>      ext_clk;||'
    st_check "SoC 顶层端口变化 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑮ C6 端口**方向**翻转 MUST 被抓到
    F="$WORK/fx_ports_dir"; mk_fixture "$F"
    sed_inplace "$F/examples/riscv-mini/src/rv32i_pipeline_top.h" 's|ch_in<ch_bool>      ext_rst;|ch_out<ch_bool>      ext_rst;|'
    st_check "SoC 顶层端口方向翻转 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑯ C6 端口**位宽**变化 MUST 被抓到
    F="$WORK/fx_ports_w"; mk_fixture "$F"
    sed_inplace "$F/examples/riscv-mini/src/rv32i_pipeline_top.h" 's|ch_in<ch_uint<32>>  ext_instr_data;|ch_in<ch_uint<64>>  ext_instr_data;|'
    st_check "SoC 顶层端口位宽变化 MUST 被抓" 1 "$(run_guard "$F")"

    echo ""
    if [ "$st_fail" -eq 0 ]; then
        echo "${GREEN}✅ self-test: 全部通过${NC}"
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

dup_allow_n="$(printf '%s\n' "$CH_DUP_ALLOWANCE" | grep -c . || true)"
if [ "$dup_allow_n" -ne "$EXPECTED_DUP_ALLOWANCES" ]; then
    violation "同 basename 豁免条数为 $dup_allow_n，期望 $EXPECTED_DUP_ALLOWANCES —— 豁免项 MUST 显式增删（防止静默增加）"
fi
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
# ⚠️ CMake 的多行命令**默认不用反斜杠续行** —— 它靠**括号配平**判定命令结束
#    （仓库自身风格正是如此）⇒ MUST 按括号深度拼接，而不是按反斜杠。
# ---------------------------------------------------------------------------
echo ""
note "--- C2: 根 tests/CMakeLists.txt MUST NOT 有反向 -I ---"
c2_hits="$(awk '
    {
        if (buf == "") start = FNR
        buf = (buf == "") ? $0 : buf " " $0
        o = gsub(/\(/, "&")
        c = gsub(/\)/, "&")
        depth += o - c
        cont = ($0 ~ /\\[[:space:]]*$/)
        if (depth > 0 || cont) next
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
# 5) C4 —— 硬死头判定（**两个证据源都判定无 includer** 才判死）
# ---------------------------------------------------------------------------
echo ""
note "--- C4: 硬死头 MUST 删除（不得提升回来）---"

INC_DIRS="$WORK/inc_dirs.txt"
SRC_LIST="$WORK/src_list.txt"
CONTENTS="$WORK/contents.txt"
CAND="$WORK/candidates.norm"
DEPS="$WORK/deps.norm"

{
    printf '%s\n' include include/core include/ast include/abstract include/utils \
                   include/cpu include/cpu/pipeline include/cpu/riscv include/cpu/cache \
                   src tests external/inipp examples/riscv-mini/src examples/riscv-mini/tests
} > "$INC_DIRS"

find . -type d \( -name build -o -name .git -o -name third_party -o -name .rddf \
              -o -name node_modules -o -name docs \) -prune -o \
    -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.hh' -o -name '*.cpp' \
            -o -name '*.cc' -o -name '*.cxx' -o -name '*.C' -o -name '*.tpp' \
            -o -name '*.ipp' \) -print 2>/dev/null \
    | sed 's|^\./||' | sort > "$SRC_LIST"

# 内容流：哨兵行携带文件路径（便于还原 includer 所在目录）
# ⚠️ 用 while-read 逐个喂 awk（避免未引号的 `$(cat ...)` 在文件名含空格时误切）
: > "$CONTENTS"
while IFS= read -r f; do
    [ -z "$f" ] && continue
    printf '<<<FILE %s>>>\n' "$f" >> "$CONTENTS"
    cat "$f" >> "$CONTENTS" 2>/dev/null || true
done < "$SRC_LIST"

# 5a) 证据源①：候选路径集合
# ⚠️ include 形态的**续行**（`#include \` + 下一行）也要能识别 ⇒ 先拼行。
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
            pending = ""; next
        }
        # 续行拼接：`#include \` + 下一行
        if (pending != "") { line = pending " " $0; pending = "" }
        else line = $0
        if (line ~ /\\[[:space:]]*$/) { sub(/\\[[:space:]]*$/, "", line); pending = line; next }
        if (line ~ /^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^>"]+[>"]/) {
            p = line
            sub(/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]/, "", p)
            sub(/[>"].*$/, "", p)
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", p)
            if (p == "") next
            print norm(dir "/" p) > out
            for (i = 1; i <= nd; i++) print norm(dirs[i] "/" p) > out
        }
    }
' "$INC_DIRS" "$CONTENTS" 2>/dev/null || true
sort -u "$CAND" > "$CAND.sorted" 2>/dev/null || true
mv "$CAND.sorted" "$CAND"

# ⚠️ **分析器健康检查（fail-closed）**：整个仓库一条 include 都解析不出来
#    ⇒ 绝无可能（每个 TU 至少 include 一个头）⇒ 分析器坏了 ⇒ MUST 报违规，
#    **不得**因为「判不出死头」而静默放行。
cand_n="$(wc -l < "$CAND" | tr -d ' ' || echo 0)"
if [ "$cand_n" -eq 0 ]; then
    violation "源码级 include 反查**未解析出任何候选**（整个仓库一条 include 都没有？不可能）—— 分析器失效 MUST 视为违规"
fi

# 5b) 证据源②：编译器的 .d（归一化 + 剥仓库根前缀 + 过滤陈旧 target）
: > "$DEPS"
deps_n=0
if [ -d build ]; then
    CURRENT_TARGETS="$WORK/targets.txt"
    {
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
    done < <(find build -name '*.o.d' -type f 2>/dev/null | sort || true)
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

allow_list="$CH_DEAD_ALLOWANCE"
allow_count="$(printf '%s\n' "$allow_list" | grep -c . || true)"
if [ "$allow_count" -ne "$EXPECTED_DEAD_ALLOWANCES" ]; then
    violation "零消费者头豁免条数为 $allow_count，期望 $EXPECTED_DEAD_ALLOWANCES —— 豁免项 MUST 显式增删（防止静默增加）"
fi

dead_found=0
for h in $LIB_HEADERS $EX_HEADERS; do
    [ -z "$h" ] && continue
    if printf '%s\n' "$allow_list" | awk -v h="$h" '$1 == h { found = 1 } END { exit !found }'; then
        printf "  ::notice 零消费者头豁免生效：%s\n" "$h"
        continue
    fi

    src_has=0
    has_source_includer "$h" && src_has=1
    dep_has=0
    has_dep_includer "$h" && dep_has=1

    # ⚠️ **安全属性**：只有**两个源都**说「无 includer」才判死。
    #    任一源说有 ⇒ 活的 ⇒ 不判死（哪怕另一个源不认识它的 include 形态）。
    if [ "$src_has" -eq 1 ] || [ "$dep_has" -eq 1 ]; then
        if [ "$src_has" -eq 1 ] && [ "$dep_has" -eq 0 ] && [ "$deps_n" -gt 0 ]; then
            # 只警告、不违规（可能是传递性死，也可能是 target 名抽取盲区）
            warning "零消费者头（源码有 includer 但无 TU 编译它，可能是传递性死或 target 名抽取盲区）：$h —— 请人工确认后再删"
        fi
        continue
    fi
    # 两个源都无 ⇒ 硬死头
    if [ "$src_has" -eq 0 ] && [ "$dep_has" -eq 0 ]; then
        dead_found=1
        violation "硬死头：$h —— MUST 删除，不得以「提升回 include/cpu/」保留"
    fi
done

if [ "$dead_found" -eq 0 ]; then
    note "  OK：无硬死头（证据源：源码反查 $cand_n 条候选 + ${deps_n} 条归一化 .d 记录）"
fi

# stale 豁免检查：与主判定**同一谓词**（源级 OR .d 级有消费者 ⇒ 豁免失效）
while IFS= read -r a; do
    [ -z "$a" ] && continue
    ah="$(printf '%s' "$a" | awk '{print $1}')"
    [ -z "$ah" ] && continue
    if [ ! -f "$ah" ]; then
        violation "零消费者头豁免里的 $ah 已不存在 ⇒ MUST 移除该行"
        continue
    fi
    if has_source_includer "$ah" || has_dep_includer "$ah"; then
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
    #    再只取 **basename** 与磁盘比对。
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
    if [ -z "$struct_names" ]; then
        violation "$CPU_AGENTS 的 STRUCTURE 块里未解析出任何 .h 条目 —— 解析器失效 MUST 视为违规"
    fi
    while IFS= read -r nm; do
        [ -z "$nm" ] && continue
        if ! find "$LIB_DIR" -type f -name "$nm" 2>/dev/null | grep -q .; then
            violation "$CPU_AGENTS 的 STRUCTURE 表列出磁盘上不存在的文件：$nm"
        fi
    done <<EOF
$struct_names
EOF

    # 6b) KEY FILES 表里的每个 .h 都必须在磁盘上存在（**路径感知**：
    #     条目可能指向 include/cpu/ 之外，如 `include/chlib/forwarding.h`）
    key_names="$(awk '
        /^## KEY FILES/ { inb = 1; next }
        inb && /^## / { inb = 0 }
        inb { print }
    ' "$CPU_AGENTS" 2>/dev/null | grep -oE '[A-Za-z0-9_*./-]+\.h' | sort -u || true)"
    if [ -z "$key_names" ]; then
        warning "$CPU_AGENTS 的 KEY FILES 表里未解析出任何 .h 条目 —— 无法校验"
    fi
    while IFS= read -r nm; do
        [ -z "$nm" ] && continue
        case "$nm" in
            */*)  # 带路径 ⇒ 相对仓库根解析（找不到再试相对 include/cpu/）
                if [ ! -f "$nm" ] && [ ! -f "$LIB_DIR/$nm" ]; then
                    violation "$CPU_AGENTS 的 KEY FILES 表列出磁盘上不存在的文件：$nm"
                fi
                ;;
            *)    # 纯 basename ⇒ 必须在 include/cpu/ 之下
                if ! find "$LIB_DIR" -type f -name "$nm" 2>/dev/null | grep -q .; then
                    violation "$CPU_AGENTS 的 KEY FILES 表列出磁盘上不存在的文件：$nm"
                fi
                ;;
        esac
    done <<EOF
$key_names
EOF

    # 6c) 自述计数与实测一致（**每一处**都要一致 —— 文档里同一计数可能出现在多处）
    actual_hdr="$lib_n"
    actual_loc="$(find "$LIB_DIR" -type f -name '*.h' -exec cat {} + 2>/dev/null | wc -l | tr -d ' ' || echo 0)"
    claim_re='([0-9]+)[ ]headers?,[ ]([0-9,]+)[ ]lines'
    claim_list="$(grep -oE "$claim_re" "$CPU_AGENTS" 2>/dev/null || true)"
    claim_n="$(printf '%s\n' "$claim_list" | grep -c . || true)"
    if [ "$claim_n" -eq 0 ]; then
        warning "$CPU_AGENTS 里未找到「N headers, M lines」形式的自述计数 —— 无法校验计数"
    fi
    while IFS= read -r cl; do
        [ -z "$cl" ] && continue
        c_hdr="$(printf '%s' "$cl" | grep -oE '[0-9]+' | head -1 || true)"
        c_loc="$(printf '%s' "$cl" | tr -d ',' | grep -oE '[0-9]+' | sed -n '2p' || true)"
        if [ -n "$c_hdr" ] && [ "$c_hdr" != "$actual_hdr" ]; then
            violation "$CPU_AGENTS 自述头文件数 $c_hdr ≠ 实测 $actual_hdr（声明：$cl）"
        fi
        if [ -n "$c_loc" ] && [ "$c_loc" != "$actual_loc" ]; then
            violation "$CPU_AGENTS 自述行数 $c_loc ≠ 实测 $actual_loc（声明：$cl）"
        fi
    done <<EOF
$claim_list
EOF
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
            sub(/^[[:space:]]*ch_(in|out)<.*>[[:space:]]*/, "", line)
            sub(/;.*$/, "", line)
            gsub(/[[:space:]]/, "", line)
            if (line != "") print line
        }
    ' "$SOC_TOP_HEADER" 2>/dev/null | sort | tr '\n' ' ' | sed 's/ *$//' || true)"
    # ⚠️ 快照含**方向与位宽** ⇒ 需从源里一并抽出
    # ⚠️ **嵌套泛型**（`ch_in<ch_uint<32>>`）⇒ MUST 用「**最后一个** `>`」定位类型
    #    的结束，否则 `[^>]*` 会在第一个 `>` 处停下 ⇒ 端口被整条漏掉。
    actual_ports2="$(awk '
        /__io\(/ { inb = 1; next }
        inb && /\)/ { exit }
        inb {
            line = $0
            sub(/^[[:space:]]*/, "", line)
            sub(/;.*$/, "", line)
            if (line !~ /^ch_(in|out)</) next
            k = 0
            for (i = length(line); i > 0; i--) {
                if (substr(line, i, 1) == ">") { k = i; break }
            }
            if (k == 0) next
            typepart = substr(line, 1, k)
            namepart = substr(line, k + 1)
            gsub(/[[:space:]]/, "", namepart)
            if (namepart == "") next
            d = (typepart ~ /^ch_in/) ? "ch_in" : "ch_out"
            ty = typepart
            sub(/^ch_(in|out)</, "", ty)
            sub(/>$/, "", ty)
            print namepart ":" d ":" ty
        }
    ' "$SOC_TOP_HEADER" 2>/dev/null | sort | tr '\n' ' ' | sed 's/ *$//' || true)"
    expected_ports="$(printf '%s\n' $SOC_TOP_PORTS | sort | tr '\n' ' ' | sed 's/ *$//' || true)"
    if [ -z "$actual_ports2" ]; then
        violation "未能从 $SOC_TOP_HEADER 解析出端口集合 —— 解析器失效 MUST 视为违规"
    elif [ "$actual_ports2" != "$expected_ports" ]; then
        violation "SoC 顶层端口集合与快照不一致：\n      期望：$expected_ports\n      实际：$actual_ports2\n    ⇒ ADR-046 D10 冻结端口列表（名 + 方向 + 位宽）⇒ 任何增删改 MUST 显式改脚本里的 SOC_TOP_PORTS"
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
