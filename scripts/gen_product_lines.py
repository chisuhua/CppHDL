#!/usr/bin/env python3
"""产品线登记面生成器（ADR-078 D8 · S4c-0）

**做什么**：从**每条产品线自带的 marker**（`examples/<line>/.rddf-line.yaml`）派生
`.rddf/checks/product-lines.md`（登记面）⇒ ⇒ 登记面是**派生物**，不是手写中央表
（理由见 `openspec/changes/rv32i-full-product-line-2026-11/design.md` §1）。

**为什么**：手写中央登记表在多会话并行下会冲突；纯自动发现会「静默不覆盖新线」⇒ ⇒
本仓最恐惧的失败族。派生物 + 元护栏（`scripts/verify_product_lines.sh`）两者都避开。

**用法**：
    scripts/gen_product_lines.py            # 写入登记面
    scripts/gen_product_lines.py --check    # 只比对（陈旧 => exit 1，CI 用）
    scripts/gen_product_lines.py --print    # 打到 stdout（调试用）

**零新依赖**：只用标准库 + `yaml`（PyYAML 已是仓内既有依赖 ——
`scripts/lib/guard_reachability.py` / `scripts/lib/improvement_status_truth.py` 在用）。
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover - 环境缺陷
    print("FATAL：缺 PyYAML（仓内既有依赖）⇒ pip install pyyaml", file=sys.stderr)
    sys.exit(2)

REPO_ROOT = Path(__file__).resolve().parent.parent
MARKER_NAME = ".rddf-line.yaml"
REGISTRY_REL = ".rddf/checks/product-lines.md"
SCOPE_TSV_REL = "scripts/product_line_scope.tsv"
AGENTS_REL = "AGENTS.md"

# 根 AGENTS.md 的 CI 锚点键（顺序 = 登记面里的展示顺序）
ANCHOR_KEYS = (
    "CTEST_TOTAL",
    "VERILATOR_E2E_TOTAL",
    "DUAL_BACKEND_PARITY_TOTAL",
    "DUAL_BACKEND_PARITY_PAIRS",
    "EXAMPLES_TEST_SOURCES",
)

# 「每个产品线成员目录」在 `examples/` 下的**层判定**：这些目录名是产品线成员，
#   但**不是**产品线锚点（锚点 = 带 marker 的那个目录）。
SCOPE_COLUMNS = ("script", "kind", "hardcoded", "class", "action", "grouped_when")


def find_markers() -> list[Path]:
    """发现集 = `examples/*/` 下所有 marker（**不带**递归 ⇒ 只认产品线锚点）。"""
    examples = REPO_ROOT / "examples"
    if not examples.is_dir():
        return []
    return sorted(p for p in examples.glob(f"*/{MARKER_NAME}") if p.is_file())


def load_marker(path: Path) -> dict:
    with path.open(encoding="utf-8") as fh:
        data = yaml.safe_load(fh)
    if not isinstance(data, dict):
        raise ValueError(f"{path}: marker MUST 是 mapping")
    return data


def read_scope_rows() -> list[dict]:
    """处置表 = `scripts/product_line_scope.tsv`（14 行 = 12 sh + 2 py）。"""
    tsv = REPO_ROOT / SCOPE_TSV_REL
    if not tsv.is_file():
        return []
    rows: list[dict] = []
    for raw in tsv.read_text(encoding="utf-8").splitlines():
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        cells = raw.split("\t")
        if len(cells) != len(SCOPE_COLUMNS):
            raise ValueError(f"{SCOPE_TSV_REL}: 列数 != {len(SCOPE_COLUMNS)}：{raw[:60]}")
        rows.append(dict(zip(SCOPE_COLUMNS, cells)))
    return rows


def read_anchors() -> list[tuple[str, str]]:
    """从根 `AGENTS.md` 的 CI anchor 段读锚点行（`- KEY: value`）。

    返回 [(key, raw_value)]；未登记的键不出现。
    ⚠️ **单值形态**（`CTEST_TOTAL: 285`）= **未归组**；**分组形态**
    （`CTEST_TOTAL: mini=285, full=1`）= 已归组。两种都容忍（归组是 T4.2 的动作）。
    """
    agents = REPO_ROOT / AGENTS_REL
    if not agents.is_file():
        return []
    out: list[tuple[str, str]] = []
    for line in agents.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^-\s+([A-Z0-9_]+):\s*(.+?)\s*$", line)
        if m and m.group(1) in ANCHOR_KEYS:
            out.append((m.group(1), m.group(2)))
    return out


def render(lines: list[dict], scope_rows: list[dict], anchors: list[tuple[str, str]]) -> str:
    out: list[str] = []
    out.append("# 产品线登记面（**派生物** —— 由 `scripts/gen_product_lines.py` 生成）")
    out.append("")
    out.append("> ⚠️ **本文件 MUST NOT 手工编辑** —— 它是每条产品线 marker 的**派生物**")
    out.append("> （marker = `examples/<line>/.rddf-line.yaml`，是产品线的**唯一真相**）。")
    out.append("> 手改 ⇒ `scripts/gen_product_lines.py --check` 报红 ⇒ 元护栏 C1 报红。")
    out.append(">")
    out.append("> **来源**：ADR-078（`docs/adr/ADR-078-dual-product-lines-and-full-line-scope.md`）")
    out.append("> 的 D8「产品线抽象」· S4c-0。")
    out.append("")
    out.append("## 1. 产品线表")
    out.append("")
    out.append("| 产品线 | 锚点目录（IP 级） | 成员目录 | 关联 ADR | 关联 capability |")
    out.append("|---|---|---|---|---|")
    for line in lines:
        members = "<br>".join(f"`{m}`" for m in line["members"])
        adrs = " ".join(line["adrs"]) or "—"
        specs = " ".join(f"`{s}`" for s in line["specs"]) or "—"
        out.append(
            f"| `{line['line']}` | `{line['anchor']}` | {members} | {adrs} | {specs} |"
        )
    out.append("")
    out.append("## 2. CI 锚点分组")
    out.append("")
    if not anchors:
        out.append("（根 `AGENTS.md` 的 CI anchor 段未读到 ⇒ 登记面为空）")
    else:
        out.append("| 锚点 | 值 | 归组形态 |")
        out.append("|---|---|---|")
        for key, raw in anchors:
            grouped = "已归组" if "=" in raw else "**未归组**（T4.2）"
            out.append(f"| `{key}` | `{raw}` | {grouped} |")
    out.append("")
    out.append("## 3. 脚本处置表")
    out.append("")
    out.append(f"**权威源** = `{SCOPE_TSV_REL}`（{len(scope_rows)} 行）⇒ 本节只是其**摘要视图**。")
    out.append("")
    if scope_rows:
        out.append("| 脚本 | 类 | S4c-0 的动作 | 何时才需按线分组 |")
        out.append("|---|---|---|---|")
        for row in scope_rows:
            out.append(
                f"| `{row['script']}` | **{row['class']}** | {row['action']} | {row['grouped_when']} |"
            )
        cls_a = sum(1 for r in scope_rows if r["class"] == "A")
        cls_b = sum(1 for r in scope_rows if r["class"] == "B")
        out.append("")
        out.append(f"⇒ **A 类 {cls_a} 个**（该线专属 · 只登记不改）· **B 类 {cls_b} 个**（MUST 参数化）")
    else:
        out.append(f"（`{SCOPE_TSV_REL}` 不存在或为空）")
    out.append("")
    return "\n".join(out)


def build_registry() -> str:
    lines: list[dict] = []
    for marker in find_markers():
        data = load_marker(marker)
        lines.append(
            {
                "line": data.get("line", ""),
                "anchor": str(marker.parent.relative_to(REPO_ROOT)),
                "members": list(data.get("members") or []),
                "adrs": list(data.get("adrs") or []),
                "specs": list(data.get("specs") or []),
            }
        )
    lines.sort(key=lambda x: x["line"])
    return render(lines, read_scope_rows(), read_anchors())


def main() -> int:
    ap = argparse.ArgumentParser(description="产品线登记面生成器（ADR-078 · S4c-0）")
    ap.add_argument("--check", action="store_true", help="只比对（陈旧 => exit 1）")
    ap.add_argument("--print", dest="dump", action="store_true", help="打到 stdout")
    args = ap.parse_args()

    try:
        content = build_registry()
    except (ValueError, KeyError) as exc:
        print(f"FATAL：生成失败：{exc}", file=sys.stderr)
        return 2

    if args.dump:
        sys.stdout.write(content)
        return 0

    target = REPO_ROOT / REGISTRY_REL
    if args.check:
        current = target.read_text(encoding="utf-8") if target.is_file() else ""
        if current != content:
            print(
                f"FATAL：登记面与 marker 派生的结果不一致（陈旧）⇒ {REGISTRY_REL}\n"
                f"       修复：scripts/gen_product_lines.py",
                file=sys.stderr,
            )
            return 1
        print(f"OK：登记面与 marker 一致（{REGISTRY_REL}）")
        return 0

    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")
    print(f"已生成 {REGISTRY_REL}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
