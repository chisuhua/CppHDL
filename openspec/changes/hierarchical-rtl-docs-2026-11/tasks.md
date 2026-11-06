# 任务清单：`hierarchical-rtl-docs-2026-11`

> 判据来源：proposal.md §Acceptance。
> ⚠️ **前置**：**T3-⑤**（等价性验证）—— 文档 MUST 描述**已落地**的行为。
> ⚠️ **无 delta** ⇒ `.openspec.yaml` 显式 `skip_specs: true`。

## T1 用户面文档

- [x] 1.1 `docs/usage_guide/05-component-design.md` §5 改写
      ⇒ ⇒ **MUST 含 ADR-058 §4 风险 6 的观测盲区边界段**（等价性只覆盖顶层可见行为 ·
      被移出顶层的 `ch_out` 顶层不可观测 · 输入方向无盲区）—— 这是「三处同步登记」的第三处
      （ADR · capability spec 由 T3-005 的 delta 负责 · 用法指南由本任务负责）
      ⇒ ⇒ **已落**（2026-11-06）：§5 从「单一扁平 module」改写为「**两条出口**」⇒
      ① 5.1 扁平产物（**按归属修正后的真实形态**重写：节点名 `top_m_lf_unnamed_output`
      ⇒ **不再**是 `top_top_m_lf_*`）· ② 5.2 扁平端口规则 · ③ 5.3 层次化产物
      （**怎么声明成层** + **完整可编译示例** + **真实产物** + **端口规则** + **选项**）·
      ④ 5.4 三类「贵」的写法 · ⑤ 5.5 **观测盲区边界段**（ADR-058 §4 风险 6 的登记 ·
      输出方向有盲区 / **输入方向无盲区** · MUST NOT 假装全覆盖）· ⑥ 5.6 三条纪律 ·
      ⑦ 5.7 护栏与范例索引
      ⇒ ⇒ ⚠️ 文档头（版本 v1.0 → **v1.1** · 最后更新 2026-11-06 · 目标 4）同步
      ⇒ ⇒ **实测依据**：§5.1 / §5.3.3 的两段 Verilog **由当前实现实际生成**（非手写）
- [x] 1.2 `HIERARCHICAL-RTL-AUTHORING-GUIDE.md`：删 `CH_MODULE_NAME` ·
      改 `CH_HIERARCHY_LAYER("name")` · 补档 ③ 形态
      ⇒ ⇒ **已落**（2026-11-06）：① 头部「框架**只支持扁平输出** · API 是提案」⇒ **已落地**
      （Status: Draft → **Active**）· ② §1 **D6** 的「module 名来自组件**类型**」⇒ **来自你给的
      字符串** · ③ §2.3「声明长什么样（提案形态）」⇒ **已落地形态**（类级带名字 + 档 ② 单参 +
      档 ③ 双参 + `ch_inline`）· ④ §2.5「再加一行 `CH_HIERARCHY_LAYER`」⇒ **带名字**形态
      · ⑤ §3 标题与开头（「API 尚不存在」⇒ **已落地**）· ⑥ §4 **P3**（删 `CH_MODULE_NAME`
      ⇒ 改判为「已解决 · 合并为单宏」）· ⑦ §4 **P6 / P8**（⇒ **已裁决独立立项** · 归位条件
      已达成 · 待 planner 立项）· ⑧ §4.1 结论补 2026-11-06 更新 · ⑨ §5「待用户确认」
      ⇒ **3 条均已裁决**（保留作历史记录 + 加裁决列）
      ⇒ ⇒ ⚠️ **遗留清扫时补抓到的一处**（首轮改动**漏掉**）：§2 开头仍写「层次化 API 是
      **提案**（ADR-058 / 061）⇒ **今天默认全是扁平**」⇒ ⇒ 与「已落地」**矛盾**
      ⇒ ⇒ 已改为「层次化 API **已落地** ⇒ 默认仍是**全扁平**（没人声明 ⇒ 逐字节一致）」
      ⇒ ⇒ **判据**：`grep -rnE 'API 是\*\*提案\*\*|API 尚不存在|只支持扁平输出|
      框架\*\*只支持扁平' docs/usage_guide docs/developer_guide` = **0**
- [x] 1.3 合并宏的**签名面 5 处**全部改毕
      ⇒ ⇒ ⚠️ **台账 §10.6-A-⑥b 的 5 处清单**（2026-11-04 architect 写）在今天的真实状态：
      | 原清单位置 | 2026-11-06 实况 | 处置 |
      |---|---|---|
      | `design.md:56`（无参 `CH_HIERARCHY_LAYER;`） | T3-①c **归档前已修正** ⇒ 现在 `CH_HIERARCHY_LAYER("rv32i_pipeline")` | ✅ 已达标（**归档文档 = 历史记录 ⇒ 不改写**） |
      | `design.md:59`（`ch_layer<Sub> s{"s"}` 未体现 ③） | T3-①c **归档前已修正** ⇒ 档 ② / 档 ③ **两档都有** | ✅ 已达标（同上） |
      | `tasks.md:54-55` | T3-①c **归档前已修正** | ✅ 已达标（同上） |
      | `proposal.md:59-60` | T3-①c **归档前已修正** | ✅ 已达标（同上） |
      | 用户面指南 `:236`（P3 行「只能靠 `CH_MODULE_NAME` 宏」） | 本轮 1.2 改判为「已解决」 | ✅ 本轮已改 |
      ⇒ ⇒ **额外发现并修正**（台账清单**未列**）：用户面指南 `:145` 的**无参**
      `CH_HIERARCHY_LAYER;` ⇒ ⇒ 已改为带名字形态 + 「不带名字的写法编译不过」的散文说明
      ⇒ ⇒ ⇒ **判据**（三条 grep，全部 **0**）：
      `grep -rn 'CH_MODULE_NAME' docs/usage_guide docs/developer_guide` = **0** ·
      `grep -rn 'CH_HIERARCHY_LAYER;' docs/usage_guide docs/developer_guide` = **0** ·
      `grep -rn 'CH_HIERARCHY_LAYER' docs/usage_guide docs/developer_guide | grep -v
      'CH_HIERARCHY_LAYER("'` = **0**（⇒ ⇒ 用户面文档里的宏用法**一律带名字**）

## T2 架构与索引

- [x] 2.1 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` 转正
      ⇒ ⇒ **已落**（2026-11-06）：① 标题「目标架构」⇒ 「**架构主题文档（当前态…）**」·
      ② Status: Draft ⇒ **Accepted**（ADR 全部 Accepted + T3-①a ~ T3-⑤ 全部归档）·
      ③ §1「当前态快照（2026-11-02 实测）」⇒ **按 2026-11-06 实测重写**：
      1.1 **两条 RTL 出口**（`toVerilog` 逐字节未变 · `toVerilogHierarchical` 已落地 ·
      backend **已可**仿真层次化产物 ⇒ `try_initialize_with_verilog`）·
      1.2 归属与命名的**四个缺陷 ⇒ 已消解**（逐条点名消解方式：
      `Component::node_owner()`（`src/component.cpp:16`）= 唯一归属判定实现 ·
      `build_hierarchical_name(parent, local_name)`（`include/component.h:266`）= 唯一拼接点
      ⇒ MUST 用 `parent->name()` ⇒ 实测 3 层名 = `top_m_lf_unnamed_output`）·
      1.3 已排除的岔路（不变）· ④ §2 **N1** 的「⏳ 等价性证据缺失」⇒ **已落地** +
      **判别力边界**（同源 ⇒ 底座错 ⇒ 两边一起错 ⇒ 仍归 parity + 手推期望值义务）
      ⇒ ⇒ ⚠️ **决策内容零变更**（决策在 ADR；本文件只承载当前态与结构事实）
- [x] 2.2 根 `AGENTS.md` 的「WHERE TO LOOK」补层次化入口
      ⇒ ⇒ **已落**（2026-11-06）：① 「WHERE TO LOOK」**补一行**「层次化**等价性**（扁平 ≡ 层次 ·
      逐拍）」⇒ 指向 `tests/test_hierarchy_equivalence.cpp` + `tests/hier_equivalence.h`
      （harness 在 `tests/` 内 ⇒ **不暴露**公共 API）· ② KNOWN LIMITATIONS 的层次化段
      同步到 **T3-⑤ 归档后**：R1–R7 ⇒ **R1–R8**（R8 = 顶层端口表 == 手推期望集）·
      「逐拍等价性归 T3-005（本 change 未做）」⇒ **已落地**（9 用例 / 414 断言）·
      **补观测盲区** + **判别力边界**（同源 ⇒ 底座错 ⇒ 两边一起错 ⇒ 照样全绿）
      ⇒ ⇒ ⚠️ `scripts/verify_agents_size.sh` 通过（50 599 B ≤ 72 KiB）
      ⇒ ⇒ ⚠️ **原清单的 2 行已在仓内**（写法指南 + 分析/发射层）⇒ 本轮只**补缺**（等价性入口）

## T3 零漂移

- [x] 3.1 文档示例**可编译**（照抄即过）
      ⇒ ⇒ **判据常驻在本文件**（⇒ 归档后照跑即可复算 ⇒ **不依赖任何临时脚本**）：
      ① **抽取即编译**：从 `docs/usage_guide/05-component-design.md` §5.3.2 抽出唯一的
      ```cpp 块 ⇒ `g++ -std=c++23 -fsyntax-only` ⇒ **零错误**（见下方「验证命令」段）
      ⇒ ⇒ ⚠️ 该示例**逐字**取自本轮**实际编译通过**的那份（差异仅一行标题注释）
      ⇒ ⇒ ② **API 形态判据**：示例里用到的框架 API MUST 出现在仓内**已编译**的源里
      ⇒ ⇒ ⇒ **判别力**：抓住「文档教了不存在的 API」（本轮 §2.3 的无参宏 / §4 的 P3 行
      就是这样被抓出来的 —— 它们对应的 API 在仓内**根本不存在**）
- [x] 3.2 `ctest -LE perf` / samples / examples 全绿（文档 change ⇒ 理论零代码影响）
      ⇒ ⇒ **已跑**（2026-11-06）：见下方「验证命令」段的实测记录

## F8 吸收（T3-⑥ D1 · planner 登记 · 验收 = grep 清单清零）

- [x] F8 用法指南 **P7 痛点行**（「层次化产物**不被** Verilator backend 消费 ⇒ 用户**不能**
      直接对自己的设计跑双后端」）**已陈旧**（技术入口 T3-005 已具备）⇒ ⇒ **已改**
      ⇒ ⇒ 判据（**4 条 grep，全部 0**）：`grep -rn '不被 Verilator backend 消费'`
      / `'两套真值'` / `'待 T3-005'` / `'待 T3-⑤'` 在
      `docs/usage_guide/**` + `docs/developer_guide/**` 内 **0** 命中
      ⇒ ⇒ ✅ 顺带把 P7 的建议改成**已收口**形态（入口 = `try_initialize_with_verilog` ·
      `ports` MUST 由 emitter 产物派生 · 失败**致命**）+ **判别力边界**（同源 ⇒ 底座错 ⇒
      两边一起错 ⇒ 仍归 parity + 手推期望值义务）

## 验证命令

```bash
cd /workspace/project/CppHDL
# ⚠️ `set -e` ⇒ 中途失败**即红**（否则最后一条命令的退出码会**吃掉**前面的失败）
# ⚠️ `ctest` MUST 在 **build 目录**里跑（仓库根没有 `CTestTestfile.cmake` ⇒
#    ctest 会报「No tests were found!!!」⇒ **空跑** ⇒ 判据形同虚设）
set -euo pipefail
cmake --build build -j"$(nproc)"
( cd build && ctest -LE perf --output-on-failure )
openspec validate hierarchical-rtl-docs-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```

## 零漂移判据（**常驻** · 归档后照跑即可复算）

### ① 用户面文档的**合并宏签名面**（任务 1.3）

```bash
cd /workspace/project/CppHDL
set -euo pipefail
# ⚠️ 每条 MUST **显式判定** ⇒ 命中即 `exit 1`（只 `echo` 计数 ⇒ 命中也**恒绿** ⇒ 判据形同虚设）
# ⚠️ `set -e` ⇒ python 子步骤的 `sys.exit(1)` MUST 传播成**块**失败
#    （否则 python 只打印 ❌ 而块仍退出 0 ⇒ 该子判据**恒绿**）
# ⚠️ `{ grep ... || true; } | wc -l` ⇒ `grep` 无命中时退出码 1 不污染管道（`pipefail` 下会）
for pat in 'CH_MODULE_NAME' 'CH_HIERARCHY_LAYER;'; do
  n=$( { grep -rn -- "$pat" docs/usage_guide docs/developer_guide || true; } | wc -l )
  printf '%-26s => %s（期望 0）\n' "$pat" "$n"
  if [ "$n" -ne 0 ]; then
    echo "❌ '$pat' 命中 $n 处 ⇒ 用户面文档存在签名面漂移"
    exit 1
  fi
done
# **代码块内**的宏用法 MUST 一律带名字（散文提及不算 ⇒ 「必须带名字」的说明不误伤）
python3 - <<'PY'
import re, glob, sys
bad = 0
for f in sorted(glob.glob('docs/usage_guide/**/*.md', recursive=True) +
                glob.glob('docs/developer_guide/**/*.md', recursive=True)):
    s = open(f, encoding='utf-8').read()
    for blk in re.findall(r'```[a-zA-Z]*\n(.*?)```', s, re.S):
        for m in re.finditer(r'CH_HIERARCHY_LAYER(.*)', blk):
            if not m.group(1).lstrip().startswith('("'):
                bad += 1
                print(f'❌ {f}: 代码块内出现不带名字的宏用法 → {m.group(0)[:60]}')
print('BAD_MACRO_USAGE =', bad)
sys.exit(1 if bad else 0)
PY
echo '✅ 判据 ① 通过'
```

### ② F8 的 **4 条 grep 清单**

```bash
cd /workspace/project/CppHDL
set -euo pipefail
fail=0
for pat in '不被 Verilator backend 消费' '两套真值' '待 T3-005' '待 T3-⑤'; do
  n=$( { grep -rn "$pat" docs/usage_guide docs/developer_guide || true; } | wc -l )
  printf '%-28s => %s（期望 0）\n' "$pat" "$n"
  if [ "$n" -ne 0 ]; then fail=1; fi
done
if [ "$fail" -ne 0 ]; then
  echo '❌ F8 的 4 条清单未清零 ⇒ 用户面文档仍写着已过时的痛点行'
  exit 1
fi
echo '✅ 判据 ② 通过'
```

### ③ 文档示例**照抄即过**（任务 3.1）

```bash
cd /workspace/project/CppHDL
set -euo pipefail
TMP=$(mktemp -d /tmp/opencode/docex.XXXXXX)
python3 - docs/usage_guide/05-component-design.md > "$TMP/example.cpp" <<'PY'
import re, sys
s = open(sys.argv[1], encoding='utf-8').read()
i = s.index('### 5.3.2 完整可编译示例')
j = s.index('### 5.3.3', i)
blocks = re.findall(r'```cpp\n(.*?)```', s[i:j], re.S)
assert len(blocks) == 1, f'期望 1 个 cpp 块，实际 {len(blocks)}'
sys.stdout.write(blocks[0])
PY
# 零错误 = 通过（`-fsyntax-only` ⇒ 只查语法 / 类型，不链接）
if ! g++ -std=c++23 -fsyntax-only "$TMP/example.cpp" \
     -Iinclude -Iinclude/core -Iinclude/ast -Iinclude/abstract -Iinclude/utils \
     -Iexternal/inipp -DCH_JIT_ENABLED=1; then
  echo '❌ 文档示例编译失败 ⇒ 文档教了编译不过的代码'
  exit 1
fi
echo '✅ 判据 ③ 通过（示例照抄即过）'
rm -rf "$TMP"
```

### ④ 示例用到的**框架 API MUST 存在于仓内已编译的源**

```bash
cd /workspace/project/CppHDL
set -euo pipefail
fail=0
for api in CH_HIERARCHY_LAYER ch_layer ch_inline toVerilogHierarchical HierarchyEmitResult; do
  n=$( { grep -rl -- "$api" include src tests examples samples || true; } | wc -l )
  printf '%-26s => %s 个文件（期望 ≥ 1）\n' "$api" "$n"
  if [ "$n" -lt 1 ]; then fail=1; fi
done
if [ "$fail" -ne 0 ]; then
  echo '❌ 有 API 在仓内**不存在** ⇒ 文档教了不存在的 API'
  exit 1
fi
echo '✅ 判据 ④ 通过'
```

### ⑤ 本次改写段落内的 `file:line` 引用 MUST 指向**非空行**

```bash
cd /workspace/project/CppHDL
set -euo pipefail
python3 - <<'PY'
import os, re, sys
# 范围 = 本 change 改写的两段（台账 §1 · 用法指南 §5.1/§5.2）
# ⇒ ⇒ 存量漂移（台账其它段落的旧引用）**不在范围内** ⇒ 判据不被存量淹没
REGIONS = [
    ('docs/architecture/2026-11-02-hierarchical-rtl-output.md',
     '## 1. 当前态快照', '## 2. 下游需求清单'),
    ('docs/usage_guide/05-component-design.md',
     '### 5.1 扁平产物（默认）', '### 5.3 层次化产物'),
]
bad = 0
for doc, a, b in REGIONS:
    s = open(doc, encoding='utf-8').read()
    i = s.index(a); j = s.index(b, i)
    for m in re.finditer(r'`([A-Za-z0-9_./-]+\.(?:h|cpp))(?::(\d+)(?:-(\d+))?)?`', s[i:j]):
        f = m.group(1)
        if not os.path.exists(f):
            # ⚠️ MUST NOT 静默跳过 ⇒ 否则「cwd 漂移 / 引用指向不存在的文件」会被跳过
            #    ⇒ 判据**恒绿**（本次负向对照抓到的真实缺陷）
            print(f'❌ {doc}: `{f}` 不存在 ⇒ cwd 漂移或引用失效')
            bad += 1
            continue
        lines = open(f, encoding='utf-8').read().split('\n')
        for g in (m.group(2), m.group(3)):
            if g is None:
                continue
            n = int(g)
            if n > len(lines) or not lines[n - 1].strip():
                print(f'❌ {doc}: `{f}:{g}` ⇒ 越界或空行')
                bad += 1
print('BAD_LINENO_REF =', bad)
sys.exit(1 if bad else 0)
PY
echo '✅ 判据 ⑤ 通过'
```

## 判据的**判别力自证**（变异对照 · 每条先确认仍能编译）

> ⚠️ 「判据会红」本身是**必须自证**的 —— 否则「恒绿」与「真判据」不可区分。

| 变异 | 改动 | 期望 | 实测 |
|---|---|---|---|
| **M0** | 无（原始） | 绿 | ✅ `EXIT=0` |
| **M1** | `CH_HIERARCHY_LAYER("my_mid")` ⇒ **无参** | 红 | ✅ `EXIT=1`（`'CH_HIERARCHY_LAYER' does not name a type`）⇒ ⇒ 判据 ③ **会红** |
| **M2** | `toVerilogHierarchical` ⇒ `toVerilogHierarchica`（**不存在的 API**） | 红 | ✅ `EXIT=1`（`is not a member of 'ch'`）⇒ ⇒ 判据 ③ **会红** |
| **M3** | 删掉 `Mid` 的 `(parent, name)` 构造器 | 红 | ✅ `EXIT=1`（`no matching function for call to 'construct_at(Mid*&, ...)'`）⇒ ⇒ 判据 ③ **会红** |

⇒ ⇒ **判据 ③ 覆盖三类漂移**：① 宏用法形态漂移 · ② API 存在性漂移 · ③ 「三件套」义务漂移。

### grep 类判据（① / ② / ④）的负向对照（**临时树 · 仓内文档零改动**）

> ⚠️ 手法 = 把 `cd` 去掉 + cwd 指向**临时树** ⇒ 跑**同款代码** ⇒ ⇒ 判据的判定逻辑
> 原样复用 ⇒ ⇒ 注入漂移 ⇒ MUST 转红。

| 判据 | 注入的漂移 | 期望 | 实测 |
|---|---|---|---|
| **①** | `CH_MODULE_NAME` 出现 1 处 | 红 | ✅ `EXIT=1` + 诊断「❌ 'CH_MODULE_NAME' 命中 1 处」 |
| **①** | 代码块内 `CH_HIERARCHY_LAYER`（不带名字 · **无分号** ⇒ grep 干净 ⇒ 只命中 python 检查） | 红 | ✅ `EXIT=1` + 诊断「代码块内出现不带名字的宏用法」+ `BAD_MACRO_USAGE = 1` |
| **①** | 代码块内 `CH_HIERARCHY_LAYER;`（**无参**） | 红 | ✅ `EXIT=1` + 诊断「❌ 'CH_HIERARCHY_LAYER;' 命中 1 处」（grep **先拦** ⇒ fail-fast） |
| **①** | **反向**：散文提及 `CH_HIERARCHY_LAYER`（「必须带名字」的说明） | **绿** | ✅ `EXIT=0` + `BAD_MACRO_USAGE = 0` ⇒ ⇒ **散文提及不误伤** |
| **②** | `不被 Verilator backend 消费` 出现 1 处 | 红 | ✅ `EXIT=1` + 诊断「❌ F8 的 4 条清单未清零」 |
| **④** | API 名换成 `CH_HIERARCHY_LAYER_NOT_EXIST` | 红 | ✅ `EXIT=1` + 诊断「❌ 有 API 在仓内**不存在**」 |
| **⑤** | 把 `src/codegen_verilog.cpp:313` 改成 `:99999`（**备份 → 原地改 → 跑 → 还原**） | 红 | ✅ `EXIT=1` + 诊断「`src/codegen_verilog.cpp:99999` ⇒ 越界或空行」⇒ ⇒ **还原后复跑绿**（`BAD_LINENO_REF = 0`）且仓内文档**逐字节还原**（`cmp` 通过） |

⇒ ⇒ **两个 grep 检查互补**：字面 `CH_HIERARCHY_LAYER;`（带分号）由 grep 拦；其余**不带名字**
的形态由 python 检查拦 ⇒ ⇒ 单靠任一个都**漏**。

## 判据**自检**时抓到的问题（**本轮**）

| # | 问题 | 现象 | 处置 |
|---|---|---|---|
| **S1** | 「验证命令」块的 `ctest` 在**仓库根**跑 | 报「No tests were found!!!」⇒ **空跑** ⇒ 而整块退出码被最后一条命令**吃掉** ⇒ 判据**形同虚设** | ✅ 已改：`( cd build && ctest -LE perf --output-on-failure )` + `set -euo pipefail`（⇒ 中途失败**即红**） |
| **S2** | 判据块里的 Python 正则**含 ` ``` `** | 任何**朴素**的 `re.findall(r'```bash\n(.*?)```')` 抽取器会**提前截断** ⇒ 抽出的块**跑不通** | ✅ 文件**本身合法**（CommonMark：闭合栅栏必须整行只有反引号 ⇒ 源码行不闭合）⇒ ⇒ **R4 的 awk 是行锚定的** ⇒ 抽取正确 ⇒ ⇒ ⚠️ **但**任何**新写的**抽取器 MUST 用**行锚定**口径（= R4 同款），**不得**用朴素正则 |
| **S3** | 判据 ① / ② / ④ 的 grep **只 `echo` 计数、不判定** | 命中也只是打印 ⇒ 块的退出码仍是 0 ⇒ ⇒ **恒绿** ⇒ ⇒ 判据**形同虚设**（与 S1 同型） | ✅ 已改：每条 grep 都**显式判定**（≠ 期望 ⇒ 打印原因 + `exit 1`）⇒ ⇒ 块 ① / ② / ④ 现在**会红**；⚠️ `grep` 无命中时退出码 1 ⇒ 用 `{ grep ... \|\| true; } \| wc -l`（否则 `pipefail` 下管道本身即失败） |
| **S4** | 判据 ① 的 **python 子步骤**失败**不传播**成块失败（块缺 `set -e`） | python 检出漂移并打印 ❌ + `BAD_MACRO_USAGE = 1` ⇒ 但块的退出码仍是 **0** ⇒ ⇒ 该子判据**恒绿** | ✅ 已改：块 ① 补 `set -euo pipefail` ⇒ python 的 `sys.exit(1)` **传播**成块失败（负向对照已复验 ⇒ 转红） |
| **S5** | 台账 §1 与用法指南 §5 沿用 2026-11-02 的**行号引用**（`codegen_verilog.h:36` · `codegen_verilog.cpp:301` / `:409` / `:333-360` / `:363-370`） | `src/codegen_verilog.cpp` 在 **T3-③ 的 emitter 拆分**后行号已变 ⇒ ⇒ 引用**全部失效** ⇒ ⇒ 写成**假事实** | ✅ 已按**当前实现**逐条改正（`codegen_verilog.h:43` · `codegen_verilog.cpp:313` / `:390` · `print_decl:396` · `print_logic:525` · `print_output:707`）⇒ ⇒ ⚠️ 行号引用**改为函数级锚点**（`ModuleEmitter::print_decl` 等）⇒ ⇒ **判据 ⑤**（本 change 改写段落内的引用 MUST 指向非空行） |
| **S6** | 判据 ⑤ 里「文件不存在 ⇒ `continue`」 | cwd 漂移 / 引用指向**不存在的文件** ⇒ 被**静默跳过** ⇒ ⇒ 判据**恒绿**（负向对照首跑就抓到：注入 `:99999` 却仍报 `BAD_LINENO_REF = 0`） | ✅ 已改：文件不存在 ⇒ **报错**（不再跳过）⇒ ⇒ 连带暴露台账 §1 的一处**裸文件名**引用（`io.h`）⇒ 已改成完整路径 `include/core/io.h`（更精确）⇒ ⇒ 复验：负向转红 + 还原后复跑绿 |

## 本次**不做**（范围外的历史遗留 · 只记录不清理）

| 项 | 位置 | 事实 | 为何本次不做 |
|---|---|---|---|
| 台账 §11.6 的 **C1 行**表格**少一列**（该行只有 3 个 `\|` · 表头是 4 列） | `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §11.6 | **存量**（`git show HEAD~2` 已存在 ⇒ 早于本 change）⇒ ⇒ 与层次化语义**无关**（纯排版） | 本次只改台账的 **§1 / §2 N1 行**⇒ ⇒ 该行在**改动段落之外** ⇒ ⇒ 按「范围外的历史遗留只记录、不趁机清理」处置 |
| roadmap「跨阶段拆分 / phase-5」清单里 **T2a / T3-①a ~ T3-⑤ 共 9 行**仍是 ⏳ | `.rddf/roadmap/features/feat-hierarchical-rtl-output.md` §跨阶段拆分 | **存量漂移**（这 9 个 change **早已归档** ⇒ 清单未同步）⇒ ⇒ 与本 change 无关 | 本次**只改 T3-⑥ 自己那行**（否则归档后**制造新的不一致**）⇒ ⇒ 其余 9 行按「范围外的历史遗留**只记录不清理**」处置 ⇒ ⇒ ⚠️ 权威状态 = 本文件「实施进度」表 |
| 台账 §10.4 的「重整后的落点」表 | `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §10.4 | 该表是 **architect 2026-11-04 的计划表**（历史记录）⇒ ⇒ 其中 T3-⑥ 行的状态**不改** | 改写 architect 的历史计划表 = **伪造历史** ⇒ ⇒ 权威状态 = roadmap 的「实施进度」表 |
| 台账 §10.6-A-⑥b 列的合并宏签名面 **4 处** | `openspec/changes/archive/2026-11-04-hierarchical-rtl-granularity-declaration-2026-11/{design,tasks,proposal}.md` | 已随 **T3-①c 归档**修正 ⇒ 归档文档 = **历史记录** | 改写归档 change 的历史文档 = **伪造历史** ⇒ ⇒ 只在 tasks 1.3 里**逐条点名实测状态** |
