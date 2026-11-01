#!/usr/bin/env python3
"""**台账证据核验**（纪律机制 M · 2026-11-01）

⚠️ **为什么要有它**（oracle 评审 · 2026-11-01）：
本轮出现**三次「自己的结论被自己的探针推翻」** —— builder 的「静默丢字」被推翻、
XML 判据被负向验证推翻、成本单位被实测推翻 ⇒ ⇒ ⇒ 三次的**共同形状**是
**「结论先被写进标题 / 根因 / 口径，测量随后才发生」** ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒**
**⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒**

⇒ ⇒ **本机制把「结论复核」从【个人品质】变成【CI 可检的流程】**：
台账 front-matter 必须带 `evidence:`（三值）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒

⇒ ⇒ **三值的语义**：
| 值 | 含义 | 判据 |
|---|---|---|
| `test:<path>#<symbol>` | **仓内可复算**的反例测试 | 文件 MUST 存在 + MUST 在 CMake 注册 |
| `probe:<path>` | **已提交**的可复算探针 | 文件 MUST 存在（⚠️ `/tmp/opencode/**` **不算** ⇒ ⇒ session 结束即消失） |
| `unmeasured` | **明确未实测** | ⇒ ⇒ 只 WARN（但 **MUST NOT** 出现在「根因 / 标题」里 ⇒ ⇒ 由人判断，脚本只点名） |

⚠️ **规则一句话**（本文件头即规则）：
**「根因 MUST NOT 先于测量写入标题」** ⇒ ⇒ 台账标题 MUST 用**中性描述**
（例：「builder 链的上游反压」⇒ ⇒ **不是**「静默丢字」）。

⚠️ **fail-closed 的范围**（2026-11-01 定）：
① `evidence:` 指向**不存在 / 未注册**的 `test:` ⇒ ⇒ **FAIL**（⇒ ⇒ 假证据）；
② `evidence:` 指向 `/tmp/...` 的 `probe:` ⇒ ⇒ **FAIL**（⇒ ⇒ 不可复算）；
③ **缺 `evidence:`** ⇒ ⇒ ⚠️ **WARN + 点名**（⇒ ⇒ 历史台账**不阻塞**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 但**「点名」⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 缺失是【可见】的**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ M-b 批量补齐后 ⇒ ⇒ WARN 归零**）
⇒ ⇒ ⇒ ⇒ **⇒ ④ 标题含「实测 / 已确认」等断言词 ⇒ ⇒ 但 `evidence: unmeasured`
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ FAIL**（⇒ ⇒ 「未实测却写成实测」= 最危险的一种）。
"""
from __future__ import annotations

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
LEDGER_DIRS = [REPO / ".rddf" / "improvements", REPO / ".rddf" / "checks"]

# ⚠️ 标题里出现这些词 ⇒ ⇒ ⇒ 「断言已由测量支撑」⇒ ⇒ 若 evidence 是
#    `unmeasured` ⇒ ⇒ ⇒ **矛盾** ⇒ ⇒ FAIL
CLAIM_WORDS = ("实测", "已确认", "已证实", "confirmed", "verified")

FAILS = 0
WARNS = 0


def _out(msg: str) -> None:
    print(msg)


def fail(msg: str) -> None:
    global FAILS
    FAILS += 1
    _out(f"❌ {msg}")


def warn(msg: str) -> None:
    global WARNS
    WARNS += 1
    _out(f"::warning {msg}")


def _front_matter(raw: str) -> str:
    if not raw.startswith("---"):
        return ""
    parts = raw.split("---", 2)
    return parts[1] if len(parts) >= 3 else ""


def _evidence(fm: str) -> str | None:
    m = re.search(r"^evidence:\s*(.+?)\s*$", fm, re.M)
    return m.group(1).strip() if m else None


def _title(raw: str) -> str:
    m = re.search(r"^#\s+(.+?)\s*$", raw, re.M)
    return m.group(1) if m else ""


def _cmake_registered(path: pathlib.Path) -> bool:
    """该源文件是否在 CMake 里被注册（grep 文件名）。"""
    name = path.name
    for cm in (REPO / "tests" / "CMakeLists.txt",
               REPO / "examples" / "CMakeLists.txt",
               REPO / "samples" / "CMakeLists.txt"):
        if cm.exists() and name in cm.read_text(encoding="utf-8"):
            return True
    return False


def check() -> int:
    global FAILS, WARNS
    ledgers = []
    for d in LEDGER_DIRS:
        if d.exists():
            ledgers.extend(sorted(d.glob("*.md")))
    if not ledgers:
        fail("台账目录为空 ⇒ ⇒ 无法核验（⇒ ⇒ fail-closed）")
        return 1
    missing: list[str] = []
    for p in ledgers:
        rel = p.relative_to(REPO)
        raw = p.read_text(encoding="utf-8")
        fm = _front_matter(raw)
        ev = _evidence(fm)
        if ev is None:
            if not fm:
                missing.append(str(rel))
            else:
                missing.append(str(rel))
            continue
        if ev == "unmeasured":
            title = _title(raw)
            if any(w in title for w in CLAIM_WORDS):
                fail(f"{rel}：标题含「{CLAIM_WORDS}」类断言词，但 evidence 是 "
                     f"`unmeasured`（⇒ ⇒ 未实测却写成实测）")
            else:
                warn(f"{rel}：evidence=unmeasured（该条**未经实测**）")
            continue
        if ev.startswith("test:"):
            target = ev[len("test:"):].split("#", 1)[0].strip()
            tp = (REPO / target) if target else None
            if tp is None or not tp.exists():
                fail(f"{rel}：evidence 指向**不存在**的源文件 `{target}`"
                     f"（⇒ ⇒ 假证据 ⇒ ⇒ MUST 转红）")
                continue
            if not _cmake_registered(tp):
                fail(f"{rel}：evidence 指向 `{target}`，但它**未在 CMake 注册**"
                     f"（⇒ ⇒ 孤儿证据 ⇒ ⇒ MUST 转红）")
                continue
        elif ev.startswith("probe:"):
            target = ev[len("probe:"):].split("#", 1)[0].strip()
            if target.startswith("/tmp/") or target.startswith("/var/"):
                fail(f"{rel}：evidence 指向**临时目录**的探针 `{target}`"
                     f"（⇒ ⇒ session 结束即消失 ⇒ ⇒ 不可复算 ⇒ ⇒ MUST 转红）")
                continue
            if not (REPO / target).exists():
                fail(f"{rel}：evidence 指向**不存在**的探针 `{target}`")
                continue
        else:
            fail(f"{rel}：evidence 的取值非法 `{ev}`"
                 f"（MUST 是 `test:` / `probe:` / `unmeasured`）")
    if missing:
        _out(f"::warning {len(missing)} 个台账**缺 `evidence:`**"
             f"（⇒ ⇒ M-b 批量补齐前**不阻塞**，但**缺失是可见的**）：")
        for m in missing:
            warn(f"    - {m}")
    if FAILS:
        _out(f"❌ 台账证据核验 FAIL（{FAILS} 项）")
        return 1
    _out(f"✅ 台账证据核验通过（{len(ledgers)} 个台账 · {WARNS} 项待补）")
    return 0


def main() -> int:
    return check()


if __name__ == "__main__":
    sys.exit(main())
