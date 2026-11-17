#!/usr/bin/env python3
"""scripts/lib/verif_tb_shape_check.py

**「TB 由 manifest 生成」的形态判据**（`rtl-verification-single-source` · ADR-074）。

判据（两条，缺一即 fail）：
  ① DUT 实例的**连线数** == `manifest.port_table[]` 的条目数
  ② 每一条连线的**端口名** MUST ∈ `port_table[]`

⚠️ 为什么两条都要：
  · 只判 ①（条数）⇒ ⇒ 连错名字（`awaddr` ↔ `wdata` 互换）**照样通过**
  · 只判 ②（集合）⇒ ⇒ 少连几根、把剩下的连对 ⇒ ⇒ 也会通过
  ⇒ ⇒ 合起来才排除「手写映射表残留」与「什么都没连」两种退化。

用法：
  verif_tb_shape_check.py <tb.sv> <manifest.json>          # 全部判据
  verif_tb_shape_check.py <tb.sv> <manifest.json> join     # 只判「顶层端口身份 1:1」（AC2）
退出码：0 = 通过 · 1 = 违反（诊断打到 stderr）· 2 = 用法/IO 错误
"""

from __future__ import annotations

import json
import pathlib
import re
import sys


def main(argv: list[str]) -> int:
    if len(argv) not in (3, 4):
        print(
            "用法：verif_tb_shape_check.py <tb.sv> <manifest.json> [join|full]",
            file=sys.stderr,
        )
        return 2
    tb_path = pathlib.Path(argv[1])
    man_path = pathlib.Path(argv[2])
    mode = argv[3] if len(argv) == 4 else "full"
    if mode not in ("join", "full"):
        print(f"::error::未知模式 {mode}（只允许 join / full）", file=sys.stderr)
        return 2
    try:
        tb = tb_path.read_text(encoding="utf-8")
        man = json.loads(man_path.read_text(encoding="utf-8"))
    except OSError as exc:
        print(f"::error::读取失败：{exc}", file=sys.stderr)
        return 2
    except json.JSONDecodeError as exc:
        print(f"::error::manifest 不是合法 JSON：{exc}", file=sys.stderr)
        return 2

    port_table = man.get("port_table")
    if not isinstance(port_table, list) or not port_table:
        print("::error::manifest 缺 `port_table[]`（fail-closed）", file=sys.stderr)
        return 1
    expected = [e["name"] for e in port_table]

    # ── AC2：**顶层端口身份**的 1:1 join（`port_table[].name` ↔ `ports[].cpp_member`）──
    #   ⚠️ 判据面 = `role ∉ {clock, reset}` 且 `is_top_io == true`
    #   ⇒ ⇒ ① 每个顶层端口 MUST **具名**（`cpp_member` 不是占位符）
    #   ⇒ ⇒ ② `cpp_member` MUST **唯一**
    #   ⇒ ⇒ ③ 两边 MUST **一一对应**（集合相等）
    ports = man.get("ports")
    if not isinstance(ports, list):
        print("::error::manifest 缺 `ports[]`（fail-closed）", file=sys.stderr)
        return 1
    top_ports = [p for p in ports if p.get("is_top_io")]
    # ⚠️ **占位符**（`unnamed_input` / `unnamed_output`）= 未具名端口 ⇒ ⇒ 它们**不参与**
    #   「cpp_member 唯一」判定（未具名是**既有债** ⇒ 由 `verify_port_naming.sh` 的棘轮表管）
    named = []
    for p in top_ports:
        member = p.get("cpp_member", "")
        if not member or member in ("unnamed_input", "unnamed_output"):
            continue
        named.append((member, p["name"]))
    members = [m for m, _ in named]
    if len(set(members)) != len(members):
        dup = sorted({m for m in members if members.count(m) > 1})
        print("::error::顶层端口的 cpp_member 不唯一：" + ", ".join(dup), file=sys.stderr)
        return 1
    # ① 集合相等：`ports[]` 的顶层端口 == `port_table[]` 的非 clock/reset 项
    data_ports = [e["name"] for e in port_table if e.get("role") not in ("clock", "reset")]
    if sorted(p["name"] for p in top_ports) != sorted(data_ports):
        print("::error::顶层端口集合不一致：ports[] =",
              sorted(p["name"] for p in top_ports),
              " port_table[] =", sorted(data_ports), file=sys.stderr)
        return 1
    # ② 每个**具名**的顶层端口 MUST 在 `port_table[]` 里有同名的条目（1:1 的另一半）
    for _, pname in named:
        if pname not in data_ports:
            print("::error::具名顶层端口 `" + pname + "` 不在 port_table[] 里", file=sys.stderr)
            return 1

    if mode == "join":
        return 0

    # DUT 实例：`<module> dut ( ... );`（非贪婪到第一个 `);`）
    m = re.search(r"\b(\w+)\s+dut\s*\((.*?)\);", tb, re.S)
    if not m:
        print("::error::TB 里找不到 `dut ( ... );` 实例", file=sys.stderr)
        return 1
    body = m.group(2)
    conns = re.findall(r"\.\s*(\w+)\s*\(", body)

    failures = 0
    if len(conns) != len(expected):
        print(
            f"::error::DUT 连线 {len(conns)} 条 ≠ port_table {len(expected)} 条",
            file=sys.stderr,
        )
        failures += 1
    missing = [n for n in expected if n not in conns]
    if missing:
        print("::error::port_table 里有端口未连线：" + ", ".join(missing), file=sys.stderr)
        failures += 1
    extra = [n for n in conns if n not in expected]
    if extra:
        print("::error::连线里有 port_table 之外的端口：" + ", ".join(extra), file=sys.stderr)
        failures += 1
    # 重复连线（同一端口连两次）
    dup = sorted({n for n in conns if conns.count(n) > 1})
    if dup:
        print("::error::端口被重复连线：" + ", ".join(dup), file=sys.stderr)
        failures += 1

    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
