---
id: phase-6
kind: phase
status: active
phase_refs: []
主题: [层次化父侧落点收口, mem 端口实参跨层支持, RV32I ISA 缺口, PortPolicy 三档分化, 已知功能缺口清零]
---

## phase-6 概览

Phase 6（**2026-11-09 激活**）是**「收口 + 新缺口」窗口**。

**为什么现在开这个窗口**：架构裁定线（台账 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7
的 20 行残留）**已全部终态** —— 9 `已收口` · 5 `已裁决` · 4 `不做` · 2 `已知` ⇒ ⇒ **不再需要新的架构裁定**
（唯一例外 = **P4** 的一次**范围**裁定）⇒ ⇒ 本窗口的工作是
「把**已签批**的决策落地」+「把**已登记**的缺口变成可执行的 change」。

⚠️ **2026-11-09 订正**：本窗口**新开**了一条架构裁定线 = **P13**（上游 issue #28 的
「析构异常规格 vs 可组合性」⇒ ADR-067）⇒ ⇒ 上句的「不再需要新的架构裁定」**已被证伪**
⇒ ⇒ 但**新增的裁定是「新缺口引入的」**（不是台账 §7 那 20 行的延续）⇒ ⇒ 台账 §7 的 20 行**不受影响**
⇒ ⇒ **P4**（mem 端口范围裁定）仍是台账侧唯一的例外。

**承接**：
① `2026-11-09-hierarchical-parent-side-landing-implementation`（父侧落点语义落地 · 已归档）
—— 它的 **3 项直接遗留** = 本窗口的 **P1 / P3–P5**；
② ⚠️ **phase-5 在 2026-10-26 修订里已把两条转交本窗口**（**既有约定，不是新裁定**）：
「ADR-036 逻辑算子的**物理移除**」（日历锁 · 最早 2027-04-17）⇒ ⇒ **P11**；
「Windows-2022 CI 启用」⇒ ⇒ ⚠️ **该条已被 2026-11-03 用户裁决关闭**（CI 平台范围 =
**仅 Ubuntu**）⇒ ⇒ **P2b**（只登记理由 + 关闭，不实施）。

⚠️ **2026-11-09 修订**（本轮 · planner 域 · 用户口令「按评审结论修订」）：
① **文档缺陷 9 项**已订正（重复行 / 陈旧结论 / 数字漂移 / 口径冲突 ⇒ 逐条见 §「本轮修订记录」）；
② **P4–P10 的任务行**按实测证据改写（**出口判据**与**前置**多处重排）；
③ ⇒ ⇒ **「做不做」有 3 处变了**（其余任务集合不变 = P4 / P5 / P6 / P7 / P8 / P9 / P10 + P11 日历锁）⇒ ⇒
**P10** 由「实施」改为「**登记已知漂移 · 不实施**」· **P8** 删去 architect 步 · **P9** 的 ① ② 由「登记为已知」
改为「**删死代码**（真实现**另立**）」⇒ ⇒ 其余各步**只改「怎么做 / 怎么判」**。

---

## ★ 后续任务清单 + 实施路径（**planner 域** · 2026-11-09）

> **本窗口的**后续任务**内容 + 实施路径** = `.rddf/roadmap/phase-6-next-steps.md`
> （**planner 域** · 2026-11-09 · 上游 = architect 中期复盘
> `docs/architecture/2026-11-09-phase-6-midterm-review.md` · ADR-066 **v1.4** / ADR-060 **v1.21** 事实订正）。
> ⇒ ⇒ 该文件按**可执行性**排序 T1–T8（**主线** T1 = P5 收尾 · T3/T4/T5 **可并行**）
> ⇒ ⇒ **本文件（本 phase-6.md）仍是任务状态的唯一真相**；next-steps 文件**不复制**状态格。
> ⚠️ **2026-11-09 architect 实测订正的影响**（已同步进本文件 P5 行 + change 文档）：
> ① P5 **判据 ②**（P4 裁「不支持」分支）= **死分支**（决策 ⑧ 已裁定「支持」）⇒ ⇒ 只保留为
> 「新裁定推翻决策 ⑧ 时的备选落点」；② P5 **判据 ⑥**（V20 探针）的**预期值** = **分析层已响亮拒绝**
> ⇒ ⇒ **无**契约-实现缺口 ⇒ ⇒ 探针的**实质交付物** = **(i) 与 (ii) 的归因分开**；
> ③ P5 **判据 ⑤**（归属比对判定）的**理由**改写 = **独立于边结构** + **归因精确** + **三桶可判**
> ⇒ ⇒ **不是**「补一条缺失的拒绝」⇒ ⇒ MUST **保留**；④ `(ii)` 的父侧落点网**由既有机制自动合成**
> ⇒ ⇒ **S3 不需要写产品代码** ⇒ ⇒ 交付物 = **产物级断言**。

---

## 任务路径（**执行顺序**）

> ⚠️ **顺序约束（2026-11-09 修订）**：**P5 ⇒ P7 的强制串行已解除** —— 逐**函数**核对写点：
> P5 = `src/hierarchy_analysis.cpp`（mem 闸门）+ `src/codegen_verilog.cpp`（`print_mem_read_port` 及其操作数解析）·
> P7 = `src/hierarchy_verilog.cpp`（policy 分支）+ `src/codegen_verilog.cpp`（`print_header` / `print_logic` 的产物注释）
> ⇒ ⇒ ⚠️ 两者在 `codegen_verilog.cpp` 上**文件级重叠**、**函数级不重叠** ⇒ ⇒ 结论 = **可并行**
> （若同批合入，**rebase 基点 = P5** ⇒ ⇒ 合并冲突落在 `codegen_verilog.cpp`）⇒ ⇒ 其余各步**互不阻塞**，可并行。
> ⚠️ **真实阻塞链只有一条**：**P3（improvement 草案）→ P4（architect 裁定）→ P5（实施）** ⇒ ⇒ ⚠️ **2026-11-09 更新**：**P4 已闭** ⇒ ⇒ **P5 可开工**；**P7 / P8 的 architect 步均已闭** ⇒ ⇒ builder 可直接开工（✅ **2026-11-09 更新**：P5 的「正确支持」支**已由 ADR-066 v1.3 决策 ⑧ 裁定** ⇒ ⇒ **不再**是
「仍待独立裁定」⇒ ⇒ 且经 v1.4 实测订正：**S3 不需要写产品代码**（父侧落点网由既有机制自动合成））
> ⇒ ⇒ **P4 是 architect 域**（ADR-066 已 Accepted ⇒ 改其决策内容 MUST 切 architect ⇒ builder 不得单方面改）。
> ⚠️ **P7 的 architect 步不可省**（产物注释的精确文本 = 契约面）⇒ ⇒ 但**范围**收窄为两件小事（见 P7 行）。

| 步 | 任务 | 归属 | 前置 | 出口判据（**机械可判**） | 状态 |
|---|---|---|---|---|---|
| **P1** | 补 **A8 面 ①** 的机械证据：「`node_names_[` 的 **62 行 / 64 次**无需改动」⇒ ⇒ **grep 清单 + 一次负向注入**（把某处名称换成**未在父作用域声明**的名字 ⇒ lint 必红） | builder | — | 证据脚本**可复跑**；负向注入使 **R9 lint 必红**、还原后复绿 | ✅ **2026-11-09** ⇒ 证据脚本 = `scripts/verify_node_names_single_source.sh`（R1 清单 62 行/64 次 · R2 单写入点 · R3 命名真源同源 · R4 **源码级**注入 ⇒ R9 必红 ⇒ 还原复绿 · `--self-test` **5/5**）· 记录 = `.rddf/reviews/hierarchical-parent-side-landing-implementation-2026-11.md` §A–§H · CI = `build-and-test` guard 段（只读 + `--self-test`；`--inject` 不上 CI） |
| **P2** | ★ **roadmap 事实核对**（★ 范围比预期更广，见下） | planner | — | ① feature 与 phase 的状态**逐条**与 `AGENTS.md`/capability/归档件一致；② `verify_roadmap_fragments.sh` 绿；③ **phase-5 的 `status` 可据实转 `done`** | ✅ **2026-11-09** ⇒ 核对记录 = `.rddf/reviews/phase-6-p2-roadmap-audit-2026-11.md`（逐条工具证据 · 9 feature + 6 phase + 4 objective）· 改动 = `phases/phase-5.md`（**转 `done`** + 概览表 4 行 + 完成条件 3 勾 1 不做）+ `features/feat-rv32i-isa-completeness.md`（审计快照注记）+ 本文件漂移清单订正（**① / ② 实测已不成立** · 新增 **⑥**）· 护栏 `verify_roadmap_fragments.sh`（+`--self-test`）/ `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 全绿 · ⚠️ **AUTO 块本轮无需改**（未动任何 feature 的 `status`/`phase_refs`/`主题` ⇒ 该义务是**条件触发**） |
| **P2b** | ⚠️ **Windows-2022 CI**（phase-5 于 2026-10-26 转交）⇒ ⇒ **2026-11-03 用户裁决：CI 平台范围 = 仅 Ubuntu** ⇒ ⇒ **不做** ⇒ ⇒ 只**登记理由 + 关闭该完成条件**（不动 CI） | planner | — | phase-5 完成条件里该项标为「不做（附裁决日期）」 | ✅ **2026-11-09** ⇒ `phases/phase-5.md` 的完成条件已改为 `- [x] **不做**（2026-11-03 用户裁决）` + No-Go 判据的处置说明 · **CI 文件零改动**（实测 `runs-on` = 5×`ubuntu-22.04` + 1×`${{ matrix.os }}`，其取值 `['ubuntu-22.04']`） |
| **P3** | **mem 端口实参跨层**（台账行 20 的下游）：起草 improvement（5 段 · Why / What Changes / Acceptance / Capabilities / Impact）⇒ ⇒ 过 `pre_create_brainstorm_check.sh` HARD-GATE | planner | — | improvement 文件存在且过 HARD-GATE（exit 0） | ✅ **2026-11-09** ⇒ 草案 = `.rddf/improvements/mem-port-arg-cross-layer-2026-11.md`（**7 段**（架构依据 / 范围 / Why / What Changes / Acceptance / Capabilities / Impact）+ YAML frontmatter · **HARD-GATE exit 0**）· 已注册进 `improvement-suggestions.md` · ⚠️ 状态 = `draft` ⇒ ⇒ **待 P4 的 architect 范围裁定**落地后才可转 `approved` ⇒ 才可立项为 change · ★ **口径（用户 2026-11-09 裁决）**：「粒度拆分 + 归因订正」= **无条件必做**；「正确支持」这一支 = **条件化**于 P4 的边界句（⇒ ⇒ 本 improvement **不替 architect 拍板**） |
| **P4** | 同上：**architect 范围裁定** ⇒ ⇒ ★ **边界句 MUST 写死** ——「mem 持有 module **==** 使用 module、仅**实参视线**跨层」⇒ ⇒ 否则 ADR-066 决策 ⑥「跨模块 `ch_mem` 仍拒」会被逐案重解释 | architect | P3 | ① **可开工前置**：本条**无未决子项**（P3 草案已存在 ⇒ ⇒ 可直接开工）；② ADR-066 §2.7 的「`:543` 闸门 **MUST 维持**响亮拒绝」**整句改写**为两分句（(i) mem **本体**跨 module ⇒ 拒；(ii) mem **本体**同 module、仅**实参视线**跨层 ⇒ **不属**本闸门）—— ⚠️ 只**追加**边界句会与原句**并存** ⇒ ADR 自相矛盾 ⇒ ⇒ 判据 = **双向 grep（markdown 不敏感 + 限定 §2.7 区间）** —— 正：`awk '/^### 2.7/,/^---/' ADR-066 \| grep -c '实参视线'` ≥ 1；负：`awk '/^### 2.7/,/^---/' ADR-066 \| grep -c '维持.*响亮拒绝'` == 0；锚词（**markdown 不敏感**）：(i) 分句含 `本体.*跨 module`、(ii) 分句含 `实参视线` ⇒ ⇒ 两者 `grep -c` 各 ≥ 1（⚠️ 裸字面 `本体跨 module` 会被 `**本体**跨 module` 的强调符打断 ⇒ ⇒ 假失败） —— ⚠️ **旧句在原文带 markdown 强调符**（`MUST **维持**响亮拒绝`）⇒ ⇒ 裸 `grep 'MUST 维持响亮拒绝'` **恒为 0**（判据恒真 ⇒ 检不出缺陷）⇒ ⇒ MUST 用区间 + `维持.*响亮拒绝` 模式；③ **ADR-060 §2.1 矩阵补格**（**逐字**含边界句）—— ⚠️ ADR-060 已 **Accepted** ⇒ ⇒ 补格归 **architect（本步）** ⇒ ⇒ builder **MUST NOT** 改（improvement In Scope 4 已相应改标）；④ 两份 ADR 的**元数据加修订行**（版本 bump + 日期 + 角色）+ 审稿记录落 `.rddf/reviews/`；⑤ **oracle → critic** 双审 **0 BLOCKER** | ✅ **2026-11-09** ⇒ ADR-066 **v1.2**（`docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md`）⇒ ⇒ §2.7 改**三桶**（(i) mem **本体**跨 module ⇒ 响亮拒绝 · (ii) 同 module + 仅 `addr`/`en`/`wdata` **实参视线**跨层（且实参来源**相邻**）⇒ 不属该闸门 · (iii) **非相邻**实参 ⇒ 仍按 `:563` 拒 ⇒ 三者**互斥且穷举**）+ **边界句（逐字）**「mem 持有 module == 使用 module、仅`实参视线`跨层」+ **节点级判别程序**（持有 = `port->parent()` 本体的折叠后归属 · 使用 = 端口节点自身归属 · (i) ⇔ ①≠② · (ii) ⇔ ①==② + 三槽 + **相邻**）+ **产物形态归属**（本 ADR **不裁定** (ii) 支持态的产物形态 ⇒ MUST 走独立修订）+ 选项 **D/E/F** + §4（负面 1 / 风险 2）+ §5 **V18/V19/V20** + 元数据（修订 v1.2 · 超长自检 **414 行** · AC 映射暂缺 · `backlog: mem-port-arg-cross-layer-2026-11`）⇒ ⇒ ADR-060 **v1.17** ⇒ §2.1 矩阵**补格**（逐字含边界句）+ 既有「跨模块 `ch_mem`」行**收窄**为「**mem 本体**跨 module」+ §2.4.1「合成不适用」格收窄 + §5 效果判据 + §6⇒ ⇒ 审稿 = `.rddf/reviews/ADR-066-parent-side-landing.md` **§8**（oracle 9 条 + critic 1 BLOCKER / 3 SHOULD_FIX / 3 NITS **逐条应答** ⇒ 复审 **APPROVE**）⇒ ⇒ **判据自检**：§2.7 内旧措辞 `维持.*响亮拒绝` = **0**（⚠️ **修订前实测 = 1** ⇒ 判据**不恒真**）· `实参视线` = **5** · `非相邻 ⇒ (iii)` = **1** · ADR-060 §2.1 边界句 ≥ **1** · `§2.2 G3` 字面 = **0**⇒ ⇒ 提交 `4b3def4` ⇒ ⚠️ **遗留（不属 P4 · 已登记给 P5）**：**字面 (i) 的现状未实测**（oracle 判据 = 分析放行 + 发射期静默丢端口 ⇒ ⇒ 由 **P5 的 V20 探针**登记）+ 分析层 `:542` 代码注释（权威链首环）随 P5 同步 |
| **P5**（**change 已立项** = `openspec/changes/mem-port-arg-cross-layer-2026-11/` · AC1–AC10 + 实施顺序 S0–S6） | 同上：**实施** —— 分析层 `:543` 的**粒度拆分**（mem 端口**实参**跨层 vs 真跨模块访问）+ 归因订正（诊断 MUST 给出**正确**改法）+ **条件化**的 mem 端口正确支持（✅ **已裁定** —— ADR-066 **v1.3 决策 ⑧**（第二次裁定）= 复用决策 ① 的父侧落点网（**由登记触发**）· **命名恒等** ⇒ codegen 不查表 · **R2 表 +1 行 = 登记义务** · `en` 槽 fail-closed ⇒ ⇒ 「正确支持」**已可实施**；⚠️ **2026-11-09 实测订正**（ADR-066 **v1.4**）：`(ii)` 的父侧落点网**由既有机制自动合成** ⇒ ⇒ **登记义务天然满足** ⇒ ⇒ **S3 不需要新代码** ⇒ ⇒ 交付物 = **产物级断言**（AC3 值判据 + AC4 声明断言））+ **A15**（端口**读值非 0**，不是只 lint） | builder | P4 | **① 共同**：跨模块 mem 访问**仍被拒**（负向用例转红）· 诊断文案**不含**「把 mem 与使用者放同层」（当 mem **本体**已在同层时）· 台账行 **20** 闭口（登记表：状态 `已裁决 → 已收口` · `归属` → `—` · `闭口证据` = archive 路径（含 `proposal.md`）· 台账 §7 派生同步 · 护栏绿）；**② P4 裁「不支持」时**（⚠️ **2026-11-09 订正**：本分支**已成死分支** —— ADR-066 **v1.3 决策 ⑧**已裁定「**支持**」⇒ ⇒ 本分支的**前提消失** ⇒ ⇒ **本轮不再适用**；⚠️ **保留在文中的唯一用途** = 记录「若将来有**新裁定**推翻决策 ⑧ ⇒ 则 MUST 落点二选一（**任一即可**）：**分析层响亮拒绝**（更早 · 更响 · 归因更正）**或** **发射器 collect 处 fail-closed**（`r.ok == false` **且** `file_exists == false`）⇒ ⇒ 负向用例 MUST 转红」⇒ ⇒ ⚠️ **不得**被读成「当前仍需在两条落点里二选一」）；**③ P4 裁「支持」时追加**：mem 端口**读 addr** 与**写 wdata/en** 两侧 MUST 断言**精确期望值**（A15 · **值**判据 · 期望值**手推** ⇒ ⇒ 「非 0」只是**下界** ⇒ ⇒ MUST 断言**确定值**，否则错值只要非 0 即过）⇒ ⇒ MUST 覆盖**两处** = 分析层拆分点 + 发射层 `print_mem_read_port`（否则「分析层放行、发射层仍 early-return」会复现「静默不发射」）；**④ 边界清单**（孙组件仍拒 / 父层自身节点不得误报 / 读写不同侧 / 多个 `ch_mem` / `aread`+`sread` 两类端口 / lint 绿 ≠ 值对）MUST 落进 **P3 的 improvement 草案**（本文件只管顺序与门槛 ⇒ 不复制实现规格）；**⑤ 逐字遵守 ADR-066 v1.2 的判别程序**（三桶 + **归属比对判定**：持有 = `port->parent()` 本体的折叠后归属 · 使用 = 端口节点自身归属 · (ii) MUST 带**相邻**约束）⇒ ⇒ **新增**：字面 (i) 的**归属比对**判定（⚠️ **2026-11-09 实测订正**（ADR-066 **v1.4**）：现行 `:542-553` 的**边闸门**对字面 (i) **可达**（mem **端口**节点的 src 列表**含 mem 本体** ⇒ ⇒ 该边**跨 module** ⇒ ⇒ **现状 = 已响亮拒绝** ⇒ ⇒ **无**契约-实现缺口）⇒ ⇒ 该判定的价值 = **独立于边结构** + **归因精确** + **三桶可判** ⇒ ⇒ **不是**「补一条缺失的拒绝」⇒ ⇒ MUST **保留**（否则判别程序 ①②③ 落空）· **不是**只改单条件）；**⑥ V20 探针（P4 遗留 · 承重）**：① **运行期探针**登记字面 (i) 的**现状处置** —— ⚠️ **2026-11-09 实测订正**（ADR-066 **v1.4**）：**预期 = 分析层**已**响亮拒绝**（**不是**「分析放行 + 发射期静默丢端口」⇒ ⇒ **无**契约-实现缺口）⇒ ⇒ ⚠️ **禁止**把「现状 = 已拒绝」表述为「新增了拒绝能力」② 结果 MUST 登记进 **台账行 20** **与** `.rddf/checks/arch-residual-registry.md` 的**归因描述**（★ 探针的**实质交付物** = **(i) 与 (ii) 的归因分开**：(i) 说「mem 本体跨 module」· (ii) 说「仅实参视线跨层」）③ 且 MUST 确认新判定**不放松**该拒绝（负向用例**仍红**）⇒ ⇒ ⚠️ 探针**仍必要**（删掉会失去归因登记载体 + 不放松护栏）；**⑦ 代码注释同步**：分析层 `:542` 的注释（权威链**首环**）MUST 随判定拆分改（分 (i)/(ii)）；**⑧ V18 用 P5 样本复算**：`awk '/^### 2.7/,/^---/' ADR-066 \| grep -c '维持.*响亮拒绝'` == **0** · `实参视线` ≥ 1（⇒ ⇒ 与 P4 自检**同源**） | ⬜ |
| **P6** | ★ **RV32I ISA 缺口**：**CSR**（`csrrw`/`csrrs`/`csrrc`）+ **`ECALL`/`EBREAK`** + **`FENCE`**（不含预测取指）⇒ ⇒ **独立立项**（`AGENTS.md` KNOWN LIMITATIONS 明写「不在任何仓内 capability 覆盖内 ⇒ 不可顺手塞进本条」）· ⚠️ **2026-11-09 实测订正**：**CSR / exception 的硬件已存在**（`include/cpu/riscv/rv32i_csr.h` 的 `CsrBank` · `include/cpu/riscv/rv32i_exception.h` 的 `ExceptionUnit` · 用例 `examples/riscv-mini/tests/test_csr_unit.cpp` 已注册 ctest）⇒ ⇒ 缺口在**指令级集成**（`OP_SYSTEM` 仅在 `rv32i_decoder.h:52` 定义、**零使用**；pipeline/stages 无 trap/mret 端口）⇒ ⇒ **P6-0 侦察先行**（清点复用面 ⇒ MUST NOT 重复实现） | planner → builder | — | ① **P6-0 侦察**：复用面清单（复用 / 新增）+ **`FENCE` 裁决**（单 hart + in-order + 无写缓冲 ⇒ 可否实现为 NOP）落进新 capability 的 spec；② 固件覆盖三类语义（**复用** `CsrBank`/`ExceptionUnit`）⇒ ⇒ 判据 = **新 ctest target**（如 `tests/test_rv32i_isa_csr.cpp`）· 逐条**手推期望值**断言（禁只抄仿真输出 —— 两后端同源 ⇒ parity 对该族缺陷**结构性失明**）· `CTEST_TOTAL` 锚点同步 · `OP_SYSTEM` 的**剥注释后**引用数从 0 变正（与 `verify_public_api_visibility.sh` 的 tsv 探针**同口径** ⇒ ⇒ 禁裸 `grep -c` 计数注释）**且** 命中落在 `rv32i_decoder.h` **与** `id_stage.h` / `ex_stage.h` / `mem_stage.h` **任一**（⇒ ⇒ 排除「只多一行注释」）· 「手推期望值」的 verifier 判法 = **随机抽 1 条断言核对其期望值非抄仿真输出**· 双后端 parity = **辅助**证据；③ capability 新增 | ⬜ |
| **P7** | **台账行 15**：三档 `PortPolicy` 在**可合成形态**上的语义**分化**（`Reject` 拒可合成形态 / `SynthesizeFatal` 先合成后失败 / 默认 `SynthesizeWarn` 的 CHWARN + 产物注释义务）· ⚠️ **实测**：**失败路径**唯一行为点 = `src/hierarchy_verilog.cpp:956` `all = (policy != Reject)` + `:984` CHWARN ⇒ `SynthesizeFatal` 与 `SynthesizeWarn` **行为完全相同**；**成功路径**（`synthesized_ports` 的声明/连接发射点）**无**策略读取、**无** CHWARN、**无**产物注释 ⇒ ⇒ 三档**未分化** | architect → builder | ⚠️ **可与 P5 并行**（写点**函数级**不重叠 ⇒ 详见 §顺序约束；**rebase 基点 = P5**） | ① **architect 步 ✅ 已闭（2026-11-09）** ⇒ ADR-060 **v1.18**（§2.4.1.1 产物注释四件事 + v1.7/v1.8 措辞对齐 + §5 判据 + §6 + 元数据）· 审稿 `.rddf/reviews/ADR-060-port-policy-v1.18.md`（oracle 9 条 + critic 第 1 轮 REJECT（1 BLOCKER / 2 SHOULD_FIX / 4 NITS **全修**）⇒ 复审 **APPROVE**）· 提交 `c0658d0` · **契约要点** = ① 触发**结构化**（对象 = `synthesized_ports` ∪ `synthesized_wires` 全体成员 + 闭合句；冒泡端口 / 注入 clock/reset **不算**）② 文本 `// CppHDL-synth: implicit port <name> (ADR-060 2.4.1)` / `// CppHDL-synth: net <name> (ADR-060 2.4.1)`（**4 空格**）③ 位置（端口表内该条目**正上方单独成行** · 插入点 = 合成端口发射循环内 ⇒ **MUST NOT** 复用 `port_notes` 查表）④ 每合成物**恰好一处** + 只含名字（确定性）· **新增义务** = 成功路径**每条被合成的边一条 `CHWARN`**（现状**零**）· **两条计数分别判**（1 边 ⇒ k 端口 + 1 网 ⇒ 「计数一致」**不可满足**）；② builder 步：同一**可合成**样本（⚠️ MUST **≥ 2 条**可合成边 ⇒ 否则 `Fatal` 与 `Reject` 的诊断边数同为 1 ⇒ 不可区分）上三档**两两可区分** ⇒ ⇒ 判据 = 元组 `(产物是否存在, 诊断列出的边数, 产物注释是否存在)`：`Warn = (true, N, 有)` · `Fatal = (false, N, —)` · `Reject = (false, 1, —)` 且 **N ≥ 2** —— ⚠️ 现样本 `TwoOpTop` 是**矩阵外不可合成**边 ⇒ 三档**恒失败** ⇒ 判不了分化 ⇒ MUST 换样本；③ **行为点** = 失败路径（policy 分支）**+** 成功路径（`synthesized_ports` 的声明/连接发射点 ⇒ 新增策略读取 + CHWARN + 产物注释）；④ 文档/注释义务落地 + 台账行 15 闭口（`已知 → 已收口` · `归属` → `—` · `闭口证据` = archive 路径 · `复核条件` 清空）；⑤ **builder 步的样本四约束**：矩阵内可合成 · N ≥ 2 · **含端口 + 网** · **无悬空 `ch_in`** ⇒ ⇒ 失败档用例 MUST **先删残留产物** ⇒ ⇒ 计数用 `// CppHDL-synth:` 的 `implicit port` / `net` **分别**计数 | ⬜（**① ✅ / ②③④⑤ ⬜**） |
| **P8** | **`Simulator::set_ab_verification()` 是 no-op stub**（ADR-035 R9）⇒ ⇒ ⚠️ **2026-11-09 实测订正**：**「A/B 从未实现」只对 runtime flag 成立** —— 测试层**已有** interpreter vs JIT 逐值 byte-identical 比对（`tests/jit/test_jit_golden.cpp`）⇒ ⇒ 路线改为 **A（推荐）**：**删 stub**（Zero-Debt「No dead code」）+ 把 JIT 正确性交给 **golden 覆盖**（现覆盖 5 个 / `include/jit/jit_ir.h` 的 39 个 JitOp）；**B（备选）**：实现 runtime A/B（tick 级快照 diff）⇒ ⇒ **本窗口只做 A** —— ⚠️ **B 的被拒理由（记录在案 · 免得下一个窗口把 B 当未完成义务重启）**：golden 比对（测试期 · 逐值 byte-identical diff）**已覆盖 runtime A/B 的核心价值**（= diff 本身）⇒ ⇒ runtime flag 是**冗余载体**；且 B 的「每 tick 复制 `data_map_`」成本已在 `AGENTS.md` KNOWN LIMITATIONS 载明 ⇒ ⇒ 「删 vs 实现」是**有记录的取舍** | architect → builder | ⚠️ **architect 步（critic 第 1 轮 BLOCKER 3）**：删 `set_ab_verification` 会使 **ADR-035 R9 决策表行整体失效**（「A/B 默认关闭；显式 `set_ab_verification(true)` 才启用」）⇒ ⇒ R9 是**决策表承重行**、**不是历史旁注** ⇒ ⇒ MUST 走 **ADR 修订流程**（版本 bump + 修订行：删除裁定 + golden 比对替代 + 角色 + 日期）+ 审稿记录落 `.rddf/reviews/`（**inline 注记不构成对决策行的取代**） | ① stub 删除（`set_ab_verification` / `is_ab_verification` / `ab_verification_` / 每 tick WARN）⇒ ⇒ **公开 API 移除 MUST 走 P14 机制**（`public-api-surface.md` 登记章 + `public-api-removals.tsv` 探针 + `docs/CHANGELOG.md` 一行 + `verify_public_api_visibility.sh` 绿）；② golden 覆盖**下限 = 5 / 39**（分母 = `include/jit/jit_ir.h` 的 JitOp 总数）⇒ ⇒ **防回退**；新增 JitOp MUST 附 1 条 golden 用例 **或** 在 `.rddf/checks/` 登记未覆盖理由；③ **architect 步 ✅ 已闭（2026-11-09）** ⇒ ADR-035 **v2.2 补丁**（R9 改记「已裁定 · ⏳ 待实施」+ 选项表 A/B/C + 范围 + **残余风险** + 下游判据 4 条）· 审稿 `.rddf/reviews/ADR-035-verilator-backend-v2.2.md`（oracle 9 条 + critic **3 轮**（第 2/3 轮为复审）⇒ 最终 **APPROVE** · N-B1/N-B2 均 `CLOSED`）· 提交 `c0658d0` + `53029af` · **契约要点** = 删 3 个 API + WARN 块 + **两个局部量**（`ab_result_a` / `run_ab_verify` ⇒ 否则成新死变量）· **MUST NOT** 改 JIT 其余行为 · **残余风险**（运行期无双跑校验）· ⚠️ **事实订正**：`__has_include` 对**源树物理头恒真** ⇒ 该 API **两种构建都在**（根 `AGENTS.md:313` 的「OFF 时不存在」是**事实错误** ⇒ MUST 订正）· **AGENTS.md 那条 = 替换（不是纯删）** ⇒ ADR 内已给**逐字建议文本**；④ 其余文案清理 ⇒ ⇒ 判据 = **代码面** `include/ src/` == **0** + **文档面**（`AGENTS.md` + `docs/` · **排除本 ADR 的审计轨迹**）其余命中 MUST 同段含「已移除 / 已删除」（⚠️ **两条**判据 · 原「全仓 0 且允许 1 处」与「R9 行 MUST NOT 删」**自相矛盾**）⇒ ⇒ 原「全仓 grep」：`grep -rn "set_ab_verification\|is_ab_verification\|ab_verification_" include/ src/ docs/ AGENTS.md` 命中数 == 0（**修订后的 ADR / ROADMAP 允许保留 1 处**且须带「已移除」字样 ⇒ ⇒ `ADR-005` / `ADR-035` / `docs/simulation/JIT_ROADMAP.md` / `docs/usage_guide/10-verilator-backend.md` / 根 `AGENTS.md` 逐处处置） | ⬜（**③ ✅ / ①②④ ⬜**） |
| **P9** | **cache wrapper 三项已知债务**：① `DCacheWriteBuffer` **不真缓冲**（`WriteBufferEntry buffer[4]` **零消费者** ⇒ 死代码）· ② `ICachePrefetch` 预取缓冲是**死数据**（3 个 `ch_mem` + `(void)` 抑制）· ③ 内层 cache `miss`（sread 推导）与 `axi_addr`（当拍地址）**1 拍错位** ⇒ ⇒ ⚠️ **2026-11-09 实测订正**：① ② 已被登记为「已知债务」（`include/cpu/AGENTS.md:167-170`）⇒ ⇒ 「**再登记一次**」= 零产出，且与 ZERO-DEBT「No dead code：立即删除」**冲突** ⇒ ⇒ **默认只删**（真实现**另立**）；③ **未判定是否真缺陷** ⇒ MUST 先写**判别性探针** | builder | — | ① ② **默认只删**：`DCacheWriteBuffer` 删 `WriteBufferEntry` + `buffer[4]`、`ICachePrefetch` 删三个死缓冲 ⇒ ⇒ 删前 MUST 用 `git grep` 证明**零消费者** ⇒ ⇒ **MUST NOT** 以 `(void)` 抑制（Zero-Debt）⇒ ⇒ **真实现（真 FIFO / 真预取）= 另立 P 步 / 另立 improvement**（不在本步）⇒ ⇒ ⚠️ **被删符号在公开头里**（`WriteBufferEntry` 是公开 `struct` · `prefetch_*` 是公开成员）⇒ ⇒ **「仓内零消费者」恰是 ZERO-DEBT 判决边界点名的**证据形态不足**（对跨仓下游结构性失明）⇒ ⇒ 删除 MUST **走 P14 登记机制**（`public-api-surface.md` 移除章 + `public-api-removals.tsv` 探针行 + `docs/CHANGELOG.md` 一行 + `verify_public_api_visibility.sh` 绿）；③ 1 拍错位：**先写判别性探针**（miss 持续期间**改变 addr** ⇒ 观察 `axi_addr`）⇒ ⇒ 裁决二选一：「**修**」**或**「写成**带可执行断言**的使用约束」—— ⚠️ **约束分支的契约载体 MUST 指定**（不指定 ⇒ 实施者可随手选低载体）⇒ ⇒ 约束 MUST 落在 `include/cpu/AGENTS.md` 债务段 **+** capability spec（cache 相关）⇒ ⇒ 且 MUST 附一条**会失败**的用例（进 ctest）断言该行为 ⇒ ⇒ 否则**删除该分支**；④ `include/cpu/AGENTS.md` 债务段更新 + 其头部「30 headers, 6506 lines」的**实测数字**同步 | ✅ **2026-11-09** ⇒ change = `openspec/changes/archive/2026-11-09-cache-wrapper-dead-code-removal-2026-11/`（**已归档** · spec delta 已合入主 spec：`cpu-cache-subsystem` **ADDED ×1**「`miss` 期间的地址稳定性」）⇒ ⇒ 实现 = `include/cpu/cache/d_cache_write_buffer.h`（删 `WriteBufferEntry` / `WriteBufferConfig` / `buffer[4]` / `(void)buffer`）· `include/cpu/cache/i_cache_prefetch.h`（删三个 `prefetch_*` 缓冲 + `PrefetchConfig` + `(void)` 抑制）⇒ ⇒ **`__io` 签名零改动** ⇒ wrapper 的**可观测行为不变** ⇒ ⇒ 登记 = **P14 机制**（登记表 +2 行 · tsv +6 探针 · 护栏锚点 `13→15` / `18→24` · `docs/CHANGELOG.md` 一行）⇒ ⇒ 判别力 = `examples/riscv-mini/tests/test_cache_smoke.cpp` 新增**错位探针**（`miss` 期间改 `addr` ⇒ `axi_addr` 跟随**当拍**地址 ⇒ 若内层改成跟随 `miss` 所指地址 ⇒ **转红**）⇒ ⇒ 文件 **12 用例 / 78 断言**（双后端）⇒ ⇒ 契约 = capability `cpu-cache-subsystem` 的 ADDED Requirement（正/负 Scenario）⇒ ⇒ 文档 = `include/cpu/AGENTS.md`（债务段 + 行数 **6506 → 6473**）· 根 `AGENTS.md` KNOWN LIMITATIONS 同步⇒ ⇒ **验证**：`ctest -LE perf` **244/244 全绿** · `verify_public_api_visibility.sh` 绿 · `verify_openspec_delta.sh` 绿 · `verify_test_counts.sh` 绿（**`CTEST_TOTAL` 255 / `VERILATOR_E2E_TOTAL` 77 均不变**）· `verify_roadmap_fragments.sh` 绿 ⇒ ⇒ ⚠️ **留作 backlog（不在本步）**：① 真 FIFO 写缓冲 ② 真预取 ③ **仓内唯一调用方**（RV32I 取指侧）是否真的在 `miss` 期间保持 `pc` 不变（契约的调用方核对）|
| **P10** | ⚠️ **2026-11-09 订正 · 路线变更**：原行「openspec CLI 钉回 1.4.1」的前提与手段**均已失效** —— 实测 `openspec --version` = **1.14.3**（**不是** 1.4.1）；`openspec validate --specs --strict` **exit 1** · `Totals: 21 passed, 23 failed (44 items)`（**不是**「15/17」）· 红项全部来自 1.14.x 新增的两条 strict 规则（Requirement 正文 >500 字符 / Purpose 占位符）⇒ ⇒ 且 CI **不跑** `openspec validate`（只跑 `scripts/verify_openspec_delta.sh`）⇒ ⇒ 「钉回 1.4.1」= **环境级、不可复现、不落仓**的手段 ⇒ ⇒ **改为：登记已知漂移 · 不实施** | planner | — | ① 根 `AGENTS.md` 的「版本钉在 1.4.1」**订正为实测事实**（CLI 版本 + **规则名**（Requirement 正文 >500 字符 / Purpose 占位符）+ **执行日期** + 当时的计数**快照** ⇒ ⇒ **不承诺长期有效**，避免下一次 CLI 升级再漂移）；② 同文件 `:193` 的「CI 校验：`openspec validate --specs --strict` 必须在 archive 前通过」**同步订正**（CI 实际只跑 `scripts/verify_openspec_delta.sh`；`validate --specs` 仅作参考且当前仓级为红）；③ `feat-hierarchical-rtl-output`「全程」条的注记数字**同步订正**（**只改正文** ⇒ **不动 front-matter** ⇒ AUTO 块逐字不变 ⇒ `verify_roadmap_fragments.sh` 绿）；④ 「仓级 validate 全绿」**不是**本窗口的交付物 ⇒ ⇒ 该复选框的**红项**在本窗口**只登记、不消除** | ✅ **2026-11-09** ⇒ ① 根 `AGENTS.md` 的 CLI 版本条**订正为实测事实**（CLI **1.14.3** + 两条规则名 + **2026-11-09 快照** `21 passed / 23 failed (44 items)` + 「**只登记、不消除**」+ 环境 pin「**未采用**」的理由）· ② 根 `AGENTS.md` 的「**CI 校验**」条**同步订正**（archive 前的实际门 = `scripts/verify_openspec_delta.sh` · `validate --specs` **仅作参考** · 证据 = P15 归档在仓级红下**成功**）· ③ `feat-hierarchical-rtl-output.md`「全程」注记数字**订正**（15/22 → **21/23（44 项）**）· **front-matter 未动** ⇒ ⇒ `verify_roadmap_fragments.sh` **绿** · ④ ⚠️ **登记给 architect**：`ADR-067` §Verification 与 `ADR-052` / `ADR-063` 仍写「`openspec validate --specs --strict` 绿（CLI 须 **1.4.1**）」⇒ ⇒ 那是 **Accepted ADR 的判据** ⇒ ⇒ **planner / builder MUST NOT** 单方面改 ⇒ ⇒ 列入 architect 待办 |
| **P11** | **ADR-036 逻辑算子物理移除**（`&&` / `\|\|` ⇒ `&` / `\|`） | builder | 🔒 **日历锁：最早 2027-04-17**（`verify_deprecation_calendar_gate.sh` 守） | 移除完成 + `verify_deprecation_callsites.sh` 绿 | 🔒 |
| **P12** | **上游 issue #28**（下游 ChipForge 反馈 · **新缺口**）：起草**两项** improvement（**拆项** —— ② 需架构裁定 / ① 不需 ⇒ 不被审稿门卡住）⇒ ⇒ 过 `pre_create_brainstorm_check.sh` HARD-GATE | planner | — | 两个 improvement 文件存在且各过 HARD-GATE（exit 0） | ✅ **2026-11-09** ⇒ ② = `.rddf/improvements/chlib-dtor-noexcept-composability-2026-11.md`（**析构异常规格 vs 可组合性**）· ① = `.rddf/improvements/public-api-remove-visibility-2026-11.md`（**API 移除的跨仓可见性**）⇒ 两者均 **7 段** + YAML frontmatter · **HARD-GATE exit 0** ⇒ 已注册进 `improvement-suggestions.md` · ⚠️ 两者状态 = `draft` ⇒ ⇒ **② 待 P13 的 architect 裁定**后才可转 `approved`；**① 不需裁定**（★ 唯一待用户拍板项 = CHANGELOG 载体命运 R1/R2/R3）· ★ **metis 第 1 轮**（12 条 finding · 4 blocker）已逐条处置 ⇒ 处置记录 = 本文件 §「本窗口自身的两个已知缺口」后的 **P12 订正段** |
| **P13** | ② 的 **architect 架构裁定** ⇒ ⇒ ADR-067：析构**异常规格** × 检出**触发点** 的取舍（选项 A / A′ / B / C / D / E —— **两处分别裁定**：FSM 析构**只检查** vs `conditional_block` 析构**做事** ⇒ **不同构** ⇒ 不可一把钥匙） | architect | P12 | ADR-067 落库 · **oracle → critic** 双审 **0 BLOCKER** · 审稿记录落 `.rddf/reviews/` | ✅ **2026-11-09** ⇒ ADR = `docs/adr/ADR-067-dtor-exception-spec-composability.md`（**Accepted** · **用户拍板**）⇒ ⇒ **裁决**：① 两处分别裁定 ② FSM 默认形态**逐字不变** + **显式**提供「可组合形态」（noexcept 析构 + 显式 `check_unresolved()` + 非展开期 terminate 兜底 fail-closed）③ `conditional_block` **维持** `noexcept(false)`（**可行但当前不做** · 无需求 + 固定成本 + 基线缺失）④ 使用契约（精确措辞）⑤ 机械判据（`is_nothrow_destructible_v` 三连 + 变异自检）⑥ 展开期门复制 ⑦⑧⑨ 不做项（elaboration 扫描 / 恢复 `is_built()` / 删拷贝）⇒ 审稿 = `.rddf/reviews/ADR-067-dtor-composability.md`（oracle 6 条 + critic 9 条 **逐条应答** · 第 1 轮 1 BLOCKER ⇒ 复审 `CLOSED` / `mergeable: yes`）⇒ ★ 评审推翻了我方两处错误（「5 处」→「**8 处**」· `static_assert(!is_nothrow_destructible_v<std::optional<Sm>>)` **恒假** ⇒ 改**合取**形态 ⇒ 编译实锤） |
| **P14** | ① 的**实施**：CHANGELOG 载体（按裁定 R1/R2/R3）· Zero-Debt **判决规则边界**（根 `AGENTS.md` ZERO-DEBT 段 +1 句 · **AUTO 块不动**）· 活账 `.rddf/checks/public-api-surface.md` 扩到**符号级**（含**移除记录**字段 + R4② 双向核对）· **消费者 fixture 进上游 CI**（install 到临时 prefix ⇒ 独立 CMake 工程编译下游视角 TU · **fail-closed**） | builder | P12 | ① CHANGELOG 判据（token 清单 / 计数守恒）绿；② 判决规则句 `grep` 可判；③ R4② 符号级**原地投毒**必红；④ 消费者 fixture **缺 = 失败** + 计数锚点（`verify_test_counts.sh` 绿） | ✅ **2026-11-09** ⇒ change = `openspec/changes/archive/2026-11-09-public-api-remove-visibility/`（**已归档** · 无 spec 变更 ⇒ `--skip-specs`）⇒ ⇒ 用户裁定 = **R1 + R2**（逐 change 一行 + 一次性回填） ⇒ 载体 = ① `docs/CHANGELOG.md` 的 2026-11-09 条目（回填 2026-10-15 以来的用户可见破坏性变更 + 本窗口的「可组合形态」）② 每个**机制生效后**归档的 change 的 `## 用户可见变更` 段（首个例证 = P15 的归档件） ⇒ 判决规则边界 = 根 `AGENTS.md` ZERO-DEBT 段 +1 句（AUTO 块不动 ⇒ `verify_roadmap_fragments.sh` 绿）+ ADR-050 / ADR-043 的**反向指针**（决策内容零改动）⇒ 活账 = `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」章（+ 已弃用表）+ `.rddf/checks/public-api-removals.tsv`（机器探针 · 四种 kind · 剥注释后判定）—— ⚠️ **行数 / 探针数的单一真相 = `scripts/verify_public_api_visibility.sh` 的 `EXPECTED_REMOVAL_ROWS` / `EXPECTED_PROBES`**（本文件**不复制**计数 ⇒ ⇒ 免于漂移）⇒ 护栏 = **`scripts/verify_public_api_visibility.sh`**（R1 归档载体 / R2 移除登记 / R3 判决边界 + roadmap 守恒 / R4 CHANGELOG **token 清单** / R5 机械探针 / R6 缩面锚点）+ `--self-test`（M1–M4 原地投毒必红）· **`scripts/verify_downstream_consumer.sh`**（install → **临时 prefix** → **独立 CMake 工程** → **运行** · 三种持有形态：局部 / `unique_ptr` / **按值成员 + 虚析构基类**）+ `--self-test`（M1 投毒**已安装副本** ⇒ 下游 build 必红 = issue #28 的复现 · M2 空 fixture ⇒ fail-closed）⇒ 两者已挂 CI 的 `build-and-test` guard 段 ⇒ 判据 = `tests/downstream-consumer/{main.cpp,CMakeLists.txt}` + ctest 条目 `downstream_consumer`（`CTEST_TOTAL` 254→**255**）⇒ ⇒ **验证**：`ctest -LE perf` **244/244 全绿**·`verify_test_counts.sh` / `verify_public_api_visibility.sh`(+self-test) / `verify_downstream_consumer.sh`(+self-test) / `verify_roadmap_fragments.sh` / `verify_hierarchical_p8.sh` / `verify_openspec_delta.sh` 全绿 ⇒ ★ 副产品（已登记，不在本 change 修）= 安装后的头**不是单目录自足**的（`ast/ast_nodes.h` 引 `lnodeimpl.h`，而它装在 `core/`）⇒ 下游 MUST 加 5 个 `-I`（= 上游 `CH_INCLUDE_DIRS` 口径） |
| **P15** | ② 的**实施**：按 ADR-067 裁定改析构异常规格 / 加 API（复制**展开期门**）· 2 份 capability delta（**禁** `--skip-specs`）· **可组合性的机械判据**（`tests/test_chlib_composability_api.cpp` · 用仓内 `ch::Component` 作基类 ⇒ 与下游同形）+ **变异自检** · 补「忘记 `.end()` ⇒ 析构期可捕获」基线用例 | builder | P13 | ① `static_assert(std::is_nothrow_destructible_v<Dut>>` 绿 + 变异投毒必红；② `verify_openspec_delta.sh` + `openspec validate --specs --strict` 绿；③ `CTEST_TOTAL` 锚点同步 | ✅ **2026-11-09** ⇒ change = `openspec/changes/archive/2026-11-09-chlib-dtor-composability-2026-11/`（**已归档** · spec delta 已合入主 spec：2 份 capability **ADDED ×1**）⇒ ⇒ 实现 = `include/chlib/state_machine.h`（新增 `dtor_detection` 枚举 + 第三模板实参 + `check_unresolved()` + 终止兜底 + 头注释「③ 析构形态与可组合性」段）· `include/chlib/if_stmt.h`（**仅注释**：使用契约）⇒ **`core/` 零改动** ⇒ 判据 = `tests/test_chlib_composability_api.cpp`（**新增 1 个 ctest target** ⇒ `CTEST_TOTAL` 253→**254**）· 护栏 = `scripts/verify_chlib_dtor_composability.sh`（R1 fixture 编译 / R2 API 面 / R3 两份头注释的契约 / R4 capability 契约）+ `--self-test`（M1/M2/M3 **原地投毒** ⇒ 必红）⇒ CI 已挂 `build-and-test` 的 guard 段 ⇒ ⇒ **验证**：`ctest -LE perf` **243/243 全绿**（基线 242 + 新增 1）· `verify_test_counts.sh` / `verify_openspec_delta.sh` / `verify_fsm_next_wiring.sh` / `verify_if_stmt_wiring.sh` / `verify_roadmap_fragments.sh` 全绿 · **M1 变异实测必红**（3 处：2 × `static_assert` + 1 × 真实「looser exception specification」编译错误）⇒ ⇒ 还原复绿 · 追补（遗留清扫 · metis #7）= `tests/chlib/test_if_stmt.cpp` 的「忘记 `.end()`」**基线两条**（负例：输出缺 `else_` ⇒ **析构兜底** ⇒ 拒绝**可捕获**；正例：各目标都有 `else_` ⇒ 兜底**不**抛）⇒ 变异（删析构兜底）两条**都转红** ⇒ 还原后 `test_if_stmt` **19 用例 / 32 断言**全绿 | ⇒ ⇒ ⚠️ **2026-11-09 口径订正**（metis 第 1 轮发现）：本行 ② 的「`openspec validate --specs --strict` 绿」**表述过宽** —— 实际满足的是 **change / capability 级**（`verify_openspec_delta.sh` + 该 change 的 spec 合并），而**仓级** `--specs --strict` 现为**红**（CLI 1.14.x 漂移 ⇒ 见 **P10**）⇒ ⇒ 本行 ✅ **不撤回**，但判据口径**以本注记为准**

---

## P2 的核对范围（★ **已实测的漂移清单** · 2026-11-09）

> ⚠️ 护栏 `verify_roadmap_fragments.sh` **判不了「叙事性陈旧」**（符号一致 · 只是内容过时）
> ⇒ ⇒ 这类漂移**只能人工核对** ⇒ ⇒ 本步是**唯一**能发现它们的关口。

| # | 位置 | 陈旧描述 | 实际（依据） |
|---|---|---|---|
| ① | `features/feat-rv32i-isa-completeness.md` | ④ 取指 skid buffer / 双装、⑤ load 侧字节道「待做」 | `AGENTS.md` KNOWN LIMITATIONS：skid buffer + 双装**已消除**（ADR-048）· LB/LH/LBU/LHU **已覆盖** ⇒ ⇒ **2026-11-09 复核：已无漂移**（`ce683e2` 已把 ④⑤ 改为 ✅ 归档 · feature `completed`；该文件内零「待做 / ⏳ / 🟡」） |
| ② | `phases/phase-5.md` 完成条件 | 「dual-backend 全量覆盖」未勾 | objective `objective-test-coverage-100pct` **2026-11-03 已 `completed`**（parity 源文件 66 ≥ 60）⇒ ⇒ 该条**可勾** ⇒ ⇒ **2026-11-09 复核：「objective 正文仍有旧的 ≥ 100」不是漂移**（§2 已标「**取代**」；§1 那段是**带日期的历史快照**，按仓内惯例原样保留） |
| ③ | `phases/phase-5.md` 完成条件 | 「RV32I ISA 语义边界闭合 ①–④」未勾 | ①–③ 已归档 + ④ 已由 ADR-048 消除 ⇒ ⇒ 可勾（⚠️ 实为 **①–⑤**：⑤ `rv32i-load-byte-lane-2026-10` 2026-10-28 归档） |
| ④ | `phases/phase-5.md` 完成条件 | 「riscv-mini RTL 缺陷收口 ①②」未勾 | feature `feat-riscv-mini-rtl-debt` 已 `completed`（两项均已收口：2026-10-28 / 2026-10-30 归档）⇒ ⇒ 可勾 |
| ⑤ | `phases/phase-5.md` 完成条件 | 「Windows-2022 CI 启用」未勾 | ⇒ ⇒ 转 **P2b**（不做 · 附裁决） |
| ⑥ | `phases/phase-5.md` **概览表**（★ 原清单**漏列**，P2 实读补上） | 4 行状态陈旧：RV32I ISA / RTL 债 / dual-backend 仍 🟡⏳；平台矩阵仍「⏳ 计划中」 | 四者均已终结 ⇒ ⇒ 已改 **✅×3 + ⛔×1**（见该表） |

⇒ ⇒ **P2 的真正价值**：phase-5 的 3 条完成条件 + 2 条 feature 描述**都停在「实际已完成但账面未勾」**
⇒ ⇒ 核对后 **phase-5 可转 `done`** ⇒ ⇒ 本窗口（phase-6）成为**唯一** active 窗口 ⇒ ⇒
根 `AGENTS.md` 的 AUTO 块里相关 feature 的 `status` 也需同步（★ 受 G1 约束 ⇒ ⇒
**P2 的收尾必须包含 AUTO 块同步**）⇒ ⇒ ⚠️ **2026-11-09 订正**：**「同步手段缺失」已不成立** ——
dispatch **存在**（工具链 rdd-workflow v4.0.0）⇒ ⇒ 正确动作 = **直接调用全局 payload**
（`bash ~/.agents/skills/rdd-workflow/skills/roadmap/scripts/roadmap_refresh_fragments.sh`）
⇒ ⇒ 详见本文件 §「本窗口自身的两个已知缺口」的 **G1** 行与其**修复注记** ⇒ ⇒
⇒ ⇒ **无独立待办**（原「独立待办」的表述**已作废**）。

---

## 每步的**证据落点**（**闭口证据 = 可 grep 的路径**）

| 步 | 证据落点 |
|---|---|
| P1 | `.rddf/reviews/hierarchical-parent-side-landing-implementation-2026-11.md`（追加段）· 变异/注入记录 |
| P2 | `.rddf/reviews/phase-6-p2-roadmap-audit-2026-11.md`（核对记录）+ `phases/phase-5.md`（**转 `done`**）+ `features/feat-rv32i-isa-completeness.md`（审计快照注记） |
| P2b | `phases/phase-5.md` 的完成条件（该项标「不做（附裁决日期）」）+ 本文件的结论行 |
| P3 | `.rddf/improvements/mem-port-arg-cross-layer-2026-11.md`（草案 · HARD-GATE exit 0）+ `improvement-suggestions.md` 的索引行 |
| P4 | **✅ 已落**：`docs/adr/ADR-066-...md`（**v1.2**）+ `docs/adr/ADR-060-...md`（**v1.17**）+ `.rddf/reviews/ADR-066-parent-side-landing.md`（**§8**） |
| P5 | `openspec/changes/archive/<mem-port-实参>-<日期>/` + `.rddf/checks/arch-residual-registry.md`（行 20）+ 台账 §7 |
| P6 | `openspec/changes/archive/<isa-缺口>-<日期>/` + 新 capability spec |
| P7 | **① ✅ 已落**：`docs/adr/ADR-060-...md`（**v1.18**）+ `.rddf/reviews/ADR-060-port-policy-v1.18.md` ⇒ ⇒ **②③④⑤ ⬜**：archive change + 台账行 15 + §5「产物注释判据（v1.18）」的样本四约束 |
| P8 | `scripts/verify_public_api_visibility.sh` 绿（移除登记）+ `docs/CHANGELOG.md` 一行 + `.rddf/checks/public-api-removals.tsv` 探针行 + `tests/jit/test_jit_golden.cpp`（覆盖率分子/分母）+ 根 `AGENTS.md` KNOWN LIMITATIONS + `docs/usage_guide/10-verilator-backend.md:133` 文案删除 |
| P9 | `include/cpu/cache/{d_cache_write_buffer,i_cache_prefetch}.h` 的**死成员删除**或真实现 + ③ 的**判别性探针**（用例）+ `include/cpu/AGENTS.md` 债务段更新 |
| P10 | 根 `AGENTS.md` 的 CLI 版本说明**订正** + `features/feat-hierarchical-rtl-output.md`「全程」条的注记数字**订正**（**不要求** validate 全绿） |
| P12 | `.rddf/improvements/chlib-dtor-noexcept-composability-2026-11.md` + `.rddf/improvements/public-api-remove-visibility-2026-11.md`（两份草案 · 各 HARD-GATE exit 0）+ `improvement-suggestions.md` 的两行索引 |
| P13 | `docs/adr/ADR-067-*.md`（新 ADR）+ `.rddf/reviews/ADR-067-*.md`（审稿记录） |
| P14 | `docs/CHANGELOG.md`（R2 一次性回填 · R1 机制）+ 根 `AGENTS.md` ZERO-DEBT 段（+1 句）+ `.rddf/checks/public-api-surface.md`（符号级移除章）+ `.rddf/checks/public-api-removals.tsv`（**新** · 15 条机器探针）+ `scripts/verify_public_api_visibility.sh`（**新** · R1–R6 + `--self-test`）+ `scripts/verify_downstream_consumer.sh`（**新** · 安装产物消费者 + `--self-test`）+ `tests/downstream-consumer/{main.cpp,CMakeLists.txt}`（**新** · 独立工程）+ CI 挂载（guard 段 4 步）+ `CTEST_TOTAL` 锚点（255）+ ADR-050 / ADR-043 的反向指针 |
| P15 | `openspec/changes/archive/<析构异常规格>-<日期>/` + 2 份 capability spec + `tests/test_chlib_composability_api.cpp`（新）+ `tests/chlib/test_if_stmt.cpp`（基线用例） |

---

## P12 的 **metis 第 1 轮**处置记录（12 条 finding · 4 blocker）

> ⚠️ **planner 的固定审查者** = `metis`（新建 improvement ⇒ 1 轮）⇒ ⇒ 逐条处置如下
> ⇒ ⇒ **无一条「只列不改」** ⇒ ⇒ 每条要么已落进两份草案，要么写明「不采纳 + 理由」。

| # | finding（metis） | 处置 |
|---|---|---|
| 1 | **[blocker]** 范围错：① 与 ② 应**拆成两个** improvement（① 不需裁定 ⇒ 不被审稿门卡住） | ✅ **采纳** ⇒ 拆为 `public-api-remove-visibility-2026-11`（①）与 `chlib-dtor-noexcept-composability-2026-11`（②） |
| 2 | **[blocker]** A–D 既不互斥也不穷举；D 的「自动」是**事实错误**；漏 **E**（elaboration 扫描）与 **A′** | ✅ **采纳** ⇒ ② 的 §What Changes 改为**两轴**（触发点 × 析构规格）+ 六项（A / A′ / B / C / D / E）+ 删掉全部 ✅/❌ 评价与 planner 倾向 |
| 3 | **[blocker]** D 会把缺陷复制到新类型（守卫析构自身 `noexcept(false)` ⇒ 违反本项自己的约束） | ✅ **采纳** ⇒ D 降级为「守卫须为**作用域内临时对象** + 显式豁免 + 写明理由」，并记入改名建议（`scope_check()` / `lifecycle_scope()`） |
| 4 | **[blocker]** 「可组合性」必须写成**一行可判** | ✅ **采纳** ⇒ ② 的 Acceptance 用 `static_assert(std::is_nothrow_destructible_v<Dut>)`（`Dut : ch::Component`）+ **反向**断言（`std::optional<Sm>` 陷阱）+ 变异自检 ⇒ 并删掉「可行动地失败」这类散文 |
| 5 | **[blocker]** CHANGELOG 判据从「日期 ≥」换成 **token 清单**；且载体命运需先裁定（**6** 条 vs **145** 个归档 change ⇒ 整体停更） | ✅ **采纳** ⇒ 移入 ① 的 §What Changes（**R1 / R2 / R3** 三选一 · ★ 待用户拍板）+ Acceptance 用 token 清单 / 计数守恒 |
| 6 | **[quality]** 出口判据补 `verify_openspec_delta.sh` + capability **符号级不变量** + `CTEST_TOTAL` 同步 | ✅ **采纳** ⇒ 进 ② 的 Acceptance（P15 出口判据） |
| 7 | **[quality]** 「忘记 `.end()` ⇒ 析构期可捕获」**今天零测试** ⇒ `spec.md:223` Scenario 无背书 | ✅ **采纳** ⇒ 进 ② 的 Acceptance（基线用例）⇒ 另核实：`.end()` 在 `tests/chlib/test_if_stmt.cpp` 出现 **14** 次 · **0** 处走析构路径 |
| 8 | **[quality]** 约束句三处同改 + 删「编译器会说人话」的 over-promise | ✅ **采纳** ⇒ ② 的 §What Changes 给出精确措辞（**含「作为基类」**）⇒ 三处（② 的范围 / ② 的 Acceptance / 本文件 P13 行）同改 |
| 9 | **[quality]** 用**仓内**证据补强 Why | ✅ **采纳** ⇒ ★ **新增实测**：`include/component.h:62` 的 `virtual ~Component();`（隐式 noexcept）作基类 + 按值持有 FSM ⇒ **同样的编译错误** ⇒ ⇒ 「把 describe() 里的局部 FSM 提为成员」这个**标准重构**今天不可用（`uart_tx.cpp:71` 就是局部对象）⇒ ⇒ 比引用下游 issue 更有力 |
| 10 | **[quality]** 本文件「不再需要新的架构裁定」被 P13 证伪 ⇒ 必须订正 | ✅ **采纳** ⇒ 概览段已加 **2026-11-09 订正**（并说明台账 §7 的 20 行**不受影响**） |
| 11 | **[nice]** 删 planner 倾向与 ✅/❌ 评价符号 | ✅ **采纳** ⇒ ② 全文无倾向、无评价符号 |
| 12 | **[nice]** `guard` 术语撞车（`chlib/guard_contribution.h` 已占用） | ✅ **采纳** ⇒ 记入 ② 的选项 D 说明（改名建议） |

★ **metis 之外、由本轮复核新增的三条事实**（已实测，进 ② 的正文）：

| # | 事实 | 证据 |
|---|---|---|
| a | **`conditional_block` 与 `ch_state_machine` 的析构不同构** ⇒ 前者析构**做事**（兜底 `end()` 合成）⇒ 后者只**检查** ⇒ ⇒ **必须分别裁定**（改 `noexcept` 对条件块 = **静默丢功能**） | `include/chlib/if_stmt.h:133-139` vs `include/chlib/state_machine.h:506-517` |
| b | **`std::optional<Sm>` 是静默降级陷阱**（比「编译期撞墙」更危险） | `static_assert(std::is_nothrow_destructible_v<std::optional<Sm>>)` **成立**（libstdc++ 把 `T` 放 union ⇒ union 析构 noexcept ⇒ `~optional()` 推导 noexcept） |
| c | **展开期门 / 静态析构期门**：`std::uncaught_exceptions() == 0` 两处都在 ⇒ 新形态 MUST 复制；`in_static_destruction()` 门**当前永不生效**（ADR-050:166 自陈 3 个调用点全被注释）⇒ 「保持既有守卫」**行为不可测** ⇒ 须手工置位测或明写 dormant | `state_machine.h:509` · `if_stmt.h:135` · `ADR-050:166` |

---

---

## 本轮修订记录（**2026-11-09 · planner 域** · 用户口令「按评审结论修订」）

> ⚠️ **只改「怎么做 / 怎么判」，不改「做不做」** ⇒ ⇒ 任务集合 = **P4 / P5 / P6 / P7 / P8 / P9 / P10**
> （+ P11 日历锁）⇒ ⇒ 状态全部仍为 ⬜（本轮**未实施**任何一步）。
> ⚠️ **依据** = 一次只读评审（逐项到仓内取证：grep / 读实现 / 跑护栏与 CLI）⇒ ⇒ 逐条处置见下。

### ① 文档缺陷（9 项 · 逐条订正）

| # | 位置 | 缺陷 | 处置 |
|---|---|---|---|
| 1 | §概览「承接」 | 同一句「3 项直接遗留 = P1 / P3–P5」**出现两次**（`:25` / `:30`） | 删重复的那一句（② 转交的是 P11 / P2b）⇒ 保留 ① 的那一句 |
| 2 | §P2 漂移清单末段 | 仍写「CLI dispatch 在本仓**缺失** ⇒ **独立待办**」—— 与 §G1 的 2026-11-09 修复注记**直接矛盾** | 改引 G1 + 全局 payload 调用方式；删除「独立待办」表述 |
| 3 | P5 出口 ① vs P3 口径 vs §明确不做 | 三处「支持 / 不支持」**口径冲突** ⇒ ⇒ P5 按字面**永远无法闭口** | P5 出口判据改**分支式**（共同项 + 「仅当 P4 裁支持时」追加项）⇒ §明确不做补一行**分支边界** |
| 4 | P10 | 前提数字**过时**（「15/17 主 spec 红」⇒ 实测 44 项 / 21 passed / 23 failed） | P10 行按实测重写 |
| 5 | P14 行的活账计数 | 「11 行 / 15 条探针」是**复制**的计数 ⇒ ⇒ 与脚本锚点漂移（工作树当前已是 13 / 18） | 改为**指向脚本锚点**（`EXPECTED_REMOVAL_ROWS` / `EXPECTED_PROBES`）⇒ ⇒ 本文件不再复制计数 |
| 6 | P6 | 隐含「CSR / ECALL 从零实现」—— 与**已存在**的 `CsrBank` / `ExceptionUnit` + ctest 用例不符 | P6 行补「实测订正」+ **P6-0 侦察** + 「复用 / 不重复实现」 |
| 7 | P9 | 「降级为已知并登记理由」与 ZERO-DEBT「No dead code：立即删除」**冲突** ⇒ ⇒ 「再登记一次」= 零产出 | P9 行改为**二选一**（删死成员 **或** 真实现）+ ③ 的**判别性探针** |
| 8 | P8 | 「A/B 从未真正实现」—— 与 `tests/jit/test_jit_golden.cpp` 已有的逐值比对不符 | P8 行按实测改写（路线 A / B ⇒ 只做 A） |
| 9 | P7 前置 `P5` | 「两者都动分析层的形态判定」**不成立**（写点不重叠）⇒ ⇒ 无谓串行 | 顺序约束段改写 + P7 前置改「可并行」 |

### ② 任务行改写（7 行）

| 步 | 改了什么（结论） | 落点（可 grep 的符号） |
|---|---|---|
| P4 | 出口判据 +3 条（可开工前置 / §2.7 整句改写 + 双向 grep / ADR-060 §2.1 补格归 architect） | `实参视线` · `MUST 维持响亮拒绝` · `docs/adr/ADR-060-*.md` §2.1 |
| P5 | 出口判据改**四段式**（共同 / P4 裁不支持 / P4 裁支持 / 边界清单归 P3 草案） | `src/hierarchy_analysis.cpp` 的 mem 闸门 · `src/codegen_verilog.cpp` 的 `print_mem_read_port` |
| P6 | 加 **P6-0 侦察** + `FENCE` 裁决 + 判据具体化（新 target / 手推期望值 / `CTEST_TOTAL`） | `OP_SYSTEM` · `CsrBank` · `ExceptionUnit` · `test_csr_unit.cpp` |
| P7 | 前置改**并行**（函数级）· 判据改**两两可区分元组** + 样本 ≥ 2 条可合成边 · 加 ADR 修订流程 | `port_policy` 的读取点 · `synthesized_ports` · `TwoOpTop` |
| P8 | 路线改 **A**（删 stub + golden 覆盖）· 去掉 architect 步 · 判据加**覆盖下限** + 全仓 grep | `set_ab_verification` · `is_ab_verification` · `ab_verification_` · `tests/jit/test_jit_golden.cpp` |
| P9 | ①②**默认只删**（真实现另立）· ③ 加**判别性探针** + 约束分支须带**可执行断言** · ④ 加行数同步 | `WriteBufferEntry` · `buffer[` · `prefetch_data` · `include/cpu/AGENTS.md` |
| P10 | 实施 ⇒ **登记已知漂移 · 不实施** · 数量改**快照** · 补 `AGENTS.md:193` + 「只改正文」 | `openspec --version` · `AGENTS.md` 的 CLI 段与 CI 校验段 |

### ③ 审查记录（**metis → critic**）

> ⚠️ **planner 的固定审查者 = `metis`**（本次为「重写 ≥ 1/3」的规划工件 ⇒ 触发 1 轮）⇒ ⇒
> **critic** 按用户口令追加（`plan` 档 · 架构方向与关键决策）。

| 轮 | 审查者 | 结论 | 处置 |
|---|---|---|---|
| 1 | metis（planner 固定审查者 · 10 条建议：6 blocker / 4 quality） | **全部逐条处置**（6 blocker **全部采纳或改写后采纳** · 4 quality 采纳）⇒ ⇒ **无一条「只列不改」** | 逐条见下表 |
| 1 | critic（`plan` 档 + `arch` 档契约面 · 用户口令追加） | ❌ **REJECT**（**3 BLOCKER** + 4 SHOULD_FIX + 2 NITS · `mergeable: no` · `scope_exceeded: false`）⇒ ⇒ 3 BLOCKER **全部采纳并修**（1 = 判据恒真 · 2 = 公开符号删除未走 P14 登记 · 3 = ADR-035 R9 决策行须走修订流程）⇒ ⇒ 4 SHOULD_FIX + 2 NITS **全部采纳** | 逐条见下表 · **复审（第 2 轮 · 只验 BLOCKER 闭合）待跑** |
| 2 | critic（**复审** · 只验 BLOCKER 闭合） | ✅ **APPROVE**（B1 / B2 / B3 全部 **CLOSED** · `mergeable: yes` · 无新 BLOCKER）⇒ ⇒ 复审判定：① §2.7 区间 = 行 170–197（区间不溢出）· 旧句在区间内 ⇒ 新负向判据**未修订态即红**（非恒真）· 正向判据现值 0 ✅；② 触发条件为真（`struct WriteBufferEntry` 确在公开头 `:61`）· P14 机制**已在库**（无前向依赖）；③ ADR-035 `:234` 的 R9 行**当前零改动**（P8 未实施 ⇒ 无「inline 注记架空决策行」的窗口期）⇒ ⇒ **本轮评审预算用尽**（2/2） | ⇒ 复审通过 ⇒ **本轮 critic 评审预算用尽**（2/2） |

#### critic 第 1 轮逐条处置（3 BLOCKER + 4 SHOULD_FIX + 2 NITS）

| # | critic finding | 档 | 处置 |
|---|---|---|---|
| B1 | P4 的负向 grep **恒真**（旧句在 ADR-066 `:174` 带 `**` 强调符 ⇒ 裸 `grep 'MUST 维持响亮拒绝'` 今天就 = 0）⇒ 判据检不出「只追加不删旧句」 | BLOCKER | ✅ **采纳** ⇒ 判据改「**限定 §2.7 区间 + markdown 不敏感**」（`awk '/^### 2.7/,/^---/' … \| grep -c '维持.*响亮拒绝'` == 0）⇒ ⇒ **复核命令段**与 improvement **A8** 同步 ⇒ ⇒ 并加注「裸 grep 恒真」的原因（防后人再写回） |
| B2 | P9 删除的 `WriteBufferEntry` / `prefetch_*` 是**公开头**里的公开符号 ⇒ 「仓内零消费者」恰是 ZERO-DEBT 判决边界点名的**证据形态不足** ⇒ 删除 MUST 走 **P14 登记机制** | BLOCKER | ✅ **采纳** ⇒ P9 出口 ① 追加「被删符号 MUST 走 P14 登记（移除章 + tsv 探针行 + CHANGELOG 一行 + 护栏绿）」 |
| B3 | 删 `set_ab_verification` 使 **ADR-035 R9 决策表行整体失效** ⇒ R9 是**承重行**、inline 注记**不构成取代** ⇒ MUST 走 ADR 修订流程 | BLOCKER | ✅ **采纳** ⇒ P8 **恢复 architect 步**（归属改 `architect → builder`）+ 出口 ③ 改「R9 经修订流程处置（版本 bump + 修订行 + 审稿记录）」⇒ ⇒ ★ 该步同时**修正了 metis 轮**「删去 architect 步」的判断 |
| S1 | P5 出口 ② 把 fail-closed 落点写死为「发射器 collect」⇒ 与「分析层直接响亮拒绝」这一**合理实现形态**互斥 | SHOULD_FIX | ✅ **采纳** ⇒ 改「分析层响亮拒绝 **或** 发射器 collect fail-closed ⇒ **任一即可**」+ 负向用例 MUST 转红 |
| S2 | P6 的 `grep -rc OP_SYSTEM include/ > 0` **会数到注释** ⇒ 一行注释即可平凡满足（与 B1 同型的「grep 凑数」） | SHOULD_FIX | ✅ **采纳** ⇒ 改「**剥注释后**计数（与 tsv 探针同口径）+ 命中 MUST 落在 `rv32i_decoder.h` **与** 任一 stage 头」+ 「手推期望值」补 verifier 判法（抽 1 条核对非抄仿真输出） |
| S3 | P8 对备选 **B** 未写**被拒理由** ⇒ 下一窗口可能把 B 当未完成义务重启 | SHOULD_FIX | ✅ **采纳** ⇒ P8 行补「B 的被拒理由：golden 比对已覆盖 diff 价值 · 快照成本已载明 ⇒ runtime flag 是冗余载体」 |
| S4 | P9 出口 ③ 的「写使用约束」分支**未指定契约载体** ⇒ 实施者可随手选低载体 | SHOULD_FIX | ✅ **采纳** ⇒ 载体指定为 `include/cpu/AGENTS.md` 债务段 **+** capability spec + **会失败的用例**进 ctest |
| N1 | P5 值判据写「非 0」比「手推确定值」弱一档（错值只要非 0 即过） | NIT | ✅ **采纳** ⇒ P5 出口 ③ 改「MUST 断言**精确期望值**（非 0 只是下界）」⇒ ⇒ improvement **A4** 同步 |
| N2 | P4「(i)(ii) 两分句各自可 grep」未给锚词 ⇒ 实施时又是临时散文判据 | NIT | ✅ **采纳** ⇒ 锚词写死为「**本体**跨 module」与「**实参视线**跨层」两个短语的 grep 计数各 ≥ 1 |

⚠️ **critic 对 focus 三问的正面结论**（不改动作，仅记录）：① P10「登记不消除红项」**不**构成与仓内义务的实质冲突（`AGENTS.md:193` 与「CI 不跑 validate」本就自相矛盾 ⇒ P10 ② 是**消除**矛盾）；② P4/P5 的编排方向**正确**（补格归 architect · builder MUST NOT 改 Accepted ADR · 分支由外部可 grep 事实锚定 ⇒ 无「两条分支都能闭口」的漏洞）；③ P7 的元组判据**可执行**（ADR-060 §2.1 行 1/行 2 两类可合成边支撑「≥ 2 条样本」）。

#### metis 第 1 轮逐条处置（10 条）

| # | metis finding | 严重度 | 处置 |
|---|---|---|---|
| 1 | 概览 ③「任务集合未变」与事实不符（P10 做不做已变 · P8 删 architect 步 · P9 改为删） | blocker | ✅ **采纳** ⇒ 概览 ③ 改为「**「做不做」有 3 处变了**」并逐项列出 |
| 2 | P7「三面各不同」数学上不可满足（产物存在性只有两值） | blocker | ✅ **采纳** ⇒ 判据改**两两可区分元组** `(产物是否存在, 诊断边数, 产物注释)` + 样本 **≥ 2 条**可合成边 |
| 3 | P7「唯一行为点」把「策略被读取点」当「要改的点」⇒ 落不到「产物内写注释」 | blocker | ✅ **采纳** ⇒ 行为点改为「失败路径 + **成功路径**（`synthesized_ports` 声明/连接发射点）」 |
| 4 | P5「写点不重叠」与其自身判据（覆盖 `codegen_verilog.cpp`）矛盾 | blocker | ✅ **采纳** ⇒ 顺序约束段改**函数级**清单 + 明写「`codegen_verilog.cpp` **文件级重叠**」⇒ 结论（可并行）保留 |
| 5 | P5「P4 裁不支持」时分支 (ii) 无拒绝落点 | blocker | ✅ **采纳** ⇒ 出口判据新增第 ② 段：`r.ok == false` **且** `file_exists == false`（发射器 collect 处 fail-closed） |
| 6 | P4 的 `grep -c "实参视线" == 1` 可漏判也可凑数 | quality | ✅ **采纳** ⇒ 改**双向 grep**（新句 ≥ 1 **且** 旧单句 == 0 **且** 两分句各自可 grep） |
| 7 | P10 写死数量 ⇒ 下次 CLI 升级再漂移 | quality | ✅ **采纳** ⇒ 改「**规则名 + 执行日期 + 计数快照**（不承诺长期有效）」 |
| 8 | 「实测」多数不给可复跑命令 ⇒ 行号会随重构漂移 | quality | ✅ **采纳（部分）** ⇒ 新增 §「本轮修订的复核命令」（可复跑 grep 清单）；行号引用保留（仓内既有风格）⇒ ⇒ **不逐行改写**（避免噪声） |
| 9 | §② 表与任务行重复叙述 ⇒ 两处必然漂移 | quality | ✅ **采纳** ⇒ §② 表压缩为「结论 + 可 grep 符号」，完整论证只留在任务行 |
| 10 | P8「golden 覆盖报出分子/分母」是空判据（5/39 也绿） | blocker | ✅ **采纳** ⇒ 加**覆盖下限 = 5/39（防回退）** + 「新增 JitOp MUST 附 golden 或登记理由」+ 出口 ③ 改**全仓 grep 命中数 == 0** |

#### 其余 finding（**已路由 / 已合并**）

| finding | 处置 |
|---|---|
| **ADR 修订流程缺失**（P7 无版本行 / 无审稿记录 / 无 oracle→critic） | ✅ **采纳** ⇒ P7 出口 ① 补「ADR-060 修订行 + 审稿记录 + oracle→critic 0 BLOCKER」（与 P4 对齐） |
| **ADR-060 §2.1 补格被指派给 builder** ⇒ 越权（ADR-060 已 Accepted） | ✅ **采纳** ⇒ 补格**移入 P4（architect）**；improvement In Scope 4 / A5 已相应改标 |
| **台账闭口没写终态**（状态 / 归属 / 闭口证据 / 复核条件） | ✅ **采纳** ⇒ P5 与 P7 的出口判据写死终态字段与取值 |
| **P8 旧引用清理清单不全**（ADR-005:313 / ADR-035:234 / JIT_ROADMAP:216 / usage_guide:141 / 根 AGENTS.md:313） | ✅ **采纳** ⇒ 出口 ③ 改**全仓 grep 命中数 == 0** + 逐处点名 |
| **P10 漏了 `AGENTS.md:193`**（「CI 校验：openspec validate 必须在 archive 前通过」与 P10 前提冲突） | ✅ **采纳** ⇒ P10 出口 ② 补该行订正 |
| **P15 的 ✅ 与 P10 的 exit 1 直接矛盾**（判据表述过宽） | ✅ **采纳** ⇒ P15 行加**口径订正注记**（实际满足的是 change / capability 级）⇒ ⇒ **不撤回 ✅**，但口径以注记为准 |
| **P9 的 line-count 义务漏写**（`include/cpu/AGENTS.md` 的「30 headers, 6506 lines」） | ✅ **采纳** ⇒ P9 出口 ④ 补行数同步 |
| **P5 的 A15 值判据只覆盖读端口**（写端口 `wdata`/`en` 同样跨层） | ✅ **采纳** ⇒ P5 出口 ③ 补「**读 addr** 与**写 wdata/en** 两侧」 |
| **P6「固件覆盖三类语义」不是判据** | ✅ **采纳** ⇒ P6 出口 ② 具体化为「新 ctest target + 手推期望值 + `CTEST_TOTAL` + `OP_SYSTEM` 引用数 > 0」 |
| **P5 / P7 / P8 / P9 / P10 的边界情况清单**（孙组件仍拒 · 父层节点不误报 · 读写不同侧 · 多 `ch_mem` · `aread`/`sread` 两类 · lint 绿≠值对 · `CH_JIT_ENABLE=OFF` 两种构建 · AXI 地址稳定性 · 注释确定性 · 顶层 `synthesized_ports` 必空） | ✅ **采纳（路由）** ⇒ 按本文件 §推进机制 第 3 条「实现细节 MUST 落在各自 change 的 proposal/tasks」⇒ ⇒ **不写进本文件** ⇒ ⇒ 边界清单落 **P3 的 improvement 草案**（`A8` / `A9` 已补）⇒ ⇒ P5 出口 ④ 只留一条**路由句** |
| **「可开工前置」显式化**（P4/P5 加「本条无未决子项」） | ✅ **采纳** ⇒ P4 出口 ① 已补 |

---

## 本轮修订的**复核命令**（可复跑 · 逐条 re-derive 上面各行的「实测」）

```bash
# P4（ADR-066 边界句存在 + 旧单句清零）—— ⚠️ MUST 限定 §2.7 区间 + markdown 不敏感
awk '/^### 2.7/,/^---/' docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md | grep -c '实参视线'
awk '/^### 2.7/,/^---/' docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md | grep -c '维持.*响亮拒绝'
#   ⇒ ⇒ 裸 `grep -c 'MUST 维持响亮拒绝'` 今天就恒为 0（旧句带 `**` 强调符）⇒ ⇒ 判据会恒真

# P5（分析层 mem 闸门 · 仓内「mem 端口实参跨层」形态）
grep -n 'cross-module ch_mem access' src/hierarchy_analysis.cpp
grep -n 'reg_file.aread\|reg_file.write' include/cpu/pipeline/rv32i_pipeline.h

# P7（PortPolicy 的读取点 + 成功路径的注释位）
grep -n 'port_policy' src/hierarchy_verilog.cpp include/hierarchy_verilog.h
grep -n 'synthesized_ports' src/hierarchy_verilog.cpp src/codegen_verilog.cpp | head

# P8（stub 残留清单 · 实施后应为 0 命中）
grep -rn 'set_ab_verification\|is_ab_verification\|ab_verification_' include/ src/ docs/ AGENTS.md

# P9（死成员 + 零消费者证明）
grep -n 'buffer\[' include/cpu/cache/d_cache_write_buffer.h
grep -n 'prefetch_data\|prefetch_addr\|prefetch_valid' include/cpu/cache/i_cache_prefetch.h
git grep -n 'WriteBufferEntry\|prefetch_valid' -- include src tests examples

# P10（CLI 漂移快照）
openspec --version
openspec validate --specs --strict | tail -1

# P6（OP_SYSTEM 的引用数 · 现为 1 = 只有定义）
grep -rn 'OP_SYSTEM' include/ | wc -l
```


## 推进机制（**本文件是路径的唯一真相**）

1. **进入下一步的门槛** = 上一段的**出口判据全绿**（**机械可判** ⇒ 必须落成命令或护栏）。
2. **完成一步** ⇒ 在本文件把该行的 ⬜ 改成 ✅ **并写上证据路径**（不写证据 = 未完成）。
3. **实现细节** MUST 落在各自 change 的 `proposal.md` / `design.md` / `tasks.md` ⇒ ⇒ **单点真相**原则不变
   ⇒ ⇒ 本文件**只管顺序与门槛**，不复制实现规格。
4. **涉架构残留的步骤** ⇒ MUST 同步 `.rddf/checks/arch-residual-registry.md`（唯一真相）+ 台账 §7（派生）
   ⇒ ⇒ 护栏 `scripts/verify_arch_ledger_residuals.sh` + `--self-test` 必须绿。
5. **每完成一步** ⇒ 回填本文件 + 更新对应 feature 的完成度（★ `feat-*.md` 的 front-matter 是
   **两份**真相 ⇒ 改动 MUST 同步根 `AGENTS.md` 的 AUTO 块 ⇒ ⇒ **同步手段**：
   `bash ~/.agents/skills/rdd-workflow/skills/roadmap/scripts/roadmap_refresh_fragments.sh`
   （**同时**刷新 `.rddf/roadmap.md` 的 AUTO-INDEX 段 + `AGENTS.md` 的 AUTO 块）
   ⇒ ⇒ ⚠️ `rddf roadmap --update-agent-md` 的 dispatch 按**项目根**找 `skills/` ⇒ 本仓未 vendor
   ⇒ ⇒ **直接调用全局 payload**（2026-11-09 实测 ⇒ 详见 §「本窗口自身的两个已知缺口」的 **G1**）。

---

## 明确不做（**本窗口的边界** · MUST NOT 静默即兴）

| 项 | 依据 |
|---|---|
| 跨多层（孙组件）的父侧落点 | ADR-066 决策 ⑥（**另立裁定**） |
| **支持**跨模块 `ch_mem` | ADR-066 决策 ⑥ + 决策 ⑦ ⇒ ⇒ ⚠️ **P4/P5 的「正确支持」分支只覆盖「mem **本体**同 module、仅**实参视线**跨层」这一支**（且**条件于 P4**）⇒ ⇒ 「mem **本体**跨 module」**仍拒**（不变） |
| macOS / Windows CI | 2026-11-03 用户裁决：CI 平台范围 = **仅 Ubuntu** |
| ADR-036 的物理移除 | **日历锁**（最早 2027-04-17）⇒ ⇒ P11 只登记，不提前 |
| 「双后端覆盖」的**批量扩张** | objective `objective-test-coverage-100pct` 已 **completed** ⇒ ⇒ 后续**按缺陷驱动**推进（历史三例真 bug 都是**全绿时**挖出的）⇒ ⇒ **不排期** |
| 仓级 `openspec validate --specs --strict` **全绿** | 1.14.x 新增的两条 strict 规则 ⇒ ⇒ **红项与仓内改动无关**（CI 也不跑该命令）⇒ ⇒ **只登记、不消除**（见 **P10**） |

---

## 与既有簿记的映射（**谁指向谁**）

| 本文件步 | 台账行 | feature / objective | improvement / ADR |
|---|---|---|---|
| P1 | — | `feat-hierarchical-rtl-output` | ADR-066 §5 **V8 面 ①** |
| P2 | — | `feat-rv32i-isa-completeness` · `feat-hierarchical-rtl-output` | ADR-048（skid buffer） |
| P3–P5 | **行 20**（已裁决 · 归下游） | `feat-hierarchical-rtl-output` | ADR-066 **决策 ⑦** |
| P6 | — | `feat-rv32i-isa-completeness`（**仅「ISA 领域」归属**；CSR / `ECALL` / `FENCE` **MUST 独立立项** ⇒ 新 capability + 新 change） | （新 ADR 或 spec 新增） |
| P7 | **行 15**（已知 · 归属 builder） | `feat-hierarchical-rtl-output` | **ADR-060 v1.18**（§2.4.1.1 产物注释 + v1.7/v1.8 对齐）· 审稿 `.rddf/reviews/ADR-060-port-policy-v1.18.md` |
| P8 | — | `feat-dual-backend-coverage` | **ADR-035 v2.2**（R9 改记「已裁定 · ⏳ 待实施」+ 残余风险 + 下游判据）· 审稿 `.rddf/reviews/ADR-035-verilator-backend-v2.2.md` |
| P9 | — | `feat-riscv-mini-rtl-debt` · `feat-cache-rewrite` | change `2026-11-09-cache-wrapper-dead-code-removal-2026-11`（已归档）· capability `cpu-cache-subsystem` ADDED ×1 |
| P10 | — | `feat-hierarchical-rtl-output`（「全程」条的**注记数字**订正 · 仓级 validate 全绿**不**在本窗口） | — |
| P11 | — | `feat-hdl-logical-deprecation` | ADR-036 |
| P12–P15 | — | —（**框架级**缺口 ⇒ 暂不挂 feature） | 上游 issue #28 ⇒ ② 待 ADR-067（**P13**）· ① 无需 ADR（**P14**） |

---

## ⚠️ 本窗口自身的两个已知缺口（**如实登记**）

| # | 缺口 | 影响 | 处置 |
|---|---|---|---|
| **G1** | 本文件与既有簿记（台账 §7 / feature / objective / improvement / ADR）的**关联没有机械载体**；其中 **feature 的 `status` 变更**还牵动根 `AGENTS.md` 的 AUTO 块（+ `.rddf/roadmap.md` 的 AUTO-INDEX） | 关联**漂移无声** —— 改了台账 / feature 后本文件的映射表**不会**跟着变；AUTO 块若**手工**维护 ⇒ 手工 = 会漂（历史上两次由人工发现） | ⇒ ⇒ 关联改用本文件 §「与既有簿记的映射」承担 ⇒ ⇒ ⚠️ 但 **P2 收尾时 feature 的 `status` 变更会牵动 AUTO 块** ⇒ ⇒ 那一处**必须**同步 ⇒ ⇒ **同步手段已存在**（2026-11-09 实测 ⇒ 见下方修复注记）⇒ ⇒ ⇒ **不再需要手工维护** ⇒ ⇒ ⇒ 本条已由「独立待办」**降级为「有工具，但需知道怎么调」** |
| **G2** | 本文件的**步状态**（⬜/✅）**无机械判据** | 与「护栏 = 机械可判」的仓内主流做法不一致 | 本文件是**规划**工件（不是验收判据）⇒ ⇒ 真正的判据在各自 change 的 `tasks.md` + 护栏 ⇒ ⇒ 本文件的 ✅ MUST 与那些判据同源（不新增判据面）⇒ ⇒ **不新建护栏**（避免又多一处需自证的活件） |

> ⚠️ **2026-11-09 修复（G1 行曾残缺 + 「CLI dispatch 缺失」结论已过时）**：
> ① 该行在 `e03dbb9` 引入时只剩**处置列的尾段**（表格首三格丢失 ⇒ 渲染出无 `|` 起始的悬空行）。
>    缺口 / 影响两列按本文件 §推进机制 第 5 条（`:142-146`）与 `scripts/verify_roadmap_fragments.sh`
>    的前言**重建**；**处置列逐字保留**（未改一字）。同步修：§推进机制 第 5 条的 `见 P12`
>    改为指向本表 G1（P12 **当时**不存在 —— 步号只有 P1–P11 + P2b；**2026-11-09 追加 P12–P14** ⇒
>    但该行的悬空引用**仍然**指向 G1 —— P12 是**另一件事**（下游编译破坏的框架级缺口 · issue #28））。
> ② ⚠️ **「`rddf roadmap --update-agent-md` 的 CLI dispatch 在本仓缺失」这条结论已过时**
>    （2026-11-09 实测）：dispatch **存在**（工具链 **rdd-workflow v4.0.0** @
>    `d5376087`）⇒ 但它按**项目根**解析 `skills/roadmap/scripts/…` ⇒ 本仓**未 vendor** `skills/`
>    ⇒ ⇒ 报「script not found」⇒ ⇒ **直接调用全局 payload 即可**：
>    `bash ~/.agents/skills/rdd-workflow/skills/roadmap/scripts/roadmap_update_agent_md.sh`
>    （⇒ 重写根 `AGENTS.md` 的 AUTO 块）
>    `bash ~/.agents/skills/rdd-workflow/skills/roadmap/scripts/roadmap_refresh_fragments.sh`
>    （⇒ **同时**重写 `.rddf/roadmap.md` 的 AUTO-INDEX 段 + `AGENTS.md` 的 AUTO 块）
>    ⇒ ⇒ **实测**：两者产出的 AUTO 块与已提交块**逐字节一致** ⇒ ⇒ ⇒ 护栏
>    `scripts/verify_roadmap_fragments.sh` 的 **vendored 渲染规则仍然等价**
>    （其「rdd-workflow 升级 ⇒ MUST 重跑 oracle」的**再验证触发条件已满足并已复跑**）
>    ⇒ ⇒ ⇒ 「AUTO 块只能手工维护」的**必要性已消除**（护栏仍保留 ⇒ 它判的是
>    「front-matter ↔ AUTO 块是否一致」，与生成器**互补**）。
