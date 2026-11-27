#!/usr/bin/env python3
"""improvement_index.py —— `improvement-suggestions.md` 的状态列**生成器 + 一致性判据**

判据来源 = `.rddf/roadmap/phase-6-rtl-productization.md` §4 ·「索引表状态列的自动生成」
（登记原文：现状 = **人工维护** ⇒ ⇒ 9 份草案要人工同步 ⇒ ⇒ **必然漂** ⇒ ⇒ 根本修法 =
**从 frontmatter `状态:` 生成** + **一致性护栏**）。

⚠️ **为什么索引表不能「整表派生」**：
   索引表的状态列 = **状态词 + 大量人工附注**（复核记录 / 裁决依据 / 遗留登记）
   ⇒ ⇒ 整表派生会**丢掉附注** ⇒ ⇒ 本工具只**刷新状态词**，**逐字保留** emoji 与附注。

⚠️ **词 / 附注的切分锚点**（两处都必须是**机械**可判的）：
   · 索引表状态列 = `<emoji> **<word>**<附注>` ⇒ ⇒ 词 = 单元格内**第一个** `**…**`；
   · frontmatter `状态:` = `<word>` 或 `<word>（<说明>）` ⇒ ⇒ 词 = **首个词法 token**。

⚠️ **边界：索引表是「提案池」而不是全量清单**
   （文件头的生命周期：实施 / 归档后**从本表移除**）⇒ ⇒ **不**判「每个
   `.rddf/improvements/*.md` 都在表里」（全仓 70+ 份 · 表里 16 份 ⇒ ⇒ 那样判必假红）
   ⇒ ⇒ **反向覆盖由「表内每行都必须有对应文件」承担**（R2-a）。

用法：
   python3 scripts/lib/improvement_index.py --check          # 判据（exit 0/1）
   python3 scripts/lib/improvement_index.py --render          # 渲染（stdout）
   python3 scripts/lib/improvement_index.py --self-test       # fixture 自检

⚠️ **fail-closed**：索引文件缺失 / 行形态异常 / 指向的文件缺失 / frontmatter 缺 `状态:`
   ⇒ ⇒ 一律**红**（绝不静默跳过）。
"""

import argparse
import os
import pathlib
import re
import shutil
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
INDEX_NAME = "improvement-suggestions.md"
IMPROVEMENT_DIR = ".rddf/improvements"
RDFF_DIR = ".rddf"

# 索引表 = 5 列 ⇒ `|` 分段后恒为 **7** 段（首尾各一个空段）
EXPECTED_CELLS = 7
STATE_CELL_INDEX = 5  # ← 第 5 列（状态）

ROW_RE = re.compile(r"^\|\s*\[([^\]]+)\]\(([^)]+)\)\s*\|")
# ⚠️ **键名 MUST 允许非 ASCII** —— 本仓的 front-matter 键是**中文**（`状态:` / `进度:` /
#   `来源:` / `主题:` …）⇒ ⇒ 只认 `[A-Za-z_]` 会让 `状态:` **永远解析不到**
#   ⇒ ⇒ 护栏对**每一行**都误判红（实测：F0/F6/F7 三条 fixture 全红才发现）。
#   ⇒ ⇒ 键 = **第一个 `:` 前**的内容；值 = 其后（**可含 `:`** ⇒ 如 `evidence: probe:…`）。
#   ⇒ ⇒ 前导空白 ⇒ 嵌套键（如 `roadmap_ref:` 的子行）⇒ ⇒ **不**当作顶层键。
FM_KEY_RE = re.compile(r"^([^\s:#][^:#]*):[ \t]*(.*)$")
WORD_RE = re.compile(r"^([A-Za-z][A-Za-z0-9_-]*)")
CELL_WORD_RE = re.compile(r"\*\*([A-Za-z][A-Za-z0-9_-]*)\*\*")


def _out(msg: str) -> None:
    print(msg)


def frontmatter(text: str) -> dict:
    """解析 YAML front-matter（**够用即可** ⇒ 只取顶层 `key: value`）。

    ⚠️ 不引入 yaml 依赖 ⇒ ⇒ 嵌套结构（如 `roadmap_ref:` 的子键）会被当作**平铺**的
    独立键收进来 ⇒ ⇒ 本工具只读 `状态:` ⇒ ⇒ 无害。
    """
    if not text.startswith("---"):
        return {}
    parts = text.split("---", 2)
    if len(parts) < 3:
        return {}
    out: dict = {}
    for line in parts[1].split("\n"):
        m = FM_KEY_RE.match(line)
        if m:
            out[m.group(1)] = m.group(2).strip()
    return out


def word_of(value: str) -> str:
    """`状态:` 的值 ⇒ 首个词法 token（`partial（…）` ⇒ `partial`）。"""
    m = WORD_RE.match(value.strip())
    return m.group(1) if m else ""


def cell_word(cell: str) -> str:
    """索引表状态单元格 ⇒ 第一个 `**word**`（空 ⇒ 无词形态）。"""
    m = CELL_WORD_RE.search(cell)
    return m.group(1) if m else ""


def parse_index(text: str) -> list:
    """解析索引表的**数据行** ⇒ ⇒ `[{"line": 1-based, "name", "path", "cell", "word"}]`。"""
    rows = []
    for idx, line in enumerate(text.split("\n"), 1):
        m = ROW_RE.match(line)
        if not m:
            continue
        cells = line.split("|")
        rows.append(
            {
                "line": idx,
                "name": m.group(1),
                "path": m.group(2),
                "cells": cells,
                "cell": cells[STATE_CELL_INDEX] if len(cells) == EXPECTED_CELLS else "",
                "word": cell_word(cells[STATE_CELL_INDEX])
                if len(cells) == EXPECTED_CELLS
                else "",
            }
        )
    return rows


def index_path(repo: pathlib.Path) -> pathlib.Path:
    """索引表位置：`.rddf/<name>` 优先（canonical）→ 仓根 `<name>`（legacy 兜底）。

    ⚠️ 与产品侧 `_lib/core/rddf_paths.py::resolve` **同一口径**（canonical 在前 ·
    legacy 在后）⇒ ⇒ 两边不会漂。
    ⚠️ **不 import 产品包** ⇒ ⇒ 本护栏仍只依赖 python3 + 仓内文件
    （`verify_improvement_index.sh` 的可达性前提 = 仓外零依赖）。
    """
    canonical = repo / RDFF_DIR / INDEX_NAME
    if canonical.is_file():
        return canonical
    legacy = repo / INDEX_NAME
    if legacy.is_file():
        return legacy
    return canonical


def _rel(repo: pathlib.Path, path: pathlib.Path) -> str:
    """仓内相对路径（消息用）⇒ ⇒ 避免消息里出现绝对路径。"""
    try:
        return str(path.relative_to(repo.resolve()))
    except ValueError:
        return str(path)


def _resolve(repo: pathlib.Path, base: pathlib.Path, rel: str):
    """把索引表里的相对路径解析成仓内绝对路径（**防路径逃逸** ⇒ 逃逸 ⇒ None）。

    ⚠️ `base` = **索引表所在目录**（不是仓根）⇒ ⇒ `.rddf/` 托管的索引用
    `improvements/<name>.md` 相对链接，legacy 仓根索引用 `.rddf/improvements/<name>.md`
    ⇒ ⇒ 与产品侧 `rddf_paths.link_prefix()` **同口径**。
    """
    target = (base / rel).resolve()
    try:
        target.relative_to(repo.resolve())
    except ValueError:
        return None
    return target


def collect(repo: pathlib.Path):
    """收集（rows · states · errors）⇒ ⇒ `states[row_line] = 期望的状态词`。"""
    idx = index_path(repo)
    errors: list = []
    states: dict = {}
    rows: list = []
    if not idx.is_file():
        return rows, states, ["索引文件缺失：`%s`" % _rel(repo, idx)]
    text = idx.read_text(encoding="utf-8")
    rows = parse_index(text)
    if not rows:
        errors.append("索引表里**没有任何数据行**（`| [name](path) | … |` 形态）⇒ ⇒ 无法判定")
    for row in rows:
        where = "%s:%d" % (_rel(repo, idx), row["line"])
        if len(row["cells"]) != EXPECTED_CELLS:
            errors.append(
                "%s：索引行形态异常（`|` 分段数 = %d，期望 %d）"
                % (where, len(row["cells"]), EXPECTED_CELLS)
            )
            continue
        if not row["word"]:
            errors.append(
                "%s：状态列**没有** `**word**` 形态 ⇒ ⇒ 无法判定状态词" % where
            )
            continue
        target = _resolve(repo, idx.parent, row["path"])
        if target is None:
            errors.append("%s：链接路径**逃出仓根**（`%s`）" % (where, row["path"]))
            continue
        if not target.is_file():
            errors.append("%s：链接指向的文件**不存在**（`%s`）" % (where, row["path"]))
            continue
        fm = frontmatter(target.read_text(encoding="utf-8"))
        if "状态" not in fm:
            errors.append(
                "%s：`%s` 的 front-matter **缺 `状态:` 字段** ⇒ ⇒ 无法判定"
                % (where, row["path"])
            )
            continue
        want = word_of(fm["状态"])
        if not want:
            errors.append(
                "%s：`%s` 的 `状态:` 值**无法抽出词**（`%s`）" % (where, row["path"], fm["状态"])
            )
            continue
        states[row["line"]] = want
    return rows, states, errors


def render(repo: pathlib.Path) -> str:
    """渲染索引表：只把状态列的 `**word**` 换成 frontmatter 的词 ⇒ ⇒ emoji / 附注**逐字保留**。"""
    idx = index_path(repo)
    text = idx.read_text(encoding="utf-8")
    _rows, states, _errors = collect(repo)
    out = []
    for idx, line in enumerate(text.split("\n"), 1):
        want = states.get(idx)
        if want:
            cells = line.split("|")
            cells[STATE_CELL_INDEX] = CELL_WORD_RE.sub(
                lambda m: "**%s**" % want, cells[STATE_CELL_INDEX], count=1
            )
            line = "|".join(cells)
        out.append(line)
    return "\n".join(out)


def check(repo: pathlib.Path) -> int:
    """判据：R1 状态词一致 · R2 fail-closed。"""
    rows, states, errors = collect(repo)
    violations = list(errors)

    # ── R1：逐行比对「索引表的状态词」↔「frontmatter 的状态词」────────────
    idx = index_path(repo)
    text = idx.read_text(encoding="utf-8") if idx.is_file() else ""
    for row in parse_index(text):
        want = states.get(row["line"])
        if want is None:
            continue  # R2 已报过
        if row["word"] != want:
            violations.append(
                "%s:%d：状态词**不一致** —— 索引表 = `%s`，frontmatter `状态:` = `%s`"
                " ⇒ ⇒ 索引表的状态列**未从 frontmatter 生成**"
                % (INDEX_NAME, row["line"], row["word"], want)
            )
        else:
            _out(
                "  ✅ %-46s 状态词 = `%s`" % (row["name"][:46], row["word"])
            )

    if violations:
        for v in violations:
            _out("::error::%s" % v)
        _out("❌ 索引状态列一致性未通过（%d 项）" % len(violations))
        return 1
    _out(
        "✅ 索引状态列与 frontmatter `状态:` 逐行一致（%d 行）"
        % len(rows)
    )
    _out(
        "ℹ️ 边界：索引表是**提案池**（实施/归档后从表移除）⇒ ⇒ **不**判"
        "「每个 `.rddf/improvements/*.md` 都在表里」"
    )
    return 0


# ---------------------------------------------------------------------------
# fixture 自检（**不依赖真实仓 / 网络 / git 历史 / verilator**）
# ---------------------------------------------------------------------------

# ⚠️ **两套 fixture 索引** = 索引表的**两种合法托管形态** ⇒ ⇒ **链接前缀随托管位置变**
#   （与产品侧 `rddf_paths.link_prefix()` 同口径）：
#   · legacy（仓根托管）⇒ 链接 = `.rddf/improvements/<name>.md`
#   · canonical（`.rddf/` 托管）⇒ 链接 = `improvements/<name>.md`
FIXTURE_INDEX_LEGACY = """# improvement-suggestions —— fixture

| 提案 | 优先级 | 来源 | 添加时间 | 状态 |
|---|---|---|---|---|
| [alpha](.rddf/improvements/alpha.md) | P1 | 来源甲 | 2026-01-01 | 📝 **draft**（附注甲） |
| [beta](.rddf/improvements/beta.md) | P2 | 来源乙 | 2026-01-02 | ✅ **implemented** |
"""

FIXTURE_INDEX_CANONICAL = """# improvement-suggestions —— fixture

| 提案 | 优先级 | 来源 | 添加时间 | 状态 |
|---|---|---|---|---|
| [alpha](improvements/alpha.md) | P1 | 来源甲 | 2026-01-01 | 📝 **draft**（附注甲） |
| [beta](improvements/beta.md) | P2 | 来源乙 | 2026-01-02 | ✅ **implemented** |
"""

FIXTURE_ALPHA = """---
name: alpha
priority: P1
状态: draft
---

# alpha
"""

FIXTURE_BETA = """---
name: beta
priority: P2
状态: implemented
---

# beta
"""


def _make_fixture(root: pathlib.Path, canonical: bool = True) -> None:
    """搭一个 fixture 仓（**canonical=True** ⇒ 索引托管在 `.rddf/`；False ⇒ legacy 仓根）。

    ⚠️ **两处候选位置 MUST 先清干净** ⇒ ⇒ 否则上一条 case 留下的索引文件会让
    `index_path()` 解析到**另一个位置** ⇒ ⇒ fixture 变成假绿。
    """
    imp = root / IMPROVEMENT_DIR
    imp.mkdir(parents=True, exist_ok=True)
    for cand in (root / RDFF_DIR / INDEX_NAME, root / INDEX_NAME):
        if cand.is_file():
            cand.unlink()
    if canonical:
        idx = root / RDFF_DIR / INDEX_NAME
        idx.write_text(FIXTURE_INDEX_CANONICAL, encoding="utf-8")
    else:
        (root / INDEX_NAME).write_text(FIXTURE_INDEX_LEGACY, encoding="utf-8")
    (imp / "alpha.md").write_text(FIXTURE_ALPHA, encoding="utf-8")
    (imp / "beta.md").write_text(FIXTURE_BETA, encoding="utf-8")


def selftest() -> int:
    fails = 0
    passes = 0

    def case(name: str, ok: bool) -> None:
        nonlocal fails, passes
        if ok:
            passes += 1
            _out("  ✓ %s" % name)
        else:
            fails += 1
            _out("  ✗ %s" % name)

    tmp = pathlib.Path(tempfile.mkdtemp(prefix="improve-index-"))
    try:
        # ── 基线：未投毒 ⇒ 必须绿 ────────────────────────────────────
        _make_fixture(tmp)
        case("F0 基线：一致的 fixture ⇒ 绿", check(tmp) == 0)

        # ── F1：索引表的状态词与 frontmatter 不符 ⇒ 必须红 ───────────
        _make_fixture(tmp)
        p = index_path(tmp)
        p.write_text(
            FIXTURE_INDEX_CANONICAL.replace("📝 **draft**", "📝 **approved**"),
            encoding="utf-8",
        )
        case("F1 索引表状态词与 frontmatter 不符 ⇒ 红", check(tmp) != 0)

        # ── F2：frontmatter 缺 `状态:` ⇒ 必须红 ─────────────────────
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "alpha.md").write_text(
            FIXTURE_ALPHA.replace("状态: draft\n", ""), encoding="utf-8"
        )
        case("F2 frontmatter 缺 `状态:` ⇒ 红", check(tmp) != 0)

        # ── F3：frontmatter 的状态改了、索引表没跟上 ⇒ 必须红 ───────
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "alpha.md").write_text(
            FIXTURE_ALPHA.replace("状态: draft", "状态: approved"), encoding="utf-8"
        )
        case("F3 frontmatter 状态改了、索引表未跟上 ⇒ 红", check(tmp) != 0)

        # ── F4：索引行指向的文件不存在 ⇒ 必须红 ─────────────────────
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "beta.md").unlink()
        case("F4 索引行指向的文件不存在 ⇒ 红", check(tmp) != 0)

        # ── F5：状态列没有 `**word**` 形态 ⇒ 必须红 ─────────────────
        _make_fixture(tmp)
        p = index_path(tmp)
        p.write_text(
            FIXTURE_INDEX_CANONICAL.replace("📝 **draft**（附注甲）", "draft"),
            encoding="utf-8",
        )
        case("F5 状态列无 `**word**` 形态 ⇒ 红", check(tmp) != 0)

        # ── F6：`--render` 能把表刷新成一致 ⇒ 渲染后必须绿 ──────────
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "alpha.md").write_text(
            FIXTURE_ALPHA.replace("状态: draft", "状态: approved"), encoding="utf-8"
        )
        rendered = render(tmp)
        index_path(tmp).write_text(rendered, encoding="utf-8")
        case("F6 `--render` 刷新状态词 ⇒ 渲染后绿", check(tmp) == 0)

        # ── F7：`--render` MUST **逐字保留** emoji 与附注 ────────────
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "alpha.md").write_text(
            FIXTURE_ALPHA.replace("状态: draft", "状态: approved"), encoding="utf-8"
        )
        rendered = render(tmp)
        case(
            "F7 `--render` 逐字保留 emoji 与附注",
            "📝 **approved**（附注甲）" in rendered and "✅ **implemented**" in rendered,
        )

        # ── F8：canonical 托管（`.rddf/`）⇒ 必须绿 ──────────────────
        #   （判别力 = 「解析到 `.rddf/` 那份」；若仍硬编码仓根 ⇒ 索引文件缺失 ⇒ 红）
        _make_fixture(tmp, canonical=True)
        case("F8 canonical（`.rddf/` 托管）索引 ⇒ 绿", check(tmp) == 0)

        # ── F9：legacy 托管（仓根）⇒ 必须绿（向后兼容）─────────────
        _make_fixture(tmp, canonical=False)
        case("F9 legacy（仓根托管）索引 ⇒ 绿", check(tmp) == 0)

        # ── F10：canonical 索引误用仓根相对链接 ⇒ 必须红 ────────────
        #   （判别力 = 「链接基准 = 索引文件所在目录」；若基准仍用仓根 ⇒ 链接指向
        #    `.rddf/.rddf/improvements/…` ⇒ 不存在 ⇒ 红）
        _make_fixture(tmp, canonical=True)
        index_path(tmp).write_text(FIXTURE_INDEX_LEGACY, encoding="utf-8")
        case("F10 canonical 索引误用仓根相对链接 ⇒ 红", check(tmp) != 0)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    total = passes + fails
    if fails:
        _out("❌ self-test: %d/%d fixture 全绿" % (passes, total))
        return 1
    _out("✅ self-test: %d/%d fixture 全绿" % (passes, total))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=str(REPO))
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    repo = pathlib.Path(args.repo).resolve()

    if args.self_test:
        return selftest()
    if args.render:
        # ⚠️ **必须 `write` 而不是 `print`** ⇒ ⇒ `print` 会**追加**一个尾部换行
        #   ⇒ ⇒ 渲染结果比原文件多一行空行 ⇒ ⇒ `> file` 覆盖后**多出**一个空行
        #   ⇒ ⇒ 与「逐字一致」的判据冲突（实测 diff 报 `28d27`）。
        sys.stdout.write(render(repo))
        return 0
    if args.check:
        return check(repo)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
