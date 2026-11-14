#!/usr/bin/env bash
# ────────────────────────────────────────────────────────────────
# verify_port_naming.sh —— **端口命名**的棘轮护栏
# （`port-naming-ratchet-2026-11` **N2** · 基线棘轮）
#
# ⚠️ **它做什么**：扫 **product 侧**（`include/**` + `examples/**`；`tests/**` 排除）
#   的 `__io(...)` 块 ⇒ ⇒ 抽出每一个端口声明 ⇒ ⇒ 判定**合规性** ⇒ ⇒
#   **不合规的端口 MUST 全部登记在棘轮豁免表里**（`.rddf/checks/port-naming-exempt.tsv`）：
#
#   | 形态 | 合规？ |
#   |---|---|
#   | `ch_in<T> ident;`（未具名） | ❌（既有债） |
#   | `ch_in<T> ident{"name"};` 且 `ident == "name"` | ✅ |
#   | `ch_in<T> ident{"name"};` 且 `ident != "name"` | ❌（**P1**：名字串是唯一真源） |
#
#   ⇒ ⇒ **双向**判定：
#     ① **正向**（债增长）：树里有、登记表里没有 ⇒ ⇒ **红**（新增端口 MUST 具名）
#     ② **反向**（登记表烂掉）：登记表里有、树里没有 ⇒ ⇒ **红**（登记表 MUST 同步）
#
# ⚠️ **为什么要有它**：现状产物端口名是 `top_unnamed_input_3` 这类**不可自解释**的名字
#   ⇒ ⇒ 用户写 TB 必须查 port map ⇒ ⇒ 而「**新增**端口必须具名」这条义务此前
#   **没有任何自动化护栏**⇒ ⇒ 「存量 1452 个债」不能一次改完（改名会动全部产物）
#   ⇒ ⇒ 故用**棘轮**：存量登记为豁免 ⇒ ⇒ **债只许减不许增** ⇒ ⇒ 新增端口一旦具名
#   ⇒ ⇒ 登记表逐条缩小 ⇒ ⇒ 债自然归零。
#
# ⚠️ **为什么逐端口、而不是逐文件**（N2 ⑤）：
#   文件级集合比对**堵不住**「豁免文件里新增未命名端口」⇒ ⇒ 条目身份必须是
#   `<文件>\t<__io 块序号>\t<声明文本>`（**不是文件哈希** —— 哈希对任何注释改动都失效）。
#
# ⚠️ **为什么 P1 MUST 额外判「标识符 == 显式名字串」**（oracle 根因 F-C）：
#   框架**无反射** ⇒ ⇒ 运行期唯一存活的是**显式命名串** ⇒ ⇒ manifest 的 `cpp_member`
#   **只能是它** ⇒ ⇒ 「端口名可解析到 C++ 成员名」这条 MUST **必须**由这条静态护栏兜住
#   ⇒ ⇒ 否则那条 AC 是**不可验证的 MUST** ⇒ ⇒ **零新增成本**（扫描面已经拿到声明文本）。
#
# ⚠️ **消息里 MUST NOT 含反引号** —— shell 双引号里的反引号会被当**命令替换**
#   ⇒ ⇒ 护栏消息被**静默吞掉**（本仓已踩过）。
#
# 用法：
#   scripts/verify_port_naming.sh [--self-test]
# ---------------------------------------------------------------------------

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/port_naming.py"
REGISTRY="$REPO_ROOT/.rddf/checks/port-naming-exempt.tsv"

# ⚠️ **计数锚点钉在护栏脚本里**（N2 ④）—— **不**钉在可编辑的登记表里
#    （否则「改登记表」与「改锚点」会一起动 ⇒ ⇒ 锚点失去守门作用）。
EXPECTED_EXEMPT_PORTS=1450

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0
ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS + 1)); }
note() { printf "  ${YELLOW}::notice %s${RESET}\n" "$*"; }

SELF_TEST=0
for arg in "$@"; do
    case "$arg" in
        --self-test) SELF_TEST=1 ;;
        *) echo "用法：scripts/verify_port_naming.sh [--self-test]" >&2; exit 2 ;;
    esac
done

# ── R0：fail-closed ───────────────────────────────────────────────
echo "── R0：必需文件齐备（fail-closed）"
if [ ! -f "$ANALYZER" ]; then
    fail "R0 缺少分析器 scripts/lib/port_naming.py ⇒ fail-closed"
    exit 1
fi
if [ ! -f "$REGISTRY" ]; then
    fail "R0 缺少登记表 .rddf/checks/port-naming-exempt.tsv ⇒ fail-closed"
    exit 1
fi
ok "R0 分析器 + 登记表齐备（fail-closed）"

# ── R1：P1 + 棘轮双向（逐端口）────────────────────────────────────
echo "── R1：端口合规性 + 棘轮双向（逐端口）"
out="$(python3 "$ANALYZER" --root "$REPO_ROOT" --registry "$REGISTRY" \
        --expect-count "$EXPECTED_EXEMPT_PORTS" 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
if [ $rc -ne 0 ]; then
    fail "R1 判定不通过（见上）⇒ ⇒ 新增端口未具名 / 标识符与名字串不一致 / 登记表过期"
else
    ok "R1 通过（逐端口双向一致 + P1 全绿）"
fi

# ── R2：计数锚点（由 R1 的 --expect-count 判；此处单独复述便于定位）──
echo "── R2：计数锚点"
actual="$(grep -vc '^#' "$REGISTRY" 2>/dev/null || echo 0)"
if [ "$actual" = "$EXPECTED_EXEMPT_PORTS" ]; then
    ok "R2 登记表条目数 = ${actual}（== EXPECTED_EXEMPT_PORTS）"
else
    fail "R2 登记表条目数 = ${actual}，锚点 EXPECTED_EXEMPT_PORTS = ${EXPECTED_EXEMPT_PORTS}"
fi

# ────────────────────────────────────────────────────────────────
# 自检：**原地投毒 ⇒ 规则 MUST 转红 ⇒ 还原**
#   （沿用 `scripts/verify_public_api_visibility.sh` 的同款形态）
#   ⚠️ backup / poisoned MUST 是 **GLOBAL**（`local` 在 EXIT trap 里不可见
#      ⇒ `set -u` 报 unbound ⇒ 源码**留在投毒状态**）
# ────────────────────────────────────────────────────────────────
if [ "$SELF_TEST" -eq 1 ]; then
    echo "── 自检：每条规则 MUST 有判别力（投毒 ⇒ 必红 ⇒ 还原）"
    WORK="$(mktemp -d "${TMPDIR:-/tmp/opencode}/port_naming_selftest.XXXXXX" 2>/dev/null)" || {
        echo "::error cannot create temp dir (mktemp failed)"; exit 1;
    }
    backup="$WORK/backup"
    mkdir -p "$backup"
    poisoned=0

    restore() {
        if [ "${poisoned:-0}" = "1" ]; then
            cp "$backup/registry.tsv" "$REGISTRY" 2>/dev/null
            cp "$backup/probe.h" "$PROBE_H" 2>/dev/null
            poisoned=0
        fi
        rm -rf "$WORK"
        return 0
    }
    trap 'restore' EXIT INT TERM

    # 投毒靶：产品侧的一个**小**头文件（改动它不影响任何构建产物）
    PROBE_H="$REPO_ROOT/include/cpu/pipeline/rv32i_pc.h"
    cp "$REGISTRY" "$backup/registry.tsv"
    cp "$PROBE_H" "$backup/probe.h"

    run_guard() { bash "$REPO_ROOT/scripts/verify_port_naming.sh" >/dev/null 2>&1; echo $?; }

    # M1（**正向对照**）：加一个**合规**的具名端口 ⇒ 护栏 MUST **保持绿**
    poisoned=1
    python3 - "$PROBE_H" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
anchor = "__io("
i = s.index(anchor)
j = s.index("\n", i) + 1
s = s[:j] + "        ch_in<ch_bool> selftest_named{\"selftest_named\"};\n" + s[j:]
open(p, 'w', encoding='utf-8').write(s)
PY
    if [ $? -ne 0 ]; then
        fail "自检 M1：投毒未生效（找不到 __io 块？）"; exit 1
    fi
    rc=$(run_guard)
    if [ "$rc" -ne 0 ]; then
        fail "自检 M1（**正向对照**）：加一个合规具名端口后护栏变红（exit=${rc}）⇒ ⇒ 护栏过严"
        exit 1
    fi
    ok "自检 M1（正向对照）：合规具名端口 ⇒ 护栏保持绿 ✓"

    # M2：把那个名字串改成**与标识符不同** ⇒ P1 MUST 红
    python3 - "$PROBE_H" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
old = 'ch_in<ch_bool> selftest_named{"selftest_named"};'
new = 'ch_in<ch_bool> selftest_named{"totally_different"};'
if old not in s:
    sys.exit(3)
open(p, 'w', encoding='utf-8').write(s.replace(old, new, 1))
PY
    if [ $? -eq 3 ]; then
        fail "自检 M2：投毒未生效（找不到 M1 注入的端口？）"; exit 1
    fi
    rc=$(run_guard)
    if [ "$rc" -eq 0 ]; then
        fail "自检 M2：名字串与标识符不一致 ⇒ 护栏仍绿 ⇒ ⇒ **P1 无判别力**"
        exit 1
    fi
    ok "自检 M2：名字串 ≠ 标识符 ⇒ P1 转红 ✓"

    # M3：在**已豁免**文件里**新增**一个未命名端口 ⇒ 正向 MUST 红
    cp "$backup/probe.h" "$PROBE_H"
    python3 - "$PROBE_H" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
anchor = "__io("
i = s.index(anchor)
j = s.index("\n", i) + 1
s = s[:j] + "        ch_in<ch_bool> selftest_unnamed;\n" + s[j:]
open(p, 'w', encoding='utf-8').write(s)
PY
    rc=$(run_guard)
    if [ "$rc" -eq 0 ]; then
        fail "自检 M3：豁免文件里新增未命名端口 ⇒ 护栏仍绿 ⇒ ⇒ **正向无判别力**"
        exit 1
    fi
    ok "自检 M3：豁免文件新增未命名端口 ⇒ 正向转红 ✓"

    # M4：删掉登记表里的一行 ⇒ 反向 MUST 红
    cp "$backup/probe.h" "$PROBE_H"
    python3 - "$REGISTRY" <<'PY'
import sys
p = sys.argv[1]
lines = open(p, encoding='utf-8').read().splitlines()
out = [l for l in lines if not l.startswith("#")]
del out[0]
open(p, 'w', encoding='utf-8').write("\n".join(lines[:4] + out) + "\n")
PY
    rc=$(run_guard)
    if [ "$rc" -eq 0 ]; then
        fail "自检 M4：删登记表一行 ⇒ 护栏仍绿 ⇒ ⇒ **反向无判别力**"
        exit 1
    fi
    ok "自检 M4：登记表少一行 ⇒ 反向转红 ✓"

    # 还原 + 复绿
    restore
    rc=$(run_guard)
    if [ "$rc" -ne 0 ]; then
        fail "自检：还原后护栏仍红（exit=${rc}）⇒ ⇒ 还原不彻底"
        exit 1
    fi
    ok "自检：还原后复绿 ✓"
    echo "✅ self-test: 每条规则都有判别力（M1 正向对照 / M2 P1 / M3 正向 / M4 反向）"
fi

# ────────────────────────────────────────────────────────────────
if [ "$FAILS" -ne 0 ]; then
    echo "❌ verify_port_naming: ${FAILS} violation(s)"
    exit 1
fi
echo "✅ verify_port_naming: PASS (R0 fail-closed · R1 P1+棘轮双向 · R2 计数锚点)"
exit 0
