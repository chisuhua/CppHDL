#!/usr/bin/env python3
"""docs_link_scope.py —— 文档分级派生器 + 链接/引用有效性检查器

单一逻辑载体（`scripts/verify_docs_links.sh` 是薄壳）。
判据来源 = `docs/DOCUMENT-ARCHITECTURE.md` §9（治理口径正本）。

用法：
  python3 scripts/lib/docs_link_scope.py --render          # 打印派生式登记表（md）
  python3 scripts/lib/docs_link_scope.py --check           # C1/C2/C3/C5（exit 0/1）
  python3 scripts/lib/docs_link_scope.py --list-l1         # 只列 L1 文件（调试用）

退出码：0 全绿 · 1 有违规 · 2 fail-closed（配置/环境异常）
"""
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# ── 扫描范围（D1″ 的 out-of-scope 已排除：AGENTS.md 家族 · openspec/** · .rddf/checks/**）
SKIP_DIR_PARTS = {".git", "build", "node_modules", "external", "openspec", "third_party", "verilator-install"}
SKIP_FILE_NAMES = {"AGENTS.md"}          # AGENTS.md 家族：显式 out-of-scope（§D1″）

# ── L1 前缀（D1′ ①：目录前缀默认）
L1_PREFIX = ("docs/usage_guide/", "docs/developer_guide/", "docs/skills/", "docs/simulation/",
             "docs/riscv-mini/", "docs/architecture/",
             "tests/", "examples/", "samples/")   # 代码目录的 README 也算活文档
# ── L2/L3 前缀（最长前缀优先 ⇒ docs/skills/archive/ 落 L2）
L23_PREFIX = ("docs/adr/", "docs/archive/", "docs/skills/archive/",
              ".rddf/", "openspec/changes/archive/")   # 历史（最长前缀优先）
# ── 逐字保留区（P1 豁免 + 级别 = L2）
ARCHIVE_PREFIX = "docs/archive/"

# ��─ 混合目录的派生规则（D1′ ②：不需要人工表 ⇒ 由规则派生，C5 兜住「规则没覆盖」）
# ── 特殊文件显式清单（D1′ ②：混合目录里**无法由前缀机械判定**的例外）
#    ⚠️ 依据 = `docs/architecture/README.md` §1/§2 的判读口径 + 文件自身的 dated 声明
SPECIAL_LEVEL = {
    # 变更流水 ⇒ 历史记录（不是导航入口）
    "docs/CHANGELOG.md": "L2",
}
ROOT_MD_L3 = {"perf_results.md"}   # 生成物（gitignored）


_DOCS_CACHE = None
_NAME_INDEX = None   # basename → [Path]，一次 rglob 建好（C2 消歧用；避免逐引用全仓扫）


def _scan():
    """一次 rglob，同时产出 (docs 列表, 文件名索引)"""
    global _DOCS_CACHE, _NAME_INDEX
    if _DOCS_CACHE is not None:
        return _DOCS_CACHE, _NAME_INDEX
    out, by_name = [], {}
    for p in REPO.rglob("*.md"):
        rel = p.relative_to(REPO).as_posix()
        parts = set(rel.split("/")[:-1])
        if parts & SKIP_DIR_PARTS:
            continue
        by_name.setdefault(p.name, []).append(p)
        if p.name in SKIP_FILE_NAMES:
            continue
        out.append(rel)
    _DOCS_CACHE, _NAME_INDEX = sorted(out), by_name
    return _DOCS_CACHE, _NAME_INDEX


def iter_docs():
    """扫描范围内所有 .md（相对仓根的 posix 路径）"""
    return _scan()[0]


def level_of(rel):
    """派生级别。返回 'L1'/'L2'/'L3'/'L4'；无法归类 ⇒ None（C5 红）"""
    if rel in SPECIAL_LEVEL:
        return SPECIAL_LEVEL[rel]
    # 最长前缀优先
    matches = [("L1", p) for p in L1_PREFIX if rel.startswith(p)]
    matches += [("L2", p) for p in L23_PREFIX if rel.startswith(p)]
    if matches:
        matches.sort(key=lambda x: -len(x[1]))
        lvl = matches[0][0]
        # skills/archive 归 L2（已由最长前缀保证）
        return lvl
    if rel.startswith("external/"):
        return "L4"
    if rel in ROOT_MD_L3:
        return "L3"
    if "/" not in rel:                        # 仓根散落 *.md
        return "L1"
    if rel.startswith("docs/"):               # docs 根散落 *.md
        return "L1"
    return None


LINK_PAT = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
REF_PAT = re.compile(r"`?([A-Za-z0-9_./-]+\.(?:h|cpp|py|sh|sv|verif|yaml|yml|md|tsv|txt)):(\d+)(?:-(\d+))?`?")
ABS_PAT = re.compile(r"/workspace/[A-Za-z0-9_./-]+|/home/[A-Za-z0-9_]+/|/mnt/[A-Za-z0-9_./-]+")


def strip_code_fences(text):
    """返回 (去代码块后的文本, 每行是否在代码块内)"""
    lines = text.split("\n")
    incode, fence = [], False
    for l in lines:
        if l.strip().startswith("```"):
            fence = not fence
            incode.append(True)
            continue
        incode.append(fence)
    kept = "\n".join("" if incode[i] else l for i, l in enumerate(lines))
    return kept, incode


def check(verbose=False):
    """返回 (violations: list[(rule, path, detail)], l1_files, stats)"""
    viol = []
    docs = iter_docs()
    l1, stats = [], {"docs": len(docs), "links": 0, "refs": 0, "l1_links": 0, "l1_refs": 0}

    for rel in docs:
        lvl = level_of(rel)
        if lvl is None:
            viol.append(("C5", rel, "无法归入任一级 ⇒ 分级规则需补（否则落进治理盲区）"))
            continue
        if lvl == "L1":
            l1.append(rel)

    # C1 / C2：只扫 L1
    for rel in l1:
        p = REPO / rel
        text = p.read_text(errors="replace")
        kept, incode = strip_code_fences(text)
        base = p.parent
        for m in LINK_PAT.finditer(kept):
            tgt = m.group(2)
            if tgt.startswith(("http", "#", "mailto")):
                continue
            stats["l1_links"] += 1
            # ⚠️ 锚点（`path#anchor`）MUST 先剥离再判文件存在 —— 否则 `AGENTS.md#sec` 会被误判死链
            path_part = tgt.split("#", 1)[0]
            if not path_part:
                continue                      # 纯锚点（`#sec`）⇒ 同文档内跳转 ⇒ 不判
            if path_part.startswith("file://"):
                viol.append(("C1", rel, f"本机 file:// 链接 [{m.group(1)}]({tgt})"))
                continue
            if not (base / path_part).resolve().exists():
                viol.append(("C1", rel, f"死链 [{m.group(1)}]({tgt})"))
        for m in REF_PAT.finditer(kept):
            f, a, b = m.group(1), int(m.group(2)), m.group(3)
            stats["l1_refs"] += 1
            q = REPO / f
            if not q.exists():
                cands = _scan()[1].get(Path(f).name, [])
                if len(cands) == 1:
                    q = cands[0]
                elif "/" in f:
                    # **带路径**的引用 ⇒ 严格判（路径本身就是定位信息）
                    viol.append(("C2", rel, f"引用文件不存在 `{f}:{a}`"))
                    continue
                else:
                    # **裸文件名** + 多候选/无同名 ⇒ **不可判定**（不是死链）
                    # ⚠️ 依据：治理口径 D2「先分类再判定」→ 不可判定者不报（防伪缺陷）
                    continue
            n = len(q.read_text(errors="replace").split("\n"))
            if (int(b) if b else a) > n:
                viol.append(("C2", rel, f"行号越界 `{f}:{a}`（文件共 {n} 行）"))

    # C3：全仓扫（排除 archive）
    for rel in docs:
        if rel.startswith(ARCHIVE_PREFIX):
            continue
        t = (REPO / rel).read_text(errors="replace")
        for m in ABS_PAT.finditer(t):
            viol.append(("C3", rel, f"本机绝对路径 `{m.group(0)[:60]}`"))

    stats["l1"] = len(l1)
    stats["l23"] = len(docs) - len(l1)
    return viol, l1, stats


def render(l1, stats):
    lines = [
        "---",
        "# ⚠️ **本文件是台账** ⇒ ⇒ `evidence:` 必填（`scripts/verify_ledger_evidence.sh` 的 R1）",
        "#    ⇒ 指向**派生器自己的护栏**（本表的分级漂移由它抓 ⇒ ⇒ 同一载体）",
        "evidence: probe:scripts/verify_docs_links.sh",
        "---",
        "",
        "# 文档链接护栏 —— 分级登记表（**派生 · 禁手改**）",
        "",
        "> ⚠️ **本文件由 `scripts/lib/docs_link_scope.py --render` 生成** ⇒ ⇒ **手改无效**",
        "> （下次 `--render` 会覆盖 ⇒ 分级漂移会被 C5 抓）。",
        "> 分级规则见 `docs/DOCUMENT-ARCHITECTURE.md` §9.1（治理口径正本）。",
        "",
        f"**统计**：扫描 {stats['docs']} 篇 · L1 **{stats['l1']}** 篇 · L2/L3 {stats['l23']} 篇",
        "",
    ]
    lines += ["## 显式登记的例外（无法由前缀机械判定）", "", "| 文件 | 级别 |", "|---|---|"]
    for f, lv in sorted(SPECIAL_LEVEL.items()):
        lines.append(f"| `{f}` | {lv} |")
    lines += ["", "## L1 活文档（死链 / 行号 / 本机路径 MUST 全绿）", "", "| # | 文件 |", "|---|---|"]
    for i, f in enumerate(l1, 1):
        lines.append(f"| {i} | `{f}` |")
    lines += ["", "## 规则摘要", "",
              "- **L1 前缀**：" + " · ".join(f"`{p}`" for p in L1_PREFIX),
              "- **L2/L3 前缀（最长优先）**：" + " · ".join(f"`{p}`" for p in L23_PREFIX),
              "- **混合目录派生**：`docs/architecture/phases|*review*|README.md` ⇒ L2，其余 `docs/architecture/*.md` ⇒ L1；"
              "仓根散落 `*.md` ⇒ L1（`PHASE1-REVISED-PLAN.md` / `perf_results.md` / `Compare.md` ⇒ L3）；`docs/*.md` ⇒ L1",
              "- **out-of-scope（不扫）**：`AGENTS.md` 家族 · `openspec/**` · `.rddf/checks/**` · `external/**`",
              "- **P1 豁免**：`docs/archive/**`（逐字保留区）", ""]
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--list-l1", action="store_true")
    args = ap.parse_args()

    if not (args.render or args.check or args.list_l1):
        print("用法：--render | --check | --list-l1", file=sys.stderr)
        return 2

    try:
        viol, l1, stats = check()
    except Exception as e:                                   # fail-closed
        print(f"::error::派生器异常（fail-closed）：{e}", file=sys.stderr)
        return 2

    if args.list_l1:
        print("\n".join(l1))
        return 0
    if args.render:
        print(render(l1, stats))
        return 0

    # --check
    order = {"C5": 0, "C1": 1, "C2": 2, "C3": 3}
    viol.sort(key=lambda v: (order.get(v[0], 9), v[1]))
    for rule, path, detail in viol:
        print(f"::error::{rule} · {path} · {detail}", file=sys.stderr)
    if viol:
        print(f"\n❌ docs-links: {len(viol)} 处违规"
              f"（C5 分级 {sum(1 for v in viol if v[0]=='C5')} · C1 死链 {sum(1 for v in viol if v[0]=='C1')}"
              f" · C2 引用 {sum(1 for v in viol if v[0]=='C2')} · C3 本机路径 {sum(1 for v in viol if v[0]=='C3')}）",
              file=sys.stderr)
        return 1
    print(f"✅ docs-links: PASS（L1 {stats['l1']} 篇 · 链接 {stats['l1_links']} 条 · "
          f"引用 {stats['l1_refs']} 处 · L2/L3 {stats['l23']} 篇不扫）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
