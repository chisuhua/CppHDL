#!/usr/bin/env python3
"""从 verilator 的 `--json-only` 产物里提取 **module 树**（层级实例关系）。

用途（层次化 RTL 输出方案 · C0-0 spike 的产物）：
「结构同构」判据 MUST 能**独立于 emitter** 地读出 RTL 的 module 树 ⇒
否则判据自证（分析层算错 ⇒ 两边一起错 ⇒ 「计数对但树形错」永远测不出）。

⚠️ **为什么用 `--json-only` 而不是 `--xml-only`**：
仓内钉的 verilator 是 **5.048**（`third_party/verilator/configure.ac:15`），
其 `Changes:228` 明确「Remove deprecated `--xml-only`」⇒ ⇒ 该选项**在本仓不存在**。

提取机制（**实测**，见 `docs/architecture/2026-11-02-hierarchical-rtl-output.md`）：
  1. `.tree.json` 的 `modulesp` 是**模块列表**（不是单指针）；
  2. 每个模块项带 `addr`（如 `'(M)'`）与 `name`；
  3. 每个模块的 `stmtsp` 里 `type == 'CELL'` 的项 = 一个**实例**，带
     `name`（实例名）与 `modp`（指向被实例化模块的 `addr`）；
  4. ⇒ ⇒ **join = 用 `modp` 在 `modulesp` 的 `addr → name` 表里查**。

用法：
    verilog_tree_extract.py <path/to/Vxxx.tree.json> [--json]
输出（默认 TSV，每行一条）：`<parent_module>\t<inst_name>\t<module_name>`
"""

from __future__ import annotations

import json
import sys
from typing import Any, Iterator

UNLINKED = {"UNLINKED", "", None}


def _walk(node: Any) -> Iterator[dict]:
    """递归遍历 JSON 树，产出所有 dict 节点。"""
    if isinstance(node, dict):
        yield node
        for value in node.values():
            yield from _walk(value)
    elif isinstance(node, list):
        for item in node:
            yield from _walk(item)


def _module_addr_table(tree: dict) -> dict[str, str]:
    """`addr → module name`（`modulesp` 是列表，每项带 `addr` 与 `name`）。"""
    table: dict[str, str] = {}
    for module in tree.get("modulesp") or []:
        if not isinstance(module, dict):
            continue
        addr = module.get("addr")
        name = module.get("name") or module.get("verilogName")
        if isinstance(addr, str) and name:
            table[addr] = name
    return table


def extract(tree: dict) -> list[tuple[str, str, str]]:
    """产出 `(parent_module, inst_name, module_name)` 三元组（按父模块名排序）。"""
    addr_to_name = _module_addr_table(tree)
    rows: list[tuple[str, str, str]] = []

    for module in tree.get("modulesp") or []:
        if not isinstance(module, dict):
            continue
        parent = module.get("name") or module.get("verilogName") or ""
        for node in _walk(module.get("stmtsp") or []):
            if node.get("type") != "CELL":
                continue
            inst = node.get("name") or node.get("verilogName") or ""
            modp = node.get("modp")
            if modp in UNLINKED:
                # 未链接的 cell ⇒ 没有可判定的模块名 ⇒ 跳过（**不**猜）
                continue
            target = addr_to_name.get(modp)
            if target is None:
                continue
            rows.append((parent, inst, target))

    rows.sort(key=lambda r: (r[0], r[1]))
    return rows


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    as_json = "--json" in argv[2:]

    with open(path, "r", encoding="utf-8") as handle:
        tree = json.load(handle)

    rows = extract(tree)
    if as_json:
        print(json.dumps(
            [{"parent": p, "instance": i, "module": m} for p, i, m in rows],
            ensure_ascii=False, indent=2))
    else:
        for parent, inst, module in rows:
            print(f"{parent}\t{inst}\t{module}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
