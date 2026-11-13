# ADR-070: RV32I 层次化 RTL 输出的**成层粒度** + **发射器兄弟边修法**

- **Status**: **Accepted**（2026-11-13 · **oracle ×1**（第 4 轮 · 独立重推 + 对抗式复核）· **critic ×2**（第 1 轮 **REJECT** · 1 BLOCKER ⇒ 第 2 轮 **APPROVE** · 0 BLOCKER）· ⚠️ §6 ⑦ 的「契约强度」仍是**用户待裁决项** ⇒ ⇒ 该项**不因本 ADR 的 Accepted 而冻结**）
- **Date**: 2026-11-13
- **Author**: architect role（build agent）
- **Parent**: **ADR-060**（层次化端口策略与模块复用）· **ADR-061**（粒度用户可控）· **ADR-066**（父侧落点语义）· **ADR-046**（RV32I 收敛 · D10 SoC 端口契约冻结）· **ADR-059**（模块命名）
- **关联 change**: `backlog: rv32i-hierarchical-example-2026-11`（`.rddf/improvements/rv32i-hierarchical-example-2026-11.md` · **已立项 · 未开工** ⇒ ⇒ 立项为 change 后回填本指针 + AC 映射）
- **AC 映射**: 本 ADR §5 的 **V1–V10** → `rv32i-hierarchical-example` 的 **R21-A**（发射器）+ **R21-B**（成层）+ **R21-C**（验收面）⇒ ⇒ **R21-C** 的 3 条（逐拍等价 / 判别性自证 / 期望值手推）不在本 ADR 的 AC 面 ⇒ ⇒ 归 change 的 AC
- **审稿记录**: `.rddf/reviews/ADR-070-rv32i-hierarchical-layering-granularity.md`（**oracle ×1**（第 4 轮 · 独立重推 + 对抗式复核 · 确认「① 选中」+「⑤ 条件式可接受的三条件」+「D1 与 D2 正交」）· **critic ×2**（第 1 轮 **REJECT** · 1 BLOCKER = 「ADR-069 §7.1.2 的『MUST NOT 前移』与 ADR-047 §5.5.2 ③ 构成两份 Accepted ADR 的相反处方且 ADR-047 侧无反向指针」· 第 2 轮 **APPROVE** · 0 BLOCKER））

---

## 1. 背景与问题

### 1.1 诉求

用户 2026-11-10 拍板的 7 项 RTL 侧诉求中的 **④ = 交付一个 RV32I 的层次化 RTL 示例**（真实设计的
多 module Verilog 产物）。⚠️ 该诉求**至今未兑现** ⇒ ⇒ 本 ADR 是它的**架构裁决**。

### 1.2 实测事实（2026-11-13 · 可复算）

| # | 事实 | 证据 |
|---|---|---|
| ① | ⚠️ **RV32I 成层发射 = fail-closed · 23 条**跨 module 边无法映射（`Granularity::ForceLayer`） | `bash scripts/probe_r21_hier_granularity.sh` ⇒ `ok=false` + 23 条逐条清单 |
| ② | ✅ **根因不是「隐式端口合成未实现」** —— 合成链已存在且工作正常 | 诊断文本自陈「the synthesized chain would drive it from …」 |
| ③ | 🔴 **抛点** = `src/hierarchy_verilog.cpp:350-359` 的 `add_conn` **pin 冲突判据** —— 它把「分析层记录的**消费点**」当成「pin 的**实参**」 | 23 条**全部**落 `:689-702` 的**兄弟边分支** |
| ④ | 🔴 **IR 级根因** = 「同一个子输出有**两类**消费者」：① 兄弟子模块的输入端口 ② 父体内的运算表达式 ⇒ ⇒ 发射器把「分支」当「冲突」 | `top.pipeline.if_stage` 的 2 个输入端口 `src[0]` = `top.pipeline.hazard_unit.unnamed_output` |
| ⑤ | ⚠️ **判据不对称**：直接 子→父 路径**已**用 `replace_existing=true`（`:910-911`），而**兄弟边分支未**用 | 源码 |
| ⑥ | ⚠️ **第二处机制**：`parent_landing_wire`（键 = `module\x1finstance\x1fchild_port`）决定 `:1490-1514` 是否**丢弃**分析层旧行 ⇒ ⇒ 兄弟边分支**不填**它 ⇒ ⇒ 旧行**未被丢弃** ⇒ ⇒ 与 ③ 叠加 | 源码 |
| ⑦ | ⚠️ **RV32I 的 stage 实例只在库头内创建**（`rv32i_pipeline.h:119-128` 的 7 处 `ch::ch_module<T>`）⇒ ⇒ **example 侧把 stage 声明成层结构上不可能** | 源码 |
| ⑧ | ✅ **实例级成层 API 已存在**：`ch::ch_layer<T>`（`T` 未做类级声明时 MUST 在实例处给模块类型名）⇒ ⇒ **不需要**改类级声明 | `include/component.h:227` |
| ⑨ | ✅ **ADR-046 D10 冻结的是 SoC 顶层**的**端口集**（32 端口快照）⇒ ⇒ **模块类型名不是端口** ⇒ ⇒ 本 ADR 与 D10 **不冲突** | `docs/adr/ADR-046-rv32i-canonical-home.md` |

### 1.3 两个**独立**的问题

⚠️ **本 ADR 裁决两件事，它们互相正交**（**判据面必须分开**）：

| 问题 | 内容 | 写点 |
|---|---|---|
| **P-A** | 发射器**怎么判**一个 pin 是否冲突（**缺陷**） | `src/hierarchy_verilog.cpp` |
| **P-B** | RV32I **哪些组件成层**（**粒度**） | `include/cpu/pipeline/rv32i_pipeline.h` |

⇒ ⇒ **P-A 与任何成层粒度下正交**（任何非退化粒度都会产生落同一分支的兄弟边）
⇒ ⇒ ⇒ **P-A 的修法不依赖 P-B 的答案** ⇒ ⇒ ⇒ **P-A 可先行实施**。

### 1.4 **P-B 的额外约束**：成层声明的**落点**只有一个

⚠️ 事实 ⑦ ⇒ ⇒ **实例级声明（`ch_layer`）的落点只剩库内一个**（`rv32i_pipeline.h` 的 7 处成员声明）
⇒ ⇒ ⇒ **类级声明（`CH_HIERARCHY_LAYER`）会把决定推给每一个嵌入者** ⇒ ⇒ ⇒ 二者**不可互换**。

---

## 2. 决策

### 2.1 **D1**（P-A）= 发射器侧**判据修正**（**不是** 23 处源码单驱动化）

| 子项 | 处置 | 位置 |
|---|---|---|
| **H-A** | `add_conn` 的**冲突循环 MUST 方向感知**：**跳过** `c.into_child == false` 的既有行（那是「消费点」不是「实参」）⇒ ⇒ `into_child == true` 的双驱动**仍 fail-closed** | `src/hierarchy_verilog.cpp:350-359` |
| **H-B** | 兄弟边分支 MUST **填 `parent_landing_wire`**（键 = `lca\x1finstance\x1fchild_port`）⇒ ⇒ 分析层的**旧行**在 `:1490-1514` 被**丢弃** ⇒ ⇒ 避免与新行**并存** | `src/hierarchy_verilog.cpp:689-702` |

⚠️ **不变式（MUST 写进代码注释 + capability 的反向 Requirement）**：
**`into_child == true` 的双驱动永远 fail-closed** ⇒ ⇒ 理由 = IR 的 `set_src` 是 last-wins ⇒ ⇒ 放行 = 静默丢驱动。
⇒ ⇒ ⇒ 且 **H-A MUST NOT 放宽成「同 pin 多行不冲突」**（那会放过真正的双驱动）。

⚠️ **可观测性 MUST 用结构化字段**（`HierarchyEmitResult` 追加 `rewired_pins`），**不用** `CHWARN`
⇒ ⇒ 理由 = `CHWARN` 只进日志 ⇒ ⇒ **不可行动**（与既有 `R3.2` 的同型教训一致）。

⚠️ **「23 处源码单驱动化」被否**：它与 B3/B4 的写点**撞车**（`rv32i_pipeline.h`）⇒ ⇒ ⇒ 且它解决的是
**症状**（让 pin 只有一个消费者）而非**病根**（判据把消费点当实参）。

### 2.2 **D2**（P-B）= 成层粒度 = **选项 ①**（**实例级** `ch_layer`，7 处）

⇒ ⇒ `rv32i_pipeline.h:119-128` 的 **7 处** `ch::ch_module<T>` ⇒ ⇒ `ch::ch_layer<T>` + **显式模块类型名**：

| 实例名 | 模块类型名 |
|---|---|
| `if_stage` | `IfStage` |
| `id_stage` | `IdStage` |
| `ex_stage` | `ExStage` |
| `mem_stage` | `MemStage` |
| `wb_stage` | `WbStage` |
| `hazard_unit` | `HazardUnit` |
| `forwarding_mux` | `ForwardingMux<32>` |

⚠️ **只影响层次化产物** ⇒ ⇒ **扁平产物逐字节不变**（`ch_layer` 只写 `is_layer` / `module_type_name` 两个标志）
⇒ ⇒ ⇒ 扁平不变性是**结构性恒真**的跳线，**不是**判据。

⚠️ **时机**：**排在 B3 之后**（同一文件 ⇒ ⇒ 写点冲突）⇒ ⇒ **不与 B3 同批**。

### 2.3 **D3** = 7 个模块类型名 = **产物级契约名**（登记在 capability）

⚠️ **契约强度待用户裁决**（见 §6 ⑦）⇒ ⇒ 本节给出**本 ADR 的推荐取向**，**不冻结** ⇒ ⇒ capability 起草方 MUST 以 §6 ⑦ 的裁决为准。

⇒ ⇒ **载体 = capability `rtl-hierarchical-output` 的 MODIFIED**（新增一条 Scenario，点名 7 个模块类型名
为**产物级契约名** —— 改名 = **产物级破坏**）⇒ ⇒ + `docs/CHANGELOG.md` 一行
⇒ ⇒ `public-api-surface.md` **可选**（它们不是**端口** ⇒ ⇒ 不在 D10 的冻结面内 ⇒ ⇒ 见事实 ⑨）。

### 2.4 **D4** = 「**部分成层**」（原选项 ⑤）= **条件式可接受**，三条件**同时**成立才算数

| # | 条件 | 理由 |
|---|---|---|
| ① | **真实设计零复刻、零改动** | 复刻 = 第二份权威 ⇒ ⇒ 必然漂移 |
| ② | 交付粒度 = **声明式**（**不是** `ForceLayer`） | `ForceLayer` 演示的是**逃生门**不是声明式特性 |
| ③ | ⚠️ **产物中至少有一条真实边穿过修好的兄弟边分支** | 否则示例对 **D1 零判别** ⇒ ⇒ 退化成「报告级示例」 |

⇒ ⇒ **反例（必须点名）**：「只 `dtcm` 成层、`pipeline` inline」⇒ ⇒ `pipeline` 内容 inline 进 top 后
`dtcm ↔ pipeline` 的边变成**父子边**（行走行 1/行 2 机制）⇒ ⇒ **兄弟边数为 0** ⇒ ⇒ **不满足条件 ③**。

⇒ ⇒ **裁决**：**选 ①**（选项 ① 在边际成本上**支配** ⑤ —— ① 的成本 = 7 行 + 7 个名字登记；
⑤ 省下这 7 行，付出的是**更弱的示例** + 需重新论证 D1）⇒ ⇒ **⑤ 保留为退路**（若 ① 的 spike 撞出第二层失败）。

---

## 3. 选项对比表（P-B 的 6 个选项）

| # | 选项 | 做法 | **结论** | 理由（**真的更差**，不是「不如推荐项」） |
|---|---|---|---|---|
| **①** | **实例级 `ch_layer`**（7 处） | `rv32i_pipeline.h:119-128` | ✅ **选中** | 只影响 `Rv32iPipeline` 的装配产物 ⇒ ⇒ **不污染库外嵌入**；且**唯一**可行（事实 ⑦） |
| ② | example 侧复刻装配 | `examples/` 另建 7 个 stage 连线 | ❌ **拒** | 制造**第二份 stage 连线权威** ⇒ ⇒ 与 B3/B4 写点**直接冲突**且无护栏守同步 ⇒ ⇒ 必然漂移 ⇒ ⇒ 复活 ADR-046 消灭的形态 |
| ③ | tests-only 本地子类 | `tests/` 自建子类 | ❌ **拒** | **不可行** = 7 个 stage 是基类成员（事实 ⑦），子类无法重声明 ⇒ ⇒ 唯一实现 = ② 的换皮；且诉求 ④ 未兑现 |
| ④ | 类级 `CH_HIERARCHY_LAYER` | 7 个 stage 头各加类级声明 | ❌ **拒** | **决定权错配**到每个嵌入者 ⇒ ⇒ `ForwardingMux`（chlib 公共件）与 `HazardUnit`（根 `AGENTS.md` KNOWN LIMITATIONS 明文保留为「独立组件仍可用」）的嵌入面**超出 RV32I** ⇒ ⇒ 类级成层会改变所有独立装置的层次化产物 |
| ⑤ | 部分成层 | 只成层部分组件 | 🟡 **退路**（条件式 · 见 D4） | **不是真的更差，是被 ① 支配** —— 真实成本不是「重新探针」（两者都要重跑）而是**可能退化成零兄弟边** ⇒ ⇒ 失去对 D1 的判别力 |
| ⑥ | `ForceLayer` 零库改动 | `GranularityScope(ForceLayer)` 包住发射 | 🟡 **退路**（用户可接受「报告级示例」时） | 模块名回退 `typeid`（**ABI 相关**）+ **常态** `CHWARN`（ADR-059 §2.2.1 档④）⇒ ⇒ 演示逃生门而非声明式特性 ⇒ ⇒ 作**交付物**更差 |

⇒ ⇒ **⑦（未列）** `Rv32iPipeline` 加模板/策略参数在 `ch_module`/`ch_layer` 间编译期选择 ⇒ ⇒
**新增公开旋钮 + 两套实例化形态分别测** ⇒ ⇒ 收益零、复杂度高 ⇒ ⇒ **排除**。

---

## 4. 后果 / 代价

### 4.1 正面

- ✅ **诉求 ④ 兑现路径明确**（① ⇒ ⇒ 「真实设计 + 多 module 产物」）。
- ✅ **D1 与 D2 正交** ⇒ ⇒ D1 可先行 ⇒ ⇒ **不阻塞 B3**。
- ✅ **扁平产物零扰动**（D2 只写两个标志 ⇒ ⇒ 逐字节不变）。
- ✅ **不污染库外嵌入**（① 的决定权落在**装配点**，不在类上）。

### 4.2 负面（**代价**）

| # | 代价 | 缓解 |
|---|---|---|
| ① | ⚠️ **D1 的 H-A 是「放宽一个 fail-closed 判据」** ⇒ ⇒ 有**放过真双驱动**的风险 | ① `into_child == true` 保持 fail-closed（不变式）② **负向对照 × 3**（两个**合成**驱动争同一 pin ⇒ **仍** fail-closed）③ `rewired_pins` 结构化可观测 |
| ② | ⚠️ **D2 要改 `rv32i_pipeline.h`** ⇒ ⇒ 与 B3 **写点冲突** | ⇒ ⇒ **排在 B3 之后**（D2 时机已钉） |
| ③ | ⚠️ **7 个模块类型名成为产物级契约** ⇒ ⇒ 改名 = 产物级破坏 ⇒ ⇒ 未来成本 | ⇒ ⇒ 明文登记在 capability（可判） |
| ④ | ⚠️ **① 的边集合 ≠ ForceLayer 的 23 条** ⇒ ⇒ `dtcm`/`itcm` 仍 inline ⇒ ⇒ 第 21–23 条边会**重分类**到 `:844-917` 的 `analysis.edges` 通路 | ⇒ ⇒ **spike 门（ForceLayer 23→0）充分**，但**交付判据 MUST 在 ① 的真实声明态下重跑** ⇒ ⇒ 探针脚本现成（成本低） |
| ⑤ | ⚠️ **R21-A 的修法目前仍是「假说」** | ⇒ ⇒ **spike 先行**（H-A + H-B 后探针 `ok=true` 且 **23 → 0**，**不是**「减少到 N」）⇒ ⇒ 未归零 ⇒ ⇒ 残余边**逐条点名**并**重新裁决范围**（**不许**就地扩权） |

### 4.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| ① | 🔴 **D1 的 H-A 放宽后**，`rewired_pins` **为空** ⇒ ⇒ 判据**恒绿** | ⇒ ⇒ **负向对照**：构造一个真双驱动 ⇒ ⇒ MUST 转红（否则 D1 零判别力） |
| ② | ⚠️ **D2 的 7 个名字**与 ADR-059 的命名规则冲突（`sanitize_verilog_name` 后同 base 不同 type ⇒ `:1017-1028` 拒绝） | ⇒ ⇒ **撞名 MUST 可诊断**（不是静默改名） |
| ③ | ⚠️ **逐拍等价只覆盖顶层可见行为**（**观测盲区**：成层子树的 `ch_out` 被移出顶层 ⇒ 顶层读不到 ⇒ 「子树内部算错但顶层输出恰好相同」结构性测不出） | ⇒ ⇒ 与既有 `rtl-hierarchical-output` capability 的同一条边界一致 ⇒ ⇒ **不因本 ADR 加码**（加码需另立项） |

---

## 5. Verification（**前瞻**验收判据 —— 可勾选 · 未跑不勾）

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出）⇒ ⇒ **两后端 / 两份同源产物一致 ≠ 正确性证据**
> ⇒ ⇒ ⇒ 每条 MUST 配**变异自证**（故意做错 ⇒ ⇒ 必红）。

### 5.1 D1（发射器）

- [ ] **V1** 🔴 **spike 先行**：H-A + H-B 后探针 `ok=true` 且 **23 → 0**（**不是**「减少到 N」）
      ⇒ ⇒ 未归零 ⇒ ⇒ **残余边逐条点名** + **重新裁决范围**（**不许**就地扩权）
- [ ] **V2** 🔴 **负向对照 × 3（承重）**：(a) 两个**合成**驱动争同一 pin ⇒ ⇒ **仍** fail-closed
      (b) 同一子输出 fan-out 到 ≥ 3 个消费者 ⇒ ⇒ **全部连上**（幂等路径）
      (c) `into_child == true` 的双驱动 ⇒ ⇒ **仍** fail-closed
- [ ] **V3** **产物级验证**（探针之外）：层次化产物跑 lint ⇒ ⇒ **无 `%Error`**、退出码 0
      ⇒ ⇒ ⚠️ `ok=true` **不够**（那是发射器的自述 ⇒ ⇒ 不是产物事实）
- [ ] **V4** **确定性**：两次发射**逐字节**相同
- [ ] **V5** **单连接不变式**：每个 pin 在实例语句里**恰好出现一次**（正则计数）
- [ ] **V6** `rewired_pins` 为**结构化字段**（`HierarchyEmitResult`）⇒ ⇒ **不是** `CHWARN`
      ⇒ ⇒ 且**不破坏**既有断言 `warnings.empty()` 的测试
- [ ] **V7** ⚠️ **判别性自证**：把 H-A 改回**不分方向** ⇒ ⇒ **必红**（否则 D1 零判别力）

### 5.2 D2 / D3（成层）

- [ ] **V8** **交付粒度发射** `ok==true` 且 **`files.size() > 1`**（★ **不是**探针的 `ForceLayer`）
      ⇒ ⇒ 且 MUST 在 **① 的真实声明态**下重跑探针（边集合 ≠ ForceLayer 的 23）
- [ ] **V9** **7 个模块类型名登记**：capability `rtl-hierarchical-output` 的 MODIFIED（**产物级契约名**）
      + `docs/CHANGELOG.md` 一行 ⇒ ⇒ **不与 ADR-046 D10 冲突**（D10 冻结的是**端口集**）
- [ ] **V10** **扁平产物不变**（**结构性恒真**的跳线 ⇒ ⇒ **不是**判据 ⇒ ⇒ 真实跳线 = 层次化产物与扁平产物**逐拍等价** ⇒ ⇒ 归 change 的 AC）

---

## 6. 落点与后续

| # | 事项 | 归属 | 载体 |
|---|---|---|---|
| ① | **D1 的 spike**（H-A + H-B）⇒ ⇒ **先行**（与 B3 **零交集**） | builder | change `rv32i-hierarchical-example` 的 T1 |
| ② | **D1 的实施**（含 `rewired_pins` + 3 条负向对照） | builder | 同上 T2 |
| ③ | **D2 的实施**（7 处 `ch_layer`）⇒ ⇒ **排在 B3 之后** | builder | 同上 T3 |
| ④ | **D3 的登记**（capability MODIFIED + CHANGELOG） | planner（capability delta）+ builder（CHANGELOG） | 同上 T4 |
| ⑤ | **① 的边集合重跑探针** ⇒ ⇒ 新清单写进 change 的 `design.md` | builder | 同上 T3 |
| ⑥ | ⚠️ **诉求 ④ 的交付形态认定**（若 spike 撞出第二层失败 ⇒ ⇒ 退路 ⑥ 还是 ⑤） | **用户 / architect** | **不自决** ⇒ ⇒ 属诉求解释 |
| ⑦ | **7 个名字的契约强度**（产物级契约名 vs 明文非契约） | **用户** | 本 ADR 选前者 ⇒ ⇒ 若用户倾向后者 ⇒ ⇒ capability MUST 写死「非契约」字样 |

> ⚠️ **本 ADR 的 Status 在 oracle + critic 全闭前 MUST 保持 `Draft`** ⇒ ⇒ 「把未决问题写成已决」属
> `arch` 档 BLOCKER 第四类 ⇒ ⇒ **未审先 Accepted = 违规**。
> ⚠️ **oracle 第 1 轮（2026-11-13）**：确认「① 选中」+「⑤ 条件式可接受的三条件」+
> 「D1 与 D2 正交」⇒ ⇒ 并指出 **D1 的 23 条清单 ≠ ① 的边集合**（第 21–23 条会重分类）⇒ ⇒ 本 ADR §4.2 ④ 已登记。
