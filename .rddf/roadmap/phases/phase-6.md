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

**承接**：
① `2026-11-09-hierarchical-parent-side-landing-implementation`（父侧落点语义落地 · 已归档）
—— 它的 **3 项直接遗留** = 本窗口的 **P1 / P3–P5**；
② ⚠️ **phase-5 在 2026-10-26 修订里已把两条转交本窗口**（**既有约定，不是新裁定**）：
「ADR-036 逻辑算子的**物理移除**」（日历锁 · 最早 2027-04-17）⇒ ⇒ **P11**；
「Windows-2022 CI 启用」⇒ ⇒ ⚠️ **该条已被 2026-11-03 用户裁决关闭**（CI 平台范围 =
**仅 Ubuntu**）⇒ ⇒ **P2b**（只登记理由 + 关闭，不实施）。
—— 它的 **3 项直接遗留** = 本窗口的 **P1 / P3–P5**。

---

## 任务路径（**执行顺序**）

> ⚠️ **顺序约束**：P5 必须在 P7 **之前**（两者都动分析层的形态判定 ⇒ 同批做会互相掩盖）
> ⇒ ⇒ P7 与 P5 **不并行**；其余各步**互不阻塞**，可并行。

| 步 | 任务 | 归属 | 前置 | 出口判据（**机械可判**） | 状态 |
|---|---|---|---|---|---|
| **P1** | 补 **A8 面 ①** 的机械证据：「`node_names_[` 的 **62 行 / 64 次**无需改动」⇒ ⇒ **grep 清单 + 一次负向注入**（把某处名称换成**未在父作用域声明**的名字 ⇒ lint 必红） | builder | — | 证据脚本**可复跑**；负向注入使 **R9 lint 必红**、还原后复绿 | ✅ **2026-11-09** ⇒ 证据脚本 = `scripts/verify_node_names_single_source.sh`（R1 清单 62 行/64 次 · R2 单写入点 · R3 命名真源同源 · R4 **源码级**注入 ⇒ R9 必红 ⇒ 还原复绿 · `--self-test` **5/5**）· 记录 = `.rddf/reviews/hierarchical-parent-side-landing-implementation-2026-11.md` §A–§H · CI = `build-and-test` guard 段（只读 + `--self-test`；`--inject` 不上 CI） |
| **P2** | ★ **roadmap 事实核对**（★ 范围比预期更广，见下） | planner | — | ① feature 与 phase 的状态**逐条**与 `AGENTS.md`/capability/归档件一致；② `verify_roadmap_fragments.sh` 绿；③ **phase-5 的 `status` 可据实转 `done`** | ✅ **2026-11-09** ⇒ 核对记录 = `.rddf/reviews/phase-6-p2-roadmap-audit-2026-11.md`（逐条工具证据 · 9 feature + 6 phase + 4 objective）· 改动 = `phases/phase-5.md`（**转 `done`** + 概览表 4 行 + 完成条件 3 勾 1 不做）+ `features/feat-rv32i-isa-completeness.md`（审计快照注记）+ 本文件漂移清单订正（**① / ② 实测已不成立** · 新增 **⑥**）· 护栏 `verify_roadmap_fragments.sh`（+`--self-test`）/ `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 全绿 · ⚠️ **AUTO 块本轮无需改**（未动任何 feature 的 `status`/`phase_refs`/`主题` ⇒ 该义务是**条件触发**） |
| **P2b** | ⚠️ **Windows-2022 CI**（phase-5 于 2026-10-26 转交）⇒ ⇒ **2026-11-03 用户裁决：CI 平台范围 = 仅 Ubuntu** ⇒ ⇒ **不做** ⇒ ⇒ 只**登记理由 + 关闭该完成条件**（不动 CI） | planner | — | phase-5 完成条件里该项标为「不做（附裁决日期）」 | ✅ **2026-11-09** ⇒ `phases/phase-5.md` 的完成条件已改为 `- [x] **不做**（2026-11-03 用户裁决）` + No-Go 判据的处置说明 · **CI 文件零改动**（实测 `runs-on` = 5×`ubuntu-22.04` + 1×`${{ matrix.os }}`，其取值 `['ubuntu-22.04']`） |
| **P3** | **mem 端口实参跨层**（台账行 20 的下游）：起草 improvement（5 段 · Why / What Changes / Acceptance / Capabilities / Impact）⇒ ⇒ 过 `pre_create_brainstorm_check.sh` HARD-GATE | planner | — | improvement 文件存在且过 HARD-GATE（exit 0） | ✅ **2026-11-09** ⇒ 草案 = `.rddf/improvements/mem-port-arg-cross-layer-2026-11.md`（**7 段**（架构依据 / 范围 / Why / What Changes / Acceptance / Capabilities / Impact）+ YAML frontmatter · **HARD-GATE exit 0**）· 已注册进 `improvement-suggestions.md` · ⚠️ 状态 = `draft` ⇒ ⇒ **待 P4 的 architect 范围裁定**落地后才可转 `approved` ⇒ 才可立项为 change · ★ **口径（用户 2026-11-09 裁决）**：「粒度拆分 + 归因订正」= **无条件必做**；「正确支持」这一支 = **条件化**于 P4 的边界句（⇒ ⇒ 本 improvement **不替 architect 拍板**） |
| **P4** | 同上：**architect 范围裁定** ⇒ ⇒ ★ **边界句 MUST 写死** ——「mem 持有 module **==** 使用 module、仅**实参视线**跨层」⇒ ⇒ 否则 ADR-066 决策 ⑥「跨模块 `ch_mem` 仍拒」会被逐案重解释 | architect | P3 | ADR 修订落地 · **oracle → critic** 双审 **0 BLOCKER** | ⬜ |
| **P5** | 同上：**实施** —— 分析层 `:543` 的**粒度拆分**（mem 端口**实参**跨层 vs 真跨模块访问）+ 归因订正（诊断 MUST 给出**正确**改法）+ mem 端口**正确支持** + **A15**（端口**读值非 0**，不是只 lint） | builder | P4 | ① mem 端口读值**非 0**；② 跨模块 mem 访问**仍被拒**；③ 台账行 **20** 闭口（登记表 + 台账派生 + 护栏绿） | ⬜ |
| **P6** | ★ **RV32I ISA 缺口**：**CSR**（`csrrw`/`csrrs`/`csrrc`）+ **`ECALL`/`EBREAK`** + **`FENCE`**（不含预测取指）⇒ ⇒ **独立立项**（`AGENTS.md` KNOWN LIMITATIONS 明写「不在任何仓内 capability 覆盖内 ⇒ 不可顺手塞进本条」） | planner → builder | — | 固件覆盖三类语义 + 双后端 parity + capability 新增 | ⬜ |
| **P7** | **台账行 15**：三档 `PortPolicy` 在**可合成形态**上的语义**分化**（`Reject` 拒可合成形态 / `SynthesizeFatal` 先合成后失败 / 默认 `SynthesizeWarn` 的 CHWARN + 产物注释义务） | architect → builder | **P5** | 三档在可合成形态上行为可区分 + 文档/注释义务落地 + 台账行 15 闭口 | ⬜ |
| **P8** | **`Simulator::set_ab_verification()` 是 no-op stub**（ADR-035 R9）⇒ ⇒ A/B（interpreter vs JIT）比对**从未真正实现** ⇒ ⇒ 重设计（快照 + 逐指令对账 + 归因） | planner → architect → builder | — | A/B 比对**真的**逐指令对账 + 断言；stub 文案删除 | ⬜ |
| **P9** | **cache wrapper 三项已知债务**：`DCacheWriteBuffer` **不真缓冲**（名字 over-promise ⇒ `ready` 无背压语义）· `ICachePrefetch` 预取缓冲是**死数据** · 内层 cache `miss` 与 `axi_addr` **1 拍错位** ⇒ ⇒ 各自闭口**或**降级为「已知」并登记理由 | planner → builder | — | 三项各自有裁决 + 证据 | ⬜ |
| **P10** | **环境**：openspec CLI **钉回 1.4.1**（仓库按 1.4.1 写就；本机 1.14.x ⇒ 15/17 主 spec 红 ⇒ **与本仓改动无关**）⇒ ⇒ 解 `feat-hierarchical-rtl-output`「全程」条目的唯一红项 | 环境（非代码） | — | 仓级 `openspec validate --specs --strict` 绿 | ⬜ |
| **P11** | **ADR-036 逻辑算子物理移除**（`&&` / `\|\|` ⇒ `&` / `\|`） | builder | 🔒 **日历锁：最早 2027-04-17**（`verify_deprecation_calendar_gate.sh` 守） | 移除完成 + `verify_deprecation_callsites.sh` 绿 | 🔒 |

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
**P2 的收尾必须包含 AUTO 块同步** ⇒ ⇒ 同步手段（`rddf roadmap --update-agent-md` 的 CLI
dispatch 在本仓**缺失**）本身是**独立待办**）。

---

## 每步的**证据落点**（**闭口证据 = 可 grep 的路径**）

| 步 | 证据落点 |
|---|---|
| P1 | `.rddf/reviews/hierarchical-parent-side-landing-implementation-2026-11.md`（追加段）· 变异/注入记录 |
| P2 | `.rddf/reviews/phase-6-p2-roadmap-audit-2026-11.md`（核对记录）+ `phases/phase-5.md`（**转 `done`**）+ `features/feat-rv32i-isa-completeness.md`（审计快照注记） |
| P2b | `phases/phase-5.md` 的完成条件（该项标「不做（附裁决日期）」）+ 本文件的结论行 |
| P3 | `.rddf/improvements/mem-port-arg-cross-layer-2026-11.md`（草案 · HARD-GATE exit 0）+ `improvement-suggestions.md` 的索引行 |
| P4 | `docs/adr/ADR-066-...md`（修订）+ `.rddf/reviews/ADR-066-parent-side-landing.md`（追加段） |
| P5 | `openspec/changes/archive/<mem-port-实参>-<日期>/` + `.rddf/checks/arch-residual-registry.md`（行 20）+ 台账 §7 |
| P6 | `openspec/changes/archive/<isa-缺口>-<日期>/` + 新 capability spec |
| P7 | 同上 + 台账行 15 |
| P8 | 同上 + `include/simulator.h` / `src/simulator.cpp` 的文档同步 |
| P9 | 同上 + `include/cpu/AGENTS.md` 的债务段更新 |
| P10 | （环境项）⇒ 证据 = `openspec validate --specs --strict` 的输出 |

---

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
| **支持**跨模块 `ch_mem` | ADR-066 决策 ⑥ + 决策 ⑦（P4/P5 只做**粒度拆分 + 归因订正**） |
| macOS / Windows CI | 2026-11-03 用户裁决：CI 平台范围 = **仅 Ubuntu** |
| ADR-036 的物理移除 | **日历锁**（最早 2027-04-17）⇒ ⇒ P11 只登记，不提前 |
| 「双后端覆盖」的**批量扩张** | objective `objective-test-coverage-100pct` 已 **completed** ⇒ ⇒ 后续**按缺陷驱动**推进（历史三例真 bug 都是**全绿时**挖出的）⇒ ⇒ **不排期** |

---

## 与既有簿记的映射（**谁指向谁**）

| 本文件步 | 台账行 | feature / objective | improvement / ADR |
|---|---|---|---|
| P1 | — | `feat-hierarchical-rtl-output` | ADR-066 §5 **V8 面 ①** |
| P2 | — | `feat-rv32i-isa-completeness` · `feat-hierarchical-rtl-output` | ADR-048（skid buffer） |
| P3–P5 | **行 20**（已裁决 · 归下游） | `feat-hierarchical-rtl-output` | ADR-066 **决策 ⑦** |
| P6 | — | `feat-rv32i-isa-completeness`（**仅「ISA 领域」归属**；CSR / `ECALL` / `FENCE` **MUST 独立立项** ⇒ 新 capability + 新 change） | （新 ADR 或 spec 新增） |
| P7 | **行 15**（已知 · 归属 builder） | `feat-hierarchical-rtl-output` | ADR-060 §2.4.1 |
| P8 | — | `feat-dual-backend-coverage` | ADR-035 R9 |
| P9 | — | `feat-riscv-mini-rtl-debt` · `feat-cache-rewrite` | — |
| P10 | — | `feat-hierarchical-rtl-output`（「全程」条目） | — |
| P11 | — | `feat-hdl-logical-deprecation` | ADR-036 |

---

## ⚠️ 本窗口自身的两个已知缺口（**如实登记**）

| # | 缺口 | 影响 | 处置 |
|---|---|---|---|
| **G1** | 本文件与既有簿记（台账 §7 / feature / objective / improvement / ADR）的**关联没有机械载体**；其中 **feature 的 `status` 变更**还牵动根 `AGENTS.md` 的 AUTO 块（+ `.rddf/roadmap.md` 的 AUTO-INDEX） | 关联**漂移无声** —— 改了台账 / feature 后本文件的映射表**不会**跟着变；AUTO 块若**手工**维护 ⇒ 手工 = 会漂（历史上两次由人工发现） | ⇒ ⇒ 关联改用本文件 §「与既有簿记的映射」承担 ⇒ ⇒ ⚠️ 但 **P2 收尾时 feature 的 `status` 变更会牵动 AUTO 块** ⇒ ⇒ 那一处**必须**同步 ⇒ ⇒ **同步手段已存在**（2026-11-09 实测 ⇒ 见下方修复注记）⇒ ⇒ ⇒ **不再需要手工维护** ⇒ ⇒ ⇒ 本条已由「独立待办」**降级为「有工具，但需知道怎么调」** |
| **G2** | 本文件的**步状态**（⬜/✅）**无机械判据** | 与「护栏 = 机械可判」的仓内主流做法不一致 | 本文件是**规划**工件（不是验收判据）⇒ ⇒ 真正的判据在各自 change 的 `tasks.md` + 护栏 ⇒ ⇒ 本文件的 ✅ MUST 与那些判据同源（不新增判据面）⇒ ⇒ **不新建护栏**（避免又多一处需自证的活件） |

> ⚠️ **2026-11-09 修复（G1 行曾残缺 + 「CLI dispatch 缺失」结论已过时）**：
> ① 该行在 `e03dbb9` 引入时只剩**处置列的尾段**（表格首三格丢失 ⇒ 渲染出无 `|` 起始的悬空行）。
>    缺口 / 影响两列按本文件 §推进机制 第 5 条（`:97-99`）与 `scripts/verify_roadmap_fragments.sh`
>    的前言**重建**；**处置列逐字保留**（未改一字）。同步修：§推进机制 第 5 条的 `见 P12`
>    改为指向本表 G1（P12 不存在 —— 步号只有 P1–P11 + P2b）。
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
