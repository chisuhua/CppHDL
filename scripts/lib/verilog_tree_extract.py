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

    verilog_tree_extract.py <path/to/Vxxx.tree.json> --ports [--json]
输出：`<module>\t<port_name>\t<INPUT|OUTPUT>\t<width>`（**只含真端口**）

⚠️ **端口提取同样是结构化提取**（`isPrimaryIO` + dtype 的 `range`）⇒ ⇒ 端口集合的
   判据 MUST 走这条路 ⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器。
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


def extract_module_ports(tree: dict) -> list[tuple[str, str, str, int]]:
    """产出 `(module, port_name, direction, width)` —— **每个 module 自己的端口声明**。

    ⚠️⚠️ **为什么不能只靠 `extract_ports()`（`isPrimaryIO`）**：
    verilator 的 `isPrimaryIO` 语义 = **顶层 primary IO** ⇒ ⇒ **非顶层 module 的
    端口 `isPrimaryIO` 是 `None`** ⇒ ⇒ 只用 `extract_ports()` **抓不到**非根 module
    的端口（实测：`synth_x_child` 的合成端口 `__x_0` 的 `isPrimaryIO=None`）
    ⇒ ⇒ 任何「非根 module 有没有合成端口」的判据若走 `--ports`，都会**恒绿**
    ⇒ ⇒ 那是**假绿**，不是「没发现问题」。

    ⇒ ⇒ 本函数改用 Verilog 语义的判据：**module 的一级 `stmtsp` 里 `type == 'VAR'`
    且 `direction ∈ {INPUT, OUTPUT}`** ⇒ ⇒ 精确对应 module 头的 `input`/`output`
    声明行 ⇒ ⇒ 体内 `wire` 的 `direction` 恒为 `'NONE'`（Verilog 里 wire 无方向）
    ⇒ ⇒ **不**会误抓体内线网。

    ⚠️ **只看一级 `stmtsp`（不递归）** ⇒ ⇒ CELL 的 `pinsp`（**实例端口映射**）里的
    端口名**不**在此列 ⇒ ⇒ 「根的实例映射里合法地出现 `.__x_0(...)`」不会被误判。

    ⚠️ 宽度取自 dtype 节点的 `range`（与 `extract_ports()` 同源 ⇒ ⇒ 同值）。
    """
    rows: list[tuple[str, str, str, int]] = []
    for module in tree.get("modulesp") or []:
        if not isinstance(module, dict):
            continue
        mod_name = module.get("name") or module.get("verilogName") or ""
        for node in module.get("stmtsp") or []:      # ⚠️ **一级**（不递归）
            if not isinstance(node, dict):
                continue
            if node.get("type") != "VAR":
                continue
            direction = node.get("direction") or ""
            if direction not in ("INPUT", "OUTPUT"):
                continue                              # 体内 wire = 'NONE'
            port = node.get("name") or node.get("verilogName") or ""
            if not port:
                continue
            rows.append((mod_name, port, direction,
                         _width_of(tree, node.get("dtypep") or "")))
    return rows


def extract_modules(tree: dict) -> list[str]:
    """产出**全部声明**的 module 名（按 `modulesp` 声明序）。

    ⚠️ **与 `extract_ports()` 的关键差别**：本函数**包含零端口 module**
    （`extract_ports()` 只返回带 `isPrimaryIO` 端口的 module ⇒ 对零端口 module
    有盲区）⇒ ⇒ 「哪个 module 是根」必须用本函数算声明集，
    否则「声明了但没端口」的 module 会从声明集里消失 ⇒ ⇒ 判根出错。

    ⚠️ **只取 `modulesp` 的一级项**（不递归）⇒ ⇒ 内联/嵌套 module 不在此列
    （它们不是独立的顶层声明）。
    """
    names: list[str] = []
    seen: set[str] = set()
    for module in tree.get("modulesp") or []:
        if not isinstance(module, dict):
            continue
        name = module.get("name") or module.get("verilogName") or ""
        if name and name not in seen:
            seen.add(name)
            names.append(name)
    return names


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


def _find_by_addr(tree: dict, addr: str) -> dict | None:
    """按 `addr` 找节点（深度优先）。"""
    if not addr or addr in UNLINKED:
        return None
    for node in _walk(tree):
        if node.get("addr") == addr:
            return node
    return None


def _width_of(tree: dict, dtypep: str) -> int:
    """从 dtype 节点的 `range`（如 `7:0`）算位宽；无 range ⇒ 1 位。"""
    dtype = _find_by_addr(tree, dtypep)
    if not isinstance(dtype, dict):
        return 1
    rng = dtype.get("range")
    if not isinstance(rng, str) or ":" not in rng:
        return 1
    msb_text, _, lsb_text = rng.partition(":")
    try:
        return int(msb_text) - int(lsb_text) + 1
    except ValueError:
        return 1


def extract_ports(tree: dict) -> list[tuple[str, str, str, int]]:
    """产出 `(module, port_name, direction, width)` 四元组（按 module 声明序）。

    ⚠️ **只取真端口**（`isPrimaryIO`）⇒ 体内线网（`direction == 'NONE'`）不算。
    ⚠️ 宽度取自 dtype 节点的 `range`（结构化提取 ⇒ MUST NOT 自研文本解析器）。
    """
    rows: list[tuple[str, str, str, int]] = []
    for module in tree.get("modulesp") or []:
        if not isinstance(module, dict):
            continue
        mod_name = module.get("name") or module.get("verilogName") or ""
        for node in _walk(module.get("stmtsp") or []):
            if node.get("type") != "VAR" or not node.get("isPrimaryIO"):
                continue
            port = node.get("name") or node.get("verilogName") or ""
            if not port:
                continue
            direction = node.get("direction") or ""
            rows.append((mod_name, port, direction, _width_of(tree, node.get("dtypep") or "")))
    return rows


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    as_json = "--json" in argv[2:]
    as_ports = "--ports" in argv[2:]
    as_modules = "--modules" in argv[2:]
    as_module_ports = "--module-ports" in argv[2:]

    with open(path, "r", encoding="utf-8") as handle:
        tree = json.load(handle)

    if as_module_ports:
        # ⚠️ 每个 module **自己的**端口声明（含非根）⇒ 「合成前缀只出现在非根」
        #    这类判据**必须**用它（`--ports` 的 `isPrimaryIO` 只标顶层 ⇒ 会恒绿）
        rows = extract_module_ports(tree)
        if as_json:
            print(json.dumps(
                [{"module": m, "port": p, "direction": d, "width": w}
                 for m, p, d, w in rows],
                ensure_ascii=False, indent=2))
        else:
            for mod_name, port, direction, width in rows:
                print(f"{mod_name}\t{port}\t{direction}\t{width}")
        return 0

    if as_modules:
        # ⚠️ **声明集**（含零端口 module）⇒ 「谁是根」的输入
        names = extract_modules(tree)
        if as_json:
            print(json.dumps({"modules": names}, ensure_ascii=False, indent=2))
        else:
            for mod_name in names:
                print(mod_name)
        return 0

    if as_ports:
        rows = extract_ports(tree)
        if as_json:
            print(json.dumps(
                [{"module": m, "port": p, "direction": d, "width": w}
                 for m, p, d, w in rows],
                ensure_ascii=False, indent=2))
        else:
            for mod_name, port, direction, width in rows:
                print(f"{mod_name}\t{port}\t{direction}\t{width}")
        return 0

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
