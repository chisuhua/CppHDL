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

⚠️ **逐行判**（2026-11-23 · 用户裁决）
   -------------------------------
   本载体判**每个** `kind=soc` 单元的迁移态 ⇒ ⇒ **逐行**判自洽。

   ⚠️ **为什么改**：原实现写死「`kind=soc` 的行**恰好 1 条**」⇒ ⇒ 那是「仓里只有 1 个
   SoC 单元」的**过时实现假设**（不是判据语义）⇒ ⇒ S1 登记 `tb_riscv_soc`（第 2 条）后
   变成 fail-closed ⇒ ⇒ **覆盖面归零**（2 个单元**一个都没判**）⇒ ⇒ `--self-test` 同样崩溃。

   ⇒ ⇒ **防「删行缩面」不在本载体**：行数守恒由 `scripts/verify_examples_rtl_sim.sh` 的
   `REGISTERED_SOC` 锚点（SoC 桶行数）负责 ⇒ ⇒ 本载体**不另造**行数锚点（避免两处清单漂移）。

⚠️ **归档门在多单元下的语义**（`--require-closed`）
   ------------------------------------------
   归档门 = 「**已宣称闭口**（`generated=yes`）的 SoC 单元 MUST 迁移完整」⇒ ⇒
   `generated=no` 的单元**跳过**（并 `note` 列出）。

   ⚠️ **已知边界（如实登记）**：单行时代归档门会拦住「停在未迁态就归档」；逐行后该格
   **不再被机械拦**（`generated=no` 的单元被跳过）⇒ ⇒ 「不许停在未迁态就归档」这条退化为
   **文档纪律**（蓝图 §7 R13 的「检查时机」）⇒ ⇒ 而「**宣称已迁却没迁**」这一格**仍由主路径
   抓住**（态 B 但 `.verif` 缺失 = 回潮 ⇒ 红）。

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


def _judge_row(row: dict, gate: bool = False) -> list[str]:
    """判一行 SoC 单元的迁移态自洽。返回错误列表（**空 = 自洽**）。

    ⚠️ **主路径与归档门对 `generated=yes` 的行判据相同** ⇒ ⇒ 只有 `generated=no` 的行
    在归档门下被跳过（见文件头「归档门在多单元下的语义」）。
    """
    tgt = row["target"]
    gen = row["generated"].strip()
    verif = _verif_path(row)
    has_verif = verif.is_file()
    interp = row["interp_checks"].strip()
    minck = row["min_checks"].strip()
    prefix = "R13 归档门：" if gate else ""

    if gen == "no":
        if has_verif:
            return [f"{prefix}态 A（未迁）但验证源存在：{verif.relative_to(ROOT)} "
                    f"⇒ ⇒ 半迁态（generated=no 与实际不符）"]
        return []
    if gen == "yes":
        errs = []
        if not has_verif:
            errs.append(f"{prefix}态 B（已迁）但验证源缺失：{verif.relative_to(ROOT)} "
                        f"⇒ ⇒ **回潮**")
        if not interp.isdigit() or not minck.isdigit() or interp != minck:
            errs.append(f"{prefix}态 B（已迁）但两侧条数不等：interp_checks={interp} "
                        f"· min_checks={minck} ⇒ ⇒ 迁移不完整")
        return errs
    return [f"{prefix}{tgt} 的 generated 取值非法：'{gen}'（MUST 是 yes / no）"]


def check(require_closed: bool = False) -> int:
    if not TSV.is_file():
        raise FailClosed(f"缺清单 {TSV}")
    lines = TSV.read_text(encoding="utf-8").splitlines()
    cols = _columns(lines)
    missing = [c for c in REQUIRED_COLUMNS if c not in cols]
    if missing:
        raise FailClosed(f"`#COLUMNS` 缺列 {missing}")
    rows = _rows(lines, cols)

    # ⚠️ **2026-11-23（用户裁决）**：**逐行**判 ⇒ 不再要求「恰好 1 行」
    #    （「恰好 1 行」是过时实现假设 ⇒ ⇒ 覆盖面曾归零）。
    targets = [r for r in rows if r["kind"].strip() == "soc"]
    if not targets:
        raise FailClosed("清单里没有 `kind=soc` 的行 ⇒ 无待判单元 ⇒ fail-closed")

    if require_closed:
        judged = [r for r in targets if r["generated"].strip() == "yes"]
        skipped = [r for r in targets if r["generated"].strip() != "yes"]
        if not judged:
            raise FailClosed("归档门：`kind=soc` 的行里没有 `generated=yes` 的单元 "
                             "⇒ 无待判单元 ⇒ fail-closed")
        for r in skipped:
            note(f"归档门跳过（未迁）：{r['target']}（generated={r['generated'].strip()}）"
                 f" ⇒ ⇒ R13 对它未闭")
    else:
        judged = targets

    errors: list[str] = []
    for row in judged:
        errs = _judge_row(row, gate=require_closed)
        if errs:
            errors.extend(errs)
            for e in errs:
                fail(e)
            continue
        tgt = row["target"]
        if row["generated"].strip() == "yes":
            ok(f"态 B · 已迁（自洽）：{tgt} · generated=yes · "
               f"{_verif_path(row).relative_to(ROOT)} · 两侧条数 {row['interp_checks'].strip()}")
        else:
            ok(f"态 A · 未迁（自洽）：{tgt} · generated=no · 验证源不存在 "
               f"⇒ ⇒ R13 未闭（合法中间态）")

    if errors:
        return 1
    if require_closed:
        ok(f"R13 归档门：{len(judged)} 个已宣称闭口的 SoC 单元均已迁且迁移完整")
    else:
        note("『已闭』声明**不在本载体判定**（无可判载体）⇒ ⇒ 归档门用 `--require-closed`")
        note("⑥' 新鲜度由 scripts/verify_examples_rtl_sim.sh 负责（不在本载体重复判）")
    return 0


def self_test() -> int:
    """**5 类投毒**各自必红 + **2 条必绿** ⇒ 判别力自证。"""
    if not TSV.is_file():
        raise FailClosed(f"缺清单 {TSV}")
    orig = TSV.read_text(encoding="utf-8")
    fails = 0
    lines = orig.splitlines()
    cols = _columns(lines)
    soc_idx = [i for i, l in enumerate(lines)
               if not l.startswith("#") and "\tsoc\t" in l]
    if len(soc_idx) < 2:
        raise FailClosed(f"`kind=soc` 的行只有 {len(soc_idx)} 条 ⇒ "
                         f"逐行判的自测需要 ≥2 条 ⇒ fail-closed")
    i1, i2 = soc_idx[0], soc_idx[1]
    row1, row2 = lines[i1], lines[i2]

    def row_dict(row: str) -> dict:
        f = row.split("\t")
        return {c: f[cols.index(c)] for c in cols}

    def with_col(row: str, col: str, value: str) -> str:
        f = row.split("\t")
        f[cols.index(col)] = value
        return "\t".join(f)

    def as_migrated(row: str) -> str:
        """构造「已迁且两侧条数相等」的清单行（**不依赖**清单的当前值）。"""
        minck = row.split("\t")[cols.index("min_checks")]
        return with_col(with_col(row, "generated", "yes"), "interp_checks", minck)

    def write(text: str) -> None:
        TSV.write_text(text, encoding="utf-8")

    def mutate(repl: dict) -> None:
        out = list(lines)
        for i, v in repl.items():
            out[i] = v
        write("\n".join(out))

    created: list = []

    def expect_red(name: str, mutate_fn, require_closed: bool = False) -> None:
        nonlocal fails
        rc = 0
        try:
            mutate_fn()
            try:
                rc = check(require_closed=require_closed)
            except FailClosed:
                rc = 2      # fail-closed（exit 2）**也是红**
        finally:
            TSV.write_text(orig, encoding="utf-8")
        if rc == 0:
            print(f"{RED}❌ 自检 {name}：**仍绿** ⇒ ⇒ 该判据无判别力{RESET}")
            fails += 1
        else:
            ok(f"自检 {name}：转红 ✓")

    # ① 第二行宣称已迁但验证源缺失 ⇒ 回潮（**也证明逐行都判** —— 不是只看第一行）
    expect_red("投毒①（第二行宣称已迁但验证源缺失 ⇒ 回潮）",
               lambda: mutate({i2: with_col(row2, "generated", "yes")}))

    # ② 第一行宣称未迁但验证源存在 ⇒ 半迁态
    expect_red("投毒②（第一行宣称未迁但验证源存在 ⇒ 半迁态）",
               lambda: mutate({i1: with_col(row1, "generated", "no")}))

    # ③ 已迁但两侧条数不等 ⇒ 迁移不完整
    expect_red("投毒③（已迁但两侧条数不等 ⇒ 迁移不完整）",
               lambda: mutate({i1: with_col(row1, "interp_checks", "999")}))

    # ④ 归档门在「无已宣称闭口的单元」时必红
    expect_red("投毒④（归档门在无已迁单元时必红）",
               lambda: mutate({i1: with_col(row1, "generated", "no"),
                               i2: with_col(row2, "generated", "no")}),
               require_closed=True)

    # ⑤ **逐行判的判别力自证**：只破坏**第二行**的自洽性（造 `.verif` 而不改清单）
    #    ⇒ ⇒ 若实现只判第一行 ⇒ 此投毒**仍绿** ⇒ 判据力为零。
    vpath2 = _verif_path(row_dict(row2))
    had2 = vpath2.is_file()
    vbak2 = vpath2.read_text(encoding="utf-8") if had2 else None

    def poison_row2_verif() -> None:
        vpath2.parent.mkdir(parents=True, exist_ok=True)
        vpath2.write_text("// self-test poison\n", encoding="utf-8")

    try:
        expect_red("投毒⑤（第二行自洽性被破坏 ⇒ 证明逐行都判）", poison_row2_verif)
    finally:
        if had2:
            vpath2.write_text(vbak2, encoding="utf-8")
        elif vpath2.is_file():
            vpath2.unlink()

    # 必绿① = 主路径（逐行自洽 · 不改任何东西）
    rc = check()
    if rc != 0:
        print(f"{RED}❌ 自检 必绿①（主路径逐行自洽）：误红{RESET}")
        fails += 1
    else:
        ok("自检 必绿①（主路径逐行自洽）：仍绿 ✓")

    # 必绿② = 归档门在「**全部**单元都已迁且完整」时放行
    #   （构造 fixture ⇒ 不依赖清单的当前状态）
    v1 = _verif_path(row_dict(row1))
    v2 = _verif_path(row_dict(row2))
    try:
        mutate({i1: as_migrated(row1), i2: as_migrated(row2)})
        for p in (v1, v2):
            if not p.is_file():
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text("// self-test fixture\n", encoding="utf-8")
                created.append(p)
        rc = check(require_closed=True)
    finally:
        TSV.write_text(orig, encoding="utf-8")
        for p in created:
            if p.is_file():
                p.unlink()
    if rc != 0:
        print(f"{RED}❌ 自检 必绿②（归档门在已迁态应放行）：误红{RESET}")
        fails += 1
    else:
        ok("自检 必绿②（归档门在已迁态放行）：仍绿 ✓")

    print()
    if fails:
        print(f"{RED}❌ verify_verif_migration_state --self-test: {fails} 条未被抓住{RESET}")
        return 1
    print(f"{GREEN}✅ verify_verif_migration_state --self-test: PASS"
          f"（5 类投毒各自转红 · 2 条必绿仍绿 · 还原复绿）{RESET}")
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
