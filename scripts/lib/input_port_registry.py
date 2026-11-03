#!/usr/bin/env python3
"""**必需端口登记表**（`.rddf/checks/input-port-required-registry.md`）的判据（D2-A）。

⚠️ **为什么要有它**：根 `AGENTS.md §ANTI-PATTERNS` 的「新增端口 MUST 显式驱动」只
覆盖**新增**端口 ⇒ ⇒ **存量**端口里「未驱动就静默失效」的那些**没有**护栏
⇒ ⇒ 而 2026-10-28 的 spike 已关闭「通用状态型护栏」路线（运行期无法识别端口身份）
⇒ ⇒ 剩下的可行路线 = **先分诊，再对「已知必需端口」立护栏**（本表 + 本判据）。

⚠️ **判据**：
   R1 **登记 ⊆ 实际端口**：每条登记的 (组件, 端口) MUST 由**该组件自己的类体**声明
      （`ch_in<...> <port>;` 落在 `class <组件> { ... }` 的花括号内）⇒ ⇒ 登记过时
      （组件改名 / 端口删除 / 端口挪到别的类）⇒ **红**。
      ⚠️ 「该头文件里出现过」**不算** —— 一个头里常有多个类（实测
      `rv32i_tcm.h` 里有 `DataTCM` / `InstrTCM` / `BootROM`）⇒ ⇒ 只按文件匹配会把
      兄弟类当成登记项（**误报**，实测踩过）。
   R2 **实际端口 ⊆ 登记**（**反向判据**）：凡在 `include/**` 里由某个类声明了
      风险类端口（永久复位 `ch_in<ch_bool> rst_n` / 写入放大 `ch_in<ch_uint<4>> strbe`）
      ⇒ ⇒ 那个类 MUST 在表里 ⇒ ⇒ 「新组件带必需端口却没登记」⇒ **红**。
   R3 **判别性用例存在**：每条登记 MUST 有判别性用例指针（文件存在 + 该文件里
      **实例化**了该组件）⇒ ⇒ 用例被删/改名 ⇒ **红**。
   R4 **写入放大类的驱动网**（file 级）：任何**实例化**了已登记组件的文件 MUST
      驱动那个端口 —— 「驱动」= `set_input_value(...<port>...)` **或**
      `io().<port> <<= ...`（内部驱动：wrapper 的合法形态，它**不是**「未驱动」）
      ⇒ ⇒ 未驱动 ⇒ **红**。通用豁免 `input_port_guard: intentionally-undriven <port>`
      （**窄**：文件名含 `guard`）⇒ 豁免**必须可见**（点名）。
   R5 **单一实现点**：「永久复位」类的 per-TEST-CASE 驱动检测**只**在
      `scripts/verify_rst_n_wiring.sh` 一处 ⇒ ⇒ 本判据**不重复实现**；
      且该脚本的组件清单 MUST **从本表读**（不硬编码 ⇒ 无双源漂移）。

用法：
    python3 scripts/lib/input_port_registry.py check
    python3 scripts/lib/input_port_registry.py rst-n-components     # 给 rst_n 护栏读
    python3 scripts/lib/input_port_registry.py self-test
"""
from __future__ import annotations

import pathlib
import re
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
REGISTRY = ROOT / ".rddf" / "checks" / "input-port-required-registry.md"
RST_GUARD = ROOT / "scripts" / "verify_rst_n_wiring.sh"

CLASS_RE = re.compile(r"^\s*(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)")

# 风险类 → 端口的声明正则
RISK_PORTS = {
    "永久复位": re.compile(r"\bch_in\s*<\s*ch_bool\s*>\s*rst_n\b"),
    "写入放大": re.compile(r"\bch_in\s*<[^;]*>\s*strbe\b"),
}
RISK_KINDS = set(RISK_PORTS)

# 「无判别性用例」的显式标记 ⇒ ⇒ 缺口**可见**，且**双向**（补上覆盖后表会因过时而红）
GAP_MARKER = "已知缺口"

GREEN = "\033[0;32m"
RED = "\033[0;31m"
YELLOW = "\033[0;33m"
RESET = "\033[0m"

FAILS = 0
NOTES: list[str] = []


def ok(msg: str) -> None:
    print(f"  {GREEN}✅ {msg}{RESET}")


def fail(msg: str) -> None:
    global FAILS
    FAILS += 1
    print(f"  {RED}❌ {msg}{RESET}")


def note(msg: str) -> None:
    NOTES.append(msg)


def parse_registry() -> list[dict]:
    """解析登记表的 4 列数据行。

    ⚠️ **不能**用「4 列 = 登记行」来判 —— 文件里还有别的 4 列表（已知缺口表）
    ⇒ ⇒ 用**第 3 列 MUST 是已知风险类**做判别（`RISK_KINDS`）。
    """
    rows: list[dict] = []
    for line in REGISTRY.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) != 4:
            continue
        comp_m = re.fullmatch(r"`([^`]+)`", cells[0])
        port_m = re.fullmatch(r"`([^`]+)`", cells[1])
        if not comp_m or not port_m:
            continue
        kind = cells[2].replace("*", "").strip()
        if kind not in RISK_KINDS:
            continue
        case = cells[3]
        if case.startswith("`") and case.endswith("`"):
            case = case[1:-1]
        rows.append({"component": comp_m.group(1), "port": port_m.group(1),
                     "kind": kind, "case": case})
    return rows


def port_owners(header: pathlib.Path, port_re: re.Pattern) -> list[tuple[int, str]]:
    """返回 [(行号, 拥有该声明的类名)]（用花括号配平取**最内层**类）。"""
    owners: list[tuple[int, str]] = []
    if not header.exists():
        return owners
    stack: list[tuple[int, str]] = []
    depth = 0
    for ln, text in enumerate(header.read_text(errors="replace").splitlines(), 1):
        m = CLASS_RE.match(text)
        if m:
            stack.append((depth, m.group(1)))
        for ch in text:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                while stack and stack[-1][0] >= depth:
                    stack.pop()
        if port_re.search(text) and stack:
            owners.append((ln, stack[-1][1]))
    return owners


def header_of(component: str) -> pathlib.Path | None:
    """组件定义所在的头（含该组件名且有 class/struct 声明）。"""
    for p in sorted((ROOT / "include").rglob("*.h")):
        text = p.read_text(errors="replace")
        if re.search(r"\b(?:class|struct)\s+" + re.escape(component) + r"\b", text):
            return p
    return None


def source_files() -> list[pathlib.Path]:
    out = []
    for d in ("tests", "examples", "samples"):
        base = ROOT / d
        if not base.is_dir():
            continue
        for p in sorted(base.rglob("*")):
            if p.suffix in {".h", ".cpp", ".cc", ".cxx", ".C"} and p.is_file():
                out.append(p)
    return out


def strip_comments(text: str) -> str:
    """剥注释（**保留字符串** ⇒ 文件名/路径字面量不影响判定）。

    ⚠️ MUST 剥：否则**注释里提到组件名**会被当成「实例化」（本仓已有先例：
    `test_rv32i_stage_dual.cpp` 只在注释里提 `DataTCM`）。
    """
    out: list[str] = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def instantiation_names(text: str, component: str) -> set[str]:
    """该文件里「代表 component」的全部名字 = 组件名 + 指向它的类型别名。

    ⚠️ **别名的存在是必需的**：仓内测试普遍写
        using Lite32 = axi4::AxiLiteSlave<32, 32, 4>;
        ch::ch_device<Lite32> dut;
    ⇒ ⇒ 只看 `ch_device<AxiLiteSlave` 会**全部漏判**（实测 12/13 条 R3 假红）。
    """
    names = {component}
    alias_re = re.compile(r"\busing\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*[^;]*?"
                          r"\b" + re.escape(component) + r"\b\s*<")
    for m in alias_re.finditer(text):
        names.add(m.group(1))
    return names


def instantiates(path: pathlib.Path, component: str) -> bool:
    """该文件是否**实例化**了 component（`ch_device<T>` / `ch_module<T>` / dual 宏）。

    ⚠️ 两个「非字面」形态必须覆盖，否则会**大批假红**（实测 12/13 条）：
      ① 模板实参里**含嵌套 `>`**（`AxiLiteSlave<32, 32, 4>`）⇒ 不能用 `[^>]*`；
      ② 仓内 dual 测试走 `DUAL_BACKEND_TEST_PAIR(DutType, ...)`，宏体内才是
         `ch::ch_device<DutType>` ⇒ ⇒ **测试文件里根本不出现 `ch_device`**。
    """
    text = strip_comments(path.read_text(errors="replace"))
    for nm in instantiation_names(text, component):
        q = r"(?:[A-Za-z_][A-Za-z0-9_]*::)*" + re.escape(nm) + r"\b"
        if re.search(r"\bch_(?:device|module)\s*<\s*" + q, text):
            return True
        # DUAL_BACKEND_TEST_PAIR(DutType, ...) / DUAL_BACKEND_TEST(DutType, ...)
        if re.search(r"\bDUAL_BACKEND_TEST(?:_PAIR)?\s*\(\s*" + q, text):
            return True
    return False


def r1(rows: list[dict]) -> None:
    for r in rows:
        hdr = header_of(r["component"])
        if hdr is None:
            fail(f"R1 组件「{r['component']}」在 include/ 下找不到类定义 ⇒ 登记过时")
            continue
        owners = [c for _, c in port_owners(hdr, re.compile(r"\bch_in\s*<[^;]*>\s*"
                                                          + re.escape(r["port"]) + r"\b"))]
        if not owners:
            fail(f"R1 端口「{r['component']}.{r['port']}」在 {hdr.relative_to(ROOT)} 里没有 ch_in 声明")
        elif r["component"] not in owners:
            fail(f"R1 端口「{r['port']}」在 {hdr.relative_to(ROOT)} 里由 "
                 f"{owners} 声明，**不是**「{r['component']}」⇒ 登记错位")
    ok(f"R1 检查完毕（{len(rows)} 条登记 ⊆ 实际端口）")


def r2(rows: list[dict]) -> None:
    registered = {(r["component"], r["port"]) for r in rows}
    for kind, port_re in RISK_PORTS.items():
        for p in sorted((ROOT / "include").rglob("*.h")):
            for ln, owner in port_owners(p, port_re):
                port = "rst_n" if kind == "永久复位" else "strbe"
                if (owner, port) not in registered:
                    fail(f"R2 组件「{owner}」（{p.relative_to(ROOT)}:{ln}）声明了{kind}类端口 "
                         f"{port}，但**未登记** ⇒ 新端口永不进表")
    ok("R2 检查完毕（实际端口 ⊆ 登记）")


def tb_drives_port(code: str, port: str) -> bool:
    """**测试台级**驱动（`set_input_value(...<port>...)`）。`code` 须已剥注释。"""
    return bool(re.search(r"set_input_value\s*\([^;]*?\b" + re.escape(port) + r"\b", code))


def mentions_component(code: str, component: str) -> bool:
    """`code`（已剥注释）里是否提到该组件（不要求经 `ch_device` 实例化）。

    ⚠️ 缺口判定 MUST 用它而不是 `instantiation_names()/instantiates()`：仓内存在
    **直接构造**形态（`AxiDma<32,32,4,16> dma(nullptr, "dma");`，实测
    `tests/test_axi_dma.cpp` 里 `ch_device`/`ch_module` 出现 **0** 次）⇒ ⇒ 用
    「实例化」判据会让「补上覆盖」这件事**永远看不见** ⇒ ⇒ 缺口行永久过时。
    """
    return bool(re.search(r"(?<![A-Za-z0-9_])" + re.escape(component) + r"(?![A-Za-z0-9_])", code))


def r3(rows: list[dict]) -> None:
    for r in rows:
        if GAP_MARKER in r["case"]:
            # 缺口行：断言「缺口**仍然**存在」⇒ ⇒ 一旦有人补上覆盖，表就过时 ⇒ 红
            #
            # 判据口径（两个条件都要，且都**保守**）：
            #   ① 该文件提到这个组件（**不**要求经 `ch_device` 实例化 —— 见
            #      `mentions_component()` 的注释）
            #   ② 该文件有**测试台级**驱动 `set_input_value(...<port>...)`
            #      ⇒ ⇒ wrapper 的 `io().<port> <<= ` **不**算关闭缺口（转发不等于
            #      「有行为覆盖」）⇒ ⇒ 宁可漏报「缺口已被补上」，不误报。
            still_open = [str(f.relative_to(ROOT)) for f in source_files()
                          if mentions_component(strip_comments(f.read_text(errors="replace")),
                                                r["component"])
                          and tb_drives_port(strip_comments(f.read_text(errors="replace")),
                                             r["port"])]
            if still_open:
                fail(f"R3 「{r['component']}.{r['port']}」已登记为缺口，但 {still_open} "
                     "已经在驱动它 ⇒ ⇒ **表已过时**（把它改成真实指针，并复核缺口段）")
            else:
                note(f"已知缺口：{r['component']}.{r['port']} **无行为覆盖**"
                     f"（⇒ 该端口的静默失效风险目前无判据兜底）")
            continue
        f = ROOT / r["case"]
        if not f.exists():
            fail(f"R3 判别性用例「{r['case']}」不存在（组件 {r['component']}）⇒ 判别力消失")
            continue
        if not instantiates(f, r["component"]):
            fail(f"R3 判别性用例「{r['case']}」里没有**实例化**「{r['component']}」"
                 "⇒ 判别力消失（只在注释里提到不算）")
    ok(f"R3 检查完毕（{len(rows)} 条：真实指针存在 ⇒ 实例化；缺口标记 ⇒ 缺口仍在）")


def drives_port(code: str, port: str) -> bool:
    """`code`（**已剥注释**）里是否驱动了 `port`。

    ⚠️ MUST 传**剥注释后**的文本 —— 注释里写 `set_input_value(...strbe...)`
    不是驱动（否则一条说明性注释就能让判据假绿）。
    """
    tb_pat = re.compile(r"set_input_value\s*\([^;]*?\b" + re.escape(port) + r"\b")
    inner_pat = re.compile(r"io\(\)\s*\.\s*" + re.escape(port) + r"\s*<<=")
    return bool(tb_pat.search(code) or inner_pat.search(code))


def has_exemption(raw_text: str, filename: str, port: str) -> bool:
    """`raw_text`（**未剥注释**）是否带豁免标记 ⇒ ⇒ 豁免 MUST 可见且窄。

    🔴 **MUST 传未剥注释的原文** —— 标记本身就写在注释里（`// input_port_guard: ...`）
    ⇒ ⇒ 拿 `strip_comments()` 的结果去匹配**恒不命中**（实测踩过：机制实现后
    从未生效，且**无任何用例**能发现 ⇒ 与 `fwft_fifo` 的「名字 over-promise」同型）。
    ⇒ ⇒ 本函数与 `drives_port()` 的**输入口径相反**，`selftest_exemption()` 钉住这一点。

    窄：文件名 MUST 含 `guard`（往普通测试文件塞标记**不豁免**）。
    """
    if "guard" not in filename:
        return False
    pat = re.compile(r"input_port_guard:\s*intentionally-undriven\s*" + re.escape(port))
    return bool(pat.search(raw_text))


def r4(rows: list[dict]) -> None:
    for r in rows:
        if r["kind"] != "写入放大":
            continue
        for f in source_files():
            if not instantiates(f, r["component"]):
                continue
            raw = f.read_text(errors="replace")
            if drives_port(strip_comments(raw), r["port"]):
                continue
            if has_exemption(raw, f.name, r["port"]):
                note(f"file={f.relative_to(ROOT)} 豁免"
                     f"（input_port_guard: intentionally-undriven {r['port']}）")
                continue
            fail(f"R4 文件 {f.relative_to(ROOT)} 实例化了「{r['component']}」但**未驱动** {r['port']}"
                 f"（未驱动的 {r['port']} 读回全 1 ⇒ 写路径按整字落盘 ⇒ 断言可能碰巧通过）")
    ok("R4 检查完毕（写入放大类的 file 级驱动网）")


def r5(rows: list[dict]) -> None:
    """R5 单一实现点：rst_n 护栏的组件清单 MUST 从本表读（不硬编码）。

    ⚠️ 判据不是「文本里有没有某句话」，而是**能不能观察到硬编码**：
      ① 脚本 MUST 提到登记表路径（读它）；
      ② 脚本里 MUST NOT 有「一行里出现 ≥ 3 个已登记组件名」的行 ——
         那正是硬编码清单的签名（`COMPONENTS="A B C ..."` 或 `grep -E "A|B|C"`）。
    ⇒ ⇒ ② 是**必需的**：只查 ① 的话，保留旧清单 + 顺手加一行注释提到登记表
    就能通过（⇒ 覆盖率静默缩面，正是本表要治的病）。
    """
    text = RST_GUARD.read_text(encoding="utf-8")
    names = [r["component"] for r in rows if r["port"] == "rst_n"]
    if "input-port-required-registry.md" not in text:
        fail("R5 rst_n 护栏没有引用登记表 ⇒ 组件清单必然硬编码 ⇒ 双源必然漂移"
             "（修法：让它从 .rddf/checks/input-port-required-registry.md 读）")
        return
    offenders = []
    for ln, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith("#"):
            continue
        hit = [n for n in names if re.search(r"(?<![A-Za-z0-9_])" + re.escape(n) + r"(?![A-Za-z0-9_])", line)]
        if len(hit) >= 3:
            offenders.append((ln, hit))
    if offenders:
        for ln, hit in offenders:
            fail(f"R5 rst_n 护栏第 {ln} 行仍硬编码 {len(hit)} 个组件名（{', '.join(hit[:4])}…）"
                 " ⇒ 与本表构成双源 ⇒ 必然漂移")
        return
    ok(f"R5 rst_n 护栏从本表读（{len(names)} 个组件，脚本内无硬编码清单）")


def selftest_exemption() -> int:
    """**纯函数级**自检：钉住「两个输入口径相反」这条最容易写反的地方。

    🔴 为什么需要它（而不是靠投毒）：投毒只能证明「判据会**红**」；而在这里
    写错的形态是「判据**永远绿**」（豁免机制失效 ⇒ 看起来正常工作）⇒ ⇒ 必须
    用**期望为真**的断言钉住，且它 MUST 在修复前**失败**（否则无判别力）。

    覆盖 7 条：
      ① `//` 注释里的豁免标记  ⇒ 命中（**修复前恒不命中** = 本自检的判别点）
      ② `/* */` 注释里的豁免标记 ⇒ 命中
      ③ 文件名不含 `guard` ⇒ **不**豁免（窄）
      ④ 注释里的 `set_input_value(...strbe...)` **不**算驱动
      ⑤ `io().strbe <<= ...`（内部驱动）算驱动
      ⑥ `mentions_component` 认得直接构造形态 `AxiDma<32,32,4,16>`
      ⑦ `tb_drives_port` **不**把 wrapper 的 `io().rst_n <<=` 当测试台驱动
    """
    global FAILS
    FAILS = 0
    cases: list[tuple[str, bool, bool]] = []

    raw_line = "// input_port_guard: intentionally-undriven strbe\nio.x = 1;\n"
    cases.append(("① 行注释里的豁免标记命中", has_exemption(raw_line, "foo_guard.cpp", "strbe"), True))

    raw_block = "/* input_port_guard: intentionally-undriven strbe */\nio.x = 1;\n"
    cases.append(("② 块注释里的豁免标记命中", has_exemption(raw_block, "foo_guard.cpp", "strbe"), True))

    cases.append(("③ 文件名不含 guard ⇒ 不豁免（窄）",
                  has_exemption(raw_line, "test_foo.cpp", "strbe"), False))

    commented_drive = "// sim.set_input_value(io.strbe, 0xF);\n"
    cases.append(("④ 注释里的驱动不算驱动",
                  drives_port(strip_comments(commented_drive), "strbe"), False))

    inner_drive = "sub.io().strbe <<= pipeline.io().data_strbe;\n"
    cases.append(("⑤ io().<port> <<= 算驱动",
                  drives_port(strip_comments(inner_drive), "strbe"), True))

    direct_ctor = "AxiDma<32, 32, 4, 16> dma(nullptr, \"dma\");\n"
    cases.append(("⑥ 直接构造形态被 mentions_component 认出",
                  mentions_component(strip_comments(direct_ctor), "AxiDma"), True))

    wrapper_fwd = "slave.io().rst_n <<= io().rst_n;\n"
    cases.append(("⑦ wrapper 转发不算测试台驱动",
                  tb_drives_port(strip_comments(wrapper_fwd), "rst_n"), False))

    for name, got, want in cases:
        if got == want:
            ok(f"自检 {name}")
        else:
            fail(f"自检 {name} —— 实测 {got}，应为 {want}")
    if FAILS:
        print(f"{RED}❌ 豁免机制自检 FAIL（{FAILS} 项）{RESET}")
        return 1
    print(f"{GREEN}✅ 豁免机制自检通过（{len(cases)} 条）{RESET}")
    return 0


def check() -> int:
    global FAILS
    FAILS = 0
    NOTES.clear()
    if not REGISTRY.exists():
        fail(f"登记表不存在：{REGISTRY.relative_to(ROOT)} ⇒ fail-closed")
        return 1
    rows = parse_registry()
    if not rows:
        fail("登记表里没有可解析的行 ⇒ fail-closed")
        return 1
    r1(rows)
    r2(rows)
    r3(rows)
    r4(rows)
    r5(rows)
    for n in NOTES:
        print(f"  {YELLOW}::notice {n}{RESET}")
    if FAILS:
        print(f"{RED}❌ 必需端口登记表核验 FAIL（{FAILS} 项）{RESET}")
        return 1
    print(f"{GREEN}✅ 必需端口登记表核验通过（{len(rows)} 条登记）{RESET}")
    return 0


def rst_n_components() -> int:
    """给 `verify_rst_n_wiring.sh` 读：永久复位类的组件名（空格分隔）。"""
    rows = parse_registry()
    names = [r["component"] for r in rows if r["kind"] == "永久复位" and r["port"] == "rst_n"]
    print(" ".join(sorted(set(names))))
    return 0


# ────────────────────────────────────────────────────────────────────────────
# 自检用的原地投毒（每条 MUST 让 check() 转红；还原后 MUST 复绿）
# ────────────────────────────────────────────────────────────────────────────
SNAPSHOT_DIR = pathlib.Path("/tmp/opencode/input_port_registry.snapshot")
POISON_TARGET = pathlib.Path("/tmp/opencode/input_port_registry.target")


def _rows_index(lines: list[str], component: str, port: str) -> int:
    for i, line in enumerate(lines):
        cells = [c.strip() for c in line.strip().strip("|").split("|")] if line.startswith("|") else []
        if len(cells) != 4:
            continue
        if cells[0] == f"`{component}`" and cells[1] == f"`{port}`":
            return i
    raise AssertionError(f"登记表里找不到 {component}.{port}")


def _poison_stale_port() -> None:
    """把一条登记的端口名改错 ⇒ R1（端口不存在）+ R2（组件未登记）必红。"""
    lines = REGISTRY.read_text(encoding="utf-8").splitlines(keepends=True)
    i = _rows_index(lines, "AxiLiteSlave", "rst_n")
    lines[i] = lines[i].replace("`rst_n`", "`rst_n_typo`", 1)
    REGISTRY.write_text("".join(lines), encoding="utf-8")


def _poison_drop_row() -> None:
    """删掉一条 rst_n 登记 ⇒ R2（实际端口 ⊆ 登记）必红。"""
    lines = REGISTRY.read_text(encoding="utf-8").splitlines(keepends=True)
    i = _rows_index(lines, "AxiLiteGpio", "rst_n")
    del lines[i]
    REGISTRY.write_text("".join(lines), encoding="utf-8")


def _poison_wrong_case() -> None:
    """把判别性用例指向一个**不实例化**该组件的文件 ⇒ R3 必红。"""
    lines = REGISTRY.read_text(encoding="utf-8").splitlines(keepends=True)
    i = _rows_index(lines, "DataTCM", "strbe")
    cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
    cells[3] = "`tests/test_axi_dma.cpp`"
    lines[i] = "| " + " | ".join(cells) + " |\n"
    REGISTRY.write_text("".join(lines), encoding="utf-8")


def _poison_fake_gap() -> None:
    """把**有覆盖**的组件标成「已知缺口」⇒ R3 必红（缺口已被补上 ⇒ 表过时）。"""
    lines = REGISTRY.read_text(encoding="utf-8").splitlines(keepends=True)
    i = _rows_index(lines, "AxiLiteSlave", "rst_n")
    cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
    cells[3] = f"—（**{GAP_MARKER}**）"
    lines[i] = "| " + " | ".join(cells) + " |\n"
    REGISTRY.write_text("".join(lines), encoding="utf-8")


def _poison_hardcode_back() -> None:
    """把组件清单硬编码回 rst_n 护栏 ⇒ R5 必红（双源）。"""
    text = RST_GUARD.read_text(encoding="utf-8")
    inject = ('COMPONENTS_W_RST_N="AxiLiteSlave Axi4Slave AxiToAxiLiteConverter AxiLiteGpio"\n')
    lines = text.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if line.startswith("set -euo pipefail"):
            lines.insert(i + 1, inject)
            break
    else:
        raise AssertionError("找不到 set -euo pipefail 锚点")
    RST_GUARD.write_text("".join(lines), encoding="utf-8")


POISONS = {
    "stale-port": ("把一条登记的端口名改错（R1 + R2）", _poison_stale_port),
    "drop-row": ("删掉一条 rst_n 登记（R2）", _poison_drop_row),
    "wrong-case": ("判别性用例指向不实例化该组件的文件（R3）", _poison_wrong_case),
    "fake-gap": ("把有覆盖的组件标成已知缺口（R3）", _poison_fake_gap),
    "hardcode-back": ("把组件清单硬编码回 rst_n 护栏（R5）", _poison_hardcode_back),
}

POISON_FILES = {
    "stale-port": [REGISTRY],
    "drop-row": [REGISTRY],
    "wrong-case": [REGISTRY],
    "fake-gap": [REGISTRY],
    "hardcode-back": [RST_GUARD],
}


def list_poisons() -> int:
    for name, (desc, _) in POISONS.items():
        print(f"{name}\t{desc}")
    return 0


def poison(name: str) -> int:
    if name not in POISONS:
        print(f"未知投毒：{name}", file=sys.stderr)
        return 2
    desc, fn = POISONS[name]
    paths = POISON_FILES[name]
    if SNAPSHOT_DIR.exists():
        shutil.rmtree(SNAPSHOT_DIR)
    SNAPSHOT_DIR.mkdir(parents=True)
    for i, p in enumerate(paths):
        shutil.copyfile(p, SNAPSHOT_DIR / f"{i}.bak")
    try:
        fn()
    except AssertionError as exc:
        shutil.rmtree(SNAPSHOT_DIR, ignore_errors=True)
        print(f"投毒失败：{exc}", file=sys.stderr)
        return 2
    POISON_TARGET.write_text(name, encoding="utf-8")
    print(f"已投毒：{name}（{desc}）")
    return 0


def restore() -> int:
    if not SNAPSHOT_DIR.exists() or not POISON_TARGET.exists():
        print("没有投毒快照（⇒ 无需还原）", file=sys.stderr)
        return 1
    name = POISON_TARGET.read_text(encoding="utf-8").strip()
    paths = POISON_FILES.get(name)
    if not paths:
        print("投毒目标缺失 ⇒ 无法还原", file=sys.stderr)
        return 2
    for i, p in enumerate(paths):
        bak = SNAPSHOT_DIR / f"{i}.bak"
        if not bak.exists():
            print("投毒快照缺失 ⇒ 无法还原", file=sys.stderr)
            return 2
        # ⚠️ MUST 用 copyfile（刷新 mtime）—— 「还原源码 ≠ 复原」的纪律
        shutil.copyfile(bak, p)
    shutil.rmtree(SNAPSHOT_DIR, ignore_errors=True)
    POISON_TARGET.unlink(missing_ok=True)
    print(f"已还原：{name}")
    return 0


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__)
        return 2
    if argv[0] == "check":
        return check()
    if argv[0] == "rst-n-components":
        return rst_n_components()
    if argv[0] == "list-poisons":
        return list_poisons()
    if argv[0] == "poison":
        return poison(argv[1] if len(argv) > 1 else "")
    if argv[0] == "restore":
        return restore()
    if argv[0] == "selftest-exemption":
        return selftest_exemption()
    print(f"未知命令：{argv[0]}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
