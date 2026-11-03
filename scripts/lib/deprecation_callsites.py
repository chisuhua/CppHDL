#!/usr/bin/env python3
"""**HDL 逻辑算子（`&&` / `||`）弃用状态**的判据（ADR-036 落地后的常态化护栏）。

⚠️ **为什么要有它**（2026-11-03）：
   ADR-036 把 8 个 `ch_bool` / `port<ch_bool>` 的 `&&` / `||` overload 标记为
   `[[deprecated]]`，并把「**0 新增调用点**」写成 phase-5 的完成条件 ⇒ ⇒ 但
   **没有任何 CI 判据守它** ⇒ ⇒ 「完成条件」只是一次性人工核对 ⇒ ⇒ 今后新增
   一处误用，CI **不会红**（只会在编译日志里留一条 deprecation 警告，而当前构建
   不是 `-Werror`）⇒ ⇒ 本判据把该条件变成 **CI 可检**。

⚠️ **判据分三条**：
   R1 **8 个弃用标记仍在位**：登记表列的 8 个定义点 MUST 仍带 `[[deprecated(`；
      且两个头里 `[[deprecated(` 的**总数** MUST 恰好 = 8（多出来 ⇒ 有 overload
      打了标记却没登记 ⇒ 漏登记）。
   R2 **0 新增调用点**：全仓扫描到的 `&&` / `||` 且操作数是 `ch_bool` /
      `port<ch_bool>` 语义的调用点，MUST **全部**在登记表的豁免清单里。
   R3 **豁免登记不得漂移**：登记表里的每一条豁免 MUST 仍能在扫描结果里找到
      （豁免已过时 ⇒ 红）⇒ ⇒ 与 R2 构成**双向**核对。

⚠️ **判定精度（宁可漏报、不误报）**：
   · 操作数扫描**按括号配平**：`)` ⇒ 跳到配对 `(`；反向遇到 `(` ⇒ 终止
     （说明运算符位于某个调用的实参表里 ⇒ 操作数就是括号后的部分）；
   · 孤立的 `=`（赋值）终止操作数；`==` / `!=` / `<=` / `>=` 里的 `=` 属操作数；
   · **比较表达式**的结果是 `bool` ⇒ **不是**弃用 overload 的操作数
     （`flag1 == other.flag1 && flag2 == other.flag2` 里的 `&&` 是内建 bool 运算）；
   · `if constexpr` / `static_assert` 行、**overload 定义行**（`operator&&`）
     一律跳过；
   ⇒ ⇒ 2026-11-03 实测：全仓命中 **7** 处，**全部**是登记在案的豁免调用点
   ⇒ ⇒ 误报 = 0。

⚠️ **豁免为何必须保留**（ADR-036 §4）：mixed overload 的语义是 **nonzero-test
   归约**（`true && 0b10` = 1，而 `true & 0b10` = 0）⇒ ⇒ naive 重写会**静默改
   硬件语义** ⇒ ⇒ 那 4 条 mixed 用例 + 3 条 SEGV 复现 MUST 保留到物理移除那一刻。

用法：
    python3 scripts/lib/deprecation_callsites.py check
    python3 scripts/lib/deprecation_callsites.py list-exemptions
    python3 scripts/lib/deprecation_callsites.py poison <名字>
    python3 scripts/lib/deprecation_callsites.py restore
    python3 scripts/lib/deprecation_callsites.py list-poisons
"""
from __future__ import annotations

import pathlib
import re
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
REGISTRY = ROOT / ".rddf" / "checks" / "hdl-logical-deprecation-registry.md"
SCAN_DIRS = ("include", "src", "tests", "examples", "samples")
EXTS = {".h", ".cpp", ".cc", ".cxx", ".C"}

SNAPSHOT_DIR = pathlib.Path("/tmp/opencode/deprecation_callsites.snapshot")
POISON_TARGET = pathlib.Path("/tmp/opencode/deprecation_callsites.target")

GREEN = "\033[0;32m"
RED = "\033[0;31m"
YELLOW = "\033[0;33m"
RESET = "\033[0m"

FAILS = 0


def ok(msg: str) -> None:
    print(f"  {GREEN}✅ {msg}{RESET}")


def fail(msg: str) -> None:
    global FAILS
    FAILS += 1
    print(f"  {RED}❌ {msg}{RESET}")


def note(msg: str) -> None:
    print(f"  {YELLOW}::notice {msg}{RESET}")


# ────────────────────────────────────────────────────────────────────────────
# 词法：剥注释与字符串（避免文档文本造成误报）
# ────────────────────────────────────────────────────────────────────────────
def strip_comments_and_strings(text: str) -> str:
    out: list[str] = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                if text[i] == "\\":
                    i += 1
                i += 1
            i += 1
            out.append('""')
            continue
        if c == "'":
            i += 1
            while i < n and text[i] != "'":
                if text[i] == "\\":
                    i += 1
                i += 1
            i += 1
            out.append("''")
            continue
        out.append(c)
        i += 1
    return "".join(out)


OPCHARS = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_."
              "><[]:()-+*~!=&|, \t")
BOUNDARY = set(",;{}")
CMP_OPS = ("==", "!=", "<=", ">=", "<", ">")


def _eq_is_part_of_cmp(line: str, i: int) -> bool:
    """i 指向 '='；判断它是否属于 == / != / <= / >=（是 ⇒ 属操作数；否 ⇒ 属赋值 ⇒ 终止）。"""
    if i > 0 and line[i - 1] in "=!":
        return True
    if i + 1 < len(line) and line[i + 1] == "=":
        return True
    if i + 1 < len(line) and line[i + 1] in "<>":
        return True
    return False


def split_operands(line: str, pos: int) -> tuple[str, str]:
    """取运算符两侧的**最外层操作数**（括号配平；见模块 docstring）。"""
    j = pos - 1
    while j >= 0:
        c = line[j]
        if c == ")":
            depth = 0
            while j >= 0:
                if line[j] == ")":
                    depth += 1
                elif line[j] == "(":
                    depth -= 1
                    if depth == 0:
                        break
                j -= 1
            j -= 1
            continue
        if c == "(" or c in BOUNDARY:
            break
        if c == "=" and not _eq_is_part_of_cmp(line, j):
            break
        if c not in OPCHARS:
            break
        j -= 1
    left = line[j + 1:pos]
    k = pos + 2
    while k < len(line):
        c = line[k]
        if c == "(" or c in BOUNDARY:
            break
        if c == "=" and not _eq_is_part_of_cmp(line, k):
            break
        if c not in OPCHARS:
            break
        k += 1
    right = line[pos + 2:k]
    return left.strip(), right.strip()


def strip_outer_parens(t: str) -> str:
    t = t.strip()
    while t.startswith("(") and t.endswith(")"):
        depth = 0
        ok_ = True
        for i, ch in enumerate(t):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0 and i != len(t) - 1:
                    ok_ = False
                    break
        if not ok_:
            break
        t = t[1:-1].strip()
    return t


def has_cmp(t: str) -> bool:
    """操作数里（任意深度）是否含比较运算符 ⇒ ⇒ 其值是 bool ⇒ 不是弃用 overload。"""
    for op in CMP_OPS:
        if op in t:
            return True
    return False


def is_bool_result(t: str) -> bool:
    t = strip_outer_parens(t)
    if re.match(r"^static_cast\s*<\s*bool\s*>", t):
        return True
    if t in ("true", "false"):
        return True
    return has_cmp(t)


def chbool_names(text: str) -> set[str]:
    names: set[str] = set()
    for pat in (r"\bch_bool\s+(\w+)", r"\bch_bool\s*<\s*\d+\s*>\s+(\w+)",
                r"\bport\s*<\s*ch_bool\s*>\s+(\w+)"):
        for m in re.finditer(pat, text):
            names.add(m.group(1))
    for m in re.finditer(r"\bauto\s+(\w+)\s*=\s*[^;\n]*\bch_bool\b", text):
        names.add(m.group(1))
    return names


def operand_is_chbool(op: str, names: set[str]) -> bool:
    if not op:
        return False
    if is_bool_result(op):
        return False
    if "ch_bool" in op or "port<" in op:
        return True
    for n in names:
        if re.search(r"\b" + re.escape(n) + r"\b", op):
            return True
    return False


def scan_callsites() -> list[tuple[str, int, str, str]]:
    hits: list[tuple[str, int, str, str]] = []
    for d in SCAN_DIRS:
        base = ROOT / d
        if not base.is_dir():
            continue
        for p in sorted(base.rglob("*")):
            if p.suffix not in EXTS or not p.is_file():
                continue
            text = strip_comments_and_strings(p.read_text(errors="replace"))
            names = chbool_names(text)
            rel = str(p.relative_to(ROOT))
            for ln, line in enumerate(text.splitlines(), 1):
                if "if constexpr" in line or "static_assert" in line:
                    continue
                if "operator&&" in line or "operator||" in line:
                    continue
                for sym in ("&&", "||"):
                    start = 0
                    while True:
                        k = line.find(sym, start)
                        if k < 0:
                            break
                        start = k + 2
                        left, right = split_operands(line, k)
                        if operand_is_chbool(left, names) or operand_is_chbool(right, names):
                            hits.append((rel, ln, sym, line.strip()))
    return hits


# ────────────────────────────────────────────────────────────────────────────
# 登记表解析
# ────────────────────────────────────────────────────────────────────────────
ROW_RE = re.compile(r"^\|\s*\d+\s*\|\s*`([^`]+)`\s*\|")


def parse_registry() -> tuple[list[tuple[str, int]], list[tuple[str, int]]]:
    """返回 (弃用标记点, 豁免调用点)，均为 (相对路径, 行号)。"""
    text = REGISTRY.read_text(encoding="utf-8")
    lines = text.splitlines()
    section = ""
    markers: list[tuple[str, int]] = []
    exemptions: list[tuple[str, int]] = []
    for line in lines:
        if line.startswith("## "):
            section = line[3:].strip()
            continue
        m = ROW_RE.match(line)
        if not m:
            continue
        loc = m.group(1)
        if ":" not in loc:
            continue
        path, _, num = loc.rpartition(":")
        if not num.isdigit():
            continue
        entry = (path, int(num))
        if section.startswith("1."):
            markers.append(entry)
        elif section.startswith("2."):
            exemptions.append(entry)
    return markers, exemptions


# ────────────────────────────────────────────────────────────────────────────
# 判据
# ────────────────────────────────────────────────────────────────────────────
def r1_markers(markers: list[tuple[str, int]]) -> None:
    if not markers:
        fail("R1 登记表里没有弃用标记点 ⇒ 无法核对（fail-closed）")
        return
    bad = []
    for path, ln in markers:
        f = ROOT / path
        if not f.exists():
            bad.append(f"{path}:{ln}（文件不存在）")
            continue
        content = f.read_text(errors="replace").splitlines()
        if ln > len(content) or "[[deprecated(" not in content[ln - 1]:
            bad.append(f"{path}:{ln}（该行已无 [[deprecated( 标记）")
    if bad:
        for b in bad:
            fail(f"R1 弃用标记缺失/漂移：{b}")
        fail("R1 处置：物理移除 MUST 走日历门（scripts/verify_deprecation_calendar_gate.sh）"
             "⇒ ⇒ 且须一并删除本护栏与登记表")
        return
    total = 0
    for path in sorted({p for p, _ in markers}):
        total += (ROOT / path).read_text(errors="replace").count("[[deprecated(")
    if total != len(markers):
        fail(f"R1 两个头里 [[deprecated( 的总数 = {total}，登记表登记 {len(markers)} 条"
             "⇒ ⇒ 有 overload 打了标记却没登记（或登记已过时）")
        return
    ok(f"R1 8 个弃用标记全部在位（总数 = {total}）")


def r2_no_new_callsites(exemptions: list[tuple[str, int]]) -> list[tuple[str, int, str, str]]:
    hits = scan_callsites()
    exempt_set = set(exemptions)
    unexpected = [h for h in hits if (h[0], h[1]) not in exempt_set]
    if unexpected:
        for rel, ln, sym, text in unexpected:
            fail(f"R2 新增弃用调用点（未登记豁免）：{rel}:{ln}  [{sym}]  {text[:90]}")
        fail("R2 处置：改用 `&` / `|`（mixed 语义 MUST 写成显式 nonzero-test 形式）"
             "⇒ ⇒ 或在登记表第 2 节登记豁免并写明理由")
        return hits
    ok(f"R2 无新增弃用调用点（扫描命中 {len(hits)} 处，全部在豁免清单内）")
    return hits


def r3_exemptions_current(hits: list[tuple[str, int, str, str]],
                           exemptions: list[tuple[str, int]]) -> None:
    found = {(h[0], h[1]) for h in hits}
    missing = [e for e in exemptions if e not in found]
    if missing:
        for path, ln in missing:
            fail(f"R3 豁免登记已过时（该处已无弃用调用点）：{path}:{ln}")
        return
    ok(f"R3 {len(exemptions)} 条豁免登记与现实逐条相等")


def check() -> int:
    global FAILS
    FAILS = 0
    if not REGISTRY.exists():
        fail(f"登记表不存在：{REGISTRY.relative_to(ROOT)} ⇒ fail-closed")
        return 1
    markers, exemptions = parse_registry()
    r1_markers(markers)
    hits = r2_no_new_callsites(exemptions)
    r3_exemptions_current(hits, exemptions)
    if FAILS:
        print(f"{RED}❌ 弃用调用点核验 FAIL（{FAILS} 项）{RESET}")
        return 1
    print(f"{GREEN}✅ 弃用调用点核验通过{RESET}")
    return 0


# ────────────────────────────────────────────────────────────────────────────
# 自检用的原地投毒
# ────────────────────────────────────────────────────────────────────────────
POISONS: dict[str, tuple[str, callable]] = {}


def _poison_add_callsite() -> None:
    """在测试里注入一处新的 `ch_bool && ch_bool` 调用点 ⇒ R2 必红。"""
    f = ROOT / "tests" / "test_basic.cpp"
    text = f.read_text(encoding="utf-8")
    anchor = "TEST_CASE("
    idx = text.find(anchor)
    assert idx >= 0, "找不到 TEST_CASE 锚点"
    inject = ("// dep-guard poison: 新增弃用调用点（自检用，结束后还原）\n"
              "    ch_bool _dep_guard_probe = ch_bool(true) && ch_bool(true);\n")
    f.write_text(text[:idx] + inject + text[idx:], encoding="utf-8")


def _poison_remove_marker() -> None:
    """删掉一个弃用标记 ⇒ R1 必红。"""
    f = ROOT / "include" / "core" / "operators_logic.h"
    lines = f.read_text(encoding="utf-8").splitlines(keepends=True)
    for i, line in enumerate(lines):
        if line.startswith("[[deprecated("):
            lines[i] = line.replace("[[deprecated(", "[[deprecate_x(", 1)
            break
    else:
        raise AssertionError("找不到弃用标记行")
    f.write_text("".join(lines), encoding="utf-8")


def _poison_drop_registration() -> None:
    """删掉登记表里的一条豁免 ⇒ R2（未登记）+ R3（登记过时）必红。"""
    lines = REGISTRY.read_text(encoding="utf-8").splitlines(keepends=True)
    for i, line in enumerate(lines):
        if ROW_RE.match(line) and "test_operator_advanced.cpp:540" in line:
            del lines[i]
            break
    else:
        raise AssertionError("找不到要删的登记行")
    REGISTRY.write_text("".join(lines), encoding="utf-8")


def _poison_stale_registration() -> None:
    """把登记的行号改错 ⇒ R2 + R3 必红。"""
    lines = REGISTRY.read_text(encoding="utf-8").splitlines(keepends=True)
    for i, line in enumerate(lines):
        if ROW_RE.match(line) and "test_operator_advanced.cpp:541" in line:
            lines[i] = line.replace("test_operator_advanced.cpp:541",
                                    "test_operator_advanced.cpp:999")
            break
    else:
        raise AssertionError("找不到要改的登记行")
    REGISTRY.write_text("".join(lines), encoding="utf-8")


def _poison_calendar_remove() -> None:
    """删掉**全部**弃用标记 ⇒ **未提交的移除** ⇒ 日历门 MUST 转红（今天 < 期限）。"""
    for f in (ROOT / "include" / "core" / "operators_logic.h",
              ROOT / "include" / "core" / "io_logical.h"):
        lines = f.read_text(encoding="utf-8").splitlines(keepends=True)
        hit = False
        for i, line in enumerate(lines):
            if line.startswith("[[deprecated("):
                lines[i] = line.replace("[[deprecated(", "[[deprecate_x(", 1)
                hit = True
        if not hit:
            raise AssertionError(f"{f} 里找不到弃用标记行")
        f.write_text("".join(lines), encoding="utf-8")


def _poison_calendar_widen() -> None:
    """删掉一个标记 **且** 把最早移除日改到过去 ⇒ 日历门 MUST 转绿（反向判据）。

    ⚠️ 目的：证明「红」的成因是**日期**，而不是「标记消失」这件事本身。
    """
    _poison_calendar_remove()
    text = REGISTRY.read_text(encoding="utf-8")
    old = "| 最早允许移除日 | **2027-04-17** |"
    new = "| 最早允许移除日 | **2020-01-01** |"
    if old not in text:
        raise AssertionError("登记表里找不到「最早允许移除日」行")
    REGISTRY.write_text(text.replace(old, new), encoding="utf-8")


POISONS = {
    "add-callsite": ("注入一处新的 ch_bool && ch_bool 调用点（R2）", _poison_add_callsite),
    "remove-marker": ("删掉一个 [[deprecated]] 标记（R1）", _poison_remove_marker),
    "drop-registration": ("删掉一条豁免登记（R2 + R3）", _poison_drop_registration),
    "stale-registration": ("把一条豁免登记的行号改错（R2 + R3）", _poison_stale_registration),
    "calendar-remove": ("删掉一个弃用标记 ⇒ 未提交的移除（日历门 MUST 转红）",
                        _poison_calendar_remove),
    "calendar-widen-deadline": ("删掉标记 **且** 把最早移除日改到过去（日历门 MUST 转绿 = 反向判据）",
                                _poison_calendar_widen),
}

POISON_FILES = {
    "add-callsite": [ROOT / "tests" / "test_basic.cpp"],
    "remove-marker": [ROOT / "include" / "core" / "operators_logic.h"],
    "drop-registration": [REGISTRY],
    "stale-registration": [REGISTRY],
    "calendar-remove": [ROOT / "include" / "core" / "operators_logic.h",
                         ROOT / "include" / "core" / "io_logical.h"],
    "calendar-widen-deadline": [ROOT / "include" / "core" / "operators_logic.h",
                                ROOT / "include" / "core" / "io_logical.h", REGISTRY],
}


def list_poisons() -> int:
    for name, (desc, _) in POISONS.items():
        print(f"{name}\t{desc}")
    return 0


def _snapshot(paths: list[pathlib.Path]) -> None:
    if SNAPSHOT_DIR.exists():
        shutil.rmtree(SNAPSHOT_DIR)
    SNAPSHOT_DIR.mkdir(parents=True)
    for i, p in enumerate(paths):
        shutil.copyfile(p, SNAPSHOT_DIR / f"{i}.bak")


def poison(name: str) -> int:
    if name not in POISONS:
        print(f"未知投毒：{name}", file=sys.stderr)
        return 2
    desc, fn = POISONS[name]
    paths = POISON_FILES.get(name, [])
    if not paths:
        print(f"投毒 {name} 未登记目标文件", file=sys.stderr)
        return 2
    _snapshot(paths)
    try:
        fn()
    except AssertionError as exc:
        shutil.rmtree(SNAPSHOT_DIR, ignore_errors=True)
        print(f"投毒失败：{exc}", file=sys.stderr)
        return 2
    POISON_TARGET.write_text(name, encoding="utf-8")
    print(f"已投毒：{name}（{desc}）")
    return 0


def restore() -> int:
    if not SNAPSHOT_DIR.exists() or not POISON_TARGET.exists():
        print("没有投毒快照（⇒ 无需还原）", file=sys.stderr)
        return 1
    name = POISON_TARGET.read_text(encoding="utf-8").strip()
    paths = POISON_FILES.get(name)
    if not paths:
        print("投毒目标缺失 ⇒ 无法还原", file=sys.stderr)
        return 2
    for i, p in enumerate(paths):
        bak = SNAPSHOT_DIR / f"{i}.bak"
        if not bak.exists() or not p.exists():
            print("投毒快照或目标缺失 ⇒ 无法还原", file=sys.stderr)
            return 2
        # ⚠️ MUST 用 copyfile（刷新 mtime ⇒ 强制重建）—— 本仓踩过「还原源码 ≠ 复原
        #    （二进制也是状态）」的坑（scripts/lib/mutation_harness.py 同一纪律）。
        shutil.copyfile(bak, p)
    shutil.rmtree(SNAPSHOT_DIR, ignore_errors=True)
    POISON_TARGET.unlink(missing_ok=True)
    print(f"已还原：{name}")
    return 0


# ────────────────────────────────────────────────────────────────────────────
# 移除门（calendar gate）—— ADR-036 §8
# ────────────────────────────────────────────────────────────────────────────
MARKER_FILES = ("include/core/io_logical.h", "include/core/operators_logic.h")
EXPECTED_MARKERS = 8          # ADR-036 §2：8 个 overload（4 纯 + 4 mixed）


def _git(*args: str) -> tuple[int, str]:
    import subprocess
    r = subprocess.run(["git", "-C", str(ROOT), *args],
                       capture_output=True, text=True)
    return r.returncode, r.stdout


def parse_calendar() -> tuple[str, str, int, int]:
    """从登记表第 3 节解析 (落地日, 最早移除日, 低活动阈值, 顺延月数)。"""
    text = REGISTRY.read_text(encoding="utf-8")
    dep = base = None
    thr, ext = 5, 3
    m = re.search(r"\| deprecation 落地日 \|[^|]*\*\*(\d{4}-\d{2}-\d{2})\*\*", text)
    if m:
        dep = m.group(1)
    m = re.search(r"\| 最早允许移除日 \|[^|]*\*\*(\d{4}-\d{2}-\d{2})\*\*", text)
    if m:
        base = m.group(1)
    m = re.search(r"\| 低活动顺延[^|]*\*\*< (\d+)\*\*", text)
    if m:
        thr = int(m.group(1))
    m = re.search(r"\| 低活动顺延[^|]*\*\*(\d+) 个月\*\*", text)
    if m:
        ext = int(m.group(1))
    if not dep or not base:
        raise ValueError("登记表里读不到日历门的日期（fail-closed）")
    return dep, base, thr, ext


def _add_months(iso: str, months: int) -> str:
    y, m, d = (int(x) for x in iso.split("-"))
    total = y * 12 + (m - 1) + months
    ny, nm = total // 12, total % 12 + 1
    import calendar as _cal
    nd = min(d, _cal.monthrange(ny, nm)[1])
    return f"{ny:04d}-{nm:02d}-{nd:02d}"


def worktree_marker_count() -> int:
    return sum((ROOT / f).read_text(errors="replace").count("[[deprecated(")
               for f in MARKER_FILES if (ROOT / f).exists())


def rev_marker_count(rev: str) -> int | None:
    """某个 revision 里的标记总数；None = 该 revision 里两个文件都不存在。"""
    seen = False
    total = 0
    for f in MARKER_FILES:
        rc, out = _git("show", f"{rev}:{f}")
        if rc != 0:
            continue
        seen = True
        total += out.count("[[deprecated(")
    return total if seen else None


def removal_commit() -> tuple[str, str] | None:
    """找「标记由在位变为消失」的那个 commit ⇒ (sha, 日期)。"""
    rc, out = _git("log", "--reverse", "--format=%H", "--", *MARKER_FILES)
    if rc != 0:
        return None
    prev: int | None = None
    for sha in out.split():
        cur = rev_marker_count(sha)
        if prev is not None and prev > 0 and cur == 0:
            _, date = _git("show", "-s", "--format=%ad", "--date=short", sha)
            return sha, date.strip()
        prev = cur
    return None


def calendar_check() -> int:
    global FAILS
    FAILS = 0
    try:
        dep, base, thr, ext = parse_calendar()
    except ValueError as exc:
        fail(f"日历门参数缺失：{exc}")
        return 1
    ok(f"日历门参数：落地日 {dep} · 最早移除日 {base}"
       f"（低活动 < {thr} commits ⇒ 顺延 {ext} 个月）")

    total = worktree_marker_count()
    if total > 0 and total < EXPECTED_MARKERS:
        fail(f"弃用标记只剩 {total}/{EXPECTED_MARKERS} 个 ⇒ **部分移除**"
             " ⇒ ⇒ 8 个 overload MUST **一起**移除（ADR-036 §6）"
             " ⇒ ⇒ 且 7 处豁免调用点 MUST 同时改写")
        return 1
    if total > 0:
        ok(f"弃用标记仍在位（{total}/{EXPECTED_MARKERS}）⇒ **尚未移除**（最早允许 {base}）")
        print(f"{GREEN}✅ 移除门核验通过（未移除）{RESET}")
        return 0

    # 标记已全部消失 ⇒ 判定移除日期
    head_count = rev_marker_count("HEAD")
    if head_count is not None and head_count > 0:
        removal_date = _today()
        note("移除**尚未提交** ⇒ 移除日期按今天计")
    else:
        found = removal_commit()
        if found is None:
            fail("弃用标记已消失，但**无法判定移除日期**"
                 "（git 历史不足 / 移除不在历史里）⇒ ⇒ fail-closed")
            return 1
        _, removal_date = found

    rc, cnt_out = _git("rev-list", "--count", f"--since={dep}", f"--until={removal_date}", "HEAD")
    activity = int(cnt_out.strip()) if rc == 0 and cnt_out.strip().isdigit() else 0
    effective = base if activity >= thr else _add_months(base, ext)
    if activity < thr:
        note(f"低活动条款生效：[{dep}, {removal_date}] 内只有 {activity} 个 commit"
             f"（< {thr}）⇒ 生效期限顺延到 {effective}")
    else:
        note(f"活动度 [{dep}, {removal_date}] 内 {activity} 个 commit（≥ {thr}）⇒ 生效期限 {effective}")

    if removal_date >= effective:
        ok(f"移除日期 {removal_date} ≥ 生效期限 {effective} ⇒ 合规")
        print(f"{GREEN}✅ 移除门核验通过（移除合规）{RESET}")
        return 0
    fail(f"移除日期 {removal_date} **早于**生效期限 {effective}"
         f" ⇒ ⇒ 违反 ADR-036 §8.1 的日历锁")
    return 1


def _today() -> str:
    import datetime
    return datetime.date.today().isoformat()


def list_exemptions() -> int:
    _, exemptions = parse_registry()
    for path, ln in exemptions:
        print(f"{path}:{ln}")
    return 0


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__)
        return 2
    cmd = argv[0]
    if cmd == "check":
        return check()
    if cmd == "list-exemptions":
        return list_exemptions()
    if cmd == "calendar-check":
        return calendar_check()
    if cmd == "list-poisons":
        return list_poisons()
    if cmd == "poison":
        return poison(argv[1] if len(argv) > 1 else "")
    if cmd == "restore":
        return restore()
    print(f"未知命令：{cmd}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
