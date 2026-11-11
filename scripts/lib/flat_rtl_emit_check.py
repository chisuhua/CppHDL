#!/usr/bin/env python3
"""扁平 RTL 产物的**产物级**检查（`rtl-emit-product-tooling` **C5** 的分析器）。

⚠️ **它做什么**：
读「发射产物 `.v`」+「端口清单 `manifest.json`」⇒ ⇒ 做三件事：

* `--strip-lint-off`  ⇒ 剥掉产物里的**blanket `lint_off`** ⇒ 供外部 lint 重跑
  （⚠️ 剥的是**整份产物**里的 `/* verilator lint_off X */` ⇒ ⇒ **不改**其它内容）。
* `--top-ports`      ⇒ 提取**顶层 module 的端口表**（按产物里的**顺序**）。
* `--check-top-ports` ⇒ 校验「产物顶层端口集 **==** 清单算出的集合」
  ⇒ ⇒ 集合的定义**就是**契约（`rtl-hierarchical-output`）：
  「顶层端口表 MUST 是 **clock → reset → 无 driver 的 input → 全部 `ch_out`**」
  ⇒ ⇒ 即：`{role ∈ {clock, reset}} ∪ {dir == output} ∪ {dir == input ∧ driven == false}`。
* `--self-assign`    ⇒ 列出产物里的**自赋值**死逻辑（`assign X = X;`）⇒ ⇒ 应为 0。

⚠️ **为什么端口表用「位置 + 集合」两查**：
契约给了**顺序**（clock → reset → input → output）⇒ ⇒ 顺序本身也是契约的一部分
⇒ ⇒ `--top-ports` 保留顺序（供人核对）· `--check-top-ports` 比**集合**（防顺序噪声）。

用法：
    python3 scripts/lib/flat_rtl_emit_check.py --strip-lint-off <in.v> <out.v>
    python3 scripts/lib/flat_rtl_emit_check.py --top-ports <in.v>
    python3 scripts/lib/flat_rtl_emit_check.py --check-top-ports <in.v> <manifest.json>
    python3 scripts/lib/flat_rtl_emit_check.py --self-assign <in.v>
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

LINT_OFF_RE = re.compile(r"/\*\s*verilator\s+lint_off\s+([A-Za-z0-9_]+)\s*\*/")
MODULE_RE = re.compile(r"^\s*module\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(", re.M)
# 顶层 module 的端口声明行：`input  name,` / `output [7:0] name` / `input name`
PORT_LINE_RE = re.compile(
    r"^\s*(input|output|inout)\s*(?:\[[^\]]*\]\s*)?([A-Za-z_][A-Za-z0-9_]*)\s*[,)]?\s*$"
)
ASSIGN_RE = re.compile(r"^\s*assign\s+([A-Za-z_][A-Za-z0-9_$]*)\s*=\s*([A-Za-z_][A-Za-z0-9_$]*)\s*;", re.M)


def strip_lint_off(text: str) -> str:
    """剥掉产物里的 `/* verilator lint_off X */`（**逐字**删除该注释行）。"""
    return LINT_OFF_RE.sub("", text)


def top_module_ports(text: str) -> list[tuple[str, str]]:
    """提取顶层 module 的端口表 ⇒ 返回 `[(dir, name), ...]`（**按产物顺序**）。"""
    m = MODULE_RE.search(text)
    if not m:
        return []
    start = text.index("(", m.start())
    depth = 0
    end = -1
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                end = i
                break
    if end < 0:
        return []
    body = text[start + 1:end]
    ports: list[tuple[str, str]] = []
    for raw in body.split(","):
        raw = raw.strip()
        if not raw:
            continue
        pm = PORT_LINE_RE.match(raw)
        if not pm:
            continue
        ports.append((pm.group(1), pm.group(2)))
    return ports


def manifest_expected_top_ports(manifest: dict) -> set[str]:
    """按**契约**从清单算出顶层端口集。"""
    out: set[str] = set()
    for p in manifest.get("ports", []):
        role = p.get("role")
        if role in ("clock", "reset"):
            out.add(p["name"])
            continue
        if p.get("dir") == "output":
            out.add(p["name"])
            continue
        if p.get("dir") == "input" and not p.get("driven", False):
            out.add(p["name"])
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--strip-lint-off", nargs=2, metavar=("IN", "OUT"))
    ap.add_argument("--top-ports")
    ap.add_argument("--check-top-ports", nargs=2, metavar=("V", "MANIFEST"))
    ap.add_argument("--self-assign")
    args = ap.parse_args()

    if args.strip_lint_off:
        src, dst = args.strip_lint_off
        text = pathlib.Path(src).read_text(encoding="utf-8", errors="replace")
        pathlib.Path(dst).write_text(strip_lint_off(text), encoding="utf-8")
        return 0

    if args.top_ports:
        text = pathlib.Path(args.top_ports).read_text(encoding="utf-8", errors="replace")
        for d, n in top_module_ports(text):
            print(f"{d} {n}")
        return 0

    if args.check_top_ports:
        vpath, mpath = args.check_top_ports
        text = pathlib.Path(vpath).read_text(encoding="utf-8", errors="replace")
        manifest = json.loads(pathlib.Path(mpath).read_text(encoding="utf-8"))
        actual = {n for _, n in top_module_ports(text)}
        expected = manifest_expected_top_ports(manifest)
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        if missing:
            print(f"::error::清单算出但产物顶层端口表里没有：{missing}")
        if extra:
            print(f"::error::产物顶层端口表里有但清单算不出：{extra}")
        if missing or extra:
            return 1
        print(f"OK：顶层端口集 == 清单（{len(actual)} 个）")
        return 0

    if args.self_assign:
        text = pathlib.Path(args.self_assign).read_text(encoding="utf-8", errors="replace")
        hits = sorted({lhs for lhs, rhs in ASSIGN_RE.findall(text) if lhs == rhs})
        for h in hits:
            print(f"::error::自赋值死逻辑：assign {h} = {h};")
        if hits:
            return 1
        print("OK：无自赋值死逻辑")
        return 0

    print("::error 需要一个模式参数", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
