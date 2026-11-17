# ADR-072: 「RTL 生成能力产品化」—— 顶层端口面收敛 + 产品边界与验收契约

- **Status**: **Accepted · v2**（2026-11-16 · architect role（build agent）· **v1 轮**：oracle ×1 **REJECT**（3 BLOCKER）⇒ 全闭 · critic ×2（第 1 轮 **REJECT** 1 BLOCKER ⇒ 复审 **APPROVE**）· **v2 轮**：oracle ×1 **REJECT**（3 BLOCKER + 3 SHOULD_FIX）⇒ 全闭 · critic ×2（第 1 轮 **REJECT** 2 BLOCKER + 3 NIT ⇒ 全闭 · 复审 **APPROVE · 0 BLOCKER** · `mergeable: yes`）·
  ⚠️ **保留的未决分支**：D1 的条件式（V1 落地确认失败 ⇒ 退到选项 ④）仍是「显式标注的未决」⇒ ⇒ 一旦触发 = 决策内容变更 ⇒ MUST 重新审稿）
- **Date**: 2026-11-16
- **修订 v2.1（2026-11-16 · architect · **事实订正 · 无决策变更** · oracle 复核 **APPROVE · 0 BLOCKER · 3 SHOULD_FIX 全闭**）**：
  用户追问「为什么 `VerilatorBackend` 拿不到 root」⇒ ⇒ 复核发现 **v2 的表述过强**：
  root **原理上可从 ctx 推导**（`src/core/lnodeimpl.cpp:30` 节点记归属 + `include/component.h:79` `parent()` 可上溯）
  ⇒ ⇒ **真实障碍 = 「推导不可靠」** —— `node_owner()` 只有 **2** 条 `nullptr` 路径（`src/component.cpp:16-26`）· **多根** ⇒ 上溯结果不唯一 · `parent_` 是 `weak_ptr` ⇒ 父组件非 shared 管理时上溯静默终止，
  不是「物理不可能」。
  ⇒ ⇒ **订正 6 处**：§1.2 事实 ⑩ · **§1.2 事实 ⑫（同义残留 —— oracle SHOULD_FIX-1）** · 「修订 v2 表」第 1 行 ·
  §2.1.2 D1-a（**精确归因**：`node_owner()` 只有 **2** 条 `nullptr` 路径；**多根** ⇒ 上溯不唯一（不是 `nullptr`）；
  `parent_` 是 `weak_ptr` ⇒ 父组件非 shared 管理时上溯**静默终止**）· §3 选项 ⑩ · §5 V5。
  ⇒ ⇒ **决策零变更**（backend 仍 = 遗留路径 · ① 仍撤回 · V5 仍撤回）⇒ ⇒ 只是**理由变准确且更弱**
  （「推导不可靠」比「不可能」更弱，但结论仍成立：**显式路径已存在 ⇒ 无收益**）。
- **Author**: architect role（build agent）
- **Parent**: **ADR-046**（D10 SoC 端口冻结）· **ADR-058**（层次化 RTL 输出）· **ADR-060**（端口策略）· **ADR-061**（粒度用户可控）· **ADR-066**（父侧落点）· **ADR-070**（RV32I 成层粒度）· **ADR-071**（`FENCE` / 非法指令冻结）
- **关联 change**: **`rtl-productization-surface-2026-11`**（`openspec/changes/rtl-productization-surface-2026-11/` · **已立项** · planner 2026-11-16）⇒ ⇒ 本 ADR 的 **AC 映射** = 该 change 的 **AC1–AC15** + task **T1–T14**（逐条见 §5）
- **审稿记录**: `.rddf/reviews/ADR-072-rtl-productization-surface.md`（**v1 轮** oracle ×1 + critic ×2 · **v2 轮** oracle ×1（REJECT · 3B+3S ⇒ 全闭）+ critic ×2（第 1 轮 REJECT · 2B ⇒ 全闭 · 复审 **APPROVE · 0 BLOCKER** · `mergeable: yes`）· 全部 finding 逐条应答）
- **超长自检**: 正文 **约 430 行**（> 250）⇒ ⇒ 自问「这是不是设计规格？」= **部分是**（§2.8 D8 的 6 条逐条改法要点 = 规格级）⇒ ⇒ **不拆分**，理由：该 6 条是 **spec delta 的唯一权威输入**（拆走 ⇒ builder 会自由发挥 ⇒ ⇒ 违反「契约先行」）⇒ ⇒ 仓内 ADR 既有体量：ADR-060 = 779 行 / ADR-066 = 638 行

### 修订 v2（2026-11-16 · architect · **v1 的 7 处被实测推翻 + v2 自身被 oracle 挖出 4 处**）

> **触发**：planner 的 **metis 第 1 轮**（7 P0 + 3 P1 + 2 P2 全处置）+ change 的 **critic 第 1 轮**（plan 档 · `APPROVE_WITH_CONDITIONS` · 0 BLOCKER · 3 SHOULD_FIX 全处置）**实测挖出 v1 的缺陷**。

| # | v1 的问题 | v2 的改法 | 出处 |
|---|---|---|---|
| **1** | **①（Verilator backend 端口面改造）不可执行** —— `VerilatorBackend` 的 auto-mode **没有被交付 root**（接口只收 `ctx`）· 且**从 ctx 推导 root 不可靠**（见 §2.1.2）⇒ ⇒ 连带的 **V5「77 个 `verilator_e2e` 全绿」也不可执行** | **撤回 ① 与 V5** ⇒ ⇒ **backend = 遗留路径**（ctx-only 旧语义 · **零改动**）⇒ ⇒ 产品产物的验证载体 = **独立 Verilog TB** ⇒ ⇒ 新增 **V5′**（A 类零扰动） | metis P0-1 ② |
| **2** | **② 的 delta 形状错** —— 「各一条 MODIFIED」⇒ ⇒ 实为 **2 份 capability · 5 条 Requirement**；且 **1 条标题会变假**（MODIFIED 改不了标题） | 新增 **§2.8 D8**：**逐条**给改法要点 + **新标题逐字** ⇒ ⇒ 按实测定为 **2 份 capability · 6 条**：**4 MODIFIED**（`:32` / `:98` / `:240` / **`:402`**）+ **2 REMOVED+ADDED**（`rtl-emit-portability` 的 manifest schema · `:190`）⇒ ⇒ **另 4 条不需 delta**（`:478` / `:518` / `:529` / `:637`） | metis P0-1 ⑤ · critic plan 档 · **v2-oracle BLOCKER-1** |
| **3** | **⑩「切换调用点」是事实错误** —— 「带根重载」**不存在**（`include/codegen_verilog.h:65,68,151,347` 全是 `ctx` 形参） | ⑩ = **新增公开入口**（**ADDED** 符号，**不是**行为变更）⇒ ⇒ **只增不改**（ctx-only 保留） | metis P0-1 ① |
| **4** | **「34」的构成未写明** · **manifest `ports[]` 收敛不收敛未定** · **「顶层端口」三处各自实现且口径不同** | **D1-c / D1-d / D1-e** 三条写死 | metis P0-1 ③④ + P0-2 ⑤ |
| **5** | **后代悬空 `ch_in` 的产物语义未定**（v1 只说「不上顶层」） | **D1-f** = **fail-closed** + 新增 **V17** | metis P0-2 ① |
| **6** | **`:98`（无归属节点）与 root-owned 冲突未裁决** | **D1-g** = 归**体内** + 保留 `CHWARN` + **MUST NOT 被拒绝** ⇒ ⇒ 且 `:98` 是 **MODIFIED**（标题中性）**不是** REMOVED+ADDED | critic plan 档 focus 5 |
| **7** | **`:478` / `:518` / `:529` 被误判为「需 delta」** | **裁决：三条都不需 delta**（「既有路径」= 既有 `toVerilog()` 的 ctx 形参重载 ⇒ 新增入口不改它 ⇒ 逐字节不变**天然成立**；「扁平产物」**两个入口都成立** ⇒ 只需**补判据**）⇒ ⇒ **delta 范围从 6/7 条降到 5 条** | 本轮实测（v1 过度扩张）· **v2-oracle Q6 独立核实** |
| **8** | **`:637` 的「D1 下顶层不再有 C 类」= 事实错误** —— `AxiLiteTop` 的根 `__io` 有 **20 个 legacy 未具名端口**（`examples/axi4/axi4_lite_example.cpp:20-31` + 豁免表 `.rddf/checks/port-naming-exempt.tsv:36-55`）⇒ ⇒ D1 下**仍是**顶层端口 ⇒ ⇒ 它们是 **C 类**；而 `axi4_lite_example` 是**首批 10 个 verified 产品目标之一** | **撤回 R-6** ⇒ ⇒ **`:637` 不需 delta**（三类判定与「冒泡端口是否存在」**正交**）⇒ ⇒ 与 `:478` / `:518` / `:529` 同列 | **v2-oracle 第 1 轮 BLOCKER-1** |
| **9** | **「inline 折叠进根」的悬空 `ch_in` 未裁决** —— 现行 spec `:192-194` + Scenario `:213-214` + 测试 `tests/test_hierarchy_verilog.cpp:2181-2192`（`REQUIRE(r.ok)` + 注释「拒绝它们 = 拒绝合法设计」）都要求**放行**；而 D1 契约要求**拒绝**（它不是根自有 ⇒ 不能上顶层） | **D1-f 扩写**：明确「inline 折叠进根 ⇒ **拒绝**」⇒ ⇒ **本条取代** `:192-194` / `:213-214` 的旧裁决（旧理由「在扁平里本就是顶层 input」的**前提已不成立**）+ 新增 **V18** + 同批必改 ④ 补 `:2181-2192` 与 4 个 `[bubble]` 用例 | **v2-oracle 第 1 轮 BLOCKER-2** |
| **10** | **冒泡机制在 D1-f 后不可达 ⇒ 死代码**（`UndrivenPolicy::Bubble` / `HierarchyPortOrigin::Bubbled` / `PortProvenanceOrigin::Bubbled` / `ensure_bubbled_port`）⇒ ⇒ 且 `tests/test_component_granularity.cpp:248-254` 的 `static_assert` 把它钉为 canonical 默认 ⇒ ⇒ 一退役即**编译失败** | 新增 **D1-h**：**MUST 退役 · 时点 = 同批**（不留「实施期可选」）+ 公开面移除登记 + V19 | **v2-oracle 第 1 轮 SHOULD_FIX-2** |
| **11** | **Status 曾写「Accepted · v2」而 §7 不存在** ⇒ ⇒ 「把未决问题写成已决」 | 退回 **Draft · v2 待审稿** | **v2-oracle 第 1 轮 BLOCKER-3** |
| **12** | **R-4 要点④ 与 D1-h 自相矛盾** —— R-4 写「枚举是否退役 = 实施期可选简化」，D1-h 写「MUST 退役 · 时点 = 同批」，V19 又把「零残留」列为 🔴 ⇒ ⇒ builder 无法决定 delta 里该写哪个 | R-4 要点④ 改为「**MUST 退役（见 D1-h）· 时点 = 同批** · MUST NOT 留可选项余地」 | **critic v2 第 1 轮 BLOCKER-1** |
| **13** | **§2.8 覆盖不全** —— 主 spec 另有 4 处旧契约面不被 6 条 delta 覆盖：`:240` 的 Scenario `:336-344`（「未驱动子 input MUST 冒泡」）· `:402` 正文（指称 `UndrivenPolicy` 的 `Bubble`）· capabilitiy Purpose `:12-13`（「悬空子输入 MUST 向上冒泡」）· `rtl-emit-portability:64-69`（「manifest 不定义端口面」的解读） | R-5 补第 4 处改法 · **新增 R-6（`:402`）⇒ delta 5 → 6 条** · Purpose 两处进 §6「归档后手工写」· R-1 补 `:64-69` 解读 · 新增「MODIFIED 的完整性义务」段 | **critic v2 第 1 轮 BLOCKER-2** |
| **14** | **退役清单漏 `BubbledInput` 结构与其消费者** ⇒ ⇒ 退役后残留死公开结构 ⇒ 与 D1-h 自我矛盾 | D1-h / 同批必改 ④′ / V19 三处补入 `BubbledInput`（`include/hierarchy_analysis.h:121,152`）+ `src/hierarchy_verilog.cpp:144,1261` 的 `dangling` 消费 | **critic v2 第 1 轮 SHOULD_FIX-1** |

### 修订 v2.2（2026-11-17 · architect · **D4 第 3 行被部分 supersede** · 决策面变更 ⇒ ⇒ **不是**「无决策变更」的 dated 订正）

> **触发**：用户 2026-11-17 追问「为什么不能直接从 `*example.cpp` 生成 dut + tb」⇒ ⇒ 分析后由 **ADR-074**（验证单一真相源与生成式 TB）+ **ADR-075**（端口具名化 = 产品面前提）承接 ⇒ ⇒ 本 ADR 有**一处决策内容被修订**，其余**零变更**。

| # | 本 ADR 的条目 | v2.2 的处置 | 依据 |
|---|---|---|---|
| **①** | **§2.4 D4 第 3 行**：「激励 + 断言 + **期望值** \| 人（手推） \| 期望值 MUST 手推（禁抄仿真输出）⇒ **不可自动化**」 | 🔴 **部分 supersede** ⇒ ⇒ **被取代的确切对象 = 该行末句「**不可自动化**」**（不是整行）⇒ ⇒ 拆成两行：**「撰写」（场景怎么走 / 值该是多少）= 人 ⇒ 推导与撰写不可自动化，不变** · **「渲染」（`.verif` → `.sv` 转写）= 生成器 ⇒ 新增能力**（判据有单一语义载体 IR）⇒ ⇒ ⚠️ **生成器不产生期望值**，只转写人写的 `.verif` | **ADR-074 D1 / D5**（⚠️ 推导层仍在 `.rddf/checks/*.md`，`EXPECT-SRC` 仍指向它 ⇒ ⇒ 护栏 ②' **字面不变**） |
| **②** | **§2.2 D2**「产品单元 = **四件套**（manifest + 产物 + 独立 TB + 覆盖登记）」 | ✅ **零变更** ⇒ ⇒ TB **仍进仓**（checked-in）⇒ ⇒ 只**新增**「TB 是生成物 ⇒ MUST 有新鲜度判据（重新生成逐字节相同）」 | **ADR-074 D2**（⚠️ 被否的替代 = build-time 不进仓 ⇒ ⇒ 那会改 D2 的语义 ⇒ ⇒ 已拒） |
| **③** | **§2.1.2 D1-a** 末句「产品产物的验证载体 = **独立 Verilog TB**」 | ✅ **零变更** ⇒ ⇒ (A)/(B)/(C) 三条路径的分工**实质不变** ⇒ ⇒ 变的只是 (C) 的**撰写形态** | **ADR-074 D7** |
| **④** | （无对应条目）| 📌 **事实登记**（**不是**决策变更）：库层 `run_emit_cli` **已于 2026-11-17 支持 `--hierarchical`**（commit `2db2eda` · `src/rtl_emit/cli.cpp:77-78, 186-208`）⇒ ⇒ 「层次化发射只能自己实现」的旧形态**已不成立** ⇒ ⇒ 发射面收敛为**实施收敛**（蓝图 §4.3），**不需要新的 ADR** | — |

⚠️ **Status 行不动**（本 ADR 仍为 `Accepted · v2` ⇒ ⇒ v2.2 是**增补** ⇒ ⇒ **不改变**已决状态）⇒ ⇒ 唯一被修订的 D4 第 3 行**由 ADR-074 承接** ⇒ ⇒ 若 ADR-074 被否 ⇒ ⇒ 本行必须回滚。
⚠️ **归档后手工写**：`docs/CHANGELOG.md` 需一条「ADR-072 D4 第 3 行被 ADR-074 部分 supersede」。

---

## 1. 背景与问题

### 1.1 诉求（用户 2026-11-16）

两问合一件：**评估** CppHDL 能否生成功能完整的 RISC-V CPU 核 RTL；**推进**为「RTL 生成能力产品化」。
覆盖面 = 通用生成器 + 独立 Verilog testbench · 固件转储器/ELF 装载 · examples 全覆盖 · 层次化输出示例 ·
端口名与 C++ 侧一致 · RV32I 重复实现收口 · RTL 卫生。

### 1.2 实测事实（2026-11-16 · 可复算）

| # | 事实 | 证据 |
|---|---|---|
| ① | ✅ **扁平 RTL 可生成**：RV32I SoC 产物 **130 端口** · **诊断 0 条** · manifest **213 条** | `emit_rtl_rv32i_soc --out <dir>` ⇒ `OK：端口 213 个 · 诊断 0 条`；与 `examples/riscv-mini/tb_rv32i_soc.sv:60` 自述**独立吻合** |
| ② | ✅ **层次化 RTL 可生成**：**8 module** · 顶层 **68 端口** | 只读探针 ⇒ `IfStage 13 / IdStage 31 / ExStage 21 / MemStage 11 / WbStage 11 / HazardUnit 22 / ForwardingMux_32_ 10 / top 68` |
| ③ | ✅ **独立 Verilog TB 可跑**：`checks=24 fails=0`（手推固件） | `examples/riscv-mini/tb_rv32i_soc.sv` |
| ④ | ✅ **固件链路完整**：ELF32 + `PT_LOAD` · `.bss` 补零 · 三格式 | `tools/rv32i-fw-dump/main.cpp` |
| ⑤ | ✅ **ISA 用户态覆盖完整** + 非法指令 **4 态**（0 合法 / 1 已知未实现 / 2 未知 opcode / **3 保留组合**） | `include/cpu/pipeline/rv32i_decoder.h:217-220` |
| ⑥ | 🔴 **顶层端口面 = 130 / 68，其中 **96 / 34** 是后代 `ch_out` 冒泡**（无语义 · `unnamed_output_N`） | 根自有 **32** + 框架 **2** + 冒泡 **96** = 130 · 32 + 2 + **34** = 68 |
| ⑦ | 🔴 **codegen 没有「顶层面」概念** —— 端口 = 「图中**每个** `type_output`」 | `src/codegen_verilog.cpp:410-448` |
| ⑧ | 🔴 **manifest 不携带产品端口表** —— `is_top_io = (owner == &root)` ⇒ ⇒ 实测 `is_top_io: true` = **32** vs 产物顶层 **130** ⇒ ⇒ **98 个错标** | `src/rtl_emit/manifest.cpp:155-156` |
| ⑨ | 🔴 **「顶层端口」三处各自实现且口径互不相同** | `scripts/lib/flat_rtl_emit_check.py:79-92`（**契约算法**）/ `src/rtl_emit/tb_stub.cpp:59`（`p.is_top_io`）/ `tests/test_rtl_emit_contract.cpp:142`（`e.is_top_io`） |
| ⑩ | 🔴 **`VerilatorBackend` 的 auto-mode **未被交付 root**（接口只收 `ctx`）** · ⚠️ **但 root **原理上可从 ctx 推导**（节点记归属 + `Component::parent()` 可上溯）⇒ ⇒ **真实障碍 = 「推导不可靠」，不是「不可能」**（见 §2.1.2 D1-a 的三类 `nullptr` 形态） | `include/core/eval_backend.h:25`（`initialize(context*, …)`）· `include/core/verilator_backend.h:96`（ctor 只收 `verilog_path`）· `include/simulator.h:68`（`Simulator(context*, …)`）· `src/core/verilator_backend.cpp:857-867`（端口表来自 `ctx->get_eval_list()`）· **推导面**：`src/core/lnodeimpl.cpp:30`（`parent_ = node_owner()`）· `src/component.cpp:16-26`（`node_owner()` 的**三种返回 `nullptr`**）· `include/component.h:79`（`parent()` 可上溯）· 旁证 = `include/hierarchy_verilog.h:144`（`toVerilogHierarchical` **显式收 root** ⇒ 框架选择「不猜」） |
| ⑪ | ✅ **显式入口已存在**：`try_initialize_with_verilog(path, ports, dm)` + `backend_port_set(emitted, root_module)` | `include/core/verilator_backend.h:113,121` · `include/hierarchy_verilog.h:154` ⇒ ⇒ **层次化**产物可被 backend 消费；**扁平**产物**无**对应入口 |
| ⑫ | 🔴 **ctx-only 公开 API **无根注册表可直取**（root 只可**不可靠地推导** —— 见事实 ⑩ · **不是**「不可推导」）· 仓内 **49** 处活跃调用点 · ⚠️ **产品路径走的就是 ctx-only** | `include/core/context.h:83-247` · `include/codegen_verilog.h:65,68` · `src/rtl_emit/cli.cpp:181` |
| ⑬ | 🔴 **无任何以 `Component&` 为根的公开入口** | `include/codegen_verilog.h:65,68,151,347` |
| ⑭ | 🔴 **capability 契约与「端口面收敛」逐字相反**（**5 条 Requirement** · 见 §2.8） | `rtl-emit-portability:61-62` · `rtl-hierarchical-output:32,98,190,240,637` |
| ⑮ | 🔴 **两份 Accepted ADR 的决策内容被实质修订** | `ADR-058:63-64` + `ADR-061:132`（写死的枚举「任意 `ch_out` ⇒ 顶层 `output`」） |
| ⑯ | ✅ `:478`「既有路径 MUST 逐字节不变」/ `:518`「全 inline 与扁平逐字节一致」/ `:529`「层次化与扁平等价」**三条不需 delta**（理由见 §2.8 末） | 本轮实测 |
| ⑰ | ⚠️ **examples 覆盖 9/28 · 缺口结构性不可见**（护栏只比自身锚点）· 排除表已漂 19 → 28 | `verify_examples_rtl_sim.sh:89-107` · `examples-rtl-sim-exclusions.md:23` |

### 1.3 评估结论

> **能力上「能」—— 产物形态上「还不能称为 CPU 核 RTL 产品」。** 三个**正交**缺口：
> **G1** 顶层端口面未收敛 + manifest 不自洽 + 契约相反（事实 ⑥⑦⑧⑨⑭⑮）·
> **G2** 特权级缺失 ⇒「冻结是终态」（事实 ⑤ 只覆盖用户态）·
> **G3** 覆盖缺口不可见（事实 ⑰）。

### 1.4 正交性

G1 = **发射器面** · G2 = **ISA 集成面**（独立立项 · 与 G1 零写点交集）· G3 = **登记/护栏面**（已拆独立 change `rtl-productization-coverage-registration-2026-11`）。
⇒ ⇒ **G1 MUST 原子落地**（§2.1 的同批必改表）。

---

## 2. 决策

### 2.1 **D1**（G1）= 顶层端口面 = **根组件自有 `__io`**（**直接收敛** · 推翻残差 id=23 的 2026-11-11 裁决）

#### 2.1.1 契约（**不变**）

> 顶层端口表 = `default_clock` → `default_reset` → **根组件自有 `__io`**（无内部 driver 的 `ch_in` + 全部 `ch_out`）
> ⇒ ⇒ 后代组件的 `ch_out`（含被父层消费的）**一律降为内部 `logic`**（不是端口）。

⇒ ⇒ **RV32I SoC**：扁平 **130 → 34** · 层次化 **68 → 34**。
⇒ ⇒ **与 ADR-046 D10 同向**：D10 冻结的正是**根组件自有 32 个** ⇒ ⇒ 本决策让**产物顶层**收敛到那一套 ⇒ ⇒ **对齐，不是新造**。

#### 2.1.2 **D1-a**（v2 新增）= **撤回原 ①**：`VerilatorBackend` = **遗留路径**（零改动）

- `VerilatorBackend` 的 auto-mode **只有 `ctx`**（事实 ⑩）⇒ ⇒ **root 未被交付**。
- ⚠️ **但「拿不到」是过强的说法（**v2.1 事实订正**）**：root **原理上可推导** —— 每个节点建时记归属（`src/core/lnodeimpl.cpp:30`），`Component::parent()` 可上溯到最顶层（`include/component.h:79`）。
  ⇒ ⇒ **真实障碍 = 「推导不可靠」**，四个来源（**精确归因** · v2.1-oracle SHOULD_FIX-3）：
  **① `node_owner()` 返回 `nullptr` 的两条路径**（`src/component.cpp:16-26` 只有两条 `return nullptr`）：`node_ownership_suppressed()` 期间（唯一置位点 = `src/core/context.cpp:111` 的 RAII ⇒ 建 `default_clock`/`default_reset`）· `scope_.empty() && current_ == nullptr`（用户在 `describe()` 之外建节点）
  **② 多根** ⇒ ⇒ `node_owner()` **不是** `nullptr`，而是**上溯结果不唯一**（`Component::build(external_ctx)` 是公开 API ⇒ 同一 ctx 可建多棵组件树 · ctx 级节点无根）
  **③ `parent_` 是 `weak_ptr`** ⇒ ⇒ 父组件非 `shared_ptr` 管理时上溯**静默终止**（`src/component.cpp:54-56,88-92`；`Simulator` **不持有**组件树 —— `include/simulator.h:68` 只收 `ctx*`）
  ⇒ ⇒ 用启发式 ⇒ **端口面建立在一个会静默误分类的猜测上**（本仓最怕的缺陷族）。
  ⇒ ⇒ **旁证**：`toVerilogHierarchical(path, Component &root, …)` **显式收 root**（`include/hierarchy_verilog.h:144`）⇒ 框架自己也选择「不猜」。
- ⇒ ⇒ **裁决：auto-mode 的 backend 保持 ctx-only 旧语义**（130 端口 · **零改动**）。
- ⇒ ⇒ **后果必须显式声明**（下游面）：**产品产物（34 端口）≠ 仿真器产物（130 端口）· 行为等价、端口面不同**。
  ⇒ ⇒ 理由：**对 RV32I** 差别**只在顶层端口面**且多出来的 **96 个全是 `ch_out`**（事实 ⑥）⇒ ⇒ 内部逻辑与逐拍行为**不变**（v2-oracle Q1 已独立核实两侧的 `type_input` driver 过滤**逐字同构** ⇒ D1 不引入新变化）。
  ⇒ ⇒ ⚠️ **不得外推为一般性不变量**（v2-oracle SHOULD_FIX-3）：含**后代悬空 `ch_in`** 的设计在产品路径上是**拒绝**（D1-f），**不是**「变成 output」⇒ ⇒ 「端口面差别 ⊆ 输出」**只对 RV32I 成立**。
- ⇒ ⇒ **产品产物的验证载体 = 独立 Verilog TB**（诉求「独立 Verilog testbench」正好落在这里）。
- ⇒ ⇒ **层次化**产物可用既有显式入口（事实 ⑪ 的 `backend_port_set`）；**扁平**产物**无**对应入口 ⇒ ⇒ 若将来需要 ⇒ ⇒ **另立**项。
- ⚠️ **被拒的替代方案**：「让 backend 跟随产品面」⇒ ⇒ 需新增「root 进 backend / Simulator」的设计点 + 推翻 **ADR-058 §2.3 决策③「既有入口一律不动」** ⇒ ⇒ 代价远超本决策收益 ⇒ ⇒ **拒**（见 §3 选项 ⑩）。

#### 2.1.3 **D1-b**（v2 修订）= 新增**带根**公开入口（**只增不改**）

- 新增：`toVerilog(path, Component&, EmitOptions)` + `make_flat_module_view(Component&, bool)`
  ⇒ ⇒ **`ctx` 形参的既有入口全部保留**（事实 ⑬ ⇒「带根重载」不存在 ⇒ 这是**新增符号**，**不是**行为变更）。
- ⇒ ⇒ **ctx-only 路径 = 非产品遗留路径**（49 处活跃调用点 · **不迁移** · 拓展半径远超本决策）
  ⇒ ⇒ MUST 在 manifest / 工具文档里**显式标注**（否则下游会把它当产品路径）。
- ⇒ ⇒ **产品路径 = CLI 切到带根入口**（`src/rtl_emit/cli.cpp:181`）⇒ ⇒ **不切 ⇒ D1 在产品路径上根本不生效**。

#### 2.1.4 **D1-c**（v2 新增）= manifest：`ports[]` **不收敛** + **新增顶层数组** + schema **v1 → v2**

| 子项 | 裁决 | 理由 |
|---|---|---|
| `ports[]` | **仍列全量**（节点清单 · 213 条） | `tests/test_rtl_emit_contract.cpp:278` 的 `top_ports < man.ports.size()` ⇒ ⇒ 收敛 `ports[]` ⇒ ⇒ 该断言恒红 |
| 新增 | **顶层数组 `port_table: [{name, dir, width, role}]`**（**按产物顺序**） | 不用「加第 11 个条目字段」⇒ ⇒ 那会改「每个条目 MUST 携带全部 10 个字段」这个数 |
| schema | **`rtl-emit-manifest-v1` → `rtl-emit-manifest-v2`** | 新增顶层键 = schema 变更 ⇒ ⇒ `tests/test_rtl_manifest.cpp:114` 的 schema 名断言必须同步 |
| `is_top_io` | **语义不变**（仍是「根组件自有 IO」）⇒ ⇒ 但 **MUST NOT** 被用作端口面的定义 | 事实 ⑧ ⇒ ⇒ 端口面的**唯一权威** = `port_table[]` |

#### 2.1.5 **D1-d**（v2 新增）= 「34」的构成写死

> **产品端口表 = 框架基础设施 2（`default_clock` / `default_reset`）+ 根组件自有 `__io` 32 = 34**
> ⇒ ⇒ ⚠️ **「产品端口表」≠ `is_top_io`** —— 后者对 clock/reset 是 `false`（它们 `owner == nullptr`）⇒ ⇒ 按 `is_top_io` 筛只得 **32**。

#### 2.1.6 **D1-e**（v2 新增）= 顶层端口判定**收敛到唯一来源**

- **唯一来源 = manifest 的 `port_table[]`** ⇒ ⇒ 三处消费点**只消费**，不再各自实现（事实 ⑨）：
  `src/rtl_emit/tb_stub.cpp:59` · `tests/test_rtl_emit_contract.cpp:142` · `scripts/lib/flat_rtl_emit_check.py:79-92`。
- ⚠️ **不收敛的后果**：`tb_stub` 会把**全部 output 条目**（`ports[]` 的 output 侧 = 根自有 28 + 冒泡 96 = **124**）连到 34 端口的 DUT 上 ⇒ ⇒ 派生式 = `ports[] 中 dir==output 的条目数`（**禁**写死）。

#### 2.1.7 **D1-f**（v2 新增 · **v2-oracle 第 1 轮后扩写**）= 后代悬空 `ch_in` = **fail-closed**

> **根自己**的悬空 `ch_in` ⇒ MUST **是顶层端口**（D1 契约第一项的第二半）✔
> **后代**的悬空 `ch_in` ⇒ 发射 MUST **返回 `ok = false`** + `error` 点名（端口名 + module + 层级）+ `files` 为空
> ⇒ ⇒ **MUST NOT** 静默降格为 ① 顶层端口 ② 体内未驱动 wire ③ tie-off。

**「后代」的界定（**v2-oracle 第 1 轮 BLOCKER-2 的闭合**）**：

| 子类 | 归属 | 裁决 |
|---|---|---|
| 根的 `ch_in`（`owner == &root`） | 根自有 | **放行** ⇒ 是顶层端口 |
| **`inline` 折叠进根的后代 `ch_in`**（它属于**内联子组件** ⇒ `owner` = 该子组件 ≠ 根） | **不是根自有** | 🔴 **拒绝**（fail-closed）⇒ ⇒ ⚠️ **本条取代** `rtl-hierarchical-output/spec.md:192-194` 与 Scenario `:213-214` 的旧裁决（旧裁决的理由是「**在扁平里本就是顶层 input**」⇒ ⇒ 那个**前提**在 D1 下**已不成立**） |
| 非根成层 module 内的后代悬空 `ch_in` | 不是根自有 | 🔴 **拒绝** |
| ⚠️ **`owner == nullptr` 的 `type_input`**（**critic v2 第 1 轮边界登记**：`src/hierarchy_analysis.cpp:412-416` 有该形态的防御分支 ⇒ 框架作者认为**可能出现**） | 无归属 | **由 D1-g 管辖**（归体内 + `CHWARN` + **不拒绝**）⇒ ⇒ **不属于 D1-f** ⇒ ⇒ 与 D1-f 的分类键 `get_parent()` **不相交** |

- 理由：三个候选语义**完全不同**（① 改端口面 ② 读 X ③ 静默改行为）⇒ ⇒ 前两者**静默** ⇒ ⇒ fail-closed 与仓内「响亮拒绝」纪律一致。
- ⚠️ **代价（如实登记）**：对「inline 子组件有未驱动输入」的设计，**今天能发射**，D1 后**发射失败** ⇒ ⇒ 这是**行为回归** ⇒ ⇒ 但它是**诚实**的（那种设计的输入本就**无来源** ⇒ ⇒ 作为独立产品不成立）⇒ ⇒ 诊断 MUST 点明「该输入无驱动源且不属于根 `__io` ⇒ ⇒ 请驱动它或把它上提到根的 `__io`」。
- ⚠️ **需同步**（**v2-oracle SHOULD_FIX-1 的补漏**）：`tests/test_hierarchy_verilog.cpp` 的 **4 个 `[bubble]` TEST_CASE**（`:2044` 冒泡连线 / `:2096` inline 冒泡 / `:2162` Reject 列全部 / `:2239` provenance `Bubbled == 1`）+ **防误拒用例 `:2181-2192`**（`RootUndrivenTop` · 现断言 `REQUIRE(r.ok)` + 注释「拒绝它们 = 拒绝合法设计」）⇒ ⇒ **MUST 重设计为「验证拒绝」**（断言 `ok == false` + error 点名）**或删除** ⇒ ⇒ ⚠️ **不是**「改期望端口表」（fail-closed 后**产物不存在** ⇒ 无表可改）· 友好处置条目 `:2638-2639` · `tests/hier_rtl_products.cpp:114-141,476`（护栏 R8 的产物发生器）· `scripts/verify_hierarchical_rtl.sh:501-520` 的 `bubble` 语料。

#### 2.1.8 **D1-g**（v2 新增）= **无归属节点**（`owner == nullptr`）的处置

| 子类 | 裁决 |
|---|---|
| `default_clock` / `default_reset`（框架基础设施） | **MUST 是顶层端口** · MUST NOT 触发 `CHWARN`（既有 Scenario 已要求） |
| 其余 `owner == nullptr` 的节点（**用户直接创建**） | **归顶层 module 的体内**（**MUST NOT** 是顶层端口）· **MUST 保留 `CHWARN` 点名** · **MUST NOT 被拒绝** |

- 理由：`:98` 的既有义务是「**归属空洞必须可观测**」⇒ ⇒ `CHWARN` **MUST 保留**；「归顶层 module」= **归属**，**不等于**「占顶层端口」（**v2-oracle Q4 已独立核实**：`:98` 的三个条款 + 唯一 Scenario **均无**「用户直建节点的值可经顶层端口观测」的断言 ⇒ ⇒ 标题**确实中性**）⇒ ⇒ **`:98` 是 MODIFIED（不是 REMOVED+ADDED）**。
- ⚠️ **与 D1-f 的差别**：悬空 = **无驱动源** ⇒ ⇒ fail-closed；无归属 = **有逻辑无归属** ⇒ ⇒ 归体内 + 告警（**不**拒绝）。
- ⚠️ **两条款无冲突（v2-oracle Q4 已证）**：`ch_in` **端口节点恒由所属组件创建** ⇒ `owner != nullptr` ⇒ ⇒ 与 `owner == nullptr` 集合**不相交**。

#### 2.1.9 **D1-h**（v2-oracle 第 1 轮 SHOULD_FIX-2 新增）= **冒泡机制与 `UndrivenPolicy::Bubble` 的退役**

- D1-f 让「后代悬空 `ch_in`」**恒被拒绝** ⇒ ⇒ **冒泡端口零生产** ⇒ ⇒ 下列符号在**层次化路径上不可达**：
  `HierarchyPortOrigin::Bubbled`（`src/hierarchy_analysis.cpp:404-415` 的生产者 ⇒ `ensure_bubbled_port`）·
  `PortProvenanceOrigin::Bubbled` · `UndrivenPolicy::Bubble` ·
  **`BubbledInput` 结构**（`include/hierarchy_analysis.h:121,152`）及其消费者（`src/hierarchy_verilog.cpp:144,1261` 的 `dangling` 收集与 provenance 映射）—— **critic v2 第 1 轮 SHOULD_FIX-1 补入**（漏它 ⇒ 退役后残留死公开结构 ⇒ 与 D1-h 自我矛盾）。
- ⇒ ⇒ **裁决：MUST 退役**（ZERO-DEBT「No dead code：立即删除」）⇒ ⇒ **时点 = 本 change 同批**（**MUST NOT** 留作「实施期可选简化」）。
- ⇒ ⇒ **形式**：`UndrivenPolicy` 的 **`Bubble` 值删除**（剩 `Reject`）；两个 `*Origin::Bubbled` 值删除 ⇒ ⇒ **MUST 走公开面「移除 / 改名登记」**（`.rddf/checks/public-api-surface.md` + 探针 + CHANGELOG）。
- ⚠️ **必同步**：`tests/test_component_granularity.cpp:248-254` 的 `static_assert(undriven_policy == ch::UndrivenPolicy::Bubble)`（**canonical 默认值**）⇒ ⇒ MUST 改为新默认（`Reject` 或删除该断言）⇒ ⇒ ⚠️ **不改 ⇒ 编译失败**（v2-oracle 实测）。
- ⚠️ **默认行为变化**：`UndrivenPolicy` 的默认从 `Bubble` 变 `Reject` ⇒ ⇒ 与 D1-f 一致 ⇒ ⇒ MUST 在 CHANGELOG 登记。

#### 2.1.10 同批必改（**v2 重列** · ① 已撤回 · **v2-oracle 后 11 项 → 12 项**）

| # | 必改项 | 原因（事实） |
|---|---|---|
| ① | 🔴 **capability delta（2 份 spec · **6 条** Requirement · Purpose 两处归档后手工写）** ⇒ ⇒ **改法要点逐条见 §2.8 D8** ⇒ ⇒ MUST 走 change 流程 | ⑭⑮ |
| ② | **manifest 契约**：新增 `port_table[]` + schema v2（**D1-c / D1-d**） | ⑧ |
| ③ | **10 份手写 TB** 的端口映射 · `axi4_lite` 的 S8 场景整体删除（**8** 条判据 ⇒ `min_checks` 45 → 37 · `scenarios` 8 → 7） | 冒泡端口消失 ⇒ 连线悬空 |
| ④ | **护栏/测试期望集**（**v2-oracle 补漏后的完整清单**）：`flat_rtl_emit_check.py:79-92` 的 F3 **契约算法** · `verify_flat_rtl_emit.sh:119` 的 F3 echo · `verify_hierarchical_rtl.sh` R8 + `:501-520` 的 `bubble` 语料 · `verify_hierarchical_port_naming.sh` 语料 · `tests/test_hierarchy_verilog.cpp`（`:2079` 顺序 / `:2876-2884` 端口表 / `:2972-2994` 全 inline / `:3154` 全覆盖 / **`:2044` `:2096` `:2162` `:2239` 的 4 个 `[bubble]` 用例** / **`:2181-2192` 防误拒用例** / `:2638-2639` 友好处置条目）· `tests/hier_rtl_products.cpp:114-141,476` · `tests/test_component_granularity.cpp:248-254`（**`UndrivenPolicy::Bubble` 的 `static_assert`**）· `tests/test_rtl_manifest.cpp` · `tests/test_rtl_tb_stub.cpp` · `tests/test_rtl_emit_contract.cpp` · `tests/test_hierarchy_equivalence.cpp` | 契约成文写死旧规则 ⇒ 不是实现细节 |
| ④′ | **冒泡机制退役**（D1-h）：`UndrivenPolicy::Bubble` · `HierarchyPortOrigin::Bubbled` · `PortProvenanceOrigin::Bubbled` · `ensure_bubbled_port` · **`BubbledInput`**（`include/hierarchy_analysis.h:121,152`）· `src/hierarchy_verilog.cpp:144,1261` 的 `dangling` 消费 的删除 + **公开面移除登记** | ZERO-DEBT「No dead code」⇒ D1-f 后**不可达** |
| ⑤ | **`.rddf/checks/examples-rtl-sim-targets.tsv`** + `examples-rtl-expectations.md` | 与 ③ 同源 |
| ⑥ | **公开面登记**：新增带根入口 = **新增公开符号**（**非**行为变更）⇒ ⇒ `public-api-surface.md` + `CHANGELOG` | `.rddf/checks/public-api-surface.md` 机制 |
| ⑦ | **残差 id=23 的状态**（台账 §7 + 登记表**两侧同批**）⇒ ⇒ ⚠️ **只能在 archive 之后**（闭口证据须指向 archive 目录） | 「保持现状」被推翻 ⇒ 护栏判双向 1:1 |
| ⑧ | **`ADR-058` / `ADR-061` 的 dated addendum**（「任意 `ch_out` ⇒ 顶层 `output`」是**写死的决策内容**）⇒ ⇒ **决策内容变更 ⇒ 需复审** | ⑮ |
| ⑨ | **CLI 切到带根入口**（`cli.cpp:181`） | D1-b |
| ⑩ | **顶层端口判定收敛到唯一来源**（D1-e 的三处消费点） | ⑨ |
| ⑪ | **用户面/架构文档同步**：`docs/usage_guide/05-component-design.md:552,680` · `HIERARCHICAL-RTL-AUTHORING-GUIDE.md:49-55,88` · `PORT-NAMING-GUIDE.md:78` ⇒ ⇒ 「扁平规则」从此**一分为二**（ctx-only 旧规则 vs 带根新规则）⇒ ⇒ MUST 写明**哪条入口适用哪条规则** | 事实 ⑫ |

> ⚠️ **NOT IN SCOPE**：**不**改 `Rv32iPipelineTop` 的 `__io`（ADR-046 D10 的 32 端口快照**零改动**）·
> **不**改 `VerilatorBackend`（D1-a）· **不**迁移 49 处 ctx-only 调用点（D1-b）。

### 2.2 **D2** = 产品单元 = **四件套**

> ① manifest（机器可读契约）· ② Verilog 产物 · ③ **独立可跑的** Verilog TB（激励 + 断言 + `RESULT: checks=<N> fails=0` + 失败非零退出码）· ④ 覆盖登记。

⇒ ⇒ 「产品化完成」= **四件套齐备且 TB 独立可跑**。「未完成」= 只有 ①②（**工具**，不是**产品**）。
⇒ ⇒ ⚠️ **四件套判据的载体 MUST 有判别力** —— `verify_examples_rtl_sim.sh` 的**新增判据④**（逐产品单元断言 manifest 的 `port_table[]` == 该单元产物端口表）⇒ ⇒ **今天恒红**（32 vs 130，差 98）⇒ ⇒ 实施后转绿。

### 2.3 **D3** = ISA 边界 = **RV32I 用户态**

- 含 `FENCE`=NOP · 非法指令 **4 态** 可观测冻结 · **不含** CSR / `ECALL` / `EBREAK` / `MRET` / trap / 中断。
- ⇒ ⇒ 特权级 = **独立立项**（`rv32i-privileged-csr-2026-11` **已 draft**）⇒ ⇒ **本产品化 MUST NOT 顺手塞进来**。
- ⇒ ⇒ **产品 MUST 显式声明该限制**（「不含特权级」+「**冻结是终态**」—— 后者是**运行时可观测的终态** ⇒ ⇒ 不声明 = 误导）。

### 2.4 **D4** = 通用 TB = **机械部分生成 + 判据人工写**

| 部分 | 谁产出 | 理由 |
|---|---|---|
| 端口连线 · clk/rst · VCD | 生成器 | 纯机械 |
| `RESULT: checks=<N> fails=0` 骨架 + 失败非零退出码 | 生成器 | 纯机械 · **且它承载「独立可跑」** |
| 激励 + 断言 + **期望值**（**撰写**：场景怎么走、值该是多少） | 人（手推） | 期望值 MUST 手推（禁抄仿真输出）⇒ **推导与撰写不可自动化**（**✅ 本条 v2.2 不变**） |
| 激励 + 断言（**渲染**：`.verif` → `.sv` 的转写 · **2026-11-17 新增行**） | 生成器 | **新增能力**（**ADR-074 D1**）：判据有**单一语义载体**（IR）⇒ 转写可机械化。⚠️ 生成器**不产生**期望值 ⇒ **只转写人写的 `.verif`** |


⇒ ⇒ **「examples 全覆盖」= 人工工作** ⇒ ⇒ MUST NOT 承诺成「一键产出完整 TB」。

### 2.5 **D5** = 覆盖口径 = **维持首批 10** + 登记完备性

- 覆盖面 = 首批 **10**（9 target + 1 `kind=dut`）⇒ ⇒ 用户 2026-11-16 裁决**维持** ⇒ ⇒ tsv 锚点 `REGISTERED_TARGETS: 10` **不变**。
- **登记完备性 MUST 补**：每个 tracked target MUST ∈ tsv ∪ 排除表；排除表计数**从 tracked 集合派生**（禁手写）⇒ ⇒ 堵死 19 → 28 的漂移。
- ⚠️ **本项已拆独立 change** `rtl-productization-coverage-registration-2026-11`（写点与 G1 零交集 ⇒ 可并行）。

### 2.6 **D6** = RV32I 收口判据（**流程契约**）

「**同一语义只有一个权威实现**」+ **任何新组件接入流水线的 change MUST 先过差分测试**（形态 = `tests/test_rv32i_alu_differential.cpp`）。
⇒ ⇒ 否则「收口」只是一次性清扫。⇒ ⇒ ⚠️ **本项随 D5 一并拆出**。

### 2.7 **D7** = 卫生进默认档（**仅 CLI 层**）

- **A 类（进默认档）**：零未引用信号 · 零恒等直通 · 零未声明引用 · lint exit 0 · 确定性 · 诊断 0。
- **B 类（进默认档）**：`default_nettype none` · 确定性元数据头 · 不发 blanket `lint_off` · 复位写法规范化。
- ⚠️ **作用域**：**只翻 `CliOptions::emit_hygiene`**；**库层 `EmitOptions{}` / `verilogwriter(ctx)` 不动**（那是**每个** auto-mode Verilator 仿真的路径）。
- ⚠️ **v2 补**：逃生口 = `--no-emit-hygiene`；两处护栏里的显式 `--emit-hygiene` **保留**（护栏**必须**显式打卫生产品 ⇒ 删了会让 F8 失去靶子）。
- ⚠️ **为什么 B 类能进默认档**：默认 off 的**唯一**理由是「保产物逐字节不变」⇒ ⇒ **D1 已改默认产物字节** ⇒ ⇒ 理由消失。

### 2.8 **D8**（v2 新增）= **顶层端口面的边界条款裁决**（**6 条** delta 的**唯一权威输入**）

> ⚠️ 本节 = **spec delta 的规格**。`builder` 写 delta 时 MUST **逐条照抄**本节的改法要点与**新标题逐字** ⇒ ⇒ **MUST NOT** 自由发挥（否则 = 在实施期决定契约 ⇒ 违反「契约先行」）。

#### R-1 · `rtl-emit-portability`（**REMOVED + ADDED**）

| 项 | 内容 |
|---|---|
| **旧标题（逐字）** | `端口清单（manifest）MUST 遵循 \`rtl-emit-manifest-v1\` schema` |
| **新标题（逐字 · ADDED）** | `端口清单（manifest）MUST 遵循 \`rtl-emit-manifest-v2\` schema，且 MUST 携带产品端口表` |
| **改法要点** | ① 条目 10 字段**不变**（`ports[]` 仍列全量）② **新增顶层数组** `port_table: [{name, dir, width, role}]`（**按产物顺序** · 与产物的端口表**逐字逐序**）③ **端口面的唯一权威 = `port_table[]`** ④ 改写「`is_top_io` 的筛选是消费方的选择」场景 ⇒ ⇒ 「**发射器 MUST 按 `port_table[]` 产端口面**；`is_top_io` **只是**消费方的筛选信号，MUST NOT 被用作端口面的定义」⑤ schema 名 `v1` → `v2` ⑥ 🔴 **`spec.md:64-69` 的 Scenario「manifest 不定义端口面与命名规则」MUST 显式给出解读**（**critic v2 第 1 轮 BLOCKER-2(d)**）：该条约束的是「**产 manifest 这一动作 MUST NOT 改变产物**」（生成副作用）⇒ ⇒ **不是**「manifest 的字段不能描述端口面」⇒ ⇒ MUST 在该 Scenario 内补一句「本 Scenario 约束的是**产物不变**；`port_table[]` 是**对产物的描述**，不构成对产物的定义」 |
| ⚠️ 附带 | `rtl-emit-portability/spec.md:4` 的 `## Purpose` 是**占位符**（`TBD - created by archiving …`）⇒ ⇒ delta 的 Purpose 不合并 ⇒ ⇒ **归档后手工写**（不是 delta 的事，但 MUST 登记） |

#### R-2 · `rtl-hierarchical-output` · `层级粒度 MUST 由显式声明决定（默认 inline）`（**MODIFIED** · 标题不变）

**改法要点（4 处）**：

1. 正文「该祖先 = RTL 顶层 ⇒ 照扁平规则（无 driver 的 `ch_in` ⇒ 顶层 `input`；**任意** `ch_out` ⇒ 顶层 `output`）」
   ⇒ ⇒ 改为「该祖先 = RTL 顶层 ⇒ **顶层端口面契约**（`default_clock` → `default_reset` → **根组件自有 `__io`**：无 driver 的 `ch_in` ⇒ 顶层 `input`；**根自有的** `ch_out` ⇒ 顶层 `output`；**后代 `ch_out` MUST NOT 上顶层**（降为体内 `logic`））」。
2. 「**端口表收缩（无人消费的子输出不上 RTL 顶）MUST 只发生在成层子树**」
   ⇒ ⇒ 改为「端口表收缩发生在**两处**：① **顶层端口面**（由 D1 契约定 · **与粒度无关**）② 成层子树的非根 module（由本条定）」。
3. Scenario「未声明的组件 MUST 被 inline」的「祖先 = RTL 顶层 ⇒ 照扁平规则（任意 `ch_out` ⇒ 顶层 `output`，**包括已被父层消费的** ⇒ MUST NOT 降成纯内部 wire）」
   ⇒ ⇒ 改为「祖先 = RTL 顶层 ⇒ **仅根组件自有的 `ch_out`** 成为顶层 `output`；**后代** `ch_out`（**含已被父层消费的**）MUST 降为体内 `logic`」。
4. 非根成层 module 那一格（`ch_out` 是体内信号）**不变**。

#### R-3 · `rtl-hierarchical-output` · `不属于任何组件的节点 MUST 归顶层 module`（**MODIFIED** · 标题中性 · **保留**）

**改法要点**：正文**增补两类区分**（标题不变 ⇒ 不需 R+A）：

| 子类 | 条款 |
|---|---|
| `default_clock` / `default_reset` | MUST 是**顶层端口** · MUST NOT 触发 `CHWARN`（既有 Scenario 已要求 · **保留**） |
| 其余 `owner == nullptr`（**用户直接创建**） | **归顶层 module 的体内**（**MUST NOT** 是顶层端口）· **MUST 保留 `CHWARN` 点名** · **MUST NOT 被拒绝** |

⇒ ⇒ 既有 Scenario「含寄存器的设计 MUST 仍能层次化」**保持**（clock/reset MUST 是顶层端口 ⇒ 不冲突）。

#### R-4 · `rtl-hierarchical-output` · `悬空子输入 MUST 向上冒泡为顶层输入`（**REMOVED + ADDED** · 标题会变假）

| 项 | 内容 |
|---|---|
| **旧标题（逐字）** | `悬空子输入 MUST 向上冒泡为顶层输入` |
| **新标题（逐字 · ADDED）** | `悬空后代子输入 MUST 被响亮拒绝，MUST NOT 静默降格为顶层输入或体内未驱动 wire` |
| **改法要点** | ① **根自己**的悬空 `ch_in` ⇒ **仍 MUST 是顶层端口** ② **后代**（含 **inline 折叠进根**）的悬空 `ch_in` ⇒ 发射 MUST **返回 `ok = false`** + `error` 点名（端口名 + module + 层级）+ `files` 为空 ③ **MUST NOT**：静默降格为顶层端口 / 体内未驱动 wire / tie-off ④ **`UndrivenPolicy::Bubble` MUST 退役**（见 **D1-h** · 时点 = **本 change 同批**）⇒ ⇒ ⚠️ **本条正文 MUST NOT 留「枚举是否退役是可选项」的余地**（critic v2 第 1 轮 BLOCKER-1 的闭合） |

#### R-5 · `rtl-hierarchical-output` · `成层且非根的 module 端口 MUST 覆盖自身 \`ch_in\` / \`ch_out\`，且每个端口可追溯`（**MODIFIED** · 标题不变）

**改法要点（**4 处** · critic v2 第 1 轮 BLOCKER-2(a) 补第 4 处）**：

1. 端口来源「五类」⇒ ⇒ **三类**（**v2-oracle 第 1 轮 SHOULD_FIX-2 的闭合**）：
   - 「inline 后代的 `ch_out`（**仅根允许**）」⇒ **删除**（D1 下**任何 module** 都**不**把后代 `ch_out` 当端口）
   - 「**冒泡端口**」⇒ **删除**（D1-f 让后代悬空 `ch_in` 恒被拒绝 ⇒ 冒泡端口**零生产** ⇒ 见 D1-h）
   - **保留**：自身端口 · 隐式端口 · 时序基础设施
2. 「**RTL 顶层 module MUST 走扁平规则**，MUST NOT 套用本条」⇒ ⇒ 改为「**RTL 顶层 module MUST 走 D1 的顶层端口面契约**（框架 clock/reset + 根自有 `__io`），MUST NOT 套用本条」+ **补**「根的**自有 IO 段**（即 `__io` 声明的那些端口 · **不含**框架 `default_clock` / `default_reset`）MUST **==** 自身 `ch_in` ∪ `ch_out`」（**==** 而非 **⊇** ⇒ ⇒ 故仍不套用本条；⚠️ 限定「自有 IO 段」是为了与 **D1-d** 的「34 = 2 + 32」口径**逐字对齐**）。
3. `PortProvenanceOrigin` 的 **`Bubbled` 值 MUST 退役**（D1-h）+ `InlineFolded` **同理**（D1 下不产生端口）⇒ ⇒ ⇒ 顶层**只允许** `Self` + `ClockReset` ⇒ ⇒ **这同时回答了 `:324-325` 的排他句**（其「层次化顶层 MUST 含**全部** inline 后代的 `ch_out`」半句**随之删除**）。
4. 🔴 **本 Requirement 的 Scenario `:336-344` 末句 MUST 同步改**（**critic v2 第 1 轮 BLOCKER-2(a)**）：原文「⚠️ **子 input 端口 MUST NOT 用空连接糊过去 ⇒ ⇒ 未驱动的子 input MUST 冒泡成顶层 input** 并由父层接上」⇒ ⇒ **改为**「未驱动的**后代**子 input MUST **依 R-4 被响亮拒绝**（`ok = false` + 点名）⇒ ⇒ MUST NOT 冒泡成顶层 input」。
   ⇒ ⇒ ⚠️ **漏这一处的后果**：MODIFIED 是**整条替换**（含全部 Scenario）⇒ ⇒ 漏改 ⇒ ⇒ 归档后主 spec 的 `:344` 仍强制「未驱动子 input MUST 冒泡」⇒ ⇒ 与新的 R-4 **逐字相反**。
   ⇒ ⇒ ⚠️ **builder MUST 逐条核对 `:240` 的**全部 Scenario**（不是只改本节列的 4 处）** —— 见 §2.8 末的「MODIFIED 的完整性义务」。

#### ⚠️ **撤回 R-6**（v2-oracle 第 1 轮 **BLOCKER-1** 的闭合）：`:637` **不需 delta**

> v2 初稿写「**D1 下顶层不再有 C 类**」⇒ ⇒ **事实错误**：冒泡端口只是顶层 C 类的**一个来源**；另一个来源 —— **根组件自有的 legacy 未具名端口** —— 在 D1 下**仍是顶层端口**。

| 证据 | 内容 |
|---|---|
| `examples/axi4/axi4_lite_example.cpp:20-31` | 根 `AxiLiteTop` 的 `__io` 全部为 **legacy 未具名声明**（`ch_in<ch_uint<32>> awaddr;` 形态 ⇒ **无具名构造串**）⇒ ⇒ 20 个端口全部是 **C 类** |
| `.rddf/checks/port-naming-exempt.tsv:36-55` | 这 20 个端口全部登记为「既有债·基线豁免」⇒ ⇒ 按 `:651-655` 的三类判定 ⇒ **C 类** |
| `.rddf/checks/examples-rtl-sim-targets.tsv:40` | `axi4_lite_example` = **首批 10 个登记产品目标之一**（`status=verified`） |

⇒ ⇒ **裁决：`:637` 的三类判定与「冒泡端口是否存在」正交** ⇒ ⇒ **不需 delta** ⇒ ⇒ 与 `:478` / `:518` / `:529` 同列「不需 delta」。
⇒ ⇒ ⚠️ 且 `:637` 自己已写「下游工具 MUST NOT 依赖『Verilog 端口名 == C++ 成员名』这一**当前不成立**的假设」⇒ ⇒ **C 类在顶层继续存在是既有事实，不是 D1 造成的**。

#### R-6（**v2 新增 · critic v2 第 1 轮 BLOCKER-2(b)**）· `rtl-hierarchical-output` · `跨层端口策略 MUST 是单一权威的线性枚举，且各取值的失败语义 MUST 明确`（**MODIFIED** · 标题不变）

| 项 | 内容 |
|---|---|
| **旧标题（逐字）** | `跨层端口策略 MUST 是单一权威的线性枚举，且各取值的失败语义 MUST 明确`（主 spec `:402`） |
| **改法要点（1 处）** | 正文 `:408` 的「悬空输入的策略 MUST 由 `UndrivenPolicy`（**`Bubble` / `Reject`**）承载」⇒ ⇒ **改为**「悬空输入的策略 MUST 由 `UndrivenPolicy`（**仅 `Reject`**）承载 ⇒ ⇒ **`Bubble` 已按 ADR-072 D1-h 退役**」 |
| ⚠️ 为何**必须**有这一条 | D1-h **物理删除** `Bubble` 枚举值 ⇒ ⇒ 若 `:408` 不改 ⇒ ⇒ 归档后主 spec **指称一个不存在的符号** ⇒ ⇒ **冻错契约** |

#### ⚠️ **Purpose 的两处落点**（delta **不合并 Purpose** ⇒ ⇒ 归档后 MUST 手工写）

| capability | Purpose 现状 | 归档后 MUST |
|---|---|---|
| `rtl-emit-portability` | `TBD - created by archiving change …`（**占位符**） | 写**实质** Purpose |
| `rtl-hierarchical-output` | `:12-13`「悬空子输入 MUST 向上**冒泡**为顶层输入」⇒ ⇒ **与新 R-4 方向相反** | 改写为「未驱动的**后代**子输入 MUST 被**响亮拒绝**」 |

#### ⚠️ **MODIFIED 的完整性义务**（**critic v2 第 1 轮 BLOCKER-2(a) 的通用化**）

> **`## MODIFIED Requirements` 是整条替换**（含**全部** Scenario）⇒ ⇒ `builder` **MUST 逐条核对**目标 Requirement 的**全部** Scenario，而不是只改本节列出的「改法要点」。
> ⇒ ⇒ **必查清单**：`rtl-hierarchical-output` 的 `:240` 有 **8 个 Scenario**（`:264` / `:277` / `:286` / `:300` / `:311` / `:321` / `:336` 等）⇒ ⇒ 其中 `:336-344` 的末句含「未驱动子 input MUST 冒泡」⇒ ⇒ **必须改**（否则归档后与新 R-4 相反）。
> ⇒ ⇒ **验收**：`scripts/verify_openspec_delta.sh` 的 R3 只查**标题**逐字 ⇒ ⇒ **抓不到正文漏改** ⇒ ⇒ 故 MUST 用**人工逐 Scenario 对照**（本 ADR 已给必查清单）。
> ⇒ ⇒ ⚠️ **V6 负向判据的 grep 范围裁定**（**critic v2 第 2 轮 SHOULD_FIX 的闭合**）：
> **清零范围 = 6 条 Requirement 的正文与 Scenario**；**Purpose 不在该范围内**（delta 不合并 Purpose ⇒ 由 §6 ② / ⑨ 的「归档后手工写」单独处置）。
> ⇒ ⇒ 主 spec 现有 8 处「冒泡 / `Bubble` / `Bubbled`」命中（`:13` / `:190` / `:196` / `:197` / `:252` / `:327` / `:344` / `:408`）中，**仅 `:13`（Purpose）走 Purpose 通道**，其余 7 处 MUST 被 6 条 delta 清零。
> ⇒ ⇒ verifier 执行 V6 时 MUST **显式记录**该范围与 `:13` 的处置（防口径漂移）。

#### ⚠️ **不需 delta 的四条**（v2 裁决 · 防过度扩张）

| Requirement | 为何**不**需要 delta | 但 MUST 补的 |
|---|---|---|
| `层次化 MUST 有独立的公开入口，既有路径 MUST 逐字节不变`（`:478`） | 该条的「**既有路径**」在原文**直指「既有 `toVerilog()`」的两个 ctx 形参重载**（`:478-482` + Scenario `:492-496` 把「签名不变 + 产物逐字节不变」钉在该入口上）⇒ ⇒ D1-b 用**新增**入口 ⇒ ⇒ **契约天然成立**（v2-oracle Q6 独立核实） | 在本 ADR **钉死定义**（date 注记）· **不改 spec** |
| `默认（全 inline）产物 MUST 与扁平产物逐字节一致`（`:518`） | 「扁平产物」**两个入口都成立**（ctx-only 侧由既有测试覆盖；带根侧同样 34 端口） | **新增带根侧的等价比对判据**（不是改条款） |
| `层次化产物 MUST 与扁平产物等价（可机械判定）`（`:529`） | 同上 | **新增带根侧的等价判据** |
| `顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定`（`:637`） | 三类判定与「冒泡端口是否存在」**正交**（**v2-oracle BLOCKER-1** ⇒ 见上） | **无**（C 类在顶层继续存在 = 既有事实） |

⇒ ⇒ ⇒ **delta 总范围 = 2 份 capability · **6 条** Requirement** = **4 MODIFIED**（`:32` · `:98` · `:240` · **`:402`**）+ **2 REMOVED+ADDED**（`rtl-emit-portability` 的 manifest schema · `:190`）。
⇒ ⇒ ⚠️ **条数从 5 变为 6 的原因** = `:402` 的正文指称 `UndrivenPolicy`（`Bubble` / `Reject`）⇒ ⇒ D1-h 删 `Bubble` 后**必须同步**（**critic v2 第 1 轮 BLOCKER-2(b)**）⇒ ⇒ 这是**裁决内容**，不是 builder 的自由发挥空间。

---

## 3. 选项对比表

| # | 选项 | 做法 | **结论** | 理由（**真的更差**，不是「不如推荐项」） |
|---|---|---|---|---|
| **①** | **端口面：直接收敛到根组件自有 `__io`** | 改端口选择规则 + manifest + TB + 护栏 + capability | ✅ **选中** | **唯一**能让 manifest 与产物**自洽**的形态（事实 ⑧：98 个顶层端口错标）⇒ ⇒ 让「端口名 == C++ 成员名」在**顶层**成立 ⇒ ⇒ 且**收敛到 ADR-046 D10 冻结的那 32 个** = **对齐既有冻结** |
| ② | 维持现状（残差 id=23 的 2026-11-11 裁决） | 顶层 130 / 68 照旧 | ❌ **拒**（**用户 2026-11-16 已推翻**） | manifest 与产物不自洽（98 错标）· 冒泡端口编号随 eval-list 序**漂移** ⇒ 名字不稳定 · 与 5 条成文契约**逐字相反** |
| ③ | 双档（默认零变化 + 产品档收敛） | 加开关 | ❌ **拒**（用户裁决「直接收敛」） | 长期维护**两个交付形态** ⇒ 两套期望集 ⇒ 且「默认档」变成没人用的遗留路径 —— ⚠️ **注意**：D1-b 的 ctx-only **不是**这个选项（它是**非产品**路径，不是第二档**产品**） |
| ④ | 只加 manifest 字段（不收敛产物） | `port_table[]` | 🟡 **退路** | 能修 manifest 不自洽（G1 的契约面一半）⇒ ⇒ 但顶层仍有 96 个无语义端口 ⇒ ⇒ 「CPU 核 RTL」交付面**不干净** |
| ⑤ | ISA：把特权级纳入本轮 | 顺手做 CSR / trap | ❌ **拒** | `AGENTS.md` KNOWN LIMITATIONS **要求独立立项** + draft **已存在** ⇒ ⇒ 两份权威 + 绕过立项门；且它与 G1/G3 **零写点交集** ⇒ ⇒ 工程收益为零 |
| ⑥ | ISA：不在产品面声明限制 | 只在文档角落提 | ❌ **拒** | 「冻结是终态」是**运行时可观测的终态** ⇒ 不声明 = **误导性宣传** |
| ⑦ | 覆盖：锚点钉 28 | 护栏要求 28 全有 TB | ❌ **拒**（用户裁决「维持首批 10」） | 等于把 19 个人工 TB 变成**护栏红** |
| ⑧ | 覆盖：维持现状且不管漂移 | 只修数字 | 🟡 **不足** | 一次性 ⇒ 下次再长照旧漂 ⇒ ⇒ 必须**有判据**（D5） |
| ⑨ | 卫生：保持 `emit_hygiene` off | 不动 | ❌ **拒** | 「默认 off」的唯一理由已被 D1 消灭 ⇒ 继续 off = 产品面带 blanket `lint_off` 出货 |
| **⑩** | **（v2 新增）backend 跟随产品面** | 新增「root 进 backend / Simulator」设计点（或**在 backend 内启发式推导 root**） | ❌ **拒** | ① 若走启发式推导 ⇒ **三类 `nullptr` 形态导致静默误分类**（事实 ⑩）⇒ 违背本仓 fail-closed 纪律 ② 若走显式传 root ⇒ 要改 `IEvalBackend::initialize` 的签名 ⇒ 推翻 **ADR-058 §2.3 决策③「既有入口一律不动」** ⇒ 需**第二个 ADR addendum** ③ 收益 = 仿真器能跑产品产物 —— 而**独立 Verilog TB 已经能跑**；且**显式路径已存在**（`try_initialize_with_verilog` + `backend_port_set`，调用方本来就持有 root）—— ⚠️ **但该「已有路径」只对**层次化**产物成立**（`backend_port_set` 的唯一重载收 `HierarchyEmitResult`，`include/hierarchy_verilog.h:154`；**扁平**产物的 `toVerilog` 返回 `void` ⇒ 无对应入口 —— 事实 ⑪）⇒ ⇒ 扁平面「收益为零」由另一条独立支撑：**auto-mode 本就跑扁平遗留产物**（= `verilator_e2e` 全套）⇒ **收益为零、代价是公开面 + ADR 变更** |
| **⑪** | **（v2 新增）ctx-only 路径全部迁移到带根** | 改 49 处调用点 | ❌ **拒** | 爆炸半径远超 D1 本身（49 处 · `examples/` 21 · `tests/` 18 · `samples/` 5 · `src/` 5）⇒ ⇒ 且与「产品面收敛」目标**不冲突**（产品面 = `rtl_emit` 路径）⇒ ⇒ **保留 + 登记** 即可 |
| **⑫** | **（v2 新增）后代悬空 `ch_in` 静默降格**（wire / tie-off） | 不上顶层但保留信号 | ❌ **拒** | 两者**都静默**（读 X / 静默改行为）⇒ ⇒ 与仓内「响亮拒绝」纪律相反；且 `verilator --lint-only` 对未驱动 wire 的行为不可依 |
| **⑬** | **（v2 新增）`port_table[]` 取代 `ports[]`** | `ports[]` 只列产品端口 | ❌ **拒** | `tests/test_rtl_emit_contract.cpp:278` 的 `top_ports < man.ports.size()` 恒红；且 `ports[]` 的**节点清单**用途（`owner` / `driven` / `effective` 诊断）会丢失 |

---

## 4. 后果 / 代价

### 4.1 正面

- ✅ 「功能完整的 CPU 核 RTL」从**能力描述**变成**可交付契约**（D1+D2+D3+D7）。
- ✅ manifest 与产物**自洽**（事实 ⑧ 关闭）⇒ ⇒ 下游可**机器消费**。
- ✅ 顶层端口名 == C++ 成员名 + 前缀在**顶层**成立（冒泡端口消失 ⇒ 无反例）。
- ✅ 覆盖欠账从「静默」变「可审计」（D5 · 已拆独立 change）。
- ✅ **delta 范围从 6/7 条缩到 5 条**（`:478`/`:518`/`:529` 不需 delta）。

### 4.2 负面（**代价**）

| # | 代价 | 缓解 |
|---|---|---|
| ① | 🔴 **12 项 MUST 同批**（§2.1.10 · 含 ④′ 冒泡机制退役） | **原子落地**（拆批 ⇒ 中间态「声称成立但不成立」） |
| ② | ⚠️ **产品产物（34）≠ 仿真器产物（130）** ⇒ ⇒ 下游可能困惑 | **显式声明**（D1-a + `## 用户可见变更`）+ 产品产物的验证载体 = 独立 TB |
| ③ | ⚠️ **ctx-only 49 处调用点**保留为**非产品遗留路径** ⇒ ⇒ 仓内长期存在两种扁平规则 | **显式标注**（D1-b）+ 文档写明「哪条入口适用哪条规则」（同批必改 ⑪） |
| ④ | ⚠️ **改默认产物** ⇒ ⇒ 依赖「顶层 130 / 68」的快照全失效 | 一次性重算 · **禁**写死数字（判据 = 基线 + 新增 − 删除 == 实测） |
| ⑤ | ⚠️ **`axi4_lite` 的 S8 场景整体消失**（8 条判据） | tsv 同批重登记（45 → 37 · 8 → 7）；该场景测的是**冒泡机制本身** ⇒ 该机制不存在 ⇒ **不是判别力损失** |
| ⑥ | ⚠️ **观测盲区从层次化面扩展到默认产物面**（ADR-070 风险③ 同型） | **已接受**：根自有 32 个 `obs_*`/`dbg_*` 一个不少（V4 承重）⇒ 但 MUST 在 capability 里**显式登记**该盲区 |
| ⑦ | ⚠️ **19 个 example 的 TB 是人工工作** | **明写进产品限制**（D2 的四件套按首批 10 计） |
| ⑧ | ⚠️ **D7 带进 `default_nettype none`** ⇒ ⇒ 下游若依赖 blanket `lint_off` 掩盖的宽度告警 ⇒ **新暴露** | 这是**暴露真问题**；同批把 10 个 TB 的 lint 跑一遍 |
| ⑨ | ⚠️ **两份 Accepted ADR 补 dated addendum = 决策内容变更** | 走复审（仓内先例 = ADR-060 v1.20 / ADR-062 v1.7） |
| ⑩ | 🔴 **「inline 折叠进根」的悬空 `ch_in` 从「放行」变「拒绝」= 行为回归**（今天能发射的设计，D1 后发射失败） | **诚实性收益**（那种设计的输入本就**无来源** ⇒ 作为独立产品不成立）+ 诊断 MUST 点明「无驱动源且不属于根 `__io` ⇒ 请驱动它或上提到根 `__io`」+ 既有测试 `:2181-2192` 重设计（V18） |
| ⑪ | ⚠️ **`UndrivenPolicy` 的默认从 `Bubble` 变 `Reject`**（D1-h）⇒ ⇒ 下游依赖「默认冒泡」的调用方行为变化 | **CHANGELOG 登记** + 公开面「行为变更」节 |

### 4.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| ① | ⚠️ **扁平路径需要「谁是根」** ⇒ ⇒ 产品路径**已持有**（事实 ⑫）但 ctx-only 路径**无根** | D1-b（**只增不改**）+ 处置**写死**（保留 + 登记）⇒ ⇒ **不许**硬改公开 API 而不登记 |
| ② | ⚠️ **降顶层端口 ⇒ 真实依赖消失** | **根自有 32 个一个不能少** ⇒ V4 承重 + **负向对照**（删一个 `dbg_*` ⇒ 必红） |
| ③ | ⚠️ **三处顶层判定口径不一** ⇒ ⇒ D1 后若不同步 ⇒ `tb_stub` 把 124 output 连到 34 端口 DUT | **D1-e**（唯一来源）+ V13（逐项相等 + 负向对照） |
| ④ | ⚠️ **D5 的「独立实现第二份解析」会制造第二份漂移面** | **单一共享解析函数** + 空解析 fail-closed + 投毒自证 |
| ⑤ | ⚠️ **D6 是流程契约** ⇒ 无载体则退化成口号 | **V15**：无可执行载体 ⇒ **降级为文档约定并如实登记** |
| ⑥ | ⚠️ **`UndrivenPolicy::Bubble` 对后代悬空 `ch_in` 失效** ⇒ ⇒ 既有 `bubble` 语料会红 | 同批改 `verify_hierarchical_rtl.sh:501-520` 的期望集（它**就是为测冒泡**） |

---

## 5. Verification（**前瞻**验收判据 · 未跑不勾 · **V1–V19**）

> ⚠️ 期望值 MUST **手推**（禁抄仿真输出）⇒ ⇒ 「两后端 / 两份同源产物一致」**≠** 正确性证据。
> ⚠️ 每条 MUST 配**变异自证**。⚠️ **禁裸数字**：计数一律从产物 / 清单派生。
> ⚠️ **处置**：`保留` / `修订` / **`撤回`** / `新增` ⇒ ⇒ **撤回的条目不得再被引用**。

### 5.1 D1（端口面 · **承重**）

- [ ] **V1**（修订 · 原「残余探针」已判定）🔴 **ctx-only 路径处置落盘**：(a) 保留旧行为 + 登记为「非产品遗留路径」**已落** ⇒ ⇒ 且 **CLI 已切到带根入口**（**否则 D1 不生效**）⇒ ⇒ **MUST NOT** 自行改选「迁移 49 处」
- [ ] **V2**（保留）🔴 **端口表判据**：扁平产物顶层端口表 **== {clock, reset} ∪ 根自有 32**（**逐字逐序** · 计数 = `2 + 根自有 IO 数`）⇒ ⇒ 且**冒泡端口零出现**
- [ ] **V3**（修订）🔴 **manifest 自洽**：`port_table[]` == 产物端口表（**逐字逐序**）⇒ ⇒ `ports[]` **仍列全量**（`top_ports < man.ports.size()` 仍成立）⇒ ⇒ schema == `rtl-emit-manifest-v2`
- [ ] **V4**（保留）🔴 **观测不退化**：根自有 **28 个 `obs_*` / `dbg_*`** 全部仍在顶层 ⇒ ⇒ `riscv` TB 仍 `checks=24 fails=0` ⇒ ⇒ **负向对照**：删一个 `dbg_*` ⇒ 必红
- [ ] **V5**（**撤回**）~~backend 同源（77 个 `verilator_e2e` 全绿）~~ ⇒ ⇒ **理由 = 事实 ⑩（auto-mode 未被交付 root · 从 ctx 推导不可靠）⇒ 判据在「backend 跟随产品面」这一前提下不可满足**
- [ ] **V5′**（新增 · 替代 V5）🔴 **A 类零扰动**：**ctx-only 遗留路径产物逐字节不变**（= 130 端口 · 与 v1 前一致）⇒ ⇒ 且 `verilator_e2e` 全套**仍绿**（因为它们走 ctx-only）⇒ ⇒ **负向对照**：把 backend 端口表改回「带根」⇒ ⇒ 必红（编译期失败）
- [ ] **V6**（修订）🔴 **契约逐句清理（**6 条** Requirement · 见 §2.8）**：① `rtl-emit-portability` 的 v1 标题**已删**、v2 标题**已加**、`port_table[]` 已入、`:64-69` 的解读已补 ② `:32` 的 4 处已改 ③ `:98` 的两类区分已入 ④ `:190` 的旧标题**已删**、新标题**已加** ⑤ `:240` 的 **4 处**已改（含 五类 → 三类 + **`:336-344` 的末句**）⑥ **`:402`** 的 `Bubble` 已删⇒ ⇒ **正面判据**：新契约文本**可 grep 到** ⇒ ⇒ **负向判据**：旧措辞**零残留** ⇒ ⇒ ⚠️ **`：637` 不在本判据内**（不需 delta · v2-oracle BLOCKER-1）
- [ ] **V18**（新增 · v2-oracle BLOCKER-2）🔴 **「inline 折叠进根」的悬空 `ch_in` MUST 被拒**：构造 `RootUndrivenTop`（inline 子组件有未驱动 `din`）⇒ ⇒ `ok == false` + `error` 点名 ⇒ ⇒ ⚠️ **该用例今天断言 `REQUIRE(r.ok)`**（`tests/test_hierarchy_verilog.cpp:2181-2192` + 注释「拒绝它们 = 拒绝合法设计」）⇒ ⇒ **MUST 重设计为验证拒绝**
- [ ] **V19**（新增 · v2-oracle SHOULD_FIX-2 · **critic v2 第 1 轮补 `BubbledInput`**）🔴 **冒泡机制退役（D1-h）**：`UndrivenPolicy::Bubble` / `HierarchyPortOrigin::Bubbled` / `PortProvenanceOrigin::Bubbled` / `ensure_bubbled_port` / **`BubbledInput`** / **`dangling` 收集** **零残留**（`grep -rn` 跨 `src/` `include/` `tests/` `scripts/` == 0）+ 公开面移除登记已落 ⇒ ⇒ ⚠️ **`tests/test_component_granularity.cpp:248-254` 的 `static_assert` 已同步**（不改 ⇒ 编译失败）· ⚠️ **删后 MUST 全量重建**（禁只重建被点名 target —— 旧二进制假绿）
- [ ] **V7**（保留）确定性：两次发射逐字节相同
- [ ] **V8**（保留）lint：收敛后 `verilator --lint-only` **exit 0** · 零 `%Error`
- [ ] **V9**（保留）🔴 **变异自证**：端口规则改回「全部 `ch_out`」⇒ ⇒ **V2 + V3 必红**
- [ ] **V16**（新增）🔴 **无归属节点处置（D1-g）**：用户直接创建的 `owner==nullptr` 节点 ⇒ **不上顶层端口** + **`CHWARN` 已触发** + **发射不失败** ⇒ ⇒ **三面各一条断言** ⇒ ⇒ **负向对照**：删掉 `CHWARN` ⇒ 必红
- [ ] **V17**（新增）🔴 **后代悬空 `ch_in` 的 fail-closed（D1-f）**：构造一个后代悬空 `ch_in` ⇒ ⇒ `ok == false` + `error` 点名（端口 + module + 层级）+ `files` 为空 ⇒ ⇒ **负向对照**：让发射放行（降格为 wire）⇒ 必红

### 5.2 D2 / D3 / D4 / D7

- [ ] **V10**（修订）🔴 **四件套有判别力**：`verify_examples_rtl_sim.sh` 的**新增判据④**逐个产品单元断言 manifest 的 `port_table[]` == 该单元产物端口表 ⇒ ⇒ **该判据今天恒红**（32 vs 130）⇒ 实施后转绿 ⇒ ⇒ **负向对照**：删某产品单元的 manifest ⇒ 必红
- [ ] **V11**（修订）**ISA 边界可见**：**精确串**可 grep（「不含特权级」+「冻结是终态」）
- [ ] **V12**（修订）**卫生默认档**：**CLI 默认产物**满足 A 类 6 条 + B 类 3 条 ⇒ ⇒ `verify_flat_rtl_emit.sh` 的 **F1–F6 + F8 逐条**在新靶子上仍绿 ⇒ ⇒ **变异自证**：去掉 `default_nettype none` ⇒ 必红
- [ ] **V13**（新增）**库层未被误伤**：`STATIC_REQUIRE(ch::EmitOptions{}.emit_hygiene == false)` + `git diff` 证明 `sim_main` prelude 与 `emit_sim_main_postlude` 文本未变
- [ ] **V13′**（新增 · 原「三处口径一致」升为独立条目）🔴 **唯一来源（D1-e）**：`tb_stub` / `test_rtl_emit_contract` / `flat_rtl_emit_check` 的产品端口集 == `port_table[]`（**逐项相等**）⇒ ⇒ **负向对照**：任一处回旧算法 ⇒ 必红

### 5.3 D5 / D6（**已移交** `rtl-productization-coverage-registration-2026-11`）

- [ ] **V14 / V15**（**移交**）登记完备性 + 漂移订正 + D6 的可执行载体 ⇒ ⇒ 判据随该 change（本 ADR 只登记移交事实）

---

## 6. 落点与后续

| # | 事项 | 归属 | 载体 |
|---|---|---|---|
| ① | **本 ADR v2 的 V1–V19 映射为 change 的 AC / task** | planner（已闭 · ⚠️ **V18/V19 为 v2-oracle 后新增 ⇒ 提案侧 MUST 补 AC**） | `openspec/changes/rtl-productization-surface-2026-11/proposal.md` 的 **AC1–AC15 + T1–T14** |
| ② | **6 条 Requirement 的 delta**（改法要点与标题见 §2.8）+ **两处 Purpose 归档后手工写** | builder | 该 change 的 `specs/` ⇒ ⇒ **MUST 照抄 §2.8**（禁自由发挥）· **MUST 逐条核对 MODIFIED 目标的全部 Scenario**（`:240` 有 8 个） |
| ③ | **两份 Accepted ADR 的 dated addendum**（`ADR-058` / `ADR-061`） | architect（需复审） | 该 change 的 T8 |
| ④ | **残差 id=23 闭口** | builder（**archive 之后**） | 该 change 的 T14 |
| ⑤ | **特权级** | planner → builder | **独立** change（`rv32i-privileged-csr-2026-11` · 已 draft） |
| ⑥ | **19 个 example 的 TB** | 用户 / planner | 独立排期（人工工作） |
| ⑦ | **诉求 ⑤ 的字面同名**（route (c)） | **用户** | 残差 id=21 已裁决「不要求」⇒ 本 ADR **不重开** ⇒ D1 落地后顶层已是「C++ 成员名 + `top_` 前缀」⇒ ⇒ **诉求 ⑤ 在顶层闭环**（= 成员名可推导 + 前缀稳定，**不是**字面同名） |
| ⑧ | **D5 / D6** | planner → builder | **独立** change `rtl-productization-coverage-registration-2026-11`（**待立项**） |
| ⑨ | **两处 capability Purpose 的归档后手工写**：`rtl-emit-portability/spec.md:4`（占位符）+ `rtl-hierarchical-output/spec.md:12-13`（「悬空子输入 MUST 向上冒泡」⇒ **与新 R-4 相反**） | builder | 归档后手工写（delta 的 Purpose 不合并）· **判据** = 归档后 `grep -n "冒泡" <该 spec>` 零命中（Purpose 除外通道已关闭） |

### 6.1 ⚠️ **移交清单（v2 定稿后 · planner / builder 侧 MUST 同步）**

> architect **无权**改这些工件（`openspec/changes/**` 与 `.rddf/improvements/**` 不在 `arch_paths` 内）⇒ ⇒ **逐项移交**。

| # | 工件 | MUST 改成什么 |
|---|---|---|
| **H-1** | change 的 proposal（🚪 门 P-0） | **P-0（ADR-072 v2）已闭** ⇒ ⇒ 该门可标记完成；**P-1（spec delta）解封** ⇒ ⇒ 可开工 |
| **H-2** | change 的 proposal（delta 条数） | **5 条 → 6 条**（新增 R-6 = `:402` 的 MODIFIED）+ **两处 Purpose 归档后手工写** ⇒ ⇒ §Capabilities 的计数与逐条列举 MUST 同步 |
| **H-3** | change 的 proposal（AC / task） | **V18 / V19 是 v2 新增 ⇒ 提案侧 MUST 补 AC**（V18 = 「inline 折叠进根」拒绝 · V19 = 冒泡机制零残留 + 公开面移除登记）⇒ ⇒ 且 **D1-h（冒泡机制退役）MUST 成为一条独立 task**（`UndrivenPolicy::Bubble` / `*Origin::Bubbled` / `ensure_bubbled_port` / **`BubbledInput`** 的删除 + `test_component_granularity.cpp:248-254` 的 `static_assert` 同步） |
| **H-4** | change 的 proposal（同批必改映射） | ADR 的同批必改已 **11 → 12 项**（新增 ④′ 冒泡机制退役）⇒ ⇒ proposal §A 的映射表 MUST 同步（原「12 项」的说法要按 **v2 的 12 项**重述） |
| **H-5** | `.rddf/improvements/**` | 若 proposal 对应的 improvement 草案里的范围/AC 引用 ADR v1 的条目号（如「V1–V15」「12 项」）⇒ ⇒ MUST 改为 **V1–V19 / v2 的 12 项** |
| **H-6** | change 的 `specs/` | delta MUST **照抄 §2.8 的 6 条** + **逐条核对 MODIFIED 目标的全部 Scenario**（`:240` 有 8 个）⇒ ⇒ ⚠️ `verify_openspec_delta.sh` 的 R3 只查**标题** ⇒ ⇒ **抓不到正文漏改** ⇒ ⇒ 必须人工对照 |

> ⚠️ **Status 说明**：v2 的 **oracle ×1 + critic ×1** 全闭后才标 Accepted ⇒ ⇒ 「把未决问题写成已决」属 `arch` 档 BLOCKER 第四类。**v2 轮已完成该闭环**（oracle 1 + critic 2 · 复审 APPROVE · 0 BLOCKER）。
> ⚠️ **v2-oracle 第 1 轮 = REJECT（3 BLOCKER）· 已修（记录见审稿文件 §6）**：① R-6 的「D1 下顶层不再有 C 类」**事实错误**（`AxiLiteTop` 的根 `__io` 有 **20 个 legacy 未具名端口**，D1 下**仍是**顶层端口 ⇒ ⇒ 它们是 C 类）⇒ ⇒ **R-6 撤回**（`:637` 其实**不需 delta**）② **「inline 折叠进根」的悬空 `ch_in` 未裁决**（现行 spec `:192-194` + Scenario `:213-214` + 测试 `:2181-2192` 都要求**放行**，而 D1 契约要求**拒绝**）⇒ ⇒ **D1-f 补裁决** ③ **Status 曾写「Accepted · v2」而 §7 不存在** ⇒ ⇒ 已退回 Draft（**该条的自证**）。
> ⚠️ **v2-critic 第 1 轮 = REJECT（2 BLOCKER）· 已修（记录见审稿文件 §7）**：① **R-4 要点④ 与 D1-h 自相矛盾**（前者留「实施期可选」⇒ 后者 MUST 同批退役）② **§2.8 覆盖不全**（`：240` 的 Scenario `:336-344` / `:402` 的 `Bubble` 指称 / capability Purpose `:12-13` / `rtl-emit-portability:64-69` 四处未被 6 条 delta 覆盖）⇒ ⇒ 补 R-5 第 4 处 + **新增 R-6**（delta 5 → **6** 条）+ Purpose 两处落点 + R-1 要点⑥ + 「MODIFIED 的完整性义务」段。**复审 = APPROVE · 0 BLOCKER**。
