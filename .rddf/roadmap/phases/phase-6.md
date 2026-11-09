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
| **P12** | **上游 issue #28**（下游 ChipForge 反馈 · **新缺口**）：起草**两项** improvement（**拆项** —— ② 需架构裁定 / ① 不需 ⇒ 不被审稿门卡住）⇒ ⇒ 过 `pre_create_brainstorm_check.sh` HARD-GATE | planner | — | 两个 improvement 文件存在且各过 HARD-GATE（exit 0） | ✅ **2026-11-09** ⇒ ② = `.rddf/improvements/chlib-dtor-noexcept-composability-2026-11.md`（**析构异常规格 vs 可组合性**）· ① = `.rddf/improvements/public-api-remove-visibility-2026-11.md`（**API 移除的跨仓可见性**）⇒ 两者均 **7 段** + YAML frontmatter · **HARD-GATE exit 0** ⇒ 已注册进 `improvement-suggestions.md` · ⚠️ 两者状态 = `draft` ⇒ ⇒ **② 待 P13 的 architect 裁定**后才可转 `approved`；**① 不需裁定**（★ 唯一待用户拍板项 = CHANGELOG 载体命运 R1/R2/R3）· ★ **metis 第 1 轮**（12 条 finding · 4 blocker）已逐条处置 ⇒ 处置记录 = 本文件 §「本窗口自身的两个已知缺口」后的 **P12 订正段** |
| **P13** | ② 的 **architect 架构裁定** ⇒ ⇒ ADR-067：析构**异常规格** × 检出**触发点** 的取舍（选项 A / A′ / B / C / D / E —— **两处分别裁定**：FSM 析构**只检查** vs `conditional_block` 析构**做事** ⇒ **不同构** ⇒ 不可一把钥匙） | architect | P12 | ADR-067 落库 · **oracle → critic** 双审 **0 BLOCKER** · 审稿记录落 `.rddf/reviews/` | ✅ **2026-11-09** ⇒ ADR = `docs/adr/ADR-067-dtor-exception-spec-composability.md`（**Accepted** · **用户拍板**）⇒ ⇒ **裁决**：① 两处分别裁定 ② FSM 默认形态**逐字不变** + **显式**提供「可组合形态」（noexcept 析构 + 显式 `check_unresolved()` + 非展开期 terminate 兜底 fail-closed）③ `conditional_block` **维持** `noexcept(false)`（**可行但当前不做** · 无需求 + 固定成本 + 基线缺失）④ 使用契约（精确措辞）⑤ 机械判据（`is_nothrow_destructible_v` 三连 + 变异自检）⑥ 展开期门复制 ⑦⑧⑨ 不做项（elaboration 扫描 / 恢复 `is_built()` / 删拷贝）⇒ 审稿 = `.rddf/reviews/ADR-067-dtor-composability.md`（oracle 6 条 + critic 9 条 **逐条应答** · 第 1 轮 1 BLOCKER ⇒ 复审 `CLOSED` / `mergeable: yes`）⇒ ★ 评审推翻了我方两处错误（「5 处」→「**8 处**」· `static_assert(!is_nothrow_destructible_v<std::optional<Sm>>)` **恒假** ⇒ 改**合取**形态 ⇒ 编译实锤） |
| **P14** | ① 的**实施**：CHANGELOG 载体（按裁定 R1/R2/R3）· Zero-Debt **判决规则边界**（根 `AGENTS.md` ZERO-DEBT 段 +1 句 · **AUTO 块不动**）· 活账 `.rddf/checks/public-api-surface.md` 扩到**符号级**（含**移除记录**字段 + R4② 双向核对）· **消费者 fixture 进上游 CI**（install 到临时 prefix ⇒ 独立 CMake 工程编译下游视角 TU · **fail-closed**） | builder | P12 | ① CHANGELOG 判据（token 清单 / 计数守恒）绿；② 判决规则句 `grep` 可判；③ R4② 符号级**原地投毒**必红；④ 消费者 fixture **缺 = 失败** + 计数锚点（`verify_test_counts.sh` 绿） | ✅ **2026-11-09** ⇒ change = `openspec/changes/archive/2026-11-09-public-api-remove-visibility/`（**已归档** · 无 spec 变更 ⇒ `--skip-specs`）⇒ ⇒ 用户裁定 = **R1 + R2**（逐 change 一行 + 一次性回填） ⇒ 载体 = ① `docs/CHANGELOG.md` 的 2026-11-09 条目（回填 2026-10-15 以来的用户可见破坏性变更 + 本窗口的「可组合形态」）② 每个**机制生效后**归档的 change 的 `## 用户可见变更` 段（首个例证 = P15 的归档件） ⇒ 判决规则边界 = 根 `AGENTS.md` ZERO-DEBT 段 +1 句（AUTO 块不动 ⇒ `verify_roadmap_fragments.sh` 绿）+ ADR-050 / ADR-043 的**反向指针**（决策内容零改动）⇒ 活账 = `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」章（11 行 + 已弃用表）+ `.rddf/checks/public-api-removals.tsv`（**15 条机器探针** · 四种 kind · 剥注释后判定）⇒ 护栏 = **`scripts/verify_public_api_visibility.sh`**（R1 归档载体 / R2 移除登记 / R3 判决边界 + roadmap 守恒 / R4 CHANGELOG **token 清单** / R5 机械探针 / R6 缩面锚点）+ `--self-test`（M1–M4 原地投毒必红）· **`scripts/verify_downstream_consumer.sh`**（install → **临时 prefix** → **独立 CMake 工程** → **运行** · 三种持有形态：局部 / `unique_ptr` / **按值成员 + 虚析构基类**）+ `--self-test`（M1 投毒**已安装副本** ⇒ 下游 build 必红 = issue #28 的复现 · M2 空 fixture ⇒ fail-closed）⇒ 两者已挂 CI 的 `build-and-test` guard 段 ⇒ 判据 = `tests/downstream-consumer/{main.cpp,CMakeLists.txt}` + ctest 条目 `downstream_consumer`（`CTEST_TOTAL` 254→**255**）⇒ ⇒ **验证**：`ctest -LE perf` **244/244 全绿**·`verify_test_counts.sh` / `verify_public_api_visibility.sh`(+self-test) / `verify_downstream_consumer.sh`(+self-test) / `verify_roadmap_fragments.sh` / `verify_hierarchical_p8.sh` / `verify_openspec_delta.sh` 全绿 ⇒ ★ 副产品（已登记，不在本 change 修）= 安装后的头**不是单目录自足**的（`ast/ast_nodes.h` 引 `lnodeimpl.h`，而它装在 `core/`）⇒ 下游 MUST 加 5 个 `-I`（= 上游 `CH_INCLUDE_DIRS` 口径）
|
| **P15** | ② 的**实施**：按 ADR-067 裁定改析构异常规格 / 加 API（复制**展开期门**）· 2 份 capability delta（**禁** `--skip-specs`）· **可组合性的机械判据**（`tests/test_chlib_composability_api.cpp` · 用仓内 `ch::Component` 作基类 ⇒ 与下游同形）+ **变异自检** · 补「忘记 `.end()` ⇒ 析构期可捕获」基线用例 | builder | P13 | ① `static_assert(std::is_nothrow_destructible_v<Dut>>` 绿 + 变异投毒必红；② `verify_openspec_delta.sh` + `openspec validate --specs --strict` 绿；③ `CTEST_TOTAL` 锚点同步 | ✅ **2026-11-09** ⇒ change = `openspec/changes/archive/2026-11-09-chlib-dtor-composability-2026-11/`（**已归档** · spec delta 已合入主 spec：2 份 capability **ADDED ×1**）⇒ ⇒ 实现 = `include/chlib/state_machine.h`（新增 `dtor_detection` 枚举 + 第三模板实参 + `check_unresolved()` + 终止兜底 + 头注释「③ 析构形态与可组合性」段）· `include/chlib/if_stmt.h`（**仅注释**：使用契约）⇒ **`core/` 零改动** ⇒ 判据 = `tests/test_chlib_composability_api.cpp`（**新增 1 个 ctest target** ⇒ `CTEST_TOTAL` 253→**254**）· 护栏 = `scripts/verify_chlib_dtor_composability.sh`（R1 fixture 编译 / R2 API 面 / R3 两份头注释的契约 / R4 capability 契约）+ `--self-test`（M1/M2/M3 **原地投毒** ⇒ 必红）⇒ CI 已挂 `build-and-test` 的 guard 段 ⇒ ⇒ **验证**：`ctest -LE perf` **243/243 全绿**（基线 242 + 新增 1）· `verify_test_counts.sh` / `verify_openspec_delta.sh` / `verify_fsm_next_wiring.sh` / `verify_if_stmt_wiring.sh` / `verify_roadmap_fragments.sh` 全绿 · **M1 变异实测必红**（3 处：2 × `static_assert` + 1 × 真实「looser exception specification」编译错误）⇒ ⇒ 还原复绿 |

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
