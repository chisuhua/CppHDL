#!/usr/bin/env python3
"""「头文件自包含性」判据（每个 `include/**/*.h` MUST 能被**单独**编译）。

⚠️ **为什么要有它**：根 AGENTS.md 的 CONVENTIONS 明写「**Headers
   self-contained**；no `.cpp` include of other `.cpp`」⇒ ⇒ 但该约定**从未被
   机械核验** ⇒ ⇒ 于是「靠 `-I` 巧合才能编译」的���**长期存在**：
   - `include/cpu/pipeline/stages/id_stage.h` include `"rv32i_decoder.h"`
     （在**上一级**目录）⇒ ⇒ 单独 include 编译失败；
   - `include/cpu/pipeline/stages/ex_stage.h` 用 `OP_OPIMM` 等 decoder 常量
     却**没有** include decoder 头 ⇒ ⇒ 单独 include 编译失败；
   - ⚠️ **`#include "chlib.h"`（`include/AGENTS.md` 记载的**公共入口**）
     单独编译失败** —— 根因是 `bundle/stream_bundle_member_inlines.h` 在
     `namespace ch` 内写 `chlib::X` ⇒ ⇒ 被同名的 `ch::chlib` **遮蔽**了
     顶层 `::chlib` ⇒ ⇒ 而 `chlib.h` 先 include 了声明 `ch::chlib` 的
     `chlib/simulator_trace.h` ⇒ ⇒ **包含顺序相关**的编译失败。

⚠️ **判据分三条**：
   R1 **未豁免的头 MUST 能单独编译**：为每个头生成一个只 include 它的 TU，
      用 `g++ -fsyntax-only` 编译 ⇒ ⇒ 失败 ⇒ **红**。
   R2a **豁免 MUST 仍必要**：名单里的每个头 MUST **确实**编译失败
      ⇒ ⇒ 「豁免已过时」（分片被补全了）⇒ ⇒ **红**（防止豁免变成万能借口）。
   R2b **豁免 MUST 与实际失败集合一致**：任何「新出现的失败头」⇒ ⇒ **红**
      （⇒ ⇒ 漏登记会被 R1 抓到，R2b 是同一事实的显式化）。
   R0 **fail-closed**：编译器不存在 / 名单文件缺失 ⇒ ⇒ **红**（MUST NOT
      静默通过）—— 本仓「护栏自身有盲区」病症的一贯处置。

⚠️ **豁免名单只收「分片头」**（设计意图：被聚合头 include 后才有意义）⇒ ⇒
   **公共入口头 MUST NOT 在名单里** ⇒ ⇒ 名单本身由 R2 双向核对。

用法：
    python3 scripts/lib/header_self_contained.py check
    python3 scripts/lib/header_self_contained.py list-poisons
    python3 scripts/lib/header_self_contained.py poison <名字>
    python3 scripts/lib/header_self_contained.py restore
"""
from __future__ import annotations

import concurrent.futures
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
INCLUDE_DIR = REPO / "include"
EXEMPT_FILE = pathlib.Path(__file__).resolve().parent / "header_self_contained_exempt.txt"
SNAPSHOT = pathlib.Path("/tmp/opencode/header_self_contained.snapshot")
POISON_TARGET = pathlib.Path("/tmp/opencode/header_self_contained.target")

# 与根 CMakeLists.txt 的 CH_INCLUDE_DIRS 保持一致
INCLUDE_PATHS = [
    "include",
    "include/core",
    "include/ast",
    "include/abstract",
    "include/utils",
    "external/inipp",
]

# 投毒目标：把一个**必需**的 include 改成**不存在的路径** ⇒ ⇒ 该头不再自包含
# ⇒ ⇒ R1 转红。
# ⚠️ **为什么用「路径写错」而不是「删掉 include」**：删掉后该符号可能被**传递性**
#    include 补上 ⇒ ⇒ 头**仍然**能编译 ⇒ ⇒ 投毒**无效**（判别力为零）
#    ⇒ ⇒ 改路径 ⇒ ⇒ `fatal error: file not found` ⇒ ⇒ **确定性**失败。
POISON_BREAK_INCLUDE = "chlib/fifo.h"
POISON_BREAK_OLD = '#include "core/uint.h"\n'
POISON_BREAK_NEW = '#include "core/uint_poisoned.h"  // POISON: include 路径写错\n'


def compiler() -> str | None:
    for cand in (os.environ.get("CXX"), "g++", "c++", "clang++"):
        if cand and shutil.which(cand):
            return cand
    return None


# ── 缓存（增量）────────────────────────────────────────────────────────────
# ⚠️ **为什么需要缓存**：单个头的语法检查 ≈ 1.5–3 s ⇒ ⇒ 162 个头一次全扫
#    ≈ 1–2 min（并行）⇒ ⇒ 而自检要跑 **7** 次全扫 ⇒ ⇒ ≈ 12 min ⇒ ⇒ CI 不可接受。
# ⇒ ⇒ **缓存键 = 该头的「传递 include 闭包」的内容指纹** ⇒ ⇒
#    ⇒ 只改了 `chlib/fifo.h` ⇒ ⇒ 只有**依赖它**的头失效 ⇒ ⇒ 其余命中缓存。
# ⚠️ **缓存键 MUST 覆盖传递闭包**（不是只覆盖该头自己）—— 否则「A 改了 ⇒ B 的
#    编译结果变了」会被缓存掩盖 ⇒ ⇒ 那正是「护栏自身有盲区」。
CACHE_FILE = pathlib.Path("/tmp/opencode/header_self_contained.cache.json")

_INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.M)


def _resolve(includer_rel: str, target: str) -> str | None:
    """把 `#include "X"` 解析成 include/ 下的相对路径（找不到 ⇒ None）。"""
    cands = []
    parent = str(pathlib.PurePosixPath(includer_rel).parent)
    if parent not in (".", ""):
        cands.append(f"{parent}/{target}")
    cands.append(target)
    for ip in INCLUDE_PATHS:
        cands.append(f"{ip[len('include/'):]}/{target}" if ip.startswith("include/")
                     else f"{ip}/{target}")
    for c in cands:
        p = (INCLUDE_DIR / c).resolve()
        if p.exists() and p.is_file():
            try:
                return p.relative_to(INCLUDE_DIR.resolve()).as_posix()
            except ValueError:
                return None
    return None


def _global_salt() -> str:
    """全局盐：编译器版本 + 编译选项 + **非 include/ 的外部依赖**指纹。

    ⚠️ **为什么需要**：闭包指纹只覆盖 `include/**` ⇒ ⇒ 若编译器换了、或
    `external/inipp` 变了 ⇒ ⇒ 编译结果会变 ⇒ ⇒ 缓存必须失效。
    """
    h = hashlib.sha1()
    cxx = compiler() or ""
    h.update(cxx.encode())
    try:
        v = subprocess.run([cxx, "--version"], capture_output=True, text=True,
                           errors="replace", timeout=30)
        h.update(v.stdout.encode())
    except Exception:  # noqa: BLE001 - fail-safe：拿不到版本就跳过
        pass
    h.update("|".join(INCLUDE_PATHS).encode())
    h.update(b"|std=c++23|fsyntax-only")
    for ext in ("external/inipp",):
        p = REPO / ext
        if p.exists():
            for f in sorted(p.rglob("*")):
                if f.is_file():
                    try:
                        st = f.stat()
                        h.update(f"{f}:{st.st_mtime_ns}:{st.st_size}".encode())
                    except OSError:
                        pass
    return h.hexdigest()


def _closure_fingerprints() -> dict[str, str]:
    """每个头的「传递 include 闭包」内容指纹（sha1）。"""
    headers = discover_headers()
    salt = _global_salt()
    graph: dict[str, list[str]] = {}
    for rel in headers:
        deps: list[str] = []
        try:
            text = (INCLUDE_DIR / rel).read_text(errors="replace")
        except OSError:
            text = ""
        for m in _INCLUDE_RE.finditer(text):
            t = _resolve(rel, m.group(1))
            if t:
                deps.append(t)
        graph[rel] = deps

    memo: dict[str, str] = {}

    def fp(rel: str, seen: frozenset[str] = frozenset()) -> str:
        if rel in memo:
            return memo[rel]
        if rel in seen:  # 环 ⇒ 截断（环在真实编译里也会被展开）
            return ""
        h = hashlib.sha1()
        p = INCLUDE_DIR / rel
        try:
            st = p.stat()
            h.update(f"{rel}:{st.st_mtime_ns}:{st.st_size}\n".encode())
        except OSError:
            h.update(f"{rel}:missing\n".encode())
        for d in sorted(graph.get(rel, [])):
            h.update((fp(d, seen | {rel}) or "").encode())
        out = h.hexdigest()
        memo[rel] = out
        return out

    return {rel: hashlib.sha1((salt + fp(rel)).encode()).hexdigest()
            for rel in headers}


def _load_cache() -> dict:
    try:
        return json.loads(CACHE_FILE.read_text())
    except (OSError, ValueError):
        return {}


def _save_cache(cache: dict) -> None:
    try:
        CACHE_FILE.parent.mkdir(parents=True, exist_ok=True)
        CACHE_FILE.write_text(json.dumps(cache))
    except OSError:
        pass


def load_exempt() -> dict[str, tuple[str, str]]:
    """返回 {路径: (kind, 理由)}；kind = `fragment` 或 `stdlib:<能力>`。"""
    out: dict[str, tuple[str, str]] = {}
    if not EXEMPT_FILE.exists():
        return out
    for line in EXEMPT_FILE.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        kind = parts[1].strip() if len(parts) > 1 else "fragment"
        reason = parts[2].strip() if len(parts) > 2 else ""
        out[parts[0].strip()] = (kind, reason)
    return out


def probe_stdlib(cxx: str, capability: str) -> bool:
    """探测「当前编译器的标准库是否**提供该能力**」。返回 True = 提供。

    ⚠️ **探测的必须是「能力」本身，不是「头文件能否被 include」** ——
       实测（2026-11-03）：某 libc++ 环境下 `#include <expected>` **成功**
       （能找到那个头）⇒ ⇒ 但 `std::expected` **不存在** ⇒ ⇒ 只探「能否
       include」会**误判为「有能力」** ⇒ ⇒ 豁免错误地失效 ⇒ ⇒ 真缺陷被
       掩盖 ⇒ ⇒ ⇒ 探测必须**用到那个名字**。
    """
    if capability == "expected":
        body = ("#include <expected>\n"
                "int main() { std::expected<void, int> e{}; "
                "return e.has_value() ? 0 : 1; }\n")
    else:
        body = f"#include <{capability}>\nint main() {{ return 0; }}\n"
    with tempfile.TemporaryDirectory() as td:
        tu = pathlib.Path(td) / "probe.cpp"
        tu.write_text(body)
        cmd = [cxx, "-std=c++23", "-fsyntax-only", "-w"]
        cmd.append(str(tu))
        try:
            p = subprocess.run(cmd, capture_output=True, text=True,
                               errors="replace", timeout=120)
        except subprocess.TimeoutExpired:
            return True  # 探测失败 ⇒ 保守：视为「有」⇒ ⇒ 不豁免
        return p.returncode == 0


def discover_headers() -> list[str]:
    return sorted(
        p.relative_to(INCLUDE_DIR).as_posix() for p in INCLUDE_DIR.rglob("*.h")
    )


def compile_one(cxx: str, rel: str) -> tuple[str, bool, str]:
    """返回 (rel, ok, 首条错误)"""
    with tempfile.TemporaryDirectory() as td:
        tu = pathlib.Path(td) / "tu.cpp"
        tu.write_text(f'#include "{rel}"\nint main() {{ return 0; }}\n')
        cmd = [cxx, "-std=c++23", "-fsyntax-only", "-w"]
        for ip in INCLUDE_PATHS:
            cmd += ["-I", str(REPO / ip)]
        cmd.append(str(tu))
        try:
            p = subprocess.run(cmd, capture_output=True, text=True,
                               errors="replace", timeout=120)
        except subprocess.TimeoutExpired:
            return (rel, False, "编译超时（120s）")
        if p.returncode == 0:
            return (rel, True, "")
        errs = [ln for ln in (p.stdout + p.stderr).splitlines() if "error:" in ln]
        return (rel, False, errs[0].strip() if errs else "编译失败（无 error 行）")


def check() -> int:
    cxx = compiler()
    if cxx is None:
        print("::error::R0 fail-closed：找不到 C++ 编译器（CXX / g++ / c++ / clang++）")
        return 1
    if not EXEMPT_FILE.exists():
        print(f"::error::R0 fail-closed：豁免名单缺失 {EXEMPT_FILE}")
        return 1

    headers = discover_headers()
    exempt = load_exempt()

    # ── 增量：命中缓存的头跳过编译 ────────────────────────────────────────
    fps = _closure_fingerprints()
    cache = _load_cache()
    cached = cache.get("entries", {}) if isinstance(cache, dict) else {}
    need = []
    reuse: dict[str, tuple[bool, str]] = {}
    for rel in headers:
        fp = fps.get(rel, "")
        ent = cached.get(rel)
        if ent and ent.get("fp") == fp and "ok" in ent:
            reuse[rel] = (bool(ent["ok"]), ent.get("err", ""))
        else:
            need.append(rel)

    results: list[tuple[str, bool, str]] = [
        (rel, ok, msg) for rel, (ok, msg) in reuse.items()
    ]
    workers = max(1, (os.cpu_count() or 2))
    if need:
        with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
            futs = {ex.submit(compile_one, cxx, h): h for h in need}
            for fut in concurrent.futures.as_completed(futs):
                results.append(fut.result())
    results.sort()

    # 写回缓存
    new_entries = dict(cached)
    for rel, ok, msg in results:
        new_entries[rel] = {"fp": fps.get(rel, ""), "ok": ok, "err": msg}
    _save_cache({"entries": new_entries})

    failed = {rel: msg for rel, ok, msg in results if not ok}

    # ── 豁免的「生效条件」──────────────────────────────────────────────────
    #   `fragment` ⇒ 无条件生效
    #   `stdlib:<能力>` ⇒ **仅当当前编译器的标准库不提供该能力**时生效
    # ⚠️ 这样「能力」而不是「编译器名字」成为判据 ⇒ ⇒ 可移植 ⇒ ⇒ 且换到
    #    提供该能力的编译器时豁免**自动失效** ⇒ ⇒ 该头若此时能编译 ⇒ ⇒
    #    R2a 报「豁免已过时」⇒ ⇒ 红（⇒ ⇒ 豁免不会变成万能借口）。
    stdlib_cache: dict[str, bool] = {}
    active_exempt: dict[str, str] = {}   # 实际生效的豁免 → 理由
    inactive_exempt: dict[str, str] = {}  # 因能力具备而**不生效**的豁免
    for rel, (kind, reason) in exempt.items():
        if kind.startswith("stdlib:"):
            cap = kind.split(":", 1)[1]
            if cap not in stdlib_cache:
                stdlib_cache[cap] = probe_stdlib(cxx, cap)
            if stdlib_cache[cap]:
                inactive_exempt[rel] = f"能力 <{cap}> 当前编译器**已提供**"
                continue
        active_exempt[rel] = reason
    for rel, why in inactive_exempt.items():
        print(f"::notice::豁免 [{rel}] 当前**不生效**（{why}）⇒ ⇒ 该头本次仍被检查")

    # ── R1：未豁免的头 MUST 能单独编译 ────────────────────────────────────
    r1_bad = sorted(set(failed) - set(active_exempt))
    if r1_bad:
        print(f"::error::R1 有 {len(r1_bad)} 个头**不能单独编译**（且未登记豁免）：")
        for rel in r1_bad:
            print(f"::error::   {rel} ⇒ {failed[rel]}")
        print("::error::   修法：补齐该头的 include（用 `include/` 相对路径）"
              "⇒ ⇒ 或确认它确实是「分片头」⇒ ⇒ 则登记进 "
              "scripts/lib/header_self_contained_exempt.txt（须带理由）")
    else:
        print(f"✅ R1 {len(headers)} 个头中，{len(headers) - len(failed)} 个能单独编译"
              f"（{len(active_exempt)} 个分片头按设计豁免）")

    # ── R2a：豁免 MUST 仍必要（已过时 ⇒ 红）────────────────────────────────
    stale = sorted(rel for rel in active_exempt if rel not in failed)
    if stale:
        print(f"::error::R2a 有 {len(stale)} 个**豁免已过时**（现在能单独编译了）：")
        for rel in stale:
            print(f"::error::   {rel} ⇒ MUST 从豁免名单移除")
        print("::error::   （豁免名单一旦变成「万能借口」，本护栏就失去判别力）")
    else:
        print(f"✅ R2a {len(active_exempt)} 条生效豁免全部仍然必要"
              f"（对应的头确实编译失败）")

    # ── R2b：豁免 MUST 与实际失败集合一致 ─────────────────────────────────
    unlisted = sorted(set(failed) - set(active_exempt))
    if unlisted:
        print(f"::error::R2b 有 {len(unlisted)} 个失败头未登记豁免：")
        for rel in unlisted:
            print(f"::error::   {rel} ⇒ {failed[rel]}")
    else:
        print("✅ R2b 豁免名单与实际失败集合完全一致")

    # ── R2c：豁免里的头 MUST 真实存在（名单项被删 ⇒ 红）────────────────────
    ghosts = sorted(rel for rel in exempt if rel not in set(headers))
    if ghosts:
        print(f"::error::R2c 豁免名单里有 {len(ghosts)} 个**已不存在的头**：")
        for rel in ghosts:
            print(f"::error::   {rel}")
    else:
        print("✅ R2c 豁免名单里的头全部存在")

    return 1 if (r1_bad or stale or unlisted or ghosts) else 0


# ── 投毒（自检用）───────────────────────────────────────────────────────────
def list_poisons() -> list[str]:
    return ["break-include", "drop-exemption", "add-exemption"]


def poison(name: str) -> int:
    if name == "break-include":
        target = INCLUDE_DIR / POISON_BREAK_INCLUDE
        if not target.exists():
            print(f"::error::投毒目标不存在：{target}")
            return 1
        src = target.read_text()
        if src.count(POISON_BREAK_OLD) != 1:
            print("::error::投毒锚点不唯一 ⇒ fail-closed")
            return 1
        target.write_text(src.replace(POISON_BREAK_OLD, POISON_BREAK_NEW))
        POISON_TARGET.write_text(str(target))
        print(f"已投毒：{target}（删掉一个必需 include ⇒ 该头不再自包含）")
        return 0
    if name in ("drop-exemption", "add-exemption"):
        if not EXEMPT_FILE.exists():
            print("::error::豁免名单缺失 ⇒ fail-closed")
            return 1
        shutil.copyfile(EXEMPT_FILE, SNAPSHOT)
        lines = EXEMPT_FILE.read_text().splitlines()
        data = [ln for ln in lines if ln.strip() and not ln.strip().startswith("#")]
        if name == "drop-exemption":
            if not data:
                print("::error::豁免名单为空 ⇒ fail-closed")
                return 1
            victim = data[0].split("\t")[0].strip()
            lines = [ln for ln in lines if ln.split("\t")[0].strip() != victim]
            print(f"已投毒：豁免名单删掉 {victim} ⇒ R2a 应转红")
        else:
            victim = "chlib/fifo.h"
            lines.append(f"{victim}\tfragment\t投毒：把一个能自包含的头登记为豁免 ⇒ R2a 应转红")
            print(f"已投毒：豁免名单加上 {victim} ⇒ R2a 应转红")
        EXEMPT_FILE.write_text("\n".join(lines) + "\n")
        POISON_TARGET.write_text(str(EXEMPT_FILE))
        return 0
    print(f"::error::未知投毒：{name}")
    return 1


def restore() -> int:
    """还原投毒（**两种机制各自独立** ⇒ ⇒ 任何一种都能单独还原）。"""
    if not POISON_TARGET.exists():
        return 1
    tgt = pathlib.Path(POISON_TARGET.read_text())
    if not tgt.exists():
        POISON_TARGET.unlink(missing_ok=True)
        return 1
    if tgt == EXEMPT_FILE:
        if not SNAPSHOT.exists():
            print("::error::豁免名单快照缺失 ⇒ 无法还原")
            return 1
        shutil.copyfile(SNAPSHOT, EXEMPT_FILE)
        SNAPSHOT.unlink(missing_ok=True)
    else:
        # break-include：把投毒标记换回原来的 include
        src = tgt.read_text()
        if POISON_BREAK_NEW not in src:
            print("::error::投毒标记不存在 ⇒ 无法还原")
            return 1
        tgt.write_text(src.replace(POISON_BREAK_NEW, POISON_BREAK_OLD))
    POISON_TARGET.unlink(missing_ok=True)
    return 0


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    cmd = sys.argv[1]
    if cmd == "check":
        return check()
    if cmd == "list-poisons":
        for n in list_poisons():
            print(n)
        return 0
    if cmd == "poison":
        if len(sys.argv) < 3:
            print("::error::poison 需要名字")
            return 2
        return poison(sys.argv[2])
    if cmd == "restore":
        return restore()
    print(f"::error::未知命令：{cmd}")
    return 2


if __name__ == "__main__":
    sys.exit(main())
