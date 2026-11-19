#!/usr/bin/env python3
"""openspec_purpose_guard.py —— openspec 主 spec 的 **Purpose 占位符** + **规范债登记**护栏。

为什么要有它（背景）：
  · `openspec validate --specs --strict`（CLI 1.14.3）新增了「Purpose 仍是占位符」这条规则
    ⇒ ⇒ 但 **CI 不跑 `openspec validate`** ⇒ ⇒ 且 CLI 的措辞随时会变 ⇒ ⇒ 不能只靠它。
  · 2026-11-19 的实测：8 个 capability 的 `## Purpose` 是 archive 自动写的占位符
    ⇒ ⇒ 同批复核发现 **19 条**过期 / 矛盾 / 未实现的 Requirement + **7 条**范围外观察
    ⇒ ⇒ 这些债**没有**任何机械登记处 ⇒ ⇒ 本护栏把两件事一起守住：
      R1（占位符集合 ⟷ 登记表 **双向**）· R2（债行完整性）· R3（字段/唯一性）
    ⇒ ⇒ 防的是「新建 capability 归档时又写一个占位符、且无人知晓」+「债被静默遗忘」。

判据（每条都可机械判）：
  R0  fail-closed：登记表缺失 / 扫不到任何 spec ⇒ exit 2（**禁恒 0**）
  R1  **双向**：扫描到的「Purpose 占位符 capability 集合」**必须等于**登记表中
      `kind=placeholder` 行的 capability 集合（两侧同为空 ⇒ 绿 —— 这是目标状态；
      一侧有一处而无登记 ⇒ 红；登记有行而无实际占位符 ⇒ 红）
  R2  债行完整性：每条 `kind=debt` / `kind=obs` 行 MUST 有 `owner`；`status=open` 行
      MUST 有 `trigger`；`location` 给出的**文件必须存在**且行号在范围内；
      `capability` 非 `-` 时对应 spec 目录 MUST 存在
  R3  字段与唯一性：`id` MUST 唯一；必填列 MUST 非空；`status` ∈ {open, closed}

用法：
  python3 scripts/lib/openspec_purpose_guard.py --check            # R0–R3（exit 0/1/2）
  python3 scripts/lib/openspec_purpose_guard.py --render            # 打印派生式登记表

约定：违规行输出 `::error::R<n> · <位置> · <详情>`（与 `docs_link_scope.py` 同形）
      ⇒ 薄壳 `scripts/verify_openspec_purpose.sh` 据此 grep 规则号（--self-test 投毒用）。
"""
from __future__ import annotations

import os
import re
import sys

REGISTRY = ".rddf/checks/openspec-spec-debt.tsv"
SPECS_DIR = "openspec/specs"

# archive 自动写的占位符句式（CLI 判定「Purpose 仍是占位符」的同款信号）
PLACEHOLDER_RE = re.compile(
    r"(TBD\s*-\s*created by archiving"
    r"|\bTBD\b"
    r"|\bTODO\b"
    r"|\bplaceholder\b"
    r"|created by archiving)",
    re.IGNORECASE,
)

REQUIRED_COLUMNS = ["kind", "id", "capability", "location", "category",
                    "suggestion", "owner", "trigger", "status", "evidence"]


def fail_closed(msg: str) -> int:
    print(f"::error::派生失败（fail-closed）：{msg}", file=sys.stderr)
    return 2


def read_registry(repo: str) -> list[dict]:
    path = os.path.join(repo, REGISTRY)
    if not os.path.isfile(path):
        raise FileNotFoundError(path)
    rows = []
    with open(path, encoding="utf-8") as fh:
        header = None
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip("\n")
            if not line.strip():
                continue
            if line.lstrip().startswith("#"):
                continue
            cells = line.split("\t")
            if header is None:
                header = [c.strip() for c in cells]
                missing = [c for c in REQUIRED_COLUMNS if c not in header]
                if missing:
                    raise ValueError(f"登记表缺列：{missing}")
                continue
            if len(cells) != len(header):
                raise ValueError(f"登记表第 {lineno} 行列数 {len(cells)} ≠ 表头 {len(header)}")
            row = dict(zip(header, [c.strip() for c in cells]))
            row["_lineno"] = lineno
            rows.append(row)
    return rows


def scan_placeholders(repo: str) -> tuple[set[str], int]:
    """返回（占位符 capability 集合, 扫到的 spec 文件数）。"""
    base = os.path.join(repo, SPECS_DIR)
    if not os.path.isdir(base):
        raise FileNotFoundError(base)
    hit: set[str] = set()
    scanned = 0
    for cap in sorted(os.listdir(base)):
        spec = os.path.join(base, cap, "spec.md")
        if not os.path.isfile(spec):
            continue
        scanned += 1
        text = open(spec, encoding="utf-8").read()
        m = re.search(r"^##\s+Purpose[ \t]*\n(.*?)(?=^##\s|\Z)", text, re.S | re.M)
        body = m.group(1) if m else ""
        if PLACEHOLDER_RE.search(body):
            hit.add(cap)
    return hit, scanned


def check(repo: str) -> int:
    try:
        rows = read_registry(repo)
    except FileNotFoundError as exc:
        return fail_closed(f"登记表不存在：{exc}")
    except ValueError as exc:
        return fail_closed(f"登记表格式错：{exc}")

    try:
        placeholders, scanned = scan_placeholders(repo)
    except FileNotFoundError as exc:
        return fail_closed(f"spec 目录不存在：{exc}")

    # ── R0：fail-closed（扫不到 spec ⇒ 判据本身坏了）
    if scanned == 0:
        return fail_closed(f"{SPECS_DIR} 下扫到 0 个 spec.md ⇒ 判据失效")

    errors: list[tuple[str, str, str]] = []   # (rule, where, detail)

    # ── R1：占位符集合 ⟷ 登记表（双向）
    registered = {r["capability"] for r in rows if r["kind"] == "placeholder"}
    for cap in sorted(placeholders - registered):
        errors.append(("R1", f"{SPECS_DIR}/{cap}/spec.md",
                       f"Purpose 仍是占位符，但登记表无 kind=placeholder 行 ⇒ 漏登记"))
    for cap in sorted(registered - placeholders):
        errors.append(("R1", f"{REGISTRY}:{cap}",
                       f"登记表声明 {cap} 是占位符，实际已不是 ⇒ 登记表漂移（须清行）"))

    # ── R2：债行完整性
    for r in rows:
        if r["kind"] not in ("debt", "obs", "placeholder"):
            continue
        if not r["owner"]:
            errors.append(("R2", f"{REGISTRY}:{r['_lineno']}", f"{r['id']} 缺 owner"))
        if r["status"] == "open" and not r["trigger"]:
            errors.append(("R2", f"{REGISTRY}:{r['_lineno']}", f"{r['id']} 缺 trigger"))
        if r["capability"] not in ("-", ""):
            if not os.path.isdir(os.path.join(repo, SPECS_DIR, r["capability"])):
                errors.append(("R2", f"{REGISTRY}:{r['_lineno']}",
                               f"{r['id']} 的 capability `{r['capability']}` 无对应 spec 目录"))
        loc = r["location"]
        if loc and loc != "-":
            m = re.match(r"^(.*?):(\d+)$", loc)
            if m:
                fpath, lineno = m.group(1), int(m.group(2))
                abs_p = os.path.join(repo, fpath)
                if not os.path.isfile(abs_p):
                    errors.append(("R2", f"{REGISTRY}:{r['_lineno']}",
                                   f"{r['id']} 的 location 文件不存在：{fpath}"))
                else:
                    total = sum(1 for _ in open(abs_p, encoding="utf-8", errors="ignore"))
                    if lineno < 1 or lineno > total:
                        errors.append(("R2", f"{REGISTRY}:{r['_lineno']}",
                                       f"{r['id']} 的 location 行号越界：{loc}（文件共 {total} 行）"))
            elif not os.path.exists(os.path.join(repo, loc)):
                errors.append(("R2", f"{REGISTRY}:{r['_lineno']}",
                               f"{r['id']} 的 location 不存在：{loc}"))

    # ── R3：字段与唯一性
    seen: dict[str, int] = {}
    for r in rows:
        rid = r["id"]
        if not rid:
            errors.append(("R3", f"{REGISTRY}:{r['_lineno']}", "id 为空"))
            continue
        if rid in seen:
            errors.append(("R3", f"{REGISTRY}:{r['_lineno']}",
                           f"id 重复：{rid}（首次出现于第 {seen[rid]} 行）"))
        seen[rid] = r["_lineno"]
        if r["status"] not in ("open", "closed"):
            errors.append(("R3", f"{REGISTRY}:{r['_lineno']}",
                           f"{rid} 的 status 非法：{r['status']}（允许 open / closed）"))
        for col in ("kind", "category", "suggestion", "evidence"):
            if not r[col]:
                errors.append(("R3", f"{REGISTRY}:{r['_lineno']}", f"{rid} 缺 {col}"))

    for rule, where, detail in errors:
        print(f"::error::{rule} · {where} · {detail}", file=sys.stderr)

    if errors:
        print(f"❌ openspec Purpose 护栏：{len(errors)} 处违规"
              f"（占位符 {len(placeholders)} 个 · 登记债 {len(rows)} 条）", file=sys.stderr)
        return 1
    print(f"✅ openspec Purpose 护栏：占位符 0 个 ⟷ 登记表一致 · "
          f"登记债 {len(rows)} 条完整性全绿（{scanned} 个 spec 已扫）")
    return 0


def render(repo: str) -> int:
    try:
        rows = read_registry(repo)
        placeholders, scanned = scan_placeholders(repo)
    except (FileNotFoundError, ValueError) as exc:
        return fail_closed(str(exc))
    print(f"# 派生：扫到 {scanned} 个 spec · 占位符 {len(placeholders)} 个 · 登记 {len(rows)} 条")
    print("| id | kind | capability | status | owner |")
    print("|---|---|---|---|---|")
    for r in rows:
        print(f"| {r['id']} | {r['kind']} | {r['capability']} | {r['status']} | {r['owner']} |")
    return 0


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    mode = "--render" if "--render" in sys.argv else "--check"
    repo = args[0] if args else os.getcwd()
    if mode == "--render":
        return render(repo)
    return check(repo)


if __name__ == "__main__":
    sys.exit(main())
