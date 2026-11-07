#!/usr/bin/env bash
# scripts/verify_hier_corpus_coverage.sh
#
# **层次化路径的语料级回归 · 覆盖栅栏**（残留 1b · capability
# `hierarchical-corpus-regression`）。
#
# ⚠️ **为什么需要它**（★ 判别力 vs 覆盖：两者**不是**一回事）：
#   一份**固定清单**是 **snapshot**，不是 **fence** ⇒ ⇒ 半年后语料多了 20 个复合 DUT
#   ⇒ ⇒ 栅栏仍只覆盖那 10 个 ⇒ ⇒ **且没有任何红灯** ⇒ ⇒ ⇒ 缺口**静默**回归。
#   ⇒ ⇒ 本护栏做**三向双向核对**：
#     ① **登记 → 用例**：登记表里的每个 DUT ⇒ MUST 有对应的语料回归用例
#     ② **用例 → 登记**：每个语料回归用例涉及的 DUT ⇒ MUST 在登记表里
#     ③ **★ 语料 → 登记**：**所有**在 dual-backend 语料里**带子组件**的 DUT
#        ⇒ MUST 在登记表里 **或** 在**显式豁免表**里（+ 原因）
#        ⇒ ⇒ ⇒ **这一条才是让栅栏不随时间退化的机制**
#
# ⚠️ **「带子组件」的判定口径**：DUT 的类定义里出现 `ch_module<` ⇒ ⇒ 它在
#   `Granularity::ForceLayer` 下**可能**产出多个 module ⇒ ⇒ ⇒ 只有这类 DUT 才是
#   语料级回归的**有效对象**（★ 叶子 DUT 在 `ForceLayer` 下**仍只有 1 个 module**
#   ⇒ ⇒ **逐拍**等价上**结构性零判别力** ⇒ ⇒ 普查实测：132 个 DUT 里只有 10 个）。
#
# ⚠️ **该口径的已知边界**（★ critic SHOULD_FIX-4 ⇒ 如实登记 ⇒ ⇒ 「③ 是唯一机制」这个
#    主张的承重前提是**召回率** ⇒ ⇒ 边界 MUST 写明）：
#   · **误报方向（吵，不漏）**：类体截取用**列 0** 的 `};` ⇒ ⇒ 命名空间内**缩进**的类
#     会把类体延长到下一个列 0 `};` ⇒ ⇒ 可能**多**算几个复合 DUT ⇒ ⇒ ⇒ fail-safe。
#   · **漏判方向（真洞）**：`describe() override;` **声明**在类体内、**定义在类体外**
#     ⇒ ⇒ 类体文本里**没有** `ch_module<` ⇒ ⇒ 漏判；**宏生成**的类同理。
#     ⇒ ⇒ 仓内**现有**语料**无**此形态（10/10 与普查一致）⇒ ⇒ 但**新增**语料若用
#     那种写法 ⇒ ⇒ ③ 的召回率为 0 ⇒ ⇒ ⇒ **登记在此以免后人误信召回率是 100%**。
#
# ⚠️ **两个实现要点**（都踩过 ⇒ ⇒ 写进注释防后人重犯）：
#   ① 类定义**可能在 `include/` 里**（★ `ICachePrefetch` 是**库**类）⇒ ⇒ 扫描 MUST 含
#      `include/` ⇒ ⇒ 只扫 `tests/` + `examples/` 会**漏判** ⇒ ⇒ ⇒ 第 ③ 个方向就**有洞**。
#   ② DUT 名**可能带命名空间限定**（★ `chlib::ICacheAsync`）⇒ ⇒ 正则 MUST 允许 `::`
#      ⇒ ⇒ 否则会只截到 `chlib` ⇒ ⇒ **误判**。
#
# 用法：
#   bash scripts/verify_hier_corpus_coverage.sh              # 主路径
#   bash scripts/verify_hier_corpus_coverage.sh --self-test  # 逐条投毒 ⇒ 必红
#
# 退出码：0 = 通过 · 1 = 判据失败
# ============================================================================
set -uo pipefail

cd "$(dirname "$0")/.." || exit 2

WORK="$(mktemp -d "${TMPDIR:-/tmp}/cpphdl_corpus.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

fails=0
ok()   { printf '\033[0;32m✅ %s\033[0m\n' "$*"; }
fail() { printf '\033[0;31m❌ %s\033[0m\n' "$*"; fails=$((fails + 1)); }

REGISTRY="tests/hier_corpus_registry.md"
EXTRA_CORPUS_ROOT=""

# ═══════════════════════════════════════════════════════════════════════════
# 分析器（python3 ⇒ ⇒ 免去 bash 正则的脆弱性）
# ═══════════════════════════════════════════════════════════════════════════
cat > "$WORK/analyze.py" <<'PYEOF'
import os
import re
import sys

ROOT = sys.argv[1]
REGISTRY = sys.argv[2]
EXTRA = sys.argv[3] if len(sys.argv) > 3 else ''

# ⚠️ DUT 类型**可能带命名空间限定**（★ `chlib::ICacheAsync`）⇒ ⇒ 正则 MUST 允许 `::`
CALL = re.compile(r'DUAL_BACKEND_TEST_(?:PAIR|TEST)\s*\(\s*((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)')
CORPUS_CASE = re.compile(r'corpus hierarchy regression \([^)]*\):\s*([A-Za-z_]\w*)')


def scan_sources(bases, exts):
    out = []
    for base in bases:
        d = os.path.join(ROOT, base)
        if not os.path.isdir(d):
            continue
        for dirpath, _dirs, files in os.walk(d):
            for fn in files:
                if fn.endswith(exts):
                    out.append(os.path.join(dirpath, fn))
    return sorted(out)


# ⚠️ **类定义可能在 `include/` 里**（★ `ICachePrefetch` 是**库**类 ⇒ 它的
#    `ch_module<` 不在测试文件里）⇒ ⇒ 扫描 MUST 含 `include/`。
corpus_files = scan_sources(('tests', 'examples'), ('.cpp',))
source_files = scan_sources(('tests', 'examples', 'include'), ('.cpp', '.h'))
if EXTRA:
    for dirpath, _dirs, files in os.walk(EXTRA):
        for fn in files:
            if fn.endswith('.cpp'):
                corpus_files.append(os.path.join(dirpath, fn))
                source_files.append(os.path.join(dirpath, fn))
    corpus_files = sorted(corpus_files)
    source_files = sorted(source_files)

dut_types = {}
corpus_case_duts = {}
for path in corpus_files:
    rel = os.path.relpath(path, ROOT)
    text = open(path, encoding='utf-8').read()
    for m in CALL.finditer(text):
        dut_types.setdefault(m.group(1), []).append(rel)
    for m in CORPUS_CASE.finditer(text):
        corpus_case_duts.setdefault(m.group(1), []).append(rel)

CLASS = re.compile(r'(?m)^\s*(?:class|struct)\s+([A-Za-z_]\w*)[^\n{]*\{')


def composite_duts_in(path):
    text = open(path, encoding='utf-8').read()
    out = set()
    for m in CLASS.finditer(text):
        name = m.group(1)
        start = m.end()
        nxt = text.find('\n};', start)
        body = text[start:nxt if nxt > 0 else len(text)]
        if 'ch_module<' in body:
            out.add(name)
    return out


composite = {}
for path in source_files:
    rel = os.path.relpath(path, ROOT)
    for name in composite_duts_in(path):
        composite.setdefault(name, []).append(rel)

# 只保留**真在 dual-backend 语料里被调用**的复合 DUT
# ⚠️ DUT 名可能带命名空间（`chlib::X`）⇒ ⇒ 类名匹配 MUST 取**最后一段**。
corpus_composite = {}
for name, files in composite.items():
    simple = name.rsplit('::', 1)[-1]
    # ⚠️ **收集全部**同名 call-site（★ 早期用 `break` ⇒ ⇒ 只挂第一个 ⇒ ⇒ 第二个
    #    **从方向 ③ 的视野里消失** ⇒ ⇒ ⇒ 漏判）
    for dut in dut_types:
        if dut.rsplit('::', 1)[-1] == simple:
            corpus_composite.setdefault(dut, set()).update(files)

# ── 登记表 ────────────────────────────────────────────────────────────
registered = {}
exempt = {}
registry_path = os.path.join(ROOT, REGISTRY)
if os.path.isfile(registry_path):
    text = open(registry_path, encoding='utf-8').read()
    # ⚠️ **MUST 按 section 解析**（★ 关键修复）：豁免表里的行**首格是 DUT 名**
    #    （★ 与「登记清单」同形态）⇒ ⇒ 若不按 section 区分 ⇒ ⇒ 豁免行会被当成
    #    **登记** ⇒ ⇒ 方向 ① 会要求它有用例 ⇒ ⇒ ⇒ **豁免路径完全失效**
    #    ⇒ ⇒ ⇒ 而 delta Requirement ① 明文承诺「覆盖**或**显式豁免」两条出路
    #    ⇒ ⇒ ⇒ ⇒ 只剩一条 ⇒ ⇒ ⇒ 契约被冻成「无解的红」。
    section = ''
    for line in text.splitlines():
        if line.startswith('#'):
            section = line.lstrip('#').strip()
            continue
        if not line.strip().startswith('|'):
            continue
        cells = [c.strip() for c in line.strip().strip('|').split('|')]
        if len(cells) < 2:
            continue
        first = cells[0].strip('`')
        second = cells[1].strip('`') if len(cells) > 1 else ''
        if not re.fullmatch(r'(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*', first):
            continue
        simple = first.rsplit('::', 1)[-1]
        if '豁免' in section:
            exempt[simple] = second
        else:
            # ⚠️ **默认 = 登记**（★ 只有「豁免」section 是特殊的）⇒ ⇒ ⇒ 一个写在
            #    **别的** section 里的 DUT 名行**仍被当成登记** ⇒ ⇒ ⇒ 不会因为
            #    「放错 section」而**静默消失**（★ 静默消失 = 判据空转）
            registered[simple] = second

print('CORPUS_COMPOSITE=%d' % len(corpus_composite))
for name in sorted(corpus_composite):
    print('COMPOSITE\t%s\t%s' % (name, ','.join(corpus_composite[name])))
print('REGISTERED=%d' % len(registered))
for name in sorted(registered):
    print('REGISTERED\t%s\t%s' % (name, registered[name]))
print('CORPUS_CASES=%d' % len(corpus_case_duts))
for name in sorted(corpus_case_duts):
    print('CASE\t%s\t%s' % (name, ','.join(corpus_case_duts[name])))
print('EXEMPT=%d' % len(exempt))
for name in sorted(exempt):
    print('EXEMPT\t%s\t%s' % (name, exempt[name]))
PYEOF

analyze() {
    if [ -n "$EXTRA_CORPUS_ROOT" ]; then
        python3 "$WORK/analyze.py" "$PWD" "$REGISTRY" "$EXTRA_CORPUS_ROOT" 2>/dev/null
    else
        python3 "$WORK/analyze.py" "$PWD" "$REGISTRY" 2>/dev/null
    fi
}

run_main() {
    local out
    out="$(analyze)"
    if [ -z "$out" ]; then
        fail "分析器无输出（python3 不可用？⇒ 判据**空转**）"
        return 1
    fi
    local composite registered cases exempt
    composite="$(printf '%s\n' "$out" | awk -F'\t' '$1=="COMPOSITE"{print $2}')"
    registered="$(printf '%s\n' "$out" | awk -F'\t' '$1=="REGISTERED"{print $2}')"
    cases="$(printf '%s\n' "$out" | awk -F'\t' '$1=="CASE"{print $2}')"
    exempt="$(printf '%s\n' "$out" | awk -F'\t' '$1=="EXEMPT"{print $2}')"

    if [ -z "$composite" ]; then
        fail "分析器未找出任何**复合** DUT ⇒ 扫描口径坏了 ⇒ 判据**空转**"
        return 1
    fi
    if [ -z "$registered" ]; then
        fail "登记表为空或不可解析 ⇒ 判据**空转**"
        return 1
    fi

    local bad=0 d simple
    # ① 登记 → 用例
    for d in $registered; do
        if ! printf '%s\n' "$cases" | grep -qx -- "$d"; then
            fail "① 登记的 DUT '$d' **没有**对应的语料回归用例 ⇒ 登记与实现脱节"
            bad=1
        fi
    done
    # ② 用例 → 登记
    for d in $cases; do
        if ! printf '%s\n' "$registered" | grep -qx -- "$d"; then
            fail "② 语料回归用例涉及 DUT '$d' ⇒ **未登记** ⇒ 覆盖在暗中漂移"
            bad=1
        fi
    done
    # ③ ★ 语料 → 登记（**让栅栏不随时间退化**）
    for d in $composite; do
        simple="${d##*::}"
        if printf '%s\n' "$registered" | grep -qx -- "$simple"; then
            continue
        fi
        if printf '%s\n' "$exempt" | grep -qx -- "$simple"; then
            continue
        fi
        fail "③ 语料里的复合 DUT '$d' **既未登记也未豁免** ⇒ 新增语料时栅栏会静默退化"
        bad=1
    done
    # ③' **豁免项 MUST 确实存在于语料里**（★ 登记表自称有这条核对 ⇒ ⇒ 这里补上：
    #     ⇒ ⇒ 「为一个早已不存在的 DUT 豁免」会被抓住）
    local all_duts
    all_duts="$(printf '%s\n' "$out" | awk -F'\t' '$1=="COMPOSITE"{print $2}')"
    for d in $exempt; do
        simple="${d##*::}"
        if ! printf '%s\n' "$all_duts" | grep -qx -- "$simple"; then
            fail "③' 豁免的 DUT '$d' **不在语料的复合 DUT 集合里** ⇒ 豁免表成了垃圾桶"
            bad=1
        fi
    done

    if [ "$bad" -eq 0 ]; then
        ok "三向双向核对通过（登记 $(printf '%s\n' "$registered" | wc -l) · 复合 DUT $(printf '%s\n' "$composite" | wc -l) · 用例 $(printf '%s\n' "$cases" | wc -l)）"
    fi
    return $bad
}

run_self_test() {
    local rc=0
    local reg_backup="$WORK/registry.bak"
    [ -f "$REGISTRY" ] && cp "$REGISTRY" "$reg_backup"

    # ① 投毒：登记表**删一行** ⇒ ⇒ ①（登记→用例）+ ②（用例→登记）MUST 红
    if [ -f "$REGISTRY" ]; then
        grep -v '`Rv32iPcDut`' "$REGISTRY" > "$WORK/poison.md" && cp "$WORK/poison.md" "$REGISTRY"
        if run_main >/dev/null 2>&1; then
            fail "self-test[① 登记表删一行]：竟被判绿"
            rc=1
        else
            ok "self-test[① 登记表删一行] ⇒ 红"
        fi
        cp "$reg_backup" "$REGISTRY"
    fi

    # ② 投毒：登记表**加一个不存在的 DUT** ⇒ ⇒ ① MUST 红
    if [ -f "$REGISTRY" ]; then
        printf '\n| `NotARealDut` | `tests/nowhere.cpp` | T0 | — | — |\n' >> "$REGISTRY"
        if run_main >/dev/null 2>&1; then
            fail "self-test[② 登记表加一个不存在的 DUT]：竟被判绿"
            rc=1
        else
            ok "self-test[② 登记表加一个不存在的 DUT] ⇒ 红"
        fi
        cp "$reg_backup" "$REGISTRY"
    fi

    # ③ ★ 投毒：**唯一的第 ③ 方向**形态 —— 「新增了一个带子组件的 DUT，但它**既**没有
    #    登记**也**没有用例」⇒ ⇒ ① / ② **都抓不到**（① 要「登记了但没用例」；
    #    ② 要「有用例但没登记」）⇒ ⇒ ⇒ **只有** ③ 能抓 ⇒ ⇒ ⇒ 这条是「③ 方向
    #    **不是**摆设」的**唯一**证据。
    #    ⚠️ 手法：造一个**合成语料文件**（★ **不**污染仓库）。
    local extra="$WORK/extra_corpus"
    rm -rf "$extra"; mkdir -p "$extra"
    cat > "$extra/synthetic_composite_dut.cpp" <<'POOF'
#include "catch_amalgamated.hpp"
class SyntheticCompositeDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<SyntheticCompositeDut> child{"child"};
        child.io().din <<= io().din;
        io().dout <<= child.io().dout;
    }
};
DUAL_BACKEND_TEST_PAIR(SyntheticCompositeDut, "synthetic", "[synthetic]", [&]() {});
POOF
    EXTRA_CORPUS_ROOT="$extra"
    if run_main >/dev/null 2>&1; then
        fail "self-test[③ 新增未登记的复合 DUT]：竟被判绿 ⇒ 第 ③ 个方向是摆设"
        rc=1
    else
        ok "self-test[③ 新增未登记的复合 DUT] ⇒ 红（★ 只有第 ③ 个方向能抓）"
    fi
    EXTRA_CORPUS_ROOT=""
    rm -rf "$extra"

    # ④ 投毒：**豁免路径必须可用** —— 合成语料（一个**未登记**的复合 DUT）+ 把它
    #    **豁免** ⇒ ⇒ 方向 ③ MUST **不**触发 ⇒ ⇒ ⇒ 这是 delta Requirement ① 的
    #    「覆盖**或**显式豁免」两条出路的**第二条**的可用性证据。
    #    ⚠️ 语义：豁免 = 「**不覆盖**」⇒ ⇒ 被豁免的 DUT MUST **没有**用例
    #    ⇒ ⇒ 本投毒里的合成 DUT 天然没有用例 ⇒ ⇒ ⇒ ①②③ 都不该触发。
    local extra2="$WORK/extra_corpus_exempt"
    rm -rf "$extra2"; mkdir -p "$extra2"
    cat > "$extra2/synthetic_exempt_dut.cpp" <<'POOF'
#include "catch_amalgamated.hpp"
class SyntheticExemptDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<SyntheticExemptDut> child{"child"};
        child.io().din <<= io().din;
        io().dout <<= child.io().dout;
    }
};
DUAL_BACKEND_TEST_PAIR(SyntheticExemptDut, "synthetic", "[synthetic]", [&]() {});
POOF
    if [ -f "$REGISTRY" ]; then
        python3 - "$REGISTRY" "$extra2" <<'PYEOF2'
import re
import sys
path, extra = sys.argv[1], sys.argv[2]
text = open(path, encoding='utf-8').read()
# 在「豁免表」里插入一行（★ 必须按 section 定位 ⇒ ⇒ 插到「豁免」标题之后）
lines = text.split('\n')
out = []
in_exempt = False
inserted = False
for l in lines:
    out.append(l)
    if l.startswith('#') and '豁免' in l:
        in_exempt = True
        continue
    if in_exempt and not inserted and l.strip().startswith('|'):
        out.append('| `SyntheticExemptDut` | 临时豁免（自检：豁免路径可用） |')
        inserted = True
if not inserted:
    out.append('| `SyntheticExemptDut` | 临时豁免（自检：豁免路径可用） |')
open(path, 'w', encoding='utf-8').write('\n'.join(out))
PYEOF2
        EXTRA_CORPUS_ROOT="$extra2"
        if run_main >/dev/null 2>&1; then
            ok "self-test[④ 豁免路径可用（合成语料 + 豁免）] ⇒ 绿（★ delta 的第二条出路可用）"
        else
            fail "self-test[④ 豁免路径]：竟被判红 ⇒ delta 承诺的两条出路只剩一条"
            rc=1
        fi
        EXTRA_CORPUS_ROOT=""
        cp "$reg_backup" "$REGISTRY"
    fi
    rm -rf "$extra2"

    # ⑤ 投毒：豁免一个**不存在**的 DUT ⇒ ⇒ ③' MUST 红（★ 豁免表不是垃圾桶）
    if [ -f "$REGISTRY" ]; then
        python3 - "$REGISTRY" <<'PYEOF3'
import sys
path = sys.argv[1]
text = open(path, encoding='utf-8').read()
lines = text.split('\n')
out = []
in_exempt = False
inserted = False
for l in lines:
    out.append(l)
    if l.startswith('#') and '豁免' in l:
        in_exempt = True
        continue
    if in_exempt and not inserted and l.strip().startswith('|'):
        out.append('| `NotInCorpusAtAll` | 临时豁免一个**不存在**的 DUT（自检） |')
        inserted = True
if not inserted:
    out.append('| `NotInCorpusAtAll` | 临时豁免一个**不存在**的 DUT（自检） |')
open(path, 'w', encoding='utf-8').write('\n'.join(out))
PYEOF3
        if run_main >/dev/null 2>&1; then
            fail "self-test[⑤ 豁免一个不存在的 DUT]：竟被判绿 ⇒ 豁免表成了垃圾桶"
            rc=1
        else
            ok "self-test[⑤ 豁免一个不存在的 DUT] ⇒ 红（★ 豁免表不是垃圾桶）"
        fi
        cp "$reg_backup" "$REGISTRY"
    fi

    # ⑥ 反向对照：还原 ⇒ MUST 复绿
    if run_main >/dev/null 2>&1; then
        ok "self-test：还原后复绿"
    else
        fail "self-test：还原后仍红 ⇒ 判据自身有问题"
        rc=1
    fi
    return $rc
}

if [ "${1:-}" = "--self-test" ]; then
    run_self_test
    exit $?
fi

run_main
if [ "$fails" -ne 0 ]; then
    printf '\033[0;31mverify_hier_corpus_coverage: FAIL（%d 条违规）\033[0m\n' "$fails"
    exit 1
fi
printf '\033[0;32m✅ verify_hier_corpus_coverage: PASS（三向双向核对）\033[0m\n'
exit 0
