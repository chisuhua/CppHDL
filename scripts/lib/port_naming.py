#!/usr/bin/env python3
"""端口命名的**棘轮**分析器（`port-naming-ratchet-2026-11` **N2**）。

⚠️ **它做什么**：
扫描 **product 侧**（`include/**` + `examples/**`；`tests/**` **默认排除**）的
`__io(...)` 块 ⇒ ⇒ 抽出**每一个端口声明** ⇒ ⇒ 判定它**是否合规**：

| 形态 | 合规？ |
|---|---|
| `ch_in<T> ident;`（**未具名**） | ❌（既有债） |
| `ch_in<T> ident{"name"};` 且 `ident == "name"` | ✅ |
| `ch_in<T> ident{"name"};` 且 `ident != "name"` | ❌（**P1 违规**：名字串是唯一真源 ⇒ 标识符必须与它逐字相同） |

⇒ ⇒ **不合规的端口 MUST 全部登记在棘轮豁免表里** ⇒ ⇒
**双向**：
  ① **正向**（债增长）：树里有、登记表里没有 ⇒ ⇒ **红**（新增端口 MUST 具名）；
  ② **反向**（登记表烂掉）：登记表里有、树里没有 ⇒ ⇒ **红**（登记表 MUST 同步）。

⚠️ **为什么不用「文件哈希」做条目身份**（N2 ③）：
文件哈希对**任何注释改动**都失效 ⇒ ⇒ 一次无关的注释编辑就会让登记表整体失效
⇒ ⇒ 身份必须是**逐端口**的：`<文件>\\t<__io 块序号>\\t<声明文本>`。

⚠️ **为什么登记表与「git diff 增量」并存**：
N2 ② 说「判定粒度 = 端口级增量」⇒ ⇒ 本工具**同时**提供 `--base`（增量清单，
用于**报告**「本次新增了哪些端口」）与**全树集合比对**（用于**判定**）⇒ ⇒
两者在「债增长」方向**等价**，而在「登记表烂掉」方向**更强**（全树比对能抓到
基线之外的陈旧条目）⇒ ⇒ 且**不依赖某个具体基线 ref** ⇒ ⇒ 登记表**可移植**。

用法：
    # 判定（护栏用）：非合规端口集合 MUST 与登记表**逐条相等**
    python3 scripts/lib/port_naming.py --root <dir> --registry <tsv> [--expect-count N]

    # 生成登记表（一次性 / 刷新用）
    python3 scripts/lib/port_naming.py --root <dir> --write-registry <tsv>

    # 只列增量（报告用）
    python3 scripts/lib/port_naming.py --root <dir> --base <ref> [--head <ref>]
"""
from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

# ⚠️ **product 侧**（N2 ①）：`include/**` + `examples/**`；`tests/**` **排除**
#    （沿用既有裁决 `scripts/verify_input_port_additions.sh`：「测试 DUT 端口自包含
#    ⇒ 不属风险面」）。
PROD_FACES = ("include", "examples")
SOURCE_EXTS = {".h", ".hpp", ".tpp", ".inc", ".cpp", ".cc", ".cxx", ".C"}

IO_MACRO = "__io"


# ── C++ 注释 / 字符串剥离（与 `scripts/verify_public_api_visibility.sh` 同款）──
def strip_cxx(text: str) -> str:
    """去掉 `//` 与 `/* */` 注释，以及 `"..."` / `'...'` 字面量。"""
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
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


def strip_comments_keep_len(text: str) -> str:
    """只去掉 `//` 与 `/* */` 注释，**保持长度与换行**（⇒ 偏移不变 ⇒ 可用原文切片）。

    ⚠️ **为什么不能直接用 `strip_cxx`**（它连**字符串字面量**一起剥）：
    本工具要读的正是**显式名字串**（`ch_in<T> ident{"name"};` 里的 `"name"`）
    ⇒ ⇒ 剥掉字符串 ⇒ ⇒ 端口被误判成「未具名」⇒ ⇒ 护栏**过严**（假红）。
    ⇒ ⇒ 故此处**只**剥注释，且**用空格替换**（保持偏移对齐）。
    """
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
            continue
        if c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != "\n":
                    out[k] = " "
            i = j
            continue
        i += 1
    return "".join(out)


def _match_paren(s: str, open_idx: int) -> int:
    """返回与 `s[open_idx] == '('` 匹配的 `)` 下标；找不到返回 -1。"""
    depth = 0
    i = open_idx
    n = len(s)
    while i < n:
        if s[i] == "(":
            depth += 1
        elif s[i] == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def _match_angle(s: str, open_idx: int) -> int:
    """返回与 `s[open_idx] == '<'` 匹配的 `>` 下标；找不到返回 -1。"""
    depth = 0
    i = open_idx
    n = len(s)
    while i < n:
        if s[i] == "<":
            depth += 1
        elif s[i] == ">":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def _normalize(decl: str) -> str:
    """声明文本归一：压空白 + 去尾分号。"""
    return re.sub(r"\s+", " ", decl).strip().rstrip(";").strip()


def extract_ports(text: str) -> list[dict]:
    """抽出 `__io(...)` 块内的全部端口声明。

    返回的每项：`{block, decl, ident, name}`（`name` = 显式名字串或 None）。

    ⚠️ **全程用掩码文本**（注释→空格 · **字符串保留** · 长度不变）：
    ① 块定位与声明切分都在掩码上做（注释里的 `;` 不会误切）；
    ② 名字串（`"name"`）**原样保留** ⇒ ⇒ 不会把具名端口误判成「未具名」。
    """
    s = strip_comments_keep_len(text)
    out: list[dict] = []
    block_ord = -1
    for m in re.finditer(r"(?<![A-Za-z0-9_])" + IO_MACRO + r"\s*\(", s):
        open_idx = s.index("(", m.start())
        close_idx = _match_paren(s, open_idx)
        if close_idx < 0:
            continue
        block_ord += 1
        body = s[open_idx + 1:close_idx]
        for piece in body.split(";"):
            piece = piece.strip()
            if not piece:
                continue
            pm = re.match(r"(ch_in|ch_out)\s*<", piece)
            if not pm:
                continue
            lt = piece.index("<", pm.start())
            gt = _match_angle(piece, lt)
            if gt < 0:
                continue
            tail = piece[gt + 1:].strip()
            idm = re.match(r"([A-Za-z_][A-Za-z0-9_]*)", tail)
            if not idm:
                continue
            ident = idm.group(1)
            rest = tail[idm.end():].strip()
            name = None
            if rest.startswith("{"):
                nm = re.match(r"\{\s*\"([^\"]*)\"\s*\}", rest)
                if nm:
                    name = nm.group(1)
            out.append({
                "block": block_ord,
                "decl": _normalize(piece),
                "ident": ident,
                "name": name,
            })
    return out


def iter_product_files(root: pathlib.Path):
    for face in PROD_FACES:
        base = root / face
        if not base.is_dir():
            continue
        for p in sorted(base.rglob("*")):
            if p.is_file() and p.suffix in SOURCE_EXTS:
                yield p


def scan(root: pathlib.Path) -> list[dict]:
    """扫全树 ⇒ 返回全部端口条目（附**不合规**标记）。"""
    entries: list[dict] = []
    for p in iter_product_files(root):
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        rel = p.relative_to(root).as_posix()
        for port in extract_ports(text):
            compliant = port["name"] is not None and port["name"] == port["ident"]
            entries.append({
                "path": rel,
                "block": port["block"],
                "decl": port["decl"],
                "ident": port["ident"],
                "name": port["name"],
                "compliant": compliant,
                "kind": "unnamed" if port["name"] is None else (
                    "named-ok" if compliant else "named-mismatch"),
            })
    return entries


def identity(e: dict) -> str:
    """登记表条目身份 = `<文件>\\t<__io 块序号>\\t<声明文本>`。"""
    return f"{e['path']}\t{e['block']}\t{e['decl']}"


# ── 登记表（TSV）读写 ─────────────────────────────────────────────────
REG_HEADER = ("# 端口命名棘轮豁免表（机器面）\n"
              "# 由 `scripts/verify_port_naming.sh` 校验 ⇒ ⇒ **不要手改**\n"
              "# 格式：<文件>\\t<__io 块序号>\\t<声明文本>[\\t<原因>]\n"
              "# ⚠️ 身份是**逐端口**的（不是文件哈希）—— 哈希对任何注释改动都失效\n")


def write_registry(path: pathlib.Path, entries: list[dict]) -> None:
    rows = []
    for e in sorted((x for x in entries if not x["compliant"]),
                    key=lambda x: (x["path"], x["block"], x["decl"])):
        rows.append(f"{e['path']}\t{e['block']}\t{e['decl']}\t既有债·基线豁免")
    path.write_text(REG_HEADER + "\n".join(rows) + ("\n" if rows else ""), encoding="utf-8")


def read_registry(path: pathlib.Path) -> tuple[dict[str, str], list[str]]:
    """读登记表 ⇒ 返回（身份 → 原因，解析错误列表）。"""
    table: dict[str, str] = {}
    errs: list[str] = []
    if not path.exists():
        return table, [f"登记表不存在：{path}"]
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        parts = raw.split("\t")
        if len(parts) < 3:
            errs.append(f"第 {lineno} 行不是 ≥3 列（实际 {len(parts)}）")
            continue
        key = "\t".join(parts[:3])
        reason = parts[3] if len(parts) > 3 else ""
        if key in table:
            errs.append(f"第 {lineno} 行身份重复：{key}")
        table[key] = reason
    return table, errs


# ── 判定 ───────────────────────────────────────────────────────────
def git(root: pathlib.Path, *args: str) -> tuple[int, str]:
    r = subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True)
    return r.returncode, r.stdout


def list_new_ports(root: pathlib.Path, base: str, head: str) -> list[str]:
    """增量：base..head 里**新增**的端口声明身份（报告用）。"""
    rc, out = git(root, "diff", "-U0", f"{base}...{head}")
    if rc != 0:
        return []
    added: set[str] = set()
    cur_file = ""
    for line in out.splitlines():
        if line.startswith("+++ b/"):
            cur_file = line[6:].strip()
            continue
        if not line.startswith("+") or line.startswith("+++"):
            continue
        for port in extract_ports(line[1:]):
            added.add(f"{cur_file}\t{port['block']}\t{port['decl']}")
    return sorted(added)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--registry")
    ap.add_argument("--write-registry")
    ap.add_argument("--expect-count", type=int, default=None)
    ap.add_argument("--base")
    ap.add_argument("--head", default="HEAD")
    ap.add_argument("--list-incremental", action="store_true")
    args = ap.parse_args()

    root = pathlib.Path(args.root).resolve()
    entries = scan(root)

    if args.write_registry:
        out = pathlib.Path(args.write_registry)
        out.parent.mkdir(parents=True, exist_ok=True)
        write_registry(out, entries)
        bad = [e for e in entries if not e["compliant"]]
        print(f"wrote {out} · 端口总数 = {len(entries)} · 不合规 = {len(bad)}")
        return 0

    if args.list_incremental:
        if not args.base:
            print("::error --list-incremental 需要 --base", file=sys.stderr)
            return 2
        for line in list_new_ports(root, args.base, args.head):
            print(line)
        return 0

    if not args.registry:
        print("::error 需要 --registry 或 --write-registry", file=sys.stderr)
        return 2

    reg_path = pathlib.Path(args.registry)
    table, errs = read_registry(reg_path)
    bad = [e for e in entries if not e["compliant"]]
    tree_ids = {identity(e) for e in bad}

    rc = 0
    for e in errs:
        print(f"::error::登记表格式错误：{e}")
        rc = 1

    # ① 正向：树里有、登记表里没有 ⇒ 债增长
    for key in sorted(tree_ids - set(table)):
        print(f"::error::未登记的非合规端口（债增长 ⇒ MUST 具名或登记）：{key}")
        rc = 1
    # ② 反向：登记表里有、树里没有 ⇒ 登记表烂掉
    for key in sorted(set(table) - tree_ids):
        print(f"::error::登记表的条目在树里已不存在（登记表 MUST 同步）：{key}")
        rc = 1

    # ③ 计数锚点
    if args.expect_count is not None and len(table) != args.expect_count:
        print(f"::error::登记表条目数 = {len(table)}，锚点 EXPECTED_EXEMPT_PORTS = {args.expect_count}"
              f"（增删 MUST 同步锚点）")
        rc = 1

    if rc == 0:
        print(f"OK：端口总数 = {len(entries)} · 不合规 = {len(bad)} · 登记 = {len(table)}"
              f" · 逐端口双向一致")
    return rc


if __name__ == "__main__":
    sys.exit(main())
