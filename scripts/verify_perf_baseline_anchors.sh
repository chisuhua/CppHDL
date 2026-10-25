#!/usr/bin/env bash
# scripts/verify_perf_baseline_anchors.sh
#
# 护栏：**perf baseline 的锚点守恒**（`tests/benchmark/perf_baseline.json`）
#
# 背景：`perf_regression`（`tests/benchmark/perf_regression.cpp`）的判据是
#   ```cpp
#   if (base <= 0.0) continue;   // baseline had no measurement
#   ```
# ⇒ **baseline 里 `median_us == 0` 的行会被永久跳过** ⇒ 一次「重生成 baseline」
#   若把某些行从 PASS 采成 SKIPPED（`median_us = 0`）⇒ 那些行**永久退出**
#   比较集合 ⇒ 未来的真实性能回归**永远不会被报出来**。
#
# 真实事故（2026-10-25，change rv32i-canonical-convergence-2026-10）：
#   重生成 baseline 时 TC-09 全部 9 行 SKIPPED（`subprocess_runner.h` 的 180 s
#   超时不足）⇒ 6 个 TC-09 锚点（interpreter/jit × 3 depth）被静默丢弃 ⇒
#   实测：合成 10× 的 TC-09 回归在「降级」版 baseline 下**完全不可见**（rc=0），
#   在「恢复锚点」版下**被抓到**（6 处，rc=1）。
#
# 本护栏（**fail-closed**）：
#   C1 **行集守恒**：新旧 baseline 的 (test_name, params, backend) 集合必须一致
#      （不允许新增/删除行 —— 那会让某些行**悄悄消失**）
#   C2 **锚点守恒**：旧 baseline 里 `status == PASS && median_us > 0` 的行，
#      在新 baseline 里 MUST 仍然 `median_us > 0`
#      （除非该行带 `note_restored` 标记 ⇒ 显式声明「刻意保留旧锚点」）
#   C3 **变松披露**：新 baseline 相对旧 baseline 变慢的行 MUST 被 `note` 披露
#      （门槛 = 该 backend 的 perf 阈值；超过门槛的行数 MUST 在 note 里点名）
#
# 基准来源：`git show <ref>:tests/benchmark/perf_baseline.json`（默认 `HEAD`）
#
# Exit codes: 0 = 全部通过, 1 = 至少一处违规。

set -euo pipefail

# ⚠️ `pwd -P`（**物理**路径）：CMake 记的是物理路径 ⇒ 若仓库在 symlink 下
#    （macOS 的 `/tmp` → `/private/tmp`），逻辑路径会让**每个** `.d` 都被判
#    「非本树构建」⇒ 源② 不可信 ⇒ 护栏全面停摆（方向保守，但闸门形同虚设）。
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$REPO_ROOT"

BASELINE="tests/benchmark/perf_baseline.json"
# ⚠️ 回归阈值 MUST 从**权威源**（`perf_regression.cpp` 的常量）读出来 ——
#    硬编码副本会漂移，且漂移方向可能是**不安全**的（有人放宽 C++ 侧阈值，
#    护栏仍按旧值判 ⇒ 放过未披露的变松）。
REGRESSION_SRC="tests/benchmark/perf_regression.cpp"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

violations=0
note()      { printf "  %s\n" "$1"; }
warning()   { printf "${YELLOW}::warning${NC} %s\n" "$1"; }
violation() { printf "${RED}VIOLATION${NC} %s\n" "$1" >&2; violations=$((violations+1)); }

# ===========================================================================
# --self-test：护栏自身的回归测试（合成基线 + 每种缺陷一个变异）
#
# ⚠️ 为什么必须有：本护栏在开发过程中实测暴露过 3 个**真实**缺陷 ——
#    ① `python3 … || RC=$?` 之后残留的 `RC=$?` 把退出码覆盖回 0
#       ⇒ 违规只打印、CI 却看到 PASS；
#    ② C3 的披露判定曾用「三个子串各自出现」⇒ note 里任何一句提到
#       `TC-08|regs=1000|jit` 的无关文字都让它「已披露」⇒ 删掉披露表的行
#       **不会被发现**；
#    ③ `note_restored` 曾是**降级逃逸口** —— 加一个 JSON 字段就能把 PASS
#       锚点永久摘掉。三者都属「看起来在工作」⇒ 必须有断言钉住。
# ===========================================================================
if [ "${1:-}" = "--self-test" ]; then
    echo "=== verify_perf_baseline_anchors: self-test ==="
    if ! command -v git >/dev/null 2>&1 || ! command -v python3 >/dev/null 2>&1; then
        # ⚠️ **自测路径缺依赖 ⇒ 必须失败**（第八轮审稿 SHOULD_FIX）：
        #    静默 `exit 0` ⇒ 「自测已通过」是假的 ⇒ CI 里的保证是空的。
        printf "${RED}❌ self-test: 缺少 git / python3 ⇒ 无法自测${NC}\n" >&2
        exit 1
    fi
    st_fail=0
    st_check() {
        local desc="$1" expect="$2" actual="$3" log="${4:-}"
        if [ "$expect" = "$actual" ]; then
            printf "  ${GREEN}OK${NC}   %s（exit=%s）\n" "$desc" "$actual"
        else
            printf "  ${RED}FAIL${NC} %s（期望 exit=%s，实际 %s）\n" "$desc" "$expect" "$actual"
            if [ -n "$log" ] && [ -f "$log" ]; then
                sed 's/^/        /' "$log" | head -5 || true
            fi
            st_fail=$((st_fail+1))
        fi
    }
    # 输出级断言：<log> <expected-substring> [desc] —— 失败行可带 fixture 描述
    st_out() {
        local log="$1" want="$2" desc="${3:-}"
        if grep -qF -- "$want" "$log" 2>/dev/null; then
            return 0
        fi
        printf "  ${RED}FAIL${NC} 输出缺 [%s]%s\n" "$want" \
            "$([ -n "$desc" ] && printf ' —— %s' "$desc")"
        st_fail=$((st_fail+1))
    }

    ST_ROOT="$(mktemp -d)"
    trap 'rm -rf "$ST_ROOT"' EXIT INT TERM

    # 合成基线：GOLD 行 100→200（变松 1 次，需披露）；CALM 行 100→110（不变松）；
    # SKIP 行旧基线里是 SKIPPED（不参与锚点守恒）
    mk_fixture() {
        local root="$1"
        rm -rf "$root"; mkdir -p "$root/scripts" "$root/tests/benchmark"
        cp "${BASH_SOURCE[0]}" "$root/scripts/"
        # ⚠️ **权威阈值源 MUST 存在**（第七轮审稿 BLOCKER-3）：护栏不再回落到
        #    脚本内置的默认值 ⇒ 权威源缺失/不可解析 ⇒ fail-closed（违规）⇒
        #    若 fixture 不提供它，全部 fixture 都会红 ⇒ 阈值解析路径**零覆盖**。
        write_thresholds "$root" 1.6
        cat > "$root/tests/benchmark/perf_baseline.json" <<'OLDJ'
{
  "version": 1, "timestamp": "2026-01-01", "git_sha": "old",
  "runs": [
    {"test_name": "TC-GOLD", "params": "x=1", "backend": "jit",         "status": "PASS",    "median_us": 100.0},
    {"test_name": "TC-CALM", "params": "y=2", "backend": "interpreter", "status": "PASS",    "median_us": 100.0},
    {"test_name": "TC-SKIP", "params": "z=3", "backend": "jit",         "status": "SKIPPED", "median_us": 0.0}
  ],
  "note": "GOLD baseline (self-test)."
}
OLDJ
        ( cd "$root" && git init -q . && git add -A \
          && git -c user.email=st@local -c user.name=st commit -qm old ) >/dev/null 2>&1
        write_new "$root"
    }
    # 工作副本 = 新基线（含披露表）
    write_new() {
        cat > "$1/tests/benchmark/perf_baseline.json" <<'NEWJ'
{
  "version": 1, "timestamp": "2026-06-01", "git_sha": "new",
  "runs": [
    {"test_name": "TC-GOLD", "params": "x=1", "backend": "jit",         "status": "PASS",    "median_us": 200.0},
    {"test_name": "TC-CALM", "params": "y=2", "backend": "interpreter", "status": "PASS",    "median_us": 110.0},
    {"test_name": "TC-SKIP", "params": "z=3", "backend": "jit",         "status": "SKIPPED", "median_us": 0.0}
  ],
  "note": "Regenerated. Context sentence that mentions TC-GOLD x=1 jit for unrelated reasons.\nQUANTIFIED LOOSENING DISCLOSURE:\n  TC-GOLD  x=1  jit  old=     100.0  new=     200.0  ratio=2.00x  (backend threshold 1.6x)"
}
NEWJ
    }
    # 权威阈值源（形状与 `tests/benchmark/perf_regression.cpp` 的常量一致）
    write_thresholds() { # <root> <jit阈值>
        cat > "$1/tests/benchmark/perf_regression.cpp" <<THREOF
constexpr std::array<BackendThreshold, 3> kBackendThresholds = {{
    {"interpreter", 2.5},
    {"jit",         $2},
    {"verilator",   1.3},
}};
THREOF
    }
    run_st() { local rc=0; ( cd "$1" && bash scripts/verify_perf_baseline_anchors.sh HEAD ) >"$2" 2>&1 || rc=$?; echo "$rc"; }
    run_st_ref() { local rc=0; ( cd "$1" && bash scripts/verify_perf_baseline_anchors.sh "$3" ) >"$2" 2>&1 || rc=$?; echo "$rc"; }

    F="$ST_ROOT/fx_clean"; mk_fixture "$F"
    st_check "干净基线 MUST 通过（变松行已在披露表点名）" 0 "$(run_st "$F" "$ST_ROOT/l1")" "$ST_ROOT/l1"

    # ① 披露表的行被删，但 note 里**别处**仍提到同样的 token
    #    ⇒ 旧实现（三个子串各自出现）会判「已披露」⇒ 本 fixture 钉住该洞。
    F="$ST_ROOT/fx_nodisc"; mk_fixture "$F"
    python3 - "$F/tests/benchmark/perf_baseline.json" <<'PY'
import json, sys, re
p = sys.argv[1]; d = json.load(open(p))
d['note'] = '\n'.join(l for l in d['note'].split('\n')
                      if not re.match(r'^\s*TC-GOLD\s+x=1\s+jit\s+old=', l))
json.dump(d, open(p, 'w'), indent=2)
PY
    st_check "披露表行被删 MUST 被抓（不得靠别处的 token 蒙混）" 1 "$(run_st "$F" "$ST_ROOT/l2")" "$ST_ROOT/l2"

    # ② 旧基线 PASS 且 median>0 的行降级为 0（perf_regression 会永久跳过）
    F="$ST_ROOT/fx_zero"; mk_fixture "$F"
    python3 - "$F/tests/benchmark/perf_baseline.json" <<'PY'
import json, sys
p = sys.argv[1]; d = json.load(open(p))
for r in d['runs']:
    if r['test_name'] == 'TC-GOLD':
        r['median_us'] = 0.0
json.dump(d, open(p, 'w'), indent=2)
PY
    st_check "锚点降级为 0 MUST 被抓" 1 "$(run_st "$F" "$ST_ROOT/l3")" "$ST_ROOT/l3"

    # ③ 行消失
    F="$ST_ROOT/fx_row"; mk_fixture "$F"
    python3 - "$F/tests/benchmark/perf_baseline.json" <<'PY'
import json, sys
p = sys.argv[1]; d = json.load(open(p))
d['runs'] = [r for r in d['runs'] if r['test_name'] != 'TC-CALM']
json.dump(d, open(p, 'w'), indent=2)
PY
    st_check "行消失 MUST 被抓" 1 "$(run_st "$F" "$ST_ROOT/l4")" "$ST_ROOT/l4"

    # ④ 基准 ref 解析不到 ⇒ 跳过（exit 0）但 MUST 有 ::warning（不得静默）
    F="$ST_ROOT/fx_ref"; mk_fixture "$F"
    rc_ref="$(run_st_ref "$F" "$ST_ROOT/l5" origin/definitely-not-a-branch)"
    st_check "基准 ref 不存在 MUST 跳过（exit=0）" 0 "$rc_ref" "$ST_ROOT/l5"
    st_out "$ST_ROOT/l5" "::warning"

    # ⑤ **`note_restored` 不是「降级为 0」的许可证**（BLOCKER-3 回归）
    #    旧实现里，只要加一个 JSON 字段 `note_restored`，任何 PASS 锚点都能被
    #    永久降级为 0 而护栏报 PASS —— 而那正是这道护栏要防的事。
    F="$ST_ROOT/fx_restore_zero"; mk_fixture "$F"
    python3 - "$F/tests/benchmark/perf_baseline.json" <<'PY'
import json, sys
p = sys.argv[1]; d = json.load(open(p))
for r in d['runs']:
    if r['test_name'] == 'TC-GOLD':
        r['median_us'] = 0.0
        r['status'] = 'SKIPPED'
        r['note_restored'] = '随手写个字符串'
json.dump(d, open(p, 'w'), indent=2)
PY
    st_check "「note_restored」+ 降级为 0 MUST 被抓（不得成为逃逸口）" 1 \
        "$(run_st "$F" "$ST_ROOT/l6")" "$ST_ROOT/l6"

    # ⑥ `note_restored` 的真正含义 =「取值逐字来自旧 baseline」⇒ 取值不同 MUST 被抓
    F="$ST_ROOT/fx_restore_wrong"; mk_fixture "$F"
    python3 - "$F/tests/benchmark/perf_baseline.json" <<'PY'
import json, sys
p = sys.argv[1]; d = json.load(open(p))
for r in d['runs']:
    if r['test_name'] == 'TC-GOLD':
        r['median_us'] = 150.0          # 旧值是 100.0 ⇒ 不是「恢复」，是偷偷改值
        r['note_restored'] = '2026-01-01'
json.dump(d, open(p, 'w'), indent=2)
PY
    st_check "「note_restored」的取值与基准不一致 MUST 被抓" 1 \
        "$(run_st "$F" "$ST_ROOT/l7")" "$ST_ROOT/l7"

    # ⑦ 披露表的**数值**写错 MUST 被抓（披露的价值在于「披露了多大的变松」）
    F="$ST_ROOT/fx_badnum"; mk_fixture "$F"
    python3 - "$F/tests/benchmark/perf_baseline.json" <<'PY'
import json, sys, re
p = sys.argv[1]; d = json.load(open(p))
d['note'] = re.sub(r'ratio=([0-9.]+)x', 'ratio=9.99x', d['note'])
json.dump(d, open(p, 'w'), indent=2)
PY
    st_check "披露表的数值与实际不符 MUST 被抓" 1 \
        "$(run_st "$F" "$ST_ROOT/l8")" "$ST_ROOT/l8"

    # ⑧ **阈值 MUST 来自权威源**（第七轮审稿 BLOCKER-3）
    #    形状：GOLD 100→300（3×）**且** note 里**没有**披露表。
    #      · 权威阈值 = 9.9 ⇒ 3× 不算「更慢」⇒ 无需披露 ⇒ exit=0；
    #      · 若护栏用**内置**默认值（1.6）⇒ 3× > 1.6 ⇒ 需披露 ⇒ 未披露 ⇒ exit=1。
    #    ⇒ 该 fixture 是「用的是权威源而非内置副本」的判别器。
    F="$ST_ROOT/fx_thr_auth"; mk_fixture "$F"
    write_thresholds "$F" 9.9
    python3 - "$F/tests/benchmark/perf_baseline.json" <<'PY'
import json, sys, re
p = sys.argv[1]; d = json.load(open(p))
for r in d['runs']:
    if r['test_name'] == 'TC-GOLD':
        r['median_us'] = 300.0
d['note'] = 'Regenerated. No disclosure table here on purpose.'
json.dump(d, open(p, 'w'), indent=2)
PY
    st_check "阈值 MUST 取自权威源（9.9 ⇒ 3× 变慢不算慢）" 0 \
        "$(run_st "$F" "$ST_ROOT/l9")" "$ST_ROOT/l9"

    # ⑨ 权威阈值收紧到 1.1 ⇒ 3× 算「更慢」且未披露 ⇒ MUST 报违规
    F="$ST_ROOT/fx_thr_tight"; mk_fixture "$F"
    write_thresholds "$F" 1.1
    python3 - "$F/tests/benchmark/perf_baseline.json" <<'PY'
import json, sys
p = sys.argv[1]; d = json.load(open(p))
for r in d['runs']:
    if r['test_name'] == 'TC-GOLD':
        r['median_us'] = 300.0
d['note'] = 'Regenerated. No disclosure table here on purpose.'
json.dump(d, open(p, 'w'), indent=2)
PY
    st_check "权威阈值收紧（1.1）⇒ 未披露的 3× 变慢 MUST 被抓" 1 \
        "$(run_st "$F" "$ST_ROOT/l10")" "$ST_ROOT/l10"

    # ⑩ **权威源不可解析 ⇒ MUST fail-closed**（MUST NOT 回落到内置默认值）
    F="$ST_ROOT/fx_thr_broken"; mk_fixture "$F"
    cat > "$F/tests/benchmark/perf_regression.cpp" <<'BROKENEOF'
// 常量被改名 / 格式改写 ⇒ 护栏 MUST 报违规，而不是静默用旧阈值
constexpr std::array<BackendThreshold, 3> kSomeOtherName = {{
    {"interpreter", 2.5},
}};
BROKENEOF
    st_check "权威阈值源不可解析 MUST fail-closed（不得回落内置默认值）" 1 \
        "$(run_st "$F" "$ST_ROOT/l11")" "$ST_ROOT/l11"

    # ⑪ **baseline 的 JSON 损坏 ⇒ MUST fail-closed**（第九轮审稿 SHOULD_FIX-2）
    #     ⚠️ 断言串 MUST 是**判据专属**的（第十轮审稿 SF-2）：generic 的
    #        `VIOLATION` 会被任何违规满足 ⇒ 无法隔离判别这条 fail-closed 路径。
    F="$ST_ROOT/fx_badjson"; mk_fixture "$F"
    printf '{bad json\n' > "$F/tests/benchmark/perf_baseline.json"
    rc_bad="$(run_st "$F" "$ST_ROOT/l12")"
    st_check "baseline 的 JSON 损坏 MUST fail-closed" 1 "$rc_bad" "$ST_ROOT/l12"
    st_out "$ST_ROOT/l12" "无法解析为 JSON"

    # ⑫ **baseline 缺少合法的 `runs` 数组 ⇒ MUST fail-closed**
    F="$ST_ROOT/fx_noruns"; mk_fixture "$F"
    printf '{"version": 1, "note": "no runs key"}\n' > "$F/tests/benchmark/perf_baseline.json"
    rc_noruns="$(run_st "$F" "$ST_ROOT/l13")"
    st_check "baseline 缺「runs」数组 MUST fail-closed" 1 "$rc_noruns" "$ST_ROOT/l13"
    st_out "$ST_ROOT/l13" "缺少合法的 'runs' 数组" "⑫ 缺 runs（工作副本侧）"

    # ⑬ **基准与工作副本都缺 `runs` ⇒ MUST 仍 fail-closed**
    #     （第十轮审稿 SF-2：只有「新缺/基准不缺」的既有 fixture 钉不住
    #     「两侧都缺」形态 ⇒ `runs` 结构校验被改坏时，C1 行集守恒会替它兜住
    #     而它自身变 match 空转）
    F="$ST_ROOT/fx_noruns_both"; mk_fixture "$F"
    printf '{"version": 1, "note": "no runs key (both)"}\n' > "$F/tests/benchmark/perf_baseline.json"
    git -C "$F" add -A && git -C "$F" -c user.email=st@local -c user.name=st \
        commit -qm "old (also missing runs)" >/dev/null
    printf '{"version": 2, "note": "no runs key (both, new)"}\n' > "$F/tests/benchmark/perf_baseline.json"
    rc_both="$(run_st "$F" "$ST_ROOT/l14")"
    st_check "两侧都缺「runs」数组 MUST fail-closed" 1 "$rc_both" "$ST_ROOT/l14"
    st_out "$ST_ROOT/l14" "缺少合法的 'runs' 数组" "⑬ 两侧都缺 runs"

    # ⑮ **基准侧缺 `runs`（工作副本完好）⇒ MUST 仍 fail-closed**
    #    （⑫⑬ 钉的是工作副本侧；基准侧的同一判据改坏时，`old['runs']`
    #    会 KeyError ⇒ traceback 非人类可读 ⇒ 必须有 fixture）
    #    ⚠️ **构造顺序（第十二轮审稿 BLOCKER：此前两次「已修」都被旧备份
    #    恢复覆盖掉，留下的仍是「两侧都缺」的空转版 —— 教训：变异操作的
    #    备份恢复 MUST 用当轮快照，且 MUST 在修后立即自证）**：
    #      ① HOLD 当前（有 runs 的）工作副本；
    #      ② 把基准提交成「无 runs」；
    #      ③ **恢复工作副本** ⇒ 工作副本有 runs、基准没有 ⇔ 目标形态。
    F="$ST_ROOT/fx_noruns_old"; mk_fixture "$F"
    cp "$F/tests/benchmark/perf_baseline.json" "$ST_ROOT/new_with_runs.json"
    printf '{"version": 1, "note": "no runs key (reference side)"}\n' > "$F/tests/benchmark/perf_baseline.json"
    git -C "$F" add -A && git -C "$F" -c user.email=st@local -c user.name=st \
        commit -qm "old (missing runs)" >/dev/null
    cp "$ST_ROOT/new_with_runs.json" "$F/tests/benchmark/perf_baseline.json"
    rc_old="$(run_st "$F" "$ST_ROOT/l15")"
    st_check "基准侧缺「runs」数组 MUST fail-closed" 1 "$rc_old" "$ST_ROOT/l15"
    st_out "$ST_ROOT/l15" "缺少合法的 'runs' 数组" "⑮ 基准侧缺 runs"

    echo ""
    if [ "$st_fail" -eq 0 ]; then
        echo "${GREEN}✅ self-test: 全部通过${NC}"
        exit 0
    fi
    echo "${RED}❌ self-test: $st_fail 项失败${NC}" >&2
    exit 1
fi

REF="${1:-HEAD}"

if [ "$REF" = "HEAD" ]; then
    # ⚠️ **透明度**：基准 = HEAD ⇒ 本次比较是「工作副本 vs HEAD」⇒ 在
    #    push / schedule / workflow_dispatch 事件下（无 base branch）这就是
    #    **自比** ⇒ 闸门恒过 ⇒ 它只在 PR 事件（基准 = base branch）才真正生效。
    note "基准 ref = HEAD ⇒ 本次为**自比**（push/schedule 事件无 base branch）⇒ 闸门只在 PR 事件生效"
fi

if ! git cat-file -e "$REF:$BASELINE" 2>/dev/null; then
    # ⚠️ 无论哪种情况都 MUST **显式告警** —— 静默跳过会让这道闸门长期形同虚设
    #    （本脚本自己的原则：缺 python3 / ref 解析不到都不得无声通过）。
    warning "基准 $REF:$BASELINE 不存在（首次引入 baseline 的提交？CI 的 git fetch 失败？）⇒ 本次**跳过**锚点守恒检查"
    exit 0
fi

OLD="$(mktemp)"; NEW="$(mktemp)"; trap 'rm -f "$OLD" "$NEW"' EXIT INT TERM
git show "$REF:$BASELINE" > "$OLD"
if [ ! -f "$BASELINE" ]; then
    violation "工作副本里没有 $BASELINE（baseline 被删了？）⇒ 无法做守恒检查"
    exit 1
fi
cp "$BASELINE" "$NEW"

if ! command -v python3 >/dev/null 2>&1; then
    # ⚠️ 缺 python3 ⇒ 闸门**完全无法执行** ⇒ MUST **fail-closed**（与 self-test
    #    路径同一口径 —— 第九轮审稿 NIT-6：两条路径的策略必须一致；
    #    「ref 解析不到」跳过是 spec 明写的（基础设施问题的软着陆），
    #    而「连 python3 都没有」意味着这台机器**根本不是闸门应该跑的环境**。
    echo "${RED}::error${NC} 无 python3 ⇒ **终止**锚点守恒检查（无法解析 JSON）—— 这道闸门在本次运行中**没有生效**" >&2
    exit 1
fi

RC=0
python3 - "$OLD" "$NEW" "$REGRESSION_SRC" <<'PY' || RC=$?
import json, os, re, sys
old_p, new_p, regr_src = sys.argv[1], sys.argv[2], sys.argv[3]
try:
    old = json.load(open(old_p)); new = json.load(open(new_p))
except Exception as e:
    print("VIOLATION baseline 文件无法解析为 JSON：%s" % e, file=sys.stderr)
    sys.exit(1)
for _label, _doc in (("基准", old), ("新", new)):
    if not isinstance(_doc, dict) or 'runs' not in _doc or not isinstance(_doc['runs'], list):
        print("VIOLATION %s baseline 缺少合法的 'runs' 数组（结构非法）" % _label, file=sys.stderr)
        sys.exit(1)

def key(r): return (r['test_name'], r['params'], r.get('backend',''))
om = {key(r): r for r in old['runs']}
nm = {key(r): r for r in new['runs']}
viol = []
notes = []

# C1 行集守恒
only_old = sorted(set(om) - set(nm))
only_new = sorted(set(nm) - set(om))
for k in only_old:
    viol.append("行消失：%s —— 该行将**永久**退出 perf_regression 的比较集合" % (k,))
for k in only_new:
    notes.append("新增行（无旧锚点，不参与比较）：%s" % (k,))

# C2 锚点守恒
lost = []
restored_mismatch = []
for k, o in om.items():
    if o.get('status') != 'PASS' or o.get('median_us', 0) <= 0:
        continue
    n = nm.get(k)
    if n is None:
        continue                      # 已由 C1 报出
    if n.get('median_us', 0) > 0:
        # ⚠️ `note_restored` 的含义是「**取值**是从旧 baseline 恢复的」
        #    ⇒ MUST 断言取值相等。
        #    ⚠️ 它**不是**「允许降级为 0」的许可证 —— 那个逃逸口正是这道护栏
        #    要防的事（旧实现里加一个 JSON 字段就能把 PASS 锚点永久摘掉）。
        if n.get('note_restored'):
            om_v, nm_v = o['median_us'], n['median_us']
            if abs(nm_v - om_v) > max(0.1, om_v * 1e-4):
                restored_mismatch.append((k, om_v, nm_v))
        continue
    lost.append(k)
for k in lost:
    viol.append("锚点丢失：%s —— 旧 baseline 为 PASS/median>0，新 baseline median_us=0"
                "（perf_regression 会 `base<=0 ⇒ continue` 跳过 ⇒ 未来真回归不可见）" % (k,))
for (k, om_v, nm_v) in restored_mismatch:
    viol.append("锚点'恢复'后的取值与基准不一致：%s —— note_restored 声称取值来自旧 baseline，"
                "实际 old=%.4f new=%.4f（恢复必须是**逐字相同**的取值，否则是偷偷放松）"
                % (k, om_v, nm_v))

# C3 变松披露：超过该 backend 阈值的行必须在 note 里被点名
# ⚠️ 阈值 MUST 从**权威源**（`perf_regression.cpp` 的常量）读出来 ——
#    硬编码副本会漂移，且漂移方向可能是**不安全**的：有人放宽 C++ 侧阈值，
#    护栏仍按旧值判 ⇒ 放过未披露的变松。
# ⚠️⚠️ **不得回落到脚本内置的默认值**（第七轮审稿 BLOCKER-3）：回落是
#    **fail-open** —— 权威源被改写（格式变了 / 常量被改名 / 新增 backend）时，
#    护栏会静默用**旧阈值**判 ⇒ 恰好复现本条想消除的漂移。
#    ⇒ 权威源不可读 / 不可解析 ⇒ **fail-closed**（报违规）。
def load_thresholds():
    if not regr_src or not os.path.exists(regr_src):
        return None, '权威源 %s 不存在' % regr_src
    txt = open(regr_src, encoding='utf-8', errors='replace').read()
    m = re.search(r'kBackendThresholds\s*=\s*\{\{(.*?)\}\};', txt, re.S)
    if not m:
        return None, '权威源 %s 里的 kBackendThresholds 未解析到（格式可能已改）' % regr_src
    out = {}
    for name, val in re.findall(r'\{\s*"([^"]+)"\s*,\s*([0-9.]+)\s*\}', m.group(1)):
        out[name] = float(val)
    if not out:
        return None, '权威源 %s 的 kBackendThresholds 解析为空' % regr_src
    return out, '权威源 %s 的 kBackendThresholds' % regr_src
thr, thr_src = load_thresholds()
if thr is None:
    viol.append("无法从权威源读取回归阈值：%s —— 守恒检查 MUST fail-closed"
                "（MUST NOT 回落到脚本内置默认值：副本会漂移，且漂移方向可能是**不安全**的）" % thr_src)
    thr = {}
else:
    notes.append("C3 使用的阈值：%s（来源：%s）"
                 % (", ".join("%s=%.2f" % (k, v) for k, v in sorted(thr.items())), thr_src))
    # ⚠️ 阈值表 MUST 覆盖 baseline 里出现的**所有** backend ⇒ 否则那些行的
    #    「变松」永远不会被披露（`thr.get(backend, 0)` 会让它们静默跳过）
    unknown = sorted({k[2] for k in nm if k[2] and k[2] not in thr})
    if unknown:
        viol.append("权威阈值表未覆盖 baseline 中出现的 backend：%s —— 这些行的「变松」"
                    "永远不会被披露（静默豁免）" % (", ".join(unknown)))
looser = []
for k, o in om.items():
    if o.get('status') != 'PASS' or o.get('median_us', 0) <= 0:
        continue
    n = nm.get(k)
    if n is None or n.get('median_us', 0) <= 0:
        continue
    t = thr.get(k[2], 0)
    if t <= 0:
        continue
    ratio = n['median_us'] / o['median_us']
    if ratio > t:
        looser.append((k, ratio, t, o['median_us'], n['median_us']))
note_text = new.get('note', '') or ''
# 披露判定 MUST 精确到 (test, params, backend) 三元组 **且** 它必须出现在**披露表行**里。
# ⚠️ 为什么不能用「三个子串各自出现」：note 里还有别的句子会提到同名 token
#    （例如对照实验那句 `TC-08|regs=1000|interpreter measured ...`）⇒ 那样判会让
#    该三元组**恒被判为已披露** ⇒ 披露表漏写它也不会被发现（静默放行）。
#    表行的形状是固定的：`<test>  <params>   <backend>   old=...  new=...  ratio=...x`
undisclosed = []
bad_numbers = []
for (k, r, t, o_med, n_med) in looser:
    pat = (r'^\s*' + re.escape(k[0]) + r'\s+' + re.escape(k[1]) + r'\s+'
           + re.escape(k[2]) + r'\s+old=\s*([0-9.]+)\s+new=\s*([0-9.]+)\s+ratio=\s*([0-9.]+)x')
    m = re.search(pat, note_text, re.M)
    if not m:
        undisclosed.append((k, r, t))
        continue
    # ⚠️ 披露的**数值** MUST 与实际一致 —— 披露表的价值在于「披露了多大的变松」，
    #    数字写错仍算「已披露」⇒ 阅 reviewer 时被误导。
    d_old, d_new, d_ratio = (float(m.group(1)), float(m.group(2)), float(m.group(3)))
    errs = []
    if abs(d_old - o_med) > max(0.1, o_med * 1e-4):
        errs.append("old 披露 %g ≠ 实际 %g" % (d_old, o_med))
    if abs(d_new - n_med) > max(0.1, n_med * 1e-4):
        errs.append("new 披露 %g ≠ 实际 %g" % (d_new, n_med))
    if abs(d_ratio - r) > 0.011:
        errs.append("ratio 披露 %g ≠ 实际 %.4f" % (d_ratio, r))
    if errs:
        bad_numbers.append((k, r, t, "; ".join(errs)))
for (k, r, t) in undisclosed:
    viol.append("锚点变松未披露：%s|%s|%s 变慢 %.2fx（阈值 %.1fx）—— 基线的 note MUST 在"
                "「披露表」里以 `<test> <params> <backend> old= new= ratio=` 的行点名它"
                "（否则门禁对该行的检出力被静默削弱）" % (k[0], k[1], k[2], r, t))
for (k, r, t, why) in bad_numbers:
    viol.append("锚点变松的披露**数值**与实际不符：%s|%s|%s（%s）—— 披露表的全部价值在于"
                "「披露了多大的变松」⇒ 数字写错会让 reviewer 被误导" % (k[0], k[1], k[2], why))
if looser and not undisclosed and not bad_numbers:
    notes.append("锚点变慢的行均已在 note 的披露表中逐行点名且数值一致（%d 行）" % len(looser))

for m in notes:
    print("  ::notice " + m)
if viol:
    for v in viol:
        print("VIOLATION " + v, file=sys.stderr)
    sys.exit(1)
print("  OK：行集守恒 + 锚点守恒 + 变松披露 全部通过")
PY

if [ "$RC" -ne 0 ]; then
    echo "${RED}❌ verify_perf_baseline_anchors: 有违规${NC}" >&2
    exit 1
fi
echo "${GREEN}✅ verify_perf_baseline_anchors: PASS${NC}"
