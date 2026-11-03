#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# verify_input_port_required_registry.sh —— **必需端口登记表**护栏
# （D2-A · 2026-11-03 裁决：分级表 · 只覆盖两类已实测风险 · 双向交叉校验）
#
# ⚠️ **为什么要有它**：根 `AGENTS.md §ANTI-PATTERNS` 的「新增端口 MUST 显式驱动」
#   只覆盖**新增**端口 ⇒ ⇒ **存量**端口里「未驱动就静默失效」的那些**没有**护栏
#   ⇒ ⇒ 而 2026-10-28 的 spike 已关闭「通用状态型护栏」路线（运行期无法识别
#   端口身份 ⇒ 诊断不可行动）⇒ ⇒ 剩下的可行路线 = **先分诊，再对「已知必需
#   端口」立护栏**（登记表 + 本护栏）。
#
# ⚠️ **判据（委派给 Python 分析器；判据细节见该文件的 docstring）**：
#   R1 登记 ⊆ 实际端口（且端口 MUST 由**该组件自己的类体**声明 —— 一个头里常有
#      多个类，只按文件匹配会把兄弟类当成登记项 ⇒ 实测误报过）
#   R2 实际端口 ⊆ 登记（**反向判据**：带必需端口的新组件没进表 ⇒ 红）
#   R3 判别性用例存在且**实例化**该组件；缺口行 MUST 仍是缺口（补上覆盖 ⇒ 表过时 ⇒ 红）
#   R4 写入放大类的 **file 级**驱动网（`set_input_value` **或** `io().x <<= `）
#   R5 **单一实现点**：rst_n 护栏的组件清单 MUST 从本表读（脚本内不得有硬编码清单）
#
# ⚠️ **本表拒绝编造指针**：`AxiDma.rst_n` 是**已知缺口**（`tests/test_axi_dma.cpp`
#   只有构造 + 端口存在性，无 `Simulator`、无 `rst_n` 驱动）⇒ ⇒ 表里标缺口并
#   `::notice` 点名，**不**拿那个文件冒充判别性用例（那会让 R3 假绿）。
#
# ⚠️ **消息里 MUST NOT 含反引号** —— shell 双引号里的反引号会被当**命令替换**
#   ⇒ ⇒ 护栏消息被**静默吞掉**（本仓已踩过）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/input_port_registry.py"
REGISTRY="$REPO_ROOT/.rddf/checks/input-port-required-registry.md"
RST_GUARD="$REPO_ROOT/scripts/verify_rst_n_wiring.sh"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0

ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS+1)); }

# ── R0：fail-closed（缺文件 ⇒ MUST NOT 静默通过）──────────────────────────
echo "── R0：必需文件齐备（fail-closed）"
missing=""
[ -f "$ANALYZER" ]  || missing="$missing 判据分析器"
[ -f "$REGISTRY" ]  || missing="$missing 登记表"
[ -f "$RST_GUARD" ] || missing="$missing rst_n 护栏"
if [ -n "$missing" ]; then
    fail "R0 缺少必需文件:${missing}"
    exit 1
fi
ok "R0 必需文件齐备（fail-closed）"

# ── R1–R5：判据（委派给 Python 分析器）────────────────────────────────────
echo "── R1–R5：登记表双向核对"
out="$(python3 "$ANALYZER" check 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
if [ $rc -ne 0 ]; then
    fail "登记表核验未通过（exit=$rc）"
    exit 1
fi
ok "R1–R5 登记表核验通过"

# ── R6：rst_n 护栏在参数化后 MUST 仍然能跑（⇒ 不是「改完就没人跑」）──────
echo "── R6：参数化后的 rst_n 护栏 MUST 仍能通过"
if bash "$RST_GUARD" >/tmp/opencode/rst_n_guard.out 2>&1; then
    ok "R6 rst_n 护栏仍通过（组件清单来自本表 ⇒ 行为未退化）"
else
    fail "R6 rst_n 护栏回归失败（它从本表读清单 ⇒ 清单可能被读坏）"
    sed 's/^/      /' /tmp/opencode/rst_n_guard.out | tail -10
fi

# ─────────────────────────────────────────────────────────────────────────────
# 自检：**原地**投毒（每条 MUST 转红；还原后 MUST 复绿 + 与 HEAD 逐字相同）
# ⚠️ 投毒数 MUST 动态取（list-poisons）⇒ 硬编码会漂移
if [ "${1:-}" = "--self-test" ]; then
    echo "── 自检：原地投毒"
    # 🔴 **必须是原地投毒**（判据读的是真实文件），⇒ ⇒ 所以 MUST 有 trap：
    #    自检失败会 exit、CI 会超时打断、Ctrl-C 更常见 —— 任一中断都会把
    #    真实登记表 / rst_n 护栏**留在投毒态**（`hardcode-back` 那一态甚至
    #    会让下游护栏**静默通过**）⇒ ⇒ AGENTS.md 的「trap 保证异常路径也还原」。
    trap 'python3 "$ANALYZER" restore >/dev/null 2>&1 || true' EXIT INT TERM
    mapfile -t cases < <(python3 "$ANALYZER" list-poisons | cut -f1)
    if [ "${#cases[@]}" -eq 0 ]; then
        fail "自检：投毒清单为空 ⇒ 无法自检（fail-closed）"
        exit 1
    fi
    # ⚠️ 还原的正确判据 = 「回到**投毒前**的状态」，不是「与 HEAD 相同」——
    #    本 change 本身就在改这两个文件 ⇒ 拿 HEAD 比会**假红**。
    PRE="/tmp/opencode/input_port_registry.preselftest"
    rm -rf "$PRE"; mkdir -p "$PRE"
    cp "$REGISTRY" "$PRE/registry.md"
    cp "$RST_GUARD" "$PRE/rst_guard.sh"
    for c in "${cases[@]}"; do
        if ! python3 "$ANALYZER" poison "$c" >/dev/null 2>&1; then
            fail "自检：投毒 ${c} 失败（⇒ 判据可能无判别力）"
            python3 "$ANALYZER" restore >/dev/null 2>&1 || true
            continue
        fi
        if python3 "$ANALYZER" check >/dev/null 2>&1; then
            fail "自检：投毒 ${c} 后核验仍为绿 ⇒ 判据无判别力"
        else
            ok "自检：投毒 ${c} ⇒ 核验转红（判别力成立）"
        fi
        python3 "$ANALYZER" restore >/dev/null 2>&1 || fail "自检：还原 ${c} 失败"
        if python3 "$ANALYZER" check >/dev/null 2>&1; then
            ok "自检：还原 ${c} 后核验复绿"
        else
            fail "自检：还原 ${c} 后核验仍为红 ⇒ 还原写坏了文件"
        fi
        if cmp -s "$REGISTRY" "$PRE/registry.md" && cmp -s "$RST_GUARD" "$PRE/rst_guard.sh"; then
            ok "自检：${c} 还原后与投毒前逐字相同"
        else
            fail "自检：${c} 还原后与投毒前不一致 ⇒ 工作树被污染"
        fi
    done
    rm -rf "$PRE"
    trap - EXIT INT TERM

    # ── 纯函数级自检：钉住「两个输入口径相反」（= 豁免机制曾经恒不生效）──
    # 投毒只能证明判据会**红**；这里写错的形态是「判据永远绿」⇒ ⇒ 必须用
    # 期望为真的断言钉住（该自检在修复前会红 ⇒ 判别力已实测）。
    echo "── 自检：豁免机制的输入口径（纯函数级）"
    if python3 "$ANALYZER" selftest-exemption >/tmp/opencode/ipr_exemption.out 2>&1; then
        ok "自检：豁免机制 7 条口径全部正确（注释里的标记命中 / 注释里的驱动不算）"
    else
        fail "自检：豁免机制输入口径有误（标记写在注释里 ⇒ MUST 用未剥注释的原文匹配）"
        sed 's/^/      /' /tmp/opencode/ipr_exemption.out | tail -6
    fi

    if [ "$FAILS" -ne 0 ]; then
        fail "自检未全部通过（${FAILS} 项）"
        exit 1
    fi
    ok "自检全部通过（${#cases[@]} 种投毒 + 豁免口径 7 条）"
fi

if [ "$FAILS" -ne 0 ]; then
    fail "verify_input_port_required_registry: 有违规（${FAILS} 项）"
    exit 1
fi
echo "${GREEN}✅ verify_input_port_required_registry: PASS${RESET}"
