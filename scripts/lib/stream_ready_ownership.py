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
PIPELINE_H = REPO / "include" / "chlib" / "stream_pipeline.h"
BACKUP = pathlib.Path("/tmp/opencode/stream_h_ownership.bak")
# ⚠️ 「被投毒的内容」快照 —— 用于还原前的**第三方改动检测**
POISONED_SNAPSHOT = pathlib.Path("/tmp/opencode/stream_h_ownership.poisoned")
# ⚠️⚠️ **投毒目标的 sidecar 状态文件**（2026-11-01 critic NIT）
#    ⇒ ⇒ `poison` / `restore` 是**各自独立的 `python3` 进程**
#    ⇒ ⇒ ⇒ **进程内的模块全局**（`_last_poison_name`）**跨进程无效**
#    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 还原进程永远拿不到投毒名**
#    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 必须落盘** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
POISON_TARGET_FILE = pathlib.Path("/tmp/opencode/stream_h_ownership.target")

EXEMPT_MARK = "stream_ready_ownership: intentionally-unwired"

# ⚠️⚠️ **R8 的「已知未收口」豁免清单**（P1-a · 2026-11-01）
# ⚠️ **MUST 显式登记 + 理由 + 归属 change**（⇒ ⇒ 豁免**非静默** ⇒ ⇒ 否则
#    「已收口」的声明会变成谎言）
# ⚠️ ⚠️ **本清单 MUST 只减不增**（⇒ ⇒ 每次 P1 落地 MUST 删掉对应条目
#    ⇒ ⇒ ⇒ 否则「未收口」会被**永久豁免** ⇒ ⇒ ⇒ ⇒ **假绿**）
#    ⚠️ **新增豁免 MUST 有**：① 具体理由 ② 归属的独立 change ③ `::notice` 点名
UNSETTLED: dict[str, str] = {
    # ⚠️ 2026-11-02：`stream_fork`（ADR-057）与 `stream_demux`（ADR-056）**已收口**
    #    ⇒ ⇒ 两条豁免**已删除**（本清单「只减不增」）。
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


# ── R8 / R9：转发类（mux / arbiter / join / filters / combine_with）
#     + `connect_ready` 的多驱动守卫
#
# ⚠️⚠️ **编号订正（2026-11-01 · A3）**：**`R7` 从未实现** ⇒ ⇒
#   P1-a 的归档 `design.md` §4 曾写「R7（转发类**行为**判据）+ R8（转发类
#   **调用点**判据）」⇒ ⇒ ⇒ **其中 R7 无对应实现** ⇒ ⇒ ⇒
#   ⇒ **转发类的【行为】判据实际由 shell 侧的 `R1` 承载**（它跑整个
#   `test_stream` 二进制 ⇒ ⇒ 含转发类的行为用例）⇒ ⇒ ⇒
#   ⇒ ⇒ **本文件只有 `r2` / `r3` / `r4` / `r5` / `r6` / `r8` / `r9`** ⇒ ⇒
#   ⇒ ⇒ ⇒ **⇒ 「R7」这个编号是历史的空号** ⇒ ⇒ ⇒
#   ⇒ ⇒ ⇒ **⇒ 已在此如实登记（避免下一个人以为「少了一个判据」）**
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
# ⚠️⚠️⚠️ **R8 的正则 MUST 区分「输出侧」与「上游侧」**（2026-11-01 实测）
#    ⇒ ⇒ `result.output_stream.ready` 是**输出侧**（组合子**自己产出**）
#    ⇒ ⇒ 而 `result.ready`（filters 形态：返回**裸** `ch_stream`）是**上游侧**
#    ⇒ ⇒ ⇒ 若不区分 ⇒ ⇒ ⇒ **`has_drive` 会被输出侧的 `<<=` 置真**
#    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「删驱动」投毒后存在性判据**抓不到**
#    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **（实测：投毒后静态判据仍绿）**
_UPSTREAM_READY = re.compile(
    r"(?P<tgt1>\b(?:input_stream|input_streams|input1|input2))\s*"
    r"(?:\[[^\]]*\])?\s*\.ready\s*(?P<op1><<=|=(?!=))"
    r"|(?P<tgt2>\bresult)\s*(?:\[[^\]]*\])?\s*\.ready\s*(?P<op2><<=|=(?!=))"
    r"|(?P<tgt3>\bresult)\s*\.output_stream\s*\.ready\s*(?P<op3><<=|=(?!=))")


def _iter_stream_functions() -> list[tuple[str, str]]:
    """自动发现 `stream_*` 函数体（**穷举** ⇒ ⇒ 不靠符号清单）。"""
    out: list[tuple[str, str]] = []
    for f in (STREAM_H, ARBITER_H, PIPELINE_H):
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


def _is_pure_forwarder(body: str) -> bool:
    """该函数是否**纯转发**（只调用另一个组合子 ⇒ ⇒ 驱动在被调者里）。

    ⚠️ **为什么需要**：如 `stream_continue_when` ⇒ ⇒ 它只是
    `return stream_halt_when(input_stream, !continue_cond);` ⇒ ⇒ ⇒ **它自己
    不写 ready** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 存在性判据会误报** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
    ⇒ ⇒ **⇒ MUST 排除**。
    """
    # ⚠️ 判据 = 函数体里**调用了另一个 `stream_*` 组合子** **且** 本身
    #    **没有任何 ready 写入**（⇒ ⇒ 它的 ready 逻辑**全部**在被调者里）
    #    ⚠️⚠️ **MUST 同时要求两者**（⇒ ⇒ 否则「既调用别人又自己漏驱动」的
    #    组合子会被**误判为纯转发** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 漏检**）
    if not re.search(r"\bstream_[a-z_]+\s*\(", body):
        return False
    return not _UPSTREAM_READY.search(body)


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
        has_drive = False
        # ⚠️⚠️ **`stream_pipeline.h` 的 pipe 族**把上游驱动**收在一个共享实现**里
        #    （`detail::drive_upstream_ready`）⇒ ⇒ 它的函数体里**没有**
        #    `input_stream.ready <<=` 字样 ⇒ ⇒ 只看字面 `<<=` 会**误报**
        #    ⇒ ⇒ ⇒ 调用该 helper **就是**「驱动了上游」的证据
        #    ⇒ ⇒ ⇒ ⇒ ⇒ helper 自身的 `<<=` 由 **R11** 判（否则等于没判）。
        if re.search(r"\bdrive_upstream_ready\s*\(", body):
            has_drive = True
        # ⚠️ 同型：`stream.h` 的 `demux` / `fork` 把上游驱动收在
        #    `detail::drive_master_ready`（ADR-056 D5 / ADR-057 D5）
        #    ⇒ ⇒ 调用它就是「驱动了上游」的证据 ⇒ ⇒ helper 自身的 `<<=` 由 R13 判
        if re.search(r"\bdrive_master_ready\s*\(", body):
            has_drive = True
        for m in _UPSTREAM_READY.finditer(body):
            # ⚠️ **输出侧**字段 ⇒ ⇒ 组合子**自己产出** ⇒ ⇒ 驱动它合法
            target = m.group("tgt1") or m.group("tgt2") or m.group("tgt3")
            # ⚠️ **输出侧**判定：目标是 `output_stream` / `push_stream` /
            #    `pop_stream` ⇒ ⇒ 组合子**自己产出** ⇒ ⇒ 驱动它合法
            #    ⚠️ `result.output_stream.ready` ⇒ ⇒ target 是 `result`
            #    ⇒ ⇒ **必须**看**完整匹配文本**才知它是输出侧
            full = m.group(0)
            if target in OUTPUT_SIDE_FIELDS or "output_stream" in full \
                    or "push_stream" in full or "pop_stream" in full:
                continue
            op = m.group("op1") or m.group("op2") or m.group("op3")
            if op == "<<=":
                has_drive = True
            else:
                bad.append(f"{target}.ready = （operator= 重绑定 ⇒ ⇒ 调用方被孤儿化）")
        if bad:
            viol.append(f"{name}: " + "；".join(bad))
        elif not has_drive and not _is_pure_forwarder(body):
            # ⚠️⚠️⚠️ **存在性判据**（2026-11-01 critic SHOULD_FIX #3）
            #    ⇒ ⇒ R8 原先**只**找 `operator=` 形态 ⇒ ⇒ ⇒ **完全删除**上游
            #    驱动的组合子 ⇒ ⇒ **无 `=` 可匹配** ⇒ ⇒ ⇒ 被计为「已收口」
            #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「删驱动」这类回归**双判据**均不可见**
            #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ MUST 补存在性判据**
            viol.append(f"{name}: **无任何上游侧 `<<=` 驱动**"
                        f"（⇒ ⇒ 删驱动 ⇒ ⇒ 调用方收不到反压 ⇒ ⇒ 静默）")
        elif not has_drive:
            # ⚠️ **纯转发器**（如 `stream_continue_when` ⇒ ⇒ 只调用另一个
            #    组合子 ⇒ ⇒ 驱动在被调者里）⇒ ⇒ ⇒ **不在 R8 的管辖范围**
            #    ⇒ ⇒ ⇒ **跳过**（⇒ ⇒ 否则会误报）
            continue
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


def r9() -> None:
    """`connect_ready` MUST 有**多驱动守卫**（响亮失败 ⇒ 抛异常）。

    ⚠️ **为什么需要这条**（A3 · 2026-11-01）：`connect_ready` 用
    `<<=` 覆盖 ⇒ ⇒ `set_src(0, …)` ⇒ ⇒ **last-wins** ⇒ ⇒ 若调用方
    **已自行驱动**该字段 ⇒ ⇒ 再调 `connect_ready` 会**静默接管**调用方的
    驱动 ⇒ ⇒ 「我驱动的是我自己的信号」变成**假象** ⇒ ⇒ **静默失效**。

    ⚠️⚠️ **判据 = 结构判据**（MUST NOT 词法）：MUST 同时具备
      ① **比较式** `src(0) != default_node`（= 「当前驱动仍是默认节点」）
      ② **`CHFATAL_EXCEPTION`**（⇒ ⇒ **抛异常** ⇒ ⇒ MUST NOT 只打日志 ——
         实测 `CHERROR` / `CHFATAL` **只打日志不抛** ⇒ ⇒ 那是**假安全网**）

    ⚠️⚠️ **为什么它承重**：builder 丢字问题的**候选修法**（上游回接）
      **必须**让中间级的 `pop_stream.ready` 被覆盖 ⇒ ⇒ ⇒ ⇒ **⇒ 覆盖后
      再调 `connect_ready` MUST 抛**（这是**正确**的行为）⇒ ⇒ ⇒
      ⇒ ⇒ ⇒ **⇒ 而「串联场景对下游调 MUST NOT 抛」是它的孪生约束**
      ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 两者 MUST 同时成立** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 否则要么静默
      接管、要么误抛** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 行为用例已覆盖**
      （`tests/chlib/test_stream.cpp` 的 `[stream][ready][series]`）。
    """
    code = code_of(STREAM_H)
    m = re.search(r"void\s+stream_fifo_connect_ready\s*\([^)]*\)\s*\{", code)
    if not m:
        fail("R9 找不到 `stream_fifo_connect_ready` 的定义体")
        return
    depth = 0
    end = -1
    for k in range(m.end() - 1, len(code)):
        if code[k] == "{":
            depth += 1
        elif code[k] == "}":
            depth -= 1
            if depth == 0:
                end = k
                break
    if end < 0:
        fail("R9 `stream_fifo_connect_ready` 的函数体花括号不配平")
        return
    body = code[m.start(): end + 1]
    if "src(0) != default_node" not in body:
        fail("R9 `stream_fifo_connect_ready` 缺少多驱动守卫的**比较式**"
             "（`src(0) != default_node`）⇒ ⇒ 覆盖将 last-wins ⇒ ⇒ 静默")
        return
    if "CHFATAL_EXCEPTION" not in body:
        fail("R9 `stream_fifo_connect_ready` 的守卫 MUST 用 `CHFATAL_EXCEPTION`"
             "（⇒ ⇒ 抛异常）⇒ ⇒ MUST NOT 只打日志"
             "（`CHERROR` / `CHFATAL` 实测只打日志 ⇒ ⇒ 假安全网）")
        return
    ok("R9 `connect_ready` 有多驱动守卫（比较式 + `CHFATAL_EXCEPTION`）")


def _function_body(path: pathlib.Path, sig_re: str) -> str | None:
    """取函数体（**剥注释**后按花括号配平）⇒ ⇒ 找不到 ⇒ `None`（fail-closed）。"""
    if not path.exists():
        return None
    code = strip_comments_and_strings(path.read_text(encoding="utf-8"))
    m = re.search(sig_re, code)
    if not m:
        return None
    i = code.find("{", m.end() - 1)
    if i < 0:
        return None
    depth = 0
    j = i
    while j < len(code):
        if code[j] == "{":
            depth += 1
        elif code[j] == "}":
            depth -= 1
            if depth == 0:
                return code[m.start(): j + 1]
        j += 1
    return None


# ── R10：`StreamBuilder` 的**链内闭包不变式**（ADR-055 · B1）──────────────
#
# ⚠️⚠️ **为什么 MUST 有这条**（2026-11-01）：
#   `stream_fifo` **不驱动**入参 `ready`（写使能只用 `valid`）⇒ ⇒ builder 链里
#   **没人驱动** `source.ready` ⇒ ⇒ 它**恒为 0** ⇒ ⇒ 上游遵守 ready 协议
#   ⇒ ⇒ **永远推不动 = 死路**（实测 E10：正确接线 ⇒ 16 拍全 0）。
#   ⇒ ⇒ R8 **看不见**它：R8 只穷举 `stream_*(ch_stream …)` 形态的自由函数
#   ⇒ ⇒ ⇒ 而 builder 的链式入口是 **`StreamBuilder::queue<>()` 成员模板**
#   ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「扩 R8 扫描面」对 builder 无效** ⇒ ⇒ **必须独立判据**。
#
# ⚠️ **判据三条**（全部**结构**）：
#   ① **存在性**：`queue()` 体 MUST 调 `drive_pending_ready(`
#      ⇒ ⇒ 删掉回接 ⇒ ⇒ 上游收不到反压 ⇒ ⇒ **红**（⚠️ 本仓已登记的盲区：
#      只查 `=` 形态 ⇒ ⇒ 「整个删掉」不可见）；
#   ② **连接手段**：`drive_pending_ready()` 体的驱动 MUST 是 `<<=`
#      ⇒ ⇒ `operator=`（只重绑定本地副本的指针）⇒ ⇒ **调用方被孤儿化** ⇒ ⇒ 红；
#      ⚠️ 但 `adopt_stage_output()` 里的 `pending_ready_ = …` 是**别名登记**
#      （不是连接）⇒ ⇒ 判据 MUST **限定在 `drive_pending_ready` 体内**；
#   ③ **响亮拒绝**：`drive_pending_ready()` 体 MUST 有 ≥3 处
#      `CHFATAL_EXCEPTION`（无节点 / literal / 多驱动三种「驱动不了」）
#      + `type_lit` + `num_srcs()` ⇒ ⇒ 三者缺一 ⇒ ⇒ 「静默失效」回来了
#      ⇒ ⇒ （⚠️ `CHERROR` / `CHFATAL` **只打日志不抛** ⇒ ⇒ 假安全网）。
def r10() -> None:
    """`StreamBuilder` 的链内闭包不变式 MUST 有可机械核验的实现。"""
    if not BUILDER_H.exists():
        fail("R10 缺少 include/chlib/stream_builder.h")
        return

    queue_body = _function_body(
        BUILDER_H, r"StreamBuilder<T>\s*&\s*queue\s*\(\s*\)\s*\{")
    if queue_body is None:
        fail("R10 找不到 `StreamBuilder::queue<>()` 的定义体")
        return
    if "drive_pending_ready(" not in queue_body:
        fail("R10 `queue()` MUST 调用 `drive_pending_ready(...)`（链内闭包"
             "不变式：本级的容量信号 ⇒ ⇒ 驱动上一级的输出 ready）"
             "⇒ ⇒ 缺 ⇒ ⇒ 上游永远收不到反压（`source.ready` 恒 0 = 死路）")
        return

    drive_body = _function_body(
        BUILDER_H, r"void\s+drive_pending_ready\s*\([^)]*\)\s*\{")
    if drive_body is None:
        fail("R10 找不到 `drive_pending_ready()` 的定义体")
        return
    if "pending_ready_ <<=" not in drive_body:
        fail("R10 `drive_pending_ready()` MUST 用 `<<=` 驱动 `pending_ready_`"
             "（`operator=` 只重绑定指针 ⇒ ⇒ 上一级的驱动被孤儿化 ⇒ ⇒ 静默）")
        return
    if re.search(r"pending_ready_\s*=(?!=)", drive_body):
        fail("R10 `drive_pending_ready()` 内 MUST NOT 用 `operator=` 驱动 "
             "`pending_ready_`（节点重绑定 ≠ 连接）")
        return
    if "type_lit" not in drive_body:
        fail("R10 `drive_pending_ready()` MUST 检出 **literal** 形态"
             "（literal 不可被驱动 ⇒ ⇒ 反压静默丢失）")
        return
    if "num_srcs()" not in drive_body:
        fail("R10 `drive_pending_ready()` MUST 检出**多驱动**（链首 `num_srcs() > 0`"
             " ⇒ ⇒ 再驱动是 last-wins ⇒ ⇒ 静默接管调用方的信号）")
        return
    n_loud = drive_body.count("CHFATAL_EXCEPTION")
    if n_loud < 3:
        fail(f"R10 `drive_pending_ready()` 的**响亮拒绝**只有 {n_loud} 处"
             "（MUST ≥3：无节点 / literal / 多驱动）⇒ ⇒ 少一处就有一格"
             "「静默失效」⇒ ⇒ ⚠️ MUST 用 `CHFATAL_EXCEPTION`（**抛**），"
             "MUST NOT 用只打日志的 `CHERROR` / `CHFATAL`")
        return
    ok("R10 builder 链内闭包不变式齐备（回接存在 · `<<=` 连接 · 3 处响亮拒绝）")


# ── R11：`stream_pipeline.h` 的**共享上游驱动实现**（B6 · ADR-055）──────────
#
# ⚠️ **为什么 R8 不够**：R8 只看「组合子的函数体里有没有驱动上游」
#   ⇒ ⇒ pipe 族把驱动收在 `detail::drive_upstream_ready` 里（⇒ 一处实现、
#   三处使用）⇒ ⇒ **`<<=` 是否真的在那一处** R8 **看不见**
#   ⇒ ⇒ ⇒ 若有人把 helper 里的 `<<=` 改成 `operator=` ⇒ ⇒ 三个 pipe **同时**
#   静默失效 ⇒ ⇒ 而 R8 仍绿（它只看到「调用了 helper」）⇒ ⇒ **假绿**
#   ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 必须对 helper 本身有结构判据**。
def r11() -> None:
    """`drive_upstream_ready` MUST 用 `<<=` 且 MUST 有 3 处响亮拒绝。"""
    if not PIPELINE_H.exists():
        fail("R11 缺少 include/chlib/stream_pipeline.h")
        return
    body = _function_body(
        PIPELINE_H, r"void\s+drive_upstream_ready\s*\([^)]*\)\s*\{")
    if body is None:
        fail("R11 找不到 `drive_upstream_ready()` 的定义体")
        return
    if "upstream.ready <<=" not in body:
        fail("R11 `drive_upstream_ready()` MUST 用 `<<=` 驱动 `upstream.ready`"
             "（`operator=` 只重绑定本地副本 ⇒ ⇒ 调用方被孤儿化 ⇒ ⇒ 三个 pipe"
             "**同时**静默失效）")
        return
    if re.search(r"upstream\.ready\s*=(?!=)", body):
        fail("R11 `drive_upstream_ready()` 内 MUST NOT 用 `operator=` 驱动 "
             "`upstream.ready`（节点重绑定 ≠ 连接）")
        return
    if "type_lit" not in body:
        fail("R11 `drive_upstream_ready()` MUST 检出 **literal** 目标形态")
        return
    if "num_srcs()" not in body:
        fail("R11 `drive_upstream_ready()` MUST 检出**多驱动**")
        return
    n_loud = body.count("CHFATAL_EXCEPTION")
    if n_loud < 3:
        fail(f"R11 `drive_upstream_ready()` 的**响亮拒绝**只有 {n_loud} 处"
             "（MUST ≥3：无节点 / literal 目标 / 多驱动）⇒ ⇒ 少一处就有一格"
             "「静默失效」")
        return
    # ⚠️ 三个 pipe MUST 真的**用它**（MUST NOT 各写各的 ⇒ 漂移）
    for fn, label in (("stream_m2s_pipe", "m2s"),
                      ("stream_s2m_pipe", "s2m"),
                      ("stream_half_pipe", "half")):
        b = _function_body(PIPELINE_H, rf"{fn}\s*\([^)]*\)\s*\{{")
        if b is None:
            fail(f"R11 找不到 `{fn}` 的定义体")
            return
        if "drive_upstream_ready(" not in b:
            fail(f"R11 `{fn}`（{label} pipe）MUST 调用 `drive_upstream_ready(...)`"
                 "（上游 ready 由 pipe 驱动 ⇒ 否则上游收不到反压）")
            return
    ok("R11 pipe 族上游驱动齐备（共用 `<<=` 实现 · 3 处响亮拒绝 · 3 个 pipe 全用）")


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
    # ⚠️⚠️⚠️ **R8 专用投毒（P1-a · 存在性判据）**：**完全删掉**上游驱动
    #    ⇒ ⇒ 无 `=` 可匹配 ⇒ ⇒ ⇒ 若无存在性判据 ⇒ ⇒ **R8 会误判为「已收口」**
    #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ R8 MUST 转红**（判别力证明）
    "forward-delete-upstream-drive": (
        """        result.input_streams[i].ready <<=
            (is_selected & result.output_stream.ready);""",
        """        // ⚠️ 投毒：**删掉**上游驱动（⇒ ⇒ 无 `=` ⇒ ⇒ 靠存在性判据检出）""",
    ),
    # ⚠️⚠️⚠️ **跨文件投毒**（2026-11-01 critic NIT 的**验证**）：落在
    #    `stream_arbiter.h` ⇒ ⇒ ⇒ **⇒ 必须验证「还原回到正确的文件」**
    #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 进程内全局做不到** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ sidecar
    #    状态文件是必需的**（⇒ ⇒ 守栏自检若不覆盖它 ⇒ ⇒ ⇒ **⇒ N2 的修复
    #    仍是「未验证」** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 本投毒就是那个验证**）
    "arbiter-upstream-operator-assign": (
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
    # ⚠️⚠️⚠️ **P2 投毒（2026-11-01 · change `stream-arbiter-rr-rotation`）**
    #    `stream_arbiter_round_robin` 的轮转反馈 ⇒ ⇒ **断回常量**
    #    （= **修复前的形态**）⇒ ⇒ ⇒ **轮转消失 ⇒ 通道饿死**
    #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 判别力用例（`[rotation]` tag）MUST 转红**
    #
    #    ⚠️⚠️ **归入「仅行为判据」类别**（同 `drop-multidrive-guard`）：
    #    本变异**不改变任何结构判据**能看见的东西（`<<=` 仍是 `<<=`、
    #    驱动仍存在）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 静态判据仍绿是【正确】的**
    #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ MUST NOT 强制「静态判据也红」**（会误报）
    #    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 见 shell 侧的 `case` 分类**
    # ⚠️⚠️ **A3 投毒（2026-11-01）**：把 `connect_ready` 的**多驱动守卫**
    #    移除 ⇒ ⇒ 覆盖变 last-wins ⇒ ⇒ 静默接管调用方的驱动
    #    ⇒ ⇒ ⇒ ⇒ **⇒ R9（结构判据）MUST 转红**（比较式消失）
    #    ⇒ ⇒ ⇒ ⇒ **⇒ 且行为用例 MUST 转红**
    #    （`REQUIRE_THROWS_AS` ⇒ ⇒ 用例期望抛异常 ⇒ ⇒ 不抛 ⇒ ⇒ 红）
    "connect-ready-guard-removed": (
        "    if (ready_node->num_srcs() == 0 || ready_node->src(0) != default_node) {",
        "    if (false) {  // ⚠️ 投毒：移除多驱动守卫",
    ),
    # ⚠️⚠️ **P2 投毒（2026-11-01 · change `stream-arbiter-rr-rotation`）**
    "rr-feedback-const": (
        "auto arb_result = round_robin_selector<N_INPUTS>(valid_vector, "
        "last_grant_reg);",
        "auto arb_result = round_robin_selector<N_INPUTS>(valid_vector, "
        "ch_uint<N_INPUTS>(0_d));",
    ),
    # ⚠️⚠️⚠️ **R10 专用投毒（B1 · ADR-055）**：**完全删掉**链内回接
    #    ⇒ ⇒ `queue()` 里不再有 `drive_pending_ready(` ⇒ ⇒ 存在性判据 MUST 转红
    #    ⇒ ⇒ 行为：`source.ready` 无驱动 ⇒ ⇒ `num_srcs() == 0` ⇒ ⇒ 用例转红
    "builder-drop-chain-closure": (
        "        drive_pending_ready(result.push_stream.ready);",
        "        // ⚠️ 投毒：删掉链内回接（上游收不到反压）",
    ),
    # ⚠️⚠️ **R10 专用投毒**：驱动 `<<=` ⇒ ⇒ `operator=`（**只重绑定本地副本**）
    #    ⇒ ⇒ 调用方的 `source.ready` 仍无驱动 ⇒ ⇒ 静默
    "builder-upstream-operator-assign": (
        "        pending_ready_ <<= capacity;",
        "        pending_ready_ = capacity;",
    ),
    # ⚠️⚠️ **R13 专用投毒（ADR-056 / ADR-057）**：`demux` / `fork` **共享**的
    #    上游驱动 `<<=` ⇒ `operator=` ⇒ ⇒ **两个组件同时静默失效**
    #    ⇒ ⇒ R13 MUST 转红；行为面 = `in_ready` 恒 0（默认态也收不到）
    "demux-fork-master-ready-operator-assign": (
        "    upstream.ready <<= driver;",
        "    upstream.ready = driver;",
    ),
    # ⚠️⚠️⚠️ **存在性投毒**：**完全删掉** `stream_demux` 的上游驱动调用
    #    ⇒ ⇒ 无 `=` 可匹配 ⇒ ⇒ 靠 R13 的「两处都必须用它」+ R8 的存在性判据
    "demux-fork-drop-drive": (
        '    detail::drive_master_ready(input_stream, upstream_ready, "stream_demux");',
        "    // ⚠️ 投毒：删掉 demux 的上游驱动（上游收不到反压）",
    ),
    # ⚠️⚠️⚠️ **R11 专用投毒（B6）**：pipe 族**共享**的上游驱动 `<<=` ⇒ `operator=`
    #    ⇒ ⇒ **一条改动让三个 pipe 同时静默失效** ⇒ ⇒ R11 MUST 转红
    #    ⇒ ⇒ 行为：`source.ready` 无驱动（`num_srcs() == 0`）⇒ ⇒ 用例转红
    "pipe-upstream-operator-assign": (
        "    upstream.ready <<= driver;",
        "    upstream.ready = driver;",
    ),
}

# ⚠️ **投毒的目标文件**（⇒ ⇒ R8 的投毒可能落在 `stream_arbiter.h`）
POISON_TARGETS: dict[str, pathlib.Path] = {
    # ⚠️ **跨文件投毒**（⇒ ⇒ 验证 sidecar 状态文件的必要性）
    "arbiter-upstream-operator-assign": ARBITER_H,
    # ⚠️ R10（builder 链内闭包不变式）⇒ ⇒ 落在 `stream_builder.h`
    "builder-drop-chain-closure": BUILDER_H,
    "builder-upstream-operator-assign": BUILDER_H,
    # ⚠️ R11（pipe 族共享上游驱动）⇒ ⇒ 落在 `stream_pipeline.h`
    "pipe-upstream-operator-assign": PIPELINE_H,
    # ⚠️ R13（demux / fork 共享上游驱动）⇒ ⇒ 落在 `stream.h`
    "demux-fork-master-ready-operator-assign": STREAM_H,
    "demux-fork-drop-drive": STREAM_H,
}




def _poison_target() -> pathlib.Path:
    """本次投毒的目标文件（**跨进程** ⇒ ⇒ MUST 从**状态文件**读）。

    ⚠️⚠️ **为什么 MUST 落盘**（2026-11-01 critic NIT）：
    `poison` / `restore` / `check` 是**各自独立的 `python3` 进程**
    ⇒ ⇒ ⇒ **进程内的模块全局**（如 `_last_poison_name`）**跨进程无效**
    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 还原进程永远拿不到投毒名**
    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「登记为已修但实际未修」**
    ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ MUST 用 sidecar 状态文件**。
    """
    if POISON_TARGET_FILE.exists():
        try:
            p = pathlib.Path(POISON_TARGET_FILE.read_text(encoding="utf-8").strip())
            if p.exists():
                return p
        except (OSError, ValueError):
            pass
    return STREAM_H


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
    # ⚠️⚠️ **投毒目标 MUST 落盘**（跨进程 ⇒ ⇒ `restore` 是**另一个**进程）
    POISON_TARGET_FILE.write_text(str(target), encoding="utf-8")
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
        # ⚠️⚠️ **检测范围 MUST 覆盖投毒的实际目标**（2026-11-01 NIT #2）
        #    ⇒ ⇒ `POISON_TARGETS` 机制允许投毒落在 `stream_arbiter.h`
        #    ⇒ ⇒ ⇒ 若只比对 `STREAM_H` ⇒ ⇒ 检测会**校验错文件**
        #    ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 当前无实际影响**（6 种投毒都在 stream.h）
        #    ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 但将来加 arbiter 侧投毒时会漏** ⇒ ⇒ ⇒
        #    ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 已改为「比对所有候选目标」**
        target = _poison_target()
        current = target.read_text(encoding="utf-8")
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
    shutil.copyfile(BACKUP, _poison_target())
    if POISONED_SNAPSHOT.exists():
        POISONED_SNAPSHOT.unlink()
    if POISON_TARGET_FILE.exists():
        POISON_TARGET_FILE.unlink()
    return 0


# ── R13：`stream.h` 的**共享上游驱动实现**（ADR-056 / ADR-057）────────────
#
# ⚠️ **为什么 R8 不够**：`demux` / `fork` 把上游驱动收在
#   `chlib::detail::drive_master_ready`（一处实现、两处使用）
#   ⇒ ⇒ 若有人把那里的 `<<=` 改成 `operator=` ⇒ ⇒ **两个组件同时静默失效**
#   而 R8 仍绿（它只看到「调用了 helper」）⇒ ⇒ **必须对 helper 本身有判据**
#   （与 R11 对 `stream_pipeline.h` 的 `drive_upstream_ready` 同型，理由同）。
def r13() -> None:
    """`drive_master_ready` MUST 用 `<<=` 且 MUST 有 3 处响亮拒绝。"""
    if not STREAM_H.exists():
        fail("R13 缺少 include/chlib/stream.h")
        return
    body = _function_body(
        STREAM_H, r"void\s+drive_master_ready\s*\([^)]*\)\s*\{")
    if body is None:
        fail("R13 找不到 `detail::drive_master_ready()` 的定义体")
        return
    if "upstream.ready <<=" not in body:
        fail("R13 `drive_master_ready()` MUST 用 `<<=` 驱动 `upstream.ready`"
             "（`operator=` 只重绑定本地句柄 ⇒ ⇒ 调用方被孤儿化 ⇒ ⇒ demux/fork"
             "**同时**静默失效）")
        return
    if re.search(r"upstream\.ready\s*=(?!=)", body):
        fail("R13 `drive_master_ready()` 内 MUST NOT 用 `operator=` 驱动 "
             "`upstream.ready`（节点重绑定 ≠ 连接）")
        return
    if "type_lit" not in body:
        fail("R13 `drive_master_ready()` MUST 检出 **literal** 目标形态")
        return
    if "num_srcs()" not in body:
        fail("R13 `drive_master_ready()` MUST 检出**多驱动**")
        return
    n_loud = body.count("CHFATAL_EXCEPTION")
    if n_loud < 3:
        fail(f"R13 `drive_master_ready()` 的**响亮拒绝**只有 {n_loud} 处"
             "（MUST ≥3：无节点 / literal 目标 / 多驱动）")
        return
    # ⚠️ `demux` / `fork` MUST 真的**用它**（MUST NOT 各写各的 ⇒ 漂移）
    # ⚠️ 不能用 `_function_body` 的 `[^)]*` 形参模式：`stream_demux` 的形参里
    #    含 `compute_idx_width(N_OUTPUTS)` ⇒ ⇒ 第一个 `)` 就把匹配截断
    #    ⇒ ⇒ 改用「找名字 ⇒ 定位其后的 `{` ⇒ 花括号配平」。
    code_all = strip_comments_and_strings(STREAM_H.read_text(encoding="utf-8"))
    for fn, label in (("stream_demux", "demux"), ("stream_fork", "fork")):
        m = re.search(rf"\b{fn}\s*\(", code_all)
        if not m:
            fail(f"R13 找不到 `{fn}` 的定义")
            return
        i = code_all.find("{", m.end())
        depth, j, end = 0, i, -1
        while j >= 0 and j < len(code_all):
            if code_all[j] == "{":
                depth += 1
            elif code_all[j] == "}":
                depth -= 1
                if depth == 0:
                    end = j
                    break
            j += 1
        if end < 0:
            fail(f"R13 `{fn}` 的函数体花括号不配平")
            return
        b = code_all[i:end + 1]
        if "drive_master_ready(" not in b:
            fail(f"R13 `{fn}`（{label}）MUST 调用 `drive_master_ready(...)`"
                 "（上游 ready 由组合子驱动 ⇒ 否则上游收不到反压）")
            return
    ok("R13 demux / fork 的上游驱动齐备（共用 `<<=` 实现 · 3 处响亮拒绝 · 两处全用）")


def main() -> int:
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    if cmd == "check":
        r2()
        r3()
        r4()
        r5()
        r6()
        r8()
        r9()
        r10()
        r11()
        r13()
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
