#!/usr/bin/env python3
"""双后端 parity 源文件计数分析器（护栏与 fixture 自检**共用同一份**）。

为什么需要（2026-10-30，planner 判据修订）
----------------------------------------------------------------------
`objective-test-coverage-100pct` 的完成判据由「`ctest -L verilator_e2e` ≥ 100」
改为「**双后端 parity 源文件数 ≥ 60**」⇒ ⇒ 需要一个**可机械核验**的计数
⇒ ⇒ 本文件是护栏与 fixture 自检**共用同一份**的分析器（与 `fsm_wiring.py` 同型）
⇒ ⇒ 护栏与自检**不会漂移**。

口径（权威定义见 `.rddf/roadmap/objectives/objective-test-coverage-100pct.md` §2）
----------------------------------------------------------------------
「双后端 parity 源文件」= 仓内测试源文件中满足**任一**者：

1. **剥注释与字符串字面量后**含 `DUAL_BACKEND_TEST_PAIR(` 的**调用**
   （⚠️ 必须是**调用**，不是**提到** —— 9 个契约锁定的**单后端** verilator
   参考实现只在**注释**里提到该宏 ⇒ 子串匹配会把它们误判成真 dual
   ⇒ ⇒ 它们**永远**不计入本口径）；
2. ∈ **hand-crafted 真 dual 白名单**（当前 1 个）—— 它在同一 `TEST_CASE` 内
   跑 2×`Simulator` + `InterpreterBackend` + `VerilatorBackend` ⇒ 语义上就是
   双后端 parity，只是没用 macro。

⚠️ **为什么复用 `fsm_wiring` 的词法器**（而不是自己写一份）：两份护栏对
「什么算**真实代码**」的口径 MUST **逐位一致** ⇒ 否则同一段文本在两个护栏里
一个算代码、一个不算 ⇒ ⇒ 漂移 ⇒ ⇒ 直接 `import` 那一份 ⇒ 口径唯一。

⚠️ **为什么用构建树（`build.make`）而不是解析 CMakeLists 来做 target→源文件映射**：
`build.make` 由 **CMake 自己生成** ⇒ 没有需要维护的语法 ⇒ 不会因「新增一种
注册写法」而漏解析 ⇒ ⇒ 不会**静默漏报**。
"""

import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from fsm_wiring import strip_comments_and_strings  # noqa: E402  (路径设置在前)

# ---------------------------------------------------------------------------
# 扫描面
# ---------------------------------------------------------------------------

#: 扫描根目录（相对仓库根）
SCAN_ROOTS = ("tests", "examples")

#: **被计数**的源文件扩展名
SOURCE_EXT = (".cpp",)

#: 扫描面内**已知**的 C/C++ 扩展名 —— 用于 **fail-closed** 探针：
#: 若仓内出现本清单之外的扩展名 ⇒ 计数可能**漏报** ⇒ 判违规（而非静默放过）。
KNOWN_CXX_EXT = (".cpp", ".cc", ".cxx", ".C")

#: hand-crafted 真 dual 白名单（**显式**、可审计；每项由自检 fixture 钉住）
HANDCRAFTED_DUAL = (
    "examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp",
)

#: `DUAL_BACKEND_TEST_PAIR(` 的**调用**（不是「提到」）
RE_MACRO_CALL = re.compile(r"DUAL_BACKEND_TEST_PAIR\s*\(")

#: `TEST_CASE(` —— hand-crafted 条目的「它是个真测试」证据
RE_TEST_CASE = re.compile(r"\bTEST_CASE\s*\(")

#: hand-crafted 条目的「它真的跑两个后端」证据
HANDCRAFTED_REQUIRED_TOKENS = ("InterpreterBackend", "VerilatorBackend")

#: capability 里「现有 9 个 verilator 测试 MUST 保留原状」的 Requirement 标题
CONTRACT_LOCKED_REQ = "现有 9 个 verilator 测试 MUST 保留原状"

#: 该 capability spec 的路径
CONTRACT_SPEC = "openspec/specs/dual-backend-test-coverage/spec.md"


class Result(object):
    """一次扫描的结果。"""

    def __init__(self):
        self.files = []       # [(relpath, kind)]，kind ∈ {"macro", "handcrafted"}
        self.violations = []  # [(kind, detail)]
        self.pairs = 0        # 真 dual 的 **macro 调用**总数（= 深度）

    @property
    def count(self):
        return len(self.files)

    def parity_set(self):
        return set(p for p, _ in self.files)


def _read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


def evidence_for(relpath, stripped):
    """返回该文件的 parity 证据种类（`"macro"` / `"handcrafted"` / `None`）。"""
    if RE_MACRO_CALL.search(stripped):
        return "macro"
    if relpath in HANDCRAFTED_DUAL:
        return "handcrafted"
    return None


def scan(root):
    """扫描 `root` 下的测试源文件，返回 `Result`。"""
    res = Result()
    seen_whitelist = set()

    for scan_root in SCAN_ROOTS:
        base = os.path.join(root, scan_root)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [
                d for d in dirnames
                if d not in (".git", "third_party", "external", "build")
            ]
            for name in filenames:
                ext = os.path.splitext(name)[1]
                if ext not in KNOWN_CXX_EXT:
                    continue
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                if ext not in SOURCE_EXT:
                    # fail-closed：扫描面未覆盖该扩展名 ⇒ 计数可能漏报
                    res.violations.append((
                        "scan-surface",
                        "%s：扩展名 %s 不在计数扫描面 %s 内 ⇒ 计数可能漏报"
                        % (rel, ext, list(SOURCE_EXT)),
                    ))
                    continue

                stripped = strip_comments_and_strings(_read(full))
                kind = evidence_for(rel, stripped)
                if kind is None:
                    continue
                res.files.append((rel, kind))
                # ⚠️ **深度**计数：只数**真实代码**里的 macro 调用 ⇒ 注释/字符串
                #    里的不计入（否则删掉一个真调用而留一个注释会被「抵消」）。
                res.pairs += len(RE_MACRO_CALL.findall(stripped))
                if rel in HANDCRAFTED_DUAL:
                    seen_whitelist.add(rel)

    # ---- 白名单 fail-closed 探针 ----
    for rel in HANDCRAFTED_DUAL:
        if rel in seen_whitelist:
            continue
        full = os.path.join(root, rel)
        if not os.path.isfile(full):
            res.violations.append((
                "whitelist-missing",
                "%s：hand-crafted 真 dual 白名单条目**不存在** ⇒ 计数口径已漂移"
                % rel,
            ))
            continue
        res.violations.append((
            "whitelist-evidence",
            "%s：白名单条目已**不再**是 hand-crafted 真 dual（macro 调用消失）"
            "⇒ 计数口径已漂移" % rel,
        ))

    res.files.sort()
    res.violations.sort()
    return res


def verify_whitelist_evidence(root):
    """逐个校验白名单条目**仍**具备「真 dual」的证据（fail-closed）。"""
    problems = []
    for rel in HANDCRAFTED_DUAL:
        full = os.path.join(root, rel)
        if not os.path.isfile(full):
            problems.append("%s：白名单条目不存在" % rel)
            continue
        stripped = strip_comments_and_strings(_read(full))
        missing = [t for t in HANDCRAFTED_REQUIRED_TOKENS if t not in stripped]
        if missing:
            problems.append(
                "%s：白名单条目缺少证据 %s ⇒ 不再是 hand-crafted 真 dual"
                % (rel, missing)
            )
        if not RE_TEST_CASE.search(stripped):
            problems.append("%s：白名单条目没有 TEST_CASE" % rel)
    return problems


# ---------------------------------------------------------------------------
# 构建树 / CTest / capability spec —— 交叉核验
# ---------------------------------------------------------------------------

def parse_build_tree(build_dir):
    """从构建树读出 `{target: set(绝对源文件路径)}`。

    ⚠️ 用 **CMake 自己生成的** `build.make`（而不是解析 `CMakeLists.txt`）⇒
    没有需要维护的语法 ⇒ 不会因「新增一种注册写法」而**静默漏解析**。
    """
    mapping = {}
    if not os.path.isdir(build_dir):
        return None
    for dirpath, dirnames, filenames in os.walk(build_dir):
        if "build.make" not in filenames:
            continue
        parent = os.path.basename(dirpath)
        if not parent.endswith(".dir"):
            continue
        target = parent[:-len(".dir")]
        srcs = mapping.setdefault(target, set())
        for line in _read(os.path.join(dirpath, "build.make")).splitlines():
            if ": " not in line:
                continue
            lhs, rhs = line.split(": ", 1)
            lhs = lhs.strip()
            rhs = rhs.strip()
            if not lhs.endswith(".cpp.o"):
                continue
            if rhs.startswith("/") and rhs.endswith(".cpp"):
                srcs.add(rhs)
    return mapping


def labeled_entries(build_dir):
    """返回带 `verilator_e2e` 标签的 ctest 条目名集合（ctest 不可用 ⇒ `None`）。"""
    try:
        out = subprocess.run(
            ["ctest", "--test-dir", build_dir, "--show-only=json-v1"],
            capture_output=True, text=True, check=False,
        ).stdout
    except OSError:
        return None
    try:
        data = json.loads(out)
    except ValueError:
        return None
    names = set()
    for t in data.get("tests", []):
        for prop in t.get("properties", []):
            if prop.get("name") == "LABELS" and "verilator_e2e" in (prop.get("value") or []):
                names.add(t["name"])
                break
    return names


def contract_locked_files(root):
    """从 capability spec 抽出「MUST 保留原状」的契约锁定文件清单。

    ⇒ 返回 `None` 表示**无法解析**（spec 文本形态变了）⇒ 调用方 MUST
    fail-closed（不得当作「空清单」）。
    """
    spec = os.path.join(root, CONTRACT_SPEC)
    if not os.path.isfile(spec):
        return None
    text = _read(spec)
    idx = text.find(CONTRACT_LOCKED_REQ)
    if idx < 0:
        return None
    para = text[idx:]
    end = para.find("\n### ")
    if end > 0:
        para = para[:end]
    files = re.findall(r"`(tests/[A-Za-z0-9_./-]+\.cpp)`", para)
    return sorted(set(files))


def cross_check(root, build_dir, res, labels=None):
    """交叉核验：parity 源文件 ⇔ 带标签的 ctest 条目 ⇔ 契约锁定清单。

    `labels` 为 `None` ⇒ 从 `build_dir` 的 ctest 查询；**非 None** ⇒ 直接采用
    （fixture 自检用 —— 合成构建树不是真的 ctest 树 ⇒ ctest 查不到标签）。

    返回 (violations, notes)；`notes` 是给人看的说明（不判违规）。
    """
    violations = []
    notes = []

    mapping = parse_build_tree(build_dir)
    if mapping is None:
        notes.append("构建树 %s 不存在 ⇒ **跳过**交叉核验（计数闸门仍生效）" % build_dir)
        return violations, notes

    if labels is None:
        labels = labeled_entries(build_dir)
        if labels is None:
            notes.append("ctest 不可用 ⇒ **跳过**交叉核验（计数闸门仍生效）")
            return violations, notes

    locked = contract_locked_files(root)
    if locked is None:
        violations.append((
            "contract-locked-unparsed",
            "无法从 %s 解析「MUST 保留原状」的契约锁定清单 ⇒ 交叉核验 **fail-closed**"
            % CONTRACT_SPEC,
        ))
        locked = []

    parity = res.parity_set()
    locked_set = set(locked)

    # ① 契约锁定文件**不得**是 parity 文件（capability：MUST NOT 迁移到 macro）
    for rel in sorted(parity & locked_set):
        violations.append((
            "contract-locked-parity",
            "%s：属 capability「MUST 保留原状」的契约锁定文件 ⇒ MUST NOT 迁到 "
            "DUAL_BACKEND_TEST macro" % rel,
        ))

    # ② target → 源文件（仓库相对路径）
    def rel_sources(target):
        out = set()
        for abs_src in mapping.get(target, ()):
            rel = os.path.relpath(abs_src, root).replace(os.sep, "/")
            out.add(rel)
        return out

    # ③ 带标签条目里，哪些**背后**有 parity 源文件
    parity_entries = set()
    non_parity_entries = {}
    for entry in sorted(labels):
        srcs = rel_sources(entry)
        if srcs & parity:
            parity_entries.add(entry)
        else:
            non_parity_entries[entry] = srcs

    # ④ 每个 parity 源文件 MUST 至少被一个**带标签**条目构建
    backed = set()
    for entry in parity_entries:
        backed |= (rel_sources(entry) & parity)
    for rel in sorted(parity - backed):
        violations.append((
            "parity-not-labeled",
            "%s：有 parity 证据，但**没有任何**带 `verilator_e2e` 标签的 ctest 条目"
            "构建它 ⇒ 该覆盖**不会真的被跑**" % rel,
        ))

    # ⑤ 带标签但**背后没有** parity 的条目 ⇒ MUST 属契约锁定清单
    for entry, srcs in sorted(non_parity_entries.items()):
        test_srcs = set(s for s in srcs if s not in ("tests/catch_amalgamated.cpp",))
        if not test_srcs:
            continue
        if not (test_srcs & locked_set):
            violations.append((
                "unclassified-labeled-entry",
                "%s：带 `verilator_e2e` 标签但**背后没有**双后端 parity，"
                "且**不在** capability 的契约锁定清单里 ⇒ 旧口径会把它误算成"
                "「双后端覆盖」（这正是本次修订要消灭的漏洞）" % entry,
            ))

    notes.append("带 `verilator_e2e` 标签的 ctest 条目 = %d" % len(labels))
    notes.append("其中**背后有**双后端 parity 的条目 = %d" % len(parity_entries))
    notes.append("其中**单后端**（契约锁定）条目 = %d" % len(non_parity_entries))
    notes.append("parity 源文件数 = %d" % res.count)
    notes.append("parity macro 调用数（深度）= %d" % res.pairs)
    return violations, notes


# ---------------------------------------------------------------------------
# fixture 自检（护栏与自检共用同一份分析器 ⇒ 不会漂移）
# ---------------------------------------------------------------------------

def _write(path, text):
    d = os.path.dirname(path)
    if d and not os.path.isdir(d):
        os.makedirs(d)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def _fixture_tree(tmp):
    """构造一棵**合成**的仓内树（不是真实仓）⇒ 自检与真实仓解耦。"""
    # ① macro 调用 ⇒ 计数
    _write(os.path.join(tmp, "tests/test_a.cpp"),
           '#include "x.h"\nvoid f() {\n'
           '    DUAL_BACKEND_TEST_PAIR("name", []() { /* body */ });\n}\n')
    # ② **仅注释**提到 macro ⇒ 不计数（这正是契约锁定参考实现的形态）
    _write(os.path.join(tmp, "tests/test_b.cpp"),
           '// DUAL_BACKEND_TEST_PAIR("name", []() {});\n'
           '/* DUAL_BACKEND_TEST_PAIR( */\nint g() { return 0; }\n')
    # ③ **字符串字面量**里出现 macro 文本 ⇒ 不计数
    _write(os.path.join(tmp, "tests/test_c.cpp"),
           'const char *s = "DUAL_BACKEND_TEST_PAIR(";\n')
    # ④ hand-crafted 白名单条目（真实路径）⇒ 计数
    _write(os.path.join(tmp, HANDCRAFTED_DUAL[0]),
           '#include "dual.h"\n'
           'TEST_CASE("handcrafted") {\n'
           '  InterpreterBackend ib; VerilatorBackend vb;\n'
           '  (void)ib; (void)vb;\n}\n')
    # ⑤ 扫描面外的扩展名 ⇒ fail-closed 违规
    _write(os.path.join(tmp, "tests/test_d.cc"), "int d() { return 0; }\n")
    # ⑥ 一份**合成**的 capability spec（契约锁定清单的可解析来源）
    _write(os.path.join(tmp, CONTRACT_SPEC),
           "### Requirement: %s\n\n"
           "`tests/test_locked_ref.cpp` / `tests/test_locked_ref2.cpp` 2 个测试 "
           "MUST 保留原状，不迁移到 `DUAL_BACKEND_TEST` macro。\n"
           % CONTRACT_LOCKED_REQ)
    return tmp


def _fixture_build_tree(tmp, build_dir, target_sources):
    """构造一棵**合成**的构建树：`build.make` 由 CMake 生成 ⇒ 这里照抄它的形态。"""
    for target, srcs in target_sources.items():
        d = os.path.join(build_dir, "tests", "CMakeFiles", target + ".dir")
        os.makedirs(d, exist_ok=True)
        lines = []
        for rel in srcs:
            obj = "tests/CMakeFiles/%s.dir/%s.cpp.o" % (target, rel.replace("/", "_"))
            lines.append("%s: /abs/repo/%s" % (obj, rel))
        _write(os.path.join(d, "build.make"), "\n".join(lines) + "\n")


def self_test():
    import shutil
    import tempfile

    failures = []

    def check(name, cond, detail=""):
        if cond:
            print("  ✓ %s" % name)
        else:
            print("  ✗ %s %s" % (name, detail))
            failures.append(name)

    tmp = tempfile.mkdtemp(prefix="dual_parity_")
    try:
        _fixture_tree(tmp)
        res = scan(tmp)
        by_path = dict(res.files)

        # F1：macro 调用 ⇒ 计数
        check("F1 macro-call-counted", "tests/test_a.cpp" in by_path)
        # F2：仅注释提到 ⇒ 不计数
        check("F2 comment-mention-not-counted", "tests/test_b.cpp" not in by_path)
        # F3：字符串字面量 ⇒ 不计数
        check("F3 string-literal-not-counted", "tests/test_c.cpp" not in by_path)
        # F4：hand-crafted 白名单 ⇒ 计数
        check("F4 handcrafted-counted",
              by_path.get(HANDCRAFTED_DUAL[0]) == "handcrafted")
        # F5：扫描面外的扩展名 ⇒ **fail-closed** 违规
        kinds = [v[0] for v in res.violations]
        check("F5 scan-surface-fail-closed", "scan-surface" in kinds, str(res.violations))
        # F6：白名单条目证据齐全 ⇒ 无问题
        probs = verify_whitelist_evidence(tmp)
        check("F6 whitelist-evidence-ok", not probs, str(probs))
        # F7：白名单条目缺证据 ⇒ 违规
        _write(os.path.join(tmp, HANDCRAFTED_DUAL[0]),
               'TEST_CASE("x") { }\n')  # 去掉两个 backend 标识
        probs2 = verify_whitelist_evidence(tmp)
        check("F7 whitelist-evidence-violation", bool(probs2), str(probs2))
        # F8：白名单条目被删 ⇒ 违规
        os.remove(os.path.join(tmp, HANDCRAFTED_DUAL[0]))
        res3 = scan(tmp)
        kinds3 = [v[0] for v in res3.violations]
        check("F8 whitelist-missing-violation",
              "whitelist-missing" in kinds3, str(res3.violations))

        # ---- 交叉核验的 fixture ----
        # F9：parity 源文件**没有**任何带标签条目构建 ⇒ 违规
        tmp2 = tempfile.mkdtemp(prefix="dual_parity_x_")
        try:
            _fixture_tree(tmp2)
            bd = os.path.join(tmp2, "build")
            _fixture_build_tree(tmp2, bd, {"test_other": ["tests/test_x.cpp"]})
            r9 = scan(tmp2)
            v9, _ = cross_check(tmp2, bd, r9, labels={"test_other"})
            check("F9 parity-not-labeled-violation",
                  any(k == "parity-not-labeled" for k, _ in v9), str(v9))
        finally:
            shutil.rmtree(tmp2, ignore_errors=True)

        # F10：契约锁定清单**无法解析** ⇒ fail-closed 违规
        tmp3 = tempfile.mkdtemp(prefix="dual_parity_y_")
        try:
            _fixture_tree(tmp3)
            # ⚠️ 删掉 capability spec ⇒ 契约锁定清单**无法解析** ⇒ MUST fail-closed
            os.remove(os.path.join(tmp3, CONTRACT_SPEC))
            bd = os.path.join(tmp3, "build")
            _fixture_build_tree(tmp3, bd, {"test_a": ["tests/test_a.cpp"]})
            r10 = scan(tmp3)
            v10, _ = cross_check(tmp3, bd, r10, labels={"test_a"})
            check("F10 contract-locked-unparsed-fail-closed",
                  any(k == "contract-locked-unparsed" for k, _ in v10), str(v10))
        finally:
            shutil.rmtree(tmp3, ignore_errors=True)

        # F11：带标签条目**既无** parity **又不在**契约锁定清单 ⇒ 违规
        #     （这正是**旧判据**的漏洞：标签数会把它误算成「双后端覆盖」）
        tmp4 = tempfile.mkdtemp(prefix="dual_parity_z_")
        try:
            _fixture_tree(tmp4)
            bd = os.path.join(tmp4, "build")
            _fixture_build_tree(tmp4, bd, {
                "test_a": ["tests/test_a.cpp"],              # parity ✔
                "test_b": ["tests/test_b.cpp"],              # 非 parity、非锁定 ✘
            })
            r11 = scan(tmp4)
            v11, _ = cross_check(tmp4, bd, r11, labels={"test_a", "test_b"})
            check("F11 unclassified-labeled-entry-violation",
                  any(k == "unclassified-labeled-entry" for k, _ in v11), str(v11))
        finally:
            shutil.rmtree(tmp4, ignore_errors=True)

        # F12：契约锁定文件**却**带 parity 证据 ⇒ 违规
        #     （capability：MUST NOT 迁到 DUAL_BACKEND_TEST macro）
        tmp5 = tempfile.mkdtemp(prefix="dual_parity_w_")
        try:
            _fixture_tree(tmp5)
            _write(os.path.join(tmp5, "tests/test_locked_ref.cpp"),
                   'DUAL_BACKEND_TEST_PAIR("x", []() {});\n')
            bd = os.path.join(tmp5, "build")
            _fixture_build_tree(tmp5, bd, {
                "test_a": ["tests/test_a.cpp"],
                "test_locked_ref": ["tests/test_locked_ref.cpp"],
            })
            r12 = scan(tmp5)
            v12, _ = cross_check(tmp5, bd, r12, labels={"test_a", "test_locked_ref"})
            check("F12 contract-locked-parity-violation",
                  any(k == "contract-locked-parity" for k, _ in v12), str(v12))
        finally:
            shutil.rmtree(tmp5, ignore_errors=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if failures:
        print("SELF-TEST FAILED: %d 项" % len(failures))
        return 1
    print("self-test: 12/12 fixture 全绿")
    return 0


def main(argv):
    root = "."
    build_dir = "build"
    do_list = False
    do_self_test = False
    do_check = False
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--root":
            i += 1
            root = argv[i]
        elif a == "--build-dir":
            i += 1
            build_dir = argv[i]
        elif a == "--list":
            do_list = True
        elif a == "--check":
            do_check = True
        elif a == "--self-test":
            do_self_test = True
        elif a in ("-h", "--help"):
            print(__doc__)
            return 0
        else:
            sys.stderr.write("unknown argument: %s\n" % a)
            return 2
        i += 1

    if do_self_test:
        return self_test()

    res = scan(root)
    if do_list:
        for rel, kind in res.files:
            print("%-70s %s" % (rel, kind))
    print("PARITY_COUNT=%d" % res.count)
    print("PARITY_PAIRS=%d" % res.pairs)
    for kind, detail in res.violations:
        print("VIOLATION %s: %s" % (kind, detail))

    if do_check:
        for problem in verify_whitelist_evidence(root):
            print("VIOLATION whitelist-evidence: %s" % problem)
        cross_v, notes = cross_check(root, build_dir, res)
        for kind, detail in cross_v:
            print("VIOLATION %s: %s" % (kind, detail))
        for note in notes:
            print("NOTE %s" % note)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
