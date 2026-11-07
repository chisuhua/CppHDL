# arch-residual-registry-2026-11 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给架构台账 §7「已知残留」装上**唯一真相登记表 + 派生护栏**，让「状态漂移」从**人工发现**变成**CI 可检出**。

**Architecture:** 登记表 `.rddf/checks/arch-residual-registry.md` 是**唯一真相**（9 列 × 15 行）⇒ 台账 §7 每行状态格 MUST 以「由 (状态, 日期, 后缀) **确定性生成**的规范句」开头（**逐字节前缀**比较，散文保留其后）⇒ 分析器 `scripts/lib/ledger_residual.py` 做**表格解析 + 12 行判据判定 + 3 个只读子命令** ⇒ 护栏 `scripts/verify_arch_ledger_residuals.sh` 是**薄壳**（参数解析 + 退出码透传）⇒ 探针 `scripts/probe_arch_ledger_residuals.sh` 改 **wrapper 转发**（★ 路径 MUST 存活 = 裁决请求单的 `evidence:` 指针）。

**Tech Stack:** bash 3.2+ · python3（**stdlib only**：`re` / `pathlib` / `sys`）· Markdown 表格 + YAML front-matter · Catch2 **不涉及**（护栏自检用**合成 fixture**，不碰 architect 域文件）

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `scripts/lib/ledger_residual.py` | 登记表 + 台账 §7 的解析、**规范句生成**、12 行判据判定、`check` / `render` / `check-normalization` / `selftest` 四个子命令（**唯一**判据实现 ⇒ shell 不做表解析） |
| `scripts/verify_arch_ledger_residuals.sh` | 护栏薄壳：参数（`--self-test` / `--render` / `--check-normalization` / `--root`）· 缺工具 ⇒ `exit 2` · 判定不一致 ⇒ `exit 1` |
| `.rddf/checks/arch-residual-registry.md` | **登记表（唯一真相）**：9 列 × **15 行** + front-matter（`evidence:`） |
| `scripts/probe_arch_ledger_residuals.sh` | 改 wrapper：转发到护栏的只读子命令；★ **路径 MUST 保留**（裁决请求单的 `evidence:` 指针） |
| `AGENTS.md` | **COMMANDS** 段登记新护栏（★ **MUST NOT** 动 AUTO 块 ⇒ 由 `verify_roadmap_fragments.sh` 守） |

### Tests

| File | Responsibility |
|---|---|
| （**合成 fixture**，由分析器在 `/tmp/opencode/**` 现场生成） | `scripts/lib/ledger_residual.py::selftest`：合成台账 + 合成登记表 ⇒ **11 条投毒** ⇒ 每条断言退出码（`1` 不一致 / `2` fail-closed）⇒ 每次投毒后**丢弃副本**⇒ ★ 真实文件零污染 |

---

## 判据 → 实现 → 投毒 的对应（实施时逐条对照，**MUST NOT** 漏项）

| 判据 | 内容 | 实现位置 | 投毒 |
|---|---|---|---|
| ① | 双向 + **多重性敏感 1:1**（键 = 台账+section+id） | `r1_bidirectional()` | #1 #2 #7 |
| ② | 已登记闭口项的**证据路径 MUST 存在**（archive 类含 `proposal.md`） | `r2_evidence()` | #4 |
| ②' | `未决` 行的**候选闭口证据已存在** ⇒ 红 | `r2prime_candidate()` | #9 |
| ③ | 台账状态格 MUST **以生成句开头**（逐字节） | `r3_sentence_prefix()` | #3 #5 |
| ④ | 结算 fail-closed（0 行 ⇒ `exit 2` · 每行打印判定 · 生成句在登记表内唯一） | `parse_ledger_rows()` / `r4_settlement()` | #6 |
| ⑤ | 退出码语义（`1` 不一致 / `2` fail-closed；★ **禁**继承探针的 `exit 0`） | `main()` | —（断言在 #6 #10） |
| ⑥ | 自检：合成 fixture + 11 投毒 + 还原逐字相同 + 工作树干净 | `selftest()` | 全部 |
| ⑦ | `--render`（打印期望规范句） | `cmd_render()` | — |
| ⑦' | `--check-normalization`（归一化「信息不丢」） | `cmd_check_normalization()` | — |
| ⑧ | 进 CI（★ **S3**，**本计划不做** ⇒ 护栏**不接 CI**） | — | — |
| ⑨ | 探针**路径 MUST 存活** | `scripts/probe_arch_ledger_residuals.sh` | — |
| ⑩ | 登记表**自身格式非法** ⇒ `exit 2` | `r10_registry_format()` | #10 #11 |

---

## Tasks

### Task 1: 分析器骨架 —— 路径解析 + 登记表/台账表解析 + **fail-closed**

**Files:**
- Create: `scripts/lib/ledger_residual.py`
- Test: 合成 fixture（Task 8 落地断言；本任务先跑 `--help` 与 `check` 的人工核对）

- [x] **Step 1: 写失败断言** —— 在计划外的临时脚本 `/tmp/opencode/lr_t1.py` 里断言「解析器能从合成登记表读出 9 列、从合成台账 §7 读出 N 行、§7 缺失时抛 fail-closed」：

```python
import importlib.util, pathlib, sys
spec = importlib.util.spec_from_file_location("lr", "/workspace/project/CppHDL/scripts/lib/ledger_residual.py")
lr = importlib.util.module_from_spec(spec); spec.loader.exec_module(lr)
reg = lr.parse_registry(pathlib.Path("/tmp/opencode/lr_fixture/registry.md"))
assert [r["id"] for r in reg] == ["1", "1b"], reg
led = lr.parse_ledger_rows(pathlib.Path("/tmp/opencode/lr_fixture/ledger.md"), "7")
assert len(led) == 2 and led[0]["id"] == "1"
try:
    lr.parse_ledger_rows(pathlib.Path("/tmp/opencode/lr_fixture/no_section.md"), "7")
    raise AssertionError("缺 §7 必须是 fail-closed 异常")
except lr.FailClosed:
    pass
print("T1 OK")
```

- [x] **Step 2: 跑 ⇒ 失败** —— `python3 /tmp/opencode/lr_t1.py` ⇒ 期望 `ModuleNotFoundError: No module named 'ledger_residual'`（文件还不存在）
- [x] **Step 3: 最小实现** —— 创建 `scripts/lib/ledger_residual.py`：

```python
#!/usr/bin/env python3
"""残留登记表 ↔ 台账 §7 的派生护栏**分析器**（唯一判据实现）。

判据来源：openspec/changes/arch-residual-registry-2026-11/specs/ledger-residual-registry/spec.md
（5 Requirement）+ .rddf/improvements/arch-residual-registry-2026-11.md（12 行判据 · 11 投毒）。

子命令：check / render / check-normalization / selftest
退出码：0 绿 · 1 不一致 · 2 fail-closed（缺工具 · 缺文件 · 解析 0 行 · 登记表格式非法）
"""
from __future__ import annotations
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
REGISTRY_REL = ".rddf/checks/arch-residual-registry.md"
LEDGER_REL = "docs/architecture/2026-11-02-hierarchical-rtl-output.md"
LEDGER_SECTION = "7"
COLUMNS = ["台账", "section", "id", "状态", "日期", "后缀", "闭口证据", "复核条件", "归属"]
STATES = {"已收口", "已裁决", "不做", "已知", "未决"}
TERMINAL = {"已收口", "已裁决", "不做", "已知"}
EVIDENCE_REQUIRED = {"已收口", "已裁决"}
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
ID_RE = re.compile(r"^\d+b?$")
LEDGER_ROW_RE = re.compile(r"^\|\s*(\d+b?)\s*\|(.*)$")


class FailClosed(Exception):
    """fail-closed：环境缺失 / 文件缺失 / 解析 0 行 / 登记表格式非法 ⇒ exit 2。"""


def _cells(line: str) -> list[str]:
    """把 `| a | b |` 切成 ['a','b']；裸 `|` 会多出元素（⇒ 列数不符 ⇒ 调用方判红）。"""
    body = line.strip()
    if body.startswith("|"):
        body = body[1:]
    if body.endswith("|"):
        body = body[:-1]
    return [c.strip() for c in body.split("|")]


def parse_registry(path: pathlib.Path) -> list[dict]:
    """解析登记表 ⇒ 9 列的 dict 列表；表头不匹配 / 0 行 ⇒ FailClosed。"""
    if not path.exists():
        raise FailClosed(f"登记表不存在：{path}")
    lines = path.read_text(encoding="utf-8").splitlines()
    header_idx = None
    for i, l in enumerate(lines):
        if _cells(l) == COLUMNS:
            header_idx = i
            break
    if header_idx is None:
        raise FailClosed(f"登记表表头不匹配（期望 {COLUMNS}）：{path}")
    rows: list[dict] = []
    for l in lines[header_idx + 1:]:
        if not l.strip().startswith("|"):
            break
        cells = _cells(l)
        if len(cells) != len(COLUMNS):
            raise FailClosed(f"登记表列数 ≠ {len(COLUMNS)}：{l.strip()[:120]}")
        if all(set(c) <= set("-: ") for c in cells):   # 分隔行
            continue
        rows.append(dict(zip(COLUMNS, cells)))
    if not rows:
        raise FailClosed(f"登记表 0 行：{path}")
    return rows


def parse_ledger_rows(path: pathlib.Path, section: str) -> list[dict]:
    """解析台账的 `## <section>.` 段里形如 `| <id> | 描述 | 状态格 |` 的行。

    ⇒ 只取 `#` 列标签形如 `1` / `1b` 的行（★ **不是**物理行号）。
    ⇒ 找不到该段 / 解析出 0 行 ⇒ FailClosed（★ 防「空集上恒真 ⇒ 全绿」）。
    """
    if not path.exists():
        raise FailClosed(f"台账不存在：{path}")
    text = path.read_text(encoding="utf-8")
    m = re.search(rf"^## {re.escape(section)}\..*?(?=^## |\Z)", text, re.MULTILINE | re.DOTALL)
    if not m:
        raise FailClosed(f"台账缺 ## {section}. 段：{path}")
    rows: list[dict] = []
    for line in m.group(0).splitlines():
        lm = LEDGER_ROW_RE.match(line)
        if not lm:
            continue
        cells = _cells(line)
        if len(cells) != 3:
            raise FailClosed(f"台账 §{section} 行列数 ≠ 3（★ 状态格 MUST NOT 含裸 `|`）：{line.strip()[:120]}")
        rows.append({"id": lm.group(1), "desc": cells[1], "status": cells[2]})
    if not rows:
        raise FailClosed(f"台账 §{section} 解析出 0 行：{path}")
    return rows


def main(argv: list[str]) -> int:
    if len(argv) < 2 or argv[1] in ("-h", "--help"):
        print(__doc__)
        return 0
    raise FailClosed(f"未实现的子命令：{argv[1]}")


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except FailClosed as e:
        print(f"FAIL-CLOSED: {e}", file=sys.stderr)
        sys.exit(2)
```

- [x] **Step 4: 跑 ⇒ 通过** —— 先造合成 fixture 再跑断言：

```bash
mkdir -p /tmp/opencode/lr_fixture
cat > /tmp/opencode/lr_fixture/registry.md <<'EOF'
| 台账 | section | id | 状态 | 日期 | 后缀 | 闭口证据 | 复核条件 | 归属 |
|---|---|---|---|---|---|---|---|---|
| docs/architecture/x.md | 7 | 1 | 已收口 | 2026-11-07 | 后缀一 | openspec/changes/archive/a | — | — |
| docs/architecture/x.md | 7 | 1b | 不做 | 2026-11-07 | 后缀二 | — | — | — |
EOF
cat > /tmp/opencode/lr_fixture/ledger.md <<'EOF'
## 7. 已知残留

| # | 残留 | 状态 / 归属 |
|---|---|---|
| 1 | 描述一 | ✅ 已收口（2026-11-07 · 后缀一）⇒ ⇒ 散文 |
| 1b | 描述二 | 不做 ⇒ 后缀二 |
EOF
printf '## 8. 其它\n\n无\n' > /tmp/opencode/lr_fixture/no_section.md
python3 /tmp/opencode/lr_t1.py
```
⇒ 期望 `T1 OK`（★ `no_section.md` 缺 §7 ⇒ **必须**抛 `FailClosed` ⇒ 这就是「0 行 / 缺段 ⇒ fail-closed」的第一道断言）

- [x] **Step 5: Defer commit** —— 按仓库约定，execute 阶段不逐任务 commit；所有变更将在 archive 阶段统一提交。

---

### Task 2: 规范句生成 + **前缀判定**（判据 ③）+ 生成句唯一（判据 ④c）

**Files:**
- Modify: `scripts/lib/ledger_residual.py`（加 `gen_sentence()` / `r3_sentence_prefix()` / `r10_registry_format()` 的一部分）
- Test: `/tmp/opencode/lr_t2.py`

- [x] **Step 1: 写失败断言**

```python
import importlib.util, pathlib
spec = importlib.util.spec_from_file_location("lr", "/workspace/project/CppHDL/scripts/lib/ledger_residual.py")
lr = importlib.util.module_from_spec(spec); spec.loader.exec_module(lr)
# 生成句模板（★ builder 草案 ⇒ architect 在 S2 复核）
assert lr.gen_sentence("已收口", "2026-11-07", "P8 公共 helper 归档") == "✅ 已收口（2026-11-07 · P8 公共 helper 归档）"
assert lr.gen_sentence("已裁决", "2026-11-07", "ADR-060 §2.1 第四行") == "✅ 已裁决（2026-11-07 · ADR-060 §2.1 第四行）"
assert lr.gen_sentence("不做", "2026-11-07", "每类型一份 module") == "不做 ⇒ 每类型一份 module"
assert lr.gen_sentence("已知", "2026-11-07", ">64bit 端口排除") == "⚠️ 已知 ⇒ >64bit 端口排除"
assert lr.gen_sentence("未决", "", "等 planner 立项") == "⚠️ 未决 ⇒ 等 planner 立项"
assert lr.gen_sentence("不做", "2026-11-07", "") == "不做"
# 前缀判定：逐字节
assert lr.sentence_matches("✅ 已收口（2026-11-07 · X）⇒ ⇒ 散文", lr.gen_sentence("已收口", "2026-11-07", "X"))
assert not lr.sentence_matches("✅ 已收口（2026-11-07 · X）", lr.gen_sentence("已收口", "2026-11-07", "Y"))
print("T2 OK")
```

- [x] **Step 2: 跑 ⇒ 失败** —— `python3 /tmp/opencode/lr_t2.py` ⇒ 期望 `AttributeError: module 'lr' has no attribute 'gen_sentence'`
- [x] **Step 3: 最小实现** —— 追加到 `scripts/lib/ledger_residual.py`：

```python
_STATE_PREFIX = {
    "已收口": "✅ 已收口",
    "已裁决": "✅ 已裁决",
    "不做": "不做",
    "已知": "⚠️ 已知",
    "未决": "⚠️ 未决",
}


def gen_sentence(state: str, date: str, suffix: str) -> str:
    """★ 规范句 = f(状态, 日期, 后缀) **确定性生成**（无加粗 · 无反引号 · 不含证据路径）。

    ⇒ 终态（已收口 / 已裁决）⇒ `✅ <状态>（<日期> · <后缀>）`
    ⇒ 非终态（不做 / 已知 / 未决）⇒ `<前缀> ⇒ <后缀>`（后缀为空 ⇒ 只留前缀）
    """
    prefix = _STATE_PREFIX[state]
    if state in ("已收口", "已裁决"):
        return f"{prefix}（{date} · {suffix}）"
    return f"{prefix} ⇒ {suffix}".rstrip()


def sentence_matches(status_cell: str, sentence: str) -> bool:
    """★ 逐字节前缀比较（★ MUST NOT 做任何归一化 ⇒ 大小写 / 全角空格 / 空格数都算差异）。"""
    return status_cell.startswith(sentence)
```

- [x] **Step 4: 跑 ⇒ 通过** —— `python3 /tmp/opencode/lr_t2.py` ⇒ 期望 `T2 OK`
- [x] **Step 5: Defer commit**

---

### Task 3: 双向 + 多重性敏感 1:1 核对（判据 ①）+ 结算打印（判据 ④b）

**Files:**
- Modify: `scripts/lib/ledger_residual.py`
- Test: `/tmp/opencode/lr_t3.py`

- [x] **Step 1: 写失败断言**

```python
import importlib.util, pathlib
spec = importlib.util.spec_from_file_location("lr", "/workspace/project/CppHDL/scripts/lib/ledger_residual.py")
lr = importlib.util.module_from_spec(spec); spec.loader.exec_module(lr)
reg = [
    {"台账": "docs/architecture/x.md", "section": "7", "id": "1", "状态": "已收口", "日期": "2026-11-07",
     "后缀": "S1", "闭口证据": "openspec/changes/archive/a", "复核条件": "—", "归属": "—"},
    {"台账": "docs/architecture/x.md", "section": "7", "id": "1b", "状态": "不做", "日期": "2026-11-07",
     "后缀": "S2", "闭口证据": "—", "复核条件": "—", "归属": "—"},
]
led = [
    {"id": "1", "desc": "d1", "status": "x"},
    {"id": "1b", "desc": "d2", "status": "y"},
]
assert lr.r1_bidirectional(reg, led) == []                     # 一致 ⇒ 无问题
bad = lr.r1_bidirectional(reg, [{"id": "1", "desc": "", "status": ""}, {"id": "1", "desc": "", "status": ""}])
assert any("1b" in m for m in bad), bad                        # 台账缺 1b ⇒ 报出
dup = lr.r1_bidirectional(reg, led + [{"id": "1", "desc": "", "status": ""}])
assert any("1" in m for m in dup), dup                        # ★ 计数守恒但多重性不符 ⇒ 必红
print("T3 OK")
```

- [x] **Step 2: 跑 ⇒ 失败** ⇒ 期望 `AttributeError: ... 'r1_bidirectional'`
- [x] **Step 3: 最小实现**

```python
def _key(row: dict) -> tuple[str, str, str]:
    return (row.get("台账", ""), row.get("section", ""), row.get("id", ""))


def r1_bidirectional(reg: list[dict], led: list[dict]) -> list[str]:
    """★ 双向 + **多重性敏感 1:1**（★ 堵「删一行 + 复制同 id 行」⇒ 计数守恒但语义换人）。

    ⇒ 登记表每个键恰好匹配 1 条台账行；台账每个 id 恰好匹配 1 条登记。
    """
    problems: list[str] = []
    reg_keys = [_key(r) for r in reg]
    led_ids = [r["id"] for r in led]
    for r in reg:
        k = _key(r)
        n = led_ids.count(k[2])
        if n == 0:
            problems.append(f"登记表有台账无：id={k[2]} section={k[1]}")
        elif n > 1:
            problems.append(f"台账中 id={k[2]} 出现 {n} 次（★ 1:1 多重性不符）")
    for lid in sorted(set(led_ids)):
        n = reg_keys.count((reg_keys[0][0] if reg_keys else "", "7", lid))
        if n == 0:
            problems.append(f"台账有登记表无：id={lid}")
        elif n > 1:
            problems.append(f"登记表中 id={lid} 出现 {n} 次（★ 1:1 多重性不符）")
    return problems
```

- [x] **Step 4: 跑 ⇒ 通过** ⇒ `python3 /tmp/opencode/lr_t3.py` ⇒ 期望 `T3 OK`
- [x] **Step 5: Defer commit**

---

### Task 4: 证据存在性（判据 ②）+ 候选闭口证据（判据 ②'）

**Files:**
- Modify: `scripts/lib/ledger_residual.py`
- Test: `/tmp/opencode/lr_t4.py`

- [x] **Step 1: 写失败断言**

```python
import importlib.util, pathlib
spec = importlib.util.spec_from_file_location("lr", "/workspace/project/CppHDL/scripts/lib/ledger_residual.py")
lr = importlib.util.module_from_spec(spec); spec.loader.exec_module(lr)
root = pathlib.Path("/workspace/project/CppHDL")
ok_row = {"id": "1", "状态": "已收口", "闭口证据": "docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md"}
assert lr.r2_evidence([ok_row], root) == []
missing = {"id": "5", "状态": "已收口", "闭口证据": "docs/adr/ADR-NOPE.md"}
assert any("ADR-NOPE" in m for m in lr.r2_evidence([missing], root))
arch = {"id": "x", "状态": "已收口", "闭口证据": "openspec/changes/archive/2026-11-07-hierarchical-corpus-regression-2026-11"}
assert any("proposal.md" in m for m in lr.r2_evidence([arch], root))   # archive 类 MUST 含 proposal.md
# 候选闭口证据：未决行登记了**已存在**的路径 ⇒ 红
cand = {"id": "9", "状态": "未决", "闭口证据": "docs/adr/ADR-008-single-clock-domain-constraint.md"}
assert lr.r2prime_candidate([cand], root) != []
print("T4 OK")
```

- [x] **Step 2: 跑 ⇒ 失败** ⇒ 期望 `AttributeError: ... 'r2_evidence'`
- [x] **Step 3: 最小实现**

```python
def _split_evidence(raw: str) -> list[str]:
    """★ 格式：`path1,path2`（半角逗号 · 无空格 · 无反引号 · 无空段 · 不重复）。"""
    raw = raw.strip()
    if not raw or raw == "—":
        return []
    return raw.split(",")


def r2_evidence(reg: list[dict], root: pathlib.Path) -> list[str]:
    """★ 登记了闭口证据的行 ⇒ 路径 MUST 存在；archive 类 MUST 含 `proposal.md`。"""
    problems: list[str] = []
    for r in reg:
        for p in _split_evidence(r.get("闭口证据", "")):
            if not (root / p).exists():
                problems.append(f"id={r['id']} 闭口证据不存在：{p}")
            elif "/archive/" in p and not (root / p / "proposal.md").exists():
                problems.append(f"id={r['id']} archive 类证据 MUST 含 proposal.md：{p}")
    return problems


def r2prime_candidate(reg: list[dict], root: pathlib.Path) -> list[str]:
    """★ `未决` 行登记的**候选闭口证据已存在** ⇒ 红（该行应转 `已收口`）。"""
    problems: list[str] = []
    for r in reg:
        if r.get("状态") != "未决":
            continue
        for p in _split_evidence(r.get("闭口证据", "")):
            if (root / p).exists():
                problems.append(f"id={r['id']} 登记为未决但候选闭口证据已存在（应转已收口）：{p}")
    return problems
```

- [x] **Step 4: 跑 ⇒ 通过** ⇒ `python3 /tmp/opencode/lr_t4.py` ⇒ 期望 `T4 OK`
- [x] **Step 5: Defer commit**

---

### Task 5: 登记表格式校验（判据 ⑩）+ `check` 子命令 + 退出码语义（判据 ⑤）

**Files:**
- Modify: `scripts/lib/ledger_residual.py`（加 `r10_registry_format()` / `cmd_check()` / `main()`）
- Test: `/tmp/opencode/lr_t5.py`

- [x] **Step 1: 写失败断言**

```python
import importlib.util, pathlib
spec = importlib.util.spec_from_file_location("lr", "/workspace/project/CppHDL/scripts/lib/ledger_residual.py")
lr = importlib.util.module_from_spec(spec); spec.loader.exec_module(lr)
good = {"台账": "d.md", "section": "7", "id": "1", "状态": "已收口", "日期": "2026-11-07",
        "后缀": "S", "闭口证据": "docs/adr/ADR-008-single-clock-domain-constraint.md",
        "复核条件": "—", "归属": "—"}
assert lr.r10_registry_format([good]) == []
bad_state = dict(good, 状态="已收囗")
assert any("状态" in m for m in lr.r10_registry_format([bad_state]))
bad_date = dict(good, 日期="2026/11/07")
assert any("日期" in m for m in lr.r10_registry_format([bad_date]))
no_suffix = dict(good, 后缀="")
assert any("后缀" in m for m in lr.r10_registry_format([no_suffix]))
bad_ev = dict(good, 闭口证据="a, b")
assert any("闭口证据" in m for m in lr.r10_registry_format([bad_ev]))
open_row = dict(good, 状态="未决", 日期="", 后缀="S", 闭口证据="—", 复核条件="", 归属="builder")
assert any("复核条件" in m for m in lr.r10_registry_format([open_row]))
print("T5 OK")
```

- [x] **Step 2: 跑 ⇒ 失败** ⇒ 期望 `AttributeError: ... 'r10_registry_format'`
- [x] **Step 3: 最小实现**

```python
def r10_registry_format(reg: list[dict]) -> list[str]:
    """★ 登记表**自身**格式非法 ⇒ fail-closed（调用方 ⇒ exit 2）。

    ⇒ 状态枚举外取值 / 日期非 YYYY-MM-DD（终态必填）/ 后缀缺失（终态）/ 闭口证据列格式违规 /
      id 重复 / `未决` 行缺复核条件。
    """
    problems: list[str] = []
    seen: set[str] = set()
    for r in reg:
        rid = r.get("id", "")
        if not ID_RE.match(rid):
            problems.append(f"id 非法：{rid!r}（★ 形如 1 / 1b）")
        if rid in seen:
            problems.append(f"id 重复：{rid}")
        seen.add(rid)
        state = r.get("状态", "")
        if state not in STATES:
            problems.append(f"id={rid} 状态非法：{state!r}（★ 枚举 {sorted(STATES)}）")
        date = r.get("日期", "")
        if state in TERMINAL:
            if not DATE_RE.match(date):
                problems.append(f"id={rid} 日期非法：{date!r}（★ YYYY-MM-DD）")
            if not r.get("后缀", "").strip():
                problems.append(f"id={rid} 终态行的后缀 MUST 非空（★ 否则生成句会碰撞）")
        elif date and not DATE_RE.match(date):
            problems.append(f"id={rid} 日期非法：{date!r}（★ YYYY-MM-DD 或空）")
        raw = r.get("闭口证据", "").strip()
        if raw and raw != "—":
            if " " in raw or "`" in raw or raw.endswith(",") or ",," in raw or raw.startswith(","):
                problems.append(f"id={rid} 闭口证据列格式违规：{raw!r}（★ 半角逗号分隔 · 无空格 · 无反引号 · 无空段）")
            paths = _split_evidence(raw)
            if len(paths) != len(set(paths)):
                problems.append(f"id={rid} 闭口证据重复：{raw!r}")
            if state in EVIDENCE_REQUIRED and not paths:
                problems.append(f"id={rid} 「{state}」态 MUST 登记闭口证据")
        if state == "未决" and not r.get("复核条件", "").strip():
            problems.append(f"id={rid} 「未决」行 MUST 登记复核条件")
    return problems
```

并把 `main()` 补成：

```python
def cmd_check(root: pathlib.Path) -> int:
    reg = parse_registry(root / REGISTRY_REL)
    led = parse_ledger_rows(root / LEDGER_REL, LEDGER_SECTION)
    fmt = r10_registry_format(reg)
    if fmt:
        for m in fmt:
            print(f"FAIL-CLOSED 登记表格式非法：{m}", file=sys.stderr)
        return 2
    problems = r1_bidirectional(reg, led)
    problems += r2_evidence(reg, root)
    problems += r2prime_candidate(reg, root)
    seen_sent: dict[str, str] = {}
    for r in reg:
        s = gen_sentence(r["状态"], r["日期"], r["后缀"])
        if s in seen_sent:
            problems.append(f"id={r['id']} 生成句与 id={seen_sent[s]} 相同（★ MUST 唯一）")
        seen_sent[s] = r["id"]
    by_id = {r["id"]: r for r in led}
    for r in reg:
        row = by_id.get(r["id"])
        if row is None:
            continue
        s = gen_sentence(r["状态"], r["日期"], r["后缀"])
        if not sentence_matches(row["status"], s):
            problems.append(f"id={r['id']} 台账状态格未以生成句开头：期望 {s!r}")
        print(f"  id={r['id']:<3} 状态={r['状态']:<4} 证据数={len(_split_evidence(r.get('闭口证据','')))} 命中={'OK' if row['status'].startswith(s) else 'MISS'}")
    if len(reg) != len(led):
        problems.append(f"结算行数不一致：登记表 {len(reg)} ≠ 台账 {len(led)}")
    if problems:
        for p in problems:
            print(f"FAIL {p}", file=sys.stderr)
        return 1
    print(f"PASS 残留登记一致性（{len(reg)} 行）")
    return 0
```

（`main()` 的分派：`check` ⇒ `cmd_check`；`render` / `check-normalization` / `selftest` 在 Task 6/7/8 落地 ⇒ 未实现时 `FailClosed` ⇒ `exit 2`）

- [x] **Step 4: 跑 ⇒ 通过** ⇒ `python3 /tmp/opencode/lr_t5.py` ⇒ 期望 `T5 OK`；再跑 `python3 scripts/lib/ledger_residual.py check --root /tmp/opencode/lr_fixture` ⇒ 期望能打印逐行判定
- [x] **Step 5: Defer commit**

---

### Task 6: `--render` 子命令（判据 ⑦）

**Files:**
- Modify: `scripts/lib/ledger_residual.py`
- Test: `/tmp/opencode/lr_t6.py`

- [x] **Step 1: 写失败断言**

```python
import importlib.util, pathlib, subprocess
spec = importlib.util.spec_from_file_location("lr", "/workspace/project/CppHDL/scripts/lib/ledger_residual.py")
lr = importlib.util.module_from_spec(spec); spec.loader.exec_module(lr)
lines = lr.cmd_render_rows([{"id": "1", "状态": "已收口", "日期": "2026-11-07", "后缀": "S"}])
assert lines == ["id=1 | ✅ 已收口（2026-11-07 · S）"], lines
print("T6 OK")
```

- [x] **Step 2: 跑 ⇒ 失败** ⇒ 期望 `AttributeError: ... 'cmd_render_rows'`
- [x] **Step 3: 最小实现**

```python
def cmd_render_rows(reg: list[dict]) -> list[str]:
    """★ 打印每行的**期望规范句**（★ 供 architect copy-paste ⇒ 降低「双份维护」成本）。"""
    return [f"id={r['id']} | {gen_sentence(r['状态'], r['日期'], r['后缀'])}" for r in reg]
```

- [x] **Step 4: 跑 ⇒ 通过** ⇒ `python3 /tmp/opencode/lr_t6.py` ⇒ 期望 `T6 OK`
- [x] **Step 5: Defer commit**

---

### Task 7: `--check-normalization` 子命令（判据 ⑦'）

**Files:**
- Modify: `scripts/lib/ledger_residual.py`
- Test: `/tmp/opencode/lr_t7.py`

- [x] **Step 1: 写失败断言**

```python
import importlib.util
spec = importlib.util.spec_from_file_location("lr", "/workspace/project/CppHDL/scripts/lib/ledger_residual.py")
lr = importlib.util.module_from_spec(spec); spec.loader.exec_module(lr)
old = "✅ 已收口（2026-11-07 · 旧）⇒ ⇒ 段一 ⇒ ⇒ 段二"
new = "✅ 已收口（2026-11-07 · 旧）⇒ ⇒ 段一"
removed = lr.removed_segments(old, new)
assert removed == ["段二"], removed            # ★ 切界 = 第一个 `⇒` 之后按 `⇒` 切段
kept = lr.removed_segments(old, old)
assert kept == [], kept
print("T7 OK")
```

- [x] **Step 2: 跑 ⇒ 失败** ⇒ 期望 `AttributeError: ... 'removed_segments'`
- [x] **Step 3: 最小实现**

```python
def _prose_segments(status_cell: str) -> list[str]:
    """★ 散文段 = 状态格中**第一个 `⇒` 之后**的部分，再按 `⇒` 切成段（★ 段 = 删除粒度）。"""
    idx = status_cell.find("⇒")
    if idx < 0:
        return []
    tail = status_cell[idx:]
    return [s.strip() for s in tail.split("⇒") if s.strip()]


def removed_segments(old_cell: str, new_cell: str) -> list[str]:
    """★ 旧散文里**没有**出现在新散文里的段 = 「被删除的散文」（★ 须逐条登记 ⇒ 否则归一化丢信息）。"""
    new_set = set(_prose_segments(new_cell))
    return [s for s in _prose_segments(old_cell) if s not in new_set]
```

- [x] **Step 4: 跑 ⇒ 通过** ⇒ `python3 /tmp/opencode/lr_t7.py` ⇒ 期望 `T7 OK`
- [x] **Step 5: Defer commit**

---

### Task 8: `--self-test` 合成 fixture + **11 条投毒**（判据 ⑥）

**Files:**
- Modify: `scripts/lib/ledger_residual.py`（加 `selftest()` + 合成 fixture 生成 + 投毒表）
- Test: `bash scripts/verify_arch_ledger_residuals.sh --self-test`（Task 9 落地后跑；本任务先跑分析器子命令）

- [x] **Step 1: 写失败断言** —— 在分析器里写自检骨架（★ **合成 fixture** ⇒ 不碰 `docs/architecture/**`）：

```python
POISONS = [
    # (编号, 描述, 投毒函数名, 期望退出码)
    ("1", "台账 §7 多一行（登记表无）", "p_ledger_extra_row", 1),
    ("2", "登记表多一行（台账无）", "p_registry_extra_row", 1),
    ("3", "台账生成句改 1 个字", "p_sentence_typo", 1),
    ("4", "行 11 的证据路径改错", "p_evidence_wrong", 1),
    ("5", "登记表状态标为未决而台账前缀是已收口", "p_state_mismatch", 1),
    ("6", "§7 标题被改名 ⇒ 解析 0 行", "p_section_renamed", 2),
    ("7", "台账删一行 + 复制同 id 行（计数守恒）", "p_duplicate_id", 1),
    ("8", "台账散文改 1 个字 ⇒ 必须仍绿", "p_prose_edit", 0),
    ("9", "未决行登记一个已存在的候选证据", "p_candidate_exists", 1),
    ("10", "登记表状态取枚举外值", "p_bad_state", 2),
    ("11", "未决行缺复核条件", "p_missing_review", 2),
]
```

- [x] **Step 2: 跑 ⇒ 失败** ⇒ `python3 scripts/lib/ledger_residual.py selftest` ⇒ 期望 `NotImplementedError: selftest 尚未实现`
- [x] **Step 3: 最小实现** —— 追加：

```python
def _synthetic_root(tmp: pathlib.Path) -> pathlib.Path:
    """★ 合成 fixture：合成台账 + 合成登记表（★ **不碰** docs/architecture/** 与真实登记表）。"""
    root = tmp / "root"
    (root / ".rddf" / "checks").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "architecture").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "adr").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "adr" / "ADR-008-single-clock-domain-constraint.md").write_text("# ADR-008\n", encoding="utf-8")
    (root / ".rddf" / "checks" / "arch-residual-registry.md").write_text(
        "| 台账 | section | id | 状态 | 日期 | 后缀 | 闭口证据 | 复核条件 | 归属 |\n"
        "|---|---|---|---|---|---|---|---|---|\n"
        f"| {LEDGER_REL} | 7 | 1 | 已收口 | 2026-11-07 | S1 | docs/adr/ADR-008-single-clock-domain-constraint.md | — | — |\n"
        f"| {LEDGER_REL} | 7 | 2 | 未决 |  | S2 | — | 等 planner 立项 | builder |\n",
        encoding="utf-8")
    (root / LEDGER_REL).write_text(
        "## 7. 已知残留\n\n"
        "| # | 残留 | 状态 / 归属 |\n|---|---|---|\n"
        "| 1 | 描述一 | ✅ 已收口（2026-11-07 · S1）⇒ ⇒ 散文段 |\n"
        "| 2 | 描述二 | ⚠️ 未决 ⇒ S2 |\n",
        encoding="utf-8")
    return root


def selftest() -> int:
    """★ 合成 fixture + 11 条投毒（★ 每条 MUST 让 check() 转红/保持绿 ⇒ 否则判据无判别力）。

    ⇒ 每次投毒在**副本**上做 ⇒ 用完丢弃 ⇒ ★ 真实文件零污染（★ 「自检后工作树干净」AC）。
    """
    import shutil
    import tempfile
    failures = 0
    with tempfile.TemporaryDirectory(prefix="lr-selftest-", dir="/tmp/opencode") as td:
        tmp = pathlib.Path(td)
        pristine = _synthetic_root(tmp / "pristine")
        before = {p: p.read_bytes() for p in pristine.rglob("*") if p.is_file()}
        for pid, desc, fn_name, expected in POISONS:
            work = tmp / f"case-{pid}"
            if work.exists():
                shutil.rmtree(work)
            shutil.copytree(pristine, work)
            globals()[fn_name](work)
            rc = cmd_check(work)
            ok = (rc == expected)
            print(f"  [{'OK ' if ok else 'BAD'}] 投毒 {pid}：{desc} ⇒ exit={rc}（期望 {expected}）")
            if not ok:
                failures += 1
            # 还原核对：pristine MUST 逐字未变（★ 合成 fixture 不受影响 ⇒ 工作树干净）
            for p, data in before.items():
                if not p.exists() or p.read_bytes() != data:
                    print(f"  [BAD] 投毒 {pid} 污染了 pristine fixture")
                    failures += 1
                    break
    print(f"自检：{len(POISONS)} 条投毒，失败 {failures}")
    return 1 if failures else 0
```

（11 个投毒函数各自只改**副本**里的一处：改台账行 / 改登记表行 / 改台账标题 / 改证据路径 / 改状态 / 复制 id 行 / 改散文（★ 期望**仍绿**）· 加未决行的候选证据 / 写枚举外值 / 清空复核条件）

- [x] **Step 4: 跑 ⇒ 通过** ⇒ `python3 scripts/lib/ledger_residual.py selftest` ⇒ 期望 **11 条全 OK** + `自检：11 条投毒，失败 0`
- [x] **Step 5: Defer commit**

---

### Task 9: 护栏薄壳 `scripts/verify_arch_ledger_residuals.sh`

**Files:**
- Create: `scripts/verify_arch_ledger_residuals.sh`
- Modify: `scripts/lib/ledger_residual.py`（`main()` 补齐子命令分派 + `--root` 参数）

- [x] **Step 1: 写失败断言** —— `bash scripts/verify_arch_ledger_residuals.sh` 在脚本不存在时 ⇒ `No such file or directory`（⇒ 期望失败）
- [x] **Step 2: 跑 ⇒ 失败** ⇒ `bash scripts/verify_arch_ledger_residuals.sh` ⇒ 期望 `No such file or directory`
- [x] **Step 3: 最小实现** —— 创建 `scripts/verify_arch_ledger_residuals.sh`：

```bash
#!/usr/bin/env bash
# scripts/verify_arch_ledger_residuals.sh
#
# 护栏：**架构台账「已知残留」的状态 MUST 有唯一真相**（登记表 ⇒ 派生 ⇒ 双向核对）
#
# 判据来源：openspec/changes/arch-residual-registry-2026-11/specs/ledger-residual-registry/spec.md
#          + .rddf/improvements/arch-residual-registry-2026-11.md（12 行判据 · 11 投毒）
#
# ★ 退出码语义（★ **禁**继承探针的恒 exit 0）：
#   0 = 通过 · 1 = 不一致（漂移 / 缺登记 / 证据不存在）· 2 = fail-closed（缺工具 · 缺文件 ·
#   解析 0 行 · 登记表格式非法）⇒ ⇒ ⇒ CI 红 ⇒ ⇒ 漂移可见
#
# ★ 本护栏**不接 CI**（⇒ S3 才接）：台账归一化（architect）落地前，判据 ③ 必然红
#   ⇒ 那是**预期状态**，不是缺陷 ⇒ ⇒ MUST NOT 因此回退归一化。
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANALYZER="$REPO_ROOT/scripts/lib/ledger_residual.py"
ROOT="$REPO_ROOT"
MODE="check"

while [ $# -gt 0 ]; do
    case "$1" in
        --self-test) MODE="selftest"; shift ;;
        --render) MODE="render"; shift ;;
        --check-normalization) MODE="check-normalization"; shift ;;
        --root) ROOT="$2"; shift 2 ;;
        -h|--help) echo "usage: $0 [--self-test|--render|--check-normalization] [--root DIR]"; exit 0 ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

if ! command -v python3 >/dev/null 2>&1; then
    echo "FAIL-CLOSED: 缺 python3 ⇒ 无法判定（★ fail-closed）" >&2
    exit 2
fi
if [ ! -f "$ANALYZER" ]; then
    echo "FAIL-CLOSED: 缺判据分析器 $ANALYZER" >&2
    exit 2
fi

python3 "$ANALYZER" "$MODE" --root "$ROOT"
exit $?
```

（`main()` 同步补：`--root` 解析 + `check` / `render` / `check-normalization` / `selftest` 四个子命令分派）

- [x] **Step 4: 跑 ⇒ 通过** ⇒

```bash
chmod +x scripts/verify_arch_ledger_residuals.sh
bash scripts/verify_arch_ledger_residuals.sh --self-test      # ⇒ 11 条全 OK
bash scripts/verify_arch_ledger_residuals.sh                   # ⇒ 真实登记表尚未建 ⇒ 此时期望 exit 2（缺文件 ⇒ fail-closed）
```
⇒ ★ **注意**：Task 10 建表之前，护栏对真实台账必然是 `exit 2`（登记表不存在）⇒ 这是**预期**的 fail-closed，不是缺陷

- [x] **Step 5: Defer commit**

---

### Task 10: 登记表首期 **15 行** + front-matter（含**生成句模板草案**）

**Files:**
- Create: `.rddf/checks/arch-residual-registry.md`
- Test: `bash scripts/verify_arch_ledger_residuals.sh`（⇒ 此时台账未归一化 ⇒ 判据 ③ 红 ⇒ **预期**）

- [x] **Step 1: 写失败断言** —— 先跑护栏 ⇒ 期望 `FAIL-CLOSED 登记表不存在`（exit 2）
- [x] **Step 2: 跑 ⇒ 失败** ⇒ `bash scripts/verify_arch_ledger_residuals.sh; echo $?` ⇒ 期望 `2`
- [x] **Step 3: 最小实现** —— 创建 `.rddf/checks/arch-residual-registry.md`：

```markdown
---
# ⚠️ 本文件是**活账**（★ 架构台账「已知残留」的**唯一真相**）⇒ ⇒ ⇒ 护栏
#    `scripts/verify_arch_ledger_residuals.sh` 对它做**双向 + 1:1**核对。
evidence: probe:scripts/probe_arch_ledger_residuals.sh
---

# 残留状态登记表（唯一真相）

> **权威**：本表是「哪些残留已闭口 / 未闭口 / 不做 / 已知」的**唯一登记处** ⇒ ⇒ ⇒
> 台账 §7 的状态格 MUST 以本表**确定性生成**的规范句开头 ⇒ ⇒ ⇒ 散文保留在规范句之后。

| 台账 | section | id | 状态 | 日期 | 后缀 | 闭口证据 | 复核条件 | 归属 |
|---|---|---|---|---|---|---|---|---|
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 1 | 已收口 | 2026-11-07 | P8 公共 helper 归档 | openspec/changes/archive/2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11 | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 1b | 已收口 | 2026-11-07 | 语料级回归归档 | openspec/changes/archive/2026-11-07-hierarchical-corpus-regression-2026-11 | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 2 | 已裁决 | 2026-11-07 | ADR-060 §2.1 第四行 | docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 3 | 已裁决 | 2026-11-07 | ADR-058 §2.1 v2.3 | docs/adr/ADR-058-hierarchical-rtl-output.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 4 | 已收口 | 2026-11-07 | T3-005 实施后收口 | .rddf/reviews/T3-005-equivalence-debt-review.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 5 | 已收口 | 2026-11-06 | 端口命名护栏 | scripts/verify_hierarchical_port_naming.sh | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 6 | 不做 | 2026-11-07 | 每类型一份 module | — | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 7 | 不做 | 2026-11-07 | 框架单时钟域 | docs/adr/ADR-008-single-clock-domain-constraint.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 8 | 不做 | 2026-11-07 | ADR-024 已评估 | docs/adr/ADR-024-blackbox-ip-integration-evaluation.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 9 | 已知 | 2026-11-07 | 等价性验证 MUST 排除 >64bit 端口 | docs/adr/ADR-035-verilator-backend.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 10 | 不做 | 2026-11-07 | 需真实综合工具 | — | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 14 | 已裁决 | 2026-11-07 | ADR-062 决策⑤ | docs/adr/ADR-062-hierarchical-unit-interface-typing.md | — | 下游 |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 12 | 已收口 | 2026-11-02 | C0-0 spike 提取器 | scripts/lib/verilog_tree_extract.py | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 13 | 已收口 | 2026-11-06 | 随 T3-005 关闭 | docs/adr/ADR-058-hierarchical-rtl-output.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 11 | 已收口 | 2026-11-07 | P6 + P8 双立项归档 | openspec/changes/archive/2026-11-07-p6-hierarchy-friendliness-diagnosis-2026-11,openspec/changes/archive/2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11 | — | — |
```

⚠️ **行 14 的状态 = `已裁决` 是 builder 的暂定值**（★ 该行 = 「⚠️ 已登记（ADR-062 决策⑤）⇒ 需下游同步」⇒ 决策已定 ⇒ 归 `已裁决`；「下游同步」作为**归属**记为 `下游`）⇒ ⇒ **待 architect 在 S2 裁定** ⇒ ⇒ 若裁定不同 ⇒ **只需改这一行**。

- [x] **Step 4: 跑 ⇒ 通过** ⇒

```bash
bash scripts/verify_arch_ledger_residuals.sh; echo "exit=$?"
```
⇒ 期望：**exit 1**（★ 台账尚未归一化 ⇒ 判据 ③ 报「未以生成句开头」⇒ ★ **这是预期状态**，不是缺陷 ⇒ 归一化由 architect 在 S2 做）

- [x] **Step 5: Defer commit**

---

### Task 11: 探针改 **wrapper 转发**（判据 ⑨）

**Files:**
- Modify: `scripts/probe_arch_ledger_residuals.sh`
- Test: `bash scripts/probe_arch_ledger_residuals.sh`（⇒ 输出含行 11；★ 路径不变 ⇒ 裁决请求单的 `evidence:` 指针不��）

- [x] **Step 1: 写失败断言** —— `grep -n "RESIDUAL_EVIDENCE" scripts/probe_arch_ledger_residuals.sh` 期望**无匹配**（★ 手工清单 MUST 被去掉 ⇒ 判定从登记表派生）
- [x] **Step 2: 跑 ⇒ 失败** ⇒ 现在**有**匹配（手工清单仍在）⇒ 期望失败
- [x] **Step 3: 最小实现** —— 把 `scripts/probe_arch_ledger_residuals.sh` 改成薄 wrapper：

```bash
#!/usr/bin/env bash
# scripts/probe_arch_ledger_residuals.sh
#
# ★ **本路径 MUST 保留**：它是 `.rddf/checks/decision-request-hierarchical-ledger-residuals-2026-11.md`
#   的 `evidence:` 指针 ⇒ ⇒ ⇒ 改名 ⇒ 证据链断 ⇒ ⇒ ⇒（`verify_ledger_evidence.sh` 会红）
#
# 现在：本脚本 = 护栏 `scripts/verify_arch_ledger_residuals.sh` 的**只读 wrapper**
# ⇒ ⇒ 「状态从哪来」的唯一真相 = 登记表 `.rddf/checks/arch-residual-registry.md`
# ⇒ ⇒ ⇒ 不再有「手工 RESIDUAL_EVIDENCE 清单」⇒ ⇒ ⇒ 那份清单曾与台账**互不校验** ⇒ ⇒ 漂移温床
set -uo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GUARD="$REPO_ROOT/scripts/verify_arch_ledger_residuals.sh"
if [ ! -f "$GUARD" ]; then
    echo "FAIL-CLOSED: 缺护栏 $GUARD" >&2
    exit 2
fi
exec bash "$GUARD" "$@"
```

- [x] **Step 4: 跑 ⇒ 通过** ⇒ `bash scripts/probe_arch_ledger_residuals.sh; echo "exit=$?"` ⇒ 期望 exit 1（★ 台账未归一化 ⇒ 同 Task 10 ⇒ **预期**）；且 `bash scripts/verify_ledger_evidence.sh` 仍 **PASS**（★ 路径未变 ⇒ 证据链不断）
- [x] **Step 5: Defer commit**

---

### Task 12: 文档同步 + 全量护栏复验（★ **不接 CI**）

**Files:**
- Modify: `AGENTS.md`（**COMMANDS** 段 ⇒ 登记新护栏；★ **MUST NOT** 动 AUTO 块）
- Test: 全量护栏复跑

- [x] **Step 1: 写失败断言** —— `grep -c "verify_arch_ledger_residuals" AGENTS.md` ⇒ 期望 `0`（尚未登记）
- [x] **Step 2: 跑 ⇒ 失败** ⇒ 期望 `0`
- [x] **Step 3: 最小实现** —— 在 `AGENTS.md` 的 COMMANDS 段追加（★ 位置：既有 `verify_hierarchical_rtl.sh` 之后）：

```bash
# 残留登记表护栏（架构台账 §7「已知残留」的状态 MUST 有唯一真相 ⇒ 台账是它的派生）
# ⚠️ 判据 ① 双向 + 1:1 · ② 证据存在 · ②' 候选闭口证据 · ③ 生成句逐字节前缀 · ④ fail-closed 结算
# ⚠️ ⑤ 退出码 1/2（禁恒 exit 0）· ⑥ 合成 fixture 自检（11 投毒）· ⑦ --render · ⑦' --check-normalization
# ⚠️ ★ 本护栏**尚未接 CI**（⇒ 台账归一化落地后才接 ⇒ 否则 CI 恒红）
bash scripts/verify_arch_ledger_residuals.sh
bash scripts/verify_arch_ledger_residuals.sh --self-test
```

- [x] **Step 4: 跑 ⇒ 通过** ⇒ 全量复验：

```bash
bash scripts/verify_arch_ledger_residuals.sh --self-test        # ⇒ 11 条全 OK
bash scripts/verify_ledger_evidence.sh                          # ⇒ PASS（探针路径未断）
bash scripts/verify_roadmap_fragments.sh                        # ⇒ PASS（★ 证明没碰 AUTO 块）
bash scripts/verify_hier_corpus_coverage.sh                     # ⇒ PASS
bash scripts/verify_hierarchical_port_naming.sh                 # ⇒ PASS
openspec validate arch-residual-registry-2026-11 --strict       # ⇒ valid
git status --porcelain docs/architecture .rddf/checks           # ⇒ ★ 空（自检零污染）
```
⇒ ★ **预期**：`verify_arch_ledger_residuals.sh`（非自检）对真实台账仍 **exit 1**（★ 台账未归一化 ⇒ S2 才有绿）

- [x] **Step 5: Defer commit** —— 按仓库约定，execute 阶段不逐任务 commit；所有变更将在 archive 阶段统一提交。

---

## 交接给 architect 的事项（★ S2 —— **已由 architect 完成（2026-11-07）**）

> ✅ **S2 已完成**：台账 §7 15 行归一化 + §7 前言去副本化 + 行 14 裁定 + 登记表日期/后缀/证据更正
> ⇒ ⇒ 审稿记录 = `.rddf/reviews/arch-residual-registry-normalization-2026-11.md`
> ⇒ ⇒ oracle 1 轮审稿：8 问全过 · 5 条建议全部采纳
> ⇒ ⇒ 护栏对真实台账 = **exit 0**（★ 归一化前是 exit 1）

| # | 事项 | 说明 |
|---|---|---|
| 1 | **生成句模板复核** | builder 草案：`✅ 已收口（<日期> · <后缀>）` / `✅ 已裁决（…）` / `不做 ⇒ <后缀>` / `⚠️ 已知 ⇒ <后缀>` / `⚠️ 未决 ⇒ <后缀>`（★ 无加粗 · 无反引号）⇒ architect 可改 ⇒ ⇒ 改了 ⇒ 登记表 15 行 + 台账同步改 |
| 2 | **台账 15 行归一化** | 用 `--render` 的输出落笔（★ 删除「因本 change 而变假」的散文 ⇒ **逐条**登记进 architect review 记录 ⇒ `--check-normalization` 据此核对） |
| 3 | **§7 前言注记去副本化** | 注记只列「同步了哪些行 id」+「状态以登记表为准」⇒ **MUST NOT** 逐字复述状态 |
| 4 | **行 14 归一化裁定** | builder 暂定 `已裁决` ⇒ architect 确认或改 ⇒ 改了 ⇒ 只改登记表那一行 |
| 5 | **CI 接线** | ★ **S3** ⇒ MUST 不早于 S2 合并 ⇒ 由 builder 执行 |

## 实施期发现（如实登记 —— 6 条）

| # | 发现 | 处置 | 依据 |
|---|---|---|---|
| 1 | 计划版 `r1_bidirectional` 的作用域用「登记表首行的台账 + 硬编码 `section=7`」⇒ **错**（多台账可扩展 ⇒ 登记表里可能同时有别的台账的行） | ✅ 改为**显式 `(ledger_path, section)` 作用域参数** ⇒ 判定只作用于本台账本 section | 机制「多台账可扩展」+ 键 = (台账, section, id) |
| 2 | 格式校验只把 `""` 当空值 ⇒ **漏了占位符 `—`** ⇒ ⇒ 「未决行缺复核条件」这条判据**恒绿** | ✅ 新增 `_is_empty()`（`""` 或 `—`）⇒ ★ 投毒 11 靠它才转红 | 投毒 11 首次跑出 exit 0（期望 2）⇒ 定位 |
| 3 | `gen_sentence` 用 `.rstrip()` ⇒ 空后缀时留下 `不做 ⇒`（尾随 ⇒） | ✅ 后缀为空时**只留前缀**（`不做`） | T2 断言首跑红 |
| 4 | 合成 fixture 的分隔行写成 `"|---|" + join` ⇒ **多一列** ⇒ 解析直接 fail-closed | ✅ 改为 `"|" + join + "|"`（9 列） | selftest 首跑 8 条全 exit 2 |
| 5 | `--check-normalization` 的接口在计划里只有一句「核对归一化是否丢信息」 | ✅ 落地为：旧台账取 `git show <base>:<台账路径>`（默认 `HEAD`）⇒ 逐行算**被删散文段** ⇒ `--review <file>` 给出时**逐段核登记**（未登记 ⇒ 红）⇒ 未给时只报告不判不合格 | AC19 + 「删除只允许『因本 change 而变假』的散文且必须逐条登记」 |
| 6 | **worktree 里 `verify_hierarchical_port_naming.sh` exit 2**（N0 fail-closed）⇒ 因裸 worktree 无 `build/tests/hier_rtl_products` | ⏸ 临时软链主仓 `build/` 后 PASS ⇒ **软链已移除（不入库）**⇒ ⇒ ★ 该护栏在裸 worktree 里**须先构建**（★ 与本次改动无关） | 环境事实登记 |

⇒ ★ **以上 6 条均已落在实现与断言里** ⇒ ⇒ ⇒ `scripts/lib/ledger_residual.py` 的 docstring 与
`scripts/verify_arch_ledger_residuals.sh` 的头部注释同步记录了口径（★ 无隐藏行为）。
