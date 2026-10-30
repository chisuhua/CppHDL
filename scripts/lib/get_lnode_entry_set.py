#!/usr/bin/env python3
"""`get_lnode` 的**入口集合**分析器 —— ADR-053 的**通用**判据。

为什么需要一个「通用」分析器（而不是只看某个 fixture）
--------------------------------------------------------------------------
实测踩中的盲区：护栏的**编译期**判据（fixture 里 `static_assert`）只能看到
**fixture 自己 include 的**那些头文件 ⇒ ⇒ 缺陷真正的所在地
（`core/bundle/bundle_base.h`）若**没被** fixture include ⇒ ⇒ 护栏**漏报**
（实测：恢复那个无约束重载后，编译期判据仍全绿）。

⇒ ⇒ 因此「不存在无约束的**通用** `get_lnode` 入口」这条契约 MUST 由
**源码级分析**兜底 ⇒ ⇒ 本分析器扫描 `include/**` + `src/**` 的**全部**声明点。

判据（与 ADR-053 的契约一致）
--------------------------------------------------------------------------
一个 `get_lnode` 的**声明**是**违规** iff 它同时满足：

1. **通用**：形参类型是 `const T &` / `T &` / `const T`（`T` 是**未约束**的模板
   类型参数）⇒ ⇒ 它能匹配**任意**类型；
2. **无约束**：声明**没有** `requires` 子句 ⇒ ⇒ 它对**无** `impl()` 的类型也
   **可行**（其返回类型显式 ⇒ 函数体不实例化 ⇒ `requires` 探测得 **true**
   ⇒ **假阳性**）。

**不违规**的形态（明确豁免）：

- `get_lnode(const lnode<T> &)` —— 恒等版（模板参数无法从实参推导 ⇒ 非通用）；
- `get_lnode(const ch_uint<N> &)` —— **类型专用**（只匹配 `ch_uint` ⇒ 不会对
  无 `impl()` 的类型产生假阳性）；
- `get_lnode(const ch_bool &)` —— 同上；
- 任何**带** `requires` 子句的通用声明。

用法
--------------------------------------------------------------------------
    python3 scripts/lib/get_lnode_entry_set.py <root>            # 扫描，违规 ⇒ exit 1
    python3 scripts/lib/get_lnode_entry_set.py --self-test <root> # 分析器自检
"""

import os
import pathlib
import re
import sys

# 扫描面（与仓内其它分析器一致：源码目录）
SCAN_DIRS = ("include", "src")

# 一个「声明」看起来像什么：`... get_lnode(` 且不是调用（前面不是 `=`/`(`/`,`/运算符）
DEF_RE = re.compile(r"\bget_lnode\s*\(")

# 通用形参：`const T &` / `T &` / `const T` —— T 是**单个**标识符
GENERIC_PARAM_RE = re.compile(
    r"\(\s*(?:const\s+)?([A-Za-z_]\w*)\s*(?:const\s*)?&\s*(?:[A-Za-z_]\w*\s*)?\)"
)

# 模板参数列表里的类型参数名（非类型参数 `unsigned N` 不算）
TEMPLATE_TYPE_PARAM_RE = re.compile(r"typename\s+([A-Za-z_]\w*)|class\s+([A-Za-z_]\w*)")

# `requires` 子句（允许跨行）
REQUIRES_RE = re.compile(r"\brequires\b")


def _strip_comments_and_strings(text):
    """把注释与字符串字面量替换成等长空格（保持行号）。"""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                if text[i] != "\n":
                    out[i] = " "
                i += 1
            for _ in range(2):
                if i < n:
                    out[i] = " "
                    i += 1
        elif c in ('"', "'"):
            quote = c
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    out[i] = " "
                    i += 1
                    if i < n:
                        out[i] = " "
                        i += 1
                    continue
                if text[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = " "
                i += 1
        else:
            i += 1
    return "".join(out)


def _collect_sources(root):
    for d in SCAN_DIRS:
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            continue
        for dirpath, _dirs, files in os.walk(base):
            for fn in files:
                if fn.endswith((".h", ".cpp", ".tpp", ".hpp")):
                    yield os.path.join(dirpath, fn)


def _declaration_context(lines, idx):
    """取出 `lines[idx]` 所属声明的上下文。

    返回 (template_header, signature_block)：
    - `template_header`：向上找最近的 `template<...>` 行（若在 6 行内）；
    - `signature_block`：从声明行起，直到 `{` 或 `;` 为止的文本。
    """
    tmpl = ""
    # ⚠️ **同一行**也可能带 template 头（例：`template <typename T> inline ...`）
    #    ⇒ 先查当前行，再往上找。
    if "template" in lines[idx] and "<" in lines[idx]:
        tmpl = lines[idx]
    else:
        for back in range(idx - 1, max(-1, idx - 8), -1):
            if "template" in lines[back] and "<" in lines[back]:
                tmpl = lines[back]
                break
    sig_lines = []
    for k in range(idx, min(len(lines), idx + 6)):
        sig_lines.append(lines[k])
        if "{" in lines[k] or ";" in lines[k]:
            break
    return tmpl, "\n".join(sig_lines)


def analyze(root):
    """返回违规列表：[(path, lineno, snippet, reason), ...]"""
    violations = []
    for path in _collect_sources(root):
        with open(path, encoding="utf-8", errors="replace") as fh:
            raw = fh.read()
        clean = _strip_comments_and_strings(raw)
        lines = clean.split("\n")
        for idx, line in enumerate(lines):
            if not DEF_RE.search(line):
                continue
            # 排除**调用**：形如 `= get_lnode(x)` / `get_lnode(x).` / `, get_lnode(x)`
            pre = line[: DEF_RE.search(line).start()]
            if "=" in pre or "(" in pre or "," in pre or "return" in pre:
                continue
            tmpl, sig = _declaration_context(lines, idx)
            if not tmpl:
                # 非模板声明 ⇒ 形参是**具体类型** ⇒ 类型专用 ⇒ 非通用
                continue
            params = TEMPLATE_TYPE_PARAM_RE.findall(tmpl)
            type_params = {a or b for a, b in params}
            m = GENERIC_PARAM_RE.search(sig)
            if not m:
                continue
            param_t = m.group(1)
            if param_t not in type_params:
                # 形参类型不是**该模板的类型参数** ⇒ 类型专用（如 `const ch_uint<N> &`）
                continue
            if REQUIRES_RE.search(sig):
                continue
            violations.append((
                path,
                idx + 1,
                (tmpl.strip() + " " + sig.strip()).replace("\n", " ")[:160],
                "无约束的**通用** get_lnode 入口（形参 `%s` 是未约束的模板类型参数）" % param_t,
            ))
    return violations


# ── 自检 fixture（分析器自身的判别力）──────────────────────────────────────
SELF_TEST_CASES = [
    (
        "generic_unconstrained",
        "template <typename T>\nlnode<T> get_lnode(const T &t) { return {}; }\n",
        True,
        "通用 + 无约束 ⇒ 违规",
    ),
    (
        "generic_constrained",
        "template <typename T>\nlnode<T> get_lnode(const T &t) requires requires { t.impl(); } "
        "{ return {}; }\n",
        False,
        "通用 + 有约束 ⇒ 合法",
    ),
    (
        "type_specific_ch_uint",
        "template <unsigned N>\ninline lnode<ch_uint<N>> get_lnode(const ch_uint<N> &v) "
        "{ return {}; }\n",
        False,
        "类型专用 ⇒ 只匹配特定类型 ⇒ 不产生假阳性探测 ⇒ 合法",
    ),
    (
        "identity",
        "template <typename T> lnode<T> get_lnode(const lnode<T> &n) { return n; }\n",
        False,
        "恒等版 ⇒ 模板参数无法从实参推导 ⇒ 非通用 ⇒ 合法",
    ),
    (
        "commented_out",
        "/*\ntemplate <typename T>\nlnode<T> get_lnode(const T &t) { return {}; }\n*/\n",
        False,
        "注释里的 ⇒ 不是声明 ⇒ 合法",
    ),
    (
        "call_site",
        "void f() { auto x = get_lnode(value); }\n",
        False,
        "调用点 ⇒ 不是声明 ⇒ 合法",
    ),
]


def self_test():
    """用内联 fixture 验证分析器本身有判别力。"""
    import tempfile

    ok = True
    for name, body, should_violate, why in SELF_TEST_CASES:
        with tempfile.TemporaryDirectory() as td:
            inc = os.path.join(td, "include")
            os.makedirs(inc)
            with open(os.path.join(inc, "t.h"), "w", encoding="utf-8") as fh:
                fh.write(body)
            vs = analyze(td)
        got = len(vs) > 0
        if got != should_violate:
            ok = False
            print("   ❌ %-24s 期望违规=%s 实得=%s（%s）" % (name, should_violate, got, why))
        else:
            print("   ✅ %-24s %s" % (name, why))
    return ok


def inject_unconstrained(path):
    """往目标头文件里**注入**一个无约束的通用 `get_lnode` 入口（自检用）。

    ⇒ 用于证明 R4（源码级扫描）**确实会抓** —— 因为编译期判据只看得到
    fixture 自己 include 的头文件，**看不到**本文件。
    """
    p = pathlib.Path(path)
    s = p.read_text(encoding="utf-8")
    inject = """
// ⚠️ SELF-TEST MUTATION: 模拟「有人又加了一个无约束的通用入口」
template <typename Derived> inline lnode<Derived> get_lnode(const Derived &b) {
    return lnode<Derived>(b.impl());
}
"""
    anchor = "} // namespace ch::core"
    if anchor not in s:
        raise SystemExit("inject: 找不到锚点 %r" % anchor)
    s = s.replace(anchor, inject + anchor, 1)
    p.write_text(s, encoding="utf-8")


def main():
    args = sys.argv[1:]
    if not args:
        print("用法：get_lnode_entry_set.py <root> | --self-test <root> "
              "| --inject-unconstrained <file>")
        return 2
    if args[0] == "--inject-unconstrained":
        inject_unconstrained(args[1])
        return 0
    if args[0] == "--self-test":
        root = args[1] if len(args) > 1 else os.getcwd()
        print("   自检：分析器自身的判别力")
        if not self_test():
            print("❌ 分析器自检失败")
            return 1
        print("   ✅ 分析器自检通过")
        return 0

    root = os.path.abspath(args[0])
    violations = analyze(root)
    if violations:
        print("❌ 发现 %d 处**无约束的通用** get_lnode 入口：" % len(violations))
        for path, lineno, snippet, reason in violations:
            print("   %s:%d" % (os.path.relpath(path, root), lineno))
            print("      %s" % snippet)
            print("      ⇒ %s" % reason)
        return 1
    print("✅ 无「无约束的通用 get_lnode」入口")
    return 0


if __name__ == "__main__":
    sys.exit(main())
