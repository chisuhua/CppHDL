#!/usr/bin/env python3
"""共享分析器：检测 `on_active` 闭包内的「寄存器 `next` 写入」与「IO 端口赋值」。

⚠️ **为什么抽成 lib**（照 `scripts/lib/assertion_quality.sh` 的先例）：
   护栏本体与它的 **fixture 自检（= 规格）** MUST 用**同一份**实现 ——
   两处各写一份 ⇒ 分析器会**漂移** ⇒ 「护栏行为」与「规格」分叉
   ⇒ ⇒ 本仓的 `example-assertion-quality` change 的对抗审稿轮次里
   连续多轮的 BLOCKER 就出在「两处对同一机制的两种理解」上。

用法：
    from fsm_wiring import analyze_text, iter_source_files
    for v in analyze_text(src, path):
        print(v)

违规种类（`kind`）：
    `reg-next-in-closure`  —— 闭包内写 `<reg>->next = ...`
    `io-assign-in-closure`  —— 闭包内给 `io().<port>` 赋值（`=` 或 `<<=`）
    `contrib-in-closure`   —— 闭包内调用 `sm.contrib(...)`（ADR-050 D4：
                             贡献 MUST 在闭包**之外** ⇒ 闭包内**只**注册转移）
    `parse-error`          —— 闭包体无法配平（**fail-closed** ⇒ 判违规）

⚠️ **能力边界（如实声明，勿再夸张）**：
    ① **已覆盖**：整行/行尾/跨行块注释、字符串与字符字面量、lambda capture list
       的嵌套 `[]`、lambda 参数列表、花括号配平。
    ② **未覆盖**：raw string（`R"(...)"`）按普通字符串处理（内容被抹掉 ⇒
       **漏报**方向）；宏展开后的形态（**漏报**方向）；通过**函数调用**间接写
       `reg->next` 或 `io().x`（**漏报**方向 ⇒ 例如
       `on_active([&]{ helper(); })` 且 `helper()` 内部写了寄存器）。
    ③ 方向性：本分析器**宁可漏报**、**不**误报 ⇒ 除 `parse-error` 外，
       「没报」**不**等于「没问题」，只等于「本分析器看不到」。
"""

import os
import re

# 扫描面的源码扩展名
CODE_EXT = (".h", ".hpp", ".hh", ".ipp", ".cpp", ".cc", ".cxx", ".C")

# 第三方 / VCS 元数据目录（**按目录名**判定 ⇒ 内容无关）
SKIP_DIR_NAMES = frozenset({".git", "third_party", "external"})

# 闭包内写寄存器 next（`==` 不算）
RE_REG_NEXT = re.compile(r"->\s*next\s*=(?!=)")
# 闭包内给 IO 端口赋值（`=` / `<<=`；`==` / `!=` 不算）
RE_IO_ASSIGN = re.compile(
    r"io\s*\(\s*\)\s*\.\s*[A-Za-z_]\w*\s*(?:<<\s*)?=(?!=)")
# 闭包内调用 `contrib(...)`（ADR-050 D4：贡献 MUST 在闭包之外）
RE_CONTRIB_CALL = re.compile(r"\bcontrib\s*\(")


def strip_comments_and_strings(text):
    """把注释与字符串/字符字面量替换成**等长空格**（保留换行 ⇒ 偏移量不变）。

    ⇒ 后续所有正则只在**真实代码**上匹配 ⇒ 注释里的 `->next =` 不会误报。
    """
    out = list(text)
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != "\n":
                    out[k] = " "
            i = j
        elif c == '"' or c == "'":
            quote = c
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == quote:
                    j += 1
                    break
                if text[j] == "\n":
                    break  # 未闭合 ⇒ 保守停在本行
                j += 1
            for k in range(i, min(j, n)):
                if out[k] != "\n":
                    out[k] = " "
            i = j
        else:
            i += 1
    return "".join(out)


def find_closures(stripped):
    """返回 `[(body_start, body_end_or_None)]` —— 每个 `on_active` **lambda** 闭包。"""
    return find_lambda_closures(stripped, r"\bon_active\s*\(")


def find_lambda_closures(stripped, entry_re):
    """返回 `[(body_start, body_end_or_None)]` —— 每个**入口调用**的 lambda 闭包。

    ⚠️ 只有「入口 `(` 之后**先**出现 capture list `[`」的才当作 lambda 实参
    ⇒ 框架自身的成员函数**定义**
    （`state_def& on_active(state_action_t action) { ... }`）**不会**被误判。
    ⚠️ `body_end is None` ⇒ 花括号无法配平 ⇒ **fail-closed**（判违规）。

    ⚠️ **为什么抽成通用**（ADR-051 D6）：`_if` 的护栏需要找 `.then(` / `.else_(`
    的 lambda 闭包 ⇒ ⇒ **花括号配平**这套逻辑 MUST **只有一份**
    ⇒ ⇒ 否则两份护栏的「什么算一个闭包」会漂移。
    """
    out = []
    n = len(stripped)
    for m in re.finditer(entry_re, stripped):
        j = m.end()
        while j < n and stripped[j].isspace():
            j += 1
        if j >= n or stripped[j] != "[":
            continue  # 不是 lambda 实参 ⇒ 跳过
        # capture list（支持嵌套 `[]`）
        depth = 0
        k = j
        while k < n:
            if stripped[k] == "[":
                depth += 1
            elif stripped[k] == "]":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        if k >= n:
            out.append((j, None))
            continue
        k += 1
        while k < n and stripped[k].isspace():
            k += 1
        if k < n and stripped[k] == "(":
            depth = 0
            while k < n:
                if stripped[k] == "(":
                    depth += 1
                elif stripped[k] == ")":
                    depth -= 1
                    if depth == 0:
                        k += 1
                        break
                k += 1
            while k < n and stripped[k].isspace():
                k += 1
        if k >= n or stripped[k] != "{":
            out.append((j, None))
            continue
        # 花括号配平
        depth = 0
        m2 = k
        while m2 < n:
            if stripped[m2] == "{":
                depth += 1
            elif stripped[m2] == "}":
                depth -= 1
                if depth == 0:
                    break
            m2 += 1
        if m2 >= n:
            out.append((j, None))
            continue
        # ⚠️ 配平后的下一个非空白字符 MUST 是 `)`（入口调用 `<lambda>` 的收尾）
        #    ⇒ 否则说明**配平到了错误的 `}`**（典型：源码花括号本身不平衡 ⇒
        #    配平会一路吃到更外层的 `}`）⇒ **fail-closed**。
        p = m2 + 1
        while p < n and stripped[p].isspace():
            p += 1
        if p >= n or stripped[p] != ")":
            out.append((j, None))
            continue
        out.append((k, m2))
    return out


class Violation(object):
    __slots__ = ("kind", "path", "line", "snippet")

    def __init__(self, kind, path, line, snippet):
        self.kind = kind
        self.path = path
        self.line = line
        self.snippet = snippet

    def __repr__(self):
        return "%s:%d: %s: %s" % (self.path, self.line, self.kind, self.snippet)


def analyze_text(text, path="<text>"):
    """返回 `Violation` 列表（空列表 = 无违规）。"""
    stripped = strip_comments_and_strings(text)
    lines = stripped.split("\n")
    out = []
    for body_start, body_end in find_closures(stripped):
        if body_end is None:
            line = stripped.count("\n", 0, body_start) + 1
            out.append(Violation("parse-error", path, line,
                                 "on_active 闭包体无法配平 ⇒ fail-closed"))
            continue
        body = stripped[body_start:body_end]
        for m in RE_REG_NEXT.finditer(body):
            off = body_start + m.start()
            line = stripped.count("\n", 0, off) + 1
            out.append(Violation("reg-next-in-closure", path, line,
                                 _snippet(lines, line)))
        for m in RE_IO_ASSIGN.finditer(body):
            off = body_start + m.start()
            line = stripped.count("\n", 0, off) + 1
            out.append(Violation("io-assign-in-closure", path, line,
                                 _snippet(lines, line)))
        for m in RE_CONTRIB_CALL.finditer(body):
            off = body_start + m.start()
            line = stripped.count("\n", 0, off) + 1
            out.append(Violation("contrib-in-closure", path, line,
                                 _snippet(lines, line)))
    return out


def _snippet(lines, line):
    if 1 <= line <= len(lines):
        return lines[line - 1].strip()
    return ""


def iter_source_files(root):
    """遍历仓库内的 C/C++ 源文件。

    ⚠️ **构建输出目录按【内容】判定**（含 `CMakeCache.txt`）⇒ **不**按目录名
    prune ⇒ `buildtools/` / `builder/` 这类**真实**目录不会被误剪
    （照 `verify_no_duplicate_rv32i_headers.sh` 的既定规则）。
    """
    for dirpath, dirnames, filenames in os.walk(root):
        keep = []
        for d in sorted(dirnames):
            if d in SKIP_DIR_NAMES:
                continue
            if os.path.exists(os.path.join(dirpath, d, "CMakeCache.txt")):
                continue  # 构建输出目录
            keep.append(d)
        dirnames[:] = keep
        for fn in sorted(filenames):
            if fn.endswith(CODE_EXT):
                yield os.path.join(dirpath, fn)


def analyze_file(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        return analyze_text(fh.read(), path)
