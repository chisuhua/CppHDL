#!/usr/bin/env bash
# scripts/verify_hierarchical_p8.sh
#
# **P8 守卫**（roadmap「P8」条 · A4 裁决 · capability
# `hierarchical-equivalence-public-helper`）。
#
# ⚠️ **为什么有两条形态**（★ 本脚本经历了**一次判据方向反转** ⇒ 必读）：
#   · **T3-005 形态**（本脚本的 R4①）：「harness MUST 写成『可公共化』形态，但
#     **MUST NOT** 暴露公共 API」⇒ ⇒ R1/R2/R3 钉的是「**禁入** `include/`」。
#   · **P8 形态**（本脚本的 R1/R2/R2'/R3/R4②）：「harness **公共化**」⇒ ⇒ 公共头
#     **MUST** 在 `include/` ⇒ ⇒ ⇒ 那三条「禁入」判据**必然**与 P8 冲突。
#
# ⚠️ **改法不是「换成白名单」**（★ 那会严格**变弱**：∀ → ∃）⇒ ⇒ 改法是
#   **「全称否定 + 单一显式豁免」** ⇒ ⇒ **新判据 ⊇ 旧判据** ⇒ ⇒ 「强度不减」
#   这句话**才有载体**（不是一句断言）。
#
# 判据：
#   R1  **全称否定 + 单一字面豁免**：`include/` 下**文件名**匹配 `*equivalence*`
#       或 `*hier_eq*` 的文件 ⇒ MUST **逐字等于** `include/hierarchy_equivalence.h`
#       ⇒ ⇒ 改名绕过 ⇒ ⇒ R2'（内容维度）兜住
#   R2  内部实现符号 deny-list（★ **+ 双向核对**：每个 deny 符号 MUST **仍存在于
#       代码库某处** ⇒ ⇒ 否则「符号不存在 ⇒ grep 无命中 ⇒ 绿」⇒ ⇒ 判据**静默失效**）
#   R2'' **闭合集合**（新增）：公共头的**声明符号集** MUST **恰好**等于白名单
#       ⇒ ⇒ 加一个公开符号 MUST **同时**改白名单 + 改 spec
#       ⚠️ **枚举器的已知边界**（如实登记 · critic NIT-2）：它只看
#          `struct/class/enum` 声明 + **自由函数**（含模板函数）⇒ ⇒ **看不见**
#          成员函数 / `using` 别名 / `typedef` / 宏 / 命名空间级 `constexpr` 变量。
#          当前公共头**无**这些形态 ⇒ 今天无影响 ⇒ ⇒ ⇒ 它是 **tripwire**，
#          **不是**完备门（⇒ 完备性仍由「新增符号 MUST 改 spec」这条流程义务承担）。
#   R2' **内容维度**（新增）：**声明** `namespace ch::hier_eq` 的文件 ⇒ MUST **恰好是**
#       `include/hierarchy_equivalence.h` ⇒ ⇒ 堵住「改名绕过」
#   R3  公共头 MUST **确实存在**（★ **断言存在** ⇒ ⇒ MUST NOT 沿用旧脚本
#       「不存在 ⇒ 说明没有 ⇒ 绿」的逻辑）
#   R4  **拆两条**：
#       ① **历史窗口**（T3-005 的原判据 · base/head **两端钉死** ⇒ ⇒ **逐字保留**
#          ⇒ ⇒ 后续 change 合法修改 `include/` 不会误伤它）
#       ② **活账**（新增）：公共 API 面 MUST 登记在 `.rddf/checks/public-api-surface.md`
#          ⇒ ⇒ **双向核对**（登记了不存在 ⇒ 红 · 存在未登记 ⇒ 红）
#       ⚠️ **为什么②是必要的**：①的窗口**钉死**在 T3-005 的两个 commit ⇒ ⇒ P8 的
#          公共头**不在窗口内** ⇒ ⇒ 「白名单 4→5 扩容」是**空操作**（判别力零变化）
#
# 用法：
#   bash scripts/verify_hierarchical_p8.sh              # 主路径（R1–R4）
#   bash scripts/verify_hierarchical_p8.sh --self-test  # 逐条投毒 ⇒ 必红
#   RDDF_CHANGE_BASE=<ref> bash scripts/verify_hierarchical_p8.sh   # 覆盖①的 base
#   RDDF_CHANGE_HEAD=<ref> bash scripts/verify_hierarchical_p8.sh  # 覆盖①的 head
#
# 退出码：0 = 通过 · 1 = 判据失败
# ============================================================================
set -uo pipefail

cd "$(dirname "$0")/.." || exit 2
ROOT="$(pwd)"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/cpphdl_p8.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

fails=0
ok()   { printf '\033[0;32m✅ %s\033[0m\n' "$*"; }
fail() { printf '\033[0;31m❌ %s\033[0m\n' "$*"; fails=$((fails + 1)); }
note() { printf '  ::notice %s\n' "$*"; }

# ── 已裁决的公开 API 面（T3-005 的四个公共头）──────────────────────────────
# ⚠️ **历史窗口①**的 allowlist ⇒ ⇒ **逐字保留**（⇒ ⇒ 后续 change 不误伤）。
ALLOWED_INCLUDE=(
    "include/codegen_verilog.h"
    "include/hierarchy_verilog.h"
    "include/core/verilator_backend.h"
    "include/core/verilator_port_set.h"
)

# ── 本 change 的 base（= T3-005 的第一个 commit 的父）─────────────────────────
# ⚠️ 可用 `RDDF_CHANGE_BASE` 覆盖（例如在别的 checkout 里跑）
DEFAULT_BASE="1453a8f92bb31d16b3e1d7594bd385f7ea835f55"
BASE="${RDDF_CHANGE_BASE:-$DEFAULT_BASE}"

# ── 本 change 的 **head**（= T3-005 的归档 commit）───────────────────────────
# ⚠️ **MUST 钉死，MUST NOT 用移动的 `HEAD`**：
#    R4① 的契约是「**本 change**（T3-005）的公开 API 面 MUST ⊆ 4 个已裁决公共头」
#    ⇒ ⇒ 用 `HEAD` 会让窗口随仓库前进而**无限延长** ⇒ ⇒ 之后**任何**合法修改 `include/`
#    的 change 都会让它转红 ⇒ ⇒ 判据与自身契约**漂移**。
DEFAULT_HEAD="5f8f35702066e98495927b2952a922532c8ca59c"
HEAD_REF="${RDDF_CHANGE_HEAD:-$DEFAULT_HEAD}"

# ── P8 公共头（P8 形态的**唯一**豁免点）────────────────────────────────────
PUBLIC_HEADER="include/hierarchy_equivalence.h"
PUBLIC_NAMESPACE="ch::hier_eq"

# ── 公共符号闭合集合（A2）───────────────────────────────────────────────────
# ⚠️ **加一个公开符号 MUST 同时改**：① 这里的白名单 ② capability 的规范正文
#    ⇒ ⇒ ⇒ 「公共面会静默扩张」被机械拦住（★ **不是**靠评审记得）。
PUBLIC_SYMBOLS=(
    "DrivenInput"
    "Observation"
    "ProductSet"
    "Result"
    "emit"
    "compare"
    "make_observation"
)

# ── 内部实现符号 deny-list（R2）────────────────────────────────────────────
# ⚠️ 双向核对：每个符号 MUST **仍存在于代码库某处** ⇒ ⇒ 否则它就是**过期条目**
#    ⇒ ⇒ 「grep 无命中 ⇒ 绿」⇒ ⇒ 判据静默失效 ⇒ ⇒ 必须报错（而不是安静地放过）。
DENY_SYMBOLS=(
    # ⚠️ `ch_test::hier::impl` 是**历史名**（P8 之后该命名空间已不存在 ⇒ ⇒ 它的
    #    「仍存在」核对靠 `tests/hier_equivalence.h` 的**注释字符串**满足）
    #    ⇒ ⇒ 条目性质 = 「防旧命名空间**复辟**的 tripwire」⇒ ⇒ **不是**活符号。
    "ch_test::hier::impl"
    "emit_products"
    "compare_products"
    "hier_eq_impl"
    "ensure_dir"
    "read_whole_file"
)

# ── 活账（R4②）─────────────────────────────────────────────────────────────
REGISTRY=".rddf/checks/public-api-surface.md"
# ⚠️ 「公共 API 面」的**扫描范围**：顶层 `include/*.h` + 这两个已裁决的 core 头
#    （★ 顶层头 = 用户会直接 `#include` 的入口 ⇒ ⇒ 新增顶层头 MUST 登记）
SCAN_EXTRA=(
    "include/core/verilator_backend.h"
    "include/core/verilator_port_set.h"
)

# ── 公共头的符号枚举器（python3 ⇒ ⇒ 免去 bash 正则的脆弱性）────────────────
cat > "$WORK/symbols.py" <<'PYEOF'
import re
import sys

text = open(sys.argv[1], encoding='utf-8').read()
# ⚠️ 去掉注释与字符串字面量（⇒ ⇒ 注释里的「struct Foo」不算声明）
text = re.sub(r'//[^\n]*', '', text)
text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)

names = set()
# ① struct / class / enum 声明
for m in re.finditer(r'(?m)^\s*(?:struct|class|enum\s+class|enum)\s+([A-Za-z_]\w*)', text):
    names.add(m.group(1))
# ② 自由函数（返回类型 + 名字 + `(`）—— 只取**顶格**的（命名空间作用域内）
for m in re.finditer(r'(?m)^[A-Za-z_][\w:<>,\s&*]*?\s+([A-Za-z_]\w*)\s*\(', text):
    names.add(m.group(1))
# ③ 模板自由函数（`template <...>` 之后的返回类型 + 名字）
for m in re.finditer(
        r'(?m)^template\s*<[^>]*>\s*[\w:<>,\s&*]+?\s+([A-Za-z_]\w*)\s*\(', text):
    names.add(m.group(1))
# ④ 去掉 C++ 关键字噪声
for kw in ('return', 'if', 'for', 'while', 'switch', 'sizeof', 'static_cast',
           'dynamic_cast', 'reinterpret_cast', 'const_cast', 'new', 'delete'):
    names.discard(kw)
for n in sorted(names):
    print(n)
PYEOF

# ── 检查函数（供主路径与 --self-test **共用同一份** ⇒ 不会漂移）──────────────

# R1：全称否定 + 单一字面豁免。
# $1 = include/ 的根目录（self-test 投毒时指向**副本** ⇒ ⇒ 绝不改仓库）
check_r1() {
    local inc="$1" prefix="${2:-include}" bad=0 f
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        if [ "$f" != "$PUBLIC_HEADER" ]; then
            fail "R1 include/ 下出现**未获豁免**的 harness 头（文件名匹配 *equivalence* / *hier_eq*）："
            printf '     %s\n' "$f" | sed 's/^/     /'
            bad=1
        fi
    done < <(find "$inc" -type f \( -name '*equivalence*' -o -name '*hier_eq*' \) 2>/dev/null | sed "s|^$inc/|$prefix/|")
    if [ "$bad" -eq 0 ]; then
        ok "R1 include/ 下匹配 *equivalence*/*hier_eq* 的文件 ≡ $PUBLIC_HEADER（唯一豁免）"
    fi
    return $bad
}

# R2：内部实现符号 deny-list + 双向核对。
# $1 = include/ 的根目录 · $2 = 「符号仍存在」的搜索根（默认 src/ tests/）
check_r2() {
    local inc="$1" impl_root="${2:-src tests}" bad=0 sym hits
    for sym in "${DENY_SYMBOLS[@]}"; do
        hits="$(grep -rl -F -- "$sym" "$inc" 2>/dev/null)"
        if [ -n "$hits" ]; then
            fail "R2 include/ 下出现内部实现符号 '$sym'："
            printf '%s\n' "$hits" | sed 's/^/     /'
            bad=1
        fi
        # ⚠️ **双向核对**：该 deny 符号 MUST **仍存在于代码库某处**
        #    ⇒ ⇒ 否则它是**过期条目** ⇒ ⇒ 「grep 无命中 ⇒ 绿」⇒ ⇒ 判据静默失效。
        if ! grep -rq -F -- "$sym" $impl_root 2>/dev/null; then
            fail "R2 deny 符号 '$sym' 在代码库里**已不存在** ⇒ 判据条目**过期** ⇒ MUST 移除或改名"
            bad=1
        fi
    done
    if [ "$bad" -eq 0 ]; then
        ok "R2 include/ 下无内部实现符号（${#DENY_SYMBOLS[@]} 个 · 且全部仍存在于代码库）"
    fi
    return $bad
}

# R2'：内容维度 —— 声明 `namespace ch::hier_eq` 的文件 MUST 恰好是那一个。
# $1 = include/ 的根目录
check_r2prime() {
    local inc="$1" prefix="${2:-include}" bad=0 f count
    count=0
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        count=$((count + 1))
        if [ "$f" != "$PUBLIC_HEADER" ]; then
            fail "R2' 另一个文件声明了 namespace $PUBLIC_NAMESPACE：$f"
            bad=1
        fi
    done < <(grep -rl -F -- "namespace $PUBLIC_NAMESPACE" "$inc" 2>/dev/null | sed "s|^$inc/|$prefix/|")
    if [ "$count" -eq 0 ]; then
        fail "R2' include/ 下**没有任何**文件声明 namespace $PUBLIC_NAMESPACE ⇒ 公共头缺失"
        bad=1
    fi
    if [ "$bad" -eq 0 ]; then
        ok "R2' 声明 namespace $PUBLIC_NAMESPACE 的文件 ≡ $PUBLIC_HEADER（唯一）"
    fi
    return $bad
}

# R2''：公共头的**声明符号集** MUST **恰好**等于白名单（★ 「闭合集合」的机械载体
#      ⇒ ⇒ 加一个公开符号 MUST 同时改白名单 + 改 spec ⇒ ⇒ ⇒ 公共面不会静默扩张）。
# $1 = include/ 的根目录
check_r2_symbols() {
    local inc="$1" bad=0
    local hdr="$inc/$(basename "$PUBLIC_HEADER")"
    if [ ! -f "$hdr" ]; then
        fail "R2'' 公共头不存在 ⇒ 无法枚举其符号"
        return 1
    fi
    local found
    found="$(python3 "$WORK/symbols.py" "$hdr" 2>/dev/null)"
    if [ -z "$found" ]; then
        fail "R2'' 符号枚举失败（python3 不可用或解析异常）⇒ 判据**空转** ⇒ MUST 修复"
        return 1
    fi
    local declared extra missing
    declared="$(printf '%s\n' "$found" | sort -u)"
    # ① 多余的符号（白名单之外）
    while IFS= read -r sym; do
        [ -n "$sym" ] || continue
        local in_list=0 w
        for w in "${PUBLIC_SYMBOLS[@]}"; do
            [ "$sym" = "$w" ] && in_list=1 && break
        done
        if [ "$in_list" -ne 1 ]; then
            fail "R2'' 公共头多出一个**未登记**的符号：$sym"
            bad=1
        fi
    done <<< "$declared"
    # ② 缺失的符号（白名单里有 ⇒ 头里必须有）
    for w in "${PUBLIC_SYMBOLS[@]}"; do
        if ! printf '%s\n' "$declared" | grep -qx -- "$w"; then
            fail "R2'' 公共头**缺少**已登记的符号：$w"
            bad=1
        fi
    done
    if [ "$bad" -eq 0 ]; then
        ok "R2'' 公共头的声明符号集 ≡ 闭合集合（${#PUBLIC_SYMBOLS[@]} 项 · 双向核对）"
    fi
    return $bad
}

# R3：公共头 MUST **确实存在**（断言存在 ⇒ ⇒ 非「不存在 ⇒ 说明没有 ⇒ 绿」）。
# $1 = include/ 的根目录
check_r3() {
    local inc="$1"
    if [ -f "$inc/$(basename "$PUBLIC_HEADER")" ]; then
        ok "R3 公共头确实存在：$PUBLIC_HEADER"
        return 0
    fi
    fail "R3 公共头不存在：$PUBLIC_HEADER ⇒ R1/R2' 可能是「文件还没建」的假绿"
    return 1
}

# R4①：历史窗口（T3-005 的原判据 · 逐字保留）。
check_r4_window() {
    if ! git rev-parse --verify "$BASE" >/dev/null 2>&1; then
        note "R4① 跳过：base '$BASE' 不可解析（CI 的浅克隆没有这段历史）"
        note "     ⇒ 在有完整历史的环境用 RDDF_CHANGE_BASE=<ref> 重跑"
        return 0
    fi
    if ! git rev-parse --verify "$HEAD_REF" >/dev/null 2>&1; then
        note "R4① 跳过：head '$HEAD_REF' 不可解析（CI 的浅克隆没有这段历史）"
        note "     ⇒ 在有完整历史的环境用 RDDF_CHANGE_HEAD=<ref> 重跑"
        return 0
    fi
    git diff --name-only "$BASE".."$HEAD_REF" -- include/ | sort > "$WORK/include_files.txt"
    local bad=0 f allowed=0 a
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        allowed=0
        for a in "${ALLOWED_INCLUDE[@]}"; do
            [ "$f" = "$a" ] && allowed=1 && break
        done
        if [ "$allowed" -ne 1 ]; then
            fail "R4① 公开 API 面越界：'$f' 不在已裁决的 4 个公共头之内"
            bad=1
        fi
    done < "$WORK/include_files.txt"
    if [ "$bad" -eq 0 ]; then
        ok "R4① T3-005 历史窗口的公开 API 面 ⊆ 已裁决的 4 个公共头"
    fi
    return $bad
}

# R4②：活账双向核对。
# $1 = include/ 的根目录 · $2 = **逻辑前缀**（★ 默认 `include` ⇒ ⇒ self-test 指向
#      副本时**逻辑路径不变** ⇒ ⇒ 登记的路径与扫描的路径**同口径**）
check_r4_registry() {
    local inc="$1" prefix="${2:-include}" bad=0 registered found_path f
    if [ ! -f "$REGISTRY" ]; then
        fail "R4② 活账不存在：$REGISTRY ⇒ 新增的公共头**无人登记** ⇒ 公开 API 面会静默扩张"
        return 1
    fi
    # ① 登记了不存在 ⇒ 红
    #    ⚠️ **只查「登记清单」段**（= 登记为**存在**的公开头）⇒ ⇒
    #    「已移除 / 已改名的公开符号」段登记的路径**按定义就不存在**
    #    ⇒ ⇒ 查它 = **假阳性 by construction**（2026-11-13 实测：B0 批退役的
    #    3 个头在该段有登记 ⇒ ⇒ 护栏红 ⇒ ⇒ 登记为 **F-2**）。
    #    ⇒ ⇒ **该段的路径 MUST NOT 被删**（它是「移除 / 改名登记」的**证据**）
    #    ⇒ ⇒ ⇒ 只是**不要求它存在** ⇒ ⇒ 判据**强度不减**（登记清单段照查）。
    local reg_scan
    reg_scan="$(awk '
        /^## .*已移除/ { skip = 1; next }
        /^## / { skip = 0 }
        !skip { print }
    ' "$REGISTRY" 2>/dev/null || true)"
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        if [ ! -f "$f" ]; then
            fail "R4② 活账登记的头**不存在**：$f"
            bad=1
        fi
    done < <(printf '%s\n' "$reg_scan" | grep -oE '`include/[^*`]+`' 2>/dev/null | tr -d '`' | sort -u)
    # ② 存在未登记 ⇒ 红（扫描范围 = 顶层 include/*.h + SCAN_EXTRA）
    #    ⚠️ **登记面同样只取「登记清单」段**（`$reg_scan`）⇒ ⇒
    #    否则「把一个**活着**的公共头塞进『已移除』段」就能冒充「已登记」
    #    ⇒ ⇒ ⇒ 收紧后**强度不减反增**（①② 都只看登记清单段）。
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        registered=0
        while IFS= read -r registered_path; do
            [ "$registered_path" = "$f" ] && registered=1 && break
        done < <(printf '%s\n' "$reg_scan" | grep -oE '`include/[^*`]+`' 2>/dev/null | tr -d '`' | sort -u)
        if [ "$registered" -ne 1 ]; then
            fail "R4② 公共头**未登记**在活账里：$f ⇒ 扫描范围内的公开 API 面静默扩张"
            bad=1
        fi
    done < <({ find "$inc" -maxdepth 1 -type f -name '*.h' 2>/dev/null | sed "s|^$inc/|$prefix/|"
               for a in "${SCAN_EXTRA[@]}"; do printf '%s\n' "$a"; done; } | sort -u)
    if [ "$bad" -eq 0 ]; then
        ok "R4② 公共 API 面 ≡ 活账（双向核对通过）"
    fi
    return $bad
}

# ★ **聚合判定**（★ self-test 的每条投毒 MUST 由**这个**函数判 ⇒ ⇒ 否则
#   「只跑单条检查」会漏掉「别的检查本来就能抓住」的情形 ⇒ ⇒ 投毒**无效**）。
# $1 = include/ 的根目录 · $2 = 逻辑前缀
judge() {
    local inc="$1" prefix="${2:-include}" bad=0
    check_r1 "$inc" "$prefix" >/dev/null 2>&1 || bad=1
    check_r2 "$inc" >/dev/null 2>&1 || bad=1
    check_r2prime "$inc" "$prefix" >/dev/null 2>&1 || bad=1
    check_r2_symbols "$inc" >/dev/null 2>&1 || bad=1
    check_r3 "$inc" >/dev/null 2>&1 || bad=1
    check_r1_include_graph "$inc" >/dev/null 2>&1 || bad=1
    check_r4_registry "$inc" "$prefix" >/dev/null 2>&1 || bad=1
    return $bad
}

run_main() {
    check_r1 include
    check_r2 include
    check_r2prime include
    check_r2_symbols include
    check_r3 include
    check_r1_include_graph include
    check_r4_window
    check_r4_registry include
}

# ═══════════════════════════════════════════════════════════════════════════
# --self-test：逐条投毒 ⇒ 必红（预期红）/ 必绿（反向对照）⇒ 还原复绿
# ⚠️ **投毒的是「include/ 的副本」⇒ ⇒ 绝不改仓库**（⇒ ⇒ 中断也不会污染工作区）
# ═══════════════════════════════════════════════════════════════════════════
run_self_test() {
    local rc=0
    local poison_root="$WORK/poison_include"

    # ① 基线 MUST 绿（否则投毒无效）
    rm -rf "$poison_root"; mkdir -p "$poison_root"
    cp -r include/. "$poison_root/" 2>/dev/null
    if judge "$poison_root" include >/dev/null 2>&1; then
        ok "self-test：基线绿"
    else
        fail "self-test：基线**不绿** ⇒ 判据在干净树上就失败 ⇒ 投毒无效"
        return 1
    fi

    # ② 投毒 ①：内部实现符号泄漏到公共面 ⇒ MUST 红
    rm -rf "$poison_root"; mkdir -p "$poison_root"; cp -r include/. "$poison_root/" 2>/dev/null
    printf '\n// emit_products\n' >> "$poison_root/hierarchy_equivalence.h"
    if judge "$poison_root" include >/dev/null 2>&1; then
        fail "self-test[① 内部实现符号泄漏]：竟被判绿"
        rc=1
    else
        ok "self-test[① 内部实现符号泄漏] ⇒ 红"
    fi

    # ③ 投毒 ②：公共头**改名**（内容保留 ⇒ ⇒ 仍声明 `namespace ch::hier_eq`）
    #    ⇒ ⇒ MUST 被 R2'（内容维度）+ R3（存在性）抓住
    #    ⇒ ⇒ ⚠️ 若只跑 R1 ⇒ ⇒ R1 的**全称否定**会「找不到匹配文件 ⇒ 恒绿」
    #    ⇒ ⇒ ⇒ 这正是「**必须由聚合 judge 判**」的原因。
    rm -rf "$poison_root"; mkdir -p "$poison_root"; cp -r include/. "$poison_root/" 2>/dev/null
    mv "$poison_root/hierarchy_equivalence.h" "$poison_root/hierarchy_equiv_api.h"
    if judge "$poison_root" include >/dev/null 2>&1; then
        fail "self-test[② 公共头改名]：竟被判绿"
        rc=1
    else
        ok "self-test[② 公共头改名] ⇒ 红"
    fi

    # ④ 投毒 ③：公共头**缺失** ⇒ MUST 红
    rm -rf "$poison_root"; mkdir -p "$poison_root"; cp -r include/. "$poison_root/" 2>/dev/null
    rm -f "$poison_root/hierarchy_equivalence.h"
    if judge "$poison_root" include >/dev/null 2>&1; then
        fail "self-test[③ 公共头缺失]：竟被判绿"
        rc=1
    else
        ok "self-test[③ 公共头缺失] ⇒ 红"
    fi

    # ⑤ 投毒 ④：**多出第 2 个公共头**，且名字**不**命中 `*equivalence*` / `*hier_eq*`
    #    ⇒ ⇒ 它**不会**被 R1 抓到 ⇒ ⇒ MUST 由 R4②（活账）抓到
    #    ⇒ ⇒ ⚠️ 若用命中模式的文件名 ⇒ ⇒ 红的是**旧**判据 ⇒ ⇒ 证明不了活账机制。
    rm -rf "$poison_root"; mkdir -p "$poison_root"; cp -r include/. "$poison_root/" 2>/dev/null
    printf '#pragma once\n// poison\n' > "$poison_root/hierarchy_compare_public.h"
    if judge "$poison_root" include >/dev/null 2>&1; then
        fail "self-test[④ 多出第 2 个公共头]：竟被判绿"
        rc=1
    else
        ok "self-test[④ 多出第 2 个公共头（名字不命中旧模式）] ⇒ 红"
    fi

    # ⑥ 投毒 ⑤：公共头 `#include` 了**内部实现头** ⇒ R2 的**盲区**（符号名检查
    #    看不见 `#include` 图）⇒ ⇒ 由 R1'（include 图检查）抓
    rm -rf "$poison_root"; mkdir -p "$poison_root"; cp -r include/. "$poison_root/" 2>/dev/null
    printf '\n#include "hierarchy_equivalence_impl.h"\n' >> "$poison_root/hierarchy_equivalence.h"
    if judge "$poison_root" include >/dev/null 2>&1; then
        fail "self-test[⑤ 公共头 include 了内部实现头]：竟被判绿"
        rc=1
    else
        ok "self-test[⑤ 公共头 include 了内部实现头] ⇒ 红"
    fi

    # ⑦ 投毒 ⑥：公共面**多**一个符号 ⇒ MUST 红（R2'' 的「闭合集合」方向）
    #    ⇒ ⇒ ⇒ 「加一个公开符号」被机械拦住 ⇒ ⇒ ⇒ 公共面不会静默扩张
    rm -rf "$poison_root"; mkdir -p "$poison_root"; cp -r include/. "$poison_root/" 2>/dev/null
    printf '\n/// poison\nint poison_symbol();\n' >> "$poison_root/hierarchy_equivalence.h"
    if judge "$poison_root" include >/dev/null 2>&1; then
        fail "self-test[⑥ 公共面多一个符号]：竟被判绿"
        rc=1
    else
        ok "self-test[⑥ 公共面多一个符号] ⇒ 红"
    fi

    # ⑧ 投毒 ⑦：活账登记了一个**不存在**的头 ⇒ MUST 红（R4② 的另一方向）
    #    ⇒ ⇒ 用**活账副本**投毒（★ 绝不改仓库里的真活账）
    local reg_backup="$WORK/registry.bak"
    cp "$REGISTRY" "$reg_backup"
    printf '\n| `include/does_not_exist_xyz.h` | poison | 2026-11-07 |\n' >> "$REGISTRY"
    rm -rf "$poison_root"; mkdir -p "$poison_root"; cp -r include/. "$poison_root/" 2>/dev/null
    if judge "$poison_root" include >/dev/null 2>&1; then
        fail "self-test[⑦ 活账登记了不存在的头]：竟被判绿"
        rc=1
    else
        ok "self-test[⑦ 活账登记了不存在的头] ⇒ 红"
    fi
    cp "$reg_backup" "$REGISTRY"

    # ⑧ 反向对照：投毒全部还原 ⇒ MUST 复绿
    rm -rf "$poison_root"; mkdir -p "$poison_root"; cp -r include/. "$poison_root/" 2>/dev/null
    if judge "$poison_root" include >/dev/null 2>&1; then
        ok "self-test：还原后复绿"
    else
        fail "self-test：还原后仍红 ⇒ 判据自身有问题"
        rc=1
    fi

    return $rc
}

# R1'：公共头 MUST NOT **间接**引入内部实现头（★ R2 的符号名检查的盲区）。
# $1 = include/ 的根目录
check_r1_include_graph() {
    local inc="$1" bad=0 line
    while IFS= read -r line; do
        case "$line" in
            *hierarchy_equivalence_impl*|*hierarchy_chain*|*hierarchy_port_check*|\
            *hierarchy_port_set_check*|*hierarchy_signal_map_check*)
                fail "R1' 公共头间接引入了内部实现头：$line"
                bad=1
                ;;
        esac
    done < <(grep -nE '^\s*#\s*include' "$inc/$(basename "$PUBLIC_HEADER")" 2>/dev/null)
    if [ "$bad" -eq 0 ]; then
        ok "R1' 公共头未间接引入内部实现头"
    fi
    return $bad
}

if [ "${1:-}" = "--self-test" ]; then
    run_self_test
    exit $?
fi

run_main
if [ "$fails" -ne 0 ]; then
    printf '\033[0;31mverify_hierarchical_p8: FAIL（%d 条违规）\033[0m\n' "$fails"
    exit 1
fi
printf '\033[0;32m✅ verify_hierarchical_p8: PASS（R1–R4）\033[0m\n'
exit 0
