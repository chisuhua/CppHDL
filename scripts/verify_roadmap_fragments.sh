#!/usr/bin/env bash
# scripts/verify_roadmap_fragments.sh
#
# 护栏：**roadmap 簿记的一致性**（根 `AGENTS.md` 的 AUTO 块 ↔ `.rddf/roadmap/**`）
#
# 背景：根 `AGENTS.md` 的 `<!-- AUTO: feature fragments start/end -->` 块与
# `.rddf/roadmap/features/*.md` 的 front-matter 是**两份**真相 ⇒ 漂移**无声**。
# ⚠️ 漂移的**根因**不是「脚本缺失」—— 工具链
# `skills/roadmap/scripts/roadmap_update_agent_md.sh`（在 **rdd-workflow** 仓内）
# **存在且可用**；缺的是 **CLI dispatch**（`rddf roadmap` 只暴露
# `init / status / edit / validate / advance`，没有 `--update-agent-md`）
# ⇒ 工具**不可达** ⇒ AUTO 块只能**手工**维护 ⇒ 手工 = 会漂。
#
# ⚠️ **vendored 渲染规则的出处与再验证条件**：
#   本脚本内嵌的渲染规则 **vendored 自**
#   `rdd-workflow@96af1629c8994fc079e100817c8540172c21226e`
#   的 `_lib/roadmap_state.py::update_agent_md`（+ `_parse_fragment_file` /
#   `load_fragments`）。
#   ⇒ **再验证触发条件**：① `rdd-workflow` 升级 ⇒ MUST 重跑 oracle
#     （命令见 `openspec/changes/roadmap-fragments-consistency-guard-2026-10/design.md` §3）；
#     ② 本脚本的渲染规则有任何改动 ⇒ MUST 重跑 oracle。
#   ⇒ ⚠️ **C1 报「AUTO 块不一致」时的正确动作**：**同步本脚本的 vendored 规则**
#     （若工具链确实变了）或修 front-matter ⇒ **不要**手工改 `AGENTS.md` 的 AUTO 块
#     —— 那正是本护栏要治的根因。
#
# 本护栏（**fail-closed**，环境性缺失除外）：
#   C1 **AUTO 块守恒**：按工具链生成器的**同一套渲染规则**从 front-matter
#      渲染出期望的 AUTO 块 ⇒ 与已提交块**逐字比对** ⇒ 不一致即失败。
#      ⚠️ 为什么是「**渲染 + 比对**」而不是「双向解析 + 字段比对」：前者只需
#      **一套**解析器（front-matter）⇒ 误报面更小；后者要额外解析已提交块的
#      Markdown 表格 ⇒ 多一类边界 ⇒ 多一类误报。
#   C2 **phase 标签的完成度一致性**：同一 feature 文件的三个来源
#      （`### phase-N` 段的 bullet / `## 验收标准` 的 checkbox / 正文里的
#      Markdown 表格行）若**都**提到**同一个 `Phase N` 标签** ⇒
#      三者给出的完成度 MUST 一致。
#   C3 **change 名的完成度一致性**：同上，但键是**已知 change 名**
#      （注册表 = `openspec/changes/**` 的目录名，去掉归档日期前缀与日期后缀）。
#
# ⚠️ **本护栏判不了的（如实登记）**：**叙事性**陈旧 —— 例如某条 bullet 仍写着
#   「Batch 2 需要 clk-spike」而该说法**已被读码推翻**：这类漂移没有可机械判定的
#   对应物（符号一致、只是内容过时）⇒ **仍靠人工 + 代码评审**兜住。
#
# 用法：
#   scripts/verify_roadmap_fragments.sh [--root DIR]
#   scripts/verify_roadmap_fragments.sh --self-test
#
# Exit codes: 0 = 全部通过, 1 = 至少一处违规, 2 = 用法错误。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
CHECK_ROOT="$REPO_ROOT"

SELF_TEST=0
while [ $# -gt 0 ]; do
    case "$1" in
        --root)
            if [ $# -lt 2 ]; then
                echo "❌ --root 需要一个目录参数" >&2
                exit 2
            fi
            CHECK_ROOT="$2"; shift 2 ;;
        --self-test) SELF_TEST=1; shift ;;
        -h|--help)
            echo "Usage: $0 [--root DIR] [--self-test]"
            exit 0 ;;
        *) echo "❌ unexpected arg: $1" >&2; exit 2 ;;
    esac
done

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

# ===========================================================================
# --self-test：护栏自身的回归测试（合成仓库 + 每种缺陷一个变异）
#
# ⚠️ 为什么必须有：本仓的护栏反复登记过「看起来在工作」的失败模式
#   （`verify_perf_baseline_anchors.sh` 在开发中暴露过 3 个真缺陷；
#   `verify_example_assertion_quality.sh` 的自研分析器「不可能被完全闭合」）
#   ⇒ 每条判据 MUST 有 fixture 钉住它**有判别力**。
#
# ⚠️ **可移植性**：fixture 的变异**一律**用 `python3 - "$file" <<'PY'`
#   （**不用** `sed -i` —— 那是 GNU-only，BSD sed 要求 `-i ''` ⇒ macOS 腿上会
#   静默不生效 ⇒ fixture 变异没发生 ⇒ self-test 假红）。
#   仓内先例：`scripts/verify_no_duplicate_rv32i_headers.sh` 的 `sed_inplace()`。
# ===========================================================================
if [ "$SELF_TEST" -eq 1 ]; then
    if [ "$CHECK_ROOT" != "$REPO_ROOT" ]; then
        echo "${YELLOW}::notice${NC} --self-test 忽略 --root（自测在自建的合成仓库里跑）"
    fi
    echo "=== verify_roadmap_fragments: self-test ==="
    if ! command -v python3 >/dev/null 2>&1; then
        printf "${RED}❌ self-test: 缺少 python3 ⇒ 无法自测${NC}\n" >&2
        exit 1
    fi
    ST_ROOT="$(mktemp -d)"
    trap 'rm -rf "$ST_ROOT"' EXIT INT TERM

    st_fail=0
    st_check() {
        local desc="$1" expect="$2" actual="$3" log="${4:-}"
        if [ "$expect" = "$actual" ]; then
            printf "  ${GREEN}OK${NC}   %s（exit=%s）\n" "$desc" "$actual"
        else
            printf "  ${RED}FAIL${NC} %s（期望 exit=%s，实际 %s）\n" "$desc" "$expect" "$actual"
            if [ -n "$log" ] && [ -f "$log" ]; then
                sed 's/^/        /' "$log" | head -8 || true
            fi
            st_fail=$((st_fail+1))
        fi
    }
    st_out() {
        local log="$1" want="$2" desc="${3:-}"
        if grep -qF -- "$want" "$log" 2>/dev/null; then
            return 0
        fi
        printf "  ${RED}FAIL${NC} 输出缺 [%s]%s\n" "$want" \
            "$([ -n "$desc" ] && printf ' —— %s' "$desc")"
        st_fail=$((st_fail+1))
    }

    # ---- 合成仓库 ----
    mk_fixture() {
        local root="$1"
        rm -rf "$root"; mkdir -p "$root/scripts" "$root/.rddf/roadmap/features" \
                                  "$root/.rddf/roadmap/phases" "$root/openspec/changes"
        cp "${BASH_SOURCE[0]}" "$root/scripts/"
        cat > "$root/.rddf/roadmap/features/feat-alpha.md" <<'FEOF'
---
id: feat-alpha
kind: feature
status: active
phase_refs: [phase-4, phase-5]
主题: alpha 主题
---

### phase-4
- ✅ Phase 1: 已完成

### phase-5
- ✅ Phase 2: 已完成
- ✅ Phase 3: 已完成
- ⏳ Phase 5: 未完成

## 验收标准
- [x] Phase 1 ship（commit abc1234）
- [x] Phase 2 ship（commit def5678）
- [ ] `ctest -L verilator_e2e` ≥ 100 test（Phase 3 续 + Phase 5）
FEOF
        cat > "$root/.rddf/roadmap/phases/phase-1.md" <<'PEOF'
---
id: phase-1
kind: phase
status: active
---
body
PEOF
        write_agents "$root"
    }
    # AGENTS.md 的 AUTO 块 = 与合成 feature 一致的渲染结果
    write_agents() {
        cat > "$1/AGENTS.md" <<'AEOF'
# Fixture

<!-- AUTO: feature fragments start -->

| id | status | phase_refs | theme |
|---|---|---|---|
| `feat-alpha` | active | phase-4, phase-5 | alpha 主题 |

_auto-generated by `rddf roadmap --update-agent-md` — DO NOT hand-edit this block_
<!-- AUTO: feature fragments end -->

## Other section
AEOF
    }
    run_st() { local rc=0; bash "$1/scripts/verify_roadmap_fragments.sh" --root "$1" >"$2" 2>&1 || rc=$?; echo "$rc"; }

    # ① 一致 ⇒ 通过
    #    ⚠️ 本 fixture **自带**一个误报陷阱：验收标准里有一行
    #    「- [ ] `ctest -L verilator_e2e` ≥ 100 test（Phase 3 续 + Phase 5）」
    #    —— 它**行中**提到 Phase 3，而 phase bullet 说 Phase 3 是 ✅（完成）
    #    ⇒ 若护栏按「出现即比对」就会**误报** ⇒ ① 通过 ⇒ 证明**行中提及不比对**
    F="$ST_ROOT/fx_ok"; mk_fixture "$F"
    st_check "一致的仓库 MUST 通过（含「行中提及」误报陷阱）" 0 "$(run_st "$F" "$ST_ROOT/l1")" "$ST_ROOT/l1"

    # ② AUTO 块的 status 与 front-matter 不符
    F="$ST_ROOT/fx_status"; mk_fixture "$F"
    python3 - "$F/AGENTS.md" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = s.replace("| `feat-alpha` | active |", "| `feat-alpha` | completed |")
open(p, 'w', encoding='utf-8').write(s)
PYEOF
    st_check "AUTO 块 status 不符 MUST 被抓" 1 "$(run_st "$F" "$ST_ROOT/l2")" "$ST_ROOT/l2"
    st_out "$ST_ROOT/l2" "AUTO feature 块与"

    # ③ AUTO 块少一个 feature（front-matter 多了 ⇒ 渲染应当不同）
    F="$ST_ROOT/fx_missing"; mk_fixture "$F"
    cat > "$F/.rddf/roadmap/features/feat-beta.md" <<'FEOF'
---
id: feat-beta
kind: feature
status: active
phase_refs: [phase-5]
主题: beta 主题
---
body
FEOF
    st_check "AUTO 块缺一行 MUST 被抓" 1 "$(run_st "$F" "$ST_ROOT/l3")" "$ST_ROOT/l3"

    # ④ front-matter 重复键 ⇒ **fail-closed**（工具自身会抛 ValueError）
    F="$ST_ROOT/fx_dupkey"; mk_fixture "$F"
    python3 - "$F/.rddf/roadmap/features/feat-alpha.md" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = s.replace("status: active", "status: active\nstatus: completed")
open(p, 'w', encoding='utf-8').write(s)
PYEOF
    st_check "front-matter 重复键 MUST fail-closed" 1 "$(run_st "$F" "$ST_ROOT/l4")" "$ST_ROOT/l4"
    st_out "$ST_ROOT/l4" "front-matter"

    # ⑤ AUTO 哨兵缺失 ⇒ **fail-closed**
    F="$ST_ROOT/fx_nosentinel"; mk_fixture "$F"
    python3 - "$F/AGENTS.md" <<'PYEOF'
import sys
p = sys.argv[1]
lines = open(p, encoding='utf-8').read().splitlines(True)
out = [l for l in lines
       if "<!-- AUTO: feature fragments start -->" not in l
       and "<!-- AUTO: feature fragments end -->" not in l]
open(p, 'w', encoding='utf-8').write("".join(out))
PYEOF
    st_check "AUTO 哨兵缺失 MUST fail-closed" 1 "$(run_st "$F" "$ST_ROOT/l5")" "$ST_ROOT/l5"
    st_out "$ST_ROOT/l5" "哨兵"

    # ⑥ phase 标签的符号冲突（🟡 Phase 2 vs [x] Phase 2）⇒ 这是**实测**漂移形态
    F="$ST_ROOT/fx_phase"; mk_fixture "$F"
    python3 - "$F/.rddf/roadmap/features/feat-alpha.md" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = s.replace("- ✅ Phase 2: 已完成", "- 🟡 Phase 2: 未完成")
open(p, 'w', encoding='utf-8').write(s)
PYEOF
    st_check "phase 标签的符号冲突 MUST 被抓" 1 "$(run_st "$F" "$ST_ROOT/l6")" "$ST_ROOT/l6"
    st_out "$ST_ROOT/l6" "label:Phase-2"

    # ⑦ **行首**的 Phase 标签**必须**被比对（与 ① 的「行中提及」相对）
    F="$ST_ROOT/fx_linestart"; mk_fixture "$F"
    python3 - "$F/.rddf/roadmap/features/feat-alpha.md" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = s.replace("- [ ] `ctest -L verilator_e2e` ≥ 100 test（Phase 3 续 + Phase 5）",
              "- [ ] `ctest -L verilator_e2e` ≥ 100 test（Phase 3 续 + Phase 5）\n"
              "- [ ] Phase 3 partial ship（archive 2026-10-20）")
open(p, 'w', encoding='utf-8').write(s)
PYEOF
    st_check "行首的 Phase 标签被比对 ⇒ 冲突 MUST 被抓" 1 "$(run_st "$F" "$ST_ROOT/l7")" "$ST_ROOT/l7"

    # ⑧ 同一 phase 在**一侧**出现多个不同符号 ⇒ **跳过**（不判违规）
    F="$ST_ROOT/fx_ambiguous"; mk_fixture "$F"
    python3 - "$F/.rddf/roadmap/features/feat-alpha.md" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = s.replace("- ✅ Phase 1: 已完成",
              "- ✅ Phase 1: 已完成\n- ⏳ Phase 1: 还有后续")
open(p, 'w', encoding='utf-8').write(s)
PYEOF
    st_check "同一 phase 一侧多符号 ⇒ MUST 跳过（不得误报）" 0 "$(run_st "$F" "$ST_ROOT/l8")" "$ST_ROOT/l8"

    # ⑨ change 名的符号冲突（同一已知 change 名，两侧符号不同）
    #    ⚠️ 名字 MUST 用**真实语料形态**：仓内 feature 文件引用的是
    #    「归档目录名去掉日期前缀与日期后缀」的**裸名**
    #    （如 `dual-backend-rv32i-integration`），而不是目录名本身 ⇒
    #    注册表若只收目录名 ⇒ 该判据在真实语料上**零命中**（= 永不触发）。
    F="$ST_ROOT/fx_change"; mk_fixture "$F"
    mkdir -p "$F/openspec/changes/archive/2026-10-20-dual-backend-demo-2026-10"
    python3 - "$F/.rddf/roadmap/features/feat-alpha.md" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = s.replace("- ✅ Phase 2: 已完成",
              "- ✅ `dual-backend-demo` 已归档（2026-10-20）")
s = s.replace("- [x] Phase 2 ship（commit def5678）",
              "- [ ] `dual-backend-demo` change archived")
open(p, 'w', encoding='utf-8').write(s)
PYEOF
    st_check "change 名的符号冲突 MUST 被抓" 1 "$(run_st "$F" "$ST_ROOT/l9")" "$ST_ROOT/l9"
    st_out "$ST_ROOT/l9" "change:dual-backend-demo"

    # ⑩ **叙事性**漂移（符号一致、内容过时）⇒ 本护栏**判不了** ⇒ 不得误报
    F="$ST_ROOT/fx_narrative"; mk_fixture "$F"
    python3 - "$F/.rddf/roadmap/features/feat-alpha.md" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = s.replace("- ✅ Phase 1: 已完成",
              "- ✅ Phase 1: 已完成（仍写着「Batch 2 需要 clk-spike」—— 已被推翻）")
open(p, 'w', encoding='utf-8').write(s)
PYEOF
    st_check "叙事性陈旧 MUST **不**误报（判不了 ⇒ 不判）" 0 "$(run_st "$F" "$ST_ROOT/l10")" "$ST_ROOT/l10"

    # ⑪ **表格行**与 bullet 的符号冲突 ⇒ MUST 被抓（表格是第三个比对面）
    F="$ST_ROOT/fx_table"; mk_fixture "$F"
    python3 - "$F/.rddf/roadmap/features/feat-alpha.md" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = s.replace("## 验收标准", """| 子项 | 状态 |
|---|---|
| Phase 2: cache 迁移 | 🟡 未完成 |

## 验收标准""")
open(p, 'w', encoding='utf-8').write(s)
PYEOF
    st_check "表格行的符号冲突 MUST 被抓" 1 "$(run_st "$F" "$ST_ROOT/l11")" "$ST_ROOT/l11"
    st_out "$ST_ROOT/l11" "label:Phase-2"

    # ⑫ 表格里**一个单元格含多个符号** ⇒ 歧义 ⇒ MUST 跳过（不得误报）
    F="$ST_ROOT/fx_table_ambig"; mk_fixture "$F"
    python3 - "$F/.rddf/roadmap/features/feat-alpha.md" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = s.replace("## 验收标准", """| 子项 | 状态 |
|---|---|
| Phase 3: cache e2e | 🟡 Sync variant ✅ parity（全量收口不属本 feature） |

## 验收标准""")
open(p, 'w', encoding='utf-8').write(s)
PYEOF
    st_check "表格单元格多符号 ⇒ 歧义 MUST 跳过（不得误报）" 0 "$(run_st "$F" "$ST_ROOT/l12")" "$ST_ROOT/l12"

    # ⑬ `.rddf/roadmap/` **目录不存在** ⇒ 环境性缺失 ⇒ 软着陆（warning + exit 0）
    F="$ST_ROOT/fx_nodir"; mk_fixture "$F"
    rm -rf "$F/.rddf/roadmap"
    st_check "fragments 目录缺失 MUST 软着陆（warning + exit 0）" 0 "$(run_st "$F" "$ST_ROOT/l13")" "$ST_ROOT/l13"
    st_out "$ST_ROOT/l13" "::warning"

    echo ""
    if [ "$st_fail" -eq 0 ]; then
        echo "${GREEN}✅ self-test: 全部通过${NC}"
        exit 0
    fi
    echo "${RED}❌ self-test: $st_fail 项失败${NC}" >&2
    exit 1
fi

# ===========================================================================
# 主检查路径
# ===========================================================================
RC=0
python3 - "$CHECK_ROOT" <<'PY' || RC=$?
import os
import re
import sys

ROOT = os.path.abspath(sys.argv[1])

SENT_START = "<!-- AUTO: feature fragments start -->"
SENT_END = "<!-- AUTO: feature fragments end -->"

DONE_SYMS = {"✅", "☑"}
TODO_SYMS = {"⏳", "🟡"}
SYM_RE = re.compile(r"[✅☑⏳🟡]")

violations = []
notices = []


def sym_class(sym):
    """机械可判定的完成度映射；不可判定的符号一律返回 None（跳过）。"""
    if sym in DONE_SYMS:
        return "done"
    if sym in TODO_SYMS:
        return "todo"
    return None


def split_list_literal(inner):
    """复刻工具链 `_lib/roadmap_state.py::_split_list_literal`。"""
    items, depth, cur = [], 0, []
    for ch in inner:
        if ch in "([{":
            depth += 1
            cur.append(ch)
        elif ch in ")]}":
            depth -= 1
            cur.append(ch)
        elif ch == "," and depth == 0:
            item = "".join(cur).strip().strip("'\"")
            if item:
                items.append(item)
            cur = []
        else:
            cur.append(ch)
    item = "".join(cur).strip().strip("'\"")
    if item:
        items.append(item)
    return items


def parse_front_matter(path):
    """复刻工具链 `_lib/roadmap_state.py::_parse_fragment_file` 的 front-matter 解析。

    ⚠️ 重复键 MUST 抛错（工具自身会抛 ValueError）⇒ 护栏 **fail-closed**。
    """
    with open(path, encoding="utf-8") as fh:
        content = fh.read()
    if not content.startswith("---"):
        raise ValueError("文件未以 '---' 开头（无 front-matter）")
    parts = content.split("---", 2)
    if len(parts) < 3:
        raise ValueError("front-matter 未闭合")
    fm = {}
    for line in parts[1].strip().splitlines():
        if ":" not in line:
            continue
        k, v = line.split(":", 1)
        k, v = k.strip(), v.strip()
        if not k:
            continue
        if k in fm:
            raise ValueError("重复的 front-matter 键 %r" % k)
        if v.startswith("[") and v.endswith("]"):
            fm[k] = split_list_literal(v[1:-1])
        else:
            fm[k] = v
    return fm


def load_features(frag_dir):
    """复刻工具链 `load_fragments(include_archived=True)` + `kind == 'feature'` 过滤。"""
    feats = []
    for sub in ("phases", "features", "archive"):
        d = os.path.join(frag_dir, sub)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".md"):
                continue
            p = os.path.join(d, fn)
            try:
                fm = parse_front_matter(p)
            except (OSError, ValueError, UnicodeDecodeError) as exc:
                violations.append("front-matter 解析失败（%s）：%s ⇒ fail-closed" % (p, exc))
                continue
            if fm.get("kind", "phase") != "feature":
                continue
            theme_v = fm.get("主题", "")
            if isinstance(theme_v, list):
                themes = theme_v
            elif theme_v:
                themes = [theme_v]
            else:
                themes = []
            feats.append({
                "id": fm.get("id", fn[:-3]),
                "status": fm.get("status", "active"),
                "phase_refs": fm.get("phase_refs", []),
                "theme": themes[0] if themes else "",
                "path": p,
            })
    feats.sort(key=lambda f: f["id"])
    return feats


def render_block(feats):
    """复刻工具链 `_lib/roadmap_state.py::update_agent_md` 的渲染规则。"""
    lines = [SENT_START, ""]
    if not feats:
        lines.append("_no feature fragments registered — run `rddf roadmap add-feature ...`_")
    else:
        lines.append("| id | status | phase_refs | theme |")
        lines.append("|---|---|---|---|")
        for f in feats:
            refs = ", ".join(f["phase_refs"]) if f["phase_refs"] else "(none)"
            theme = f["theme"].replace("|", "\\|")
            lines.append("| `%s` | %s | %s | %s |" % (f["id"], f["status"], refs, theme))
    lines.append("")
    lines.append("_auto-generated by `rddf roadmap --update-agent-md` — DO NOT hand-edit this block_")
    lines.append(SENT_END)
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# 环境性缺失 ⇒ 软着陆（与 `verify_test_counts.sh` 同型：无 build dir ⇒ warning + exit 0）
# ⚠️ 区分两种「没有」：**目录不存在** = 环境问题（软着陆）；
#   **目录存在但内容不可解析 / 一个 feature 都没有** = 真漂移（fail-closed）。
# ---------------------------------------------------------------------------
frag_dir = os.path.join(ROOT, ".rddf", "roadmap")
if not os.path.isdir(frag_dir):
    print("  ::warning `.rddf/roadmap/` 不存在（%s）⇒ 本次**跳过**roadmap 簿记检查"
          "（环境性缺失：稀疏检出 / 下游 vendor / fork）" % frag_dir)
    print("  OK：roadmap 簿记一致性检查已跳过（环境性缺失）")
    sys.exit(0)

feats = load_features(frag_dir)
if not feats:
    violations.append("`.rddf/roadmap/**` 下**没有任何 feature fragment** ⇒ "
                      "AUTO 块无法渲染（fail-closed）")

# ---------------------------------------------------------------------------
# C1：AUTO 块守恒（渲染 + 逐字比对）
# ---------------------------------------------------------------------------
expected = render_block(feats)

agents_path = os.path.join(ROOT, "AGENTS.md")
if not os.path.exists(agents_path):
    violations.append("根 AGENTS.md 不存在 ⇒ 无法比对 AUTO 块（fail-closed）")
else:
    with open(agents_path, encoding="utf-8") as fh:
        content = fh.read()
    if SENT_START not in content or SENT_END not in content:
        violations.append("根 AGENTS.md **缺** AUTO feature 哨兵（%s / %s）⇒ "
                          "无法比对（fail-closed）" % (SENT_START, SENT_END))
    else:
        s = content.index(SENT_START)
        e = content.index(SENT_END, s) + len(SENT_END)
        committed = content[s:e]
        if committed != expected:
            import difflib
            diff = list(difflib.unified_diff(
                committed.splitlines(), expected.splitlines(),
                fromfile="AGENTS.md（已提交）", tofile="front-matter（渲染出的期望）",
                lineterm="", n=1))
            violations.append("AUTO feature 块与 `.rddf/roadmap/**` 的 front-matter **不一致**")
            for line in diff[:40]:
                notices.append("  " + line)
        else:
            notices.append("AUTO feature 块与 front-matter 逐字一致（%d 个 feature）" % len(feats))

# ---------------------------------------------------------------------------
# C2 / C3：feature 文件**内部**的完成度一致性（三个比对面）
#   面 1 = `### phase-*` 段的 bullet
#   面 2 = `## 验收标准` 的 checkbox
#   面 3 = 正文里的 Markdown 表格行
# ---------------------------------------------------------------------------
def known_change_names(root):
    """change 名注册表。

    ⚠️ 键空间 MUST 与**语料实际引用形式**匹配：仓内 feature 文件引用的是
    **裸名**（`dual-backend-rv32i-integration`），而 `openspec/changes/archive/`
    下的目录名带归档日期前缀与日期后缀（`2026-10-27-dual-backend-rv32i-integration-2026-10`）
    ⇒ 只收目录名 ⇒ **零命中** ⇒ 该判据在真实语料上永不触发（= 死判据）。
    ⇒ 故收录三种形态：目录名 / 去归档日期前缀 / 再去日期后缀。
    """
    names = set()
    base = os.path.join(root, "openspec", "changes")
    if not os.path.isdir(base):
        return names
    dirs = []
    for root_d, subdirs, _files in os.walk(base):
        for d in subdirs:
            rel = os.path.relpath(os.path.join(root_d, d), base)
            if rel != "archive":
                dirs.append(rel)
    for d in dirs:
        names.add(d)
        # ⚠️ 归档目录在 `archive/` 之下 ⇒ MUST 去掉该前缀，否则裸名永远命中不了
        base_name = re.sub(r"^archive/", "", d)
        names.add(base_name)
        x = re.sub(r"^\d{4}-\d{2}-\d{2}-", "", base_name)
        names.add(x)
        # ⚠️ 本仓的 change 名后缀是**月**粒度（`-2026-10`），归档目录前缀是**日**
        #   粒度（`2026-10-28-`）⇒ 两种后缀形态都要能剥掉
        names.add(re.sub(r"-\d{4}-\d{2}$", "", x))
        names.add(re.sub(r"-\d{4}-\d{2}-\d{2}$", "", x))
    return names


CHANGE_NAMES = known_change_names(ROOT)

PHASE_BULLET_RE = re.compile(r"^\s*[-*]\s+(\S+)\s+(.*)$")
PHASE_LABEL_RE = re.compile(r"^Phase\s+(\d+)\b")
CHECKBOX_RE = re.compile(r"^\s*[-*]\s+\[( |x|X)\]\s+(.*)$")
CODE_TICK_RE = re.compile(r"^`([^`]+)`")
SEPARATOR_CELL_RE = re.compile(r"^:?-{3,}:?$")


def split_sections(lines):
    """返回 (phase 段落的行, 验收标准段落的行, 表格行)。"""
    phase_lines, accept_lines, table_lines = [], [], []
    section = None
    for line in lines:
        if line.startswith("### "):
            section = "phase" if line[4:].strip().startswith("phase") else None
            continue
        if line.startswith("## "):
            section = "accept" if line[3:].strip() == "验收标准" else None
            continue
        if section == "phase":
            phase_lines.append(line)
        elif section == "accept":
            accept_lines.append(line)
        st = line.strip()
        if st.startswith("|") and st.endswith("|"):
            table_lines.append(st)
    return phase_lines, accept_lines, table_lines


def keys_from_text(text):
    """从一段文本里抽出键：行首的 `Phase N` 标签 / 行首的已知 change 名。"""
    keys = []
    pm = PHASE_LABEL_RE.match(text)
    if pm:
        keys.append("label:Phase-" + pm.group(1))
    tm = CODE_TICK_RE.match(text)
    if tm and tm.group(1) in CHANGE_NAMES:
        keys.append("change:" + tm.group(1))
    return keys


def collect_side(lines, kind):
    """一个比对面 ⇒ 键 → 完成度集合。"""
    out = {}
    if kind == "table":
        for raw in lines:
            cells = [c.strip() for c in raw.strip("|").split("|")]
            if not cells or all(SEPARATOR_CELL_RE.match(c) for c in cells if c):
                continue
            idxs = [i for i, c in enumerate(cells) if c and SYM_RE.match(c)]
            if len(idxs) != 1:
                continue          # 0 个 = 没有状态列；>1 = 歧义 ⇒ 跳过
            i = idxs[0]
            if len(set(SYM_RE.findall(cells[i]))) > 1:
                continue          # 一个单元格里有多个符号 ⇒ 歧义 ⇒ 跳过
            cls = sym_class(cells[i][0])
            if cls is None:
                continue
            for j, cell in enumerate(cells):
                if j == i:
                    continue
                for k in keys_from_text(cell):
                    out.setdefault(k, set()).add(cls)
        return out
    for line in lines:
        if kind == "checkbox":
            m = CHECKBOX_RE.match(line)
            if not m:
                continue
            cls = "done" if m.group(1) in ("x", "X") else "todo"
            text = m.group(2).strip()
        else:
            m = PHASE_BULLET_RE.match(line)
            if not m:
                continue
            cls = sym_class(m.group(1))
            if cls is None:
                continue
            text = m.group(2).strip()
        for k in keys_from_text(text):
            out.setdefault(k, set()).add(cls)
    return out


def compare_sides(sides, where):
    """同一个键在**不同面**上的完成度 MUST 一致；任一面多值 ⇒ 跳过。"""
    keys = set()
    for s in sides.values():
        keys |= set(s.keys())
    for key in sorted(keys):
        present = {name: s[key] for name, s in sides.items() if key in s}
        if len(present) < 2:
            continue
        if any(len(v) > 1 for v in present.values()):
            notices.append("  跳过 %s 的 %r（同一面存在多个完成度符号 ⇒ 无法判定）" % (where, key))
            continue
        classes = set()
        for v in present.values():
            classes |= v
        if len(classes) > 1:
            detail = "，".join("%s=%s" % (n, sorted(v)[0]) for n, v in sorted(present.items()))
            violations.append("%s：%r 的完成度符号冲突 —— %s" % (where, key, detail))


for f in feats:
    try:
        with open(f["path"], encoding="utf-8") as fh:
            lines = fh.read().splitlines()
    except OSError as exc:
        violations.append("读取 feature 文件失败（%s）：%s" % (f["path"], exc))
        continue
    phase_lines, accept_lines, table_lines = split_sections(lines)
    sides = {
        "phase bullet": collect_side(phase_lines, "bullet"),
        "验收标准 checkbox": collect_side(accept_lines, "checkbox"),
        "表格行": collect_side(table_lines, "table"),
    }
    if not sides["phase bullet"] and not sides["验收标准 checkbox"] and not sides["表格行"]:
        notices.append("  跳过 %s：无含状态符号的 bullet / checkbox / 表格行 ⇒ 无可比对内容"
                       % f["id"])
    compare_sides(sides, f["id"])

for n in notices:
    print("  ::notice " + n)
if violations:
    for v in violations:
        print("VIOLATION " + v, file=sys.stderr)
    sys.exit(1)
print("  OK：AUTO 块守恒 + feature 文件内部的完成度一致性 全部通过")
PY

if [ "$RC" -ne 0 ]; then
    echo "${RED}❌ verify_roadmap_fragments: 有违规${NC}" >&2
    exit 1
fi
echo "${GREEN}✅ verify_roadmap_fragments: PASS${NC}"
