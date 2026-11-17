#!/usr/bin/env bash
# scripts/verify_examples_rtl_sim_coverage.sh
#
# **examples 的 RTL 仿真覆盖登记面**的守恒护栏
# （`examples-rtl-sim-registration-hygiene-2026-11` T4）
#
# ⚠️ **为什么需要这条护栏**：产品化的「四件套」判据（ADR-072 D2）把
#   **覆盖登记**也算作产品单元的一部分 ⇒ ⇒ 但「登记」这件事本身一度**只剩散文**：
#   · 被跟踪的 example 从 **19 涨到 28**，而落选名单只按 19 登记
#     ⇒ ⇒ **9 个被跟踪 example 既不在覆盖表、也不在落选表**
#     ⇒ ⇒ 「**新增即漏**被堵住」这句承诺**已经失效**（实测 2026-11-17）；
#   · 覆盖表的 `spec_checks` 列被塞进散文 ⇒ ⇒ 护栏
#     `scripts/verify_examples_rtl_sim.sh` 的 ②'' 判据
#     （`[ "$spec" -lt 3 ]`）对 11 行里的 **4 行永远不生效**
#     ⇒ ⇒ 该判据的告警能力被削成 1/11（而进程 rc 仍为 0 ⇒ ⇒ **CI 仍绿**）。
#
# 判据（缺一即红 · **fail-closed**）：
#   ① **守恒**：被跟踪 target 集合 == （覆盖表里 `kind ∈ {target, soc}` ∩ 被跟踪）
#      ∪ （落选表登记的 target）⇒ ⇒ 且三者**互斥**（既做又落选 ⇒ 红）
#      ⇒ ⇒ **漏登记即红**（不再靠散文承诺）
#   ② **落选登记完备**：落选表每行 MUST 有非空 `reason` + 非空 `return_condition`
#      ⇒ ⇒ 且 target MUST 真在被跟踪集合里
#   ③ **列纯度**：覆盖表每行 MUST 是 **11 列** 且 `spec_checks` MUST 是**裸整数**
#      ⇒ ⇒ 护栏 ②'' 判据才可判（散文混进数值列 ⇒ 判据静默失效）
#   ④ `--self-test` **负向对照**：3 条投毒（删落选行 / 清空理由 / 污染数值列）
#      各自**转红** ⇒ ⇒ 还原复绿
#
# 用法：
#   scripts/verify_examples_rtl_sim_coverage.sh            # ① + ② + ③
#   scripts/verify_examples_rtl_sim_coverage.sh --self-test # + ④
#
# ⚠️ **纯静态**（不依赖 verilator / 网络 / git 历史）⇒ ⇒ 可以挂在任何 job 上。
# ⚠️ **消息里 MUST NOT 含反引号**（shell 双引号里的反引号会被当命令替换）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# ⚠️ 三份源文件都可被 env 覆盖 ⇒ ⇒ `--self-test` 能在**副本**上投毒（零风险）
EXAMPLES_CMAKE="${EXAMPLES_CMAKE:-examples/CMakeLists.txt}"
TARGETS_TSV="${RTL_SIM_TARGETS_TSV:-.rddf/checks/examples-rtl-sim-targets.tsv}"
EXCLUSIONS_TSV="${RTL_SIM_EXCLUSIONS_TSV:-.rddf/checks/examples-rtl-sim-exclusions.tsv}"

SELF_TEST=0
for arg in "$@"; do
    case "$arg" in
    --self-test) SELF_TEST=1 ;;
    -h | --help)
        sed -n '3,35p' "$0"
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

# ── R0：源文件齐备（fail-closed）────────────────────────────────────
for f in "$EXAMPLES_CMAKE" "$TARGETS_TSV" "$EXCLUSIONS_TSV"; do
    if [ ! -f "$f" ]; then
        echo "::error::缺少登记面源文件：$f ⇒ ⇒ fail-closed（禁静默跳过）" >&2
        exit 2
    fi
done

# ── 解析 ───────────────────────────────────────────────────────────
# ⚠️ 解析口径与运行护栏 `run_all_ported_tests.sh:129-132` **同构**
#   （`set(CH_TRACKED_EXAMPLE_TARGETS …)` 块内、4 空格缩进、非注释行）。
tracked="$(sed -n '/set(CH_TRACKED_EXAMPLE_TARGETS/,/^)/p' "$EXAMPLES_CMAKE" \
    | sed -n 's/^    \([A-Za-z0-9_]\{1,\}\)$/\1/p' | sort -u)"
# 覆盖表：`kind ∈ {target, soc}` 才是「已做 RTL 仿真」的产品单元
# （`kind=dut` 是 test DUT ⇒ 不是被跟踪 target ⇒ 不进守恒等式）
covered_all="$(awk -F'\t' '!/^#/ && NF >= 2 && ($2 == "target" || $2 == "soc") {print $1}' "$TARGETS_TSV" | sort -u)"
excluded="$(awk -F'\t' '!/^#/ && NF >= 1 && $1 != "" {print $1}' "$EXCLUSIONS_TSV" | sort -u)"

if [ -z "$tracked" ] || [ -z "$covered_all" ] || [ -z "$excluded" ]; then
    echo "::error::解析不出集合（tracked=$tracked covered=$covered_all excluded=$excluded）⇒ ⇒ fail-closed" >&2
    exit 2
fi
covered="$(comm -12 <(printf '%s\n' "$tracked") <(printf '%s\n' "$covered_all"))"

n_tracked="$(printf '%s\n' "$tracked" | grep -c .)"
n_covered="$(printf '%s\n' "$covered" | grep -c .)"
n_excluded="$(printf '%s\n' "$excluded" | grep -c .)"

# ── ① 守恒 ────────────────────────────────────────────────────────
info "① 守恒：被跟踪 $n_tracked == 已覆盖 $n_covered ∪ 已登记落选 $n_excluded（且互斥）"
missing="$(comm -23 <(printf '%s\n' "$tracked") <(printf '%s\n%s\n' "$covered" "$excluded" | sort -u))"
if [ -n "$missing" ]; then
    fail "① 漏登记：$(printf '%s' "$missing" | tr '\n' ' ')（既不在覆盖表、也不在落选表 ⇒ 「新增即漏」）"
else
    ok "① 无漏登记"
fi
overlap="$(comm -12 <(printf '%s\n' "$covered") <(printf '%s\n' "$excluded"))"
if [ -n "$overlap" ]; then
    fail "① 既覆盖又落选：$(printf '%s' "$overlap" | tr '\n' ' ')"
else
    ok "① 覆盖集与落选集互斥"
fi
ghost="$(comm -13 <(printf '%s\n' "$tracked") <(printf '%s\n' "$excluded"))"
if [ -n "$ghost" ]; then
    fail "① 落选表登记了**不在被跟踪集合**里的 target：$(printf '%s' "$ghost" | tr '\n' ' ')"
else
    ok "① 落选表 ⊆ 被跟踪集合"
fi

# ── ② 落选登记完备 ────────────────────────────────────────────────
info "② 落选登记完备：每行 MUST 有非空 reason + 非空 return_condition"
bad_rows="$(awk -F'\t' '!/^#/ && NF > 0 {
    if (NF != 4) { printf "%s(列数=%d)\n", $1, NF; next }
    if ($1 == "" || $3 == "" || $4 == "") { printf "%s(有空列)\n", $1 }
}' "$EXCLUSIONS_TSV")"
if [ -n "$bad_rows" ]; then
    fail "② 落选登记不完整：$(printf '%s' "$bad_rows" | tr '\n' ' ')"
else
    ok "② 落选表 $n_excluded 行逐行完整（target + reason + 归位条件）"
fi

# ── ③ 列纯度 ──────────────────────────────────────────────────────
info "③ 列纯度：覆盖表每行 MUST 是 11 列且 spec_checks MUST 是裸整数"
bad_cols="$(awk -F'\t' '!/^#/ && NF > 0 {
    if (NF != 11) { printf "%s(列数=%d)\n", $1, NF; next }
    if ($10 !~ /^[0-9]+$/) { printf "%s(spec_checks=%s)\n", $1, $10 }
}' "$TARGETS_TSV")"
if [ -n "$bad_cols" ]; then
    fail "③ 覆盖表的列不纯：$(printf '%s' "$bad_cols" | tr '\n' ' ')⇒ ⇒ 护栏 ②'' 判据会静默失效"
else
    ok "③ 覆盖表列纯（11 列 · spec_checks 全为整数）"
fi
dupes="$(awk -F'\t' '!/^#/ && NF > 0 {print $1}' "$TARGETS_TSV" | sort | uniq -d)"
if [ -n "$dupes" ]; then
    fail "③ 覆盖表有重复 target 行：$(printf '%s' "$dupes" | tr '\n' ' ')"
else
    ok "③ 覆盖表无重复行"
fi

# ── ④ --self-test（负向对照）───────────────────────────────────────
if [ "$SELF_TEST" -eq 1 ]; then
    info "④ 负向对照：3 条投毒各自 MUST 转红（投毒只发生在**副本**上 ⇒ 真实登记面零风险）"
    WORK="$(mktemp -d "${TMPDIR:-/tmp/opencode}/rtl_sim_coverage.XXXXXX" 2>/dev/null)" || {
        echo "::error cannot create a temp dir (mktemp failed)"; exit 2; }
    trap 'rm -rf "$WORK"' EXIT INT TERM
    cp "$EXAMPLES_CMAKE" "$WORK/CMakeLists.txt"
    cp "$TARGETS_TSV" "$WORK/targets.tsv"
    cp "$EXCLUSIONS_TSV" "$WORK/exclusions.tsv"

    run_poisoned() {
        env EXAMPLES_CMAKE="$WORK/CMakeLists.txt" \
            RTL_SIM_TARGETS_TSV="$WORK/targets.tsv" \
            RTL_SIM_EXCLUSIONS_TSV="$WORK/exclusions.tsv" \
            bash "$0" >"$WORK/out.log" 2>&1
    }

    # 基线：干净副本 MUST 绿（否则后面的「转红」无意义）
    if ! run_poisoned; then
        fail "④ 自检前置：干净副本竟不绿 ⇒ ⇒ fixture 构造有误（后面全部不可信）"
        cp "$EXCLUSIONS_TSV" "$WORK/exclusions.tsv"
        cp "$TARGETS_TSV" "$WORK/targets.tsv"
    else
        ok "④ 自检前置：干净副本绿 ✓"
    fi

    # 投毒 ①：删掉落选表的一行 ⇒ ⇒ 守恒必红
    cp "$EXCLUSIONS_TSV" "$WORK/exclusions.tsv"
    awk -F'\t' '!/^#/ && NF > 0 && $1 != "spi_controller_example"' "$EXCLUSIONS_TSV" >"$WORK/exclusions.tsv"
    if run_poisoned; then
        fail "④ 投毒 ①（删掉一条落选登记）⇒ 判据仍绿 ⇒ ⇒ 判据 ① 无判别力"
    else
        ok "④ 投毒 ①（删掉一条落选登记）⇒ 判据转红 ✓"
    fi

    # 投毒 ②：清空某行的理由 ⇒ ⇒ 完备性必红
    # ⚠️ 目标 MUST 是**落选表里真实存在**的行（`uart_rx_example` 是**已覆盖**项
    #   ⇒ ⇒ 不在落选表里 ⇒ ⇒ 投毒会落空 ⇒ ⇒ 自检会误报「无判别力」）
    cp "$EXCLUSIONS_TSV" "$WORK/exclusions.tsv"
    awk -F'\t' 'BEGIN{OFS="\t"} !/^#/ && NF > 0 && $1 == "stream_fork_demo" {$3=""; print; next} {print}' \
        "$EXCLUSIONS_TSV" >"$WORK/exclusions.tsv"
    if run_poisoned; then
        fail "④ 投毒 ②（清空一条落选理由）⇒ 判据仍绿 ⇒ ⇒ 判据 ② 无判别力"
    else
        ok "④ 投毒 ②（清空一条落选理由）⇒ 判据转红 ✓"
    fi

    # 投毒 ③：给数值列塞散文 ⇒ ⇒ 列纯度必红
    cp "$TARGETS_TSV" "$WORK/targets.tsv"
    awk -F'\t' 'BEGIN{OFS="\t"} !/^#/ && NF > 0 && $1 == "uart_tx_example" {$10=$10" · 散文"; print; next} {print}' \
        "$TARGETS_TSV" >"$WORK/targets.tsv"
    if run_poisoned; then
        fail "④ 投毒 ③（给 spec_checks 塞散文）⇒ 判据仍绿 ⇒ ⇒ 判据 ③ 无判别力"
    else
        ok "④ 投毒 ③（给 spec_checks 塞散文）⇒ 判据转红 ✓"
    fi

    # 还原复绿
    cp "$EXAMPLES_CMAKE" "$WORK/CMakeLists.txt"
    cp "$TARGETS_TSV" "$WORK/targets.tsv"
    cp "$EXCLUSIONS_TSV" "$WORK/exclusions.tsv"
    if run_poisoned; then
        ok "④ 还原后复绿 ✓"
    else
        fail "④ 还原后仍红 ⇒ ⇒ 判据有副作用（污染源文件？）"
    fi
fi

echo
if [ "$FAILED" -ne 0 ]; then
    echo "❌ verify_examples_rtl_sim_coverage: $FAILED 项失败"
    exit 1
fi
echo "✅ verify_examples_rtl_sim_coverage: PASS（守恒 $n_tracked == $n_covered ∪ $n_excluded · 落选登记完备 · 列纯度）"
exit 0
