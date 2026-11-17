#!/usr/bin/env bash
# scripts/verify_verilator_pch.sh
#
# **vendored Verilator 的 PCH 工具链判据**（`verilator-pch-toolchain-fix-2026-11` T2）
#
# ⚠️ **为什么需要这条护栏**：verilator 走 **CMake** 构建 ⇒ 上游 autoconf 的 PCH 选项探测
#   （`third_party/verilator/configure.ac:704-712`）**没有变量源** ⇒ ⇒ 生成的
#   `verilated.mk` 里 `CFG_CXXFLAGS_PCH_I` 为**空** ⇒ ⇒ GCC 拿不到 PCH ⇒ ⇒
#   **任何「自动 output-split」的模型（> 20000 statements）C++ 构建必失败**
#   （实测：`g++: error: Vtop__pch.h.fast: linker input file not found`）。
#   ⇒ ⇒ 本护栏把该缺陷变成**机械可判**（今天零覆盖 ⇒ ⇒ 升级 submodule 会悄悄带回来）。
#
# 判据（缺一即红 · **fail-closed**）：
#   ① **构建期指纹**：安装产物的 `CFG_CXXFLAGS_PCH_I` **非空** 且与
#      `verilator -getenv CXX` 的**族属**匹配（GNU ⇒ `-include`；Clang ⇒ `-include-pch`）
#   ② **行为判据**：**自带最小设计**（现场生成 ⇒ 不依赖仓内任何产物）+ 强制
#      `--output-split` ⇒ 编译 rc=0 **且产出 `.gch`** ⇒ ⇒ 证明**真的**走了 PCH 路径
#      （否则「模型太小没 split」也会绿 ⇒ ⇒ 无判别力）
#   ③ `--self-test` **负向对照**：把**安装根的副本**的 flag 清空 ⇒ 同一编译**必红**
#      ⇒ ⇒ 证明判据**区分对错**；⚠️ **不碰真实安装产物**（用副本 ⇒ 零风险）
#
# 用法：
#   scripts/verify_verilator_pch.sh            # ① + ②
#   scripts/verify_verilator_pch.sh --self-test # ① + ② + ③（负向对照）
#
# ⚠️ **消息里 MUST NOT 含反引号**（shell 双引号里的反引号会被当命令替换）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
WORK_ROOT="${PCH_GUARD_WORK:-${TMPDIR:-/tmp/opencode}/verilator_pch_guard}"
SELF_TEST=0

for arg in "$@"; do
    case "$arg" in
    --self-test) SELF_TEST=1 ;;
    -h | --help)
        sed -n '3,30p' "$0"
        exit 0
        ;;
    *)
        echo "用法: $0 [--self-test]" >&2
        exit 2
        ;;
    esac
done

FAILED=0
ok() { echo "  [0;32m✅ $*[0m"; }
fail() { echo "  [0;31m❌ $*[0m"; FAILED=$((FAILED + 1)); }
info() { echo "  ── $*"; }

# ── 前置：verilator（fail-closed）────────────────────────────────────
VLT=""
for cand in "$REPO_ROOT/$BUILD_DIR/verilator-install/bin/verilator" "$(command -v verilator 2>/dev/null || true)"; do
    if [ -n "$cand" ] && [ -x "$cand" ]; then
        VLT="$cand"
        break
    fi
done
if [ -z "$VLT" ]; then
    echo "::error::找不到 verilator（缺 $BUILD_DIR/verilator-install/bin/verilator 且 PATH 里没有）⇒ fail-closed" >&2
    exit 2
fi
VROOT="$("$VLT" -getenv VERILATOR_ROOT 2>/dev/null | head -1)"
if [ -z "$VROOT" ] || [ ! -f "$VROOT/include/verilated.mk" ]; then
    echo "::error::拿不到 verilator 的安装根（含 include/verilated.mk）⇒ fail-closed" >&2
    exit 2
fi
# ⚠️ 避免继承来的 VERILATOR_ROOT 与本次探测不一致（verilator wrapper 会直接拒绝）
unset VERILATOR_ROOT

# ── ① 构建期指纹 ───────────────────────────────────────────────────
info "① 构建期指纹：CFG_CXXFLAGS_PCH_I 非空 + 与编译器族属匹配"
PCH_FLAG="$(sed -n 's/^CFG_CXXFLAGS_PCH_I[[:space:]]*=[[:space:]]*//p' "$VROOT/include/verilated.mk" | head -1)"
if [ -z "$PCH_FLAG" ]; then
    fail "① CFG_CXXFLAGS_PCH_I 为空 ⇒ GCC 分支拿不到 PCH ⇒ 自动 output-split 的模型必编译失败"
elif [ "$PCH_FLAG" = "-include" ]; then
    ok "① CFG_CXXFLAGS_PCH_I = -include（GCC 形态）"
elif [ "$PCH_FLAG" = "-include-pch" ]; then
    ok "① CFG_CXXFLAGS_PCH_I = -include-pch（Clang 形态）"
else
    fail "① CFG_CXXFLAGS_PCH_I = $PCH_FLAG（既不是 -include 也不是 -include-pch ⇒ 未知编译器形态）"
fi

CXX_USED="$("$VLT" -getenv CXX 2>/dev/null | head -1)"
CXX_FAMILY="$(printf '%s' "$CXX_USED" | sed -E 's/-?[0-9]+$//')"
case "$CXX_FAMILY" in
*g++* | *gcc*)
    if [ "$PCH_FLAG" = "-include" ]; then
        ok "① 编译器族（$CXX_FAMILY）与 PCH 选项匹配（GNU ⇒ -include）"
    else
        fail "① 编译器族（$CXX_FAMILY）需要 -include，实得 '$PCH_FLAG' ⇒ 编译模型必失败"
    fi
    ;;
*clang*)
    if [ "$PCH_FLAG" = "-include-pch" ]; then
        ok "① 编译器族（$CXX_FAMILY）与 PCH 选项匹配（Clang ⇒ -include-pch）"
    else
        fail "① 编译器族（$CXX_FAMILY）需要 -include-pch，实得 '$PCH_FLAG' ⇒ 编译模型必失败"
    fi
    ;;
*)
    fail "① 认不出编译模型用的编译器族（verilator -getenv CXX = '$CXX_USED'）⇒ 无法校验 PCH 选项"
    ;;
esac

# ── 最小设计（现场生成 ⇒ 不依赖仓内产物）────────────────────────────
# ⚠️ 必须**足够大**（> 200 statements）⇒ 强制 `--output-split 200` 后**真的**会 split
#   ⇒ ⇒ 才会走 PCH 路径 ⇒ ⇒ 判据才有判别力。
mk_mini_design() {
    local dir="$1"
    mkdir -p "$dir"
    {
        echo "module top ("
        echo "    input a,"
        echo "    output b"
        echo ");"
        echo "    logic [299:0] w;"
        echo "    assign w[0] = a;"
        local i
        for i in $(seq 1 299); do
            echo "    assign w[$i] = w[$((i - 1))] ^ a;"
        done
        echo "    assign b = w[299];"
        echo "endmodule"
    } >"$dir/top.v"
}

# 编译一次（$1 = 顶层设计目录 · $2 = obj 目录 · $3 = verilator 可执行文件）
# 回显：0 = 编译成功 · 1 = 编译失败
compile_design() {
    local design="$1" obj="$2" vlt="$3"
    rm -rf "$obj"
    mkdir -p "$obj"
    if ! "$vlt" --cc --Mdir "$obj" --output-split 200 "$design/top.v" \
            >"$obj/verilator.log" 2>&1; then
        echo "verilator 生成失败" >>"$obj/verilator.log"
        return 1
    fi
    # ⇒ 必须真的 split（VM_PARALLEL_BUILDS=1）⇒ 否则本判据无判别力
    if ! grep -q "VM_PARALLEL_BUILDS = 1" "$obj"/*classes.mk 2>/dev/null; then
        echo "模型没有自动 split（VM_PARALLEL_BUILDS != 1）⇒ 判据无判别力" >>"$obj/verilator.log"
        return 1
    fi
    if ! (cd "$obj" && make -f Vtop.mk "-j$(nproc)" >"$obj/make.log" 2>&1); then
        return 1
    fi
    # ⇒ PCH 真的被生成了 ⇒ ⇒ 「走了 PCH 路径」的机械证据
    if ! ls "$obj"/*.gch >/dev/null 2>&1; then
        echo "没有产出 .gch ⇒ 未走 PCH 路径" >>"$obj/make.log"
        return 1
    fi
    return 0
}

# ── ② 行为判据 ─────────────────────────────────────────────────────
info "② 行为判据：最小设计 + 强制 --output-split ⇒ 编译必须成功（且真的走 PCH 路径）"
rm -rf "$WORK_ROOT"
mkdir -p "$WORK_ROOT"
mk_mini_design "$WORK_ROOT/mini"
if compile_design "$WORK_ROOT/mini" "$WORK_ROOT/obj" "$VLT"; then
    ok "② 强制 split 的模型编译通过（且产出 .gch ⇒ PCH 路径被真实走到）"
else
    fail "② 强制 split 的模型编译失败 ⇒ 见 $WORK_ROOT/obj/verilator.log 与 make.log"
    grep -E "error:" "$WORK_ROOT/obj/make.log" 2>/dev/null | head -3 | sed 's/^/      /'
fi

# ── ③ 负向对照（--self-test）────────────────────────────────────────
if [ "$SELF_TEST" -eq 1 ]; then
    info "③ 负向对照：把安装根**副本**的 CFG_CXXFLAGS_PCH_I 清空 ⇒ 同一编译必须红"
    COPY_ROOT="$WORK_ROOT/vlt_copy"
    rm -rf "$COPY_ROOT"
    if ! cp -a "$VROOT" "$COPY_ROOT" 2>/dev/null; then
        fail "③ 复制安装根失败 ⇒ 自检环境有问题（真实安装产物零改动）"
    else
        sed -i 's/^CFG_CXXFLAGS_PCH_I[[:space:]]*=[[:space:]]*.*$/CFG_CXXFLAGS_PCH_I = /' \
            "$COPY_ROOT/include/verilated.mk"
        if grep -q "^CFG_CXXFLAGS_PCH_I = *$" "$COPY_ROOT/include/verilated.mk"; then
            ok "③ 副本的 flag 已清空（真实安装产物零改动）"
        else
            fail "③ 副本的 flag 没能清空 ⇒ 自检环境有问题"
        fi
        mk_mini_design "$WORK_ROOT/mini_neg"
        if compile_design "$WORK_ROOT/mini_neg" "$WORK_ROOT/obj_neg" "$COPY_ROOT/bin/verilator"; then
            fail "③ flag 清空后仍编译通过 ⇒ ⇒ 判据无判别力（缺陷已不可见）"
        else
            ok "③ flag 清空后编译转红 ✓（缺陷被抓住）"
        fi
    fi
fi

echo
if [ "$FAILED" -ne 0 ]; then
    echo "❌ verify_verilator_pch: $FAILED 项失败"
    exit 1
fi
echo "✅ verify_verilator_pch: PASS（构建期指纹 · 强制 split 编译 · PCH 路径被真实走到）"
exit 0
