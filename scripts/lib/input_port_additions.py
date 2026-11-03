#!/usr/bin/env python3
"""**新增输入端口**的事件驱动增量检查（D1-A · 半自动）。

⚠️ **它做什么**：比较 `base..head` 的 diff ⇒ 找出**新增**的 `ch_in<...> name;`
声明（**仅生产代码**：`include/**` + `src/**`）⇒ ⇒ 对每一个新端口列出
「它属于哪个组件」+「该组件在仓内的全部实例化点」+「各实例化点处是否见到驱动形态」。

⚠️ **它刻意【不做什么】**：**不判违规** ⇒ ⇒ 退出码**只**取决于基础设施
（找不到 diff / git 不可用），**永远**不因「端口没被驱动」而失败 ⇒ ⇒
这是 2026-10-28 spike 的直接结论（平规则 ⇒ 必然误报 ⇒ 死结）⇒ ⇒
**半自动 = 只列不判 ⇒ 由人判**。

⚠️ **扫描面 = 仅生产代码**（实测依据，见
`.rddf/checks/decision-request-input-port-guards-2026-11.md` §1.2）：
最近 100 个 commit 里，生产代码侧新增 `ch_in` 的分布是 **99×0 / 1×1**
⇒ ⇒ 运行成本趋零；测试 DUT 的端口由**同一个测试文件**驱动（自包含）⇒ ⇒
不属于这条义务的风险面。

⚠️ **报告里的三种提示**（都只是「提示」，不是判定）：
  · `驱动（TB）`      ⇒ 该处见到 `set_input_value(...<port>...)`
  · `内部驱动`        ⇒ 该处见到 `io().<port> <<= ...`（**不是** TB 驱动）
  · `⚠️ 疑似静默失效` ⇒ 该处见到 `io().<port> = value`（落在 `ch_in` 上 ⇒ **不驱动**
                        ⇒ 真缺陷形态 ⇒ 必须点名）
  · `未见到驱动`      ⇒ 该文件里没匹配到上述任一形态 ⇒ ⚠️ **可能**驱动在 helper 内
                        （helper 的实参不可静态判定 ⇒ ⇒ 需人工确认 ⇒ 措辞刻意保守）

用法：
    python3 scripts/lib/input_port_additions.py --base <ref> [--head <ref>] [--root <dir>]
"""
from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

PROD_PATHS = ("include", "src")
SOURCE_EXTS = {".h", ".cpp", ".cc", ".cxx", ".C"}

# `ch_in<...> name;` / `ch_in<...> name{...}`
NEW_PORT_RE = re.compile(r"\bch_in\s*<[^;{}]*>\s*([A-Za-z_][A-Za-z0-9_]*)\s*(;|\{|,)")
CLASS_RE = re.compile(r"^\s*(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)")


def git(root: pathlib.Path, *args: str) -> tuple[int, str]:
    r = subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True)
    return r.returncode, r.stdout


def parse_diff_ports(root: pathlib.Path, base: str, head: str) -> list[dict]:
    """解析 `base..head` 的 diff ⇒ 新增的 ch_in 声明。"""
    rc, out = git(root, "diff", "-U0", f"{base}..{head}", "--", *PROD_PATHS)
    if rc != 0:
        raise RuntimeError(f"git diff 失败（base={base} head={head}）")
    ports: list[dict] = []
    path = ""
    new_line = 0
    for line in out.splitlines():
        if line.startswith("+++ "):
            path = line[6:].strip()
            if path.startswith("b/"):
                path = path[2:]
            continue
        m = re.match(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@", line)
        if m:
            new_line = int(m.group(1))
            continue
        if line.startswith("+") and not line.startswith("+++"):
            m2 = NEW_PORT_RE.search(line[1:])
            if m2 and path.endswith(tuple(SOURCE_EXTS)):
                ports.append({"file": path, "line": new_line, "port": m2.group(1),
                              "decl": line[1:].strip()[:100]})
            new_line += 1
        elif not line.startswith("-"):
            new_line += 1
    return ports


def enclosing_component(root: pathlib.Path, file: str, line: int) -> str:
    """向上找最近的 `class X` / `struct X`（括号配平 ⇒ 取声明所在的最内层类）。"""
    f = root / file
    if not f.exists():
        return "?"
    lines = f.read_text(errors="replace").splitlines()
    depth_stack: list[tuple[int, str]] = []
    depth = 0
    best = "?"
    for idx, text in enumerate(lines, 1):
        if idx > line:
            break
        m = CLASS_RE.match(text)
        if m:
            depth_stack.append((depth, m.group(1)))
        for ch in text:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                while depth_stack and depth_stack[-1][0] >= depth:
                    depth_stack.pop()
        if depth_stack and idx <= line:
            best = depth_stack[-1][1]
    return best


def type_aliases(root: pathlib.Path, component: str) -> list[str]:
    """`using X = ... Component ...;` ⇒ 别名也要当实例化点搜。"""
    out: list[str] = []
    pat = re.compile(r"using\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*[^;]*\b"
                     + re.escape(component) + r"\b")
    for p in root.rglob("*"):
        if p.suffix not in SOURCE_EXTS or not p.is_file():
            continue
        if ".git" in p.parts:
            continue
        for m in pat.finditer(p.read_text(errors="replace")):
            out.append(m.group(1))
    return sorted(set(out))


def find_instantiations(root: pathlib.Path, component: str) -> list[tuple[str, int, str]]:
    """找该组件的实例化点（ch_device<T> / ch_module<T> / 类型别名作为模板实参）。"""
    names = {component, *type_aliases(root, component)}
    hits: list[tuple[str, int, str]] = []
    for p in sorted(root.rglob("*")):
        if p.suffix not in SOURCE_EXTS or not p.is_file():
            continue
        if ".git" in p.parts:
            continue
        rel = str(p.relative_to(root))
        for ln, text in enumerate(p.read_text(errors="replace").splitlines(), 1):
            for nm in names:
                # 允许命名空间限定（demo::Demo / ch::core::X）
                if re.search(r"\bch_(?:device|module)\s*<\s*(?:[A-Za-z_][A-Za-z0-9_]*::)*"
                             + re.escape(nm) + r"\b", text):
                    hits.append((rel, ln, nm))
                    break
    return hits


def driver_hints(root: pathlib.Path, site_file: str, port: str) -> list[str]:
    """在该实例化点所在文件里找驱动形态（**只列形态，不判定**）。"""
    f = root / site_file
    if not f.exists():
        return []
    hints: list[str] = []
    for text in f.read_text(errors="replace").splitlines():
        # ⚠️ MUST 用 [^;]*?（不是 [^)]*）—— 实参里常含嵌套括号
        #    （dev.instance().io().port）⇒ 用 [^)]* 会在第一个 `)` 处截断 ⇒ 漏匹配。
        if re.search(r"set_input_value\s*\([^;]*?\b" + re.escape(port) + r"\b", text):
            hints.append("驱动（TB）")
        elif re.search(r"io\(\)\s*\.\s*" + re.escape(port) + r"\s*<<=", text):
            hints.append("内部驱动")
        elif re.search(r"io\(\)\s*\.\s*" + re.escape(port) + r"\s*=(?!=)", text):
            hints.append("⚠️ 疑似静默失效")
    return sorted(set(hints))


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--head", default="HEAD")
    ap.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parents[2]))
    args = ap.parse_args(argv)

    root = pathlib.Path(args.root).resolve()
    if not (root / ".git").exists():
        print(f"ERROR: {root} 不是 git 仓库 ⇒ 无法做增量检查（fail-closed）", file=sys.stderr)
        return 2
    rc, _ = git(root, "rev-parse", "--verify", f"{args.base}^{{commit}}")
    if rc != 0:
        print(f"ERROR: 基线 ref「{args.base}」不存在 ⇒ 无法做增量检查"
              "（⇒ CI 的 git fetch 失败 MUST 是硬失败 ⇒ 不得静默跳过）", file=sys.stderr)
        return 2

    try:
        ports = parse_diff_ports(root, args.base, args.head)
    except RuntimeError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    if not ports:
        print("::notice 未发现新增的 ch_in 端口（生产代码口径）⇒ 无需人工核对")
        print("新增输入端口：0")
        return 0

    print(f"新增输入端口：{len(ports)}（生产代码口径 · base={args.base} head={args.head}）")
    for p in ports:
        comp = enclosing_component(root, p["file"], p["line"])
        print(f"  · {p['file']}:{p['line']}  端口 {p['port']}  组件 {comp}")
        print(f"      声明：{p['decl']}")
        sites = find_instantiations(root, comp)
        if not sites:
            print("      实例化点：未找到（⇒ 该组件暂无实例化 ⇒ 无需驱动）")
            continue
        for sf, sl, nm in sites:
            hints = driver_hints(root, sf, p["port"])
            if not hints:
                tag = "未见到驱动（⚠️ 可能在 helper 内 ⇒ 需人工确认）"
            else:
                tag = " / ".join(hints)
            print(f"      实例化点：{sf}:{sl}（类型 {nm}）⇒ {tag}")
    print()
    print("::warning 上列出的端口**未判违规**（半自动 ⇒ 由人判）⇒ "
          "请人工确认每个新增端口的**全部实例化点**是否显式驱动")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
