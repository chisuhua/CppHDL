#!/usr/bin/env python3
"""rv32i_obs_vector.py —— 跨后端观测向量 vs 核顶层 `ch_out` 端口面的**守恒判据**。

判据（全部可机械判定；记 P = 核顶层 ch_out 端口集 · V = 跨后端比对向量 · R = 缺口登记表）：

  C1 守恒        |V| + |R| == |P|
  C2 登记有效    R ⊆ P            （禁止编造豁免）
  C3 登记不 stale R ∩ V == ∅       （补齐后 MUST 清账）
  C4 归属        V ∪ R == P        （新增端口不得静默失同步）

  C1 与 C4 不重复：C1 只判**条数**（换名不换数时恒绿）⇒ C4 判**集合**。
  另有：登记的「为什么未纳入」/「归位条件」两列 MUST 非空。

数据来源（格式钉死 ⇒ 不靠约定）：
  P  examples/riscv-mini/src/rv32i_core_top.h            （`ch_out<T> name{"name"}` 具名构造）
  V  examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp （`kObsNames[]` 的字符串字面量）
  R  .rddf/checks/rv32i-obs-vector-registry.md          （表格第 1 列的反引号标识符）

用法：
  python3 scripts/lib/rv32i_obs_vector.py --root . --check
  python3 scripts/lib/rv32i_obs_vector.py --self-test

退出码：0 = 通过 · 1 = 违规 · 2 = fail-closed（解析为空 / 缺文件 / 用法错误）
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
import tempfile

CORE_HEADER = "examples/riscv-mini/src/rv32i_core_top.h"
VECTOR_SRC = "examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp"
REGISTRY = ".rddf/checks/rv32i-obs-vector-registry.md"

PORTS_RE = re.compile(r"\bch_out\s*<[^;{]*>\s*([A-Za-z_]\w*)\s*\{")
STRING_RE = re.compile(r'"([^"]*)"')
ROW_RE = re.compile(r"^\|\s*`([A-Za-z_]\w*)`\s*\|")


def strip_cpp_comments(text: str) -> str:
    """单趟扫描剥注释 —— **顺序敏感**（`//` 注释里出现 `/*` 时，先剥块注释会吃掉后续代码）。

    实测教训：`test_rv32i_soc_dual_parity.cpp:10` 的行注释含 ``tests/**``
    ⇒ 朴素的「先 `/* ... */` 再 `//`」会把声明 `kObsNames` 整段吃掉。
    """
    out = []
    i, n = 0, len(text)
    state = "code"  # code | line | block | str | chr
    while i < n:
        c = text[i]
        if state == "code":
            if c == "/" and i + 1 < n and text[i + 1] == "/":
                state = "line"
                i += 2
                continue
            if c == "/" and i + 1 < n and text[i + 1] == "*":
                state = "block"
                i += 2
                continue
            if c == '"':
                state = "str"
            elif c == "'":
                state = "chr"
            out.append(c)
            i += 1
            continue
        if state == "line":
            if c == "\n":
                state = "code"
                out.append(c)
            i += 1
            continue
        if state == "block":
            if c == "*" and i + 1 < n and text[i + 1] == "/":
                state = "code"
                i += 2
                continue
            i += 1
            continue
        # str / chr
        out.append(c)
        if c == "\\" and i + 1 < n:
            out.append(text[i + 1])
            i += 2
            continue
        if (state == "str" and c == '"') or (state == "chr" and c == "'"):
            state = "code"
        i += 1
    return "".join(out)


def parse_ports(text: str) -> list:
    return PORTS_RE.findall(strip_cpp_comments(text))


def parse_vector(text: str) -> list:
    """取 `kObsNames[]` 初始化列表里的字符串字面量（**花括号配平**扫描）。

    ⚠️ 为什么不用 `=\\{(.*?)\\}` 的非贪婪正则：数组花括号一旦残缺（人为损坏 / 拼接错误），
    非贪婪正则会一路吃到**后面某条语句**的 `}` ⇒ 抓到一堆无关字符串 ⇒ 报错误导。
    配平扫描的失败形态 = 返回 `[]` ⇒ 调用方 fail-closed（红且理由清楚）。
    """
    s = strip_cpp_comments(text)
    m = re.search(r"kObsNames\s*\[\s*\]\s*=\s*\{", s)
    if not m:
        return []
    i = m.end()
    depth, j = 1, i
    while j < len(s) and depth:
        if s[j] == "{":
            depth += 1
        elif s[j] == "}":
            depth -= 1
        j += 1
    if depth:  # 未闭合
        return []
    return STRING_RE.findall(s[i : j - 1])


def parse_registry(text: str) -> list:
    """→ [{"name", "why", "until"}]（只取数据行；表头/分隔行不含反引号标识符）。"""
    rows = []
    for line in text.splitlines():
        m = ROW_RE.match(line)
        if not m:
            continue
        cells = [c.strip() for c in line.split("|")]
        # `| name | dir/width | why | until |` ⇒ ['', name, dir, why, until, '']
        rows.append(
            {
                "name": m.group(1),
                "why": cells[3] if len(cells) > 3 else "",
                "until": cells[4] if len(cells) > 4 else "",
            }
        )
    return rows


def load(root: pathlib.Path) -> tuple:
    core = root / CORE_HEADER
    vec = root / VECTOR_SRC
    reg = root / REGISTRY
    for p in (core, vec, reg):
        if not p.is_file():
            raise SystemExit("::error::缺输入文件：%s" % p)
    P = parse_ports(core.read_text(encoding="utf-8"))
    V = parse_vector(vec.read_text(encoding="utf-8"))
    R_rows = parse_registry(reg.read_text(encoding="utf-8"))
    if not P:
        raise SystemExit("::error::fail-closed：核顶层解析到 0 个 ch_out（%s）" % CORE_HEADER)
    if not V:
        raise SystemExit("::error::fail-closed：向量解析到 0 条（%s）" % VECTOR_SRC)
    if not R_rows:
        raise SystemExit("::error::fail-closed：登记表解析到 0 行（%s）" % REGISTRY)
    return P, V, R_rows


def check(P: list, V: list, R_rows: list) -> list:
    """→ [(代号, ok, 文案)]。空列表 = 全绿（调用方决定怎么打）。"""
    R = [r["name"] for r in R_rows]
    outs = []

    # C1 守恒（条数）
    ok = len(V) + len(R) == len(P)
    outs.append(
        (
            "C1",
            ok,
            "守恒：向量 %d + 登记 %d == 核顶层 ch_out %d" % (len(V), len(R), len(P))
            if ok
            else "守恒失败：向量 %d + 登记 %d = %d ≠ 核顶层 ch_out %d"
            % (len(V), len(R), len(V) + len(R), len(P)),
        )
    )

    # C2 登记有效（防编造）
    Pset = set(P)
    ghosts = [n for n in R if n not in Pset]
    outs.append(
        (
            "C2",
            not ghosts,
            "登记有效：%d 条全部存在于核顶层 ch_out" % len(R)
            if not ghosts
            else "登记无效：%s 不存在于核顶层 ch_out（禁止编造豁免）" % "、".join(ghosts),
        )
    )

    # C3 登记不 stale（防过期）
    dup = sorted(set(R) & set(V))
    outs.append(
        (
            "C3",
            not dup,
            "登记不 stale：%d 条均不在向量里" % len(R)
            if not dup
            else "登记 stale：%s 已在向量里（补齐后 MUST 清账）" % "、".join(dup),
        )
    )

    # C4 归属（集合相等）
    missing = sorted(Pset - set(V) - set(R))
    extra = sorted((set(V) | set(R)) - Pset - {n for n in R if n not in Pset})
    ok4 = not missing and not extra
    detail = []
    if missing:
        detail.append("既不在向量也不在登记：%s" % "、".join(missing))
    if extra:
        detail.append("不在核顶层端口面里：%s" % "、".join(extra))
    outs.append(
        (
            "C4",
            ok4,
            "归属：向量 ∪ 登记 == 核顶层 ch_out（%d 根）" % len(P)
            if ok4
            else "端口失同步：" + "；".join(detail),
        )
    )

    # 附：登记两列非空
    blank = [r["name"] for r in R_rows if not r["why"] or not r["until"]]
    outs.append(
        (
            "C2b",
            not blank,
            "登记完整性：%d 条的「为什么未纳入」/「归位条件」均非空" % len(R_rows)
            if not blank
            else "登记不完整：%s 的「为什么未纳入」或「归位条件」为空" % "、".join(blank),
        )
    )
    return outs


def run_check(root: pathlib.Path) -> int:
    P, V, R_rows = load(root)
    results = check(P, V, R_rows)
    bad = 0
    for code, ok, msg in results:
        if ok:
            print("  \033[0;32m✅ %s %s\033[0m" % (code, msg))
        else:
            print("  \033[0;31m❌ %s %s\033[0m" % (code, msg), file=sys.stderr)
            bad += 1
    if bad:
        print("\033[0;31m❌ rv32i-obs-vector: %d 处违规\033[0m" % bad, file=sys.stderr)
        return 1
    print("\033[0;32m✅ rv32i-obs-vector: PASS（C1–C4 + C2b · 观测集合对着端口面守恒）\033[0m")
    return 0


# ── self-test ───────────────────────────────────────────────────────────────

CORE_FIXTURE = """\
struct Io {
    ch_in<ch_bool> clk{"clk"};
    ch_out<ch_uint<32>> a{"a"};
    ch_out<ch_uint<32>> b{"b"};
    ch_out<ch_bool>     c{"c"};
    ch_out<ch_bool>     d{"d"};
};
"""

VEC_FIXTURE = """\
constexpr const char *kObsNames[] = {
    "a",
    "b"};
"""

REG_FIXTURE = """\
| 端口 | 方向 / 宽度 | 为什么**未纳入** | 归位条件 |
|---|---|---|---|
| `c` | out / 1 | fixture 缺口 c | change `x` 补齐后删本行 |
| `d` | out / 1 | fixture 缺口 d | change `x` 补齐后删本行 |
"""


def _mk(tmp: pathlib.Path) -> pathlib.Path:
    for rel, txt in (
        (CORE_HEADER, CORE_FIXTURE),
        (VECTOR_SRC, VEC_FIXTURE),
        (REGISTRY, REG_FIXTURE),
    ):
        p = tmp / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(txt, encoding="utf-8")
    return tmp


def _verdict(root: pathlib.Path) -> dict:
    """→ {代号: ok}；fail-closed（解析为空）⇒ 返回空 dict（调用方按「红」处理）。"""
    try:
        P, V, R_rows = load(root)
    except SystemExit:
        return {}
    res = check(P, V, R_rows)
    return {code: ok for code, ok, _ in res}


def run_self_test() -> int:
    fails = []
    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        _mk(base)

        # 0) 基线 MUST 全绿
        v = _verdict(base)
        if not all(v.values()):
            fails.append("基线未全绿：%s" % v)
        else:
            print("  ✅ 基线 fixture 全绿（P=4 · V=2 · R=2）")

        def poison(label, rel, old, new, expect):
            root = pathlib.Path(tempfile.mkdtemp(dir=td))
            _mk(root)
            p = root / rel
            txt = p.read_text(encoding="utf-8")
            assert old in txt, "%s：投毒锚点未命中（fixture 形态变了？）" % label
            p.write_text(txt.replace(old, new, 1), encoding="utf-8")
            v = _verdict(root)
            hit = [c for c in expect if not v[c]]
            if not hit:
                fails.append("%s ⇒ **恒绿**（期望 %s 红）" % (label, expect))
            else:
                print("  ✅ %s ⇒ 红（命中 %s）" % (label, "、".join(hit)))

        # P1：从向量删一项 ⇒ C1 / C4
        poison("P1 向量删一项", VECTOR_SRC, '    "b"}', "}", ["C1", "C4"])
        # P2：登记表加编造端口 ⇒ C2
        poison(
            "P2 登记编造端口",
            REGISTRY,
            "| `c` | out / 1 |",
            "| `zzz` | out / 1 | fixture 缺口 | change `x` 补齐后删本行 |\n| `c` | out / 1 |",
            ["C2"],
        )
        # P3：登记表删一行 ⇒ C1 / C4
        poison(
            "P3 登记删一行",
            REGISTRY,
            "| `d` | out / 1 | fixture 缺口 d | change `x` 补齐后删本行 |\n",
            "",
            ["C1", "C4"],
        )
        # P4：核顶层加一个端口（既不进向量也不进登记）⇒ C1 / C4
        poison(
            "P4 新增端口未登记",
            CORE_HEADER,
            '    ch_out<ch_bool>     d{"d"};',
            '    ch_out<ch_bool>     d{"d"};\n    ch_out<ch_bool>     e{"e"};',
            ["C1", "C4"],
        )
        # P5：把登记里的端口也加进向量（补齐后忘清账）⇒ C3
        poison(
            "P5 补齐未清账",
            VECTOR_SRC,
            '    "b"}',
            '    "b",\n    "c"}',
            ["C3"],
        )

    if fails:
        for f in fails:
            print("  \033[0;31m❌ %s\033[0m" % f, file=sys.stderr)
        print("\033[0;31m❌ rv32i-obs-vector --self-test: %d 项失败\033[0m" % len(fails), file=sys.stderr)
        return 1
    print("\033[0;32m✅ rv32i-obs-vector --self-test: PASS（基线 + 5/5 投毒各自必红）\033[0m")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument("--root", default=None)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return run_self_test()
    if a.check:
        root = pathlib.Path(a.root).resolve() if a.root else pathlib.Path(__file__).resolve().parents[2]
        return run_check(root)
    ap.print_usage(sys.stderr)
    print("::error::需要 --check 或 --self-test", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
