#!/usr/bin/env python3
"""**stream 域收口清单**（`.rddf/checks/stream-domain-closure.md`）的机械核验器。

⚠️ **为什么要有这个**：本仓登记的病症之一是「**未收口项散落多处** ⇒ ⇒
无法宣告收官」—— 实测：stream 域的未兑现项分散在 **4 处**
（capability Purpose 的「范围限定」段 + 多个 improvement 台账 + ADR 的
「不在本 ADR 范围」段）⇒ ⇒ ⇒ **没人能一眼看出「还欠什么」**。

⇒ ⇒ 本核验器把收口清单变成**可机械核验**的矩阵 ⇒ ⇒ 且**状态列不是手写
的事实声明** ⇒ ⇒ **由「契约 / 判据 / 护栏」三个指针的集合推导** ⇒ ⇒
⇒ ⇒ **清单不可能自相矛盾**（「说谎」在结构上不可达）。

⚠️⚠️ **本文件与 `stream_ready_ownership.py` 共用同一个剥注释器**
（`fsm_wiring.strip_comments_and_strings`）⇒ ⇒ 两份护栏对「什么算真实
代码 / 真实文本」的口径**不会漂移**。

判据：
    R0（fail-closed）：清单存在 + 表头存在 + 数据行数 ≥ 阈值
                       （⇒ ⇒ 清单被清空 ⇒ ⇒ **假绿** ⇒ ⇒ 必须转红）
    R1：每个指针 MUST 可解析（文件存在 + 标记命中）
    R2：**状态一致性**
        · ✅ ⇒ MUST 同时有 `spec:` + `test:` + `guard:`（**三者对齐**）
        · ❌ ⇒ MUST 有 `debt:`（欠账落到真实文件）⇒ ⇒ 且 MUST NOT 同时
          具备 `test:` 与 `guard:`（⇒ ⇒ 否则 = **状态过时**）

用法：
    python3 scripts/lib/stream_domain_closure.py check
    python3 scripts/lib/stream_domain_closure.py poison <名字>
    python3 scripts/lib/stream_domain_closure.py restore
    python3 scripts/lib/stream_domain_closure.py list-poisons
"""
from __future__ import annotations

import pathlib
import re
import shutil
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
CHECKLIST = REPO / ".rddf" / "checks" / "stream-domain-closure.md"

BACKUP = pathlib.Path("/tmp/opencode/stream_domain_closure.bak")
POISONED_SNAPSHOT = pathlib.Path("/tmp/opencode/stream_domain_closure.poisoned")
POISON_TARGET_FILE = pathlib.Path("/tmp/opencode/stream_domain_closure.target")

sys.path.insert(0, str(REPO / "scripts" / "lib"))
from fsm_wiring import strip_comments_and_strings  # noqa: E402

GREEN, RED, YELLOW, RESET = "\033[0;32m", "\033[0;31m", "\033[0;33m", "\033[0m"
FAILS = 0

# ⚠️ 表头（列顺序即列顺序）⇒ ⇒ 解析 MUST 按**位置**而非名字
HEADER = ["#", "能力面", "契约", "实现", "判据", "护栏", "欠账", "状态"]
MIN_ROWS = 12
COL = {name: i for i, name in enumerate(HEADER)}

PTR_RE = re.compile(r"^(spec|code|test|guard|debt):(.+?)#(.+)$")


def ok(msg: str) -> None:
    print(f"{GREEN}✅ {msg}{RESET}")


def fail(msg: str) -> None:
    global FAILS
    FAILS += 1
    print(f"{RED}❌ {msg}{RESET}")


def notice(msg: str) -> None:
    print(f"{YELLOW}::notice {msg}{RESET}")


def _read_lines() -> list[str]:
    return CHECKLIST.read_text(encoding="utf-8").split("\n")


def _count_anchors() -> dict[str, int]:
    """读清单里的**计数锚点**（`<!-- COUNT-ANCHOR ... -->` 段）。

    ⚠️⚠️ **为什么需要它**（critic BLOCKER B1 · 2026-11-01）：
    `MIN_ROWS` 只是个**地板** ⇒ ⇒ 清单 17 行时**删 1–5 行仍然绿**
    ⇒ ⇒ ⇒ **静默缩面**（本仓最忌的病症family）⇒ ⇒ ⇒
    ⇒ ⇒ 锚点把「**实际行数 / 未兑现行数**」**钉死** ⇒ ⇒ 删行 ⇒ ⇒
    ⇒ ⇒ **锚点与实际不符 ⇒ ⇒ MUST 转红** ⇒ ⇒ ⇒ ⇒ ⇒
    ⇒ ⇒ 与仓内既有 pattern 同型（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` /
    ⇒ ⇒ `DUAL_BACKEND_PARITY_TOTAL`）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「人改了锚点」= 明示
    ⇒ ⇒ ⇒ ⇒ 改动（由 code review 捕获）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 而「悄悄删行」
    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 被机械拦住** ✔
    """
    raw = CHECKLIST.read_text(encoding="utf-8")
    out: dict[str, int] = {}
    for m in re.finditer(r"^([A-Z_]+):\s*(\d+)\s*$", raw, re.M):
        out[m.group(1)] = int(m.group(2))
    return out


def _strip_cell(cell: str) -> str:
    """去掉反引号与空白；`—` ⇒ 空串。"""
    c = cell.strip()
    if c in ("—", "-", ""):
        return ""
    if c.startswith("`") and c.endswith("`") and len(c) >= 2:
        c = c[1:-1]
    return c.strip()


def parse_rows() -> tuple[list[str], list[list[str]]]:
    """返回 (表头行, 数据行列表)。找不到表头 ⇒ 返回空。"""
    lines = _read_lines()
    hdr_idx = None
    for i, ln in enumerate(lines):
        if not ln.lstrip().startswith("|"):
            continue
        cells = [_strip_cell(c) for c in ln.strip().strip("|").split("|")]
        if cells[: len(HEADER)] == HEADER:
            hdr_idx = i
            break
    if hdr_idx is None:
        return [], []
    rows: list[list[str]] = []
    for ln in lines[hdr_idx + 1 :]:
        s = ln.strip()
        if not s.startswith("|"):
            break
        raw_cells = s.strip("|").split("|")
        # ⚠️⚠️ **分隔行 MUST 用【原始】cell 判断**（`---` 经 `_strip_cell`
        #    后会变成 `-` ⇒ ⇒ 与「空单元格」混淆 ⇒ ⇒ 会被当数据行）
        if raw_cells and all(set(c) <= set("-: ") and c for c in raw_cells):
            continue
        cells = [_strip_cell(c) for c in raw_cells]
        if len(cells) != len(HEADER):
            break
        if not cells[0]:
            break
        rows.append(cells)
    return HEADER, rows


def _resolve(path_part: str, kind: str) -> pathlib.Path | None:
    if kind == "spec":
        return REPO / "openspec" / "specs" / path_part / "spec.md"
    return REPO / path_part


def _marker_hit(path: pathlib.Path, marker: str, kind: str) -> bool:
    if not path.exists():
        return False
    raw = path.read_text(encoding="utf-8")
    if kind == "guard":
        # ⚠️ 判据 id 常在**注释**里 ⇒ ⇒ guard 指针**不剥注释**
        return marker in raw
    if kind == "spec":
        # ⇒ ⇒ 标题行（MUST 是 `### Requirement:` 或 `#### Scenario:` 开头）
        #    ⇒ ⇒ **Scenario 也算** —— 契约的判据常写在 Scenario 里
        for ln in raw.split("\n"):
            if (ln.startswith("### Requirement:") or ln.startswith("#### Scenario:")) \
                    and marker in ln:
                return True
        return False
    # ⚠️ **只剥注释、保留字符串**（⇒ ⇒ Catch2 的 tag 住在字符串里
    #    ⇒ ⇒ 剥掉字符串就永远匹配不上 ⇒ ⇒ 假红）
    code = strip_comments_and_strings(raw, strip_strings=False)
    return marker in code


def r0() -> None:
    if not CHECKLIST.exists():
        fail(f"R0 收口清单不存在：{CHECKLIST.relative_to(REPO)}")
        return
    hdr, rows = parse_rows()
    if not hdr:
        fail("R0 收口清单**表头缺失或不可解析**（⇒ ⇒ 清单已被破坏）")
        return
    if len(rows) < MIN_ROWS:
        fail(f"R0 收口清单数据行数 = {len(rows)} < {MIN_ROWS}"
             f"（⇒ ⇒ 清单被**截断/清空** ⇒ ⇒ MUST 转红）")
        return
    # ⚠️⚠️ **计数锚点校验**（critic BLOCKER B1）⇒ ⇒ 抓「静默缩面」
    anchors = _count_anchors()
    if "TOTAL_ROWS" not in anchors or "UNPAID_ROWS" not in anchors:
        fail("R0 收口清单**缺计数锚点**（TOTAL_ROWS / UNPAID_ROWS）"
             "（⇒ ⇒ 无法抓「静默删行 / 静默降级」⇒ ⇒ MUST 转红）")
        return
    if anchors["TOTAL_ROWS"] != len(rows):
        fail(f"R0 收口清单行数 = {len(rows)} ≠ 锚点 TOTAL_ROWS = "
             f"{anchors['TOTAL_ROWS']}"
             f"（⇒ ⇒ **清单被改动但锚点未同步** ⇒ ⇒ 静默缩面 ⇒ ⇒ MUST 转红）")
        return
    unpaid = sum(1 for c in rows if c[COL["状态"]] == "❌")
    if anchors["UNPAID_ROWS"] != unpaid:
        fail(f"R0 收口清单未兑现行数 = {unpaid} ≠ 锚点 UNPAID_ROWS = "
             f"{anchors['UNPAID_ROWS']}"
             f"（⇒ ⇒ **状态被改动但锚点未同步** ⇒ ⇒ MUST 转红）")
        return
    ok(f"R0 收口清单存在且结构完整（{len(rows)} 行 · 锚点一致 · "
       f"未兑现 {unpaid} 行）")


def r1() -> None:
    _, rows = parse_rows()
    if not rows:
        return
    bad: list[str] = []
    n = 0
    for cells in rows:
        for name in ("契约", "实现", "判据", "护栏", "欠账"):
            ptr = cells[COL[name]]
            if not ptr:
                continue
            m = PTR_RE.match(ptr)
            if not m:
                bad.append(f"{cells[COL['#']]} 的「{name}」指针格式非法：`{ptr}`"
                           f"（MUST 形如 `kind:path#marker`）")
                continue
            kind, path_part, marker = m.groups()
            n += 1
            path = _resolve(path_part, kind)
            if path is None or not path.exists():
                bad.append(f"{cells[COL['#']]} 的「{name}」指向**不存在的文件**："
                           f"`{ptr}`")
                continue
            if not _marker_hit(path, marker, kind):
                bad.append(f"{cells[COL['#']]} 的「{name}」标记**未命中**："
                           f"`{ptr}` ⇒ ⇒ 清单与来源**已漂移**")
    if bad:
        fail("R1 收口清单的指针 MUST 全部可解析：\n      " + "\n      ".join(bad))
        return
    ok(f"R1 收口清单的 {n} 个指针全部可解析（清单与来源未漂移）")


def r2() -> None:
    _, rows = parse_rows()
    if not rows:
        return
    bad: list[str] = []
    done = unpaid = 0
    for cells in rows:
        num = cells[COL["#"]]
        state = cells[COL["状态"]]
        spec = cells[COL["契约"]]
        test = cells[COL["判据"]]
        guard = cells[COL["护栏"]]
        debt = cells[COL["欠账"]]
        if state == "✅":
            done += 1
            missing = [
                n for n, v in (("契约", spec), ("判据", test), ("护栏", guard))
                if not v
            ]
            if missing:
                bad.append(f"{num} 标 ✅ 但缺 {' / '.join(missing)}"
                           f"（⇒ ⇒ 「已兑现」必须**三者对齐**："
                           f"契约 + 可执行判据 + 自动化护栏）")
        elif state == "❌":
            unpaid += 1
            if not debt:
                bad.append(f"{num} 标 ❌ 但**无欠账指针**"
                           f"（⇒ ⇒ 「未兑现」必须落到真实的台账 / ADR）")
            elif test and guard:
                bad.append(f"{num} 标 ❌ 却同时具备**判据 + 护栏**"
                           f"（⇒ ⇒ **状态过时**：三者已齐 ⇒ ⇒ MUST 标 ✅）")
            elif test:
                # ⚠️⚠️ **收紧**（critic BLOCKER B1）：❌ 行若**已有判据**
                #    ⇒ ⇒ ⇒ 「未兑现」与「有判据」**矛盾** ⇒ ⇒ ⇒
                #    ⇒ ⇒ ⇒ **⇒ 这是「假 ❌」的形态**（⇒ ⇒ ❌ 计数虚增
                #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 收官判据被一个错数度量**）
                #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ MUST 转红**（⇒ ⇒ 逼出
                #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 真正的修法：补 `guard:` 或
                #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 删掉 `test:`）
                bad.append(f"{num} 标 ❌ 却**已有可执行判据**"
                           f"（⇒ ⇒ 「未兑现」与「有判据」矛盾 ⇒ ⇒ "
                           f"**假 ❌** ⇒ ⇒ ❌ 计数虚增 ⇒ ⇒ MUST 标 ✅ "
                           f"或删掉该判据指针）")
        else:
            bad.append(f"{num} 的状态列取值非法：`{state}`（MUST 是 ✅ 或 ❌）")
    if bad:
        fail("R2 收口清单的**状态 MUST 与指针集合一致**：\n      "
             + "\n      ".join(bad))
        return
    ok(f"R2 状态一致性通过（✅ {done} 项 · ❌ {unpaid} 项）")


# ── 原地投毒（fail-closed：找不到目标 ⇒ 退出码 2 ⇒ 绝不静默跳过）──────────
def _find_row(lines: list[str], needle: str) -> int:
    for i, ln in enumerate(lines):
        if needle in ln and ln.strip().startswith("|"):
            return i
    return -1


def _poison_break_test_pointer(lines: list[str]) -> list[str]:
    i = _find_row(lines, "stream_arbiter_round_robin")
    if i < 0:
        raise RuntimeError("找不到 `stream_arbiter_round_robin` 行")
    lines[i] = lines[i].replace(
        "[stream][arbiter][round_robin][rotation]",
        "[stream][no-such-tag-should-fail]",
    )
    return lines


def _poison_flip_done_to_unpaid(lines: list[str]) -> list[str]:
    i = _find_row(lines, "stream_arbiter_round_robin")
    if i < 0:
        raise RuntimeError("找不到 `stream_arbiter_round_robin` 行")
    lines[i] = lines[i].replace("| ✅ |", "| ❌ |")
    return lines


def _poison_stale_row_claims_covered(lines: list[str]) -> list[str]:
    i = _find_row(lines, "stream_demux")
    if i < 0:
        raise RuntimeError("找不到 `stream_demux` 行")
    cells = lines[i].strip().strip("|").split("|")
    cells[COL["判据"]] = " `test:tests/chlib/test_stream.cpp#[stream][demux]` "
    cells[COL["护栏"]] = " `guard:scripts/verify_stream_ready_ownership.sh#R8` "
    lines[i] = "|" + "|".join(cells) + "|"
    return lines


def _poison_truncate_rows(lines: list[str]) -> list[str]:
    idx = [i for i, ln in enumerate(lines)
           if ln.strip().startswith("|") and re.match(r"^\|\s*\d+\s*\|", ln.strip())]
    if len(idx) < MIN_ROWS + 2:
        raise RuntimeError(f"数据行不足（{len(idx)}）⇒ ⇒ 无法构造截断投毒")
    # ⚠️⚠️ **必须删够行数** —— 清单 15 行、阈值 `MIN_ROWS = 12`
    #    ⇒ ⇒ 只删 3 行 ⇒ 剩 12 行 ⇒ ⇒ **不小于阈值 ⇒ ⇒ 判据不转红**
    #    ⇒ ⇒ ⇒ **⇒ 投毒**必须**证明**判据有判别力 ⇒ ⇒ 删 5 行（剩 10）
    drop = set(idx[-5:])
    return [ln for i, ln in enumerate(lines) if i not in drop]


def _poison_downgrade_row(lines):
    """把第一个 ✅ 行降级成 ❌（清空护栏格 + 填已存在的欠账指针）。"""
    for i, ln in enumerate(lines):
        if not ln.startswith("|") or "| \u2705 |" not in ln:
            continue
        c = ln.strip().strip("|").split("|")
        if len(c) != 8:
            continue
        c[5] = " \u2014 "
        c[6] = " `debt:docs/adr/ADR-045-chlib-stream-semantics.md#\u81ea\u4fdd\u62a4` "
        c[7] = " \u274c "
        lines[i] = "|" + "|".join(c) + "|"
        return lines
    raise RuntimeError("找不到可降级的 \u2705 行")


def _poison_delete_one_row(lines):
    """只删 1 行（旧实现在 MIN_ROWS 地板之上仍绿 ⇒ ⇒ 静默缩面）。"""
    import re as _re
    idx = [i for i, ln in enumerate(lines)
           if ln.strip().startswith("|") and _re.match(r"^\|\s*\d+\s*\|", ln.strip())]
    if len(idx) < 2:
        raise RuntimeError("数据行不足")
    del lines[idx[-1]]
    return lines


POISONS: dict[str, tuple[str, object]] = {
    "break-test-pointer": (
        "✅ 行把判据 tag 改成**不存在的** ⇒ ⇒ R1 MUST 转红"
        "（⇒ ⇒ 清单不能对「没有判据」的行说 ✅）",
        _poison_break_test_pointer,
    ),
    "flip-done-to-unpaid": (
        "✅ 行改成 ❌ ⇒ ⇒ R2 MUST 转红（❌ 行 MUST 有欠账指针）",
        _poison_flip_done_to_unpaid,
    ),
    "stale-row-claims-covered": (
        "❌ 行补上判据 + 护栏 ⇒ ⇒ R2 MUST 转红（状态过时）",
        _poison_stale_row_claims_covered,
    ),
    "downgrade-row": (
        "把 ✅ 行降级成 ❌（清空护栏格 + 填已存在的欠账指针）"
        " => 旧实现全绿 => 新实现 MUST 转红",
        _poison_downgrade_row,
    ),
    "delete-one-row": (
        "只删 1 行（MIN_ROWS 地板之上仍绿 => 静默缩面）"
        " => 新实现 MUST 转红（计数锚点 TOTAL_ROWS 不符）",
        _poison_delete_one_row,
    ),
    "truncate-rows": (
        "删掉 3 行 ⇒ ⇒ R0 MUST 转红（清单被截断 ⇒ ⇒ 假绿）",
        _poison_truncate_rows,
    ),
}


def _do_check() -> int:
    r0()
    r1()
    r2()
    r10()
    r11()
    if FAILS:
        print(f"{RED}❌ 收口清单核验 FAIL（{FAILS} 项）{RESET}")
        return 1
    print(f"{GREEN}✅ 收口清单核验全部通过{RESET}")
    return 0


def poison(name: str) -> int:
    if name not in POISONS:
        print(f"未知投毒：{name}", file=sys.stderr)
        return 2
    if not CHECKLIST.exists():
        print("收口清单不存在 ⇒ 无法投毒", file=sys.stderr)
        return 2
    _, fn = POISONS[name]
    lines = _read_lines()
    # ⚠️⚠️ **快照**（不是 `lines` 本身）—— 部分投毒函数**原地**改 `lines`
    #    ⇒ ⇒ 直接比较 `new_lines == lines` **恒真**（同一对象）
    #    ⇒ ⇒ ⇒ 「未产生改动」永远触发 ⇒ ⇒ **所有投毒都注入失败**
    #    ⇒ ⇒ ⇒ ⇒ **⇒ 这是本护栏自检**自己**假绿**的一处根因
    before = list(lines)
    try:
        new_lines = fn(lines)  # type: ignore[operator]
    except RuntimeError as e:
        print(f"MUTATE_FAIL：{e}", file=sys.stderr)
        return 2
    if new_lines == before:
        print(f"MUTATE_FAIL：投毒 `{name}` 未产生任何改动"
              f"（⇒ ⇒ 会跑**未投毒**的内容 ⇒ ⇒ 假绿）", file=sys.stderr)
        return 2
    # ⚠️⚠️ **备份 MUST 用 `copyfile`（刷新 mtime）** —— `copy2` 保留 mtime
    #    ⇒ ⇒ 还原后「已是最新」⇒ ⇒ 后续核验读的是**陈旧**内容
    shutil.copyfile(CHECKLIST, BACKUP)
    shutil.copyfile(CHECKLIST, POISONED_SNAPSHOT)
    POISON_TARGET_FILE.write_text(name, encoding="utf-8")
    CHECKLIST.write_text("\n".join(new_lines), encoding="utf-8")
    print(f"已投毒：{name}（{POISONS[name][0]}）")
    return 0


def restore() -> int:
    if not BACKUP.exists() or not POISON_TARGET_FILE.exists():
        print("无投毒快照 ⇒ ⇒ 拒绝还原（⇒ ⇒ 防止覆盖工作区真实改动）",
              file=sys.stderr)
        return 2
    name = POISON_TARGET_FILE.read_text(encoding="utf-8").strip()
    shutil.copyfile(BACKUP, CHECKLIST)  # ⚠️ MUST copyfile（刷新 mtime）
    BACKUP.unlink(missing_ok=True)
    POISONED_SNAPSHOT.unlink(missing_ok=True)
    POISON_TARGET_FILE.unlink(missing_ok=True)
    print(f"已还原投毒：{name}")
    return 0


FIFO_H = REPO / "include" / "chlib" / "fifo.h"


def r10() -> None:
    """`sync_fifo` 的**自保护**结构判据（写口/读指针 MUST 与 `count` 同源）。

    ⚠️ **为什么需要**（critic BLOCKER B1 · 2026-11-01）：收口清单第 10 行
    「FIFO 指针 MUST 受空/满门控（自保护）」原先标 ❌ 且其 `debt:` 指针指向
    `ADR-045` 的**已兑现 MUST** ⇒ ⇒ ⇒ **那是「假 ❌」** ⇒ ⇒ ⇒
    ⇒ ⇒ **本判据把那一条变成【可机械核验的 ✅】** ⇒ ⇒ ⇒ ⇒ ⇒
    ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 一条判据换一行收口（零 verilator 编译、零 CI 墙钟）**.

    ⇒ ⇒ **判据是结构性的**（MUST NOT 词法 grep 整个文件）：
    `sync_fifo` 的**函数体内** MUST 同时出现
      ① `will_write`（写口自保护：`wren & !is_full(count)`）
      ② `will_read`（读口自保护：`rden & !is_empty(count)`）
      ③ `is_full(count)` ④ `is_empty(count)`
    ⇒ ⇒ 四者缺一 ⇒ ⇒ 「计数与指针脱钩」⇒ ⇒ ⇒ 满写被覆写 / 空读读出脏槽
    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **（ADR-045 §2.1 的条款本体）**
    """
    if not FIFO_H.exists():
        fail("R10 缺少 include/chlib/fifo.h")
        return
    code = strip_comments_and_strings(FIFO_H.read_text(encoding="utf-8"))
    m = re.search(r"\bsync_fifo\s*\(", code)
    if not m:
        fail("R10 找不到 `sync_fifo` 的定义")
        return
    i = code.find("{", m.end())
    if i < 0:
        fail("R10 `sync_fifo` 未找到函数体")
        return
    depth = 0
    end = -1
    for k in range(i, len(code)):
        if code[k] == "{":
            depth += 1
        elif code[k] == "}":
            depth -= 1
            if depth == 0:
                end = k
                break
    if end < 0:
        fail("R10 `sync_fifo` 的函数体花括号不配平")
        return
    body = code[i:end + 1]
    missing = [mk for mk in ("will_write", "will_read", "is_full(count)",
                             "is_empty(count)") if mk not in body]
    if missing:
        fail("R10 `sync_fifo` 的自保护结构缺 " + " / ".join(missing)
             + "（⇒ ⇒ 写口/读指针与 count 脱钩 ⇒ ⇒ 满写被覆写 / 空读读脏槽）")
        return
    ok("R10 `sync_fifo` 自保护结构完整（will_write / will_read 与 count 同源）")


def r11() -> None:
    """名字 over-promise 的 FIFO 组件 MUST NOT 回归（B4 · 物理删除的**回归禁令**）。

    ⚠️ **为什么需要**（2026-11-01 · `fwft-fifo-physical-removal-2026-11`）：
    `chlib::fwft_fifo` 的**名字**承诺 FWFT（show-ahead），实测 `q` 只在
    **读之后**才更新 ⇒ ⇒ **名字说谎**。处置 = **物理删除**（零消费者）。
    ⇒ ⇒ ⇒ **但「删掉」这件事本身没有守护**：下一个人可能重新加一个同名组件
    （或被 `git revert` 带回来）⇒ ⇒ **同一个谎言会以「新代码」的形态复发**。

    ⇒ ⇒ **判据两面**（都 MUST 成立）：
      ① `include/chlib/fifo.h`（**剥注释**后）MUST NOT 出现 `fwft_fifo`
         或 `FwftFifoResult` ⇒ ⇒ 出现即「被删组件回归」⇒ ⇒ 红；
      ② 真 FWFT 能力 MUST **仍然在**（`q_async`）⇒ ⇒ 否则「删组件」把真能力
         一起删了 ⇒ ⇒ 也是红（**反向判据**，防「为让判据绿而误删」）。
    """
    if not FIFO_H.exists():
        fail("R11 缺少 include/chlib/fifo.h")
        return
    code = strip_comments_and_strings(FIFO_H.read_text(encoding="utf-8"))
    ghosts = [sym for sym in ("fwft_fifo", "FwftFifoResult") if sym in code]
    if ghosts:
        fail("R11 名字 over-promise 的组件**回归**了：" + " / ".join(ghosts)
             + "（它承诺 FWFT 但 `q` 只在读之后才更新 ⇒ ⇒ 名字说谎 ⇒ ⇒ "
             "已于 2026-11-01 物理删除 ⇒ ⇒ MUST NOT 重新引入同名组件；"
             "真 FWFT 语义请用 `sync_fifo` 的 `q_async`）")
        return
    if "q_async" not in code:
        fail("R11 真 FWFT 能力**不见了**（`q_async` 缺失）⇒ ⇒ 删除说谎组件的"
             "同时把真能力一起删了 ⇒ ⇒ MUST 恢复 `sync_fifo` 的 0-cycle `q_async`")
        return
    ok("R11 名字 over-promise 的 FIFO 组件未回归，且真 FWFT（`q_async`）仍在")


def main() -> int:
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    if cmd == "check":
        return _do_check()
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
