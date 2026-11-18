#!/usr/bin/env bash
# scripts/verify_example_only_headers.sh
#
# **example-only 头的护栏**（`example-file-boundary-2026-11` **D6** · capability
# 「example-only 头不得藏在 `include/`」）。
#
# ⚠️ **要治的病**：`include/**` 下有些头**只有 example / test 消费者**（库侧零消费者）
#   ⇒ ⇒ 它们本质上是 **example 专属设计**（⇒ 本该住 `examples/`）⇒ ⇒ 而 `include/` 是
#   **库公开面** ⇒ ⇒ 混进去就变成「同一语义两个物理载体」的温床（本 change 的立项动机）。
#
# ⚠️ **判据（三态 + 双向登记）**：
#   · **违规**（⇒ 必红，除非已登记）= `include/**` 下的头，**库侧消费者 == 0**
#     + **不是孤儿**（有 example/test 消费者）+ **不是歧义**（末段唯一）
#   · **孤儿**（零**任何**消费者）⇒ **不判**（那是 Zero-Debt 的「死代码」义务 ⇒ 本护栏不越界）
#   · **歧义**（末段同名 ⇒ ≥2 个不同路径的头同名）⇒ **不判**（无法判定引用指向谁）
#   ⇒ ⇒ **登记表双向**：① 违规头 MUST 在表里（否则红）② 表里的头 MUST 仍然违规
#     （⇒ 出现库侧消费者 ⇒ 「**可移出登记表**」⇒ 红）③ 表里的路径 MUST 存在（否则红）
#
# ⚠️ **「库侧消费者」的定义（R2 · 实现 ≠ 使用）**：
#   · 消费者 = `include/ src/ tests/ examples/ tools/` 下的 `#include "..."`（**剥注释**
#     后匹配；按**末段**匹配 ⇒ 子路径引用 `"rtl_emit/x.h"` 与 `"x.h"` 都算）
#   · **库侧** = 消费者落在 `include/**` 或 `src/**`，**且不是该头自己的实现 TU**
#     （⇒ `src/**/<同 stem>.cpp` 是**实现**，不是**使用**）
#   ⇒ ⇒ 故「公开头 + `src/` 实现」这一形态**不看实现 TU** ⇒ ⇒ 它通常需要登记。
#
# ⚠️ **缩面锚点**：`EXPECTED_EXEMPT_HEADERS` = 登记表条目数（增删 MUST 同步 ⇒
#   机制与 `CTEST_TOTAL` / `EXPECTED_EXEMPT_PORTS` 同型）。
#
# 退出码：0 = 全部成立 · 1 = 有违规 · 2 = 用法 / 环境错误（fail-closed）。
#
# 用法：
#   scripts/verify_example_only_headers.sh [--root <dir>] [--registry <tsv>]
#                                          [--expected <n>] [--self-test]

set -uo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$REPO_ROOT" || exit 2

ROOT="$REPO_ROOT"
REGISTRY=".rddf/checks/example-only-headers.tsv"
EXPECTED=38
SELF_TEST=0

while [ $# -gt 0 ]; do
    case "$1" in
        --root)     ROOT="${2:-}"; shift 2 ;;
        --registry) REGISTRY="${2:-}"; shift 2 ;;
        --expected) EXPECTED="${2:-}"; shift 2 ;;
        --self-test) SELF_TEST=1; shift ;;
        -h|--help)  sed -n '2,40p' "$0"; exit 0 ;;
        *) echo "usage: $0 [--root DIR] [--registry TSV] [--expected N] [--self-test]" >&2; exit 2 ;;
    esac
done

command -v python3 >/dev/null 2>&1 || { echo "::error::python3 不可用（fail-closed）" >&2; exit 2; }

WORK="$(mktemp -d)" || exit 2
cleanup() { rm -rf "$WORK"; return 0; }
trap cleanup EXIT

cat > "$WORK/scan.py" <<'PYEOF'
"""扫描 + 三态判定。参数：<root> <registry> <expected>"""
import glob
import os
import re
import sys
import collections

HEAD_EXTS = ("*.h", "*.hpp", "*.tpp", "*.inc")
SRC_EXTS = ("*.h", "*.hpp", "*.tpp", "*.inc", "*.cpp", "*.cc", "*.cxx", "*.C")
SCAN_ROOTS = ("include", "src", "tests", "examples", "tools")

INC_RE = re.compile(r'#\s*include\s*"([^"]+)"')


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def collect(root: str, roots, exts):
    out = []
    for r in roots:
        base = os.path.join(root, r)
        if not os.path.isdir(base):
            continue
        for e in exts:
            out.extend(glob.glob(os.path.join(base, "**", e), recursive=True))
    return sorted(set(out))


def rel(root: str, path: str) -> str:
    return os.path.relpath(path, root).replace(os.sep, "/")


def main() -> int:
    root, registry, expected = sys.argv[1], sys.argv[2], int(sys.argv[3])
    headers = collect(root, ("include",), HEAD_EXTS)
    sources = collect(root, SCAN_ROOTS, SRC_EXTS)

    by_base = collections.defaultdict(list)
    for h in headers:
        by_base[os.path.basename(h)].append(h)
    ambiguous = {b for b, hs in by_base.items() if len(hs) > 1}

    uses = collections.defaultdict(set)
    for f in sources:
        try:
            with open(f, encoding="utf-8", errors="replace") as fh:
                text = strip_comments(fh.read())
        except OSError:
            continue
        for m in INC_RE.finditer(text):
            base = os.path.basename(m.group(1))
            for h in by_base.get(base, []):
                uses[h].add(f)

    def own_impl(header: str, consumer: str) -> bool:
        """`src/**/<同 stem>.cpp` = 该头的**实现 TU** ⇒ 不是消费者（R2）。"""
        if not rel(root, consumer).startswith("src/"):
            return False
        return (os.path.splitext(os.path.basename(header))[0]
                == os.path.splitext(os.path.basename(consumer))[0])

    def is_lib(consumer: str) -> bool:
        r = rel(root, consumer)
        return r.startswith("include/") or r.startswith("src/")

    violations, orphans, ambigs = [], [], []
    for h in headers:
        if os.path.basename(h) in ambiguous:
            ambigs.append(h)
            continue
        if not uses.get(h):
            orphans.append(h)
            continue
        lib = [c for c in uses[h] if is_lib(c) and not own_impl(h, c)]
        if not lib:
            violations.append(h)

    # ── 登记表 ────────────────────────────────────────────────────────
    reg = []
    if os.path.isfile(registry):
        with open(registry, encoding="utf-8") as fh:
            for raw in fh:
                line = raw.rstrip("\n")
                if not line.strip() or line.lstrip().startswith("#"):
                    continue
                parts = line.split("\t")
                if len(parts) != 2:
                    print(f"FAIL R1 登记行不是 2 列：{line}")
                    continue
                reg.append(parts[0])
    else:
        print(f"FAIL R1 登记表不存在：{registry}")
        return 0  # 让驱动侧按 FAILURES 判红

    bad = 0

    # R2 数量锚点（防「表行与登记一起删」的静默缩面）
    if len(reg) != expected:
        print(f"FAIL R2 登记条目数 = {len(reg)}，锚点 = {expected}（增删 MUST 同步锚点）")
        bad = 1

    # R3 登记路径 MUST 存在（③ 投毒：登记路径不存在 ⇒ 红）
    for p in reg:
        if not os.path.exists(os.path.join(root, p)):
            print(f"FAIL R3 登记的路径不存在：{p}")
            bad = 1

    # R4 正向：违规头 MUST 已登记
    reg_set = set(reg)
    for h in violations:
        r = rel(root, h)
        if r not in reg_set:
            print(f"FAIL R4 example-only 头未登记（库侧零消费者 ⇒ 应搬去 examples/ 或登记理由）：{r}")
            bad = 1

    # R5 反向：登记的头 MUST **仍然**违规（出现库侧消费者 ⇒ 可移出登记表 ⇒ 红）
    viol_set = {rel(root, h) for h in violations}
    for p in reg:
        if p not in viol_set:
            print(f"FAIL R5 「可移出登记表」：{p} 已有库侧消费者（或已不是 include/ 下的头）")
            bad = 1

    print(f"  头 {len(headers)} 个 ⇒ 库侧消费者存在 {len(headers) - len(violations) - len(orphans) - len(ambigs)}"
          f" · 违规 {len(violations)} · 孤儿 {len(orphans)}（不判）· 歧义 {len(ambigs)}（不判）"
          f" · 登记 {len(reg)}")
    return bad


if __name__ == "__main__":
    sys.exit(main())
PYEOF

run_guard() {   # $1=root $2=registry $3=expected  ⇒ 输出 FAIL 行并返回 0/1
    local out rc
    out="$(python3 "$WORK/scan.py" "$1" "$2" "$3" 2>&1)"
    rc=$?
    [ -n "$out" ] && printf '%s\n' "$out"
    # 有 FAIL 行 ⇒ 红（python 返回 1）；无 FAIL 但 rc!=0 ⇒ 环境错
    if printf '%s\n' "$out" | grep -q '^FAIL '; then
        return 1
    fi
    return 0
}

# ---------------------------------------------------------------------------
# --self-test：6 条投毒（合成 fixture 树 ⇒ **不碰**真实源树）
#   ① 新头落 include/ 且只有 example 消费者 ⇒ 红
#   ② 子路径引用被认出 ⇒ 绿
#   ③ 登记路径不存在 ⇒ 红
#   ④ 登记头新增库侧消费者 ⇒ 红（「可移出登记表」）
#   ⑤ 孤儿头 ⇒ 不得红（退出码 0）
#   ⑥ 末段同名歧义 ⇒ 不得红（退出码 0）
# ---------------------------------------------------------------------------
self_test() {
    local fails=0 t
    local base="$WORK/st"

    mk() {   # mk <dir> … 造一份最小 fixture 树
        local d="$1"; shift
        mkdir -p "$d/include/sub" "$d/src" "$d/examples" "$d/tests" "$d/reg"
        # 库侧消费者存在的基线头（不违规）
        printf 'module a;\nendmodule\n' > "$d/include/sub/pub.h"
        printf '#include "sub/pub.h"\n' > "$d/src/impl.cpp"        # 实现 TU
        printf '#include "pub.h"\n'     > "$d/tests/t.cpp"          # 测试消费者
        : > "$d/reg/r.tsv"
    }

    # ① 新头只被 example 消费 ⇒ 红
    t="$base/p1"; mk "$t"
    printf 'module b;\nendmodule\n' > "$t/include/orphan_by_example.h"
    printf '#include "orphan_by_example.h"\n' > "$t/examples/e.cpp"
    if run_guard "$t" "$t/reg/r.tsv" 0 >/dev/null; then
        echo "  ❌ 投毒① 未转红（新头只被 example 消费 ⇒ 应红）"; fails=$((fails+1))
    else
        echo "  ✅ 投毒① 转红"
    fi

    # ② 子路径引用被认出 ⇒ 绿（头只被 src 通过**子路径**引用 ⇒ 不违规）
    t="$base/p2"; mk "$t"
    printf 'module c;\nendmodule\n' > "$t/include/sub/used_by_src.h"
    printf '#include "sub/used_by_src.h"\n' > "$t/src/lib_user.cpp"
    if run_guard "$t" "$t/reg/r.tsv" 0 >/dev/null; then
        echo "  ✅ 投毒② 仍绿（子路径引用被认出）"
    else
        echo "  ❌ 投毒② 误红（子路径引用未被认出）"; fails=$((fails+1))
    fi

    # ③ 登记路径不存在 ⇒ 红
    t="$base/p3"; mk "$t"
    printf 'ghost/header.h\t理由\n' > "$t/reg/r.tsv"
    if run_guard "$t" "$t/reg/r.tsv" 1 >/dev/null; then
        echo "  ❌ 投毒③ 未转红（登记路径不存在 ⇒ 应红）"; fails=$((fails+1))
    else
        echo "  ✅ 投毒③ 转红"
    fi

    # ④ 登记头新增库侧消费者 ⇒ 红（「可移出登记表」）
    t="$base/p4"; mk "$t"
    printf 'module d;\nendmodule\n' > "$t/include/was_example_only.h"
    printf '#include "was_example_only.h"\n' > "$t/examples/e.cpp"
    printf 'include/was_example_only.h\t理由\n' > "$t/reg/r.tsv"
    # ⇒ 此时应当**绿**（登记了违规头）
    if ! run_guard "$t" "$t/reg/r.tsv" 1 >/dev/null; then
        echo "  ❌ 投毒④ 前置不成立（登记后仍红）"; fails=$((fails+1))
    else
        printf '#include "was_example_only.h"\n' > "$t/src/lib_consumer.cpp"   # 新增库侧消费者
        if run_guard "$t" "$t/reg/r.tsv" 1 >/dev/null; then
            echo "  ❌ 投毒④ 未转红（登记头新增库侧消费者 ⇒ 应报「可移出登记表」）"; fails=$((fails+1))
        else
            echo "  ✅ 投毒④ 转红（可移出登记表）"
        fi
    fi

    # ⑤ 孤儿头（零消费者）⇒ **不得**红
    t="$base/p5"; mk "$t"
    printf 'module e;\nendmodule\n' > "$t/include/never_used.h"
    if run_guard "$t" "$t/reg/r.tsv" 0 >/dev/null; then
        echo "  ✅ 投毒⑤ 仍绿（孤儿头不判）"
    else
        echo "  ❌ 投毒⑤ 误红（孤儿头不应判红）"; fails=$((fails+1))
    fi

    # ⑥ 末段同名歧义 ⇒ **不得**红
    t="$base/p6"
    mkdir -p "$t/include/a" "$t/include/b" "$t/src" "$t/examples" "$t/tests" "$t/reg"
    printf 'module x;\nendmodule\n' > "$t/include/a/dup.h"
    printf 'module y;\nendmodule\n' > "$t/include/b/dup.h"
    printf '#include "dup.h"\n' > "$t/examples/e.cpp"
    : > "$t/reg/r.tsv"
    if run_guard "$t" "$t/reg/r.tsv" 0 >/dev/null; then
        echo "  ✅ 投毒⑥ 仍绿（末段同名歧义不判）"
    else
        echo "  ❌ 投毒⑥ 误红（歧义不应判红）"; fails=$((fails+1))
    fi

    if [ "$fails" -eq 0 ]; then
        echo "✅ verify_example_only_headers --self-test: PASS（6 条投毒各自成立）"
        return 0
    fi
    echo "❌ verify_example_only_headers --self-test: FAIL（$fails 条）"
    return 1
}

if [ "$SELF_TEST" = "1" ]; then
    self_test
    exit $?
fi

if run_guard "$ROOT" "$REGISTRY" "$EXPECTED"; then
    echo "✅ verify_example_only_headers: PASS（三态判定 · 登记表双向 · 数量锚点 · 实现 TU 不算消费者）"
    exit 0
fi
echo "❌ verify_example_only_headers: FAIL"
exit 1
