#!/usr/bin/env bash
# =============================================================================
# verify_rv32i_official_rtl.sh
#
#   **官方 riscv-tests 套件的「交付物级」门禁**（RTL 产物侧）
#
# ## 判据是什么
#
#   官方 `rv32ui-p-*`（物理内存）测试集 MUST 在 **RTL 产物**上全过 ⇒ ⇒ 判定口径
#   = **上游 env 的 `tohost` 协议**（`tohost == 1` ⇒ PASS；`tohost == TESTNUM|1` ⇒ FAIL）
#   ⇒ ⇒ 与**核级**门禁（`examples/riscv-mini/tests/test_rv32i_official_suite.cpp`）**同源**。
#
# ## 为什么需要这一层（**不是冗余**）
#
#   ⚠️ 核级门禁跑在**解释器**上 ⇒ ⇒ 它判的是「**本仓那份 C++**」的行为，**不是**交付的
#      Verilog 产物 ⇒ ⇒ **codegen 侧缺陷（初值块 / 端口映射 / 时序形状 / 宽度）在解释器侧
#      永远看不见**（仓内 AGENTS.md §ANTI-PATTERNS：「两后端一致」不是正确性证据 —— 同源缺陷
#      两后端一致地错）。
#   ⚠️ **本门禁的存在已被实证**：初版跑出来 **40/40 个带固件产物的 lint 全红**
#      （`WIDTHEXPAND` ⇒ 按仓内口径**编译直接失败**）⇒ ⇒ 而既有 3 个 RTL 护栏 lint 的产物
#      形态**都没有** TCM 初值块 ⇒ ⇒ 那个形态**从未被 lint 过**。
#
# ## 流程（**逐镜像独立**）
#
#   发射（`--emit-rtl --firmware <镜像目录>` + `--emit-hygiene`）
#     ⇒ ⇒ lint（退出码 0 + 零 `%Error`）        ← **门禁的一环，不是可选步骤**
#     ⇒ ⇒ 编译（仓内口径 flags）
#     ⇒ ⇒ 运行（`+tohost_expected=1`）⇒ ⇒ 判 `tohost`
#
# ## 负向对照（**防恒绿**）
#
#   手写镜像把 **3** 写进 `tohost`（上游协议的 FAIL 值形态）⇒ ⇒ 门禁 MUST 如实读到 `3`
#   ⇒ ⇒ 读到 `3` ⇒ 绿；读到 `1` ⇒ ⇒ **红**（判定被吞 ⇒ 恒绿）。
#   ⇒ ⇒ 且门禁**自证**：同一个负镜像用 `+tohost_expected=1` 跑 ⇒ ⇒ MUST **红**。
#
# ## 用法
#
#   bash scripts/verify_rv32i_official_rtl.sh [--full | --subset N]
#
#   · `--full`    ⇒ 全部 40 个镜像（≈ 10 min · verilator 编译主导）
#   · `--subset N`⇒ 前 N 个镜像（按名字排序）⇒ PR 快通道
#   · 默认        ⇒ `--subset 8`
#
# ## fail-closed（**不静默跳过**）
#
#   缺 verilator / 缺发射器 / 缺 TB / 缺镜像 ⇒ ⇒ **exit 2**（不是 skip · 不是绿）
#   ⚠️ 依据：`.rddf/checks/guard-reachability.md` 纪律 3 —— 写完护栏 ≠ 交付
#      ⇒ MUST 在 CI 里**真正 executed 且绿**过（`skipped` **不算**）。
#
# SPDX-License-Identifier: BSD-3-Clause
# =============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT" || exit 2

IMAGES_DIR="$REPO_ROOT/examples/riscv-mini/tests/riscv-tests/images"
NEG_DIR="$REPO_ROOT/examples/riscv-mini/tests/riscv-tests/negative"
TB_PATH="$REPO_ROOT/examples/riscv-mini/tb_rv32i_official.sv"
EMITTER="$REPO_ROOT/build/examples/riscv-mini/tb_rv32i_soc"
WORK_DIR="${WORK_DIR:-/tmp/rddf-rv32i-official-rtl}"
# ⚠️ 层次化抽样名单（2 个：① 常规 ② 含多次 store 的 `st_ld`）
HIER_SUBSAMPLE=("rv32ui-p-add" "rv32ui-p-st_ld")

SUBSET=8
FULL=0
while [ $# -gt 0 ]; do
    case "$1" in
        --full)    FULL=1; shift ;;
        --subset)  SUBSET="${2:-}"; shift 2 ;;
        --subset=*) SUBSET="${1#*=}"; shift ;;
        *) echo "FATAL：未知参数 $1（允许 --full / --subset N）" >&2; exit 2 ;;
    esac
done
if [ "$FULL" = "1" ]; then SUBSET=0; fi   # 0 = 全部
# ⚠️ **fail-closed**：`--subset` MUST 是非负整数 ⇒ ⇒ 否则下面 `[ "$SUBSET" -ge ... ]`
#   走 bash 算术求值 ⇒ 垃圾输入会得到**空选择** ⇒ ⇒ 只剩 2 个 hier 抽样撑 PASS（假绿）。
case "$SUBSET" in
    ''|*[!0-9]*) echo "FATAL：--subset 需要一个非负整数（拿到 '$SUBSET'）" >&2; exit 2 ;;
esac

# ── 进度 / 结果输出 ──────────────────────────────────────────────────────────
say()  { printf '%s\n' "$*"; }
ok()   { printf '  ✅ %s\n' "$*"; }
bad()  { printf '  ❌ %s\n' "$*"; }
fail() { printf '❌ %s\n' "$*" >&2; exit 1; }

# ── 前置（fail-closed）───────────────────────────────────────────────────────
say "═══ 官方 riscv-tests · RTL 产物级门禁 ═══"

VERILATOR=""
for cand in "$REPO_ROOT/build/verilator-install/bin/verilator" "$(command -v verilator || true)"; do
    [ -n "$cand" ] && [ -x "$cand" ] && { VERILATOR="$cand"; break; }
done
[ -n "$VERILATOR" ] || { fail "找不到 verilator（build/verilator-install/bin/verilator 或 PATH）⇒ fail-closed"; }
[ -x "$EMITTER" ]   || { fail "找不到发射器（$EMITTER）⇒ 先构建（cmake --build build）"; }
[ -f "$TB_PATH" ]   || { fail "找不到 TB（$TB_PATH）"; }
[ -d "$IMAGES_DIR" ]|| { fail "找不到官方镜像目录（$IMAGES_DIR）"; }
[ -d "$NEG_DIR" ]   || { fail "找不到负向对照镜像目录（$NEG_DIR）"; }
[ -x "$VERILATOR" ] || fail "verilator 不可执行"

say "  verilator：$VERILATOR（$("$VERILATOR" --version 2>&1 | head -1)）"
say "  发射器：    $EMITTER"
say "  TB：        $TB_PATH"
rm -rf "$WORK_DIR"; mkdir -p "$WORK_DIR"

# ── 产物级判据（**每个镜像一次**）──────────────────────────────────────────
#   $1 = 镜像目录名 · $2 = 期望的 tohost 值 · $3 = 形态（flat|hier）
PASS=0; FAIL=0
declare -a FAILED=()

run_one() {
    local name="$1" expect="$2" form="$3"
    local img="$IMAGES_DIR/$name"
    local w="$WORK_DIR/$name.$form"
    local emit_args=(--emit-rtl --firmware "$img" --out "$w" --emit-hygiene)
    [ "$form" = "hier" ] && emit_args+=(--hierarchical)

    rm -rf "$w"; mkdir -p "$w"

    # ① 发射
    if ! "$EMITTER" "${emit_args[@]}" >"$w/emit.log" 2>&1; then
        bad "$name（$form）：发射失败（见 $w/emit.log）"; FAILED+=("$name/$form:emit"); FAIL=$((FAIL+1)); return
    fi
    if [ ! -f "$w/top.v" ]; then
        bad "$name（$form）：产物缺失（$w/top.v）"; FAILED+=("$name/$form:noprod"); FAIL=$((FAIL+1)); return
    fi

    # ② lint（**门禁的一环** ⇒ ⇒ 退出码 0 + 零 `%Error`）
    if ! "$VERILATOR" --lint-only --top-module top "$w/top.v" >"$w/lint.log" 2>&1; then
        bad "$name（$form）：lint 非零退出（见 $w/lint.log）"
        FAILED+=("$name/$form:lint"); FAIL=$((FAIL+1)); return
    fi
    if grep -q '%Error' "$w/lint.log"; then
        bad "$name（$form）：lint 输出含 %Error（见 $w/lint.log）"
        FAILED+=("$name/$form:lint"); FAIL=$((FAIL+1)); return
    fi

    # ③ 编译（仓内口径 flags · 逐条对齐 scripts/rv32i_rtl_sim_env.sh）
    if ! "$VERILATOR" --binary --timing -CFLAGS -fcoroutines --x-assign unique \
           --top-module tb_rv32i_official --Mdir "$w/obj" \
           "$w/top.v" "$TB_PATH" >"$w/compile.log" 2>&1; then
        bad "$name（$form）：编译失败（见 $w/compile.log）"
        FAILED+=("$name/$form:compile"); FAIL=$((FAIL+1)); return
    fi

    # ④ 运行 + 判定
    if ! timeout 120 "$w/obj/Vtb_rv32i_official" "+tohost_expected=$expect" >"$w/run.log" 2>&1; then
        bad "$name（$form）：仿真失败或超时（见 $w/run.log）"
        FAILED+=("$name/$form:run"); FAIL=$((FAIL+1)); return
    fi
    local line
    line="$(grep -Eo 'RESULT: checks=[0-9]+ fails=[0-9]+' "$w/run.log" | tail -1)"
    if [ -z "$line" ]; then
        bad "$name（$form）：输出里没有 'RESULT: checks=<N> fails=<M>' 行"
        FAILED+=("$name/$form:noresult"); FAIL=$((FAIL+1)); return
    fi
    case "$line" in
        *"fails=0") ok "$name（$form）：$line · $(grep -oE 'TOHOST: .*' "$w/run.log" | tail -1)"; PASS=$((PASS+1)) ;;
        *) bad "$name（$form）：$line · $(grep -oE 'TOHOST: .*' "$w/run.log" | tail -1)"
           FAILED+=("$name/$form:$line"); FAIL=$((FAIL+1)) ;;
    esac
    # ⚠️ 立即删编译产物（每个 ≈150 MB × 40 ⇒ ⇒ 会撑爆磁盘）
    rm -rf "$w/obj"
}

# ── 负向对照（**手写镜像** · 4 个字 · 与核级负向对照逐字节同源）────────────
#   lui x5, 0x80010 ⇒ x5 = 0x8001_0000（= tohost 地址）
#   addi x6, x0, 3  ⇒ x6 = 3
#   sw x6, 0(x5)    ⇒ 把 3 写进 tohost（= FAIL 值形态）
#   jal x0, 0       ⇒ 停机
say "── 负向对照（防恒绿）"
NEG_NAME="tohost-fail-3"
neg_w="$WORK_DIR/$NEG_NAME"
mkdir -p "$neg_w"
if ! "$EMITTER" --emit-rtl --firmware "$NEG_DIR" --out "$neg_w" --emit-hygiene >"$neg_w/emit.log" 2>&1; then
    fail "负向对照：发射失败（见 $neg_w/emit.log）"
fi
if ! "$VERILATOR" --lint-only --top-module top "$neg_w/top.v" >"$neg_w/lint.log" 2>&1 \
   || grep -q '%Error' "$neg_w/lint.log"; then
    fail "负向对照：lint 不干净（见 $neg_w/lint.log）"
fi
if ! "$VERILATOR" --binary --timing -CFLAGS -fcoroutines --x-assign unique \
       --top-module tb_rv32i_official --Mdir "$neg_w/obj" \
       "$neg_w/top.v" "$TB_PATH" >"$neg_w/compile.log" 2>&1; then
    fail "负向对照：编译失败（见 $neg_w/compile.log）"
fi
# ① MUST 如实读到 **3** ⇒ 绿
if ! timeout 120 "$neg_w/obj/Vtb_rv32i_official" "+tohost_expected=3" >"$neg_w/run.log" 2>&1 \
   || ! grep -qE 'RESULT: checks=[0-9]+ fails=0' "$neg_w/run.log"; then
    fail "负向对照：MUST 如实读到 tohost=3（见 $neg_w/run.log）⇒ 判定被吞 ⇒ 门禁红"
fi
ok "负向对照：读到 tohost=3（如实）"
# ② **自证**：同一个镜像若期望 1 ⇒ MUST 红（否则判定恒绿）
if timeout 120 "$neg_w/obj/Vtb_rv32i_official" "+tohost_expected=1" >"$neg_w/run_bad.log" 2>&1 \
   && grep -qE 'RESULT: checks=[0-9]+ fails=0' "$neg_w/run_bad.log"; then
    fail "负向对照自证：期望 1 时 MUST 红 ⇒ 但它绿了 ⇒ 判定恒绿"
fi
ok "负向对照自证：期望 1 时转红 ⇒ 判定不是恒绿"
rm -rf "$neg_w/obj"

# ── 主门禁：官方镜像（扁平）─────────────────────────────────────────────────
say "── 官方镜像 · 扁平形态"
ALL_NAMES=()
while IFS= read -r d; do ALL_NAMES+=("$(basename "$d")"); done < <(find "$IMAGES_DIR" -mindepth 1 -maxdepth 1 -type d | sort)
TOTAL=${#ALL_NAMES[@]}
# ⚠️ **fail-closed**：镜像目录为空 ⇒ ⇒ 空选择会让门禁**只靠 2 个 hier 抽样撑 PASS**
[ "$TOTAL" -gt 0 ] || { fail "官方镜像目录为空（$IMAGES_DIR）⇒ fail-closed"; }
if [ "$SUBSET" -eq 0 ] || [ "$SUBSET" -ge "$TOTAL" ]; then
    SELECTED=("${ALL_NAMES[@]}")
else
    SELECTED=("${ALL_NAMES[@]:0:$SUBSET}")
fi
[ "${#SELECTED[@]}" -gt 0 ] || { fail "--subset $SUBSET 得到空选择 ⇒ fail-closed"; }
say "  镜像总数=$TOTAL · 本次跑=${#SELECTED[@]}"
i=0
for name in "${SELECTED[@]}"; do
    i=$((i+1))
    printf '  [%d/%d] %s ... ' "$i" "${#SELECTED[@]}" "$name"
    run_one "$name" 1 flat
done

# ── 层次化抽样（**与扁平同判据**）────────────────────────────────────────────
say "── 官方镜像 · 层次化形态（抽样 ${#HIER_SUBSAMPLE[@]} 个）"
for name in "${HIER_SUBSAMPLE[@]}"; do
    printf '  %s ... ' "$name"
    run_one "$name" 1 hier
done

# ── 汇总 ────────────────────────────────────────────────────────────────────
say "── 汇总"
say "  PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -ne 0 ]; then
    for f in "${FAILED[@]}"; do say "  · $f"; done
    fail "官方套件在 RTL 产物上未全过（FAIL=$FAIL）"
fi
ok "官方 riscv-tests 套件在 RTL 产物上全过（PASS=$PASS）"
exit 0
