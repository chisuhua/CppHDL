#!/usr/bin/env python3
"""`_if` 语句块条件赋值的**静态护栏**分析器（ADR-051 D6）。

为什么需要
----------------------------------------------------------------------
条件块内的**普通** `reg->next = v` / `io().x = v` **不会**被条件化
（⇒ **无条件**生效 ⇒ **静默**错误）⇒ ⇒ MUST 有静态护栏检出。

⚠️ **方向**：**宁可漏报、不误报**（除「闭包体无法配平」**fail-closed**）。

⚠️ **为什么复用 `fsm_wiring` 的词法器与花括号配平**（ADR-051 D6）：
`scripts/lib/fsm_wiring.py` 里的「找 lambda 闭包 + 花括号配平」是**一份**实现
⇒ ⇒ 本文件 `import` 它 ⇒ ⇒ 两份护栏对「什么算一个闭包」的口径**逐位一致**
⇒ ⇒ 不会漂移。⚠️ `fsm_wiring.find_closures` 的入口表是 `on_active(` **专用**
⇒ ⇒ 本文件用其**通用**入口 `find_lambda_closures(stripped, entry_re)`。
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from fsm_wiring import (  # noqa: E402  (路径设置在前)
    CODE_EXT,
    SKIP_DIR_NAMES,
    find_lambda_closures,
    strip_comments_and_strings,
)

#: 条件块分支体的入口（`_if(c).then([&]{...})`）
ENTRY_RE = r"\.\s*then\s*\("

#: 寄存器 `next` 赋值（`r->next = ...` 或 `r.next() = ...`）
RE_NEXT_ASSIGN = re.compile(r"(?:->\s*next\s*=(?!=))|(?:\.\s*next\s*\(\s*\)\s*=(?!=))")

#: IO 端口赋值（`io().x = ...` / `io().x <<= ...`；`==` / `!=` 不算）
RE_IO_ASSIGN = re.compile(
    r"io\s*\(\s*\)\s*\.\s*[A-Za-z_]\w*\s*(?:<<\s*)?=(?!=)")

#: 条件化的**唯一**合法入口
CONDITIONAL_ASSIGN = "conditional_assign"

#: 被移除的符号（ADR-051 D4 / D8）⇒ **不**得再出现在**源文件**里
REMOVED_SYMBOLS = (
    "conditional_reg_assign",
    "priority_if",
    "priority_if_then",
)


class Violation(object):
    __slots__ = ("kind", "path", "line", "snippet")

    def __init__(self, kind, path, line, snippet):
        self.kind = kind
        self.path = path
        self.line = line
        self.snippet = snippet

    def __str__(self):
        return "%s:%d: [%s] %s" % (self.path, self.line, self.kind, self.snippet)


def _snippet(lines, line):
    if 1 <= line <= len(lines):
        return lines[line - 1].strip()
    return ""


def _statement_window(stripped, pos):
    """取 `pos` 之前**最近**的语句起点（`;` / `{` / `}` 之后）。"""
    i = pos - 1
    while i >= 0 and stripped[i] not in ";{}":
        i -= 1
    return stripped[i + 1:pos]


def analyze_text(text, path="<text>"):
    """在**已剥注释与字符串**的文本上做规则判定，返回违规列表。"""
    out = []
    lines = text.split("\n")

    closures = find_lambda_closures(text, ENTRY_RE)
    for body_start, body_end in closures:
        if body_end is None:
            # 花括号无法配平 ⇒ **fail-closed**（判违规）
            line = text.count("\n", 0, body_start) + 1
            out.append(Violation("parse-error", path, line,
                                 "条件块分支体无法配平（fail-closed）"))
            continue
        body = text[body_start:body_end]
        body_offset = body_start

        for m in RE_NEXT_ASSIGN.finditer(body):
            stmt = _statement_window(body, m.start())
            if CONDITIONAL_ASSIGN in stmt:
                continue  # ✅ 走了 `conditional_assign` ⇒ 合法
            off = body_offset + m.start()
            line = text.count("\n", 0, off) + 1
            out.append(Violation("unconditional-next-in-closure", path, line,
                                 _snippet(lines, line)))

        for m in RE_IO_ASSIGN.finditer(body):
            off = body_offset + m.start()
            line = text.count("\n", 0, off) + 1
            out.append(Violation("unconditional-io-in-closure", path, line,
                                 _snippet(lines, line)))

    return out


def analyze_source(text, path="<text>"):
    """完整分析：先剥注释与字符串，再判定。"""
    return analyze_text(strip_comments_and_strings(text), path)


#: 护栏**自身**的文件（它们必然提到被移除的符号 ⇒ 豁免）
SELF_FILES = (
    "scripts/lib/if_stmt_wiring.py",
    "scripts/verify_if_stmt_wiring.sh",
)


def analyze_removed_symbols(text, path="<text>"):
    """被移除的符号 MUST NOT 再出现在**源文件**里（ADR-051 D4 / D8）。

    ⚠️ **为什么不能写成编译期断言**：被删除的**自由函数**在 SFINAE 语境下是
    **硬错误**（名称查找失败**不是** deduction failure）⇒ ⇒ 整个 TU 编译失败
    ⇒ ⇒ 只能由**静态护栏**承担（与 ADR-050 实测② 的 `= delete` 同型）。
    """
    out = []
    if path.replace(os.sep, "/") in SELF_FILES:
        return out
    stripped = strip_comments_and_strings(text)
    lines = text.split("\n")
    for sym in REMOVED_SYMBOLS:
        for m in re.finditer(r"\b" + sym + r"\b", stripped):
            line = stripped.count("\n", 0, m.start()) + 1
            out.append(Violation("removed-symbol", path, line,
                                 "%s（MUST NOT 再出现）" % sym))
    return out


def iter_source_files(root):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIR_NAMES]
        for name in sorted(filenames):
            if os.path.splitext(name)[1] in CODE_EXT:
                yield os.path.join(dirpath, name)


# ---------------------------------------------------------------------------
# fixture 自检（护栏与自检共用同一份分析器 ⇒ 不会漂移）
# ---------------------------------------------------------------------------

FIXTURES = [
    # (名字, 源码, 期望违规数)
    ("clean", """
void f() {
    _if(io().en).then([&] {
        conditional_assign(reg).next() = 7_d;
    }).end();
}
""", 0),
    ("bare-next", """
void f() {
    _if(io().en).then([&] {
        reg->next = 7_d;
    }).end();
}
""", 1),
    ("bare-io", """
void f() {
    _if(io().en).then([&] {
        io().out = 7_d;
    }).end();
}
""", 1),
    ("conditional-io-form-is-ok", """
void f() {
    _if(io().en).then([&] {
        conditional_assign(io().out).next() = 7_d;
    }).end();
}
""", 0),
    ("outside-closure", """
void f() {
    reg->next = 7_d;
}
""", 0),
    ("non-lambda-then", """
void f() {
    _if(io().en).then(some_function).end();
}
""", 0),
]


def self_test():
    failures = []
    for name, src, expected in FIXTURES:
        got = len(analyze_source(src, name))
        if got == expected:
            print("  ✓ %s (%d)" % (name, got))
        else:
            print("  ✗ %s 期望 %d 实得 %d" % (name, expected, got))
            failures.append(name)
    if failures:
        print("SELF-TEST FAILED: %d 项" % len(failures))
        return 1
    print("self-test: %d/%d fixture 全绿" % (len(FIXTURES), len(FIXTURES)))
    return 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    root = argv[argv.index("--root") + 1] if "--root" in argv else "."
    total = 0
    for path in iter_source_files(root):
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        vs = analyze_source(text, rel) + analyze_removed_symbols(text, rel)
        for v in vs:
            print(str(v))
            total += 1
    print("VIOLATIONS=%d" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
