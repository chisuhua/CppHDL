# Design: roadmap-fragments-consistency-guard-2026-10

> 本 change 交付**一个护栏脚本** + CI 挂载 + **一个新 capability 契约** + 文档簿记。
> ⚠️ **有 spec 变更**（新增 capability `roadmap-bookkeeping-consistency`）
> ⇒ 归档**不得**用 `--skip-specs`（仓内每个 CI 护栏都有 capability 契约）。

## 1. 根因（**实测订正**）

初稿写：「CLI 的 flag 存在，但**脚本 payload 缺失**
（`skills/roadmap/scripts/roadmap_update_agent_md.sh` 不存在）」⇒ **两头都反了**。

### 1.1 实测事实（2026-10-28）

| 事实 | 实测 | 怎么验的 |
|---|---|---|
| 脚本 payload | **存在且可用** | 工具链的 `skills/roadmap/scripts/roadmap_update_agent_md.sh`（1656 B，可执行）存在；`roadmap_refresh_fragments.sh` 同理 |
| 能否直接跑 | **能** | 绕开 CLI dispatch 直接调用生成器 ⇒ `OK AGENTS.md updated: 8 feature fragments rendered` ⇒ 产出与当前已提交块**逐字一致** |
| CLI dispatch | **缺** | `rddf roadmap --help` 只暴露 `init / status / edit / validate / advance` ⇒ `--update-agent-md` **flag 不存在** ⇒ 脚本**不可达** |
| 工具的依赖 | 在**仓外** | 生成器依赖 `rdd-workflow/_lib/roadmap_state_wrapper.py` ⇒ **CI runner 拿不到** ⇒ 护栏必须**自包含** |

⇒ ⇒ **真正的根因 = CLI 没把已有的脚本接出来**（工具侧职责），**不是**「脚本缺失」。

### 1.2 这个订正如何改变设计

既然生成器**可用且可用作 oracle** ⇒ 护栏**不必**自己重新实现一套
「解析已提交块 + 逐字段比对」的逻辑 ⇒ 照抄生成器的策略：
**渲染期望块 ⇒ 与已提交块逐字比对**（见 §2.1）⇒ 少一套解析器 ⇒ 误报面更小。

⚠️ **但 CI 拿不到工具链** ⇒ 渲染规则必须 **vendored** ⇒ 于是需要一个
**一次性**的等价性验证（§3）来钉住「vendored 规则在该工具链版本处与工具一致」。

## 2. 判据

### 2.1 C1 —— AUTO 块守恒（渲染 + 逐字比对）

**渲染规则**（复刻 `_lib/roadmap_state.py::update_agent_md`）：

1. 扫描 `.rddf/roadmap/{phases,features,archive}/*.md`；
2. front-matter 用**同一套朴素 YAML** 解析（`---` 分隔；`k: v`；
   `[a, b]` 列表字面量按**深度 0** 的逗号切分并剥引号）；
   ⚠️ **重复键 ⇒ 抛错**（工具自身会抛 `ValueError`）⇒ 护栏 **fail-closed**；
3. 取 `kind == "feature"` 的（**默认 kind 是 `phase`** ⇒ 漏写 `kind` 的文件
   不会被误算进 AUTO 块）；`主题` 键才是 theme（**`theme` 键不被工具读**）；
4. 按 `id` 排序 ⇒ 渲染表头 / 分隔行 / 每行
   `| \`id\` | status | refs | theme |`；
   `phase_refs` 用 `, ` 连接（空 ⇒ `(none)`）；theme 的 `|` 转义为 `\|`；
5. 与根 `AGENTS.md` 里两个哨兵之间的**已提交文本逐字比对**。

⚠️ **为什么是「渲染 + 比对」而不是「双向解析 + 字段比对」**：
前者只需**一套**解析器（front-matter）⇒ 误报面更小；后者要额外解析**已提交块**
的 Markdown 表格（转义 / 列数 / 空行 / 续行）⇒ 多一类边界 ⇒ 多一类误报。

⚠️ **fail-closed 的点**：哨兵缺失 / front-matter 损坏 / 重复键 /
`.rddf/roadmap/**` 下没有 feature ⇒ 一律**报违规**（不静默跳过）——
「静默跳过」会让闸门形同虚设（本仓护栏反复登记的失败模式）。

⚠️ **软着陆的一类**：`.rddf/roadmap/` **目录不存在** ⇒ 判为**环境性缺失**
（稀疏检出 / 下游 vendor / fork）⇒ `::warning` + exit 0（与
`verify_test_counts.sh` 的「无 build dir ⇒ warning + exit 0」同型）。

### 2.2 C2 / C3 —— feature 文件**内部**的完成度一致性

**三个比对面**（同一个 feature 文件内）：

| 面 | 来源 | 提取方式 |
|---|---|---|
| ① | `### phase-*` 段的 bullet | 行首符号 + 文本 |
| ② | `## 验收标准` 的 checkbox | `[x]` / `[ ]` + 文本 |
| ③ | **正文里的 Markdown 表格行** | 行内**唯一**一个以状态符号开头的单元格 |

**键**（一个键 = 一条「完成度声明」）：

- `label:Phase-N` —— 文本**行首**是 `Phase N`；
- `change:<name>` —— 文本**行首**是反引号包裹的**已知 change 名**。

**已知 change 名**的注册表 = `openspec/changes/**` 的目录名，收三种形态：
目录名本身 / 去归档日期前缀 / 再去**月**粒度日期后缀
（⚠️ 本仓 change 名后缀是 `-2026-10`（**月**粒度），归档目录前缀是 `2026-10-28-`
（**日**粒度）⇒ 两种都要能剥）。
⚠️ **只收目录名 ⇒ 语料零命中 = 死判据** —— 仓内 feature 文件引用的是**裸名**
（如 `dual-backend-rv32i-integration`）⇒ 实测修复后命中 **25** 个真实引用。

**判定**：

- 完成 = `✅` / `☑` / `[x]`；未完成 = `⏳` / `🟡` / `[ ]`；
  **其余符号一律跳过**（不得猜测语义）；
- ⚠️ **只在行首比对** —— 这是**零误报的关键**：验收标准里常出现
  「`- [ ] 单元测试 verilator 覆盖 7% → 100%（Phase 3 续 + Phase 5）`」
  这种**行中**提及 ⇒ 若按「出现即比对」会与「Phase 3 的 bullet 是 ✅」**冲突**
  ⇒ **误报** ⇒ 实测该形态在 `feat-dual-backend-coverage.md` 里**真实存在**；
- 表格行：状态单元格**唯一**且**单元格内只有一个符号** ⇒ 否则**歧义 ⇒ 跳过**
  （实测语料里存在「🟡 Sync variant ✅ parity」这种**一格两符号**的单元格）；
- 同一个键在**任一面**出现多个不同符号 ⇒ **跳过**（并打 `::notice`）
  ⇒ 「无法判定」不等于「违规」；
- 同一个键在**两个及以上面**出现且完成度不同 ⇒ **违规**（打印各面的取值）。

## 3. oracle 等价性（**本地**，不进 CI）

工具链版本：`rdd-workflow@96af1629c8994fc079e100817c8540172c21226e`
（`_lib/roadmap_state.py::update_agent_md` / `_parse_fragment_file` /
`load_fragments`）。

复现命令：

```bash
cp AGENTS.md /tmp/AGENTS_oracle.md
PROJECT_ROOT="$PWD" MODE=update-agent-md \
  AGENTS_MD_PATH=/tmp/AGENTS_oracle.md FRAGMENTS_DIR="$PWD/.rddf/roadmap" \
  python3 /workspace/main/rdd-workflow/_lib/roadmap_state_wrapper.py
# 再把工具输出与护栏的渲染结果逐字比对
```

⇒ 在该 SHA 处：**护栏渲染 == 工具生成 == 已提交块**（三者逐字一致）。

⚠️ **结论的边界（勿过度解读）**：这**只**证明「在该工具链版本处一致」，
**不**证明「以后一直一致」⇒ 护栏的 vendored 规则会随工具链演进而**静默漂移**
⇒ **再验证触发条件**：① `rdd-workflow` 升级 ⇒ MUST 重跑 oracle；
② 护栏的渲染规则有任何改动 ⇒ MUST 重跑 oracle。
⇒ 护栏脚本头部也写明了这条（后人无从判断复刻是否仍成立 ⇒ MUST 留在代码里）。

⚠️ **C1 报「AUTO 块不一致」时的正确动作**：**同步护栏的 vendored 规则**
（若工具链确实变了）或修 front-matter ⇒ **不要**手工改 `AGENTS.md` 的 AUTO 块
—— 那正是本护栏要治的根因。

⚠️ **oracle 不得进 CI** —— 工具链在**仓外**（`rdd-workflow`）⇒ CI runner 拿不到
⇒ 护栏必须自包含 ⇒ oracle 只在**开发期**跑一次，结论记在本文件。

## 4. 实测证据

### 4.1 当前仓库：零误报

```bash
bash scripts/verify_roadmap_fragments.sh          # ⇒ exit 0
bash scripts/verify_roadmap_fragments.sh --self-test   # ⇒ 13/13
```

### 4.2 **真实历史漂移**被抓（`git archive` 取自历史）

`3436ba4`（2026-10-27 roadmap 审查**之前**的真实历史状态，整体 checkout）：

```
VIOLATION feat-dual-backend-coverage：'label:Phase-4' 的完成度符号冲突 —— phase bullet=todo，验收标准 checkbox=done
❌ verify_roadmap_fragments: 有违规
```

⇒ 那次漂移的形态 = phase-5 bullet 写 `🟡 Phase 4: RV32I partial（…）`
vs 验收标准写 `- [x] Phase 4 partial ship（…）` ⇒ **C2 抓到** ✓
⇒ 且该历史状态下 **C1 是绿的**（AUTO 块与 front-matter 当时一致）
⇒ 证明 **C1 与 C2/C3 覆盖的是两类不同的漂移**，缺一不可。

### 4.3 ⚠️ **判不了的漂移（如实登记）**

`feat-rv32i-integration.md` 在同一历史版本里的漂移是**叙事性**的：
phase-5 bullet 写 `🟡 \`dual-backend-rv32i-integration\` partial（… Batch 2 \`IfIdPipelineReg\` ×2 clk-spike …）`，
而验收标准写 `- [ ] \`dual-backend-rv32i-integration\` change archived`
⇒ **两侧符号一致（都是未完成）** ⇒ 任何「符号一致性」判据都**判不出来**
⇒ 真正过时的是**内容**（「Batch 2 需要 clk-spike」已被读码推翻）。

⇒ ⇒ **本护栏只覆盖「符号」与「可机械判定的引用」这两类漂移**；
**叙事性陈旧仍靠人工 + 代码评审**兜住（根 `AGENTS.md` 的命令注释 + capability
spec 的 Scenario 里都已写明）。

⚠️ 这也是**为什么 self-test 里要有「叙事性陈旧」那个 fixture**：它钉住
「护栏不会把判不了的东西判成违规」。

### 4.4 变异验证（护栏自身的判据被改坏 ⇒ MUST 被 self-test 抓到）

| 变异 | 结果 |
|---|---|
| C1 的比对改成恒真 | ✅ self-test 转红 |
| 完成度比较改成恒真 | ✅ self-test 转红 |
| 「行首比对」改成「出现即比对」 | ✅ self-test 转红 |
| change 名注册表退回「只收目录名」 | ✅ self-test 转红（fixture ⑨） |
| `sed -i` 回退（不可移植形态） | ✅ **在 BSD sed 模拟器下转红**（见 §6） |

### 4.5 可移植性取证（BSD sed 模拟器）

用一支模拟 BSD sed 的 `sed`（`-i` 必须带后缀 ⇒ `sed -i 's/x/y/' file` **不改写文件**）：

- **修复后的版本**（fixture 变异全用 `python3` heredoc）⇒ **13/13 全绿** ✓
- **含 `sed -i` 的版本** ⇒ fixture ②「AUTO 块 status 不符」**红** ✓
  ⇒ 坐实初版会让 **macos-14 腿必红**（初版在 GNU sed 上是全绿的 ⇒ 本地测不出来）

## 5. CI 挂载

- `build-and-test` job（`Verify AGENTS.md test counts` 之后）+ `pr-verilator-fast` job
  ⇒ 两处都挂 **检查** + **`--self-test`**（与
  `verify_no_duplicate_rv32i_headers.sh` 同款：既跑闸门也跑它自己的 fixture）。

## 6. 已知边界

| 边界 | 说明 |
|---|---|
| **叙事性陈旧**判不了 | §4.3 ⇒ 人工 + 代码评审 |
| CI 拿不到工具链 | 护栏自包含；oracle 只在开发期跑（§3）⇒ 工具链升级时 MUST 重跑 |
| `kind` 缺省 = `phase` | 漏写 `kind: feature` 的文件**不会**进 AUTO 块 ⇒ 护栏与工具**一致**（不是 bug） |
| `phase_refs` 写成标量 | 工具会 `", ".join(str)` ⇒ 逐字符连接 ⇒ 护栏**照抄**该行为（保持等价） |
| 只管 feature | `phases/` 与 `objectives/` 的 AUTO 块**不在**本护栏范围（工具的 objectives 哨兵在本仓 AGENTS.md 里**不存在**） |
| 「无 front-matter」的口径差 | 工具 `_parse_fragment_file` 对**无 front-matter** 的文件是**静默跳过**；护栏判**违规** ⇒ 这是**有意**的偏差（护栏 fail-closed 优先）⇒ 已登记 |
| 表格行的状态列定位 | 取「行内**唯一**一个以状态符号开头的单元格」⇒ 多于一个 ⇒ 歧义 ⇒ 跳过 |

## 7. 对抗审稿第 1 轮的处置（2026-10-28）

**审稿结论 = REJECT**（plan 档 + build 档各 1 个 BLOCKER；合计 3 BLOCKER +
7 SHOULD_FIX + 5 NIT；`mergeable: no`）⇒ 本节记录**逐条**处置。

| # | 类别 | 摘要 | 处置 |
|---|---|---|---|
| P-B1 | **BLOCKER** | proposal 的判据集合 / 编号 / 覆盖声明与 design+实现**三方不一致**，且 Acceptance 要求抓「**两处**」真实漂移 —— 而其中一处**判不了** ⇒ AC **不可兑现**（归档会冻一份与实现不符的承诺） | ✅ **已闭合**：proposal 重写为「**三条**判据（C1/C2/C3）+ **三个比对面**（bullet / checkbox / 表格行）」；Acceptance 改为「抓到**可机械判定**的那一处」+「另一处如实登记为人工项」；并新增一张「可机械判定 / 不可机械判定」的对照表 |
| B-B1 | **BLOCKER** | self-test 用 GNU-only `sed -i` ⇒ macOS(BSD sed) 上 3 个 fixture 的变异**不发生** ⇒ `--self-test` 在 **macos-14 腿必红** ⇒ CI 必红；且违反仓内明文规则（`verify_no_duplicate_rv32i_headers.sh` 的 `sed_inplace()`） | ✅ **已闭合**：3 处 `sed -i` 全改为 `python3 - "$file" <<'PY'`（仓内既有写法）⇒ 并用 **BSD sed 模拟器**取证：修复后 13/13 全绿、旧版转红（design.md §4.5） |
| B-B2 | **BLOCKER** | C3 的「已知 change 名」注册表键空间与语料**不匹配**（注册表收目录名 `2026-10-27-<name>-2026-10`，语料引用**裸名** `<name>`）⇒ 该判据在真实语料上**零命中** = 死判据；fixture ⑨ 用的是人工目录名 ⇒ 只证机制不证适用性 | ✅ **已闭合**：注册表收录三种形态（目录名 / 去归档日期前缀 / 去**月**粒度日期后缀）⇒ 实测语料命中从 **0 → 25**；fixture ⑨ 改用**真实语料形态**的名字（`dual-backend-demo` ← 归档目录 `2026-10-20-dual-backend-demo-2026-10`） |
| P-S1 | SHOULD_FIX | oracle 结论**过度断言**（「证明不漂移」）+ **未记工具链 SHA** ⇒ 工具链演进后护栏会静默分叉 | ✅ **已闭合**：§3 改为「在 `<SHA>` 处一致」+ 记录 SHA + 写明**再验证触发条件**；护栏脚本头部也写明同一件事 |
| P-S2 | SHOULD_FIX | 「不涉及 ⇒ 无 spec 变更 ⇒ `--skip-specs`」与**仓内惯例**不符（每个 CI 护栏都有 capability 契约）⇒ 归档后该闸门**无 spec 承载** | ✅ **已闭合**：新增 capability `roadmap-bookkeeping-consistency`（ADDED ×4）⇒ `.openspec.yaml` 的 `skip_specs: true` **已删除** ⇒ 归档**不得**用 `--skip-specs` |
| P-S3 | SHOULD_FIX | §6 已知边界**漏登记**：(a) 表格整表不参与比对（proposal 却写成比对的一侧）；(b) 「文件无 front-matter」护栏判违规 vs 工具静默跳过 | ✅ **已闭合**：(a) **改为实现表格第三面**（不再是不参与 ⇒ 提案与实现一致）；(b) §6 新增一行，明示这是**有意**的偏差（fail-closed 优先） |
| P-S4 | SHOULD_FIX | 「当前仓库零误报」「CI 绿」这类**实测日志**写在 design 正文 | ✅ **已闭合**：实测结论留在 §4（标注为「实测证据」），design 正文只保留**判据与裁决** |
| B-S1 | SHOULD_FIX | 静默跳过路径未登记（缺 phase 段 / 验收标准段 ⇒ 整文件跳过且**无 notice**） | ✅ **已闭合**：改为「跳过 ⇒ 打 `::notice`」⇒ 当前仓库 5 个 feature 会打印跳过说明（**豁免不是静默的**） |
| B-S2 | SHOULD_FIX | 该闸门**没有任何软着陆路径** ⇒ `.rddf/roadmap/` 缺失的检出上会**硬红**且报错不可操作 | ✅ **已闭合**：区分「目录不存在」（环境性 ⇒ `::warning` + exit 0，且打印可操作说明）与「目录存在但内容不可解析」（真漂移 ⇒ fail-closed）⇒ self-test fixture ⑬ 钉住 |
| B-S3 | SHOULD_FIX | 根 `AGENTS.md` 的 COMMANDS 缺「C1 红了怎么办」⇒ 贡献者只能手工改 AUTO 块（**回到根因**） | ✅ **已闭合**：COMMANDS 注释写明「护栏打印的 diff 即期望块 ⇒ 正确动作是同步 vendored 规则 / 修 front-matter，**不要**手工改 AUTO 块」 |
| B-S4 | SHOULD_FIX | C2 的键名 `phase-N` 与 roadmap 的阶段 id（`phase-4.md`）**同名不同义** | ✅ **已闭合**：键名改为 `label:Phase-N`（与 `change:<name>` 区分） |
| B-S5 | SHOULD_FIX | vendored 规则与工具的一致性只在开发期验过一次 ⇒ 后人无从判断复刻是否仍成立 | ✅ **已闭合**：护栏脚本头部写明「vendored 自 `rdd-workflow@<SHA>` 的 `_lib/roadmap_state.py::update_agent_md`」+ **再验证触发条件** |
| N-1 | NIT | fixture ⑧ 里的 `s.replace(..., ...)` 是**自我替换**（no-op） | ✅ 已删（该行本就无作用） |
| N-2 | NIT | `--root` 缺参数 ⇒ bash 报 `unbound variable`（无友好提示，退出码与「有违规」的 1 撞车） | ✅ 已改：缺参数 ⇒ 打印提示并 **exit 2**（与「有违规」的 1 区分） |
| N-3 | NIT | `--self-test` 与 `--root` 同给 ⇒ `--root` 被**静默忽略** | ✅ 已改：打印 `::notice` 说明「自测在自建的合成仓库里跑」 |
| N-4 | NIT | `known_change_names` 对 `archive/` 只扫一层 ⇒ 将来嵌套会静默漏 | ✅ 已改：`os.walk` **递归**收集 |
| N-5 | NIT | CI 两处 step 的注释逐字重复 | ✅ 记录在案：仓内既有 step 也是逐 job 注释（同一纪律）⇒ 不改 |

⚠️ **未采纳**：审稿建议「若坚持覆盖表格，则补表格解析」—— **已采纳**（实现表格第三面），
因为原漂移 #1 的形态本就包含表格行。
