#!/usr/bin/env python3
"""契约 `chlib-stream-ready-ownership` 的**静态**判据分析器。

⚠️ **为什么要有这个分析器**：shell + grep 的静态判据在本仓**已被证明不可靠**
   —— ① 注释里的字面量会被误判（`verify_multi_if_width.sh` 的先例）；
   ② 双引号里的反引号会被 shell 当**命令替换**吃掉 ⇒ ⇒ 护栏消息被静默吞掉；
   ⇒ ⇒ 故静态判据 MUST 走 **Python + 剥注释**（复用
   `scripts/lib/fsm_wiring.py` 的 `strip_comments_and_strings` ⇒ ⇒ 两份护栏
   对「什么算真实代码」的口径**不会漂移**）。

⚠️ **判据分两类**（MUST 分清）：
   ① **效果判据**（运行期）⇒ ⇒ 在**护栏脚本**里跑行为用例（`R1`）——
      「组合子 MUST NOT **无条件**忽略消费端 ready」⇒ ⇒ MUST 用**行为**实现
      （MUST NOT 用「源码含常量 `true`」这类**词法**判据 ⇒ ⇒ 否则「合法的
      默认恒就绪」被误判 ⇒ ⇒ 判别力归零）；
   ② **静态判据**（结构）⇒ ⇒ 本文件 ⇒ ⇒ 「连接手段 MUST `<<=`」·
      「读使能 MUST 引用**字段节点本身**」·「写使能 MUST 只用 `valid`」
      ⇒ ⇒ 这三条**明文**是结构性的（capability delta Requirement 3 允许
      「静态护栏 MUST 检出 `operator=` 形态」）。

用法：
    python3 scripts/lib/stream_ready_ownership.py check      # R2~R6
    python3 scripts/lib/stream_ready_ownership.py poison <名字>  # 原地投毒
    python3 scripts/lib/stream_ready_ownership.py restore    # 还原
"""
from __future__ import annotations

import pathlib
import re
import shutil
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
STREAM_H = REPO / "include" / "chlib" / "stream.h"
BUILDER_H = REPO / "include" / "chlib" / "stream_builder.h"
ARBITER_H = REPO / "include" / "chlib" / "stream_arbiter.h"
BACKUP = pathlib.Path("/tmp/opencode/stream_h_ownership.bak")
# ⚠️ 「被投毒的内容」快照 —— 用于还原前的**第三方改动检测**
POISONED_SNAPSHOT = pathlib.Path("/tmp/opencode/stream_h_ownership.poisoned")

EXEMPT_MARK = "stream_ready_ownership: intentionally-unwired"

# ⚠️⚠️ **R8 的「已知未收口」豁免清单**（P1-a · 2026-11-01）
# ⚠️ **MUST 显式登记 + 理由 + 归属 change**（⇒ ⇒ 豁免**非静默** ⇒ ⇒ 否则
#    「已收口」的声明会变成谎言）
# ⚠️ ⚠️ **本清单 MUST 只增不减到「全部收口」为止**（⇒ ⇒ 每次 P1 落地 MUST
#    删掉对应条目 ⇒ ⇒ ⇒ 否则「未收口」会被**永久豁免** ⇒ ⇒ ⇒ ⇒ **假绿**）
UNSETTLED = {
    "stream_fork": "P1-c（同步语义从未工作过 ⇒ 等于新功能 ⇒ 判据从零建）",
    "stream_demux": "P1-b（方向修正是行为变更 ⇒ 需独立 ADR）",
}

# ⚠️ **ready 字段的「角色」分类**（**结构判据** ⇒ ⇒ 不是符号清单）：
#   · 输出侧 = 组合子**自己产出**的 ready（驱动它 ⇒ ⇒ 合法）
#   · 输入侧 = 组合子**要驱动**的 ready（⇒ ⇒ MUST 用 `<<=`）
# ⚠️ ⚠️ **角色分类 MUST 基于「字段名在组合子契约中的语义」**
#   ⇒ ⇒ **不是**「硬编码符号清单」⇒ ⇒ ⇒ **新组合子自动被覆盖** ✔
OUTPUT_SIDE_FIELDS = ("push_stream", "pop_stream", "output_stream")

sys.path.insert(0, str(REPO / "scripts" / "lib"))
from fsm_wiring import strip_comments_and_strings  # noqa: E402

GREEN, RED, YELLOW, RESET = "\033[0;32m", "\033[0;31m", "\033[0;33m", "\033[0m"
FAILS = 0


def ok(msg: str) -> None:
    print(f"  {GREEN}✅ {msg}{RESET}")


def fail(msg: str) -> None:
    global FAILS
    FAILS += 1
    print(f"  {RED}❌ {msg}{RESET}")


def notice(msg: str) -> None:
    print(f"  {YELLOW}::notice {msg}{RESET}")


def code_of(path: pathlib.Path) -> str:
    """返回**剥掉注释与字符串**的代码（保留行结构 ⇒ 行号不变）。"""
    return strip_comments_and_strings(path.read_text(encoding="utf-8"))


def sync_fifo_call_args(stream_code: str) -> list[str] | None:
    """抽出 `stream_fifo` 内那次 `sync_fifo(...)` 调用的**顶层实参**列表。"""
    idx = stream_code.find("sync_fifo<")
    if idx < 0:
        return None
    open_paren = stream_code.find("(", idx)
    if open_paren < 0:
        return None
    depth = 0
    args: list[str] = []
    cur: list[str] = []
    i = open_paren
    n = len(stream_code)
    while i < n:
        c = stream_code[i]
        if c in "([{":
            depth += 1
            if depth == 1:
                i += 1
                continue
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                args.append("".join(cur).strip())
                return args
        if depth == 1 and c == ",":
            args.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        if depth >= 1:
            cur.append(c)
        i += 1
    return None


# ── R2：读使能 MUST 引用「字段节点本身」──────────────────────────────────────
def r2() -> None:
    code = code_of(STREAM_H)
    args = sync_fifo_call_args(code)
    if args is None or len(args) < 3:
        fail("R2 找不到 `sync_fifo(...)` 调用（或实参 < 3）")
        return
    read_en = " ".join(args[2].split())
    if read_en != "result.pop_stream.ready":
        fail(f"R2 读使能 MUST 是 `result.pop_stream.ready`（字段节点本身），实得 `{read_en}`")
        return
    ok("R2 读使能引用「字段节点本身」（覆盖 ⇒ 必然传播）")


# ── R3：默认驱动 MUST 用 `<<=`（MUST NOT `operator=` 重绑定）─────────────────
def r3() -> None:
    code = code_of(STREAM_H)
    bad = [
        f"{i+1}: {ln.strip()}"
        for i, ln in enumerate(code.split("\n"))
        if re.search(r"pop_stream\.ready\s*=(?!=)", ln)
    ]
    if bad:
        fail("R3 发现 `pop_stream.ready =`（`operator=` 重绑定）⇒ ⇒ 字段被孤儿化：\n      "
             + "\n      ".join(bad))
        return
    if not re.search(r"pop_stream\.ready\s*<<=", code):
        fail("R3 未找到 `pop_stream.ready <<=` ⇒ ⇒ 默认驱动手段不符")
        return
    ok("R3 默认驱动用 `<<=`（目标 = 非 literal 的 field proxy）")


# ── R4：写使能 MUST 只用 `input_stream.valid`（MUST NOT `valid & ready`）─────
def r4() -> None:
    code = code_of(STREAM_H)
    args = sync_fifo_call_args(code)
    if args is None or not args:
        fail("R4 找不到 `sync_fifo(...)` 调用")
        return
    wren = " ".join(args[0].split())
    if wren != "input_stream.valid":
        fail(f"R4 写使能 MUST 是 `input_stream.valid`（内部 `will_write` 自保护），实得 `{wren}`")
        return
    ok("R4 写使能只用 `input_stream.valid`（`valid & ready` 是先读后写 ⇒ 组合子不可用）")


# ── R5：MUST 提供 `stream_fifo_connect_ready` 且形参 MUST 模板化 ─────────────
def r5() -> None:
    code = code_of(STREAM_H)
    if "void stream_fifo_connect_ready" not in code:
        fail("R5 缺少 `stream_fifo_connect_ready`")
        return
    m = re.search(
        r"template\s*<[^>]*typename\s+C[^>]*>\s*\nvoid\s+stream_fifo_connect_ready"
        r"\s*\([^)]*\bconst\s+C\s*&",
        code,
    )
    if not m:
        fail("R5 `stream_fifo_connect_ready` 的形参未模板化（`const ch_bool &` ⇒ ⇒ "
             "`ch_in<ch_bool>` 端口绑不上）")
        return
    ok("R5 `stream_fifo_connect_ready` 存在且形参模板化（端口可用）")


# ── R7 / R8：转发类（mux / arbiter / join / filters / combine_with）────────
#
# ⚠️⚠️ **R8 的设计要点**（P1-a · 2026-11-01）：
#   ① **穷举**：自动发现 `stream.h` + `stream_arbiter.h` 内**所有**接受
#      `ch_stream` 形参的组合子 ⇒ ⇒ ⇒ **MUST NOT** 靠**人工符号清单**
#      （⚠️ 本仓登记的护栏盲区：「只查已知符号 ⇒ ⇒ 漏掉同族入口」
#      ⇒ ⇒ ⇒ **新组合子自动被覆盖** ✔）；
#   ② **结构判据**（MUST NOT 词法）：判定「该组合子是否用 `<<=` 驱动上游」
#      ⇒ ⇒ ⇒ 而**不是**「源码含常量 `true`」；
#   ③ **角色区分**：组合子**自己产出**的 ready（`output_stream` /
#      `push_stream` / `pop_stream`）驱动它 ⇒ ⇒ **合法**；
#      **要驱动**的上游 ready ⇒ ⇒ MUST 用 `<<=`；
#   ④ **豁免非静默**：**已知未收口**者（`stream_fork` / `stream_demux`）
#      ⇒ ⇒ MUST 显式登记 + 理由 + 归属 change ⇒ ⇒ `::notice` 点名。

# ⚠️ 组合子的「输入侧」ready 字段（**要驱动**的那些）
#   ⚠️ 注意：`result.ready`（filters 形态：返回**裸** `ch_stream`）也属输入侧
# ⚠️⚠️ **R8 的正则 MUST 支持下标形式**（`input_streams[i].ready`）
#    ⇒ ⇒ 否则 mux / arbiter / join 的上游驱动**全部漏检**
#    ⇒ ⇒ ⇒ ⇒ ⇒ **（实测：投毒 `forward-upstream-operator-assign` 后静态判据仍绿
#    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 正是这个盲区**）**
_UPSTREAM_READY = re.compile(
    r"\b(input_stream|input_streams|input1|input2|result)\s*"
    r"(?:\[[^\]]*\])?\s*\.ready\s*(<<=|=(?!=))")


def _iter_stream_functions() -> list[tuple[str, str]]:
    """自动发现 `stream_*` 函数体（**穷举** ⇒ ⇒ 不靠符号清单）。"""
    out: list[tuple[str, str]] = []
    for f in (STREAM_H, ARBITER_H):
        if not f.exists():
            continue
        code = strip_comments_and_strings(f.read_text(encoding="utf-8"))
        for m in re.finditer(r"\b(stream_\w+)\s*\(", code):
            i = code.find("{", m.end())
            if i < 0:
                continue
            depth = 0
            j = i
            while j < len(code):
                if code[j] == "{":
                    depth += 1
                elif code[j] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            out.append((m.group(1), code[m.start(): j + 1]))
    return out


def r8() -> None:
    """转发类 MUST 用 `<<=` 驱动上游 ready（**穷举** + **结构判据**）。"""
    if not ARBITER_H.exists():
        fail("R8 缺少 include/chlib/stream_arbiter.h")
        return

    viol: list[str] = []
    settled: list[str] = []
    exempt: list[str] = []

    for name, body in _iter_stream_functions():
        # ⚠️ **不是组合子**（无 `ch_stream` 形参）⇒ ⇒ 跳过（如 `connect_ready`）
        head = body.split(")", 1)[0]
        if "ch_stream<" not in head:
            continue
        # ⚠️ **fifo / queue 类**是**豁免**的（P0' 已落地 ⇒ ⇒ 契约已覆盖）
        if name in ("stream_fifo", "stream_queue"):
            continue
        # ⚠️ **已知未收口**（显式登记 ⇒ ⇒ 豁免**非静默**）
        if name in UNSETTLED:
            exempt.append(f"{name}（{UNSETTLED[name]}）")
            continue

        bad = []
        for m in _UPSTREAM_READY.finditer(body):
            # ⚠️ **输出侧**字段 ⇒ ⇒ 组合子**自己产出** ⇒ ⇒ 驱动它合法
            target = m.group(1)
            if target in OUTPUT_SIDE_FIELDS:
                continue
            if m.group(2) == "=":
                bad.append(f"{target}.ready = （operator= 重绑定 ⇒ ⇒ 调用方被孤儿化）")
        if bad:
            viol.append(f"{name}: " + "；".join(bad))
        else:
            settled.append(name)

    for e in exempt:
        notice(f"R8 豁免（已知未收口 · 归属独立 change）：{e}")
    if viol:
        fail("R8 转发类 MUST 用 `<<=` 驱动上游 ready：\n      " + "\n      ".join(viol))
        return
    ok(f"R8 转发类上游 ready 全部用 `<<=`（已收口 {len(settled)} 个："
       + ", ".join(sorted(set(settled))) + "）")


# ── R6：调用点判据 —— 调 `stream_fifo` / `stream_queue` 后 MUST 接 ready ─────
#
# ⚠️⚠️ **覆盖面限定（2026-10-31）**：`CALL_RE` **只**覆盖 `stream_fifo` /
#    `stream_queue` ⇒ ⇒ ⇒ **转发类**（mux / demux / arbiter / fork / join /
#    filters）**当前不在**本判据的覆盖范围内 ⇒ ⇒ 其调用点判据属 **P1**
#    ⇒ ⇒ （`.rddf/improvements/stream-forward-ready-closure-2026-10.md`）
#    ⇒ ⇒ ⇒ **本判据当前** MUST NOT **被读作「转发类已合规」**。
CALL_RE = re.compile(r"\bstream_fifo\s*<|\bstream_queue\s*<")
SCAN_ROOTS = ["samples", "tests", "examples"]
SCAN_FILES = [BUILDER_H]


def r6() -> None:
    files: list[pathlib.Path] = []
    for root in SCAN_ROOTS:
        files.extend(sorted((REPO / root).rglob("*.cpp")))
        files.extend(sorted((REPO / root).rglob("*.h")))
    files.extend(SCAN_FILES)

    hits: list[str] = []
    for f in files:
        if not f.exists():
            continue
        raw = f.read_text(encoding="utf-8")
        code = strip_comments_and_strings(raw)
        raw_lines = raw.split("\n")
        code_lines = code.split("\n")
        for i, ln in enumerate(code_lines):
            if not CALL_RE.search(ln):
                continue
            # ⚠️ 窗口 MUST 含调用点**之前**的行 ⇒ ⇒ 豁免标记通常写在调用点**上方**
            lo = max(0, i - 10)
            window_code = "\n".join(code_lines[lo : i + 15])
            if "connect_ready" in window_code:
                continue
            window_raw = "\n".join(raw_lines[lo : i + 15])
            if EXEMPT_MARK in window_raw:
                notice(f"R6 豁免（显式标记）：{f.relative_to(REPO)}:{i+1}")
                continue
            hits.append(f"{f.relative_to(REPO)}:{i+1}")
    if hits:
        fail("R6 以下调用点未接 ready（且无显式豁免标记）：\n      " + "\n      ".join(hits))
        return
    ok("R6 所有 `stream_fifo` / `stream_queue` 调用点均已接 ready（或显式豁免）")


# ── 原地投毒（fail-closed：找不到模式 ⇒ 退出码 2 ⇒ 绝不静默跳过）──────────
POISONS: dict[str, tuple[str, str]] = {
    # 读使能改引用**局部常量节点** ⇒ ⇒ 覆盖**不传播**（钉住承重依赖）
    "read-en-local-const": (
        "result.pop_stream.ready                            // 读使能（**字段节点本身**）",
        "({ ch_bool local_r(true); local_r; })               // 读使能（**局部常量** ⇒ 覆盖不传播）",
    ),
    # 写使能改回 `valid & ready` ⇒ ⇒ 先读后写 ⇒ ⇒ 组合子不可用
    "write-en-valid-and-ready": (
        "input_stream.valid,                               // 写使能（自保护）",
        "input_stream.valid & input_stream.ready,          // ⚠️ 投毒：先读后写",
    ),
    # 默认驱动改用 `operator=` ⇒ ⇒ 重绑定到 literal ⇒ ⇒ 覆盖无效
    "default-driver-operator-assign": (
        "result.pop_stream.ready <<= result.default_ready_source;",
        "result.pop_stream.ready = result.default_ready_source;",
    ),
    # 删掉多驱动守卫 ⇒ ⇒ last-wins 静默
    "drop-multidrive-guard": (
        "if (ready_node->num_srcs() == 0 || ready_node->src(0) != default_node) {",
        "if (false) {",
    ),
    # ⚠️⚠️ **R8 专用投毒（P1-a）**：转发类的上游驱动 `<<=` ⇒ ⇒ `operator=`
    #    ⇒ ⇒ **调用方被孤儿化** ⇒ ⇒ ⇒ **R8（结构判据）MUST 转红**
    #    ⇒ ⇒ ⇒ 同时 **R1（效果判据）** MUST 转红（判别力用例）
    "forward-upstream-operator-assign": (
        """        result.input_streams[i].ready <<=
            (is_selected & result.output_stream.ready);""",
        """        result.input_streams[i].ready =
            (is_selected & result.output_stream.ready);""",
    ),
    # ⚠️⚠️ **R8 专用投毒（P1-a）**：filters 的上游驱动 `<<=` ⇒ ⇒ `operator=`
    #    ⇒ ⇒ **调用方被孤儿化** ⇒ ⇒ ⇒ **R8 MUST 转红**
    "filter-upstream-operator-assign": (
        """    input_stream.ready <<= result.ready;""",
        """    input_stream.ready = result.ready;""",
    ),
}

# ⚠️ **投毒的目标文件**（⇒ ⇒ R8 的投毒可能落在 `stream_arbiter.h`）
POISON_TARGETS: dict[str, pathlib.Path] = {}


def poison(name: str) -> int:
    if name not in POISONS:
        print(f"POISON_FAIL: 未知投毒名: {name}", file=sys.stderr)
        return 2
    old, new = POISONS[name]
    # ⚠️ **投毒目标文件**：默认 `stream.h`；R8 的投毒可能落在 `stream_arbiter.h`
    target = POISON_TARGETS.get(name, STREAM_H)
    s = target.read_text(encoding="utf-8")
    if old not in s:
        print(f"POISON_FAIL: 模式未找到 ⇒ 投毒未生效（fail-closed）: {name}", file=sys.stderr)
        return 2
    shutil.copy2(target, BACKUP)
    # ⚠️⚠️ **记录「被投毒的内容」**（用于还原前的**校验**）——
    #    见 `restore()` 的「第三方改动检测」⇒ ⇒ 防止还原**覆盖**备份之后的
    #    工作区改动（本仓「还原源码 ≠ 复原」的**第三个维度**）。
    POISONED_SNAPSHOT.write_text(s.replace(old, new, 1), encoding="utf-8")
    target.write_text(s.replace(old, new, 1), encoding="utf-8")
    return 0


def restore() -> int:
    if not BACKUP.exists():
        print("RESTORE_FAIL: 无备份", file=sys.stderr)
        return 2
    # ⚠️⚠️⚠️ **第三方改动检测（fail-closed）** ——
    #    还原 MUST NOT **覆盖**「备份之后」发生的**其它**改动 ⇒ ⇒ 否则
    #    「备份 → 投毒 → 还原」这条链路会**静默吞掉**工作区的并发编辑
    #    ⇒ ⇒ ⇒ **实测踩中（2026-10-31）**：跑一次 `--self-test` ⇒ ⇒
    #    `include/chlib/stream.h` 里**已应用的注释订正被冲掉** ⇒ ⇒
    #    ⇒ 而 `git status` 当时**干净** ⇒ ⇒ ⇒ **改动凭空消失**。
    #    ⇒ ⇒ **判据**：当前文件 MUST 仍**逐字等于**「被投毒的那份」
    #    ⇒ ⇒ 若不等 ⇒ ⇒ 说明**有第三方改动** ⇒ ⇒ ⇒ **拒绝还原并报错**
    #    ⇒ ⇒ ⇒ **MUST NOT** 覆盖它（⇒ ⇒ fail-closed）。
    if POISONED_SNAPSHOT.exists():
        current = STREAM_H.read_text(encoding="utf-8")
        poisoned = POISONED_SNAPSHOT.read_text(encoding="utf-8")
        if current != poisoned:
            print("RESTORE_FAIL: 当前文件已被第三方修改 ⇒ ⇒ 拒绝还原"
                  "（否则会覆盖那处改动）⇒ ⇒ 请人工处理", file=sys.stderr)
            print(f"  备份: {BACKUP}", file=sys.stderr)
            print(f"  投毒快照: {POISONED_SNAPSHOT}", file=sys.stderr)
            return 2
    else:
        # ⚠️⚠️⚠️ **陈旧备份检测**（P1-a 实测踩中 · **第 4 个盲区**）
        #    ⇒ ⇒ `BACKUP` 是**固定路径** ⇒ ⇒ ⇒ **跨次运行会残留**
        #    ⇒ ⇒ ⇒ **自检开头的 `restore_stream` 会用陈旧备份覆盖工作区**
        #    ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 实测：一次 `--self-test` 把工作区的全部改动冲掉**
        #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ MUST 在「无投毒快照」时拒绝还原**
        #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ （⇒ ⇒ 没有快照 ⇒ ⇒ 说明**本次运行没投毒**
        #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 此时还原无意义且危险**）
        print("RESTORE_FAIL: 无投毒快照 ⇒ ⇒ 本次运行未投毒 ⇒ ⇒ 拒绝还原"
              "（固定备份路径会跨次残留 ⇒ ⇒ 陈旧备份会覆盖工作区改动）",
              file=sys.stderr)
        print(f"  备份: {BACKUP}", file=sys.stderr)
        return 2
    # ⚠️⚠️ **MUST 用 `copyfile`（不保留 mtime），MUST NOT 用 `copy2`** ——
    #    `copy2` **保留** mtime ⇒ ⇒ 还原后的文件 mtime **比备份还旧**
    #    ⇒ ⇒ `make` 判定「已是最新」⇒ ⇒ **不重新编译** ⇒ ⇒ **跑的仍是被投毒的
    #    二进制** ⇒ ⇒ 报「还原写坏了文件」的**假失败**。
    #    ⇒ ⇒ 这是本仓「**还原源码 ≠ 复原**（二进制也是状态）」那条教训的
    #    又一实例 ⇒ ⇒ 还原后 MUST 让 mtime **变新** ⇒ ⇒ 强制重建。
    shutil.copyfile(BACKUP, STREAM_H)
    if POISONED_SNAPSHOT.exists():
        POISONED_SNAPSHOT.unlink()
    return 0


def main() -> int:
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    if cmd == "check":
        r2()
        r3()
        r4()
        r5()
        r6()
        r8()
        if FAILS:
            print(f"{RED}❌ 静态判据 FAIL（{FAILS} 项）{RESET}")
            return 1
        print(f"{GREEN}✅ 静态判据全部通过{RESET}")
        return 0
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
