#!/usr/bin/env python3
"""scripts/lib/verif_migration_state.py —— `.verif` 迁移态的**自洽性**判据（逻辑载体）

判据来源 = ADR-074 §7.6 的 **AC-R13′-1** + 蓝图 §7 **R13**（2026-11-20 用户追认：
闭口探针 MUST 做并挂 CI）。

⚠️ **为什么是「两态」而不是「必须已迁」**
   --------------------------------------------------
   R13 的闭合判据 = 「SoC 行 `generated=yes` + 同一份 `.verif` 驱动两侧 + ⑥' 新鲜度成立
   + ⑨ 等式成立」⇒ ⇒ 若探针**直接**判这四项 ⇒ ⇒ 在 SoC **尚未**迁移 `.verif` 之前
   **恒红** ⇒ ⇒ 挂进 CI = **CI 立刻红** ⇒ ⇒ 探针必须能表达「**还没迁**」这个合法中间态。

   ⇒ ⇒ 本载体判 **两态自洽**：

   | 态 | 判据 | 含义 |
   |---|---|---|
   | **A · 未迁** | `generated=no` ⇒ ⇒ 该单元的 `.verif` **MUST NOT 存在** | 「手写 TB」是合法中间态 |
   | **B · 已迁** | `generated=yes` ⇒ ⇒ `.verif` **MUST 存在** + `interp_checks == min_checks` | 迁移**完整**（无半迁态） |

   ⇒ ⇒ 两态都绿 ⇒ ⇒ **红**只发生在**不自洽**时：态 A 但 `.verif` 存在（半迁态）·
   态 B 但 `.verif` 缺失（**回潮**）· 态 B 但两侧条数不等（迁移不完整）。

⚠️ **「宣称已闭却未迁」这一格不可机械判** ⇒ ⇒ 「已闭」是**声明**，仓内当前没有可判的
   载体（蓝图 §7 **不被** `verify_arch_ledger_residuals.sh` 覆盖 —— 该护栏的台账路径硬编码
   为另一本蓝图）⇒ ⇒ 归**文档纪律**（蓝图 §7 R13 登记的「检查时机」）⇒ ⇒ 并提供
   **`--require-closed`** 作为**归档门**：归档 SoC 迁移 change 时跑一次 ⇒ ⇒ 那时**必须**
   已迁 ⇒ ⇒ 该格被抓住。

⚠️ **⑥' 新鲜度不在本载体重复判**（避免重复编译 + 双权威）⇒ ⇒ 新鲜度由
   `scripts/verify_examples_rtl_sim.sh` 的 **⑥'** 负责（它对 `generated=yes` 的行生效）。
"""
from __future__ import annotations

import argparse
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
TSV = ROOT / ".rddf/checks/examples-rtl-sim-targets.tsv"

REQUIRED_COLUMNS = [
    "target", "kind", "source", "component", "emitter", "emitter_args",
    "interp_checks", "tb", "status", "generated", "min_checks",
    "scenarios", "spec_checks", "note",
]

GREEN, RED, YELLOW, RESET = "\033[0;32m", "\033[0;31m", "\033[1;33m", "\033[0m"


def fail(msg: str) -> None:
    print(f"{RED}::error::{msg}{RESET}")


def ok(msg: str) -> None:
    print(f"{GREEN}✅{RESET} {msg}")


def note(msg: str) -> None:
    print(f"{YELLOW}::notice{RESET} {msg}")


class FailClosed(Exception):
    """fail-closed：环境缺失 / 清单格式非法 / 找不到待判行 ⇒ exit 2。"""


def _columns(lines: list[str]) -> list[str]:
    for ln in lines:
        if ln.startswith("#COLUMNS:"):
            return ln.split("#COLUMNS:", 1)[1].split()
    raise FailClosed("清单缺 `#COLUMNS:` 机器可读表头 ⇒ 无法推导列号")


def _rows(lines: list[str], cols: list[str]) -> list[dict]:
    idx = {c: i for i, c in enumerate(cols)}
    out = []
    for ln in lines:
        if ln.startswith("#") or not ln.strip():
            continue
        f = ln.split("\t")
        if len(f) != len(cols):
            raise FailClosed(f"清单行字段数 {len(f)} ≠ #COLUMNS 词数 {len(cols)}：{ln[:120]}")
        out.append({c: f[idx[c]] for c in cols})
    return out


def _verif_path(row: dict) -> pathlib.Path:
    """该行的 `.verif` 路径 = `source` 同目录 + 同 basename + `.verif`（约定）。"""
    return (ROOT / row["source"]).with_suffix(".verif")


def check(require_closed: bool = False) -> int:
    if not TSV.is_file():
        raise FailClosed(f"缺清单 {TSV}")
    lines = TSV.read_text(encoding="utf-8").splitlines()
    cols = _columns(lines)
    missing = [c for c in REQUIRED_COLUMNS if c not in cols]
    if missing:
        raise FailClosed(f"`#COLUMNS` 缺列 {missing}")
    rows = _rows(lines, cols)

    targets = [r for r in rows if r["kind"].strip() == "soc"]
    if not targets:
        raise FailClosed("清单里没有 `kind=soc` 的行 ⇒ 无待判单元 ⇒ fail-closed")
    if len(targets) != 1:
        raise FailClosed(f"清单里 `kind=soc` 的行有 {len(targets)} 条 ⇒ 口径歧义 ⇒ fail-closed")
    row = targets[0]

    tgt = row["target"]
    gen = row["generated"].strip()
    verif = _verif_path(row)
    has_verif = verif.is_file()
    interp = row["interp_checks"].strip()
    minck = row["min_checks"].strip()

    if require_closed:
        if gen != "yes":
            fail(f"R13 归档门：{tgt} 的 generated ≠ yes（实际 '{gen}'）⇒ ⇒ R13 未闭 ⇒ 不得归档")
            return 1
        if not has_verif:
            fail(f"R13 归档门：{tgt} 宣称已迁但验证源缺失：{verif.relative_to(ROOT)}")
            return 1
        if not interp.isdigit() or not minck.isdigit() or interp != minck:
            fail(f"R13 归档门：{tgt} 两侧条数不等（interp_checks={interp} · min_checks={minck}）")
            return 1
        ok(f"R13 归档门：{tgt} 已迁且迁移完整（{verif.relative_to(ROOT)} · 两侧条数 {interp}）")
        return 0

    if gen == "no":
        if has_verif:
            fail(f"态 A（未迁）但验证源存在：{verif.relative_to(ROOT)} ⇒ ⇒ 半迁态（generated=no 与实际不符）")
            return 1
        ok(f"态 A · 未迁（自洽）：{tgt} · generated=no · 验证源不存在 ⇒ ⇒ R13 未闭（合法中间态）")
        note("『已闭』声明**不在本载体判定**（无可判载体）⇒ ⇒ 归档门用 `--require-closed`")
        return 0
    if gen == "yes":
        if not has_verif:
            fail(f"态 B（已迁）但验证源缺失：{verif.relative_to(ROOT)} ⇒ ⇒ **回潮**")
            return 1
        if not interp.isdigit() or not minck.isdigit() or interp != minck:
            fail(f"态 B（已迁）但两侧条数不等：interp_checks={interp} · min_checks={minck} ⇒ ⇒ 迁移不完整")
            return 1
        ok(f"态 B · 已迁（自洽）：{tgt} · generated=yes · {verif.relative_to(ROOT)} · 两侧条数 {interp}")
        note("⑥' 新鲜度由 scripts/verify_examples_rtl_sim.sh 负责（不在本载体重复判）")
        return 0

    fail(f"{tgt} 的 generated 取值非法：'{gen}'（MUST 是 yes / no）")
    return 1


def self_test() -> int:
    """4 类投毒各自必红 + 2 条必绿 ⇒ 判别力自证。"""
    if not TSV.is_file():
        raise FailClosed(f"缺清单 {TSV}")
    orig = TSV.read_text(encoding="utf-8")
    fails = 0
    lines = orig.splitlines()
    cols = _columns(lines)
    soc_idx = next(i for i, l in enumerate(lines)
                    if not l.startswith("#") and "\tsoc\t" in l)
    row_soc = lines[soc_idx]

    def with_col(row: str, col: str, value: str) -> str:
        f = row.split("\t")
        f[cols.index(col)] = value
        return "\t".join(f)

    def write(text: str) -> None:
        TSV.write_text(text, encoding="utf-8")

    row_yes = with_col(row_soc, "generated", "yes")
    # 「完整迁移态」fixture = 已迁 + **两侧条数相等**（SoC 现状 interp=2 / min=14 ⇒ 不等
    #   ⇒ 归档门**正确地红** ⇒ ⇒ 必绿用例必须构造一个**已迁且完整**的清单行）
    row_full = with_col(row_yes, "interp_checks", row_soc.split("\t")[cols.index("min_checks")])

    def expect_red(name: str, mutate) -> None:
        nonlocal fails
        try:
            mutate()
            rc = check()
        finally:
            TSV.write_text(orig, encoding="utf-8")
        if rc == 0:
            print(f"{RED}❌ 自检 {name}：**仍绿** ⇒ ⇒ 该判据无判别力{RESET}")
            fails += 1
        else:
            ok(f"自检 {name}：转红 ✓")

    # ① 态 B 但 `.verif` 缺失 ⇒ 红（回潮）
    expect_red("投毒①（宣称已迁但验证源缺失）",
               lambda: write("\n".join(lines[:soc_idx] + [row_yes] + lines[soc_idx + 1:])))

    # ② 态 A 但 `.verif` 存在 ⇒ 红（半迁态）
    vpath = ROOT / "examples/axi4/tb_axi_soc_example.verif"
    had = vpath.is_file()
    vbak = vpath.read_text(encoding="utf-8") if had else None

    def poison_verif() -> None:
        write(orig)
        vpath.parent.mkdir(parents=True, exist_ok=True)
        vpath.write_text("// self-test poison\n", encoding="utf-8")

    try:
        expect_red("投毒②（宣称未迁但验证源存在）", poison_verif)
    finally:
        if had:
            vpath.write_text(vbak, encoding="utf-8")
        elif vpath.is_file():
            vpath.unlink()

    # ③ 态 B 但两侧条数不等 ⇒ 红（迁移不完整）
    def poison_counts() -> None:
        row = with_col(row_yes, "interp_checks", "999")
        write("\n".join(lines[:soc_idx] + [row] + lines[soc_idx + 1:]))

    expect_red("投毒③（已迁但两侧条数不等）", poison_counts)

    # ④ `--require-closed` 在未迁态 ⇒ 红（归档门拦住「宣称已闭却未迁」）
    try:
        rc = check(require_closed=True)
    finally:
        TSV.write_text(orig, encoding="utf-8")
    if rc == 0:
        print(f"{RED}❌ 自检 投毒④（归档门在未迁态仍放行）：无判别力{RESET}")
        fails += 1
    else:
        ok("自检 投毒④（归档门在未迁态必红）：转红 ✓")

    # 必绿 ×2
    rc = check()
    if rc != 0:
        print(f"{RED}❌ 自检 必绿①（未迁态自洽）：误红{RESET}")
        fails += 1
    else:
        ok("自检 必绿①（未迁态自洽）：仍绿 ✓")

    # 必绿② = **态 B 完整**（`.verif` 真实存在 + 两侧条数相等）⇒ ⇒ 归档门放行
    #   ⚠️ MUST 同时造出 `.verif` —— 只改 `generated=yes` 而 `.verif` 缺失 ⇒ ⇒
    #   归档门**正确地红**（那不是误红，是用例不完整）⇒ ⇒ 本用例必须造文件。
    try:
        write("\n".join(lines[:soc_idx] + [row_full] + lines[soc_idx + 1:]))
        vpath.parent.mkdir(parents=True, exist_ok=True)
        vpath.write_text("// self-test fixture\n", encoding="utf-8")
        rc = check(require_closed=True)
    finally:
        TSV.write_text(orig, encoding="utf-8")
        if had:
            vpath.write_text(vbak, encoding="utf-8")
        elif vpath.is_file():
            vpath.unlink()
    if rc != 0:
        print(f"{RED}❌ 自检 必绿②（归档门在已迁态应放行）：误红{RESET}")
        fails += 1
    else:
        ok("自检 必绿②（归档门在已迁态放行）：仍绿 ✓")

    print()
    if fails:
        print(f"{RED}❌ verify_verif_migration_state --self-test: {fails} 条未被抓住{RESET}")
        return 1
    print(f"{GREEN}✅ verify_verif_migration_state --self-test: PASS（4 类投毒各自转红 · 2 条必绿仍绿 · 还原复绿）{RESET}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--require-closed", action="store_true")
    args = ap.parse_args()
    try:
        if args.self_test:
            return self_test()
        return check(require_closed=args.require_closed)
    except FailClosed as exc:
        fail(str(exc))
        return 2


if __name__ == "__main__":
    sys.exit(main())
