#!/usr/bin/env python3
"""契约 `chlib-stream-arbitration` 的**命名诚实性登记表**判据（B8）。

⚠️ **为什么要有它**：本仓反复出现「**名字/注释承诺了不存在的行为**」这一族缺陷
   （`fwft_fifo` 承诺 FWFT 实则不是 · `stream_arbiter_sequence` 注释承诺
   round-robin 实则 `priority_encoder` · `stream_arbiter_round_robin` 承诺轮转
   实则恒传 0）⇒ ⇒ ⇒ **每次都靠「临时普查」发现 ⇒ ⇒ 下次还会漏**。
   ⇒ ⇒ 本判据把「登记表」与「实现 / 测试」**机械对齐** ⇒ ⇒ ⇒ 退化可被检出。

⚠️ **判据分三条**：
   R1 **登记 ⊆ 组件全集**：登记表里的每个组件 MUST 仍存在于 `include/chlib/*.h`
      ⇒ ⇒ 组件被删了却忘删登记 ⇒ ⇒ **红**（登记漂移）。
   R2 **判别性用例 MUST 存在**：每条登记的「判别性用例」文件 MUST 存在，且
      其 tag / macro MUST 在该文件里出现（**剥注释**后）⇒ ⇒ 用例被删 / 被改名
      ⇒ ⇒ **红**（判别力消失）。
   R3 **per-change 义务**：**最近的**归档 change（其 proposal 提到
      `include/chlib/`）MUST 用机器可读标记声明它触及的组件
      （`<!-- touched-components: a, b -->`）⇒ ⇒ 那些组件 MUST 在登记表里
      ⇒ ⇒ 「新增组件忘了登记」⇒ ⇒ **红**。
      ⚠️ **为什么不给「组件全集」做差集**：那正是 B8 裁决时被**明确否决**的
      「全仓普查」义务（⇒ ⇒ 全仓 27 个公开组合子里大量只有结构断言 ⇒ ⇒
      立刻全红 ⇒ ⇒ 义务不可达成）⇒ ⇒ 本判据只覆盖「**被改动过 / 名字说过
      谎**」的组件（登记表的显式范围）⇒ ⇒ **那一格的兜底是评审**（如实登记）。

用法：
    python3 scripts/lib/chlib_naming_honesty.py check
    python3 scripts/lib/chlib_naming_honesty.py poison <名字>
    python3 scripts/lib/chlib_naming_honesty.py restore
    python3 scripts/lib/chlib_naming_honesty.py list-poisons
"""
from __future__ import annotations

import pathlib
import re
import shutil
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
REGISTRY = REPO / ".rddf" / "checks" / "chlib-naming-honesty-registry.md"
CHLIB_DIR = REPO / "include" / "chlib"
TEST_DIRS = (REPO / "tests", REPO / "examples")
ARCHIVE_DIR = REPO / "openspec" / "changes" / "archive"

SNAPSHOT = pathlib.Path("/tmp/opencode/chlib_naming_honesty.snapshot")
POISON_TARGET = pathlib.Path("/tmp/opencode/chlib_naming_honesty.target")

GREEN = "\033[0;32m"
RED = "\033[0;31m"
RESET = "\033[0m"

FAILS = 0


def ok(msg: str) -> None:
    print(f"  {GREEN}✅ {msg}{RESET}")


def fail(msg: str) -> None:
    global FAILS
    FAILS += 1
    print(f"  {RED}❌ {msg}{RESET}")


def notice(msg: str) -> None:
    print(f"  \033[0;33m::notice {msg}{RESET}")


def strip_comments_and_strings(src: str) -> str:
    """剥掉注释与字符串字面量 ⇒ 「代码里真的写了什么」。

    ⚠️ **必须复用仓库既有实现**（`scripts/lib/fsm_wiring.py`）⇒ ⇒ 两份护栏
    对「什么算真实代码」的口径**不会漂移**。
    """
    sys.path.insert(0, str(REPO / "scripts" / "lib"))
    from fsm_wiring import strip_comments_and_strings as impl  # noqa: PLC0415

    return impl(src)


# ── 登记表解析 ────────────────────────────────────────────────────────────
ROW_RE = re.compile(r"^\|\s*`([A-Za-z_]\w*)`\s*\|(.+?)\|(.+?)\|(.+?)\|\s*$")


def parse_registry() -> list[tuple[str, str, str]]:
    """返回 [(组件, 判别性用例列, 登记原因列)]。"""
    if not REGISTRY.exists():
        return []
    out = []
    for line in REGISTRY.read_text(encoding="utf-8").split("\n"):
        m = ROW_RE.match(line.strip())
        if not m:
            continue
        out.append((m.group(1), m.group(3).strip(), m.group(4).strip()))
    return out


def parse_judge_cell(cell: str) -> tuple[str, str]:
    """「文件` · `tag`」⇒ (文件, 标记)。"""
    parts = [p.strip() for p in cell.split("·")]
    files = [p.strip("` ") for p in parts if "/" in p]
    tags = [p.strip("` ") for p in parts if "/" not in p]
    if not files or not tags:
        return "", ""
    return files[0], tags[0]


def component_exists(name: str) -> bool:
    for h in sorted(CHLIB_DIR.glob("*.h")):
        code = strip_comments_and_strings(h.read_text(encoding="utf-8"))
        if re.search(rf"(?<![.\w>:]){re.escape(name)}\s*[(<]", code):
            return True
    return False


def strip_comments_only(src: str) -> str:
    """只剥注释，**保留字符串字面量**。

    ⚠️ **为什么不能复用 `strip_comments_and_strings`**：判别性用例的 tag
    （如 `[stream][arbiter][round_robin][rotation]`）**就住在字符串字面量里**
    ⇒ ⇒ 连字符串一起剥 ⇒ ⇒ tag 永远找不到 ⇒ ⇒ 判据恒红。
    ⇒ ⇒ 折中：剥注释（避免「注释里提到 tag」误判）+ 留字符串。
    """
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            while i < n and src[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            i += 2
            while i + 1 < n and not (src[i] == "*" and src[i + 1] == "/"):
                if src[i] == "\n":
                    out.append("\n")
                i += 1
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def marker_in_file(path: pathlib.Path, marker: str) -> bool:
    if not path.exists():
        return False
    # ⚠️ tag 住在**字符串字面量**里 ⇒ ⇒ 只剥注释、留字符串
    return marker in strip_comments_only(path.read_text(encoding="utf-8"))


# ── R1 ─────────────────────────────────────────────────────────────────────
def r1() -> None:
    """登记 ⊆ 组件全集（登记漂移检测）。"""
    rows = parse_registry()
    if not rows:
        fail("R1 登记表为空或无法解析 ⇒ ⇒ 命名诚实性无任何登记")
        return
    missing = [name for name, _, _ in rows if not component_exists(name)]
    if missing:
        fail("R1 登记的组件**已不存在**于 include/chlib/：" + " / ".join(missing)
             + "（⇒ ⇒ 组件被删却忘删登记 ⇒ ⇒ 登记表漂移）")
        return
    ok(f"R1 登记的 {len(rows)} 个组件全部仍存在于 include/chlib/")


# ── R2 ─────────────────────────────────────────────────────────────────────
def r2() -> None:
    """每条登记的判别性用例 MUST 存在（文件 + tag/macro）。"""
    rows = parse_registry()
    bad = []
    for name, judge_cell, _ in rows:
        f, marker = parse_judge_cell(judge_cell)
        if not f or not marker:
            bad.append(f"{name}（判别性用例列格式不可解析：{judge_cell!r}）")
            continue
        p = REPO / f
        if not p.exists():
            bad.append(f"{name}（判据文件不存在：{f}）")
            continue
        if not marker_in_file(p, marker):
            bad.append(f"{name}（{f} 里找不到 {marker} ⇒ ⇒ 判别力消失）")
    if bad:
        fail("R2 判别性用例缺失：" + "；".join(bad))
        return
    ok(f"R2 {len(rows)} 条登记的判别性用例全部存在且仍带其 tag/macro")


# ── R3 ─────────────────────────────────────────────────────────────────────
MARKER_RE = re.compile(r"<!--\s*touched-components:\s*(.*?)\s*-->")


def latest_chlib_change() -> pathlib.Path | None:
    """最近的、proposal 提到 `include/chlib/` 的归档 change。"""
    if not ARCHIVE_DIR.exists():
        return None
    for d in sorted(ARCHIVE_DIR.iterdir(), reverse=True):
        p = d / "proposal.md"
        if not p.exists():
            continue
        if "include/chlib/" in p.read_text(encoding="utf-8"):
            return p
    return None


def r3() -> None:
    """per-change 义务：最近的 chlib change MUST 声明它触及的组件。"""
    rows = parse_registry()
    registered = {name for name, _, _ in rows}
    proposal = latest_chlib_change()
    if proposal is None:
        notice("R3 找不到提到 include/chlib/ 的归档 change ⇒ 跳过（无归档 ⇒ 无义务）")
        return
    text = proposal.read_text(encoding="utf-8")
    m = MARKER_RE.search(text)
    if not m:
        fail(f"R3 最近的 chlib change（{proposal.parent.name}）的 proposal "
             "MUST 用 `<!-- touched-components: ... -->` 声明它触及的组件"
             "（⇒ ⇒ 否则「新增组件忘了登记」这一格完全不可抓）")
        return
    touched = [t.strip() for t in m.group(1).split(",") if t.strip()]
    unknown = [t for t in touched if t not in registered]
    if unknown:
        fail(f"R3 change {proposal.parent.name} 触及的组件**未在登记表登记**："
             + " / ".join(unknown)
             + "（⇒ ⇒ 每个被改动的组件 MUST 有判别性行为用例 ⇒ ⇒ 忘记登记"
               "= 命名诚实性失去守护）")
        return
    ok(f"R3 最近的 chlib change（{proposal.parent.name}）声明的 "
       f"{len(touched)} 个组件全部已登记")


# ── 投毒（自检用）──────────────────────────────────────────────────────────
def _poison_drop_row(lines: list[str]) -> list[str]:
    for i, l in enumerate(lines):
        if ROW_RE.match(l.strip()):
            del lines[i]
            return lines
    raise RuntimeError("登记表里没有可删的行")


def _poison_bogus_component(lines: list[str]) -> list[str]:
    for i, l in enumerate(lines):
        if ROW_RE.match(l.strip()):
            lines[i] = l.replace("| `", "| `stream_not_a_real_component_", 1)
            return lines
    raise RuntimeError("登记表里没有可改的行")


def _poison_drop_marker(lines: list[str]) -> list[str]:
    for i, l in enumerate(lines):
        if ROW_RE.match(l.strip()):
            parts = l.split("|")
            parts[3] = " `tests/chlib/test_stream.cpp` · `[stream][no_such_tag]` "
            lines[i] = "|".join(parts)
            return lines
    raise RuntimeError("登记表里没有可改的行")


POISONS: dict[str, tuple[str, object]] = {
    "drop-registry-row": (
        "删掉一条登记 ⇒ ⇒ R1 计数变化（但 R1 不会因此红 ⇒ ⇒ 由 R2/R3 兜底）"
        " ⇒ ⇒ 期望：**R2 或 R3 转红**（被删组件若是最近 change 触及的 ⇒ R3 红）",
        _poison_drop_row,
    ),
    "bogus-component": (
        "把登记的组件名改成一个**不存在**的 ⇒ ⇒ R1 MUST 转红",
        _poison_bogus_component,
    ),
    "drop-judge-marker": (
        "把判别性用例的 tag 改成不存在的 ⇒ ⇒ R2 MUST 转红",
        _poison_drop_marker,
    ),
}


def poison(name: str) -> int:
    if name not in POISONS:
        print(f"未知投毒：{name}", file=sys.stderr)
        return 2
    if not REGISTRY.exists():
        print("登记表不存在", file=sys.stderr)
        return 2
    shutil.copyfile(REGISTRY, SNAPSHOT)
    POISON_TARGET.write_text(name, encoding="utf-8")
    lines = REGISTRY.read_text(encoding="utf-8").split("\n")
    lines = POISONS[name][1](lines)  # type: ignore[operator]
    REGISTRY.write_text("\n".join(lines), encoding="utf-8")
    print(f"已投毒：{name}")
    return 0


def restore() -> int:
    if not SNAPSHOT.exists() or not POISON_TARGET.exists():
        print("没有投毒快照（⇒ 无需还原）", file=sys.stderr)
        return 1
    shutil.copyfile(SNAPSHOT, REGISTRY)
    SNAPSHOT.unlink(missing_ok=True)
    POISON_TARGET.unlink(missing_ok=True)
    print("已还原")
    return 0


def check() -> int:
    global FAILS
    FAILS = 0
    r1()
    r2()
    r3()
    if FAILS:
        print(f"{RED}❌ 命名诚实性登记表核验 FAIL（{FAILS} 项）{RESET}")
        return 1
    print(f"{GREEN}✅ 命名诚实性登记表核验全部通过{RESET}")
    return 0


def main() -> int:
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    if cmd == "check":
        return check()
    if cmd == "poison":
        return poison(sys.argv[2] if len(sys.argv) > 2 else "")
    if cmd == "restore":
        return restore()
    if cmd == "list-poisons":
        for k in POISONS:
            print(k)
        return 0
    print(f"未知命令：{cmd}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
