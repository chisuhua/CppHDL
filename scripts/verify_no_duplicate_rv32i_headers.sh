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
#       **两个独立证据源都判定无 includer**、**且源② 可信** ⇒ MUST 删除，
#       不得「提升回来」
#       ⚠️ 源① = **basename 原文提及**（**不做** include 形态识别）
#          源② = 编译器 `.d` 的 **basename** 记录（可信 ⟺ deps_n > 0 且无非本树 `.d`）
#          源② 不可信 ⇒ 只**警告**、MUST NOT 判死
#       （保守：只漏报，**绝不**误报活头为死头 —— 这是安全属性）
#   C5  文档一致：include/cpu/AGENTS.md 的 STRUCTURE / KEY FILES 表 MUST 与磁盘一致
#       （无幽灵条目 + **每一处**自述计数与实测一致）—— ADR-046 D9
#       ⚠️ **fail-closed**：解析不出条目（表被清空 / 措辞变更 / 解析器坏了）
#          ⇒ MUST 视为违规，不得静默放行（否则「删掉文档表」可让护栏静默失效）。
#   C6  SoC 顶层端口冻结：Rv32iPipelineTop 的端口集合（**名 + 方向 + 位宽**）
#       MUST 与登记快照一致 —— ADR-046 D10
#
# ⚠️ **判据边界（如实声明，不是「已覆盖」的承诺）**：
#   ① C4 的**硬死头**判定要求**两个独立证据源都判定无 includer** *且* 源② **可信**：
#      ① 源① = **basename 原文提及**：某文件里出现 `foo.h` 这个字符串 ⟺ 它
#         **可能** #include 了该头（不论形态：引号 / 尖括号 / 宏 `#include HDR` /
#         续行 / 仓库根相对路径 / `-I` 顺序差异）。⇒ **不做 include 形态识别**
#         ⇒ **不存在「解析失败」状态** ⇒ 上一版 ~420 行的手写解析器（含路径
#         归一化 / `INC_DIRS` / 未识别形态探测三套判据，及其 5 轮审稿各自挖出的
#         新洞）被**整段删除**，而不是继续打补丁。
#         ⚠️ 代价：**只漏报**（注释/文档里提到某个死头的名字 ⇒ 判活）。
#         ⚠️ 自身提及（`/** @file foo.h */`）**按文件归属排除** —— 本仓 31 个头
#            里 29 个这么写；不排除则每个头都恒「活」⇒ C4 完全失去判别力。
#      ② 源② = 编译器依赖文件 `build/**/*.o.d` 的 **basename** 记录。
#      ⇒ **任一源说「有 includer」就不判死** ⇒ 只会**漏报**，
#        **绝不**把活头误判成死头（这条是**安全属性**，实现 MUST 保持）。
#   ② 「源码有 includer 但无 TU 编译它」这一形态**只警告、不违规**（它可能是
#      传递性死，也可能是 target 名抽取盲区 ⇒ 误报会让 agent 删掉**活头**）。
#      **传递性死**本身不覆盖。
#   ③ C2 只覆盖 **CMake 的 `include_directories` / `target_include_directories`
#      调用**（含跨行）；**变量间接**（`set(X ...)` + `${X}`）与
#      `target_include_system_directories` 等**不覆盖**。
#   ④ 源② 的 `.d` **全部参与判活**（**不按 target 名过滤** —— target 名抽取
#      有盲区：`add_executable(${tgt} ...)` 抽不到真名 ⇒ 过滤会丢弃**真实的**
#      `.d` ⇒ 活头被判死 ⇒ 违反安全属性）。仅当某头的证据**只**来自
#      「非当前 target」时给 warning（可能是 stale 产物，也可能是抽取盲区）。
#   ⑤ 目标名抽取只用于 ④ 的 warning；它不影响判活 ⇒ 其盲区不是安全缺口。
#   ⑥ 源②「可信」要求：存在 `.d` 证据（deps_n > 0）**且** 不存在「非本树构建」
#      的 `.d`（仓库被移动 ⇒ `.d` 记旧绝对路径 ⇒ 证据不可信 ⇒ **整文件跳过**
#      并把源②标为不可信，见 `deps_stale_files`）。
#      ⚠️ 它**不**保证覆盖所有 TU（增量构建可能缺件）⇒ 故判死**同时**要求
#      源① 完整运行（见 ⑧）。
#   ⑦ 源① 的**扫描面**：扩展名 = {.h .hpp .hh .cpp .cc .cxx .C .tpp .ipp}；
#      目录 = 仓库根递归但 **prune** `build*/ .git/ third_party/ node_modules/`
#      （`docs/` 与 `.rddf/` 的 TU **在**扫描面内 —— 它们里面的 `.cpp` 片段是
#      真实的 #include 消费者，看不见就会把活头判死）。
#      ⇒ 「RV32I 头的消费者全部在扫描面内」这句声明由**可执行探针**守着：
#      仓库若出现扫描面**之外**的 C/C++ 扩展名（.c/.inl/.hxx/…）⇒ 违规。
#      ⚠️ 扫描面**之外**的消费者（`third_party/`）不计入 —— 本仓头若只被
#      third_party 消费，对它而言等同零消费者（**有意**）。
#   ⑧ 源① 的完整性由**计数**守着（fail-closed）：awk 实际馈入的 TU 个数 MUST
#      == 扫描面文件数（崩溃 / 少读 ⇒ 违规）。上一版那条「prune 目录里若含指向
#      `cpu/` 的真 `#include` ⇒ 违规」的探针已**删除**：它用 `grep -rlE` 匹配
#      **文本**，会把 `.md` 代码块也算成真 include（第五轮审稿 B3）⇒ 它是**误报
#      源**；它想守的属性已被 ⑦（扫描面）+ ⑧（计数）+ ㉖（`.md` 不算 includer）
#      覆盖。
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

# ⚠️ `pwd -P`（**物理**路径）：CMake 记的是物理路径 ⇒ 若仓库在 symlink 下
#    （macOS 的 `/tmp` → `/private/tmp`），逻辑路径会让**每个** `.d` 都被判
#    「非本树构建」⇒ 源② 不可信 ⇒ 护栏全面停摆（方向保守，但闸门形同虚设）。
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
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

CH_DEAD_ALLOWANCE=""
# ⚠️ 豁免表已**清空**（change rv32i-pipeline-commit-path-2026-10，2026-10-25）：
#   `examples/riscv-mini/src/rv32i_pipeline_top.h` 获得了消费者 ——
#   `tests/test_rv32i_soc_dual_parity.cpp` 直接 `#include` 它（SoC 级双后端
#   比对测试的 DUT）。该头此前因「端口全是输入、无观测面」而零消费者；
#   本 change 落地了观测端口契约（ADR-046 D10-rev）⇒ 它成为真 DUT。
#   ⇒ 原豁免行的**失效条件已达成**（`rv32i-canonical-home` 主 spec 的
#     「stale 豁免 MUST 失败」条款要求移除登记）。
EXPECTED_DEAD_ALLOWANCES=0

# ---------------------------------------------------------------------------
# C6 —— SoC 顶层端口快照（ADR-046 D10）
#
# ⚠️ 端口列表**冻结**：C2 允许改内部装配，MUST NOT 改端口。
#    快照格式 = <名>:<方向>:<位宽>（**方向与位宽也在冻结范围内** ——
#    对下游 DUT 而言，方向翻转的破坏力等同改名）。
# ---------------------------------------------------------------------------
SOC_TOP_HEADER="examples/riscv-mini/src/rv32i_pipeline_top.h"
SOC_TOP_PORTS="dbg_branch_taken:ch_out:ch_bool dbg_exmem_alu_result:ch_out:ch_uint<32> dbg_exmem_imm:ch_out:ch_uint<32> dbg_exmem_rs2_data:ch_out:ch_uint<32> dbg_exmem_valid:ch_out:ch_bool dbg_id_instr:ch_out:ch_uint<32> dbg_id_is_branch:ch_out:ch_bool dbg_id_is_load:ch_out:ch_bool dbg_id_is_store:ch_out:ch_bool dbg_id_pc:ch_out:ch_uint<32> dbg_id_valid:ch_out:ch_bool dbg_load_use_detected:ch_out:ch_bool dbg_mem_valid_out:ch_out:ch_bool dbg_wb_rd_addr:ch_out:ch_uint<5> dbg_wb_write_data:ch_out:ch_uint<32> dbg_wb_write_en:ch_out:ch_bool ext_clk:ch_in:ch_bool ext_rst:ch_in:ch_bool obs_data_addr:ch_out:ch_uint<32> obs_data_rdata:ch_out:ch_uint<32> obs_data_write_data:ch_out:ch_uint<32> obs_data_write_en:ch_out:ch_bool obs_instr_addr:ch_out:ch_uint<32> obs_perf_branch_count:ch_out:ch_uint<32> obs_perf_branch_mispredict:ch_out:ch_uint<32> obs_perf_control_stalls:ch_out:ch_uint<32> obs_perf_cycles:ch_out:ch_uint<48> obs_perf_instructions:ch_out:ch_uint<48> obs_perf_load_use_stalls:ch_out:ch_uint<32> prog_addr:ch_in:ch_uint<32> prog_data:ch_in:ch_uint<32> prog_we:ch_in:ch_bool"

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
        # 每个 fixture 树都带一条**自己的**零消费者豁免：
        #   「stale 豁免」判据的 fixture 需要一个**真实的豁免项**才能被检验，
        #   而真实仓库的豁免表现已清空（`rv32i_pipeline_top.h` 获得了消费者）。
        sed_inplace "$root/scripts/verify_no_duplicate_rv32i_headers.sh" \
            's|^CH_DEAD_ALLOWANCE=""$|CH_DEAD_ALLOWANCE="examples/riscv-mini/src/rv32i_pipeline_top.h\tfixture\tfixture"|'
        sed_inplace "$root/scripts/verify_no_duplicate_rv32i_headers.sh" \
            's|^EXPECTED_DEAD_ALLOWANCES=0$|EXPECTED_DEAD_ALLOWANCES=1|'
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
        ch_out<ch_bool>          dbg_branch_taken;
        ch_out<ch_uint<32>>      dbg_exmem_alu_result;
        ch_out<ch_uint<32>>      dbg_exmem_imm;
        ch_out<ch_uint<32>>      dbg_exmem_rs2_data;
        ch_out<ch_bool>          dbg_exmem_valid;
        ch_out<ch_uint<32>>      dbg_id_instr;
        ch_out<ch_bool>          dbg_id_is_branch;
        ch_out<ch_bool>          dbg_id_is_load;
        ch_out<ch_bool>          dbg_id_is_store;
        ch_out<ch_uint<32>>      dbg_id_pc;
        ch_out<ch_bool>          dbg_id_valid;
        ch_out<ch_bool>          dbg_load_use_detected;
        ch_out<ch_bool>          dbg_mem_valid_out;
        ch_out<ch_uint<5>>       dbg_wb_rd_addr;
        ch_out<ch_uint<32>>      dbg_wb_write_data;
        ch_out<ch_bool>          dbg_wb_write_en;
        ch_in<ch_bool>          ext_clk;
        ch_in<ch_bool>          ext_rst;
        ch_out<ch_uint<32>>      obs_data_addr;
        ch_out<ch_uint<32>>      obs_data_rdata;
        ch_out<ch_uint<32>>      obs_data_write_data;
        ch_out<ch_bool>          obs_data_write_en;
        ch_out<ch_uint<32>>      obs_instr_addr;
        ch_out<ch_uint<32>>      obs_perf_branch_count;
        ch_out<ch_uint<32>>      obs_perf_branch_mispredict;
        ch_out<ch_uint<32>>      obs_perf_control_stalls;
        ch_out<ch_uint<48>>      obs_perf_cycles;
        ch_out<ch_uint<48>>      obs_perf_instructions;
        ch_out<ch_uint<32>>      obs_perf_load_use_stalls;
        ch_in<ch_uint<32>>      prog_addr;
        ch_in<ch_uint<32>>      prog_data;
        ch_in<ch_bool>          prog_we;
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

    # ⚠️ 同时把输出落盘（诊断用）：断言只看 exit code 时，
    #    「为什么红了」不可见 ⇒ 失败时可直接看 $WORK/last_guard.log。
    run_guard() {
        ( cd "$1" && bash scripts/verify_no_duplicate_rv32i_headers.sh ) >"$1/last_guard.log" 2>&1 && echo 0 || echo $?
    }

    # ⚠️ **输出级断言**（exit code 不够用时的必要工具）：
    #    有些缺陷「exit code 相同但成因不同」⇒ 靠 exit 无法区分 ⇒ 必须断言输出。
    #    典型例：续行 include 的**拼接**错了（插了空格）⇒ `beta.h` 的 basename
    #    不再出现在文本里 ⇒ 它被误判为硬死头；而幽灵头本来也会让 exit=1
    #    ⇒ 只能靠输出（是否误报「硬死头：…beta.h」）区分。
    #    用法：st_check_out <desc> <期望exit> <输出必须含> <输出不得含> <fixture目录>
    st_check_out() {
        local desc="$1" expect="$2" must="$3" mustnot="$4" dir="$5"
        local out rc fail=""
        out="$( cd "$dir" && bash scripts/verify_no_duplicate_rv32i_headers.sh 2>&1 )" && rc=0 || rc=$?
        [ "$rc" = "$expect" ] || fail="exit=$rc（期望 $expect）"
        if [ -n "$must" ] && ! printf '%s' "$out" | grep -qF -- "$must"; then
            fail="$fail；输出缺 [$must]"
        fi
        if [ -n "$mustnot" ] && printf '%s' "$out" | grep -qF -- "$mustnot"; then
            fail="$fail；输出含 [$mustnot]"
        fi
        if [ -z "$fail" ]; then
            printf "  %sOK%s   %s（exit=%s）\n" "$GREEN" "$NC" "$desc" "$rc"
        else
            printf "  %sFAIL%s %s：%s\n" "$RED" "$NC" "$desc" "$fail"
            st_fail=$((st_fail+1))
        fi
    }

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
    sed_inplace "$F/scripts/verify_no_duplicate_rv32i_headers.sh" 's|^CH_DEAD_ALLOWANCE="examples/riscv-mini/src/rv32i_pipeline_top.h.*|CH_DEAD_ALLOWANCE="include/cpu/pipeline/alpha.h\tx\ty"|'
    st_check "C4 豁免 stale MUST 被抓" 1 "$(run_guard "$F")"

    # ⑨b **纯注释里的提及 MUST NOT 被当成「已有消费者」**（BLOCKER-2 回归）
    #     源① 的语义是「**任何原文提及** ⇒ 可能 include 过」⇒ 它在「判死」方向
    #     是保守的（漏报，安全）；但若用同一谓词证明「**已有真实消费者**」，
    #     方向相反 ⇒ 注释提及即产生**假违规**，且错误信息会指示「MUST 移除该行」
    #     ⇒ 移除后护栏报「硬死头 MUST 删除」⇒ agent 删掉 ADR-046 D10 **冻结**的
    #     C3 DUT（rv32i_pipeline_top.h）。
    #     ⇒ 期望：exit=0（不报违规）+ 输出含**那条具体的** warning。
    #     ⚠️ 断言串 MUST 够具体（第七轮审稿 SHOULD_FIX-1）：`::warning` 会被
    #        **任何** warning 满足 ⇒ 将来新增一条无关 warning 就让 fixture 空转。
    F="$WORK/fx_stale_comment"; mk_fixture "$F"
    printf '// 参考 rv32i_pipeline_top.h 的实现思路（纯注释，不 include）\nint main(){return 0;}\n' \
        > "$F/tests/mention_only.cpp"
    st_check_out "纯注释提及 MUST NOT 判豁免 stale（只警告）" 0 \
        "在**源码里被提及**" "VIOLATION" "$F"

    # ⑨c **当前的真 include ⇒ 豁免 stale MUST 被抓**
    #     ⚠️ `.d` 与源必须**一致**：它引用的 TU 真的 include 了那个头
    #        （否则这个 `.d` 本身就是「陈旧 .d」的形状，断言的是另一件事）。
    #     ⚠️ **mtime MUST 显式设定**（第八轮审稿 BLOCKER-2）：本机 fs 的 tick
    #        粒度可能让「写 TU」与「写 .d」落在**同一秒** ⇒ 时间差判据为假 ⇒
    #        fixture 自己转红（flaky）⇒ 用 `touch -t` 钉住「.d 比 TU 至少新 1 年」
#        （时间跨度 1 年 ⇒ 消除任何 tick 粒度的影响）。
    F="$WORK/fx_stale_dep"; mk_fixture "$F"
    printf '#include "rv32i_pipeline_top.h"\nint main(){return 0;}\n' \
        > "$F/tests/use_top.cpp"
    mkdir -p "$F/build/tests/CMakeFiles/test_top.dir"
    { printf 'build/tests/CMakeFiles/test_top.dir/use_top.cpp.o: '
      printf '%s/tests/use_top.cpp %s/examples/riscv-mini/src/rv32i_pipeline_top.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    touch -t 202001010000 "$F/tests/use_top.cpp"
    touch -t 202101010000 "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    st_check_out "当前的真 include ⇒ 豁免 stale MUST 被抓" 1 \
        "已有**当前**消费者" "" "$F"

    # ⑨e **「源文件不再 include 它」⇒ MUST NOT 判 stale**（当前性判据 (b) 的覆盖）
    #     源文件仍在、`.d` 仍提到它，但 TU 里的 include 已被删（只剩注释提及）
    #     ⇒ 没有**当前**的消费者 ⇒ 只警告。
    #     ⚠️ 注释的**形状**必须让「(b) 被移除后的回落正则」**也能匹配**
    #        （第九轮审稿 BLOCKER-1）：回落正则是
    #        `(^|[^A-Za-z0-9_.-])NAME(["'>]|$)` ⇒ 提及后必须跟 `"` / `'` / `>`
    #        / 行尾。⇒ 用 `"rv32i_pipeline_top.h"`（带引号）而不是全角逗号
    #        （全角逗号既不在 `["'>]` 里也不是行尾 ⇒ 回落也不匹配 ⇒ fixture 空转）。
    F="$WORK/fx_stale_nolonger"; mk_fixture "$F"
    printf '// 曾经 include 过 "rv32i_pipeline_top.h"，现在改用别的实现\nint main(){return 0;}\n' \
        > "$F/tests/use_top.cpp"
    mkdir -p "$F/build/tests/CMakeFiles/test_top.dir"
    { printf 'build/tests/CMakeFiles/test_top.dir/use_top.cpp.o: '
      printf '%s/tests/use_top.cpp %s/examples/riscv-mini/src/rv32i_pipeline_top.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    touch -t 202001010000 "$F/tests/use_top.cpp"
    touch -t 202101010000 "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    st_check_out "源文件不再 include 它 ⇒ MUST NOT 判 stale（判据 b）" 0 \
        "无当前证据" "已有**当前**消费者" "$F"

    # ⑨f **`.d` 比源文件旧 ⇒ MUST NOT 判 stale**（当前性判据 (c) 的覆盖）
    #     ⚠️ (c) 是**唯一**挡住「假违规 + 破坏性指令」的闸门 ⇒ 必须有覆盖。
    F="$WORK/fx_stale_older"; mk_fixture "$F"
    printf '#include "rv32i_pipeline_top.h"\nint main(){return 0;}\n' \
        > "$F/tests/use_top.cpp"
    mkdir -p "$F/build/tests/CMakeFiles/test_top.dir"
    { printf 'build/tests/CMakeFiles/test_top.dir/use_top.cpp.o: '
      printf '%s/tests/use_top.cpp %s/examples/riscv-mini/src/rv32i_pipeline_top.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    touch -t 202101010000 "$F/tests/use_top.cpp"
    touch -t 202001010000 "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    st_check_out "「.d」比源文件旧 ⇒ MUST NOT 判 stale（判据 c）" 0 \
        "无当前证据" "已有**当前**消费者" "$F"

    # ⑨g **「行首是 include 但 basename 只在行尾注释里」MUST NOT 判 stale**
    #     （第十轮审稿 SF-1：判据 (b) MUST 比对 include 的**操作数**，
    #     而不是「行首是 `#include` 的整行」）
    F="$WORK/fx_stale_linecomment"; mk_fixture "$F"
    printf '#include "other.h" // rv32i_pipeline_top.h\nint main(){return 0;}\n' \
        > "$F/tests/use_top.cpp"
    mkdir -p "$F/build/tests/CMakeFiles/test_top.dir"
    { printf 'build/tests/CMakeFiles/test_top.dir/use_top.cpp.o: \\\n'
      printf ' %s/tests/use_top.cpp \\\n' "$F"
      printf ' %s/examples/riscv-mini/src/rv32i_pipeline_top.h \\\n' "$F"
      printf ' %s/include/ch.hpp\n' "$F"
    } > "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    touch -t 202001010000 "$F/tests/use_top.cpp"
    touch -t 202101010000 "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    st_check_out "行尾注释里的提及 MUST NOT 判 stale（判据 b 的操作数收窄）" 0 \
        "无当前证据" "已有**当前**消费者" "$F"

    # ⑨h **`#if 0` 非激活区里的字面 include ⇒ 仍被计入证据**（第十一轮审稿
    #     SF-C 的**如实记录** fixture：钉住**实际**行为 —— 预处理器语义不在
    #     判据 (b) 内 ⇒ 该形态**会**报 stale，这是**已知误报向量**）
    #     ⇒ 本 fixture 钉住它：将来若真修（跳过非激活区）⇒ 本 fixture 转红
    #     ⇒ MUST 同步改契约（spec / ADR / AGENTS 的「已知误报向量」段）。
    F="$WORK/fx_stale_if0"; mk_fixture "$F"
    printf '#if 0\n#include "rv32i_pipeline_top.h"\n#endif\nint main(){return 0;}\n' \
        > "$F/tests/use_top.cpp"
    mkdir -p "$F/build/tests/CMakeFiles/test_top.dir"
    { printf 'build/tests/CMakeFiles/test_top.dir/use_top.cpp.o: \\\n'
      printf ' %s/tests/use_top.cpp \\\n' "$F"
      printf ' %s/examples/riscv-mini/src/rv32i_pipeline_top.h \\\n' "$F"
      printf ' %s/include/ch.hpp\n' "$F"
    } > "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    touch -t 202001010000 "$F/tests/use_top.cpp"
    touch -t 202101010000 "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    st_check_out "「#if 0」非激活区仍计入证据（已知误报向量，如实钉住）" 1 \
        "已有**当前**消费者" "" "$F"

    # ⑩g **真实形状的多行 `.d`** ⇒ 豁免 stale MUST 被抓（第八轮审稿 BLOCKER-1）
    #      真实 CMake/GCC 的 `.d` 是**续行**格式：
    #        target.o: \
    #         /abs/src.cpp \
    #         /abs/hdr.h \
    #      ⇒ 源文件路径在**续行**里 ⇒ 提取 MUST 处理续行
    #      （`sed -n 's/^[^:]*: //p'` 只处理第一行 ⇒ 真实 `.d` 上恒为空）。
    F="$WORK/fx_stale_multiline"; mk_fixture "$F"
    printf '#include "rv32i_pipeline_top.h"\nint main(){return 0;}\n' \
        > "$F/tests/use_top.cpp"
    mkdir -p "$F/build/tests/CMakeFiles/test_top.dir"
    { printf 'build/tests/CMakeFiles/test_top.dir/use_top.cpp.o: \\\n'
      printf ' %s/tests/use_top.cpp \\\n' "$F"
      printf ' %s/examples/riscv-mini/src/rv32i_pipeline_top.h \\\n' "$F"
      printf ' %s/include/ch.hpp\n' "$F"
    } > "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    touch -t 202001010000 "$F/tests/use_top.cpp"
    touch -t 202101010000 "$F/build/tests/CMakeFiles/test_top.dir/use_top.cpp.o.d"
    st_check_out "真实形状的多行 .d ⇒ 豁免 stale MUST 被抓" 1 \
        "已有**当前**消费者" "" "$F"

    # ⑨d **陈旧 `.d` MUST NOT 判豁免 stale**（第七轮审稿 BLOCKER-1）
    #     `.d` 是**累积**构建产物：TU 被删后 `.d` 仍留着 ⇒ 「`.d` 里出现过」
    #     **不等于**「现在有消费者」⇒ 用它报违规会给出**破坏性**指令
    #     （删豁免行 ⇒ 随后护栏报「硬死头 MUST 删除」⇒ 删掉 D10 冻结的 C3 DUT）。
    #     ⇒ 陈旧 `.d`（引用的 TU 已不存在）MUST 只警告、不报违规。
    F="$WORK/fx_stale_old_dep"; mk_fixture "$F"
    mkdir -p "$F/build/tests/CMakeFiles/test_gone.dir"
    { printf 'build/tests/CMakeFiles/test_gone.dir/gone.cpp.o: '
      printf '%s/tests/already_deleted.cpp %s/examples/riscv-mini/src/rv32i_pipeline_top.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_gone.dir/gone.cpp.o.d"
    st_check_out "陈旧 .d（引用的 TU 已删除）MUST NOT 判豁免 stale" 0 \
        "无当前证据" "已有**当前**消费者" "$F"

    # ⑩h **非构建输出目录（无 CMakeCache.txt）里的 TU MUST 被扫到**
    #     （钉住 prune 的**内容判定**）
    #     ⚠️ 断言方式 MUST 是**扫描面 TU 计数**，不是「活头是否被判死」：
    #        源① 的语义是「任何原文提及 ⇒ 可能 include 过」⇒ fixture 的
    #        `include/cpu/AGENTS.md` 里必然提到 alpha.h ⇒ 它**恒活** ⇒
    #        用「判活」断言会**空转**（第八轮审稿实测）。
    F="$WORK/fx_prune_content"; mk_fixture "$F"
    tu_count() { ( cd "$1" && bash scripts/verify_no_duplicate_rv32i_headers.sh 2>&1 ) \
        | sed -n 's/.*扫描面 \([0-9][0-9]*\) 个 TU.*/\1/p' | head -1; }
    base_tu="$(tu_count "$F")"
    mkdir -p "$F/build-tools"
    printf 'int main(){return 0;}\n' > "$F/build-tools/consumer.cpp"
    new_tu="$(tu_count "$F")"
    if [ -n "$base_tu" ] && [ -n "$new_tu" ] && [ "$new_tu" -eq $((base_tu + 1)) ]; then
        printf "  ${GREEN}OK${NC}   非构建目录（无 CMakeCache.txt）里的 TU MUST 被扫到（扫描面 %s → %s）\n" \
            "$base_tu" "$new_tu"
    else
        printf "  ${RED}FAIL${NC} 非构建目录（无 CMakeCache.txt）里的 TU MUST 被扫到（扫描面 %s → %s，期望 +1）\n" \
            "${base_tu:-?}" "${new_tu:-?}"
        st_fail=$((st_fail+1))
    fi

    # ⑩i **非 `build*` 命名的构建输出目录 MUST NOT 触发「扫描面之外」违规**
    #     （`cmake -B out` 是合理用法 ⇒ 名字锚定 ⇒ 假违规 ⇒ CI 被无关布局阻断）
    F="$WORK/fx_prune_outdir"; mk_fixture "$F"
    mkdir -p "$F/out/verilator-build/src"
    printf 'int generated;\n' > "$F/out/verilator-build/src/V3ParseBison.c"
    printf 'cmake\n' > "$F/out/verilator-build/CMakeCache.txt"
    st_check_out "构建输出目录（名字非 build*）里的生成 .c MUST NOT 报违规" 0 \
        "" "扫描面之外" "$F"

    # ⑩j **允许清单的格式 MUST 被校验**（3 个 TAB 分隔字段）
    F="$WORK/fx_allow_fmt"; mk_fixture "$F"
    sed_inplace "$F/scripts/verify_no_duplicate_rv32i_headers.sh" \
        's|^CH_DEAD_ALLOWANCE="examples/riscv-mini/src/rv32i_pipeline_top.h.*|CH_DEAD_ALLOWANCE="examples/riscv-mini/src/rv32i_pipeline_top.h 没有TAB分隔"|'
    st_check "允许清单格式非法（缺 TAB 分隔）MUST 被抓" 1 "$(run_guard "$F")"

    # ⑩ **源① 分析器崩溃 MUST fail-closed**
    #     判据 = 「awk 实际馈入的 TU 个数 MUST == 扫描面文件数」⇒ 崩溃 / 少读即违规
    #     （第六轮起：源① 不再有「解析器」⇒ 该判据取代了旧版的「候选集合为空」）
    #     ⚠️ 变异点随实现演进（`-v` 传递已改成 ENVIRON）⇒ sed 模式 MUST 跟着改，
    #        否则 fixture **空转**（变异没生效 ⇒ 期望的 exit 变化不出现）。
    F="$WORK/fx_ana"; mk_fixture "$F"
    sed_inplace "$F/scripts/verify_no_duplicate_rv32i_headers.sh" \
        's|^    xargs -0 awk |    xargs -0 true awk |'
    st_check "源① 分析器崩溃 MUST fail-closed" 1 "$(run_guard "$F")"

    # ⑩b **源① 扫描面为空 MUST fail-closed**（分析器根本没读到任何文件）
    F="$WORK/fx_ana_empty"; mk_fixture "$F"
    sed_inplace "$F/scripts/verify_no_duplicate_rv32i_headers.sh" \
        's|^src_n="\$(grep -c . "\$SRC_LIST" 2>/dev/null \|\| true)"$|src_n=0|'
    st_check "源① 扫描面为空 MUST fail-closed" 1 "$(run_guard "$F")"

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
    sed_inplace "$F/examples/riscv-mini/src/rv32i_pipeline_top.h" 's|ch_in<ch_bool> *ext_clk;||'
    st_check "SoC 顶层端口变化 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑮ C6 端口**方向**翻转 MUST 被抓到
    F="$WORK/fx_ports_dir"; mk_fixture "$F"
    sed_inplace "$F/examples/riscv-mini/src/rv32i_pipeline_top.h" 's|ch_in<ch_bool> *ext_rst;|ch_out<ch_bool> ext_rst;|'
    st_check "SoC 顶层端口方向翻转 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑯ C6 端口**位宽**变化 MUST 被抓到
    F="$WORK/fx_ports_w"; mk_fixture "$F"
    sed_inplace "$F/examples/riscv-mini/src/rv32i_pipeline_top.h" 's|ch_in<ch_uint<32>> *prog_data;|ch_in<ch_uint<64>> prog_data;|'
    st_check "SoC 顶层端口位宽变化 MUST 被抓" 1 "$(run_guard "$F")"

    # ⑰ **续行状态 MUST 按文件重置，且文件末尾的悬空续行 MUST 参与判定**
    #    形状：`include/cpu/aaa_dangling.cpp` 最后一行是 `/* see alpha.h */ \`（悬空续行）
    #      ⇒ ① 该「残行」的内容 MUST 被检索 ⇒ alpha.h 判活（else 误杀活头）；
    #         ② 它 MUST NOT 被并进**下一个文件**的首行。
    #    ⚠️ **形状要求（第六轮审稿 BLOCKER-1）**：悬空续行文件 MUST 在**排序上
    #       紧邻目标头之前**！否则 ② 这条属性**根本没被构造出来** ⇒ 该 fixture
    #       会「空转」（实测：把它放在 `docs/` 下时，它的下一个文件是
    #       `examples/riscv-mini/src/rv32i_pipeline_top.h`，**不是** alpha.h ⇒
    #       跨文件归属错误的后果不可见 ⇒ 变异 `self[last]`→`self[FILENAME]`
    #       仍能 33/33 全绿）。
    #       本 fixture 用 `include/cpu/aaa_dangling.cpp` ⇒ 排序后紧邻
    #       `include/cpu/pipeline/alpha.h` ⇒ 变异 `self[last]`→`self[FILENAME]`
    #       会让 alpha.h 的唯一提及被归到它自己 ⇒ 自身提及排除吃掉它 ⇒
    #       **活头被判死**（exit=1）⇒ 本 fixture 变红 ⇒ 属性被钉住。
    #    ⇒ 期望 exit=0，且**不是靠源② 兜住**：`.d` 只引用 unrelated.h。
    F="$WORK/fx_cont_parser"; mk_fixture "$F"
    printf 'int main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    printf '/* see alpha.h */ \\\n' > "$F/include/cpu/aaa_dangling.cpp"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/unrelated.h"
    { printf 'build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o: '
      printf '%s/tests/use_alpha.cpp %s/include/cpu/pipeline/unrelated.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o.d"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**2 headers, 2 lines**|'
    st_check "文件末尾悬空续行 MUST 参与判定且 MUST NOT 跨文件拼接" 0 "$(run_guard "$F")"

    # ⑰b **续行 include 单独可判活**：续行 + **无 .d** ⇒ 源① MUST 认出（不判死）
    F="$WORK/fx_cont_nod"; mk_fixture "$F"
    rm -rf "$F/build"
    printf '#include \\\n  "cpu/pipeline/alpha.h"\nint main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    st_check "续行 include（无 .d 佐证）MUST NOT 被判死" 0 "$(run_guard "$F")"

    # ⑰c **续行落在「basename 内」MUST 被零插入拼接**（BLOCKER-1 回归）
    #     `#include "cpu/pipeline/be` + `ta.h"` —— 真实 C++ 语义是
    #     「删除 `\` 与换行、其余**原样连接**」⇒ 拼成 `"cpu/pipeline/beta.h"`。
    #     ⚠️ 若拼接时**插入空格** ⇒ 得到 `be ta.h` ⇒ beta.h 的 basename 不再
    #     出现 ⇒ 该活头被误判成「硬死头 MUST 删除」⇒ 安全属性失效。
    #     断言用**输出级**检查（exit code 相同但成因不同 ⇒ exit 不可区分）：
    #       ① exit=1（幽灵头被判死 ⇒ 源① 确实跑到了）；
    #       ② 输出**不含**「硬死头：…beta.h」（beta.h 是活的，不该被误判）。
    F="$WORK/fx_cont_path"; mk_fixture "$F"
    printf '#include "cpu/pipeline/be\\\nta.h"\nint main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/beta.h"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/ghost.h"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**3 headers, 3 lines**|'
    st_check_out "续行落在 basename 内 MUST 零插入拼接（beta.h 不被误判死）" 1 \
        "源② 状态：可信" "硬死头：include/cpu/pipeline/beta.h" "$F"

    # ⑰d **自身 `@file` 提及 MUST NOT 算作 includer**
    #     ⚠️ 本仓每个头都在 `/** @file foo.h */` 里写自己的文件名（31 个头里 29 个）
    #        ⇒ 若按「整仓出现过该 basename」判定，每个头都恒「活」⇒ C4 失去判别力。
    #     形状：alpha.h 只在**自身**注释里出现；`.d` 只引用 unrelated.h（不引用它）
    #        ⇒ alpha.h MUST 被判死（exit=1）。若自身提及被算作证据 ⇒ exit=0 ⇒ 判别成立。
    F="$WORK/fx_self_mention"; mk_fixture "$F"
    printf '/** @file alpha.h */\n#pragma once\n' > "$F/include/cpu/pipeline/alpha.h"
    printf 'int main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/unrelated.h"
    { printf 'build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o: '
      printf '%s/tests/use_alpha.cpp %s/include/cpu/pipeline/unrelated.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o.d"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**2 headers, 3 lines**|'
    st_check "头在 @file 里提及自身名字 MUST NOT 被当作 includer" 1 "$(run_guard "$F")"

    # ⑱ ~~源① 不可信 ⇒ 不判死~~ —— **该断言类别已被结构性消除**（第六轮）：
    #    源① 不再是「解析器」而是 basename 原文搜索 ⇒ **不存在**「未识别的 include
    #    形态」⇒ 没有「不完整」状态可断言。宏形式 include（`#include HDR`）由 ⑥b
    #    覆盖；「只有 `.d` 能看见」的形态由 ⑳ 覆盖。

    # ⑲ **源② 不可信 ⇒ 不判死**：幽灵头 + **无 .d**（deps_n==0）
    #    ⇒ 单源判定 MUST NOT 判死（旧实现在此退化为单源判死 = B1）
    F="$WORK/fx_nod_ghost"; mk_fixture "$F"
    rm -rf "$F/build"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/ghost.h"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**2 headers, 2 lines**|'
    st_check "无 .d 证据时幽灵头 MUST NOT 被判死（保守）" 0 "$(run_guard "$F")"

    # ⑳ **`.d` MUST NOT 按 target 名过滤**：活头 alpha.h 的**唯一**证据是一个
    #    「名字抽不到」的 target 目录下的 `.d` —— 源① 看不见它，因为它的 includer
    #    是 `build/` 里的**生成 TU**，而 `build*/` 在源① 扫描面之外。
    # ⚠️ **判别力要求**：
    #    ① 必须额外提供一个「**当前** target 的 `.d`」把 `deps_n` 抬到 > 0
    #       ⇒ 否则「按 target 名过滤」的变异会同时让 `deps_n == 0` ⇒ 触发
    #       「源②不可信 ⇒ 不判死」⇒ 期望值**碰巧**相同 ⇒ fixture 空转。
    #    ② `tests/use_alpha.cpp` **不得**再引用 alpha.h（否则源① 独立判活 ⇒ 空转）。
    F="$WORK/fx_blind"; mk_fixture "$F"
    printf 'int main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    # ① 当前 target 的 .d：引用**无关**的头（只用来把 deps_n 抬到 > 0）
    printf '#pragma once\n' > "$F/include/cpu/pipeline/unrelated.h"
    { printf 'build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o: '
      printf '%s/tests/use_alpha.cpp %s/include/cpu/pipeline/unrelated.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o.d"
    # ② 源① 看不见的 TU（`build/` 内的生成源）+ 「名字抽不到」的 target 目录
    mkdir -p "$F/build/generated" "$F/build/tests/CMakeFiles/blindspot.dir"
    printf '#include "cpu/pipeline/alpha.h"\nint main(){return 0;}\n' > "$F/build/generated/gen.cpp"
    { printf 'build/tests/CMakeFiles/blindspot.dir/gen.cpp.o: '
      printf '%s/build/generated/gen.cpp %s/include/cpu/pipeline/alpha.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/blindspot.dir/gen.cpp.o.d"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**2 headers, 2 lines**|'
    st_check "target 名盲区下的 .d 证据 MUST 仍判活" 0 "$(run_guard "$F")"

    # ㉓ ~~仓库路径含正则元字符 ⇒ `.d` 前缀剥离 MUST 用字面比较~~ ——
    #    **该缺陷类别已被结构性消除**（第六轮）：源② 现在只比 **basename**，
    #    路径处理里**没有任何正则参与**（只有 `sed 's|.*/||'`）⇒ 没有「前缀剥离」
    #    这一步可坏。㉔ 仍守着「非本树构建的 `.d`」这条**真实**判据。

    # ㉔ **仓库被移动** ⇒ 存在**不是本树构建**的 `.d`（含旧绝对路径）
    #     ⇒ 那些证据不属于本树（无法归一化到当前根）
    #     ⇒ 不能被当作「无 includer」的证据
    #     ⇒ 源②不可信 ⇒ MUST NOT 判死
    #     ⚠️ 形状要求：必须**同时**有一个**本树构建**的 `.d`（把 deps_n 抬到 > 0）
    #     ⇒ 否则「源②不可信」会因 deps_n==0 而成立 ⇒ 期望值**碰巧**相同 ⇒ 空转。
    F="$WORK/fx_moved"; mk_fixture "$F"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/ghost.h"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**2 headers, 2 lines**|'
    mkdir -p "$F/build/tests/CMakeFiles/old.dir"
    { printf 'build/tests/CMakeFiles/old.dir/old.cpp.o: '
      printf '/nonexistent/oldroot/tests/old.cpp /nonexistent/oldroot/include/cpu/pipeline/alpha.h\n'
    } > "$F/build/tests/CMakeFiles/old.dir/old.cpp.o.d"
    st_check "仓库被移动（存在非本树构建的 .d）时 MUST NOT 判死" 0 "$(run_guard "$F")"

    # ㉕ **扫描面之外**的 C/C++ 扩展名 MUST fail-closed
    #     （.c/.inl/.hxx/… 里的 #include 源①看不见 ⇒ 安全属性的扫描面声明失真）
    F="$WORK/fx_unscanned_ext"; mk_fixture "$F"
    printf 'int main(void){return 0;}\n' > "$F/tests/legacy.c"
    st_check "扫描面之外的 C 扩展名 MUST fail-closed" 1 "$(run_guard "$F")"

    # ㉖ **docs/ 下的 `.md` 代码块 MUST NOT 算作 includer**（第五轮审稿 B3）
    #     （扫描面按 **TU 扩展名**筛选 ⇒ 文档里的文字/代码块不是 TU）
    #     形状：alpha.h 的**唯一**提及在 `.md` 代码块里；`.d` 只引用 unrelated.h
    #        ⇒ alpha.h MUST 被判死（exit=1）。若 `.md` 进了扫描面 ⇒ exit=0 ⇒ 判别成立。
    F="$WORK/fx_md_codeblock"; mk_fixture "$F"
    mkdir -p "$F/docs"
    printf '# 标题\n\n```cpp\n#include "cpu/pipeline/alpha.h"\n```\n' > "$F/docs/example.md"
    printf 'int main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/unrelated.h"
    { printf 'build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o: '
      printf '%s/tests/use_alpha.cpp %s/include/cpu/pipeline/unrelated.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o.d"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**2 headers, 2 lines**|'
    st_check "docs/*.md 里的代码块 MUST NOT 算作 includer" 1 "$(run_guard "$F")"

    # ㉗ **docs/ 下的 TU 引用 cpu 头 ⇒ 该头 MUST NOT 被判死**
    #     （第五轮审稿 B1：源① 的扫描面**含** docs/ ⇒ 它里面的 `.cpp` 片段也是
    #       真实的 #include 消费者 ⇒ 不扫它就等于把活头判死）
    #     ⚠️ 形状要求：① `tests/use_alpha.cpp` **不得**再引用 alpha.h
    #       （否则 alpha.h 会被默认 includer 判活 ⇒ fixture 空转）；
    #       ② `.d` MUST 引用**至少一个头**（否则 `deps_n==0` ⇒ 源②不可信
    #       ⇒ 期望值**碰巧**相同 ⇒ 空转）。
    F="$WORK/fx_docs_tu"; mk_fixture "$F"
    mkdir -p "$F/docs"
    printf '#include "alpha.h"\n' > "$F/docs/legacy.cpp"
    printf 'int main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/unrelated.h"
    { printf 'build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o: '
      printf '%s/tests/use_alpha.cpp %s/include/cpu/pipeline/unrelated.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o.d"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**2 headers, 2 lines**|'
    st_check "docs/ 下 TU 以 basename 形式引用 cpu 头 ⇒ MUST NOT 判死" 0 "$(run_guard "$F")"

    # ㉘ **路径形态无关**：`#include "include/cpu/pipeline/alpha.h"`（仓库根相对）
    #     —— 源① 只认 basename ⇒ 「按 `-I`/相对路径解析」这类判据不再存在
    #     ⚠️ 形状要求：`.d` **不得**引用 alpha.h（否则源②兜住 ⇒ fixture 空转）
    F="$WORK/fx_inc_dirs_gap"; mk_fixture "$F"
    printf '#include "include/cpu/pipeline/alpha.h"\n' > "$F/tests/use_alpha.cpp"
    printf '#pragma once\n' > "$F/include/cpu/pipeline/unrelated.h"
    { printf 'build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o: '
      printf '%s/tests/use_alpha.cpp %s/include/cpu/pipeline/unrelated.h\n' "$F" "$F"
    } > "$F/build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o.d"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|**2 headers, 2 lines**|'
    st_check "仓库根相对路径的 include MUST 仍判活（basename 口径）" 0 "$(run_guard "$F")"

    # ㉙ **`.d` 里没有任何 `.h` token** ⇒ 源②「不可信」而**不是脚本崩溃**
    #     ⚠️ 现实形状：只构建了一个「零 include 的 TU」（`.d` 只列自己的源文件）
    #        ⇒ `normalize_deps` 的 `grep -E '\.(h|hpp|…)$'` **无匹配** ⇒ 退出 1
    #        ⇒ 在 `pipefail` + `set -e` 下整个脚本**无消息中止（exit 1）** —— 那会被
    #        读成「发现违规」，而真实语义应是「源②不可信 ⇒ 保守模式 ⇒ 不判死」。
    #     ⇒ 期望 exit=0 **且** 输出含「源② 状态：不可信」。
    F="$WORK/fx_d_noheads"; mk_fixture "$F"
    printf '#include "cpu/pipeline/alpha.h"\nint main(){return 0;}\n' > "$F/tests/use_alpha.cpp"
    { printf 'build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o: '
      printf '%s/tests/use_alpha.cpp\n' "$F"
    } > "$F/build/tests/CMakeFiles/test_alpha.dir/use_alpha.cpp.o.d"
    st_check_out "「.d 无 .h token」MUST 降级为保守模式（不得无消息中止）" 0 \
        "源② 状态：不可信" "" "$F"

    # ㉑ C5 KEY FILES 表条目不含 .h（表被清空/改写）MUST fail-closed
    F="$WORK/fx_key_empty"; mk_fixture "$F"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|`pipeline/alpha.h`|（条目已删）|'
    st_check "KEY FILES 表清空 MUST fail-closed" 1 "$(run_guard "$F")"

    # ㉒ C5 自述计数被删 MUST fail-closed
    F="$WORK/fx_cnt_gone"; mk_fixture "$F"
    sed_inplace "$F/include/cpu/AGENTS.md" 's|\*\*1 headers, 1 lines\*\*|(计数已删)|'
    st_check "自述计数被删 MUST fail-closed" 1 "$(run_guard "$F")"

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
# ⚠️ 用 `while read` 而不是 `for x in $VAR`：后者做 word splitting ⇒ 路径含空格即破
while IFS= read -r exh; do
    [ -z "$exh" ] && continue
    b="${exh##*/}"
    libh="$(find "$LIB_DIR" -type f -name "$b" 2>/dev/null | head -1 || true)"
    [ -n "$libh" ] && [ -f "$libh" ] || continue
    if printf '%s\n' "$CH_DUP_ALLOWANCE" | awk -v b="$b" -F'|' '$1 == b { found = 1 } END { exit !found }'; then
        printf "  ::notice 同 basename 豁免生效：%s（owner=%s）\n" "$b" \
            "$(printf '%s\n' "$CH_DUP_ALLOWANCE" | awk -v b="$b" -F'|' '$1 == b { print $2; exit }')"
        continue
    fi
    violation "同 basename 双副本：$libh 与 $exh 并存 —— 消歧 MUST NOT 依赖 -I 顺序"
    dup_found=1
done <<EOF
$EX_HEADERS
EOF
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
    while IFS= read -r exh; do
        [ -z "$exh" ] && continue
        [ "${exh##*/}" = "$ab" ] || continue
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
# 5) C4 —— 硬死头判定（**两源都判无 includer 且源②可信** 才判死）
#
# ⚠️ **源① 的判据 = basename 原文提及**（**不做** include 形态识别）：
#    某文件里出现 `foo.h` 这个字符串 ⟺ 它**可能** #include 了该头 ——
#    不论形态（`"..."` / `<...>` / 宏 `#include HDR` / 续行 / 仓库根相对路径 /
#    `-I` 顺序差异 / 任何其它写法）。
#    ⇒ **不存在「解析失败」这一状态** ⇒ 上一版 ~420 行的手写 include 解析器
#      （路径归一化 + `INC_DIRS` + 未识别形态探测三套判据）被**整段删除**，
#      而不是继续打补丁（AGENTS.md 已记录同类教训：自研 C 子集静态分析器
#      「不可能被完全闭合」—— `scripts/verify_example_assertion_quality.sh`）。
#    ⇒ 代价是**只会漏报**（注释/文档里提到某个死头的名字 ⇒ 判活），
#      **绝不**把活头误报成死头 —— 这正是安全属性要求的方向。
# ⚠️ **自身提及 MUST 排除**：本仓每个头都在 `/** @file foo.h */` 里写自己的
#    文件名（实测 31 个头里 29 个如此）⇒ 若不按**文件归属**排除，则每个头都
#    恒「活」⇒ C4 完全失去判别力。排除判据 = 「**除该头自身之外**仍有文件提到
#    它」。⚠️ MUST 按文件归属判，**不能**用「整仓出现次数 > 自身出现次数」：
#    续行拼接会改变行数 ⇒ 两侧计数口径不同 ⇒ 会错判（活头被判死）。
# ⚠️ **续行拼接**按 C++ 语义**零插入**（删除 `\` 与换行，其余字符原样连接）：
#    插空格会把 `"a/be` + `ta.h"` 拼成 `"a/ be ta.h"` ⇒ 活头被判死。
# ---------------------------------------------------------------------------
echo ""
note "--- C4: 硬死头 MUST 删除（不得提升回来）---"

SRC_LIST="$WORK/src_list.txt"
SRC_LIST0="$WORK/src_list.nul"
SRC_MENTIONS="$WORK/src_mentions.txt"

# ⚠️ **源① 的扫描面** = 仓库自有目录下**全部 TU 扩展名**文件
#    （**含** `docs/` 与 `.rddf/`：它们里面的 `.cpp` 片段也是真实的 #include
#      消费者 ⇒ 扫到才不会把活头判死；
#      **排除** `third_party/`（外部依赖 submodule，其 `.c` 不是本仓 TU）、
#      构建输出目录、`.git/`、`node_modules/`）。
#    ⚠️ 扫描面**之外**的消费者（`third_party/`）**不计入**源① —— 本仓头若只被
#      third_party 消费，对它而言等同零消费者（**有意**：本仓用不到的即死）。
#
# ⚠️⚠️ **prune 列表 MUST 是显式白名单，不能用 `build*` 这类 glob**（第六轮审稿
#    BLOCKER 级缺陷）：`build*` 会连带 prune 掉 `buildtools/`、`builder/`、
#    `.rddf/state/builder` 这类**源码/工具目录** ⇒ 那里面的 C/C++ 文件既不被源①
#    扫到、也不被下面的「扫描面自检」探针报出来 ⇒ 一个活头若**唯一** includer 在
#    `buildtools/` 里，源① 说无 + 源② 说无 ⇒ **误判成硬死头 ⇒ 安全属性失效**。
#    ⇒ 只 prune **构建输出目录**的显式名字。
# ⚠️⚠️ **prune 列表 MUST 是「构建输出目录」且用**内容**判定**（第六/七轮审稿）：
#    ① 不能用 `build*` 这类**前缀 glob**（它会连带 prune 掉 `buildtools/`、
#       `builder/`、以及本仓真实存在的 `openspec/specs/build-cpp-standard/`
#       这类**非构建目录** ⇒ 那里面的 C/C++ 文件既不被源① 扫到、也不被下面
#       的「扫描面自检」探针报出来 ⇒ 一个活头若**唯一** includer 在那里，
#       源① 说无 + 源② 说无 ⇒ **误判成硬死头 ⇒ 安全属性失效**）；
#    ② 即使名字匹配，也 MUST 额外确认它是**构建输出目录**
#       （判据：含 `CMakeCache.txt`）—— 实测 `build/` / `build-jit/` /
#       `build-test/` 都有，而 `openspec/specs/build-cpp-standard/` 没有。
#    ③ 探针与扫描**共用**同一份 prune 列表 ⇒ 被 prune 的目录里必然有
#       「源① 看不见」的 C/C++ 文件 ⇒ 该目录 MUST 是**生成物目录**
#       （其内容是构建产物，不是仓库自有的 TU）⇒ 探针不报它们是合理的。
PRUNE_NAMES=(.git third_party node_modules)
BUILD_DIR_NAMES=(build 'build-*' 'cmake-build-*')
# 扫描面扩展名（白名单）与其**超集**（探针用）
SCAN_EXT=(.h .hpp .hh .cpp .cc .cxx .C .tpp .ipp)
# ⚠️ 探针的扩展名 MUST 是「像 C/C++ 的全部已知扩展名」的超集 ⇒ 仓库里出现
#    任何**不在扫描面**内的 C/C++ 扩展名 ⇒ MUST 报违规（否则「源① 看得见所有
#    消费者」这句声明会静默失真）。上一版只列了 7 个扩展名 ⇒ `.h++` / `.cu` /
#    `.tcc` / `.mm` 这类落在扫描面外却**不报违规** ⇒ 同一类洞。
PROBE_EXT=("${SCAN_EXT[@]}" .c .inl .hxx .ixx .c++ .cp .CPP .h++ .cu .cuh .tcc .mm .metal)
# ⚠️ 用 **bash 数组**而不是命令替换生成 find 的谓词：命令替换的结果会丢掉引号
#    （`-name "*.h"` 会变成字面量 `-name "*.h"` ⇒ 匹配不到任何文件）且会再经
#    **glob 展开**（`-name *.h` 会被当前目录的文件名替换掉）。数组逐元素传参 ⇒
#    两者都不会发生。
# ⚠️ 谓词之间 MUST 用 **`-o` 连接**（find 里多个 `-name` 默认是 **AND** 关系
#    ⇒ 一个文件不可能同时以 `.h` 和 `.hpp` 结尾 ⇒ 匹配结果恒为空）。
prune_args=(); pfirst=1
for d in "${PRUNE_NAMES[@]}"; do
    if [ "$pfirst" = 1 ]; then prune_args+=(-name "$d"); pfirst=0
    else prune_args+=(-o -name "$d"); fi
done
# 构建输出目录 = **含 `CMakeCache.txt` 的目录**（**纯内容判定**，与名字无关）
# ⚠️ 不能只按**名字** prune（第八轮审稿 SHOULD_FIX）：`cmake -B out` /
#    `cmake -B verilator-build` 都是合理用法 ⇒ 名字锚定 ⇒ 那些目录里的
#    生成物（`.c` 等）会被探针报成「扫描面之外」⇒ **CI 被无关的构建布局阻断**。
# ⇒ 两阶段：① 常见构建目录名（浅层）先剪枝（避免全仓库深搜）；
#          ② 其余目录里任何含 `CMakeCache.txt` 的也剪枝。
build_paths=()
while IFS= read -r bd; do
    [ -z "$bd" ] && continue
    [ -f "$bd/CMakeCache.txt" ] || continue
    build_paths+=("$bd")
done < <(find . -maxdepth 3 -type d \( -name build -o -name 'build-*' \
          -o -name 'cmake-build-*' \) -print 2>/dev/null | sort)
# 阶段 2：已知的构建目录不再下钻（省时间），其余目录按**内容**找
stage2_prune=()
for bp in "${build_paths[@]}"; do
    stage2_prune+=(-o -path "$bp" -o -path "$bp/*")
done
while IFS= read -r cf; do
    [ -z "$cf" ] && continue
    bd="${cf%/CMakeCache.txt}"
    build_paths+=("$bd")
done < <(find . -type d \( "${prune_args[@]}" "${stage2_prune[@]}" \) \
         -prune -o -name CMakeCache.txt -print 2>/dev/null | sort)
for bp in "${build_paths[@]}"; do
    prune_args+=(-o -path "$bp")
done
scan_args=(); sfirst=1
for e in "${SCAN_EXT[@]}"; do
    if [ "$sfirst" = 1 ]; then scan_args+=(-name "*$e"); sfirst=0
    else scan_args+=(-o -name "*$e"); fi
done
probe_args=(); pbfirst=1
for e in "${PROBE_EXT[@]}"; do
    if [ "$pbfirst" = 1 ]; then probe_args+=(-name "*$e"); pbfirst=0
    else probe_args+=(-o -name "*$e"); fi
done

find . -type d \( "${prune_args[@]}" \) -prune -o \
    -type f \( "${scan_args[@]}" \) -print 2>/dev/null \
    | sed 's|^\./||' | sort > "$SRC_LIST"

# ⚠️ **扫描面自检（fail-closed）**：仓库里若存在**扩展名不在扫描面**的
#    C/C++ 源文件（.c / .inl / .hxx / .ixx / .c++ …）⇒ 它们的 `#include` 源①
#    看不见 ⇒ 「RV32I 头的消费者全部在扫描面内」这句声明会**静默失真**
#    ⇒ MUST 报违规（而不是在注释里默默承认）。
#    （`third_party/` 被排除：它有 12 个 `.c`，那是 verilator 自己的代码；
#      构建输出目录同理 —— 那里是**生成物**，不是仓库自有的 TU。）
# ⚠️ 末尾的 `|| true` 是必需的：`head -5` 提前退出会让 `sort` 收到 SIGPIPE
#    ⇒ 在 `pipefail` 下整个管道非零 ⇒ 命令替换失败 ⇒ `set -e` 让脚本**无消息
#    退出 141**（看起来像崩溃，而不是「发现违规」）。
UNSCANNED_EXT="$(find . -type d \( "${prune_args[@]}" \) -prune -o \
    -type f \( "${probe_args[@]}" \) -print 2>/dev/null \
    | sed 's|^\./||' \
    | while IFS= read -r f; do
        # 只报「扩展名在探针超集里、但**不在**扫描面白名单里」的文件
        e="${f##*.}"
        ok=0
        for s in "${SCAN_EXT[@]}"; do
            [ "$e" = "${s#.}" ] && ok=1 && break
        done
        [ "$ok" = 0 ] && printf '%s\n' "$f"
      done \
    | sort | head -5 || true)"
if [ -n "$UNSCANNED_EXT" ]; then
    violation "仓库存在**扫描面之外**的 C/C++ 源文件（扩展名不在 {.h .hpp .hh .cpp .cc .cxx .C .tpp .ipp}）—— 源①看不见它们的 #include，安全属性的扫描面声明会失真：$UNSCANNED_EXT"
fi

src_n="$(grep -c . "$SRC_LIST" 2>/dev/null || true)"
if [ -z "$src_n" ]; then src_n=0; fi
if [ "$src_n" -eq 0 ]; then
    violation "源① 的扫描面为空（一个 TU 都没找到）—— 分析器失效 MUST 视为违规"
fi

# 候选 basename 表 + 「头 → 自身 basename」表（用于排除自身 `@file` 提及）
BN_ALL="$WORK/basenames.txt"
HDR_ALL="$WORK/headers.txt"
: > "$BN_ALL"; : > "$HDR_ALL"
while IFS= read -r h; do
    [ -z "$h" ] && continue
    printf '%s\n' "$h" >> "$HDR_ALL"
    printf '%s\n' "${h##*/}" >> "$BN_ALL"
done <<EOF
$(printf '%s\n%s\n' "$LIB_HEADERS" "$EX_HEADERS")
EOF
sort -u "$BN_ALL" > "$WORK/basenames.sorted" 2>/dev/null || true
mv "$WORK/basenames.sorted" "$BN_ALL"

# 5a) 源①：**一次遍历**逐个文件判定「除自身之外是否被提及」
#     ⚠️ 逐个**文件**喂 awk（不经 `cat` 拼接）⇒ 文件归属天然精确 ⇒ 自身提及
#        的排除不依赖任何「路径重现」技巧。
#     ⚠️ 用 NUL 分隔的列表 + `xargs -0`（**不用 `xargs -a`**：BSD xargs 没有 `-a`）。
tr '\n' '\0' < "$SRC_LIST" > "$SRC_LIST0"
: > "$SRC_MENTIONS"
src_seen="$(
    # ⚠️ 三个路径用**环境变量**（ENVIRON）传，而不是 `-v`：
    #    ① `-v` 的实参**是否计入 ARGC** 是各实现的行为差异（mawk 1.3.4 实测不计，
    #       但 gawk / BSD awk 未在本机验证）⇒ 计数判据 `ARGC - 3` 会依赖它；
    #    ② 环境变量是 POSIX awk 明确保证的（ENVIRON）⇒ 无此依赖。
    #    BN_ALL / HDR_ALL 仍作为**位置参数**传入（ARGV[1] / ARGV[2]）⇒ awk 内
    #    用 ARGV 位置识别它们，不再依赖路径比较。
    BN_ALL="$BN_ALL" HDR_ALL="$HDR_ALL" SRC_MENTIONS="$SRC_MENTIONS" \
    xargs -0 awk '
        FILENAME == ARGV[1] { if ($0 != "") want[$0] = 1; next }
        FILENAME == ARGV[2] { b = $0; sub(/.*\//, "", b); self[$0] = b; next }
        {
            # ⚠️ 续行状态 MUST **按文件重置**（否则文件末尾的悬空 `\` 会吞掉
            #    下一个文件的首行 ⇒ 内容整体错位 ⇒ 可能漏看 includer ⇒ 误杀活头）。
            # ⚠️ 但上一个文件末尾的悬空续行**其内容仍是有效字符** ⇒ MUST 先参与
            #    判定再重置（否则「只出现在文件末尾悬空续行里」的 includer 会漏看）。
            if (FNR == 1) {
                if (pend != "" && last != "")
                    for (i in want)
                        if (self[last] != i && index(pend, i) > 0) found[i] = 1
                pend = ""
                last = FILENAME
            }
            sub(/\r$/, "", $0)
            line = (pend == "") ? $0 : pend $0
            pend = ""
            if (line ~ /\\[[:space:]]*$/) {
                sub(/\\[[:space:]]*$/, "", line)
                pend = line
                next
            }
            for (i in want) {
                if (self[FILENAME] == i) continue
                if (index(line, i) > 0) found[i] = 1
            }
        }
        END {
            # 文件末尾的悬空续行：其内容仍是有效字符 ⇒ MUST 参与判定
            if (pend != "")
                for (i in want)
                    if (self[last] != i && index(pend, i) > 0) found[i] = 1
            for (i in found) print i >> ENVIRON["SRC_MENTIONS"]
            # 报告本次调用实际馈入的 TU 个数。
            # ⚠️ `ARGC` = 1（awk 自身）+ 2（BN_ALL / HDR_ALL 两个位置参数）+ N（TU）
            #    ⇒ N = ARGC - 3。**不依赖 `-v` 实参是否计入 ARGC**（那已改成
            #    ENVIRON 传递 ⇒ 该实现差异不再影响本计数）。
            printf "%d\n", ARGC - 3
        }
    ' "$BN_ALL" "$HDR_ALL" < "$SRC_LIST0" 2>/dev/null \
    | awk '{ s += $1 } END { printf "%d\n", s + 0 }' || true
)"
if [ -z "$src_seen" ]; then src_seen=0; fi
# ⚠️ **分析器健康检查（fail-closed）**：少读一个文件就可能漏看一个 includer
#    ⇒ 活头被判死 ⇒ MUST 报违规。崩溃（awk 语法错 / 早期退出）也走这条
#    （此时 `src_seen == 0`）。
if [ "$src_seen" -ne "$src_n" ]; then
    violation "源① 只读了 $src_seen / $src_n 个 TU —— 分析器未完整运行 MUST 视为违规（否则会把活头误判为死头）"
fi
sort -u "$SRC_MENTIONS" > "$SRC_MENTIONS.sorted" 2>/dev/null || true
mv "$SRC_MENTIONS.sorted" "$SRC_MENTIONS"
mention_n="$(grep -c . "$SRC_MENTIONS" 2>/dev/null || true)"
if [ -z "$mention_n" ]; then mention_n=0; fi

# 5b) 证据源②：编译器的 `.d`
# ⚠️ 与源① **同一口径**：只比 **basename**。⇒ 上一版的「仓库前缀剥离 /
#    `.d` 路径归一化 / 系统头剥不掉前缀」整类问题**结构性消失**
#    （路径里已无正则参与 ⇒ 也就不存在「把 root 当正则」的缺陷）。
# ⚠️ `.d` **全部参与判活**，**不按 target 名过滤**：target 名抽取有盲区
#    （`add_executable(${tgt} ...)` 抽不到真名）⇒ 过滤会丢弃**真实的** `.d`
#    ⇒ 活头被判死。「非当前 target」的命中只给 warning。
DEPS="$WORK/deps.txt"
DEPS_CUR="$WORK/deps_cur.txt"
: > "$DEPS"; : > "$DEPS_CUR"
deps_n=0
deps_cur_n=0
deps_stale_files=0

normalize_deps() {
    # `.d` 是 make 依赖文件：空白分隔的 token 流（续行 `\` 会单独成为一个 token，
    # 随后被扩展名过滤掉）
    # ⚠️ `|| true` 必需：`grep` 无匹配时退出 1 ⇒ 在 `pipefail` + `set -e` 下
    #    会让整个脚本**无消息中止**。而「无匹配」的语义应是 deps 为空 ⇒
    #    `deps_n == 0` ⇒ 源②不可信 ⇒ 保守模式（不判死），不是崩溃。
    tr ' \t' '\n\n' < "$1" | sed 's|.*/||' \
        | grep -E '^[^/]*[.](h|hpp|hh|tpp|ipp)$' | sort -u || true
}

if [ -d build ]; then
    # ⚠️ target 名抽取（仅用于「非当前 target」的 warning）：**prune build/** ——
    #    否则会把生成目录里的 CMakeLists.txt 也算进来（噪声）。
    CURRENT_TARGETS="$WORK/targets.txt"
    list_repo_cmakelists() {
        find . \( -path './build*/*' -o -path './third_party/*' -o -path './.git/*' \
                  -o -path './.rddf/*' -o -path './node_modules/*' \
                  -o -path './docs/*' \) -prune -o \
            -name CMakeLists.txt -print 2>/dev/null | sort
    }
    # ⚠️ `xargs` 的退出码 **123** 表示「某个 grep 调用以 1-125 退出」，而
    #    **grep 无匹配时本身就退出 1** ⇒ 123 在**正常**场景就会出现 ⇒ 无法用它
    #    区分「无输入」与「失败」。⇒ 空输入直接返回；非空输入时忽略 123。
    #    ⚠️ 本函数只服务 ④ 的 warning ⇒ 是**尽力而为**的提示，不承担安全职责。
    extract_targets() {
        local pat="$1" sedexpr="$2"
        local files
        files="$(list_repo_cmakelists)"
        if [ -z "$files" ]; then
            return 0
        fi
        printf '%s\n' "$files" | xargs grep -hoE "$pat" 2>/dev/null \
        | sed -E "$sedexpr" || true
    }
    {
        extract_targets 'add_[a-z_]+\( *[$]?[{]?[A-Za-z_][A-Za-z0-9_]*' 's/.*\( *//; s/^[$][{]?//'
        extract_targets 'riscv_mini_(catch_test(_noctest)?|unit_test)\( *[A-Za-z_][A-Za-z0-9_]*' 's/.*\( *//'
    } | sort -u > "$CURRENT_TARGETS"

    : > "$WORK/all.d"
    : > "$WORK/cur.d"
    : > "$WORK/deps_files.txt"
    while IFS= read -r dfile; do
        [ -z "$dfile" ] && continue
        # ⚠️ **stale `.d` 整文件跳过**（仓库被移动 ⇒ `.d` 记的是**旧**绝对路径）：
        #    一个 `.d` **必然**引用它自己的源文件 ⇒ 若它**不含**当前仓库根 ⇒
        #    它就不是本树构建的 ⇒ 证据不可信。⇒ 整文件跳过 + 计数
        #    （`deps_stale_files > 0` ⇒ 源②不可信 ⇒ 不判死）。
        if ! grep -qF -- "$REPO_ROOT/" "$dfile" 2>/dev/null; then
            deps_stale_files=$((deps_stale_files+1))
            continue
        fi
        cat "$dfile" >> "$WORK/all.d" 2>/dev/null || true
        printf '%s\n' "$dfile" >> "$WORK/deps_files.txt"
        tname="$(printf '%s' "$dfile" | sed -E 's|.*/CMakeFiles/([^/]+)\.dir/.*|\1|')"
        if grep -Fx -- "$tname" "$CURRENT_TARGETS" >/dev/null 2>&1; then
            cat "$dfile" >> "$WORK/cur.d" 2>/dev/null || true
        fi
    done < <(find build -name '*.o.d' -type f 2>/dev/null | sort || true)
    # ⚠️ 证据搜索根 = **`build/` 名下**（第十轮审稿 NIT-3）：`build-jit/` /
    #    `build-test/` 的 `.d` 不进语料 ⇒ 证据**更少** ⇒ 方向**保守**
    #    （不会把活头判死 ⇒ 安全；代价是可能漏掉「当前消费者」⇒ 只警告）。

    if [ -s "$WORK/all.d" ]; then
        normalize_deps "$WORK/all.d" > "$DEPS"
        deps_n="$(grep -c . "$DEPS" 2>/dev/null || true)"
        if [ -z "$deps_n" ]; then deps_n=0; fi
    fi
    if [ -s "$WORK/cur.d" ]; then
        normalize_deps "$WORK/cur.d" > "$DEPS_CUR"
        deps_cur_n="$(grep -c . "$DEPS_CUR" 2>/dev/null || true)"
        if [ -z "$deps_cur_n" ]; then deps_cur_n=0; fi
    fi
fi

has_source_includer() {
    local bn
    bn="${1##*/}"
    [ -n "$bn" ] || return 1
    [ -s "$SRC_MENTIONS" ] || return 1
    if grep -qxF -- "$bn" "$SRC_MENTIONS"; then return 0; fi
    return 1
}

has_dep_includer() {
    [ "$deps_n" -gt 0 ] || return 1
    if grep -qxF -- "${1##*/}" "$DEPS"; then return 0; fi
    return 1
}

# ⚠️ 辅助：秒级 mtime（跨 GNU / BSD `stat`）
file_mtime() {
    local p="$1" v
    [ -f "$p" ] || { printf ''; return 0; }
    v="$(stat -c %Y "$p" 2>/dev/null || true)"
    if [ -z "$v" ]; then
        v="$(stat -f %m "$p" 2>/dev/null || true)"
    fi
    printf '%s' "$v"
}

# ⚠️ 辅助：提取 `.d` 里的**源文件**路径
#    ⚠️⚠️ 真实 CMake/GCC 的 `.d` 是**多行续行**格式：
#        target.o: \
#         /abs/src/a.cpp \
#         /usr/include/x.h \
#      ⇒ 源文件路径在**续行**里 ⇒ 提取 MUST 处理续行。
#      （`sed -n 's/^[^:]*: //p'` 只打印**第一行** ⇒ 真实 `.d` 上恒为空 ⇒
#        「当前消费者」检测在生产环境恒失效 —— 第八轮审稿 BLOCKER-1）
dep_source_files() {
    # ⚠️ 切分必须同时切 **空格 / TAB / 反斜杠**（续行符）⇒ 口径与
    #    `normalize_deps` 一致（第九轮审稿 NIT-1）。
    # ⚠️ 只去**行尾**的冒号（target token 的冒号在行尾）⇒ 不用 `tr -d ':'`
    #    （那会破坏文件名里合法的冒号 —— 第九轮审稿 NIT-5）。
    tr ' \t\\' '\n\n\n' < "$1" 2>/dev/null \
        | sed 's/:$//' \
        | grep -E '\.(cpp|cc|cxx|c|C)$' || true
}

# ⚠️ 辅助：把字面量转成**正则**（basename 含 `.` 等元字符）
#    ⚠️ 不可用 bash 的 `${bn//./\\.}`：glob 模式里 `.` 匹配**任意字符**
#       ⇒ 会把**每个**字符都替换成 `\.` ⇒ 正则永不匹配（第八轮审稿实测：
#       fixture ⑨c / ⑩g 因此转红）。⇒ 用 sed 逐字符转义。
regex_escape() {
    printf '%s' "$1" | sed 's/[][\\.^$*+?(){}|]/\\&/g'
}

# ⚠️ 辅助：`.d` 是否**精确**提到某个 basename（token 级，非子串）
#    （`alpha.h` 不得被 `my_alpha.h` 命中）
dep_mentions_basename() { # <文件> <basename>
    local f="$1" bn="${2##*/}" esc
    esc="$(regex_escape "$bn")"
    grep -qE "(^|[/[:space:]\\])${esc}([[:space:]\\]|$)" "$f" 2>/dev/null
}

# ⚠️ 辅助：TU 里是否存在一个 `#include` 指令、其**操作数**（引号 /
#    尖括号内的文本）**就是**该 basename（或以 `<目录>/` + basename 结尾）
#    —— 不是「整行匹配」（第十轮审稿 SF-1：行尾注释形态
#    `#include "other.h" // rv32i_pipeline_top.h` 的提及不得算 include 证据）。
tu_includes_basename() { # <TU 路径> <basename>
    awk -v bn="$2" '
        /^[[:space:]]*#[[:space:]]*include/ {
            line = $0
            sub(/^[[:space:]]*#[[:space:]]*include[[:space:]]*/, "", line)
            if (line !~ /^[\"<]/) next          # 操作数必须以引号 / 尖括号开始
            operand = line
            sub(/^[\"<]/, "", operand)
            sub(/[\">].*$/, "", operand)         # 操作数 = 引号 / 尖括号内文本
            if (operand == bn) { find = 1; exit }
            # 或操作数以 `<目录>/bn` 结尾（basename 精确比对，词边界）
            total = length(operand); blen = length(bn)
            if (total > blen &&
                substr(operand, total - blen) == "/" bn) { find = 1; exit }
        }
        END { exit !find }
    ' "$1" 2>/dev/null
}

# ⚠️⚠️ **「当前」的真 include 证据**（第七轮审稿 BLOCKER-1）
#    `.d` 是**累积**构建产物：TU 被删 / target 被移除后，`.d` 不会被清掉
#    ⇒「`.d` 里出现过某头」**不等于**「现在有消费者」。
#    ⇒ 判「豁免 stale」这类**破坏性**结论时，MUST 要求证据是当前的：
#      (a) `.d` 的**源文件仍存在于磁盘**（TU 已删 ⇒ 该 `.d` 是陈旧的）；
#      (b) 该源文件**仍文本提及**该 basename（它真的 include 了它）；
#      (c) 该 `.d` **比该源文件新**（`.d` 是源文件最后一次修改之后生成的）。
#    任一不满足 ⇒ 没有当前证据 ⇒ MUST **降级为警告**（与源① 一致的安全方向）。
has_current_dep_includer() {
    local h="$1" bn="${1##*/}" dfile src abs d_mt s_mt
    [ "$deps_n" -gt 0 ] || return 1
    # ⚠️ 源② 不可信时（存在**非本树构建**的 `.d`）⇒ 不给「当前」证据
    #    （与 C4 主判定同一判据 ⇒ 两条路径语义一致）
    [ "$deps_stale_files" -eq 0 ] || return 1
    [ -n "$bn" ] || return 1
    [ -s "$WORK/deps_files.txt" ] || return 1
    while IFS= read -r dfile; do
        [ -z "$dfile" ] && continue
        dep_mentions_basename "$dfile" "$bn" || continue
        d_mt="$(file_mtime "$dfile")"
        [ -n "$d_mt" ] || continue
        while IFS= read -r src; do
            [ -z "$src" ] && continue
            # ⚠️ **只接受绝对路径**（第九轮审稿 NIT-2）：CMake 生成的 `.d` 里
            #    源文件是**绝对路径**（本仓实测 447 个不同 token 全部如此）⇒
            #    相对路径 token 若按「相对仓库根 / 相对 build 目录」去猜，
            #    **可能取到另一个文件**（`tests/x.cpp` 既是仓库根下的也是
            #    build 下的）⇒ 误判风险 ⇒ 一律跳过（漏判方向安全）。
            case "$src" in
                /*) abs="$src" ;;
                *)  continue ;;
            esac
            [ -f "$abs" ] || continue            # (a) 源文件已删 ⇒ 陈旧 .d
            # (b) 该源文件**仍 `#include` 它** ⇒ 仍被 include。
            #     ⚠️ MUST 收窄到 **`#include` 指令的「操作数」**（第十轮审稿 SF-1）：
            #     只查「文本提及」⇒ 注释里的提及被当成证据（第八轮洞，重写在先）；
            #     只查「行首是 `#include`」的**整行** ⇒ 行尾注释形态
            #     （`#include "other.h" // rv32i_pipeline_top.h`）里的提及
            #     **仍**会产生破坏性假违规 ⇒ 比对对象 MUST 是操作数
            #     （引号 / 尖括号内的文本），并取**最后一段** == basename。
            #     ⚠️ 只认**字面 include** ⇒ 宏形式与续行形式**漏判**
            #     ⇒ 漏判方向是「无当前证据 ⇒ 只警告」⇒ **安全**
            #     （不会产生破坏性结论）。
            #     ⚠️⚠️ **已知误报向量（第十一轮审稿 SF-C，如实记录）**：
            #        `#if 0` / `#ifdef` **非激活区**里的字面 `#include`
            #        **仍被计入证据**（收窄到操作数**没有**消掉它 —— 预处理器
            #        语义不在本判据内）⇒ 该形态下会输出**破坏性**结论
            #        （fixture ⑨h 钉住这一实际行为 ⇒ 行为变化可见）。
            #        可达性受限：需 `.d` 提及该头 + `.d` 不早于 TU +
            #        TU 只在非激活区提及它三者同时成立 ⇒ 当前仓库不可达。
            #        ⚠️ **不得**在契约/文档里声称它是「漏判 ⇒ 安全」。
            #        真修（跳过非激活区）需要完整预处理器语义（嵌套 `#if` /
            #        `#else`）⇒ 若将来真修，MUST 补 `#else` 与嵌套分支的 fixture。
            tu_includes_basename "$abs" "$bn" || continue
            # (c) `.d` **不早于**源文件 ⇒ 该 `.d` 记载的依赖集不落后于源文件。
            #     ⚠️ 用**显式时间差**而不是裸 `-nt`：macOS 自带 bash 3.2 的 `-nt`
            #     只比秒级 st_mtime，而本机 fs 的 tick 粒度可能让两个文件落在
            #     同一秒 ⇒ `-nt` 假阴性 ⇒ 真实的当前证据被当成陈旧（假警告）。
            #     ⚠️ 阈值是 **0**（不是 1 秒）：`.d` 由**预处理阶段**写出，与源
            #     文件 mtime 的间隔常常 < 1s（实测 `g++ -MD` 产出同秒的 `.d`）
            #     ⇒ 用 1 秒会**必然**漏判（假阴性 ⇒ 该报的 stale 不报）。
            #     ⚠️ (c) 是**弱**判据：(a)+(b) 已覆盖主要风险（源文件被删 /
            #     不再 include 它）；(c) 只额外排除「`.d` 比源文件旧 ⇒ 依赖集过时」。
            s_mt="$(file_mtime "$abs")"
            [ -n "$s_mt" ] || continue
            [ $((d_mt - s_mt)) -ge 0 ] || continue
            return 0
        done < <(dep_source_files "$dfile")
    done < "$WORK/deps_files.txt"
    return 1
}

has_dep_includer_current() {
    [ "$deps_cur_n" -gt 0 ] || return 1
    if grep -qxF -- "${1##*/}" "$DEPS_CUR"; then return 0; fi
    return 1
}

allow_list="$CH_DEAD_ALLOWANCE"
allow_count="$(printf '%s\n' "$allow_list" | grep -c . || true)"
if [ "$allow_count" -ne "$EXPECTED_DEAD_ALLOWANCES" ]; then
    violation "零消费者头豁免条数为 $allow_count，期望 $EXPECTED_DEAD_ALLOWANCES —— 豁免项 MUST 显式增删（防止静默增加）"
fi
# ⚠️ **格式校验**（人工编辑极易破坏：路径 + TAB + 理由 + TAB + 失效条件）
#    破坏后的典型后果 = 字段被合并 ⇒ 理由/失效条件丢失 ⇒ 豁免变成「无期限」，
#    而护栏看不出来（它只取第 1 个字段）⇒ 必须 fail-closed。
while IFS= read -r aline; do
    [ -z "$aline" ] && continue
    nfields="$(printf '%s' "$aline" | awk -F'\t' '{print NF}')"
    if [ "$nfields" -ne 3 ]; then
        violation "零消费者头豁免条目格式非法：期望 3 个 TAB 分隔字段（路径<TAB>理由<TAB>失效条件），实际 $nfields 个 ⇒ $aline"
    fi
done <<EOF
$allow_list
EOF

dead_found=0
while IFS= read -r h; do
    [ -z "$h" ] && continue
    if printf '%s\n' "$allow_list" | awk -v h="$h" '$1 == h { found = 1 } END { exit !found }'; then
        printf "  ::notice 零消费者头豁免生效：%s\n" "$h"
        continue
    fi

    src_has=0
    if has_source_includer "$h"; then src_has=1; fi
    dep_has=0
    if has_dep_includer "$h"; then dep_has=1; fi

    # ⚠️ **安全属性**：只有**两个源都**说「无 includer」才判死。
    #    任一源说有 ⇒ 活的 ⇒ 不判死。
    if [ "$src_has" -eq 1 ] || [ "$dep_has" -eq 1 ]; then
        if [ "$src_has" -eq 1 ] && [ "$deps_n" -gt 0 ] && ! has_dep_includer_current "$h"; then
            # 只警告、不违规（可能是传递性死，也可能是 target 名抽取盲区）
            warning "零消费者头（源码有 includer 但无**当前** target 的 .d 覆盖它，可能是传递性死或 target 名抽取盲区）：$h —— 请人工确认后再删"
        fi
        continue
    fi
    # 两个源都无 ⇒ 候选硬死头。⚠️ 但源② MUST 可信才判死。
    if [ "$deps_n" -eq 0 ] || [ "$deps_stale_files" -gt 0 ]; then
        warning "候选零消费者头：$h —— 但源②不可信（.d 证据 $deps_n 条，非本树构建的 .d $deps_stale_files 个）⇒ MUST NOT 判死（保守），请人工确认"
        continue
    fi
    dead_found=1
    violation "硬死头：$h —— MUST 删除，不得以「提升回 include/cpu/」保留"
done <<EOF
$(printf '%s\n%s\n' "$LIB_HEADERS" "$EX_HEADERS")
EOF

# ⚠️ **证据源状态始终打印**（透明度）：判死与否都给出两个源各自的状态与依据
#    ⇒ (a) 人能一眼看出「这次判定是不是在保守模式」；
#    (b) self-test 可以对它做**输出级**断言（exit code 相同但成因不同的情形）。
note "  源① 状态：就绪（扫描面 $src_n 个 TU；basename 原文搜索命中 $mention_n 个 basename；不做 include 形态识别 ⇒ 不存在「解析失败」状态）"
if [ "$deps_n" -gt 0 ] && [ "$deps_stale_files" -eq 0 ]; then
    note "  源② 状态：可信（${deps_n} 条 .d basename 记录，0 个非本树构建的 .d）"
else
    note "  源② 状态：不可信（.d 证据 $deps_n 条，非本树构建的 .d $deps_stale_files 个 ⇒ 判死被禁用）"
fi

if [ "$dead_found" -eq 0 ]; then
    if [ "$deps_n" -gt 0 ] && [ "$deps_stale_files" -eq 0 ]; then
        note "  OK：无硬死头（源① 就绪 + 源② 可信）"
    else
        note "  注意：无硬死头**违规**，但源②未可信 ⇒ 判定为保守模式（只警告）"
    fi
fi

# stale 豁免检查：MUST 用**真 include 证据**判定「已有消费者」
# ⚠️⚠️ **方向 MUST 正确**（第六轮审稿 BLOCKER-2）：源① 的语义是
#    「**任何原文提及** ⇒ 可能 #include 过」—— 这在「判死」方向是**保守**的
#    （漏报，安全）；但若用**同一个谓词**去证明「**已有真实消费者**」，方向
#    完全相反 ⇒ 一个**纯注释**里的提及就会让豁免被判 stale ⇒ 假违规，且错误
#    信息会指示「MUST 移除该行」⇒ 移除后护栏报「硬死头 MUST 删除」⇒ agent
#    删掉 ADR-046 D10 **冻结**的 C3 DUT（`rv32i_pipeline_top.h`）。
#    ⇒ 违规只认**编译器 .d 证据**（真 include）；源① 的提及**只警告**，
#      且警告措辞 MUST NOT 指示删除豁免行。
while IFS= read -r a; do
    [ -z "$a" ] && continue
    ah="$(printf '%s' "$a" | awk '{print $1}')"
    [ -z "$ah" ] && continue
    if [ ! -f "$ah" ]; then
        violation "零消费者头豁免里的 $ah 已不存在 ⇒ MUST 移除该行"
        continue
    fi
    if has_current_dep_includer "$ah"; then
        violation "零消费者头豁免里的 $ah 已有**当前**消费者（编译器 .d 证据：源文件仍存在、仍 include 它、且 .d 更新）⇒ 豁免 stale，MUST 移除该行"
    elif has_dep_includer "$ah"; then
        warning "零消费者头豁免里的 $ah 在 .d 里出现过，但**无当前证据**（.d 是累积产物：引用的 TU 可能已被删除或改动）⇒ 豁免是否 stale 待人工确认（MUST NOT 依据本提示删除豁免行）"
    elif has_source_includer "$ah"; then
        warning "零消费者头豁免里的 $ah 在**源码里被提及**（无 .d 证据 ⇒ 可能是注释/文档里的提及而非真实 include）⇒ 豁免是否 stale 待人工确认（MUST NOT 依据本提示删除豁免行）"
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
    # ⚠️ 文档里有多处**简写形式**：`stages/{if,id,ex,mem,wb}_stage.h`（花括号展开）
    #    与 `i_cache_sync.h / d_cache_sync.h`（斜杠并列）⇒ MUST 先**展开花括号**，
    #    再只取 **basename** 与磁盘比对。
    # ⚠️ 用 `[{]` / `[}]` 而非 `\{` / `\}` —— POSIX ERE 里 `\{` 是 **interval
    #    表达式的开头**（未定义行为）⇒ 各实现分叉（mawk 当字面 `{`，BSD awk 可能
    #    报错）⇒ 方括号表达式是唯一定义明确的形式。
    struct_names="$(printf '%s\n' "$struct_block" | awk '
        {
            line = $0
            while (match(line, /[{][^{}]*[}]/)) {
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
        # ⚠️ **fail-closed**：KEY FILES 表是 ADR-046 D9 的契约面。解析不出条目
        #    意味着「表被删/格式变了/解析器坏了」—— 三种都不该静默放行。
        violation "$CPU_AGENTS 的 KEY FILES 表里未解析出任何 .h 条目 —— 解析器失效或表被清空 MUST 视为违规"
    else
        note "  KEY FILES 解析出 $(printf '%s\n' "$key_names" | grep -c .) 个条目"
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
        # ⚠️ **fail-closed**：自述计数是 D9 的「磁盘是权威」契约的对照面 ⇒
        #    计数被删/改成别的措辞 ⇒ 无法校验 ⇒ MUST 视为违规，不得静默放行。
        violation "$CPU_AGENTS 里未找到「N headers, M lines」形式的自述计数 —— 计数被删或措辞变更 MUST 视为违规（ADR-046 D9）"
    else
        note "  自述计数解析出 $claim_n 处「N headers, M lines」"
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
