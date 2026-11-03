#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# verify_input_port_additions.sh —— **新增输入端口**的事件驱动增量检查
# （D1-A · 2026-11-03 裁决：实施 · 只扫生产代码 · 半自动）
#
# ⚠️ **为什么要有它**：根 `AGENTS.md §ANTI-PATTERNS` 登记了义务「**新增**组件
#   输入端口 ⇒ 全部实例化点 MUST 显式驱动」⇒ 未驱动端口读到**未定义值**
#   （实测 `strbe` 读全 1、`rst_n` 读 0）⇒ 依赖默认值会让测试「**碰巧**通过」
#   而跨后端行为可能分叉 ⇒ ⇒ 该义务此前**没有任何自动化护栏** ⇒ ⇒ 而 2026-10-28
#   的 spike 已**关闭**「通用状态型护栏」路线 ⇒ ⇒ 剩下的可行路线就是本条
#   （**事件驱动**：只看「本次新增」的端口 ⇒ 正好对准义务的触发面）。
#
# ⚠️ **半自动 = 只列不判**：本脚本**永远不因「端口没被驱动」而失败** ⇒ ⇒ 退出码只
#   取决于**基础设施**（diff 拿不到 / git 不可用 ⇒ ⇒ **硬失败**，不得静默跳过）⇒ ⇒
#   这是 spike 的直接结论（平规则 ⇒ 必然误报 ⇒ 死结）⇒ ⇒ 报告由**人**判。
#
# ⚠️ **扫描面 = 仅生产代码**（`include/**` + `src/**`）：实测（最近 100 个 commit）
#   生产代码侧新增 `ch_in` 的分布 = **99×0 / 1×1** ⇒ ⇒ 运行成本趋零；测试 DUT
#   的端口由**同一个测试文件**驱动（自包含）⇒ ⇒ 不属于这条义务的风险面。
#
# ⚠️ **消息里 MUST NOT 含反引号** —— shell 双引号里的反引号会被当**命令替换**
#   ⇒ ⇒ 护栏消息被**静默吞掉**（本仓已踩过）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/input_port_additions.py"
# ⚠️ 自检的 fixture 仓库目录：MUST 是**唯一**的（固定路径 ⇒ 两个并发自检会
#    互相踩对方的 fixture ⇒ ⇒ 自检结果随机漂移）⇒ ⇒ 用 mktemp 生成。
#    （自检块里创建，trap 负责清理）
FIXROOT=""

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0

ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS+1)); }
note() { printf "  ${YELLOW}::notice %s${RESET}\n" "$*"; }

# ── R0：fail-closed ───────────────────────────────────────────────────────
echo "── R0：必需文件齐备（fail-closed）"
if [ ! -f "$ANALYZER" ]; then
    fail "R0 缺少分析器 scripts/lib/input_port_additions.py ⇒ fail-closed"
    exit 1
fi
ok "R0 分析器齐备（fail-closed）"

# ── 参数：位置 1 = 基线 ref（可选），`--self-test` 可在任一位置 ───────────────
SELF_TEST=0
BASE=""
for arg in "$@"; do
    case "$arg" in
        --self-test) SELF_TEST=1 ;;
        *) [ -z "$BASE" ] && BASE="$arg" ;;
    esac
done
echo "── R1：基线 ref"
if [ -z "$BASE" ]; then
    if git -C "$REPO_ROOT" rev-parse --verify "origin/main^{commit}" >/dev/null 2>&1; then
        BASE="origin/main"
    elif git -C "$REPO_ROOT" rev-parse --verify "HEAD~1^{commit}" >/dev/null 2>&1; then
        BASE="HEAD~1"
        note "未找到 origin/main ⇒ 退回 HEAD~1（本地用法）"
    else
        fail "R1 找不到可用基线 ref（origin/main 与 HEAD~1 都不存在）⇒ ⇒ **硬失败**"
        exit 1
    fi
fi
if ! git -C "$REPO_ROOT" rev-parse --verify "$BASE^{commit}" >/dev/null 2>&1; then
    fail "R1 基线 ref「${BASE}」不存在 ⇒ ⇒ **硬失败**（CI 的 git fetch 失败 MUST 不得静默跳过）"
    exit 1
fi
ok "R1 基线 ref = ${BASE}"

# ── R2：增量检查（只列不判）──────────────────────────────────────────────
echo "── R2：新增输入端口（只列不判）"
out="$(python3 "$ANALYZER" --base "$BASE" --root "$REPO_ROOT" 2>&1)"
rc=$?
printf '%s\n' "$out" | sed 's/^/  /'
if [ $rc -ne 0 ]; then
    fail "R2 增量检查失败（exit=$rc）⇒ ⇒ 基础设施问题（不是「端口没驱动」）"
    exit 1
fi
ok "R2 增量检查完成（未因「端口没被驱动」而失败 ⇒ 半自动语义成立）"

# ─────────────────────────────────────────────────────────────────────────────
# 自检：**合成 fixture 仓库**（在临时目录 ⇒ 完全不碰真实工作树）
#   每个 case 断言**具体子串** ⇒ 每条判据各自有判别力
if [ "$SELF_TEST" -eq 1 ]; then
    echo "── 自检：合成 fixture（每个 case 断言具体子串）"
    # ⚠️ 异常路径也必须清理（判据失败会 exit / CI 会超时打断）⇒ 否则临时
    #    fixture 仓库会留在 /tmp/opencode 里（**不碰真实工作树**，但仍是垃圾）。
    trap 'rm -rf "$FIXROOT"' EXIT INT TERM
    FIXROOT="$(mktemp -d "${TMPDIR:-/tmp/opencode}/input_port_fixtures.XXXXXX" 2>/dev/null)" || {
        echo "::error cannot create a temp fixture dir (mktemp failed)"
        exit 1
    }

    mk_fixture() {           # $1 = case 名
        local d="$FIXROOT/$1"
        mkdir -p "$d/include" "$d/tests"
        git -C "$d" init -q
        git -C "$d" config user.email t@t; git -C "$d" config user.name t
        cat > "$d/include/demo.h" <<'EOF'
#pragma once
#include "core/io.h"
namespace demo {
class Demo {
public:
    void describe() {
        ch_in<ch_uint<8>> old_port;
    }
};
}  // namespace demo
EOF
        cat > "$d/tests/use.cpp" <<'EOF'
#include "demo.h"
void use_it() {
    ch::ch_device<demo::Demo> dev;
    (void)dev;
}
int main() { return 0; }
EOF
        git -C "$d" add -A >/dev/null; git -C "$d" commit -qm base
    }

    run_case() {            # $1 = case 名 ⇒ 打印分析器输出
        local d="$FIXROOT/$1"
        python3 "$ANALYZER" --base HEAD~1 --head HEAD --root "$d" 2>&1
    }

    expect_contains() {     # $1 = case 名, $2 = 必须出现的子串, $3 = 必须出现的子串（可空）
        local out rc
        out="$(run_case "$1")"; rc=$?
        if [ $rc -ne 0 ]; then
            fail "自检 ${1}：分析器退出码 ${rc}（应为 0 ⇒ 半自动不判违规）"
            return
        fi
        if ! printf '%s' "$out" | grep -qF -- "$2"; then
            fail "自检 ${1}：输出里找不到「${2}」⇒ 判据无判别力"
            printf '%s\n' "$out" | sed 's/^/      /'
        else
            ok "自检 ${1}：输出含「${2}」"
        fi
        if [ -n "${3:-}" ] && ! printf '%s' "$out" | grep -qF -- "$3"; then
            fail "自检 ${1}：输出里找不到「${3}」⇒ 判据无判别力"
        fi
    }

    # ① 新增端口 + 显式驱动 ⇒ 必须看到「驱动（TB）」
    mk_fixture wired
    python3 - "$FIXROOT/wired" <<'PY'
import pathlib, sys
d = pathlib.Path(sys.argv[1])
h = d / "include/demo.h"
h.write_text(h.read_text().replace(
    "        ch_in<ch_uint<8>> old_port;",
    "        ch_in<ch_uint<8>> old_port;\n        ch_in<ch_uint<8>> new_port;"))
u = d / "tests/use.cpp"
u.write_text(u.read_text().replace(
    "    (void)dev;",
    "    (void)dev;\n    sim.set_input_value(dev.instance().io().new_port, 1);"))
PY
    git -C "$FIXROOT/wired" add -A >/dev/null; git -C "$FIXROOT/wired" commit -qm head
    expect_contains wired "端口 new_port" "驱动（TB）"

    # ② 新增端口 + **无驱动** ⇒ 必须被列出，但**不得**判违规（退出码 0）
    mk_fixture unwired
    python3 - "$FIXROOT/unwired" <<'PY'
import pathlib, sys
d = pathlib.Path(sys.argv[1])
h = d / "include/demo.h"
h.write_text(h.read_text().replace(
    "        ch_in<ch_uint<8>> old_port;",
    "        ch_in<ch_uint<8>> old_port;\n        ch_in<ch_uint<8>> new_port;"))
u = d / "tests/use.cpp"
u.write_text(u.read_text().replace(
    "    (void)dev;",
    "    (void)dev;\n    (void)dev.instance().io().new_port;"))
PY
    git -C "$FIXROOT/unwired" add -A >/dev/null; git -C "$FIXROOT/unwired" commit -qm head
    expect_contains unwired "端口 new_port" "未见到驱动"

    # ③ 无新增端口 ⇒ 必须报「未发现新增」
    mk_fixture none
    git -C "$FIXROOT/none" commit -q --allow-empty -m head
    expect_contains none "未发现新增的 ch_in 端口"

    # ④ 驱动在 helper 内（端口名不出现��� set_input_value 里）⇒ 措辞必须**保守**
    mk_fixture helper
    python3 - "$FIXROOT/helper" <<'PY'
import pathlib, sys
d = pathlib.Path(sys.argv[1])
h = d / "include/demo.h"
h.write_text(h.read_text().replace(
    "        ch_in<ch_uint<8>> old_port;",
    "        ch_in<ch_uint<8>> old_port;\n        ch_in<ch_uint<8>> new_port;"))
u = d / "tests/use.cpp"
u.write_text(u.read_text().replace(
    "    (void)dev;",
    "    (void)dev;\n    drive(dev.instance().io().new_port);")
    .replace("int main() { return 0; }",
             "template <class P> void drive(P p) { sim.set_input_value(p, 1); }\n"
             "int main() { return 0; }"))
PY
    git -C "$FIXROOT/helper" add -A >/dev/null; git -C "$FIXROOT/helper" commit -qm head
    expect_contains helper "未见到驱动" "可能在 helper 内"

    # ⑤ 只有 `io().X = value`（落在 ch_in 上 ⇒ **静默失效**）⇒ 必须**点名**
    mk_fixture silent
    python3 - "$FIXROOT/silent" <<'PY'
import pathlib, sys
d = pathlib.Path(sys.argv[1])
h = d / "include/demo.h"
h.write_text(h.read_text().replace(
    "        ch_in<ch_uint<8>> old_port;",
    "        ch_in<ch_uint<8>> old_port;\n        ch_in<ch_uint<8>> new_port;"))
u = d / "tests/use.cpp"
u.write_text(u.read_text().replace(
    "    (void)dev;",
    "    (void)dev;\n    dev.instance().io().new_port = 0;"))
PY
    git -C "$FIXROOT/silent" add -A >/dev/null; git -C "$FIXROOT/silent" commit -qm head
    expect_contains silent "疑似静默失效"

    # ⑥ 批量重构（12 个新端口）⇒ 全部列出且**不失败**
    mk_fixture bulk
    python3 - "$FIXROOT/bulk" <<'PY'
import pathlib, sys
d = pathlib.Path(sys.argv[1])
h = d / "include/demo.h"
h.write_text(h.read_text().replace(
    "        ch_in<ch_uint<8>> old_port;",
    "        ch_in<ch_uint<8>> old_port;\n" +
    "\n".join(f"        ch_in<ch_uint<8>> p{i};" for i in range(12))))
PY
    git -C "$FIXROOT/bulk" add -A >/dev/null; git -C "$FIXROOT/bulk" commit -qm head
    out="$(run_case bulk)"; rc=$?
    if [ $rc -ne 0 ]; then
        fail "自检 bulk：分析器退出码 ${rc}（批量 ⇒ 也必须不判违规）"
    else
        n=$(printf '%s' "$out" | grep -c "端口 p")
        if [ "$n" -eq 12 ]; then
            ok "自检 bulk：12 个新端口全部列出且未判违规"
        else
            fail "自检 bulk：只列出 ${n} 个（期望 12）"
        fi
    fi

    rm -rf "$FIXROOT"
    trap - EXIT INT TERM
    if [ "$FAILS" -ne 0 ]; then
        fail "自检未全部通过（${FAILS} 项）"
        exit 1
    fi
    ok "自检全部通过（6 个 fixture）"
fi

if [ "$FAILS" -ne 0 ]; then
    fail "verify_input_port_additions: 有违规"
    exit 1
fi
echo "${GREEN}✅ verify_input_port_additions: PASS${RESET}"
