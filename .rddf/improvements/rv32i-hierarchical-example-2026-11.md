---
name: rv32i-hierarchical-example-2026-11
priority: P2
phase: phase-6
category: framework
type: feature
状态: draft
进度: 2026-11-13 立项（用户口令「R21立项」）· **未开工** ⇒ ⇒ 规模侦察 + IR 级根因已定死 ⇒ ⇒ **已过 metis 1 轮（10 条处置全采纳，含 4 条事实订正）** ⇒ ⇒ 🚪 开工前门 = **R21-B 的成层方式需 architect 裁决** + **R21-A 需先做 spike**
主题: 层次化 RTL 输出
依赖: ADR-060（§2.1 行 1/行 2）· ADR-061（粒度四档）· ADR-066（决策 ①/③/⑤）· ADR-046 D10（SoC 端口冻结）· 逐拍等价 helper `include/hierarchy_equivalence.h`
来源: 用户 2026-11-13「R21立项」+ `.rddf/roadmap/phase-6-next-steps.md` 的 R21 行（诉求 ④ 未兑现）+ planner 2026-11-13 IR 级侦察
生成时间: 2026-11-13
evidence: probe:scripts/probe_r21_hier_granularity.sh
---

# Improvement: RV32I 的**层次化 RTL 示例**（诉求 ④）+ 发射器的「双类消费者」缺陷

- **Date drafted**: 2026-11-13
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（立项已登记 · **未开工**）
- **🚪 开工前门（两道）**: ① **R21-A 需先做 spike**（§A.3 的修法是**假说**，不是已证结论）② **R21-B 的成层方式需 architect 裁决**（§R21-B 的 6 选项）
- **用户裁决**: 2026-11-13「R21立项」
- **上游**: `.rddf/roadmap/phase-6-next-steps.md` 的 **R21** 行 · `docs/architecture/2026-11-02-hierarchical-rtl-output.md`（§7 残差台账）
- **可复算的侦察手段**: `bash scripts/probe_r21_hier_granularity.sh`（只读 · 产物写 `$TMPDIR/opencode/` · 零仓库改动）

---

**主题**: 层次化 RTL 输出

## 架构依据

### A.1 实测事实（2026-11-13 · 含 metis 第 1 轮的 4 条订正）

| # | 事实 | 证据 |
|---|---|---|
| ① | ⚠️ **RV32I 成层发射 = fail-closed · 23 条**跨 module 边无法映射（`Granularity::ForceLayer`） | `bash scripts/probe_r21_hier_granularity.sh` ⇒ `ok=false` · `23 cross-module edge(s)` |
| ② | ⚠️ **根因不是「隐式端口合成未实现」** —— 合成链已存在且正常工作（诊断自己写「the synthesized chain would drive it from …」） | 探针诊断文案 · `src/hierarchy_verilog.cpp:608-702`（兄弟边：共同祖先内的中转线） |
| ③ | **抛点精确定位（metis 订正 · 原稿写错）**：23 条**全部**落 `src/hierarchy_verilog.cpp:693-702` 的**兄弟边分支**（`add_conn(lca, …, names.at(e.from_ptr), wire, /*into_child=*/false, err)`）⇒ ⇒ 冲突来自 `add_conn` 的 `:350-359`，与 `:443`（pass-through 链）/ `:766`·`:779`（行 2）**无关** | `src/hierarchy_verilog.cpp:686-702` · `:342-359` |
| ④ | 🔴 **IR 级定死**（3 次探针迭代）：`top.pipeline.if_stage` 的 2 个输入端口 `src[0]` = `top.pipeline.hazard_unit.unnamed_output` ⇒ ⇒ **兄弟边真实存在**；**同一个** `hazard_unit.unnamed_output` 又是 `top.pipeline.add_add` 的 `src[1]` ⇒ ⇒ **父侧表达式消费**也真实存在 | 探针（`/tmp/opencode/r21_ir_probe3.cpp`）⇒ `if_stage` 输入端口 src + `top.pipeline` 内 src 含 `hazard_unit.*` 的节点 = `unnamed_output` / `mux_select`×2 / `and_and` / `add_add` |
| ⑤ | 🔴 **根因 = 「同一个子输出有**两类**消费者」** —— ① 兄弟子模块的输入端口 ② 父体内的运算表达式 ⇒ ⇒ 发射器把「分析层记录的**消费点**名字」当成「该 pin 的**实参**」⇒ 判为双驱动 | ④ + ⑥ + ⑦ |
| ⑥ | ⚠️ **判据其实「不对称」**：直接 子→父 路径**已**用 `replace_existing=true`（`:910-911`），而兄弟边分支**未**用 | `src/hierarchy_verilog.cpp:443` · `:695` · `:910-911`（唯一 `replace_existing=true`） |
| ⑦ | ⚠️ **代码自己写明这是「不是一根网」**：`:347-349` 注释 —— 「族 ①/③：分析层为「子端口 → 父体内节点」也记了一行 ⇒ **那不是一根网** ⇒ MUST 丢弃」⇒ ⇒ 正是 23 条边的形态 | 同上 |
| ⑧ | **已有覆盖 vs 未覆盖（metis 订正）**：`ImmConsistencyDut` 已覆盖「子输出**直通父 `output` 端口** + 参与父侧运算」（含**单连接不变式**断言）⇒ ⇒ **未被覆盖的轴 = 「兄弟子输入 + 父侧表达式消费」** | `tests/test_rv32i_stage_dual.cpp:423-455`（`.port(net)` 只出现一次） |
| ⑨ | 🔴 **第二处机制（metis P0）**：`parent_landing_wire`（键 = `module\x1finstance\x1fchild_port`）决定 `:1490-1514` 是否**丢弃**分析层旧行；被丢弃行连的是父 `output` 端口时还要走 **ADR-066 决策⑤** 的显式 `assign`。兄弟边分支目前**只**登记 `synth_wires`，**未**填 `parent_landing_wire` ⇒ ⇒ 修好 `add_conn` 后，21–23 条（父侧消费点 = `top.obs_*`）会出现 `Duplicate pin connection` | `src/hierarchy_verilog.cpp:689-692`（只 push `synth_wires`）· `:772-786`（行 2 才写 `parent_landing_wire`）· `:1490-1514` |
| ⑩ | **RV32I 顶层 input 的真相（metis 订正）**：`ext_clk` 是**已登记的死端口**（被赋值但无消费者；所有寄存器锁框架 `default_clock`）⇒ ⇒ **MUST NOT** 放进 `DrivenInput`（静默无效）⇒ ⇒ 真正可驱动的只有 `ext_rst` / `prog_we` / `prog_addr` / `prog_data` | `examples/riscv-mini/src/rv32i_pipeline_top.h:36-48` + `.rddf/checks/decision-request-rv32i-ext-clock-dead-port-2026-11.md` |
| ⑪ | **实例级成层 API 已存在**：`ch::ch_layer<T>`（`T` 未类级声明 ⇒ **MUST 在实例处给模块类型名**）⇒ ⇒ **不需要**改类级声明 | `include/module.h:160-176` · `include/component.h:226-243` |
| ⑫ | ⚠️ **RV32I 的 stage 实例只在库头内创建**（`RiscvPipeline::describe()`）⇒ ⇒ 在 example 侧把 stage 声明成层**结构上不可能** | `include/cpu/pipeline/rv32i_pipeline.h:119-128`（7 个 `ch::ch_module<T>`）|
| ⑬ | ADR-046 **D10 冻结 SoC 顶层端口契约**（32 端口）⇒ ⇒ 本项 **不改**任何端口 | ADR-046 D10 |
| ⑭ | **R1 lint 口径**（metis 订正）：`verify_hierarchical_rtl.sh:105` 用 `--lint-only --top-module top <file>`，**warning 默认 fatal**，`lint_off` 白名单当前只含 `WIDTHEXPAND\|WIDTHTRUNC`；**禁** `-Wno-fatal`，但**未**禁 `-Wno-<单项>`（⇒ 这是旁路 ⇒ 本项 MUST 显式封住） | `scripts/verify_hierarchical_rtl.sh:105,149` · ADR-060 §2.5 |

### A.2 23 条边（**点名，不许只报计数**）

**全部为兄弟边**（源 / 目标都是同一父 module 的直接子）⇒ ⇒ 全部落 ③ 的那一个分支。

| # | 源（`@` 所属模块） | 目标（`@` 所属模块） | 父侧已有消费点 |
|---|---|---|---|
| 1 | `hazard_unit.unnamed_output_1` @ `pipeline.hazard_unit` | `if_stage.unnamed_input` @ `pipeline.if_stage` | `add_add` @ `pipeline` |
| 2 | `hazard_unit.unnamed_output_2` @ `pipeline.hazard_unit` | `if_stage.unnamed_input_1` @ `pipeline.if_stage` | `and_and` @ `pipeline` |
| 3 | `ex_stage.unnamed_output` @ `pipeline.ex_stage` | `if_stage.unnamed_input_4` @ `pipeline.if_stage` | `unnamed_output` @ `pipeline` |
| 4 | `if_stage.unnamed_output_3` @ `pipeline.if_stage` | `id_stage.unnamed_input_2` @ `pipeline.id_stage` | `and_and` @ `pipeline` |
| 5 | `hazard_unit.unnamed_output_2` @ `pipeline.hazard_unit` | `id_stage.unnamed_input_3` @ `pipeline.id_stage` | `and_and` @ `pipeline` |
| 6 | `hazard_unit.unnamed_output_1` @ `pipeline.hazard_unit` | `id_stage.unnamed_input_4` @ `pipeline.id_stage` | `add_add` @ `pipeline` |
| 7 | `id_stage.unnamed_output_7` @ `pipeline.id_stage` | `ex_stage.unnamed_input_5` @ `pipeline.ex_stage` | `mux_select` @ `pipeline` |
| 8 | `id_stage.unnamed_output_6` @ `pipeline.id_stage` | `ex_stage.unnamed_input_4` @ `pipeline.ex_stage` | `eq_eq` @ `pipeline` |
| 9 | `id_stage.unnamed_output_4` @ `pipeline.id_stage` | `ex_stage.unnamed_input_8` @ `pipeline.ex_stage` | `eq_eq` @ `pipeline` |
| 10 | `id_stage.unnamed_output_17` @ `pipeline.id_stage` | `ex_stage.unnamed_input_9` @ `pipeline.ex_stage` | `bits` @ `pipeline` |
| 11 | `id_stage.unnamed_output` @ `pipeline.id_stage` | `ex_stage.unnamed_input_1` @ `pipeline.ex_stage` | `unnamed_output` @ `pipeline` |
| 12 | `id_stage.unnamed_output_3` @ `pipeline.id_stage` | `ex_stage.unnamed_input` @ `pipeline.ex_stage` | `mux_select` @ `pipeline` |
| 13 | `id_stage.unnamed_output_4` @ `pipeline.id_stage` | `ex_stage.unnamed_input_2` @ `pipeline.ex_stage` | `eq_eq` @ `pipeline` |
| 14 | `id_stage.unnamed_output_2` @ `pipeline.id_stage` | `ex_stage.unnamed_input_11` @ `pipeline.ex_stage` | `unnamed_output` @ `pipeline` |
| 15 | `id_stage.unnamed_output_10` @ `pipeline.id_stage` | `hazard_unit.unnamed_input_2` @ `pipeline.hazard_unit` | `reg_file.rs1`（**proxy**）@ `pipeline` |
| 16 | `id_stage.unnamed_output_11` @ `pipeline.id_stage` | `hazard_unit.unnamed_input_3` @ `pipeline.hazard_unit` | `reg_file.rs2`（**proxy**）@ `pipeline` |
| 17 | `id_stage.unnamed_output_9` @ `pipeline.id_stage` | `hazard_unit.unnamed_input_1` @ `pipeline.hazard_unit` | `mux_select` @ `pipeline` |
| 18 | `id_stage.unnamed_output_1` @ `pipeline.id_stage` | `hazard_unit.unnamed_input` @ `pipeline.hazard_unit` | `unnamed_output` @ `pipeline` |
| 19 | `ex_stage.unnamed_output` @ `pipeline.ex_stage` | `hazard_unit.unnamed_input_7` @ `pipeline.hazard_unit` | `unnamed_output` @ `pipeline` |
| 20 | `wb_stage.unnamed_output` @ `pipeline.wb_stage` | `forwarding_mux.unnamed_input_5` @ `pipeline.forwarding_mux` | `unnamed_output` @ `pipeline` |
| 21 | `dtcm.unnamed_output` @ `dtcm` | `pipeline.unnamed_input_3` @ `pipeline` | `obs_data_rdata`（父 **output 端口**）@ `top` |
| 22 | `pipeline.unnamed_output_3` @ `pipeline` | `dtcm.unnamed_input_1` @ `dtcm` | `obs_data_write_en`（父 **output 端口**）@ `top` |
| 23 | `pipeline.unnamed_output_2` @ `pipeline` | `dtcm.unnamed_input_2` @ `dtcm` | `obs_data_write_data`（父 **output 端口**）@ `top` |

**按边的源模块计数（metis 订正 · 和 = 23）**：
`id_stage` **12** · `hazard_unit` **4** · `ex_stage` **2** · `pipeline`（父层）**2** · `if_stage` **1** ·
`dtcm` **1** · `wb_stage` **1** · `forwarding_mux` **0**（它只作目标）。
> ⚠️ **订正记录**：原稿的 `id_stage 15 · ex 10 · hazard 9 …`（和 = 46）是**端点出现次数**，被错标成「按边的源模块」。

### A.3 修法 = **假说**，不是已证结论（metis P0 的降级）

**假说 H-A**：把 `add_conn` 的冲突判据改成「只看 `into_child == true` 的既有行」——
即跳过 `c.into_child == false` 的行（那是「子 **output** → 父侧**消费点**」的**记录，不是网**，⑦ 自己写明 MUST 丢弃），
只对 `c.into_child == true`（父驱动子的 **input**）做「不同 `parent_side` ⇒ 拒绝」。

**H-A 只解决冲突判据；23 条要真的发射出来还必须补 H-B**：
**H-B**：兄弟边分支（`:689-692`）在登记 `synth_wires` 之后，**同时**填 `parent_landing_wire[lca\x1finst\x1fchild_port]`（⑨）——
否则 `:1490-1514` 不会丢弃分析层旧行 ⇒ 21–23 条（父侧消费点是 `top.obs_*` **output 端口**）会 `Duplicate pin connection`。

⇒ ⇒ ⚠️ **两者 MUST 由一个 spike 验证**（判据 = 23 → 0 **且** 产物 verilator lint 无 `%Error`），
**MUST NOT** 把「修 1 处 ⇒ 23 归零」写成既定前提（原稿就是这么写的 ⇒ metis P0-1 否决）。

---

## 范围

### ✅ 在内

| 编号 | 内容 | 归属 |
|---|---|---|
| **R21-A** | **spike → 实施**：按 §A.3 的 H-A + H-B 修 `src/hierarchy_verilog.cpp`（**唯一文件**）。<br>⚠️ 判据收**窄**：只跳过 `c.into_child == false` 的既有行；`into_child == true` 的冲突**保持** fail-closed（真双驱动必须仍响亮拒绝）。<br>⚠️ **MUST NOT** 在 `:443` / `:766` / `:779` 做「顺手一致性改动」（那些不是 23 条的抛点 ⇒ 纯扩回归面）。 | builder |
| **R21-B** | **成层声明方式**（🚪 **architect 1 轮裁决**，6 选项见下）：让 RV32I 真成为多 module 产物。<br>⚠️ 约束：不改 ADR-046 D10 端口契约；不得让**扁平**产物变化。 | architect → builder |
| **R21-C** | **验收面**：产物级判据（交付粒度发射 / lint / 确定性 / 单连接不变式 / 判别性自证 / 逐拍等价 / 手推期望值）。 | builder |

### ❌ 不在本项

- **SoC 端口契约变更**（ADR-046 D10）⇒ 零端口改动。
- **RV32I 流水线语义改动**（数据通路 / 停顿 / 前推）⇒ 归 **B3 / B4**。
- **`hier_rtl_products` 语料接入（R2–R8 全套）** ⇒ ⇒ R8 需手推 32 端口表，且 R1 lint 在 RV32I 上有 `DECLFILENAME` / `UNUSEDSIGNAL` 风险（⑭）⇒ ⇒ 本轮只做「**产物级 lint + 单连接不变式**」两项（等价于 R1/R4/R2 的**最小子集**），全套接入 = **后续独立项** ⇒ ⇒ 不做则登记残差 `id=25`。
- **`ext_clk` 死端口转活** ⇒ 已登记为独立裁决（⑩）⇒ 本项只**绕开**它。
- **跨 LCA 的 fan-out 支持** ⇒ 同一子输出扇出到**不同共同祖先**的两个兄弟时，`:361-371` 报 `synthesized twice with different drivers` ⇒ ⇒ 本项**不支持** ⇒ MUST 用**判别性用例**钉住「仍 fail-closed 且消息点名」。
- **改 ADR-060 §2.1 的行 1/行 2 政策** ⇒ 若 spike 显示必须改政策（例：允许子输出驱动父 **input** 端口）⇒ **停下**按 `design_drift` 交 architect。

---

## Why

### W.1 诉求 ④ 是「真实设计能被层次化发射」

层次化输出的**全部**既有语料仍是构造出来的小 DUT ⇒ ⇒ 它们不会同时出现「一个子输出喂兄弟、又被父侧表达式消费」这种真实微架构形态 ⇒ ⇒ 23 条边上的缺陷**不可能**被它们发现（⑧ 已给出「已覆盖轴 vs 未覆盖轴」的精确划分）。

### W.2 这条缺陷的形态是「静默的语义陷阱」

诊断说的「already driven by」在**语义上是错的**：分析层那一行的 `parent_side` 记的是**消费点名字**（`:737-739`），而 pin 的实参应当是**承载该值的网名**（B1 设计 ⇒ 父侧声明同名网 ⇒ 父侧表达式自动解析）。两者比较 ⇒ ⇒ **必然不等** ⇒ 必然 fail-closed。

⚠️ 危险的是**反向修法极具诱惑力**：删掉冲突检查就能让 23 条归零 —— 那会让**真双驱动**（两个子输出争同一 pin）静默通过 ⇒ ⇒ 本项因此把「**负向对照 × 3**」写进验收。

### W.3 为什么不能靠「改 23 处源码单驱动化」

- 与 **B3 / B4** 的写点**直接冲突**（R21 的前提是写点不重叠）；
- 会**掩盖**发射器判据不一致（⑥⑦）⇒ ⇒ 下一个设计以同样形态再撞一次。

⚠️ **但这是 planner 的**建议**，不是政策裁决**：「双类消费者是否合法形态」属架构判断 ⇒ ⇒ **R21-B 的裁决顺带回答它**（若 architect 裁定「非法」⇒ 本项只能退回源码侧单驱动化，且与 B3 顺序化）。

---

## What Changes

### R21-A — 发射器：pin 冲突判据（H-A）+ 兄弟边落点网登记（H-B）

| 位置 | 现状 | 改后 |
|---|---|---|
| `src/hierarchy_verilog.cpp:350-359`（`add_conn` 的冲突循环） | 同一 pin 的既有行只要 `parent_side != arg` 就拒绝（**不分方向**） | **跳过** `c.into_child == false` 的行（消费记录，不是网）；只对 `c.into_child == true` 判冲突 |
| `src/hierarchy_verilog.cpp:689-697`（兄弟边：登记中转线 + 两条 `add_conn`） | 只 push `synth_wires`，**不**填 `parent_landing_wire` | 追加 `parent_landing_wire[lca\x1finst\x1fchild_port] = wire`（驱动侧 pin）⇒ ⇒ `:1490-1514` 才会丢旧行 + 走决策⑤ `assign` |
| `:443` / `:766` / `:779` | 同上族但**非 23 条的抛点** | **不动**（拒绝顺手改动） |
| `:910-911` | 已 `replace_existing=true` | **不动**；注释升级为「本判据对**全部**合成 site 一致（方向感知）」 |

⚠️ **不变式（MUST 写进代码注释 + capability 的反向 Requirement）**：`into_child == true` 的双驱动**永远** fail-closed —— IR 的 `set_src` 是 last-wins ⇒ 语义只会丢掉一个驱动。

⚠️ **可观测性用结构化字段，不用 CHWARN**（metis P1-8）：`HierarchyEmitResult` 追加 `std::vector<std::string> rewired_pins`（或计数）—— 该路径对**每个**「子 output 被父体消费」的 pin 都会发生（`:910-911` 今天已在做）⇒ 发 CHWARN 会让**所有**层次化发射常态告警，并牵连断言 `warnings.empty()` 的既有测试。

### R21-B — 成层声明（**6 个选项，需 architect 裁决**）

| 选项 | 做法 | 影响面 | 代价 / 风险 |
|---|---|---|---|
| ① | `include/cpu/pipeline/rv32i_pipeline.h:119-128` 的 7 处 `ch::ch_module<T>` ⇒ `ch::ch_layer<T>`（给模块类型名，⑪） | 只影响**层次化**产物；扁平不变 | 7 个 module 类型名成为**产物里的公开名字**（后续改名 = 产物级破坏性变更）⇒ 需登记或明文声明「不是契约」 |
| ② | `examples/` 侧另建装配（复刻 7 个 stage 连线） | 零库改动 | 🔴 复刻出的是**另一个流水线**（⑫）⇒ 不满足「真实设计」且必然漂移 |
| ③ | 只在 `tests/` 用测试本地子类 | 零库改动 | 🔴 不交付「RV32I 示例」（诉求 ④ 未兑现） |
| ④ | 给 7 个 stage 头各加 `CH_HIERARCHY_LAYER(...)`（**类级**声明） | `rv32i_pipeline.h` **一行不改**（与 B3 零文本重叠） | 🔴 影响面最大：**任何**嵌入这些组件的设计在 `RespectDeclarations` 下都会成层 |
| ⑤ | **部分成层**（如只 `dtcm` / 只 stage 层而 `pipeline` 仍 inline） | 最小 | 拓扑与边集合**完全不同** ⇒ 必须重新探针（`nearest_layer` 会变） |
| ⑥ | **零库改动**：示例/测试里用 `GranularityScope(Granularity::ForceLayer)` 包住发射（探针已在这么做） | 零库改动 · 零公开面 | 🔴 交付的是「**报告级**示例」而非「设计自身声明成层」⇒ 需 architect 认定诉求 ④ 是否允许 |

⇒ ⇒ **architect 裁决**必须回答两件事：**(a) 选哪个选项**；**(b) 「部分成层」是否可接受**（若可 ⇒ 需重新跑探针定边集合）。
⇒ ⇒ 若裁 ②/③/⑥ 被拒且 ①④ 均不可接受 ⇒ 本项**停在 R21-A**（缺陷修好、诉求 ④ 不兑现）⇒ 需重议范围。

### R21-C — 验收面（逐条可机械判定）

```bash
# ① 交付粒度发射（★ 不是探针的 ForceLayer）
#    探针硬编码 ForceLayer（scripts/probe_r21_hier_granularity.sh:35）⇒ 它只是侦察
#    交付判据 MUST 用「裁决选定的粒度」发射 ⇒ ok==true 且 files.size()>1

# ② lint（R1 口径 · 显式写出全部参数）
verilator --lint-only --top-module top <hier 产物，文件名 MUST == module 名>
#    · 禁 -Wall（ADR-060 §2.5）· 禁 -Wno-fatal · ★ 禁任何 -Wno-<单项>（那是旁路）
#    · 允许的 warning MUST 逐条列出 + 给理由（预期：DECLFILENAME / UNUSEDSIGNAL / UNDRIVEN(ext_clk)）

# ③ 确定性（= R4 的最小版）：同一设计发射两次 ⇒ 两次文本逐字节相同

# ④ 单连接不变式：每个 pin 在实例语句里恰好出现一次（正则计数）
```

---

## Acceptance

### R21-A（发射器）

- [ ] **spike 先行**：H-A + H-B 后探针 `ok=true` 且 **23 → 0**（不是"减少到 N"）；若未归零 ⇒ 把**残余边逐条点名**并重新裁决范围（**不许**就地扩权）⇒ 归属: **builder**
- [ ] **产物级验证**（探针之外）：对 RV32I 层次化产物跑 ② 的 lint ⇒ **无 `%Error`**、退出码 0（19–21 条的 `Duplicate pin connection` 若存在，这里必红）⇒ 归属: **builder**
- [ ] ⚠️ **负向对照 × 3（承重）**：<br>(a) 两个**合成**驱动争同一 pin ⇒ **仍** fail-closed；<br>(b) 同一子输出 fan-out 到**不同 LCA** 的两个兄弟 ⇒ **仍** fail-closed 且消息点名（`:361-371`）；<br>(c) 子 output pin 的既有行连的是父 **output 端口** ⇒ 走**决策⑤ `assign`**、**不丢连接**（21–23 条的形态）⇒ 归属: **builder**
- [ ] **多消费者 ≥ 3** 的 pin 有覆盖（幂等路径）⇒ 归属: **builder**
- [ ] **proxy 端点**（#15/#16 的 `reg_file.rs1`/`rs2` 作为父侧消费点）实测成立（`print_proxy` 的 RHS 能否读同名网）⇒ 归属: **builder**
- [ ] 既有语料零回归：`test_hierarchy_equivalence` / `test_hierarchy_verilog` / `test_hierarchy_analysis` / `test_rv32i_stage_dual`（含 `ImmConsistencyDut` 的单连接不变式）/ `verify_hierarchical_rtl.sh --self-test` 全绿 ⇒ 归属: **builder**
- [ ] 「改接」是**结构化字段**（`HierarchyEmitResult`），**不是** CHWARN；且不破坏断言 `warnings.empty()` 的既有测试 ⇒ 归属: **builder**

### R21-B（成层）

- [ ] architect 裁决落盘（选项 + 「部分成层是否可接受」两问都答）⇒ 归属: **architect**
- [ ] 若选 ⑤ ⇒ **重新跑探针**（边集合与 ForceLayer 不同）并把新清单写进 change 的 design.md ⇒ 归属: **builder**
- [ ] **扁平产物不变**：⚠️ 它是**结构性恒真**的回归跳线（`ch_layer` 只写 `is_layer` / `module_type_name` 两个标志，扁平路径不读）⇒ ⇒ **不作为承重证据**，但 MUST 保留（证明改动确实没碰扁平路径）⇒ 归属: **builder**
- [ ] 模块类型名**撞名**行为可诊断（`sanitize_verilog_name` 后同 base 不同类型 ⇒ `:1017-1028` 拒绝）⇒ 归属: **builder**
- [ ] 若选 ①/④ ⇒ 模块类型名登记（`.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md`）**或**在 capability 明文「这些名字不是契约」⇒ 归属: **builder**

### R21-C（等价性与产物）

- [ ] **交付粒度**发射 `ok==true` 且 `files.size()>1` ⇒ 归属: **builder**
- [ ] lint 按 ② 的**显式参数**通过；允许的 warning 逐条列出 + 理由；**无** `-Wno-<单项>` ⇒ 归属: **builder**
- [ ] **确定性**：两次发射逐字节相同 ⇒ 归属: **builder**
- [ ] **单连接不变式**：每个 pin 恰好一次 ⇒ 归属: **builder**
- [ ] **逐拍等价**（扁平 ≡ 层次化，同一段固件）：<br>· `DrivenInput` = `ext_rst` / `prog_we` / `prog_addr` / `prog_data`（⚠️ **不含 `ext_clk`**，⑩）<br>· `Observation` ⊇ { `dbg_id_pc` · `dbg_id_valid` · `dbg_wb_write_en` · `dbg_wb_rd_addr` · `dbg_wb_write_data` · `dbg_branch_taken` · `obs_data_addr` · `obs_data_write_en` · `obs_perf_instructions` }<br>· **MUST** 断言一个「**只有程序真被装载**才成立」的值（否则恒 NOP 的流水线也能过）⇒ 归属: **builder**
- [ ] **判别性自证**：篡改层次化产物的**一根线** ⇒ 等价性**转红**（否则全绿可能来自观测面恒定量）⇒ 归属: **builder**
- [ ] **期望值手推**（禁抄仿真输出）：手推对象 = `Result::flat_trace`（`include/hierarchy_equivalence.h:118-120` 只有扁平侧轨迹）⇒ 至少「`ext_rst` 释放后第 N 拍的 `dbg_id_pc` 序列」+「一条 `sw` 的 `obs_data_addr`/`obs_data_write_en`」+「3 条 `addi` 后的 `dbg_wb_write_data`」<br>⚠️ 手推 MUST 区分「框架 `default_reset` 脉冲」与「DUT 的 `ext_rst`」，且把 `primed`/`fetch_pc_reg` 的跳拍算进去 ⇒ 归属: **builder**
- [ ] 计数锚点同步：`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL`（新用例若带 `verilator_e2e` 标签 ⇒ **+1**；并写明 **TIMEOUT**，RV32I 层次化编译是分钟级）⇒ **MUST 引用锚点，禁写死数字** ⇒ 归属: **builder**

### 不回归 / 文档同步

- [ ] 全量 `ctest` 绿（数字以 `CTEST_TOTAL` 锚点为准）⇒ 归属: **builder**
- [ ] `test_rv32i_soc_dual_parity` 的 golden 全绿（本项零流水线语义改动）⇒ 归属: **builder**
- [ ] `verify_hierarchical_rtl.sh`（R1–R8）+ `--self-test` 全绿 ⇒ 归属: **builder**
- [ ] **`ADR-060` / `ADR-066` 的判据正文**更新（不只是主题文档加一行）：行 2 的适用面 + **反向不变式**（`into_child==true` 永远拒绝）+ 决策⑤ 的适用面 ⇒ 归属: **architect（判据面）/builder（实施后回填）**
- [ ] **roadmap R21 行改写**（旧文本写「立项范围 = 23 处双驱动点的单驱动化」，与本文冲突）⇒ MUST 同步，否则下个会话按旧范围行动 ⇒ 归属: **planner**
- [ ] `include/cpu/AGENTS.md` 的 `rv32i_pipeline.h` 行数与目录总行数（若 ①/④ 改了行数）⇒ 归属: **builder**
- [ ] 残差登记（若「语料全套 R2–R8」不做）：`.rddf/checks/arch-residual-registry.md` 的 `id=25`（`已知`）+ 台账 §7 同批 ⇒ 走 `verify_arch_ledger_residuals.sh` 的**生成句前缀**格式 ⇒ 归属: **architect**

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| **`rtl-hierarchical-output`** | MODIFIED | 新增「**双类消费者**」Requirement：同一个子模块输出既驱动兄弟子模块的输入端口、又参与父体内运算表达式 ⇒ MUST 可发射（pin 实参 = 承载该值的网名；父侧表达式经同名网解析；父 **output** 端口走决策⑤ `assign`）⇒ 并追加**反向不变式**：`into_child == true`（父驱动子 **input**）的双驱动 MUST **永远** fail-closed |
| `hierarchy-friendliness-diagnosis` | **不动** | 诊断口径与「需合成端口数」判定不变 |
| `rtl-emit-portability` | **不动** | 扁平产物不变由它守（**结构性恒真** ⇒ 只作跳线） |
| `rv32i-data-forwarding` / `rv32i-memory-timing` | **不动** | 零流水线语义改动 |

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `src/hierarchy_verilog.cpp` | **R21-A 的唯一写点**（`add_conn` 判据 + 兄弟边落点网登记 + 结构化字段）⇒ 需 critic（**触及产物形态**） |
| `include/hierarchy_verilog.h` | `HierarchyEmitResult` 加一个字段（公开面**追加**，非破坏） |
| `include/cpu/pipeline/rv32i_pipeline.h` | **仅当选 ①**（7 处 `ch_module` → `ch_layer`）；选 ④ 则改 7 个 stage 头 |
| `tests/` | 新增 RV32I 层级发射用例 + 3 条负向对照 ⇒ `CTEST_TOTAL` +N（以锚点为准） |
| `.rddf/checks/arch-residual-registry.md` | 若「语料全套」不做 ⇒ `id=25`（`已知`）⇒ 台账 §7 同批（双向 1:1） |
| 文档 | ADR-060 / ADR-066 判据正文 + 主题文档追加行 + `include/cpu/AGENTS.md` 行数 |

### 已知风险

| 风险 | 缓解 |
|---|---|
| 🔴 **放宽过头** ⇒ 真双驱动被静默吞掉 | 只跳过 `into_child == false`；**负向对照 × 3** 是必做项 |
| 🔴 **H-B 漏做** ⇒ 21–23 条 `Duplicate pin connection` | spike 必须带 **lint 级**验证（探针 `ok=true` 不够） |
| 🔴 **「修 1 处即归零」是假说** ⇒ 可能有第二层失败 | spike 门 + 残余逐条点名（不许就地扩权） |
| ⚠️ **逐拍等价同源失明** | 手推 `flat_trace` + 判别性自证（篡改一根线必红）+ 「装载生效」值 |
| ⚠️ **执行顺序**：B3 把 load-use 停顿 3→4 拍；R17.2 会改 32 端口名 | R21-A 可**立即**做（零交集）；R21-B/C **排在 B3 与 R17.2 之后**；若必须并行 ⇒ 期望值用**内容锚点 + 参数化基线**，禁写死停拍数/端口名 |
| ⚠️ 与 B3 的 `rv32i_pipeline.h` 交集 | 用行号判交集**不可机械判定**（先落地方整体平移）⇒ 改为**顺序执行**（见上） |
| ⚠️ verilator 编译成本 | RV32I 层次化是分钟级 ⇒ 用例 MUST 标 `verilator_e2e` + 写明 TIMEOUT |

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `memory-timing-contract-2026-11`（B3 / R14） | `rv32i_pipeline.h` 同文件（`:427-437` / `:584-585` / `:226-234` + `T2-ⓐ` 在 `:158` 附近新增 8 个寄存器）⇒ ⇒ **R21-B/C 排在 B3 之后**；R21-A 无交集，可先做 |
| **R17.2**（`rv32i_pipeline_top.h` 的 32 端口显式命名） | 与 R21-C 的端口表 / 基线**同区** ⇒ MUST **先于** R21-C |
| `rv32i-dead-component-consolidation-2026-11` | 已闭（B0 批）⇒ 无影响 |
| `rtl-emit-product-tooling-2026-11` | 无 `manifest` 影响（本项不改端口契约） |
| `ext_clk` 死端口裁决（`.rddf/checks/decision-request-rv32i-ext-clock-dead-port-2026-11.md`） | R21-C 的 `DrivenInput` 必须绕开它（⑩） |

---

## metis 第 1 轮 gap 分析与处置（2026-11-13）

> **轮次**：1（该 improvement 总计 1 轮）· **审查者**：metis · **处置：10 条全采纳**（含 4 条事实订正）

### ✅ 已订正（事实错误）

| # | metis finding | 处置 |
|---|---|---|
| 1 | 🔴 **抛点写错**：23 条**全部**是兄弟边 ⇒ 落 `:693-702`，**不是** `:443` / `:766` / `:779` | §A.1③ 重写；R21-A 表删掉那三处「顺手改动」，改为「**不动**」 |
| 2 | 🔴 **分布数字自相矛盾**：`id 15 · ex 10 · hazard 9 …` 和 = 46 = 2×23（那是端点出现次数） | §A.2 尾部改为按**源模块**计数（和 = 23）+ 追加订正记录 |
| 3 | 🔴 **`ext_clk` 是已登记死端口** ⇒ 放进 `DrivenInput` 静默无效 | §A.1⑩ 补记；Acceptance 的 `DrivenInput` 显式**排除**它 |
| 4 | ⚠️ **「双类消费者零覆盖」夸大**：`ImmConsistencyDut` 已覆盖「父 output 端口 + 父侧表达式」 | §A.1⑧ 改为「已覆盖轴 vs 未覆盖轴」的精确划分；回归清单加入该用例 |

### ✅ 已订正（范围 / 机制）

| # | metis finding | 处置 |
|---|---|---|
| 5 | 🔴 **「修 1 处 ⇒ 23 归零」未验证**，且**缺第二处机制**（`parent_landing_wire` 未填 ⇒ 21–23 会 `Duplicate pin connection`） | 新增 §A.3「假说 H-A + H-B」；R21-A 加 **spike 门** + **产物级 lint** 判据 |
| 6 | ⚠️ **判据应更窄**：跳过 `c.into_child == false` 的行（消费记录 ≠ 网），而不是「全放宽」 | R21-A 表按此重写；`into_child == true` 的 fail-closed 写成**不变式**（+ capability 反向 Requirement） |
| 7 | ⚠️ **R21-B 选项集不穷尽**（漏了零库改动的 `ForceLayerScope` 与类级声明）；且「部分成层」未问 | R21-B 扩到 **6 选项** + 要求 architect 回答「部分成层是否可接受」 |
| 8 | ⚠️ **「扁平产物逐字节不变」是结构性恒真** ⇒ 承重性被高估；基线未定义 | Acceptance 改标「**结构性恒真 · 只作跳线 · 不作承重证据**」 |
| 9 | ⬜ **缺判据**：交付粒度发射（探针是 ForceLayer）、R4 确定性、单连接不变式、判别性自证、允许的 lint warning 清单 | R21-C 全补；探针降级为「仅侦察」 |
| 10 | ⬜ **缺同步项**：roadmap R21 行与本草案冲突、ADR-060/066 判据正文、公开面登记、`include/cpu/AGENTS.md` 行数、残差 `id=25`、R17.2 关系 | 「不回归 / 文档同步」段全部补上；关系表加 R17.2 |

### ⚠️ 采纳但**保留 planner 立场**的一条

metis 指出「双类消费者是否合法」是**政策裁决**，planner 不能自行断言 ⇒ ⇒ **已改**：W.3 明写「这是建议、不是裁决」，并把该问题并入 R21-B 的 architect 裁决面（两问之一）。

### ⏸ 未采纳（附理由）

无。
