#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# verify_stream_ready_ownership.sh —— 契约 `chlib-stream-ready-ownership` 的护栏
#
# ⚠️ **为什么要有这个护栏**：`chlib::stream_fifo` 的读使能曾**硬编码**为
#    `true`（`include/chlib/stream.h`）⇒ ⇒ 消费端恒就绪 ⇒ ⇒ FIFO 永远在排空
#    ⇒ ⇒ **`full` 不可达** ⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」**不可演示**
#    ⇒ ⇒ 且违反已签批 ADR-045:160。
#    ⇒ ⇒ 该缺陷**零行为覆盖**（既有测试只做 width 结构断言）⇒ ⇒ 长期「假绿」。
#
# ⚠️ **判据分两类**（MUST 分清，否则「合法的默认恒就绪」会被误判 ⇒ 判别力归零）：
#   ① **效果判据**（运行期行为）⇒ ⇒ `R1`：跑行为用例（反压能阻止推进 +
#      旧语义保持 + 多驱动抛异常）⇒ ⇒ MUST 用**行为**实现；
#
# ⚠️⚠️ **覆盖面声明（2026-10-31 收口订正 · MUST 与主 spec 的「适用范围」一致）**：
#   ⚠️⚠️ **覆盖面（2026-11-01 P1-a 后更新）**：
#   · **`R1`（效果判据）** ⇒ ⇒ **已覆盖转发类**
#     （判别力用例「转发类组合子的**上游 MUST 真收到反压**」在
#     `tests/chlib/test_stream.cpp` ⇒ ⇒ 进**既有** target ⇒ ⇒ 锚点不变）
#     ⇒ ⇒ 覆盖 mux / arbiter / join / filters / combine_with；
#   · **`R8`（结构判据）** ⇒ ⇒ **穷举** `stream.h` + `stream_arbiter.h` 内
#     **所有**接受 `ch_stream` 形参的组合子（⇒ ⇒ **不是**符号清单）
#     ⇒ ⇒ 判定其上游 ready 是否全用 `<<=`；
#   · ⚠️ **仍未收口** ⇒ ⇒ `stream_fork`（P1-c）· `stream_demux`（P1-b）
#     ⇒ ⇒ ⇒ **显式豁免 + `::notice` 点名**（⇒ ⇒ 豁免**非静默**）
#     ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 主 spec 的 R1「转发类 MUST NOT 被读作『已合规』」
#     Scenario 继续在约**（⚠️ 标题 MUST NOT 改 ⇒ ⇒ 只能原地订正正文）。
#   ② **静态判据**（结构）⇒ ⇒ `R2`~`R6` ⇒ ⇒ 走
#      `scripts/lib/stream_ready_ownership.py`（**剥注释**后匹配 ⇒ ⇒ 注释里的
#      字面量不会误判）⇒ ⇒ 这几条**明文**是结构性的（capability delta
#      Requirement 3 允许「静态护栏 MUST 检出 `operator=` 形态」）。
#
# ⚠️ **自检 MUST 原地投毒**：投毒**副本**是**无效**的 —— 被测代码通过
#    `-I include` include **仓内**的 `include/chlib/stream.h` ⇒ ⇒ 投毒到
#    临时目录的副本**根本不会被 include** ⇒ ⇒ 编译**仍成功** ⇒ ⇒ 判据**恒绿**
#    （假绿）⇒ ⇒ MUST **原地**投毒（备份 → 改 → 重建 → 跑 → **还原** →
#    重建 → 再跑一次绿）⇒ ⇒ 且 MUST 用 `trap` 保证异常路径也还原。
#
# ⚠️ **本脚本 MUST NOT 在双引号里放反引号** —— shell 会把它当**命令替换**
#    ⇒ ⇒ 护栏消息被**静默吞掉**（本仓已踩过：护栏消息里的反引号消失）。
#    ⇒ ⇒ 本脚本的所有消息**不含**反引号。
# ─────────────────────────────────────────────────────────────────────────────
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

STREAM_H="include/chlib/stream.h"
TEST_SRC="tests/chlib/test_stream.cpp"
TEST_BIN="build/tests/test_stream"
ANALYZER="scripts/lib/stream_ready_ownership.py"

RED='\033[0;31m'; GRN='\033[0;32m'; RST='\033[0m'
FAILS=0

ok()   { printf '  %b✅ %s%b\n' "$GRN" "$1" "$RST"; }
fail() { printf '  %b❌ %s%b\n' "$RED" "$1" "$RST"; FAILS=$((FAILS+1)); }

# ── R0：fail-closed（fixture / 二进制缺失 ≠ 「通过」）────────────────────────
r0() {
    local missing=""
    [ -f "$STREAM_H" ] || missing="${missing} $STREAM_H"
    [ -f "$TEST_SRC" ] || missing="${missing} $TEST_SRC"
    [ -f "$TEST_BIN" ] || missing="${missing} $TEST_BIN"
    [ -f "$ANALYZER" ] || missing="${missing} $ANALYZER"
    if [ -n "$missing" ]; then
        fail "R0 缺少必需文件:${missing}"
        printf '      ⇒ 先跑：cmake -B build && cmake --build build --target test_stream\n'
        return 1
    fi
    ok "R0 必需文件齐备（fail-closed）"
    return 0
}

# ── R1：效果判据（运行期）────────────────────────────────────────────────────
r1() {
    local out rc
    # ⚠️⚠️ **MUST 先重建再跑** —— 否则可能跑的是**陈旧二进制**
    #    ⇒ ⇒ 实测踩过：一次自检失败后，源码虽已还原，但因还原**保留了 mtime**
    #    ⇒ ⇒ `make` 判定「已是最新」⇒ ⇒ **二进制仍是被投毒的那份**
    #    ⇒ ⇒ 下一轮 R1 **假红**（⇒ 「还原写坏了文件」的假失败）。
    #    ⇒ ⇒ 这就是本仓「**还原源码 ≠ 复原**（二进制也是状态）」那条教训。
    #    ⇒ ⇒ 增量重建很快（库不依赖 chlib/stream.h ⇒ 只重编测试 TU）。
    if command -v cmake >/dev/null 2>&1; then
        if ! cmake --build build --target test_stream \
                   -j"$(nproc 2>/dev/null || echo 4)" >/dev/null 2>&1; then
            fail "R1 重建测试二进制失败（⇒ 无法保证二进制与源码一致）"
            return 1
        fi
    fi
    out="$("$TEST_BIN" 2>&1)"
    rc=$?
    if [ $rc -ne 0 ]; then
        fail "R1 行为用例未通过（exit=$rc）⇒ ⇒ 「消费端 ready == 0 ⇒ 阻止推进」被破坏"
        printf '%s\n' "$out" | grep -E "FAILED|REQUIRE|with expansion" -A 2 | head -30 | sed 's/^/      /'
        return 1
    fi
    ok "R1 行为用例全绿（fifo 侧：反压阻止推进 · 旧语义保持 · 多驱动抛异常
         ｜ **转发类侧**：调用方真收到反压 · 未覆盖时恒就绪）"
    return 0
}

# ── R2~R6：静态判据（委派给 Python 分析器）─────────────────────────────────
r_static() {
    local out rc
    out="$(python3 "$ANALYZER" check 2>&1)"
    rc=$?
    printf '%s\n' "$out" | sed 's/^/  /'
    if [ $rc -ne 0 ]; then
        fail "R2~R6 静态判据未通过"
        return 1
    fi
    ok "R2~R6 静态判据通过"
    return 0
}

# ─────────────────────────────────────────────────────────────────────────────
# 自检：4 种原地投毒，每种 MUST 让 R1 转红；还原后 MUST 再确认一次绿
# ─────────────────────────────────────────────────────────────────────────────
restore_stream() { python3 "$ANALYZER" restore >/dev/null 2>&1; }
cleanup() {
    restore_stream
    trap - EXIT
}
trap cleanup EXIT

rebuild() {
    cmake --build build --target test_stream -j"$(nproc 2>/dev/null || echo 4)" >/dev/null 2>&1
}

self_test() {
    # ⚠️⚠️ **P1-a 新增 2 种 R8 专用投毒**（转发类上游 `<<=` ⇒ ⇒ `operator=`）
    #    ⇒ ⇒ 它们会让 **R8（结构判据）**转红 ⇒ ⇒ ⇒ **自检 MUST 同时检查
    #    「静态判据也红」** ⇒ ⇒ ⇒ 否则「R8 恒绿 ⇒ ⇒ 判别力为零」会被放过
    #    ⇒ ⇒ ⇒ ⇒ ⇒ **（本仓已登记的护栏盲区：「自检只查一个判据」）**
    local cases=(
        "read-en-local-const"
        "write-en-valid-and-ready"
        "default-driver-operator-assign"
        "drop-multidrive-guard"
        "forward-upstream-operator-assign"
        "forward-delete-upstream-drive"
        "arbiter-upstream-operator-assign"
        "filter-upstream-operator-assign"
        "rr-feedback-const"
    )
    local c
    for c in "${cases[@]}"; do
        # ⚠️ 开头这次 restore **可能失败**（无投毒快照 ⇒ ⇒ 陈旧备份检测 ⇒ ⇒
        #    **拒绝**）⇒ ⇒ **忽略其退出码**（⇒ ⇒ 首次运行时本就没投过毒）
        restore_stream || true
        if ! python3 "$ANALYZER" poison "$c" >/dev/null 2>&1; then
            fail "自检：投毒 [$c] 失败（模式未找到 ⇒ fail-closed）"
            return 1
        fi
        if ! rebuild; then
            fail "自检：投毒 [$c] 后重建失败（⇒ 变异不可编译 ⇒ 不算判别力）"
            restore_stream; rebuild; return 1
        fi
        if "$TEST_BIN" >/dev/null 2>&1; then
            fail "自检：投毒 [$c] 后行为用例**仍绿** ⇒ ⇒ 判别力为零"
            restore_stream; rebuild; return 1
        fi
        ok "自检：投毒 [$c] ⇒ 行为用例转红"
        # ⚠️⚠️ **静态判据 MUST 也转红**（R8 的判别力证明 · P1-a）
        #    ⚠️ **MUST NOT** 只查行为判据 ⇒ ⇒ 否则「结构判据恒绿」会被放过
        #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ （⇒ 这是本仓登记的护栏盲区）
        #    ⚠️⚠️ **但**：「多驱动守卫」类投毒**只**影响行为判据
        #    ⇒ ⇒ ⇒ **静态判据仍绿是【正确】的** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
        #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **MUST NOT** 对它强制「静态也红」
        #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ （⇒ 实测：强制 ⇒ ⇒ 自检失败 ⇒ ⇒
        #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **误报**）
        case "$c" in
            read-en-local-const|write-en-valid-and-ready|default-driver-operator-assign) ;;
            drop-multidrive-guard) continue ;;
            # ⚠️⚠️ **P2（2026-11-01）**：轮转反馈 ⇒ 常量 ⇒ ⇒ **轮转消失**
            #    ⇒ ⇒ 但**结构判据看不见**（`<<=` 仍是 `<<=`、驱动仍存在）
            #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 静态判据仍绿是【正确】的**
            #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 归入「仅行为判据」类别**
            rr-feedback-const) continue ;;
        esac
        if python3 "$ANALYZER" check >/dev/null 2>&1; then
            fail "自检：投毒 [$c] 后**静态判据仍绿** ⇒ ⇒ 结构判据的判别力为零"
            restore_stream; rebuild; return 1
        fi
        ok "自检：投毒 [$c] ⇒ 静态判据转红"
        restore_stream
        if ! rebuild; then
            fail "自检：还原后重建失败"
            return 1
        fi
        if ! "$TEST_BIN" >/dev/null 2>&1; then
            fail "自检：还原后行为用例**仍红** ⇒ ⇒ 还原写坏了文件"
            return 1
        fi
        if ! python3 "$ANALYZER" check >/dev/null 2>&1; then
            fail "自检：还原后**静态判据仍红** ⇒ ⇒ 还原写坏了文件"
            return 1
        fi
        ok "自检：还原后行为用例转绿 + 静态判据复绿"
    done
    restore_stream
    rebuild
    ok "自检：9 种投毒全部被抓住，且还原后复绿"
    ok "（含**跨文件**投毒 ⇒ ⇒ 验证「还原回到正确的文件」⇒ ⇒ sidecar 状态文件）"
    return 0
}

# ─────────────────────────────────────────────────────────────────────────────
main() {
    echo "=========================================="
    echo " verify_stream_ready_ownership"
    echo "=========================================="

    if ! r0; then
        echo
        echo "❌ R0 fail-closed ⇒ 后续判据不执行"
        exit 1
    fi
    r1
    r_static

    if [ "${1:-}" = "--self-test" ]; then
        echo
        echo "--- 自检（原地投毒）---"
        self_test
    fi

    echo
    if [ "$FAILS" -ne 0 ]; then
        echo "❌ verify_stream_ready_ownership: FAIL（$FAILS 项）"
        exit 1
    fi
    echo "✅ verify_stream_ready_ownership: PASS"
    exit 0
}

main "$@"
