#!/usr/bin/env bash
# scripts/verify_dual_backend_parity.sh
#
# 双后端 parity 覆盖计数护栏（2026-10-30，planner 判据修订配套）。
#
# 为什么需要：
#   `objective-test-coverage-100pct` 的完成判据由「`ctest -L verilator_e2e` ≥ 100」
#   改为「**双后端 parity 源文件数 ≥ 60**」⇒ ⇒ 该判据 MUST 有**可机械核验**的锚点
#   ⇒ ⇒ 本脚本把「parity 源文件数」与根 `AGENTS.md` 的
#   `DUAL_BACKEND_PARITY_TOTAL: <N>` 行比对。
#
# 它防的是什么（与 `VERILATOR_E2E_TOTAL` 锚点**同型但更严**）：
#   * `VERILATOR_E2E_TOTAL` 防「带标签的条目数」静默缩面 —— 但**标签 ≠ parity**
#     （9 个契约锁定的**单后端**参考实现也带该标签）⇒ ⇒ 它**看不见** parity 缩面；
#   * 本脚本数的**是**真正带双后端 parity 的源文件 ⇒ ⇒ parity 缩面**看得见**。
#
# 判据的权威定义见 `.rddf/roadmap/objectives/objective-test-coverage-100pct.md` §2；
# 分析器与 fixture 自检**共用同一份**（`scripts/lib/dual_backend_parity.py`）⇒ 不会漂移。
#
# ⚠️ **2026-11-26 起本脚本有两条判据**（change `rv32i-dual-backend-obs-vector-guard-2026-11`）：
#   ① **宏调用计数**（广度 = 源文件数 · 深度 = pair 调用数）—— 对两个根 `AGENTS.md` 锚点；
#   ② **观测向量守恒**（`scripts/lib/rv32i_obs_vector.py`）：跨后端**逐拍比对向量**
#      与**核顶层 `ch_out` 端口面** MUST 守恒（`|V|+|R| == |P|` · `R ⊆ P` · `R ∩ V == ∅` ·
#      `V ∪ R == P`），未纳入者 MUST 登记在 `.rddf/checks/rv32i-obs-vector-registry.md`。
#      ⇒ ⇒ ② 补的是 ① 的**结构性盲区**：① 只看「有多少调用」，看不见「比的是什么」。
#
# 用法：
#   scripts/verify_dual_backend_parity.sh            # 主路径（① + ②）
#   scripts/verify_dual_backend_parity.sh --self-test # 两个分析器的自检（12 项 + 基线 + 5 投毒）
#
# 退出码：0 = 通过；1 = 违规（计数漂移 / 观测向量失同步）；2 = fail-closed（缺逻辑载体）。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
AGENTS="$REPO_ROOT/AGENTS.md"
ANALYZER="scripts/lib/dual_backend_parity.py"
# 第二判据（2026-11-26 · change `rv32i-dual-backend-obs-vector-guard-2026-11`）：
# **观测向量 vs 核顶层 ch_out 端口面**的守恒（4 判据 + 缺口登记表双向校验）。
# ⚠️ 为什么要在**本脚本**内加（而不是新脚本）：双锚点只数 **macro 调用**（广度 + 深度），
#    对「向量相对端口面缩水」这类**语义覆盖缩面**结构性失明 —— 实测 S4a 给核加了
#    19 个端口（11 个 ch_out）而向量停在 28，**无任何护栏转红**。
#    ⇒ ⇒ 新脚本要改 ci.yml + 重生成 guard-reachability 台账 + 根 AGENTS.md 护栏索引
#    ⇒ ⇒ 与「覆盖不缩面」这条既有职责不成比例 ⇒ 挂在本脚本内（零基础设施改动）。
OBS_ANALYZER="scripts/lib/rv32i_obs_vector.py"

if [ "${1:-}" = "--self-test" ]; then
    python3 "$ANALYZER" --self-test || exit $?
    [ -f "$OBS_ANALYZER" ] || { echo "::error::缺逻辑载体 $OBS_ANALYZER（fail-closed）"; exit 2; }
    exec python3 "$OBS_ANALYZER" --self-test
fi

if [ ! -f "$AGENTS" ]; then
    echo "::warning::$AGENTS not found"
    exit 0
fi

if ! grep -qE 'DUAL_BACKEND_PARITY_TOTAL: [0-9]+' "$AGENTS"; then
    echo "::error::No DUAL_BACKEND_PARITY_TOTAL anchor in AGENTS.md"
    echo "::error::(add a line 'DUAL_BACKEND_PARITY_TOTAL: <N>' under NOTES §Test counts)"
    exit 1
fi

# 两个锚点：**广度**（源文件数）与**深度**（macro 调用数）
# ⚠️ 为什么两个都要：只钉「文件数」的话，**从有 3 个 dual pair 的文件里删掉
#    1 个 pair** ⇒ 文件仍被计数 ⇒ **护栏看不见**（实测：变异 M1 恒绿）
#    ⇒ ⇒ 深度锚点补上这个盲区。
declare -A ANCHORS=(
    [DUAL_BACKEND_PARITY_TOTAL]=PARITY_COUNT
    [DUAL_BACKEND_PARITY_PAIRS]=PARITY_PAIRS
)

out=$(python3 "$ANALYZER" --root . --build-dir "$BUILD_DIR" --check)
echo "$out"

violations=$(echo "$out" | grep -E '^VIOLATION ' || true)
if [ -n "$violations" ]; then
    echo "::error::dual-backend parity guard found violations:"
    echo "$violations" | sed 's/^/        /'
    exit 1
fi

rc=0
for anchor in DUAL_BACKEND_PARITY_TOTAL DUAL_BACKEND_PARITY_PAIRS; do
    metric=${ANCHORS[$anchor]}
    documented=$(grep -oE "$anchor: [0-9]+" "$AGENTS" | head -1 | grep -oE '[0-9]+$')
    if [ -z "$documented" ]; then
        echo "::warning::No $anchor anchor in AGENTS.md"
        continue
    fi
    actual=$(echo "$out" | grep -oE "^$metric=[0-9]+" | head -1 | grep -oE '[0-9]+$')
    if [ -z "$actual" ]; then
        echo "::error::analyzer produced no parseable $metric"
        rc=1
        continue
    fi
    if [ "$actual" != "$documented" ]; then
        echo "::error::AGENTS.md says $anchor=$documented but the analyzer reports $actual"
        echo "        Update root AGENTS.md NOTES §Test counts to match."
        rc=1
        continue
    fi
    echo "AGENTS.md $anchor ($documented) matches the analyzer ($actual) ✓"
done

# ── 第二判据：跨后端观测向量 vs 核顶层 `ch_out` 端口面 ────────────────────
echo ""
echo "=== 第二判据：跨后端观测向量 vs 核顶层 ch_out 端口面（rv32i-obs-vector）==="
[ -f "$OBS_ANALYZER" ] || { echo "::error::缺逻辑载体 $OBS_ANALYZER（fail-closed）"; exit 2; }
if ! python3 "$OBS_ANALYZER" --root . --check; then
    echo "::error::观测向量与核顶层端口面失同步（C1–C4）⇒ 补齐向量或登记缺口"
    echo "::error::登记表 = .rddf/checks/rv32i-obs-vector-registry.md"
    rc=1
fi

exit $rc
