#!/usr/bin/env python3
"""guard_reachability.py —— 护栏可达性的**静态**判据（phase-6 R23-B · ①-a）。

⚠️ **为什么有这个**：本轮 8 条 CI 红里 3 条属「环境 / 权限前提不满足 ⇒ 必然红」
   ⇒ ⇒ 「前半段红」遮蔽后面全部 step ⇒ ⇒ 主 job 的护栏链在 5 连红期间**零执行**
   ⇒ ⇒ 「仓里有护栏」与「护栏在跑」是两件事。

⚠️ **本分析器只做「静态可判」的三条**（其余三类静态不可判 ⇒ 登记为不可达类型）：
   ① **引用存在**：`ci.yml` 里引用的每个脚本 MUST 真实存在
   ② **可执行位**：被引用且非「被 source」的脚本，其 **git index mode** MUST = 100755
   ③ **gate × 环境对齐**：step 的 `if:` 谓词 MUST 与该 job 的实际环境**不矛盾**
      （否则该 step **恒 skipped**）

⚠️ **排除集合 MUST 是「派生」的** —— 本分析器从 `ci.yml` 重新派生每个脚本的
   排除类型，并要求登记表**逐字一致** ⇒ ⇒ 任何**手写**的额外排除 ⇒ ⇒ 判红
   （否则「排除集合」就是一个恒绿出口）。

⚠️ **不依赖网络 / git 历史（HEAD~1）/ verilator** —— 否则它自己就是第一个不可达护栏。
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys

try:
    import yaml
except ImportError:  # pragma: no cover
    print("❌ 缺 PyYAML ⇒ ⇒ fail-closed（不得静默跳过）")
    sys.exit(2)

CMD_RE = re.compile(r"^(?:bash\s+|sh\s+)?((?:[\w./-]+/)?[\w./-]+\.sh)\b")
SOURCE_RE = re.compile(r"^\s*(?:source|\.)\s+([^\s;&|]+)", re.M)
ENV_EQ_RE = re.compile(r"env\.([A-Z0-9_]+)\s*(==|!=)\s*'([^']*)'")
EVENT_RE = re.compile(r"github\.event_name\s*==\s*'([^']*)'")
EVENT_NAME_RE = re.compile(r"github\.event_name\s*!=\s*'([^']*)'")


def die(msg: str) -> None:
    print(f"❌ {msg}")
    sys.exit(2)


def parse_ci(path: str) -> tuple[dict, dict]:
    try:
        with open(path, encoding="utf-8") as f:
            d = yaml.safe_load(f)
    except OSError as e:
        die(f"读不到 CI 配置 {path}：{e}")
    except yaml.YAMLError as e:
        die(f"CI 配置解析失败 {path}：{e}")
    if not isinstance(d, dict) or "jobs" not in d:
        die(f"{path} 里没有 jobs 段 ⇒ ⇒ fail-closed")
    return d.get("env") or {}, d["jobs"]


def referenced_scripts(workflow_env: dict, jobs: dict) -> tuple[dict, dict]:
    """⇒ (路径 → 调用点列表, 路径 → 逐调用点的 gate 判定)

    ⚠️ **逐调用点**（不是逐脚本）：同一脚本可能被多个 job 调用，而**每个调用点的
    gate 单独成立** ⇒ ⇒ 「这个脚本恒不执行」MUST 要求**所有**调用点都恒 skipped
    ⇒ ⇒ 否则会把「只在某个 job 里被 gate 掉」误判成「整条护栏不可达」。
    """
    refs: dict[str, list[str]] = {}
    gates: dict[str, list[tuple[str, str, dict]]] = {}
    for jname, job in jobs.items():
        jenv = {**workflow_env, **(job.get("env") or {})}
        jcond = job.get("if") or ""
        for step in job.get("steps") or []:
            run = step.get("run") or ""
            scond = step.get("if") or ""
            # GH Actions：job 级 `if` 与 step 级 `if` **同时**生效 ⇒ ⇒ 合并判定
            cond = f"{jcond} && {scond}" if (jcond and scond) else (scond or jcond)
            sname = step.get("name") or "(unnamed)"
            for raw in run.split("\n"):
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                m = CMD_RE.match(line)
                if not m:
                    continue
                path = m.group(1)
                refs.setdefault(path, []).append(f"{jname}:{sname}")
                gates.setdefault(path, []).append((f"{jname}:{sname}", cond, jenv))
    return refs, gates


def sourced_scripts(repo: str) -> set[str]:
    """被 `source` / `.` 引入的脚本（它们**不需**可执行位）。

    ⚠️ 识别 MUST **解析路径**，不能只比 basename：`source ../foo.sh` 的目标是
    `scripts/foo.sh`，而 `source lib/foo.sh` 的目标是 `scripts/lib/foo.sh`
    ⇒ ⇒ 只按名字收集会把「被 source 的」与「被执行的」混在一起
    ⇒ ⇒ 反向对照（poison ④）会误判 ⇒ ⇒ 判据 ② 误伤。
    """
    out: set[str] = set()
    repo_abs = os.path.abspath(repo)
    scan_dirs = [os.path.join(repo, "scripts"), repo]
    for base in scan_dirs:
        for root, dirs, files in os.walk(base):
            dirs[:] = [d for d in dirs if d not in (".git", "build", "third_party",
                                                    "node_modules", "obj_dir")]
            for fn in files:
                if not (fn.endswith(".sh") or fn.endswith(".py")):
                    continue
                fp = os.path.join(root, fn)
                try:
                    txt = open(fp, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                for m in SOURCE_RE.finditer(txt):
                    cand = m.group(1).strip("\"'")
                    if not cand.endswith(".sh"):
                        continue
                    # 仓内常见写法：`source "$REPO/scripts/lib/x.sh"` ⇒ ⇒ 把变量前缀
                    # 解析成仓库根（否则这类被 source 的库脚本会被漏判 ⇒ ⇒ 误伤）
                    for var in ("$REPO_ROOT", "${REPO_ROOT}", "$REPO", "${REPO}"):
                        if cand.startswith(var):
                            cand = repo + cand[len(var):]
                            break
                    # ① 相对**被 source 的那个文件**的目录
                    r1 = os.path.normpath(os.path.join(root, cand))
                    # ② 相对**仓库根**
                    r2 = os.path.normpath(os.path.join(repo, cand))
                    for r in (r1, r2):
                        if os.path.exists(r) and os.path.abspath(r).startswith(repo_abs):
                            out.add(os.path.relpath(r, repo))
    return out


def git_modes(repo: str, paths: list[str]) -> dict[str, str]:
    """git **index** 里的 mode（不是工作区 —— `core.fileMode=false` 时会假绿）。"""
    out: dict[str, str] = {}
    for p in paths:
        try:
            r = subprocess.run(["git", "ls-files", "-s", "--", p], cwd=repo,
                               capture_output=True, text=True, timeout=30)
        except (OSError, subprocess.SubprocessError):
            out[p] = "<git 不可用>"
            continue
        line = r.stdout.strip()
        if not line:
            out[p] = "<未入 git index>"
            continue
        out[p] = line.split()[0]
    return out


def classify_one(cond: str, jenv: dict) -> tuple[str, str]:
    """单个调用点的 gate 分类。"""
    if not cond:
        return "none", "无 gate ⇒ 总是执行"
    # ① env 谓词矛盾 ⇒ ⇒ 该调用点恒 skipped
    for m in ENV_EQ_RE.finditer(cond):
        name, op, lit = m.group(1), m.group(2), m.group(3)
        actual = str(jenv.get(name, ""))
        satisfied = (actual == lit) if op == "==" else (actual != lit)
        if not satisfied:
            return ("gate-env-never",
                    f"`if: {cond}` 与 env `{name}={actual}` 矛盾 ⇒ ⇒ 该调用点恒 skipped")
    # ② 事件型 gate ⇒ ⇒ 分类登记（不是缺陷）
    m = EVENT_RE.search(cond)
    if m:
        return "gate-event-only", f"仅 `{m.group(1)}` 事件可达"
    m = EVENT_NAME_RE.search(cond)
    if m:
        return "gate-event-only", f"排除 `{m.group(1)}` 事件"
    # ③ 分片型 gate ⇒ ⇒ 分类登记（部分分片可达 ⇒ 不是恒 skipped）
    if "matrix." in cond:
        return "gate-matrix", f"分片型 gate：`{cond}`"
    # ④ 无法静态判定的 gate ⇒ ⇒ 分类登记（**显式不可达类型**，不硬编）
    return "gate-unevaluated", f"静态不可判的 gate：`{cond}`"


def gate_type(path: str, per_caller: list[tuple[str, str, dict]]) -> tuple[str, str]:
    """脚本级排除类型 = **所有调用点**里「最可达」的那个。

    ⇒ ⇒ 只有**全部**调用点都恒 skipped ⇒ ⇒ 才是 `gate-env-never`（真缺陷）
    ⇒ ⇒ 只要有一个调用点无 gate ⇒ ⇒ 类型 = `none`（这个护栏总体是跑得到的）。
    """
    classes = [classify_one(cond, jenv) for _caller, cond, jenv in per_caller]
    if any(c[0] == "none" for c in classes):
        return "none", "至少一个调用点无 gate ⇒ 总是执行"
    if any(c[0] == "gate-event-only" for c in classes):
        ev = next(c[1] for c in classes if c[0] == "gate-event-only")
        return "gate-event-only", f"仅事件型可达（{ev}）"
    if any(c[0] == "gate-matrix" for c in classes):
        return "gate-matrix", "仅特定分片可达"
    if all(c[0] == "gate-env-never" for c in classes):
        return "gate-env-never", "**全部**调用点都恒 skipped ⇒ ⇒ 该护栏从不执行"
    return "gate-unevaluated", "静态不可判的 gate"


def derive(repo: str, ci_path: str) -> dict:
    workflow_env, jobs = parse_ci(ci_path)
    refs, gates = referenced_scripts(workflow_env, jobs)
    sourced = sourced_scripts(repo)
    modes = git_modes(repo, sorted(refs))
    rows = []
    for path in sorted(refs):
        callers = refs[path]
        gt, why = gate_type(path, gates.get(path, []))
        rows.append({
            "path": path,
            "callers": callers,
            "gate": gates.get(path, ""),
            "excl": gt,
            "why": why,
            "mode": modes.get(path, "<未入 git index>"),
            "exists": os.path.exists(os.path.join(repo, path)),
            "sourced": path in sourced,
        })
    return {"rows": rows, "workflow_env": workflow_env}


REGISTRY_HEADING = "## 护栏清单"


def registry_rows(path: str) -> dict[str, dict]:
    """登记表解析（**只**读 `## 护栏清单` 段的那张表）。

    ⚠️ MUST 只读该段：登记表里还有「判据口径」「排除类型词表」两张表
    ⇒ ⇒ 若扫全文 ⇒ ⇒ 会把它们的行当成护栏行 ⇒ ⇒ 双向 1:1 判据恒红。
    """
    if not os.path.exists(path):
        die(f"登记表不存在：{path} ⇒ ⇒ fail-closed")
    txt = open(path, encoding="utf-8").read()
    idx = txt.find(REGISTRY_HEADING)
    if idx < 0:
        die(f"登记表缺 `{REGISTRY_HEADING}` 段 ⇒ ⇒ fail-closed")
    body = txt[idx:]
    out: dict[str, dict] = {}
    for raw in body.split("\n"):
        line = raw.strip()
        if not line.startswith("|") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 4:
            continue
        key = cells[0].strip("`")
        if key in ("脚本", "") or set(key) <= {"-", " "}:
            continue
        out[key] = {"callers": cells[1], "excl": cells[2].strip("`"), "last": cells[3]}
    return out


def render(rows: list[dict]) -> str:
    lines = ["| 脚本 | 调用入口 | 排除类型 | 最近一次 executed+绿 |",
             "|---|---|---|---|"]
    for r in rows:
        lines.append(f"| `{r['path']}` | {', '.join(f'`{c}`' for c in r['callers'])} "
                     f"| `{r['excl']}` | — |")
    return "\n".join(lines)


def check(repo: str, ci_path: str, reg_path: str) -> int:
    d = derive(repo, ci_path)
    rows = d["rows"]
    fails = 0
    print(f"── 派生：被 CI 引用的脚本 = {len(rows)} 个（每次重算，不写死）")

    # ① 引用存在
    for r in rows:
        if not r["exists"]:
            print(f"❌ ① 引用存在：{r['path']} 被 ci.yml 引用但**文件不存在**")
            fails += 1

    # ② 可执行位（git index）
    for r in rows:
        if r["sourced"]:
            continue
        if r["mode"] != "100755":
            print(f"❌ ② 可执行位：{r['path']} 的 git index mode = {r['mode']}"
                  f"（MUST = 100755；被 source 的库文件除外）")
            fails += 1

    # ③ gate × 环境对齐（真缺陷 ⇒ ⇒ 恒 skipped）
    for r in rows:
        if r["excl"] == "gate-env-never":
            print(f"❌ ③ gate × 环境：{r['path']} ⇒ ⇒ {r['why']}")
            fails += 1

    # 登记表：双向 1:1 + 排除类型**逐字**一致（禁手写额外排除）
    try:
        reg = registry_rows(reg_path)
    except SystemExit:
        return 2
    derived = {r["path"]: r["excl"] for r in rows}
    for path, excl in sorted(derived.items()):
        if path not in reg:
            print(f"❌ 登记表缺行：{path}（派生排除类型 = `{excl}`）⇒ ⇒ 双向 1:1 不成立")
            fails += 1
        elif reg[path]["excl"] != excl:
            print(f"❌ 排除类型不匹配：{path} ⇒ ⇒ 登记 = `{reg[path]['excl']}`，"
                  f"派生 = `{excl}` ⇒ ⇒ 排除 MUST 是**派生**的（禁手写）")
            fails += 1
    for path in sorted(reg):
        if path not in derived:
            print(f"❌ 登记表多行：{path}（ci.yml 里已无此引用）⇒ ⇒ 棘轮**只许增**"
                  f"（减项需显式批准）")
            fails += 1

    if fails:
        print(f"❌ verify_guard_reachability: {fails} 处违规")
        return 1
    print(f"✅ 三条静态判据全绿（{len(rows)} 个被引用脚本）· 登记表双向 1:1 且排除类型逐字派生一致")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True)
    ap.add_argument("--ci", required=True)
    ap.add_argument("--registry")
    ap.add_argument("--render", action="store_true")
    args = ap.parse_args()
    d = derive(args.repo, args.ci)
    if args.render:
        print(render(d["rows"]))
        return 0
    if not args.registry:
        print("❌ 缺 --registry ⇒ ⇒ fail-closed")
        return 2
    return check(args.repo, args.ci, args.registry)


if __name__ == "__main__":
    sys.exit(main())
