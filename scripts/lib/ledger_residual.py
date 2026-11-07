#!/usr/bin/env python3
"""残留登记表 ↔ 台账 §7 的派生护栏**分析器**（唯一判据实现）。

判据来源：openspec/changes/arch-residual-registry-2026-11/specs/ledger-residual-registry/spec.md
（5 Requirement）+ .rddf/improvements/arch-residual-registry-2026-11.md（12 行判据 · 11 投毒）。

子命令：check / render / check-normalization / selftest
退出码：0 绿 · 1 不一致 · 2 fail-closed（缺工具 · 缺文件 · 解析 0 行 · 登记表格式非法）
"""
from __future__ import annotations
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
REGISTRY_REL = ".rddf/checks/arch-residual-registry.md"
LEDGER_REL = "docs/architecture/2026-11-02-hierarchical-rtl-output.md"
LEDGER_SECTION = "7"
COLUMNS = ["台账", "section", "id", "状态", "日期", "后缀", "闭口证据", "复核条件", "归属"]
STATES = {"已收口", "已裁决", "不做", "已知", "未决"}
TERMINAL = {"已收口", "已裁决", "不做", "已知"}
EVIDENCE_REQUIRED = {"已收口", "已裁决"}
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
ID_RE = re.compile(r"^\d+b?$")
LEDGER_ROW_RE = re.compile(r"^\|\s*(\d+b?)\s*\|(.*)$")


class FailClosed(Exception):
    """fail-closed：环境缺失 / 文件缺失 / 解析 0 行 / 登记表格式非法 ⇒ exit 2。"""


def _cells(line: str) -> list[str]:
    """把 `| a | b |` 切成 ['a','b']；裸 `|` 会多出元素（⇒ 列数不符 ⇒ 调用方判红）。"""
    body = line.strip()
    if body.startswith("|"):
        body = body[1:]
    if body.endswith("|"):
        body = body[:-1]
    return [c.strip() for c in body.split("|")]


def parse_registry(path: pathlib.Path) -> list[dict]:
    """解析登记表 ⇒ 9 列的 dict 列表；表头不匹配 / 0 行 ⇒ FailClosed。"""
    if not path.exists():
        raise FailClosed(f"登记表不存在：{path}")
    lines = path.read_text(encoding="utf-8").splitlines()
    header_idx = None
    for i, l in enumerate(lines):
        if _cells(l) == COLUMNS:
            header_idx = i
            break
    if header_idx is None:
        raise FailClosed(f"登记表表头不匹配（期望 {COLUMNS}）：{path}")
    rows: list[dict] = []
    for l in lines[header_idx + 1:]:
        if not l.strip().startswith("|"):
            break
        cells = _cells(l)
        if len(cells) != len(COLUMNS):
            raise FailClosed(f"登记表列数 ≠ {len(COLUMNS)}：{l.strip()[:120]}")
        if all(set(c) <= set("-: ") for c in cells):   # 分隔行
            continue
        rows.append(dict(zip(COLUMNS, cells)))
    if not rows:
        raise FailClosed(f"登记表 0 行：{path}")
    return rows


def _parse_ledger_text(text: str, section: str, origin: str = "<text>") -> list[dict]:
    """★ 从台账**文本**解析 `## <section>.` 段的残留行（★ 与文件版共用同一套规则 ⇒ 口径唯一）。"""
    m = re.search(rf"^## {re.escape(section)}\..*?(?=^## |\Z)", text, re.MULTILINE | re.DOTALL)
    if not m:
        raise FailClosed(f"台账缺 ## {section}. 段：{origin}")
    rows: list[dict] = []
    for line in m.group(0).splitlines():
        lm = LEDGER_ROW_RE.match(line)
        if not lm:
            continue
        cells = _cells(line)
        if len(cells) != 3:
            raise FailClosed(f"台账 §{section} 行列数 ≠ 3（★ 状态格 MUST NOT 含裸 `|`）：{line.strip()[:120]}")
        rows.append({"id": lm.group(1), "desc": cells[1], "status": cells[2]})
    if not rows:
        raise FailClosed(f"台账 §{section} 解析出 0 行：{origin}")
    return rows


def parse_ledger_rows(path: pathlib.Path, section: str) -> list[dict]:
    """解析台账的 `## <section>.` 段里形如 `| <id> | 描述 | 状态格 |` 的行。

    ⇒ 只取 `#` 列标签形如 `1` / `1b` 的行（★ **不是**物理行号）。
    ⇒ 找不到该段 / 解析出 0 行 ⇒ FailClosed（★ 防「空集上恒真 ⇒ 全绿」）。
    """
    if not path.exists():
        raise FailClosed(f"台账不存在：{path}")
    return _parse_ledger_text(path.read_text(encoding="utf-8"), section, str(path))


_STATE_PREFIX = {
    "已收口": "✅ 已收口",
    "已裁决": "✅ 已裁决",
    "不做": "不做",
    "已知": "⚠️ 已知",
    "未决": "⚠️ 未决",
}


def gen_sentence(state: str, date: str, suffix: str) -> str:
    """★ 规范句 = f(状态, 日期, 后缀) **确定性生成**（无加粗 · 无反引号 · 不含证据路径）。

    ⇒ 终态（已收口 / 已裁决）⇒ `✅ <状态>（<日期> · <后缀>）`
    ⇒ 非终态（不做 / 已知 / 未决）⇒ `<前缀> ⇒ <后缀>`（后缀为空 ⇒ 只留前缀）
    """
    prefix = _STATE_PREFIX[state]
    if state in ("已收口", "已裁决"):
        return f"{prefix}（{date} · {suffix}）"
    if not suffix.strip():
        return prefix
    return f"{prefix} ⇒ {suffix}"


def sentence_matches(status_cell: str, sentence: str) -> bool:
    """★ 逐字节前缀比较（★ MUST NOT 做任何归一化 ⇒ 大小写 / 全角空格 / 空格数都算差异）。"""
    return status_cell.startswith(sentence)


def _key(row: dict) -> tuple[str, str, str]:
    return (row.get("台账", ""), row.get("section", ""), row.get("id", ""))


def r1_bidirectional(reg: list[dict], led: list[dict],
                     ledger_path: str = LEDGER_REL, section: str = LEDGER_SECTION) -> list[str]:
    """★ 双向 + **多重性敏感 1:1**（★ 堵「删一行 + 复制同 id 行」⇒ 计数守恒但语义换人）。

    ⇒ 登记表每个键恰好匹配 1 条台账行；台账每个 id 恰好匹配 1 条登记。
    ⇒ ★ 作用域 = (台账, section)：**其它台账 / 其它 section 的登记不在本判定内**
      （★ 机制按「多台账可扩展」设计 ⇒ 登记表里可能同时存在别的台账的残留行）。
    """
    problems: list[str] = []
    scope = [r for r in reg if _key(r)[:2] == (ledger_path, section)]
    reg_ids = [r["id"] for r in scope]
    led_ids = [r["id"] for r in led]
    for r in scope:
        n = led_ids.count(r["id"])
        if n == 0:
            problems.append(f"登记表有台账无：id={r['id']} section={r['section']}")
        elif n > 1:
            problems.append(f"台账中 id={r['id']} 出现 {n} 次（★ 1:1 多重性不符）")
    for lid in sorted(set(led_ids)):
        n = reg_ids.count(lid)
        if n == 0:
            problems.append(f"台账有登记表无：id={lid}")
        elif n > 1:
            problems.append(f"登记表中 id={lid} 出现 {n} 次（★ 1:1 多重性不符）")
    return problems


def _split_evidence(raw: str) -> list[str]:
    """★ 格式：`path1,path2`（半角逗号 · 无空格 · 无反引号 · 无空段 · 不重复）。"""
    raw = raw.strip()
    if not raw or raw == "—":
        return []
    return raw.split(",")


def r2_evidence(reg: list[dict], root: pathlib.Path) -> list[str]:
    """★ 登记了闭口证据的行 ⇒ 路径 MUST 存在；archive 类 MUST 含 `proposal.md`。"""
    problems: list[str] = []
    for r in reg:
        for p in _split_evidence(r.get("闭口证据", "")):
            if not (root / p).exists():
                problems.append(f"id={r['id']} 闭口证据不存在：{p}")
            elif "/archive/" in p and not (root / p / "proposal.md").exists():
                problems.append(f"id={r['id']} archive 类证据 MUST 含 proposal.md：{p}")
    return problems


def r2prime_candidate(reg: list[dict], root: pathlib.Path) -> list[str]:
    """★ `未决` 行登记的**候选闭口证据已存在** ⇒ 红（该行应转 `已收口`）。"""
    problems: list[str] = []
    for r in reg:
        if r.get("状态") != "未决":
            continue
        for p in _split_evidence(r.get("闭口证据", "")):
            if (root / p).exists():
                problems.append(f"id={r['id']} 登记为未决但候选闭口证据已存在（应转已收口）：{p}")
    return problems


def _is_empty(value: str) -> bool:
    """★ 「空」= 空串 **或** 占位符 `—`（★ 表内用 `—` 表示「无」⇒ MUST NOT 当成有效值）。"""
    return value.strip() in ("", "—")


def r10_registry_format(reg: list[dict]) -> list[str]:
    """★ 登记表**自身**格式非法 ⇒ fail-closed（调用方 ⇒ exit 2）。

    ⇒ 状态枚举外取值 / 日期非 YYYY-MM-DD（终态必填）/ 后缀缺失（终态）/ 闭口证据列格式违规 /
      id 非法或重复 / `未决` 行缺复核条件 / 「需证据的态」缺闭口证据。
    """
    problems: list[str] = []
    seen: set[str] = set()
    for r in reg:
        rid = r.get("id", "")
        if not ID_RE.match(rid):
            problems.append(f"id 非法：{rid!r}（★ 形如 1 / 1b）")
        if rid in seen:
            problems.append(f"id 重复：{rid}")
        seen.add(rid)
        state = r.get("状态", "")
        if state not in STATES:
            problems.append(f"id={rid} 状态非法：{state!r}（★ 枚举 {sorted(STATES)}）")
        date = r.get("日期", "")
        if state in TERMINAL:
            if not DATE_RE.match(date):
                problems.append(f"id={rid} 日期非法：{date!r}（★ YYYY-MM-DD）")
            if _is_empty(r.get("后缀", "")):
                problems.append(f"id={rid} 终态行的后缀 MUST 非空（★ 否则生成句会碰撞）")
        elif date and not DATE_RE.match(date):
            problems.append(f"id={rid} 日期非法：{date!r}（★ YYYY-MM-DD 或空）")
        raw = r.get("闭口证据", "").strip()
        if raw and raw != "—":
            if (" " in raw or "`" in raw or raw.endswith(",") or ",," in raw
                    or raw.startswith(",") or "，" in raw):
                problems.append(f"id={rid} 闭口证据列格式违规：{raw!r}（★ 半角逗号分隔 · 无空格 · 无反引号 · 无空段）")
            paths = _split_evidence(raw)
            if len(paths) != len(set(paths)):
                problems.append(f"id={rid} 闭口证据重复：{raw!r}")
            if state in EVIDENCE_REQUIRED and not paths:
                problems.append(f"id={rid} 「{state}」态 MUST 登记闭口证据")
        if state == "未决" and _is_empty(r.get("复核条件", "")):
            problems.append(f"id={rid} 「未决」行 MUST 登记复核条件")
    return problems


def cmd_render_rows(reg: list[dict]) -> list[str]:
    """★ 打印每行的**期望规范句**（★ 供 architect copy-paste ⇒ 降低「双份维护」成本）。"""
    return [f"id={r['id']} | {gen_sentence(r['状态'], r['日期'], r['后缀'])}" for r in reg]


def _prose_segments(status_cell: str) -> list[str]:
    """★ 散文段 = 状态格中**第一个 `⇒` 之后**的部分，再按 `⇒` 切成段（★ 段 = 删除粒度）。"""
    idx = status_cell.find("⇒")
    if idx < 0:
        return []
    tail = status_cell[idx:]
    return [s.strip() for s in tail.split("⇒") if s.strip()]


def removed_segments(old_cell: str, new_cell: str) -> list[str]:
    """★ 旧散文里**没有**出现在新散文里的段 = 「被删除的散文」（★ 须逐条登记 ⇒ 否则归一化丢信息）。"""
    new_set = set(_prose_segments(new_cell))
    return [s for s in _prose_segments(old_cell) if s not in new_set]


def cmd_check(root: pathlib.Path) -> int:
    """★ 主判定：双向 1:1 + 证据 + 候选证据 + 生成句前缀 + 生成句唯一 + 结算。"""
    reg = parse_registry(root / REGISTRY_REL)
    led = parse_ledger_rows(root / LEDGER_REL, LEDGER_SECTION)
    fmt = r10_registry_format(reg)
    if fmt:
        for m in fmt:
            print(f"FAIL-CLOSED 登记表格式非法：{m}", file=sys.stderr)
        return 2
    problems = r1_bidirectional(reg, led)
    problems += r2_evidence(reg, root)
    problems += r2prime_candidate(reg, root)
    seen_sent: dict[str, str] = {}
    for r in reg:
        s = gen_sentence(r["状态"], r["日期"], r["后缀"])
        if s in seen_sent:
            problems.append(f"id={r['id']} 生成句与 id={seen_sent[s]} 相同（★ MUST 唯一）")
        seen_sent[s] = r["id"]
    by_id = {r["id"]: r for r in led}
    for r in reg:
        row = by_id.get(r["id"])
        if row is None:
            continue
        s = gen_sentence(r["状态"], r["日期"], r["后缀"])
        hit = sentence_matches(row["status"], s)
        if not hit:
            problems.append(f"id={r['id']} 台账状态格未以生成句开头：期望 {s!r}")
        print(f"  id={r['id']:<3} 状态={r['状态']:<4} 证据数={len(_split_evidence(r.get('闭口证据', '')))} "
              f"命中={'OK' if hit else 'MISS'}")
    if len(reg) != len(led):
        problems.append(f"结算行数不一致：登记表 {len(reg)} ≠ 台账 {len(led)}")
    if problems:
        for p in problems:
            print(f"FAIL {p}", file=sys.stderr)
        return 1
    print(f"PASS 残留登记一致性（{len(reg)} 行）")
    return 0


def cmd_render(root: pathlib.Path) -> int:
    reg = parse_registry(root / REGISTRY_REL)
    for line in cmd_render_rows(reg):
        print(line)
    return 0


def cmd_check_normalization(root: pathlib.Path, base: str = "HEAD",
                            review: pathlib.Path | None = None) -> int:
    """★ 归一化「信息不丢」核对：旧散文段 ⊆ 新散文 ⇒ 被删的段 MUST 逐条登记。

    ⇒ 旧台账从 `git show <base>:<台账路径>` 取（★ 默认 HEAD）⇒ 新台账取工作区。
    ⇒ `--review <file>`：architect 的 review 记录 ⇒ 每个被删段 MUST 在其中出现。
    """
    import subprocess
    ledger_rel = LEDGER_REL
    try:
        old_text = subprocess.run(["git", "show", f"{base}:{ledger_rel}"], cwd=root,
                                  capture_output=True, text=True, check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        raise FailClosed(f"取不到 {base}:{ledger_rel}（★ 归一化核对需要旧版本）：{e}")
    old_rows = {r["id"]: r for r in _parse_ledger_text(old_text, LEDGER_SECTION)}
    new_rows = {r["id"]: r for r in parse_ledger_rows(root / LEDGER_REL, LEDGER_SECTION)}
    review_text = review.read_text(encoding="utf-8") if review and review.exists() else ""
    problems: list[str] = []
    total_removed = 0
    for rid, old_row in old_rows.items():
        new_row = new_rows.get(rid)
        if new_row is None:
            problems.append(f"id={rid} 新台账缺该行（★ 归一化 MUST NOT 删行）")
            continue
        removed = removed_segments(old_row["status"], new_row["status"])
        total_removed += len(removed)
        for seg in removed:
            registered = seg in review_text
            print(f"  id={rid:<3} 被删散文段：{seg[:60]!r} ⇒ {'已登记' if registered else '★ 未登记'}")
            if review is not None and not registered:
                problems.append(f"id={rid} 被删散文段未在 review 记录中登记：{seg[:60]!r}")
    print(f"归一化核对：{len(old_rows)} 行 · 被删散文段 {total_removed} 个"
          + (f" · review={review}" if review else " · （未给 --review ⇒ 只报告，不判不合格）"))
    if problems:
        for p in problems:
            print(f"FAIL {p}", file=sys.stderr)
        return 1
    return 0


POISONS = [
    # (编号, 描述, 投毒函数名, 期望退出码)
    ("1", "台账 §7 多一行（登记表无）", "p_ledger_extra_row", 1),
    ("2", "登记表多一行（台账无）", "p_registry_extra_row", 1),
    ("3", "台账生成句改 1 个字", "p_sentence_typo", 1),
    ("4", "行 11 的证据路径改错", "p_evidence_wrong", 1),
    ("5", "登记表状态标为未决而台账前缀是已收口", "p_state_mismatch", 1),
    ("6", "§7 标题被改名 ⇒ 解析 0 行", "p_section_renamed", 2),
    ("7", "台账删一行 + 复制同 id 行（计数守恒）", "p_duplicate_id", 1),
    ("8", "台账散文改 1 个字 ⇒ 必须仍绿", "p_prose_edit", 0),
    ("9", "未决行登记一个已存在的候选证据", "p_candidate_exists", 1),
    ("10", "登记表状态取枚举外值", "p_bad_state", 2),
    ("11", "未决行缺复核条件", "p_missing_review", 2),
]

_SYNTH_LEDGER_ROWS = (
    "| 1 | 描述一 | ✅ 已收口（2026-11-07 · S1）⇒ ⇒ 散文段 |\n"
    "| 2 | 描述二 | ⚠️ 未决 ⇒ S2 |\n"
)
_SYNTH_REGISTRY_ROWS = (
    f"| {LEDGER_REL} | 7 | 1 | 已收口 | 2026-11-07 | S1 | docs/adr/ADR-008-single-clock-domain-constraint.md | — | — |\n"
    f"| {LEDGER_REL} | 7 | 2 | 未决 |  | S2 | — | 等 planner 立项 | builder |\n"
)


def _synthetic_root(base: pathlib.Path) -> pathlib.Path:
    """★ 合成 fixture：合成台账 + 合成登记表（★ **不碰** docs/architecture/** 与真实登记表）。"""
    root = base / "root"
    (root / ".rddf" / "checks").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "architecture").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "adr").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "adr" / "ADR-008-single-clock-domain-constraint.md").write_text(
        "# ADR-008（合成 fixture）\n", encoding="utf-8")
    (root / ".rddf" / "checks" / "arch-residual-registry.md").write_text(
        "| " + " | ".join(COLUMNS) + " |\n"
        + "|" + "|".join(["---"] * len(COLUMNS)) + "|\n"
        + _SYNTH_REGISTRY_ROWS, encoding="utf-8")
    (root / LEDGER_REL).write_text(
        "## 7. 已知残留\n\n"
        "| # | 残留 | 状态 / 归属 |\n|---|---|---|\n"
        + _SYNTH_LEDGER_ROWS, encoding="utf-8")
    return root


def _patch(root: pathlib.Path, rel: str, old: str, new: str) -> None:
    p = root / rel
    text = p.read_text(encoding="utf-8")
    if old not in text:
        raise FailClosed(f"投毒锚点不存在：{rel} :: {old!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


def p_ledger_extra_row(root: pathlib.Path) -> None:
    _patch(root, LEDGER_REL, _SYNTH_LEDGER_ROWS,
           _SYNTH_LEDGER_ROWS + "| 3 | 描述三 | ⚠️ 未决 ⇒ S3 |\n")


def p_registry_extra_row(root: pathlib.Path) -> None:
    _patch(root, ".rddf/checks/arch-residual-registry.md", _SYNTH_REGISTRY_ROWS,
           _SYNTH_REGISTRY_ROWS
           + f"| {LEDGER_REL} | 7 | 3 | 未决 |  | S3 | — | 等 planner 立项 | builder |\n")


def p_sentence_typo(root: pathlib.Path) -> None:
    _patch(root, LEDGER_REL, "✅ 已收口（2026-11-07 · S1）", "✅ 已收口（2026-11-07 · S2）")


def p_evidence_wrong(root: pathlib.Path) -> None:
    _patch(root, ".rddf/checks/arch-residual-registry.md",
           "docs/adr/ADR-008-single-clock-domain-constraint.md | — | — |",
           "docs/adr/ADR-NOPE.md | — | — |")


def p_state_mismatch(root: pathlib.Path) -> None:
    """★ 状态标为未决（★ 同时补复核条件 ⇒ 格式合法 ⇒ ⇒ 只让判据 ③ 转红 ⇒ exit 1）。"""
    _patch(root, ".rddf/checks/arch-residual-registry.md",
           "| 1 | 已收口 | 2026-11-07 | S1 |",
           "| 1 | 未决 | 2026-11-07 | S1 |")
    _patch(root, ".rddf/checks/arch-residual-registry.md",
           "docs/adr/ADR-008-single-clock-domain-constraint.md | — | — |",
           "docs/adr/ADR-008-single-clock-domain-constraint.md | 等 planner 立项 | builder |")


def p_section_renamed(root: pathlib.Path) -> None:
    _patch(root, LEDGER_REL, "## 7. 已知残留", "## 七. 已知残留")


def p_duplicate_id(root: pathlib.Path) -> None:
    """★ 删一行 + 复制同 id 行（★ 计数守恒、集合守恒 ⇒ 只能靠 1:1 多重性抓）。"""
    _patch(root, LEDGER_REL, _SYNTH_LEDGER_ROWS,
           "| 1 | 描述一 | ✅ 已收口（2026-11-07 · S1）⇒ ⇒ 散文段 |\n"
           "| 1 | 描述一 | ✅ 已收口（2026-11-07 · S1）⇒ ⇒ 散文段 |\n")


def p_prose_edit(root: pathlib.Path) -> None:
    """★ 只改散文（★ 生成句前缀不变 ⇒ ⇒ MUST 仍绿 ⇒ 反向判据）。"""
    _patch(root, LEDGER_REL, "⇒ ⇒ 散文段", "⇒ ⇒ 散文段（改了一个字）")


def p_candidate_exists(root: pathlib.Path) -> None:
    """★ 未决行登记一个**已存在**的候选闭口证据 ⇒ 判据 ②' 转红。"""
    _patch(root, ".rddf/checks/arch-residual-registry.md",
           "| 2 | 未决 |  | S2 | — | 等 planner 立项 | builder |",
           "| 2 | 未决 |  | S2 | docs/adr/ADR-008-single-clock-domain-constraint.md | 等 planner 立项 | builder |")


def p_bad_state(root: pathlib.Path) -> None:
    _patch(root, ".rddf/checks/arch-residual-registry.md",
           "| 1 | 已收口 | 2026-11-07 | S1 |", "| 1 | 已收囗 | 2026-11-07 | S1 |")


def p_missing_review(root: pathlib.Path) -> None:
    _patch(root, ".rddf/checks/arch-residual-registry.md",
           "| 等 planner 立项 | builder |", "| — | builder |")


def selftest() -> int:
    """★ 合成 fixture + 11 条投毒（★ 每条 MUST 让 check() 转红 / 保持绿 ⇒ 否则判据无判别力）。

    ⇒ 每次投毒在**副本**上做 ⇒ 用完丢弃 ⇒ ★ 真实文件零污染（★ 「自检后工作树干净」AC）。
    """
    import shutil
    import tempfile
    failures = 0
    # ★ 临时目录用 `tempfile.gettempdir()`（尊重 TMPDIR）⇒ 对齐仓内护栏的 `mktemp -d` 惯例
    #   （★ 早期实现硬编码 `/tmp/opencode` ⇒ 那是**agent 环境**的习惯 ⇒ 仓内脚本不该带）
    tmpdir = tempfile.gettempdir()
    with tempfile.TemporaryDirectory(prefix="lr-selftest-", dir=tmpdir) as td:
        tmp = pathlib.Path(td)
        pristine = _synthetic_root(tmp / "pristine")
        before = {p: p.read_bytes() for p in pristine.rglob("*") if p.is_file()}
        for pid, desc, fn_name, expected in POISONS:
            work = tmp / f"case-{pid}"
            if work.exists():
                shutil.rmtree(work)
            shutil.copytree(pristine, work)
            globals()[fn_name](work)
            try:
                rc = cmd_check(work)
            except FailClosed:
                rc = 2
            ok = (rc == expected)
            print(f"  [{'OK ' if ok else 'BAD'}] 投毒 {pid}：{desc} ⇒ exit={rc}（期望 {expected}）")
            if not ok:
                failures += 1
            for p, data in before.items():
                if not p.exists() or p.read_bytes() != data:
                    print(f"  [BAD] 投毒 {pid} 污染了 pristine fixture（★ MUST NOT）")
                    failures += 1
                    break
    print(f"自检：{len(POISONS)} 条投毒，失败 {failures}")
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    if len(argv) < 2 or argv[1] in ("-h", "--help"):
        print(__doc__)
        return 0
    cmd = argv[1]
    root = ROOT
    base = "HEAD"
    review: pathlib.Path | None = None
    args = argv[2:]
    i = 0
    while i < len(args):
        if args[i] == "--root":
            root = pathlib.Path(args[i + 1]).resolve()
            i += 2
        elif args[i] == "--base":
            base = args[i + 1]
            i += 2
        elif args[i] == "--review":
            review = pathlib.Path(args[i + 1])
            i += 2
        else:
            raise FailClosed(f"未知参数：{args[i]}")
    if cmd == "check":
        return cmd_check(root)
    if cmd == "render":
        return cmd_render(root)
    if cmd == "check-normalization":
        return cmd_check_normalization(root, base, review)
    if cmd == "selftest":
        return selftest()
    raise FailClosed(f"未实现的子命令：{cmd}")


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except FailClosed as e:
        print(f"FAIL-CLOSED: {e}", file=sys.stderr)
        sys.exit(2)
