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
BACKUP = pathlib.Path("/tmp/opencode/stream_h_ownership.bak")

EXEMPT_MARK = "stream_ready_ownership: intentionally-unwired"

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


# ── R6：调用点判据 —— 调 `stream_fifo` / `stream_queue` 后 MUST 接 ready ─────
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
}


def poison(name: str) -> int:
    if name not in POISONS:
        print(f"POISON_FAIL: 未知投毒 '{name}'（可选：{', '.join(POISONS)}）", file=sys.stderr)
        return 2
    old, new = POISONS[name]
    s = STREAM_H.read_text(encoding="utf-8")
    if old not in s:
        print(f"POISON_FAIL: 模式未找到 ⇒ 投毒未生效（fail-closed）: {name}", file=sys.stderr)
        return 2
    shutil.copy2(STREAM_H, BACKUP)
    STREAM_H.write_text(s.replace(old, new, 1), encoding="utf-8")
    return 0


def restore() -> int:
    if not BACKUP.exists():
        print("RESTORE_FAIL: 无备份", file=sys.stderr)
        return 2
    # ⚠️⚠️ **MUST 用 `copyfile`（不保留 mtime），MUST NOT 用 `copy2`** ——
    #    `copy2` **保留** mtime ⇒ ⇒ 还原后的文件 mtime **比备份还旧**
    #    ⇒ ⇒ `make` 判定「已是最新」⇒ ⇒ **不重新编译** ⇒ ⇒ **跑的仍是被投毒的
    #    二进制** ⇒ ⇒ 报「还原写坏了文件」的**假失败**。
    #    ⇒ ⇒ 这是本仓「**还原源码 ≠ 复原**（二进制也是状态）」那条教训的
    #    又一实例 ⇒ ⇒ 还原后 MUST 让 mtime **变新** ⇒ ⇒ 强制重建。
    shutil.copyfile(BACKUP, STREAM_H)
    return 0


def main() -> int:
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    if cmd == "check":
        r2()
        r3()
        r4()
        r5()
        r6()
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
