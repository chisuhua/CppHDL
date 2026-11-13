#!/usr/bin/env python3
"""jit_golden_coverage.py —— JIT golden 覆盖的**集合**派生与基线比对。

（phase-6 **P8** / 路线 R4.3 · ADR-035 v2.2）

⚠️ **为什么是集合而不是计数**：`5 / 39` 这种计数判据**恒绿** ——
   把已覆盖的 `SELECT` 换成 `MUL` ⇒ 计数仍是 5 ⇒ 判据**看不见**覆盖面的漂移。
   ⇒ ⇒ 基线 MUST 记录**集合**，比对 MUST 是**逐元素相等**。

⚠️ **禁裸 `grep -c`**：全文件 grep 会把注释里提到的 `JitOp::` 算进分子
   （本仓 `phase-6.md` 的 P6 判据自己就写了这条）。⇒ ⇒ 本分析器**先剥注释**。

用法：
    python3 jit_golden_coverage.py --enum <jit_ir.h> --tests <test_jit_golden.cpp>
        ⇒ 打印派生结果（denominator / covered / uncovered）
    python3 jit_golden_coverage.py --enum ... --tests ... --baseline <md>
        ⇒ 与基线比对 ⇒ 全部一致 exit 0，任一不一致 exit 1

Exit: 0 = 一致 / 1 = 不一致 / 2 = **解析失败**（fail-closed：派生不出来 ⇒ 不得静默判绿）
"""
from __future__ import annotations

import argparse
import re
import sys

ENUM_NAME_RE = re.compile(r"^[A-Z_][A-Z0-9_]*$")
OP_IN_TEXT_RE = re.compile(r"JitOp::([A-Z_][A-Z0-9_]*)")


class ParseError(RuntimeError):
    pass


def strip_comments(text: str) -> str:
    """去掉 C++ 注释，**保留**字符串字面量（`JitOp::X` 就在测试名里）。"""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c in ('"', "'"):
            quote = c
            out.append(c)
            i += 1
            while i < n:
                if text[i] == "\\":
                    out.append(text[i])
                    if i + 1 < n:
                        out.append(text[i + 1])
                    i += 2
                    continue
                out.append(text[i])
                ch = text[i]
                i += 1
                if ch == quote:
                    break
            continue
        out.append(c)
        i += 1
    return "".join(out)


def strip_string_contents(line: str) -> str:
    """把字符串字面量的**内容**挖空（保留引号）⇒ 用于括号配平计数。

    ⚠️ 必须做：`TEST_CASE("… (byte-identical)", "[jit][golden]")` 里
       名字串自带括号 ⇒ ⇒ 直接数括号会**提前结束**。
    """
    out = []
    i, n = 0, len(line)
    while i < n:
        c = line[i]
        if c in ('"', "'"):
            quote = c
            out.append(quote)
            i += 1
            while i < n:
                if line[i] == "\\":
                    i += 2
                    continue
                if line[i] == quote:
                    i += 1
                    break
                i += 1
            out.append(quote)
            continue
        out.append(c)
        i += 1
    return "".join(out)


def parse_enum_ops(text: str) -> list[str]:
    """`enum class JitOp` 块内的名字（按声明顺序去重保序）。"""
    clean = strip_comments(text)
    m = re.search(r"enum\s+class\s+JitOp\b[^{]*\{", clean)
    if not m:
        raise ParseError("找不到 `enum class JitOp {` 块")
    i = m.end()
    depth = 1
    start = i
    while i < len(clean):
        if clean[i] == "{":
            depth += 1
        elif clean[i] == "}":
            depth -= 1
            if depth == 0:
                break
        i += 1
    if depth != 0:
        raise ParseError("`enum class JitOp` 块未闭合")
    body = clean[start:i]

    ops: list[str] = []
    for raw in body.split("\n"):
        s = raw.strip()
        if not s:
            continue
        # 允许 `NAME = 3,` / `NAME,` / `NAME`（显式值只取等号左边）
        s = s.split("=", 1)[0].strip().rstrip(",").strip()
        if ENUM_NAME_RE.match(s) and s not in ops:
            ops.append(s)
    if not ops:
        raise ParseError("`enum class JitOp` 块内没有解析出任何名字")
    return ops


def parse_covered_ops(text: str) -> set[str]:
    """`TEST_CASE(` **头**上出现的 `JitOp::<NAME>` 集合。

    判据边界：只看 `TEST_CASE(` 的**声明头**（名字 + tag 列表），
    不看用例正文 —— 正文里出现的 `JitOp::` 是**使用**，不是「该 op 有 golden 用例」。
    """
    clean = strip_comments(text)
    lines = clean.split("\n")
    covered: set[str] = set()
    i = 0
    while i < len(lines):
        if "TEST_CASE(" not in lines[i]:
            i += 1
            continue
        # 从 TEST_CASE( 起累积，直到括号配平（字符串内容已挖空 ⇒ 不会被名字里的括号误导）
        chunk: list[str] = []
        depth = 0
        seen_open = False
        j = i
        while j < len(lines) and j < i + 6:
            chunk.append(lines[j])
            bal = strip_string_contents(lines[j])
            depth += bal.count("(") - bal.count(")")
            if "(" in bal:
                seen_open = True
            if seen_open and depth <= 0:
                break
            j += 1
        for m in OP_IN_TEXT_RE.finditer("\n".join(chunk)):
            covered.add(m.group(1))
        i = j + 1
    return covered


# ── 基线（`.rddf/checks/jit-golden-coverage.md` 的数据行）──────────────────────
def parse_baseline(path: str) -> dict:
    raw = open(path, encoding="utf-8").read()
    got: dict = {}
    for line in raw.split("\n"):
        m = re.match(r"^(denominator|covered|uncovered):\s*(.*)$", line.strip())
        if not m:
            continue
        key, val = m.group(1), m.group(2).strip()
        if key == "denominator":
            if not val.isdigit():
                raise ParseError(f"基线 denominator 不是整数：{val!r}")
            got[key] = int(val)
        else:
            got[key] = {x.strip() for x in val.split(",") if x.strip()}
    for key in ("denominator", "covered", "uncovered"):
        if key not in got:
            raise ParseError(f"基线缺数据行：{key}:")
    return got


def derive(enum_path: str, tests_path: str) -> tuple[list[str], set[str]]:
    try:
        enum_text = open(enum_path, encoding="utf-8").read()
        tests_text = open(tests_path, encoding="utf-8").read()
    except OSError as e:
        raise ParseError(f"读不到输入文件：{e}") from e
    ops = parse_enum_ops(enum_text)
    covered = parse_covered_ops(tests_text)
    if not covered:
        # fail-closed：一个都没解析出来 ⇒ 大概率是判据本身坏了 ⇒ 不得判绿
        raise ParseError(f"{tests_path} 里没有解析出任何 `TEST_CASE(` 头上的 `JitOp::`")
    return ops, covered


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--enum", required=True)
    ap.add_argument("--tests", required=True)
    ap.add_argument("--baseline")
    args = ap.parse_args()

    try:
        ops, covered = derive(args.enum, args.tests)
    except ParseError as e:
        print(f"❌ 派生失败（fail-closed）：{e}")
        return 2

    unknown = sorted(covered - set(ops))
    uncovered = [o for o in ops if o not in covered]

    if not args.baseline:
        print(f"denominator: {len(ops)}")
        print(f"covered: {', '.join(sorted(covered))}")
        print(f"uncovered: {', '.join(uncovered)}")
        if unknown:
            print(f"⚠️ 测试里出现但枚举里没有的名字：{', '.join(unknown)}")
        return 0

    try:
        base = parse_baseline(args.baseline)
    except (OSError, ParseError) as e:
        print(f"❌ 基线读取失败（fail-closed）：{e}")
        return 2

    fails = 0

    # ① 分母（枚举名字数）—— 枚举新增/删除 op ⇒ 基线 MUST 同步
    if base["denominator"] != len(ops):
        print(f"❌ 分母漂移：实测 {len(ops)}，基线 {base['denominator']}"
              "（新增/删除 JitOp ⇒ 基线 MUST 同步）")
        fails += 1
    else:
        print(f"✅ 分母 = {len(ops)}（与基线一致）")

    # ② 已覆盖集 —— **逐元素相等**（不是计数）
    if covered != base["covered"]:
        missing = sorted(base["covered"] - covered)
        extra = sorted(covered - base["covered"])
        if missing:
            print(f"❌ 基线声称已覆盖但实测**没有** golden 用例：{', '.join(missing)}")
        if extra:
            print(f"❌ 实测已覆盖但**不在**基线里：{', '.join(extra)}"
                  "（新增用例 ⇒ 基线 MUST 同步；把覆盖换成另一个 op ⇒ 计数不变但集合变了）")
        fails += 1
    else:
        print(f"✅ 已覆盖集 = {len(covered)} 个，与基线**逐元素相等**")

    # ③ 未覆盖集（= 分母 − 分子）
    if set(uncovered) != base["uncovered"]:
        print("❌ 未覆盖集与基线不一致"
              f"（实测 {len(uncovered)} 个 / 基线 {len(base['uncovered'])} 个）")
        fails += 1
    else:
        print(f"✅ 未覆盖集 = {len(uncovered)} 个，与基线**逐元素相等**")

    # ④ 一致性：covered 里的名字 MUST 都在枚举里（防基线写了不存在的 op）
    if covered - set(ops):
        print(f"❌ 已覆盖集里有枚举中不存在的名字：{', '.join(sorted(covered - set(ops)))}")
        fails += 1

    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
