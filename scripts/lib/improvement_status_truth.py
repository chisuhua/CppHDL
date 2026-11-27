#!/usr/bin/env python3
"""improvement_status_truth.py —— improvement 提案的**状态真实性**判据。

判据来源 = 2026-11-27 的治理审查（30 条待办里 **21 条是「已交付但状态未回填」的僵尸条目**
⇒ ⇒ `rddf status` 的待办池被噪声淹没）。本载体把那条不变式**机械化**。

四条判据（每条都有**反向判据**）：

  **C1 front matter 合法性**
    `.rddf/improvements/*.md` 的每一份 MUST 是**合法 YAML** front matter 且 `---` **闭合**。
    ⇒ 反向：任一份 YAML 破损 / 未闭合 ⇒ 红。
    （实测依据：2026-11-27 之前有 6 份破损 ⇒ `rddf status` 读不到其 priority/phase/status
      ⇒ 其中 1 份连优先级都显示 `-` ⇒ **元数据被静默吞掉**。）

  **C2 终态单调性（已交付 ⇒ 必须标终态）**
    「已交付」= ② 正文 `- **Status**` 行的**首 token**（去标记 ⇒ 按空白切第一个）
              **包含**已完标记 {已实施,已修复,已归档,已完成,已落地,已收口,已交付,已结案}
    ⇒ 已交付 ⇒ frontmatter `状态` 的首词 MUST ∈ {终态词} ∪ {`partial`}。
    ⇒ 反向：把已交付的条目改成 `draft` ⇒ 红。
    （⚠️ `partial` 放行：正文写「已实施」但明确标注「仅 X 未做」的条目属**部分完成**。）
    （⚠️ **刻意不用「同名归档 change 存在」当交付信号** ⇒ 实测有 **3 条反例**：
      `rtl-productization-surface-2026-11` / `soc-rtl-product-and-tb-2026-11` /
      `rv32i-hierarchical-example-2026-11` ⇒ 三者归档 change 的 task **全勾**、**同名**
      ⇒ 但提案**仍有残余工作**（⇒ 残余已登记进 front matter 的 `残余:` 字段）。
      ⇒ ⇒ 台账 = `.rddf/checks/improvement-body-vs-archive-drift-2026-11.md`。）
    （⚠️ **只判一个方向**：不判「标了终态但其实没做」⇒ 那要逐条核对 body 语义；
      反向矛盾**只判可机械判的部分** ⇒ 见 C4。）

  **C4 反向矛盾（正文未判已完 + 同名归档全勾 ⇒ 必须有交代）**
    条件 = 正文首 token **未**判已完
          ∧ `openspec/changes/archive/<date>-<name>/tasks.md` 存在且**任务全勾**
          ∧ front matter `状态:` **不是**终态词
    ⇒ ⇒ MUST 有非空 `残余:` 字段（声明剩余工作）⇒ 否则红。
    ⇒ 反向：补 `残余:` 或把状态转终态 ⇒ 绿。
    （⚠️ **边界**：只判「同名归档全勾」这一种强信号；归档 `tasks.md` 缺失 / 有未勾任务
      ⇒ **不判**（`全勾` 才是「机器写的交付事实」）。这与 C2 刻意**不**用归档当交付信号
      不矛盾：C2 是「归档 ⇒ 必须终态」的**充分**推断（已实测 3 条反例 ⇒ 不成立）；
      C4 是「正文未判已完 但归档全勾 ⇒ 两者矛盾，必须有人交代」，逃逸口 = `残余:`
      ⇒ ⇒ 不要求提案一定已交付，只要求矛盾**不静默**。）

  **C3 索引表链接可达**
    `.rddf/improvement-suggestions.md` 每一行的链接目标 MUST 存在。
    ⇒ 反向：改坏任一链接 ⇒ 红。
    （实测依据：索引表托管到 `.rddf/` 后其链接前缀改为相对索引文件
      ⇒ ⇒ 链接基准错了就会指向 `.rddf/.rddf/improvements/…`。）

⚠️ **依赖前提**（与 `verify_improvement_index.sh` 同款）：只依赖 **python3 + 仓内文件**；
  **不**依赖网络 / git 历史 / verilator / 编译器 / **仓外产品包** ⇒ ⇒ 仍是可达护栏。
"""
from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
IMPROVEMENT_DIR = ".rddf/improvements"
INDEX_NAME = "improvement-suggestions.md"
RDFF_DIR = ".rddf"

# ── C2 词表 ────────────────────────────────────────────────────────────────
# 终态词：与产品侧 `_lib/dashboard/improvement_status.py::_TERMINAL_MARKERS` 同口径
# （`已实施` / `已完成` / `已归档` / `已完成并归档` / `已实施并验收` / `implemented` /
#   `completed` / `archived` / `已作废` / `作废` / `superseded`）
TERMINAL_WORDS = (
    "已完成并归档", "已实施并验收", "已归档", "已完成", "已实施",
    "implemented", "completed", "archived", "已作废", "作废", "superseded",
)
# 非终态但**允许**的（部分完成）
PARTIAL_WORDS = ("partial",)
# 正文「已完」标记（对**首 token** 做**包含**判定 ⇒ 覆盖「只读盘点已交付 + 已归档」这类措辞；
#   ⚠️ **不**对整行做包含判定 ⇒ 否则 `partial （…已落地…）` 这类会被误判为已完）
BODY_DONE_WORDS = (
    "已实施", "已修复", "已归档", "已完成", "已落地", "已收口", "已交付", "已结案",
)

_MARKUP_RE = re.compile(r"[*`\"'“”‘’\s]+")
_LEADING_SYM_RE = re.compile(r"^[^\w一-鿿]+")
_BODY_STATUS_RE = re.compile(r"^\s*-\s+\*\*Status\*\*\s*[：:]\s*(.+)$", re.M)
_INDEX_ROW_RE = re.compile(r"^\|\s*\[([^\]]+)\]\(([^)]+)\)\s*\|", re.M)


def _out(msg: str) -> None:
    print(msg)


def _strip_markup(text: str) -> str:
    return _MARKUP_RE.sub(" ", text).strip()


def _leading_word(text: str) -> str:
    """取值的**首词**（去 markdown 标记 ⇒ 去前导符号 ⇒ 取首个 token）。"""
    s = _strip_markup(text)
    s = _LEADING_SYM_RE.sub("", s)
    m = re.match(r"[A-Za-z][A-Za-z0-9_-]*", s)
    if m:
        return m.group(0)
    m = re.match(r"[一-鿿]+", s)
    return m.group(0) if m else ""


def _front_matter(text: str):
    """⇒ (fm_text, body_text) ；未闭合 ⇒ (None, text)。"""
    if not text.startswith("---"):
        return None, text
    end = text.find("\n---", 4)
    if end == -1:
        return None, text
    return text[4:end], text[end + 4 :]


def _parse_front_matter(fm_text: str):
    """⇒ (meta_dict, broken_list)

    ⚠️ **整块 PyYAML 解析**（不是逐行切分）：
      · 逐行切分会把 `>-` 折叠块标量与其缩进续行**拆散** ⇒ 误判为破损；
      · 逐值 `safe_load("k: " + v)` 会遇到 **YAML 类型强制转换**
        （`2026-11-07` → date 对象 · `28` → int）⇒ 与原字符串比不相等 ⇒ 误判为破损。
      ⇒ ⇒ 故：**整块解析一次**，有异常 = 破损；无异常 = 逐键取值。
    """
    try:
        import yaml
    except ImportError:  # pragma: no cover - PyYAML 是声明依赖
        yaml = None

    if yaml is None:
        # 退化路径：逐行切分（不判严格性 ⇒ 宁可漏判不可误判）
        out: dict = {}
        for line in fm_text.split("\n"):
            if not line.strip() or line.startswith((" ", "\t")):
                continue
            if ":" not in line:
                continue
            k, _, v = line.partition(":")
            out[k.strip()] = v.strip()
        return out, []

    try:
        parsed = yaml.safe_load(fm_text)
    except Exception as exc:  # noqa: BLE001
        return {}, ["front matter 不是合法 YAML ⇒ %s" % str(exc).split("\n")[0][:90]]
    if not isinstance(parsed, dict):
        return {}, ["front matter 解析结果不是 mapping（实际 = %s）" % type(parsed).__name__]
    return {str(k): v for k, v in parsed.items()}, []


def _body_status_token(text: str) -> str:
    """正文 `- **Status**` 行的**首 token**（去标记 ⇒ 去前导符号 ⇒ 按空白切第一个）。

    ⚠️ 必须是**首 token** 而不是整行 ⇒ ⇒ 否则 `partial （…G3 + G1… 已落地…）` 会因
    行内出现「已落地」而被误判为「已交付」⇒ ⇒ 护栏误红。
    """
    m = _BODY_STATUS_RE.search(text)
    if not m:
        return ""
    s = _strip_markup(m.group(1))
    s = _LEADING_SYM_RE.sub("", s)
    parts = s.split()
    return parts[0] if parts else ""


def _index_path(repo: pathlib.Path) -> pathlib.Path:
    """索引表位置：`.rddf/` 优先（canonical）→ 仓根（legacy 兜底）。"""
    canonical = repo / RDFF_DIR / INDEX_NAME
    if canonical.is_file():
        return canonical
    return repo / INDEX_NAME


# ── C4：同名归档 change 的「机器交付事实」 ────────────────────────────────
# 归档目录名 = `<YYYY-MM-DD>-<change-name>` ⇒ ⇒ 去掉日期前缀即为提案名。
_ARCHIVE_DIR_RE = re.compile(r"^\d{4}-\d{2}-\d{2}-(.+)$")
_TASK_BOX_RE = re.compile(r"^\s*-\s*\[([ xX])\]")


def _archived_changes(repo: pathlib.Path) -> dict:
    """⇒ {change_name: [archive_dir, ...]}（同名多份时全部保留）。"""
    root = repo / "openspec" / "changes" / "archive"
    out: dict = {}
    if not root.is_dir():
        return out
    for p in sorted(root.iterdir()):
        if not p.is_dir():
            continue
        m = _ARCHIVE_DIR_RE.match(p.name)
        if m:
            out.setdefault(m.group(1), []).append(p)
    return out


def _tasks_all_checked(change_dir: pathlib.Path) -> bool:
    """归档 change 的 `tasks.md` 是否**存在且任务全勾**（至少 1 条）。

    ⚠️ 「全勾」才可当作机器交付事实：`tasks.md` 缺失 / 无任何 checkbox /
    有未勾项 ⇒ 一律 False（⇒ ⇒ C4 不判，避免把「没记录」当成「已交付」）。
    """
    tf = change_dir / "tasks.md"
    if not tf.is_file():
        return False
    boxes = 0
    unchecked = 0
    for line in tf.read_text(encoding="utf-8", errors="replace").splitlines():
        m = _TASK_BOX_RE.match(line)
        if not m:
            continue
        boxes += 1
        if m.group(1) == " ":
            unchecked += 1
    return boxes > 0 and unchecked == 0


def _has_residual(meta: dict) -> bool:
    """front matter 是否有**非空** `残余:` 字段。"""
    v = meta.get("残余")
    return bool(v) and bool(str(v).strip())


def collect(repo: pathlib.Path):
    """⇒ (violations, stats)"""
    viol: list = []
    imp_dir = repo / IMPROVEMENT_DIR
    if not imp_dir.is_dir():
        return [("C0", str(imp_dir), "improvement 目录不存在 ⇒ fail-closed")], {}

    stats = {"files": 0, "delivered": 0, "terminal": 0}
    # ⚠️ 「同名归档 change」**不**当 C2 的交付信号（见 C2 注释里的 3 条反例）
    #   ⇒ 但 C4 需要它判「正文未判已完 + 归档全勾」的反向矛盾（只判这一种强信号）。
    archived = _archived_changes(repo)

    for f in sorted(imp_dir.glob("*.md")):
        stats["files"] += 1
        text = f.read_text(encoding="utf-8", errors="replace")
        fm_text, body = _front_matter(text)
        if fm_text is None:
            viol.append(("C1", str(f.relative_to(repo)), "front matter 未闭合（缺独立的 `---`）"))
            continue
        meta, broken = _parse_front_matter(fm_text)
        for msg in broken:
            viol.append(("C1", str(f.relative_to(repo)), msg))
        if broken:
            continue

        status_raw = meta.get("状态") or meta.get("status") or ""
        if not isinstance(status_raw, str):
            status_raw = str(status_raw)
        status_word = _leading_word(status_raw)
        if status_word in TERMINAL_WORDS:
            stats["terminal"] += 1

        # ── C2：已交付 ⇒ 状态 MUST 是终态（或 partial）────────────────
        tok = _body_status_token(body)
        delivered_by_body = any(d in tok for d in BODY_DONE_WORDS)
        if delivered_by_body:
            stats["delivered"] += 1
            if status_word not in TERMINAL_WORDS and status_word not in PARTIAL_WORDS:
                viol.append(
                    (
                        "C2",
                        str(f.relative_to(repo)),
                        "已交付（正文 Status 首 token `%s`）⇒ ⇒ `状态:` MUST 是终态词或 `partial`，实际 = `%s`"
                        % (tok, status_raw or "(缺 `状态:` 字段)"),
                    )
                )

        # ── C4：正文未判已完 ∧ 同名归档全勾 ∧ 非终态 ⇒ MUST 有 `残余:` ──
        #   ⚠️ 逃逸口 = `残余:`（不是要求「必须已交付」）⇒ ⇒ 只要求矛盾**不静默**。
        if (
            not delivered_by_body
            and status_word not in TERMINAL_WORDS
            and not _has_residual(meta)
        ):
            full = [d for d in archived.get(f.stem, []) if _tasks_all_checked(d)]
            if full:
                viol.append(
                    (
                        "C4",
                        str(f.relative_to(repo)),
                        "正文 Status 未判已完（首 token `%s`）但同名归档 change 的 tasks 全勾"
                        "（`%s`）⇒ ⇒ `状态:` MUST 是终态词，或有非空 `残余:` 字段"
                        % (tok or "(无 Status 行)", full[0].name),
                    )
                )

    # ── C3：索引表链接可达 ────────────────────────────────────────────
    idx = _index_path(repo)
    if not idx.is_file():
        viol.append(("C3", str(idx.relative_to(repo)), "索引表缺失 ⇒ fail-closed"))
    else:
        itext = idx.read_text(encoding="utf-8", errors="replace")
        rows = list(_INDEX_ROW_RE.finditer(itext))
        if not rows:
            viol.append(("C3", str(idx.relative_to(repo)), "索引表里没有任何数据行"))
        for m in rows:
            target = (idx.parent / m.group(2)).resolve()
            try:
                target.relative_to(repo.resolve())
            except ValueError:
                viol.append(("C3", str(idx.relative_to(repo)), "链接路径逃出仓根（`%s`）" % m.group(2)))
                continue
            if not target.is_file():
                viol.append(
                    ("C3", str(idx.relative_to(repo)), "链接指向的文件不存在（`%s`）" % m.group(2))
                )

    return viol, stats


def check(repo: pathlib.Path) -> int:
    viol, stats = collect(repo)
    for rule, where, detail in viol:
        _out("::error::%s: %s — %s" % (rule, where, detail))
    if viol:
        _out("❌ improvement 状态真实性未通过（%d 项）" % len(viol))
        return 1
    _out(
        "✅ improvement 状态真实性通过（%d 份 · 已交付 %d · 终态 %d）"
        % (stats.get("files", 0), stats.get("delivered", 0), stats.get("terminal", 0))
    )
    return 0


# ── fixture 自检（**不依赖真实仓 / 网络 / git 历史 / verilator**）──────────

FIXTURE_A = """---
name: alpha
priority: P1
状态: implemented
状态依据: fixture
---

# alpha

- **Status**: ✅ **已实施**（fixture）
"""

FIXTURE_B = """---
name: beta
priority: P2
状态: draft
---

# beta

- **Status**: 📝 **草稿 v2**（待用户拍板）
"""

FIXTURE_BROKEN = """---
name: gamma
priority: P1
状态: **draft**
---

# gamma
"""

FIXTURE_INDEX = """# improvement-suggestions —— fixture

| 提案 | 优先级 | 来源 | 添加时间 | 状态 |
|---|---|---|---|---|
| [alpha](improvements/alpha.md) | P1 | 来源甲 | 2026-01-01 | ✅ **implemented** |
| [beta](improvements/beta.md) | P2 | 来源乙 | 2026-01-02 | 📝 **draft** |
"""


def _make_fixture(root: pathlib.Path) -> None:
    imp = root / IMPROVEMENT_DIR
    imp.mkdir(parents=True, exist_ok=True)
    # ⚠️ **必须清空** ⇒ ⇒ 否则上一条 case 留下的文件（如 F1 的破损 `gamma.md`）
    #   会让**后续每一条** fixture 都红 ⇒ ⇒ fixture 之间互相污染 ⇒ ⇒ 判别力失真
    for old in imp.glob("*.md"):
        old.unlink()
    # C4 的归档 fixture 同理：每条 case 从「无归档」开始
    shutil.rmtree(root / "openspec", ignore_errors=True)
    (root / RDFF_DIR / INDEX_NAME).write_text(FIXTURE_INDEX, encoding="utf-8")
    (imp / "alpha.md").write_text(FIXTURE_A, encoding="utf-8")
    (imp / "beta.md").write_text(FIXTURE_B, encoding="utf-8")


def _add_archive(root: pathlib.Path, name: str, *, checked: int = 1,
                 unchecked: int = 0, with_tasks: bool = True) -> None:
    """造一个 `<date>-<name>` 归档 change（C4 fixture 用）。"""
    d = root / "openspec" / "changes" / "archive" / ("2026-01-01-" + name)
    d.mkdir(parents=True, exist_ok=True)
    if not with_tasks:
        return
    lines = ["# Tasks: %s" % name, ""]
    lines += ["- [x] done %d" % i for i in range(checked)]
    lines += ["- [ ] todo %d" % i for i in range(unchecked)]
    (d / "tasks.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


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

    tmp = pathlib.Path(tempfile.mkdtemp(prefix="improve-truth-"))
    try:
        # ── F0 基线：未投毒 ⇒ 必须绿 ─────────────────────────────────
        _make_fixture(tmp)
        case("F0 基线：合规 fixture ⇒ 绿", check(tmp) == 0)

        # ── F1 C1：YAML 破损 ⇒ 必须红 ───────────────────────────────
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "gamma.md").write_text(FIXTURE_BROKEN, encoding="utf-8")
        case("F1 C1 YAML 破损 ⇒ 红", check(tmp) != 0)

        # ── F2 C1：front matter 未闭合 ⇒ 必须红 ─────────────────────
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "alpha.md").write_text(
            FIXTURE_A.replace("---\n\n# alpha", "\n# alpha"), encoding="utf-8"
        )
        case("F2 C1 front matter 未闭合 ⇒ 红", check(tmp) != 0)

        # ── F3 C2：正文已完但状态非终态 ⇒ 必须红 ───────────────────
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "alpha.md").write_text(
            FIXTURE_A.replace("状态: implemented", "状态: draft"), encoding="utf-8"
        )
        case("F3 C2 正文已完但状态非终态 ⇒ 红", check(tmp) != 0)

        # ── F4 C2：`partial` 放行（正文已完 + 部分完成）⇒ 必须绿 ───
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "alpha.md").write_text(
            FIXTURE_A.replace("状态: implemented", "状态: partial"), encoding="utf-8"
        )
        case("F4 C2 `partial` 放行 ⇒ 绿", check(tmp) == 0)

        # ── F5 C3：索引链接指向不存在的文件 ⇒ 必须红 ───────────────
        _make_fixture(tmp)
        (tmp / RDFF_DIR / INDEX_NAME).write_text(
            FIXTURE_INDEX.replace("improvements/alpha.md", "improvements/nope.md"),
            encoding="utf-8",
        )
        case("F5 C3 索引链接死链 ⇒ 红", check(tmp) != 0)

        # ── F6 边界（正向断言）：「标了终态但正文未判已完」⇒ **绿**
        #   ⇒ 刻意**不**判「标终态但其实没做」（要逐条核对 body 语义 ⇒ 本仓有 8 条
        #   **已登记**的「正文 vs 归档」矛盾项 ⇒ 强判会把护栏变成恒红）
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "beta.md").write_text(
            FIXTURE_B.replace("状态: draft", "状态: implemented"), encoding="utf-8"
        )
        case("F6 边界：标终态但正文未判已完 ⇒ 绿（刻意不判）", check(tmp) == 0)

        # ── F7 边界（正向断言）：正文首 token 之外的「已落地」**不算**已完 ──
        #   （`partial （…已落地…）` 不得被整行子串误判 ⇒ 护栏误红）
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "beta.md").write_text(
            FIXTURE_B.replace(
                "状态: draft",
                "状态: draft",
            ).replace(
                "- **Status**: 📝 **草稿 v2**（待用户拍板）",
                "- **Status**: partial （G3 已落地 · 仅特权级未做）",
            ),
            encoding="utf-8",
        )
        case("F7 边界：正文非首 token 的「已落地」不算已完 ⇒ 绿", check(tmp) == 0)

        # ── F8 还原后必须复绿 ───────────────────────────────────────
        _make_fixture(tmp)
        case("F8 还原后复绿", check(tmp) == 0)

        # ── C4 fixtures ─────────────────────────────────────────────
        # F9 C4：正文未判已完 + 同名归档全勾 + 无 残余 + 非终态 ⇒ 必须红
        _make_fixture(tmp)
        _add_archive(tmp, "beta", checked=2)
        case("F9 C4 正文未判已完 + 归档全勾 + 无残余 ⇒ 红", check(tmp) != 0)

        # F10 C4：同上 + 有 残余 ⇒ 必须绿（逃逸口生效）
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "beta.md").write_text(
            FIXTURE_B.replace("状态: draft\n---", "状态: draft\n残余: fixture 剩余工作\n---"),
            encoding="utf-8",
        )
        _add_archive(tmp, "beta", checked=2)
        case("F10 C4 有 残余 ⇒ 绿", check(tmp) == 0)

        # F11 C4：同上 + 状态已终态 ⇒ 必须绿
        _make_fixture(tmp)
        (tmp / IMPROVEMENT_DIR / "beta.md").write_text(
            FIXTURE_B.replace("状态: draft", "状态: implemented"), encoding="utf-8"
        )
        _add_archive(tmp, "beta", checked=2)
        case("F11 C4 状态已终态 ⇒ 绿", check(tmp) == 0)

        # F12 C4 边界：归档 tasks 有未勾项 ⇒ 不算「全勾」⇒ 必须绿
        _make_fixture(tmp)
        _add_archive(tmp, "beta", checked=2, unchecked=1)
        case("F12 C4 归档 tasks 非全勾 ⇒ 绿（边界）", check(tmp) == 0)

        # F13 C4 边界：归档目录存在但无 tasks.md ⇒ 不能当交付事实 ⇒ 必须绿
        _make_fixture(tmp)
        _add_archive(tmp, "beta", with_tasks=False)
        case("F13 C4 归档无 tasks.md ⇒ 绿（边界）", check(tmp) == 0)

        # F14 C4 边界：归档名不同（非同名）⇒ 必须绿
        _make_fixture(tmp)
        _add_archive(tmp, "gamma", checked=2)
        case("F14 C4 归档名不同 ⇒ 绿（边界）", check(tmp) == 0)
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
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    repo = pathlib.Path(args.repo).resolve()

    if args.self_test:
        return selftest()
    if args.check:
        return check(repo)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
