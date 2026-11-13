#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# verify_guard_reachability.sh —— 护栏**可达性**（phase-6 R23-B · ①-a）
#
# ⚠️ **为什么要有它**：本轮 8 条 CI 红里 3 条属「环境 / 权限前提不满足 ⇒ 必然红」
#   ⇒ ⇒ 「前半段红」遮蔽后面**全部** step ⇒ ⇒ 主 job 的护栏链在 5 连红期间**零执行**
#   ⇒ ⇒ ⇒ 「仓里有护栏」与「护栏在跑」是两件事 ⇒ ⇒ 本护栏把「**存在性**」升级到「**有效性**」。
#
# ⚠️ **三条静态判据**（其余三类静态不可判 ⇒ 登记为「不可达类型」）：
#   ① 引用存在（`ci.yml` 引用但文件不存在 ⇒ 必红）
#   ② 可执行位（**git index** mode = 100755；被 `source` 的库文件除外）
#   ③ gate × 环境对齐（**逐调用点**判定 ⇒ 全部调用点都恒 skipped ⇒ 必红）
#
# ⚠️ **排除类型 MUST 派生** —— 护栏从 `ci.yml` **重新派生**并与登记表**逐字比对**
#   ⇒ ⇒ 手写的额外排除 ⇒ ⇒ 判红（否则「排除集合」就是恒绿出口）。
# ⚠️ **棘轮** —— 登记表与引用集合 MUST **双向 1:1** ⇒ ⇒ 引用集合**只许增**。
# ⚠️ **本护栏自身 MUST NOT** 依赖网络 / git 历史（`HEAD~1`）/ verilator
#   ⇒ ⇒ 否则它就是第一个不可达护栏（与 RC-5 / RC-6 同型）。
# ⚠️ 消息里 MUST NOT 含反引号（shell 双引号里的反引号会被当命令替换）。
#
# 用法：
#   scripts/verify_guard_reachability.sh [--self-test] [--render]
# ---------------------------------------------------------------------------

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/guard_reachability.py"
CI_YML="$REPO_ROOT/.github/workflows/ci.yml"
REGISTRY="$REPO_ROOT/.rddf/checks/guard-reachability.md"

SELF_TEST=0
RENDER=0
for arg in "$@"; do
    case "$arg" in
        --self-test) SELF_TEST=1 ;;
        --render) RENDER=1 ;;
        *) echo "用法：scripts/verify_guard_reachability.sh [--self-test] [--render]" >&2; exit 2 ;;
    esac
done

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
FAILS=0
ok()   { printf "  ${GREEN}✅ %s${RESET}\n" "$*"; }
fail() { printf "  ${RED}❌ %s${RESET}\n" "$*"; FAILS=$((FAILS + 1)); }
note() { printf "  ${YELLOW}::notice %s${RESET}\n" "$*"; }

# ── R0：fail-closed ───────────────────────────────────────────────────────
echo "── R0：必需文件齐备（fail-closed）"
r0_bad=0
for f in "$ANALYZER" "$CI_YML" "$REGISTRY"; do
    if [ ! -f "$f" ]; then
        fail "R0 缺少 ${f#"$REPO_ROOT"/} ⇒ fail-closed"
        r0_bad=1
    fi
done
[ "$r0_bad" -eq 0 ] || exit 2
ok "R0 派生器 + CI 配置 + 登记表 齐备"

if [ "$RENDER" -eq 1 ]; then
    python3 "$ANALYZER" --repo "$REPO_ROOT" --ci "$CI_YML" --render
    exit 0
fi

# ── R1：三条静态判据 ──────────────────────────────────────────────────────
echo "── R1：三条静态判据"
set +e
out="$(python3 "$ANALYZER" --repo "$REPO_ROOT" --ci "$CI_YML" --registry "$REGISTRY" 2>&1)"
rc=$?
set -e 2>/dev/null || true
printf '%s\n' "$out" | sed 's/^/  /'
if [ "$rc" -eq 0 ]; then
    ok "R1 引用存在 + 可执行位 + gate 对齐 全绿（且登记表双向 1:1 · 排除类型逐字派生一致）"
elif [ "$rc" -eq 2 ]; then
    fail "R1 派生失败（exit=2）⇒ ⇒ **判据本身坏了**（fail-closed，不得判绿）"
else
    fail "R1 有违规（exit=$rc）⇒ ⇒ 见上方逐条"
fi

# ── R2：自检（6 条具名投毒 ⇒ 必红；1 条反向对照 ⇒ MUST 保持绿）──────────
if [ "$SELF_TEST" -eq 1 ]; then
    echo "── R2：自检（投毒 ⇒ 必红）"
    WORK="$(mktemp -d "${TMPDIR:-/tmp/opencode}/guardreach.XXXXXX" 2>/dev/null)" || {
        echo "::error cannot create a temp dir (mktemp failed)"; exit 2; }
    trap 'rm -rf "$WORK"' EXIT INT TERM

    # 造一个**临时 git fixture 仓**（含 ci.yml 的副本 + 被引用脚本的空壳）
    # ⇒ ⇒ 投毒**只发生在副本上** ⇒ ⇒ 真实仓**零改动**
    build_fixture() {
        local dest="$1"
        rm -rf "$dest"; mkdir -p "$dest/.github/workflows" "$dest/scripts"
        cp "$CI_YML" "$dest/.github/workflows/ci.yml"
        python3 - "$REPO_ROOT" "$dest" "$ANALYZER" <<'PY'
import subprocess, sys, pathlib, shutil, os
repo, dest, analyzer = sys.argv[1], sys.argv[2], sys.argv[3]
sys.path.insert(0, str(pathlib.Path(analyzer).parent))
import guard_reachability as g
wenv, jobs = g.parse_ci(str(pathlib.Path(repo) / ".github/workflows/ci.yml"))
refs, _gates = g.referenced_scripts(wenv, jobs)
d = pathlib.Path(dest)
for p in refs:
    f = d / p
    f.parent.mkdir(parents=True, exist_ok=True)
    f.write_text("#!/usr/bin/env bash\nexit 0\n", encoding="utf-8")
    os.chmod(f, 0o755)
shutil.copy(str(pathlib.Path(repo) / ".rddf/checks/guard-reachability.md"),
            str(d / "registry.md"))
PY
        git -C "$dest" init -q
        git -C "$dest" config user.email t@t; git -C "$dest" config user.name t
        git -C "$dest" add -A >/dev/null 2>&1
        git -C "$dest" commit -qm fixture
    }

    # check <fixture-dir> ⇒ 0 = 绿
    check_fixture() {
        local d="$1"
        python3 "$ANALYZER" --repo "$d" --ci "$d/.github/workflows/ci.yml" \
            --registry "$d/registry.md" >/dev/null 2>&1
    }

    build_fixture "$WORK/fx"
    if ! check_fixture "$WORK/fx"; then
        fail "自检前置：干净 fixture 竟不绿 ⇒ ⇒ fixture 构造有误（后面全部不可信）"
        exit 1
    fi
    ok "自检前置：干净 fixture 绿 ✓"

    # ① 降权一个被引用脚本 ⇒ 必红（判据 ②）
    cp -r "$WORK/fx" "$WORK/p1"
    victim="$(git -C "$WORK/p1" ls-files | grep '^scripts/.*\.sh$' | head -1)"
    git -C "$WORK/p1" update-index --chmod=-x -- "$victim"
    if check_fixture "$WORK/p1"; then
        fail "自检 ①：把被引用脚本降权 ⇒ 判据仍绿 ⇒ ⇒ **判据 ② 无判别力**"
    else
        ok "自检 ①：把被引用脚本降权（mode → 100644）⇒ 判据转红 ✓"
    fi

    # ② 引用但文件不存在 ⇒ 必红（判据 ①）
    cp -r "$WORK/fx" "$WORK/p2"
    rm -f "$WORK/p2/$victim"
    if check_fixture "$WORK/p2"; then
        fail "自检 ②：删掉被引用脚本的文件 ⇒ 判据仍绿 ⇒ ⇒ **判据 ① 无判别力**"
    else
        ok "自检 ②：删掉被引用脚本的文件（引用仍在）⇒ 判据转红 ✓"
    fi

    # ③ gate 与 job env 矛盾 ⇒ 必红（判据 ③）
    mkdir -p "$WORK/p3/.github/workflows" "$WORK/p3/scripts"
    cat > "$WORK/p3/.github/workflows/ci.yml" <<'YEOF'
name: p3
on: [push]
env:
  BUILD_VERILATOR: 'OFF'
jobs:
  j1:
    runs-on: ubuntu-22.04
    env:
      BUILD_VERILATOR: 'OFF'
    steps:
      - name: gated step
        if: env.BUILD_VERILATOR == 'ON'
        shell: bash
        run: scripts/only_one.sh
YEOF
    printf '#!/usr/bin/env bash\nexit 0\n' > "$WORK/p3/scripts/only_one.sh"
    chmod 755 "$WORK/p3/scripts/only_one.sh"
    cat > "$WORK/p3/registry.md" <<'REOF'
| 脚本 | 调用入口 | 排除类型 | 最近一次 executed+绿 |
|---|---|---|---|
| `scripts/only_one.sh` | `j1:gated step` | `none` | — |
REOF
    if check_fixture "$WORK/p3"; then
        fail "自检 ③：gate 与 job env 矛盾（恒 skipped）⇒ 判据仍绿 ⇒ ⇒ **判据 ③ 无判别力**"
    else
        ok "自检 ③：gate 与 job env 矛盾（恒 skipped）⇒ 判据转红 ✓"
    fi

    # ④ **反向对照**：把某脚本改成被 source ⇒ MUST 保持绿（防「为绿而误删能力」）
    cp -r "$WORK/fx" "$WORK/p4"
    mkdir -p "$WORK/p4/scripts/lib"
    printf '#!/usr/bin/env bash\nsource ../%s\n' "$(basename "$victim")" > "$WORK/p4/scripts/lib/srcer.sh"
    git -C "$WORK/p4" add -A >/dev/null 2>&1
    git -C "$WORK/p4" commit -qm srcer >/dev/null 2>&1
    git -C "$WORK/p4" update-index --chmod=-x -- "$victim"
    if check_fixture "$WORK/p4"; then
        ok "自检 ④（反向对照）：被 source 的脚本不要求可执行位 ⇒ 保持绿 ✓"
    else
        fail "自检 ④（反向对照）：被 source 的脚本被误判 ⇒ ⇒ **判据 ② 误伤**"
    fi

    # ⑤ 登记表新增一条未在 ci.yml 出现的排除 ⇒ 必红（排除必须派生）
    cp -r "$WORK/fx" "$WORK/p5"
    printf '\n| `scripts/ghost_guard.sh` | `j1:invented` | `none` | — |\n' >> "$WORK/p5/registry.md"
    if check_fixture "$WORK/p5"; then
        fail "自检 ⑤：登记表新增未派生的排除 ⇒ 判据仍绿 ⇒ ⇒ **排除集合是恒绿出口**"
    else
        ok "自检 ⑤：登记表新增未派生的排除 ⇒ 判据转红 ✓"
    fi

    # ⑥ 引用集合减一项 ⇒ 必红（棘轮只许增）
    cp -r "$WORK/fx" "$WORK/p6"
    python3 - "$WORK/p6/.github/workflows/ci.yml" "$victim" <<'PY'
import sys, pathlib
p = pathlib.Path(sys.argv[1]); victim = sys.argv[2]
lines = [l for l in p.read_text(encoding="utf-8").split("\n") if victim not in l]
p.write_text("\n".join(lines), encoding="utf-8")
PY
    if check_fixture "$WORK/p6"; then
        fail "自检 ⑥：引用集合减一项 ⇒ 判据仍绿 ⇒ ⇒ **棘轮失效**"
    else
        ok "自检 ⑥：引用集合减一项（登记仍在）⇒ 判据转红 ✓"
    fi

    if [ "$FAILS" -eq 0 ]; then
        echo "✅ self-test: 6 条投毒各自判别（① 降权 / ② 文件缺失 / ③ gate 矛盾 / ④ source 反向对照 / ⑤ 未派生排除 / ⑥ 棘轮减项）"
    fi
fi

# ─────────────────────────────────────────────────────────────────────────
if [ "$FAILS" -ne 0 ]; then
    echo "❌ verify_guard_reachability: ${FAILS} violation(s)"
    exit 1
fi
echo "✅ verify_guard_reachability: PASS (R0 fail-closed · R1 引用存在+可执行位+gate 对齐 · 登记表派生一致)"
exit 0
