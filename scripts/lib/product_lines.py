#!/usr/bin/env python3
"""产品线元护栏的判据实现（ADR-078 · S4c-0）

**四条判据**（`scripts/verify_product_lines.sh` 是入口 + `--self-test`）：

- **C1** 发现集 == 登记集（marker 缺失 / 多出 / **生成物陈旧**）
- **C2** marker 完备性：
  - 必填字段齐（`line` / `layer` / `members` / `adrs`）
  - `layer` == `ip`（锚点语义：marker 放在该线的 IP 级目录）
  - `line` == 所在目录名 ⇒ ⇒ 目录改名 MUST 同步改 marker（改名不可能静默）
  - `members` 列的**每个目录 MUST 存在**
  - `adrs` 指向的 ADR 文件 MUST 存在（`docs/adr/ADR-NNN-*.md`）
  - `specs` 指向的 capability 目录 MUST 存在（`openspec/specs/<name>/`）
  - **归属完备性**：`examples/` 下每个「产品线成员目录」MUST 有归属
    （是某 marker 的 `members` 成员、或自己带 marker）
- **C3** 覆盖完备性（**「登记了但不覆盖」= 红**）：
  - 每条产品线 MUST 有 **≥ 1 个 ctest target**（`add_test(NAME <t>` 落在其成员目录下）
  - `examples/**` 下每个 ctest target MUST 能归属到**恰好一条**产品线
  ⇒ ⇒ 「新 target 忘了归线」/「新建产品线却零测试」⇒ **当场红**
- **C4** `--self-test` 投毒自证（在 `.sh` 里做）

**为什么这样定义**（见 `openspec/changes/rv32i-full-product-line-2026-11/design.md` §4/§6）：
纯自动发现会「静默不覆盖新线」；手写中央表会在多会话并行下冲突 ⇒ ⇒
marker（单点真相在各自目录）+ 本元护栏（发现集/归属/覆盖三重闭合）。

**零新依赖**：标准库 + `yaml`（PyYAML 已是仓内既有依赖）。
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover
    print("FATAL：缺 PyYAML（仓内既有依赖）", file=sys.stderr)
    sys.exit(2)

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
MARKER_NAME = ".rddf-line.yaml"
EXAMPLES_REL = "examples"
ADRS_REL = "docs/adr"
SPECS_REL = "openspec/specs"
GEN_SCRIPT_REL = "scripts/gen_product_lines.py"

REQUIRED_FIELDS = ("line", "layer", "members", "adrs")
EXCLUSIONS_REL = "examples/.non-product-lines"


def _load_exclusions() -> dict[str, str]:
    """非产品线的 example 组（`<dir> TAB <理由>`）。

    ⚠️ 这是「**非**产品线」的**显式否认**，不是产品线的登记面
    （产品线的唯一真相仍是各目录内的 marker）。
    """
    path = REPO_ROOT / EXCLUSIONS_REL
    out: dict[str, str] = {}
    if not path.is_file():
        return out
    for raw in path.read_text(encoding="utf-8").splitlines():
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        cells = raw.split("\t")
        if len(cells) < 2:
            continue
        out[cells[0].strip().rstrip("/")] = cells[1].strip()
    return out


def _rel(p: Path) -> str:
    return str(p.relative_to(REPO_ROOT))


def find_markers() -> list[Path]:
    examples = REPO_ROOT / EXAMPLES_REL
    if not examples.is_dir():
        return []
    return sorted(p for p in examples.glob(f"*/{MARKER_NAME}") if p.is_file())


def load_markers() -> list[tuple[Path, dict]]:
    out: list[tuple[Path, dict]] = []
    for marker in find_markers():
        with marker.open(encoding="utf-8") as fh:
            data = yaml.safe_load(fh)
        out.append((marker, data if isinstance(data, dict) else {}))
    return out


# ---------------------------------------------------------------------------
# C1：发现集 == 登记集
# ---------------------------------------------------------------------------
def check_c1() -> list[str]:
    """生成物陈旧 / marker 与登记面不一致 ⇒ 红（由生成器的 `--check` 判定）。"""
    gen = REPO_ROOT / GEN_SCRIPT_REL
    proc = subprocess.run(
        [sys.executable, str(gen), "--check"],
        capture_output=True,
        text=True,
        cwd=REPO_ROOT,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip().replace("\n", " / ")
        return [f"C1 发现集 != 登记集（生成物陈旧或 marker 与登记面不一致）：{detail}"]
    return []


# ---------------------------------------------------------------------------
# C2：marker 完备性 + 归属完备性
# ---------------------------------------------------------------------------
def _adr_exists(adr_id: str) -> bool:
    adrs = REPO_ROOT / ADRS_REL
    return any(adrs.glob(f"{adr_id}-*.md")) or (adrs / f"{adr_id}.md").is_file()


def _spec_exists(name: str) -> bool:
    """capability 存在 = 已归档进 `openspec/specs/` **或** 是某个 active change 的 delta。

    ⚠️ 后者是**必须**的：marker 可以引用本 change 正在新增的 capability
    （归档后才进 `openspec/specs/`）⇒ ⇒ 否则「变更进行中」时护栏恒红。
    """
    if (REPO_ROOT / SPECS_REL / name).is_dir():
        return True
    return any(REPO_ROOT.glob(f"openspec/changes/*/specs/{name}"))


def check_c2(markers: list[tuple[Path, dict]]) -> list[str]:
    problems: list[str] = []
    member_dirs: set[str] = set()

    for marker, data in markers:
        where = _rel(marker)
        for field in REQUIRED_FIELDS:
            if field not in data:
                problems.append(f"C2 {where}：缺必填字段 `{field}`")

        line = data.get("line")
        anchor_dir = marker.parent.name
        if isinstance(line, str) and line != anchor_dir:
            problems.append(
                f"C2 {where}：`line`({line}) != 所在目录名({anchor_dir})"
                f" ⇒ 目录改名 MUST 同步改 marker"
            )

        layer = data.get("layer")
        if layer is not None and layer != "ip":
            problems.append(
                f"C2 {where}：`layer`({layer}) MUST 是 `ip`（marker 放在该线的 IP 级目录 = 锚点）"
            )

        members = data.get("members") or []
        if not isinstance(members, list) or not members:
            problems.append(f"C2 {where}：`members` MUST 是非空列表")
        else:
            for m in members:
                rel = str(m).rstrip("/")
                member_dirs.add(rel)
                if not (REPO_ROOT / rel).is_dir():
                    problems.append(f"C2 {where}：`members` 里的目录不存在：{rel}")

        adrs = data.get("adrs") or []
        if not isinstance(adrs, list):
            problems.append(f"C2 {where}：`adrs` MUST 是列表")
        else:
            for adr in adrs:
                if not _adr_exists(str(adr)):
                    problems.append(f"C2 {where}：`adrs` 指向的 ADR 文件不存在：{adr}")

        specs = data.get("specs") or []
        if not isinstance(specs, list):
            problems.append(f"C2 {where}：`specs` MUST 是列表")
        else:
            for spec in specs:
                if not _spec_exists(str(spec)):
                    problems.append(f"C2 {where}：`specs` 指向的 capability 不存在：{spec}")

    # 归属完备性：`examples/*/` 的每个目录 MUST 是 (a) 某产品线的 member、
    # (b) 自带 marker、或 (c) 显式登记在**非产品线**排除清单里。
    #
    # ⚠️ **为什么需要 (c)**：仓内存在**与 RV32I 产品线无关**的 example 组
    #    （`examples/axi4` / `stream` / `spinalhdl-ported` / `verilator` —— 总线/组件级 demo）。
    #    若把「每个 examples 目录 MUST 属某产品线」当判据 ⇒ ⇒ 它们会**恒红**。
    #    而**去掉**该判据 ⇒ ⇒ 「新建产品线目录却忘登记」就无人发现（正是最恐惧的静默）。
    #    ⇒ ⇒ 折中 = **排除清单**（只登记「非产品线」· 增量极小）：判据保留判别力，
    #    且新增目录**若不属任何产品线、也没进排除清单** ⇒ ⇒ **红**。
    #
    # ⚠️ 这与 design.md 「无手写中央表」的关系：排除清单**不是**产品线的登记面
    #    （产品线的真相仍是 marker）；它是「**非**产品线」的**显式否认** ⇒ ⇒
    #    dated 订正见 tasks.md 的实施期发现。
    excluded = _load_exclusions()
    examples = REPO_ROOT / EXAMPLES_REL
    if examples.is_dir():
        for d in sorted(p for p in examples.iterdir() if p.is_dir()):
            rel = _rel(d)
            if (d / MARKER_NAME).is_file():
                continue
            if rel in member_dirs:
                continue
            if rel in excluded:
                continue
            problems.append(
                f"C2 {rel}：examples 下的目录既无 marker、也不是任何产品线的 `members`、"
                f"也不在非产品线排除清单（{EXCLUSIONS_REL}）⇒ 归属缺失"
            )
    return problems


# ---------------------------------------------------------------------------
# C3：覆盖完备性（每条线 ≥1 个 ctest target · 每个 target 归线唯一）
# ---------------------------------------------------------------------------
# ctest target 的**两种**声明形态（MUST 并集，否则 helper 注册的 target 全漏）：
#   ① 字面 `add_test(NAME <t>`；
#   ② **helper 形态** `<any_fn>(<t> <source.cpp> ...)` —— 仓内 `riscv_mini_catch_test(...)`
#      就是这样注册的（`examples/riscv-mini/CMakeLists.txt`）⇒ ⇒ 只认 ① 会漏掉
#      **整条 riscv-mini 线**的 ctest target（实测：mini 线被判「零 ctest target」）。
_ADD_TEST_RE = re.compile(r"add_test\s*\(\s*NAME\s+([A-Za-z0-9_]+)")
_ANY_FN_RE = re.compile(r"([A-Za-z0-9_]+)\s*\(\s*([A-Za-z0-9_]+)")


def _collect_add_tests() -> list[tuple[str, str]]:
    """返回 [(target, 声明所在目录相对路径)]。"""
    out: list[tuple[str, str]] = []
    examples = REPO_ROOT / EXAMPLES_REL
    if not examples.is_dir():
        return out
    for cmake in sorted(examples.rglob("CMakeLists.txt")):
        text = cmake.read_text(encoding="utf-8", errors="replace")
        # 剥行尾注释，避免注释里的示例被当成真声明
        text = "\n".join(re.sub(r"#.*$", "", ln) for ln in text.splitlines())
        for m in _ADD_TEST_RE.finditer(text):
            out.append((m.group(1), _rel(cmake.parent)))
        # helper 形态：仅当该行含 `.cpp` 源（⇒ 是「声明一个测试 target」而非普通调用）
        for line in text.splitlines():
            if ".cpp" not in line:
                continue
            m = _ANY_FN_RE.search(line)
            if m and m.group(2).endswith(("_test", "_demo", "_example", "_smoke")):
                out.append((m.group(2), _rel(cmake.parent)))
    return out


def _owner_lines(target_dir: str, members: dict[str, str]) -> list[str]:
    """该目录归属哪些产品线（目录 == member，或在 member 之下）。"""
    owners = []
    for member, line in members.items():
        if target_dir == member or target_dir.startswith(member + "/"):
            owners.append(line)
    return owners


def _is_excluded_dir(tdir: str, excluded: dict[str, str]) -> bool:
    return any(tdir == e or tdir.startswith(e + "/") for e in excluded)


def check_c3(markers: list[tuple[Path, dict]]) -> list[str]:
    problems: list[str] = []
    members: dict[str, str] = {}
    for _, data in markers:
        line = data.get("line")
        for m in data.get("members") or []:
            members[str(m).rstrip("/")] = str(line)

    per_line: dict[str, int] = {str(d.get("line")): 0 for _, d in markers}
    excluded = _load_exclusions()

    for target, tdir in _collect_add_tests():
        # ⚠️ 非产品线目录（排除清单）的 ctest target **不要求归线** ——
        #    仓内 `examples/axi4` / `stream` / `spinalhdl-ported` 的产品级 example
        #    与 RV32I 产品线无关（实测：要求归线会报 24 条恒红）。
        if _is_excluded_dir(tdir, excluded):
            continue
        owners = _owner_lines(tdir, members)
        if not owners:
            problems.append(
                f"C3 {tdir}：ctest target `{target}` 无法归属任何产品线"
                f" ⇒ 该目录 MUST 登记为某产品线的 `members`"
            )
        elif len(owners) > 1:
            problems.append(
                f"C3 {tdir}：ctest target `{target}` 归属**多条**产品线 {owners}"
                f" ⇒ 归属 MUST 唯一"
            )
        else:
            per_line[owners[0]] = per_line.get(owners[0], 0) + 1

    for line, count in sorted(per_line.items()):
        if count < 1:
            problems.append(
                f"C3 产品线 `{line}`：**零 ctest target** ⇒ 覆盖不完备"
                f"（「登记了但不覆盖」⇒ 新线的存在意义是进入护栏扫描面）"
            )
    return problems


def main() -> int:
    markers = load_markers()
    if not markers:
        print("FATAL：未发现任何 marker（`examples/*/.rddf-line.yaml`）", file=sys.stderr)
        return 2

    problems: list[str] = []
    problems += check_c1()
    problems += check_c2(markers)
    problems += check_c3(markers)

    if problems:
        for p in problems:
            print(f"VIOLATION {p}", file=sys.stderr)
        return 1

    lines = ", ".join(sorted(str(d.get("line")) for _, d in markers))
    print(f"OK：{len(markers)} 条产品线（{lines}）· C1/C2/C3 全绿")
    return 0


if __name__ == "__main__":
    sys.exit(main())
