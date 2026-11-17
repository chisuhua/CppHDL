# ADR-073: SoC 层的 RTL 产物形态与 SoC 层 TB 的交付契约

- **Status**: **Accepted**（2026-11-16 · architect role（build agent）· **oracle 第 1 轮 REJECT**（3 BLOCKER + 6 SHOULD_FIX）⇒ ⇒ 全闭 ·
  **critic 第 2 轮（复审）APPROVE** · **0 BLOCKER** · `mergeable: yes`）
  ⚠️ **保留的未决分支**：D1 的条件式（成层改造撞出发射器层面的第二层失败 ⇒ ⇒ 按 ADR-070 §6 ⑥ 重新裁决）仍是「显式标注的未决」
- **Date**: 2026-11-16
- **Author**: architect role（build agent）
- **Parent**: **ADR-072**（RTL 生成能力产品化 · 顶层端口面收敛 · **Accepted · v2.1**）· **ADR-070**（实例级 `ch_layer` 成层 · **Accepted**）· **ADR-058**（层次化 RTL 输出 + 显式 backend 入口 · **Accepted**）· **ADR-061**（粒度用户可控 · 默认 inline · **Accepted**）· **ADR-042**（bundle 的端口规则）· **ADR-060**（层次化端口策略）· **ADR-066**（父侧落点 · `ch_mem` 三桶裁决 · v1.2）
- **关联 change**: `openspec/changes/soc-rtl-product-and-tb-2026-11/`（**已立项** · 2026-11-16 · proposal 有 13 task + AC1–AC14）
- **AC 映射**: 本 ADR §5 的 **V1–V12** → change 的 **AC1–AC14**（逐条对照见 `.rddf/improvements/soc-rtl-product-and-tb-2026-11.md` 的「V→AC 对照」段）
- **架构主题文档**: `docs/architecture/2026-11-16-soc-rtl-product-and-tb.md`（当前态快照 + 模块关系 + **可枚举改动清单** + 方案细则）
- **审稿记录**: `.rddf/reviews/ADR-073-soc-rtl-product-and-tb.md`（oracle ×1 REJECT · critic ×2（第 1 轮 REJECT · 2 BLOCKER）⇒ ⇒ 全闭 · 第 2 轮复审 **APPROVE · 0 BLOCKER** · 全部 finding 逐条应答）
- **2026-11-17 dated 订正（v1.2 · **D1–D10 方向零变更** · 一处**承接性**指针 + 一处**事实**订正）**: 用户 2026-11-17 追问「为什么不能直接从 `*example.cpp` 生成 dut + tb」⇒ ⇒ 分析后由 **ADR-074**（验证单一真相源与生成式 TB）+ **ADR-075**（端口具名化 = 产品面前提）承接 ⇒ ⇒ 本 ADR **决策内容零变更**，只有承接指针与一处事实订正：
  - **① D3（§2.3）零变更** —— (A) 解释器 = 逻辑迭代 / (B) 仓内 backend = 逐拍 parity / **(C) 独立 Verilog TB = 交付验证** 的分工**实质不变** ⇒ ⇒ 变的只是 **(C) 的「撰写形态」**（手写 ⇒ 由受限 DSL 生成）⇒ ⇒ **(C) 仍是完全绕开 CppHDL runtime 的外部 `verilator` CLI 判据** ⇒ ⇒ **ADR-074 D7 明写「不推翻本决策」**。
  - **② D6（§2.6）零变更** —— 期望源的权威载体仍是 `.rddf/checks/soc-rtl-expectations.md`（手推 + 锚点）⇒ ⇒ 生成的 TB 头**仍**带 `EXPECT-SRC: …#soc-example` 指针 ⇒ ⇒ 护栏 ②'（**禁指向 `.cpp`**）**逐字不变** ⇒ ⇒ **反循环论证的防线不回归**。
  - **③ D4（§2.4）「顶层端口 14」的数量不变，但端口「名字」要变** —— **ADR-075 E1** 把 SoC **顶层**（`AxiSocTop` 的 `__io`，`examples/axi4/axi_soc_example.cpp:161-181`）的 **12** 个端口迁到**具名构造**（⚠️ **不是** 35：同文件的 `AxiLiteTestMaster`（`:41-72`）另有 **23** 个端口，但那是**成层后子 module 的端口**，**不属产品顶层**，本次不做）⇒ ⇒ 产物端口名 `top_unnamed_input_N` ⇒ ⇒ `top_<成员名>` ⇒ ⇒ **数量仍 = 框架 2 + 根自有 12 = 14** ⇒ ⇒ **D4 的契约面（数量与口径）不变**，变的是**可自解释性**（**产品可见变更** ⇒ ⇒ MUST 记 CHANGELOG）。
  - **④ 事实订正** —— 本 ADR §2.3 提到的「发射入口」形态：库层 `run_emit_cli` **已于 2026-11-17 支持 `--hierarchical`**（commit `2db2eda`）⇒ ⇒ `examples/axi4/emit_rtl_soc.cpp` 的「自己解析 + 自己发射」（483 行）**已可收敛为库层薄壳**（⇒ ⇒ 与 `examples/riscv-mini/emit_rtl_hier.cpp` 的 52 行形态统一）⇒ ⇒ **实施收敛**，**不需要新的 ADR**（蓝图 §4.3）。
  - ⚠️ **Status 行不动**（仍 `Accepted`）⇒ ⇒ v1.2 是**增补**⇒ ⇒ 本 ADR 的已决状态**未被改动**。
- **2026-11-16 dated 订正（v1.1 · **D1–D10 方向不变**；J-4/V6 验证判据随事实做**最小修订**）**: 由 metis 第 1 轮立项 gap 分析查出 7 处事实订正 ⇒ ⇒ ① 未驱动后代 `ch_in` **30 → 31**（补 `master.rst_n`）② 扁平顶层 **44 → 45** ③ D1 的「`AxiSocTop` = 5」⇒ 「根 module 名 `top`（默认）+ 4」④ `done` 是**粘滞**（非单拍脉冲）⇒ J-4 观测集去 `done`、激励 ≥4 → **≥10 拍** ⑤ s1↔uart **「19 根双向」→「19 根单向」** ⑥ V6 删「未映射地址 DECERR」（顶层无 `bresp` + matrix 默认映射铺满 ⇒ 不可观测）⑦ `master.rst_n` 补驱动**无行为影响**（`describe()` 不消费）。⇒ ⇒ 证据见审稿记录 §6；⚠️ **#4/#6 触及 D7/J-4、V6 的验证判据**（非纯排版）⇒ ⇒ 但**均因原判据建立在错误事实之上、从未可满足** ⇒ ⇒ 属**订正**而非新决策（**D1–D10 方向零变更**，全部由下游 change 的 AC 已按此口径起草）

---

## 1. 背景与问题

### 1.1 诉求

用户 2026-11-16 追问：「**SOC 的 Verilator 要怎么走**？我理解独立 TB 是继续 IP 层次的，那么 SoC 如何构建 IP 互联和 SOC 层的 TB？」
⇒ ⇒ 本 ADR 回答两个问题：**SoC 的 RTL 产物交付成什么形态** + **SoC 层 TB 的契约是什么**。

### 1.2 实测事实（2026-11-16 · 可复算）

| # | 事实 | 证据 |
|---|---|---|
| ① | ✅ **IP 互联机制已存在且有完整实例** —— `ch_module<T>` 实例化 + 逐信号 `<<=` 连线 | `examples/axi4/axi_soc_example.cpp:192-201`（4 个 IP：`AxiLiteTestMaster` / `AxiLiteMatrix2x2<32,32>` / `AxiLiteGpio<8>` / `AxiLiteUart<16>`）+ `:204-211` |
| ② | ✅ **SoC 顶层的端口面是扁平标量**（**12** 个，零 bundle） | `examples/axi4/axi_soc_example.cpp:161-181`（8 `ch_in` + 4 `ch_out`）⇒ ⇒ **框架 2 + 12 = 14** |
| ③ | 🔴 **SoC 的 RTL 产物从未被交付** —— `examples/axi4/emit_rtl.cpp` 的 factory 造的是 `AxiLiteTop`（AXI4-Lite 单 IP） | `examples/axi4/emit_rtl.cpp:36-40` |
| ④ | 🔴 **SoC 的 IP 边界在产物里不可见** —— 4 个 IP 都用 `ch_module`（默认 inline）⇒ ⇒ 层次化产物**全 inline 成 1 个 module** | `examples/axi4/axi_soc_example.cpp:192-201` · ADR-061 决策 · ⚠️ 4 个 IP **均无** `CH_HIERARCHY_LAYER` ⇒ ⇒ 成层须走**档 ③**（`include/module.h:171-175`） |
| ⑤ | 🔴 **SoC 有 31 个未驱动的后代 `ch_in`**（**2026-11-16 dated 订正**：原写 30，漏 `master.rst_n`）—— ① `matrix.m1_*` 的 **11** 个 `ch_in`（`include/axi4/axi4_lite_matrix.h:66-88`）全未接 ② `uart` 的 **11** 个 AXI 从机输入（`include/axi4/peripherals/axi_uart.h:54-76`；该文件 13 个 `ch_in` 里 `uart_rx` / `rst_n` **已接**）全未接 ③ `matrix.s1_*` 的 **8** 个**响应侧** `ch_in`（`s1_awready` / `s1_wready` / `s1_bresp` / `s1_bvalid` / `s1_arready` / `s1_rdata` / `s1_rresp` / `s1_rvalid`）全未接（**整个文件零 `s1_*` 连线** —— gpio 占的是 `s0_*`）④ **`master.rst_n`** 的 **1** 个（`axi_soc_example.cpp:47` 声明；连线段 `:263-267` **只**驱动 gpio/matrix/uart 的 `rst_n` ⇒ ⇒ `master.io().rst_n` **零驱动**）⇒ ⇒ **11 + 11 + 8 + 1 = 31** | ⇒ ⇒ **ADR-072 D1-f（后代悬空 ⇒ fail-closed）落地后，SoC 产物发不出来** ⇒ ⇒ 且 T-2 的 `uart_tx` 断言**不可达**（uart 不在总线上） |
| ⑥ | ✅ **仓内已有「未用端口 tie-off」的成例惯用法** —— `interconnect.io().m1_awvalid <<= ch_bool(false);` …（m1/m2/m3 五个 + s1/s2/s3 八个，共 3 个 matrix 的同类处理） | `examples/axi4/phase3a_complete.cpp:259-302` ⇒ ⇒ **不需发明新机制** ⇒ ⇒ 且该 idiom 使输入「有内部 driver」⇒ ⇒ **不成为顶层端口**（`src/codegen_verilog.cpp:432-441`）⇒ ⇒ **不冒泡** |
| ⑦ | ✅ **已有两个独立 TB** —— `riscv-mini` 的 SoC（`kind=dut` · `checks=24 fails=0`）· AXI4-Lite 的 IP（`checks=45 fails=0`） | `examples/riscv-mini/tb_rv32i_soc.sv` · `examples/axi4/tb_axi4_lite_example.sv` · `.rddf/checks/examples-rtl-sim-targets.tsv:31,40` |
| ⑧ | 🔴 **AXI SoC 的 TB 缺失**，且排除理由本身有事实漂移 | `.rddf/checks/examples-rtl-sim-exclusions.md:36`（写「AXI 矩阵 + GPIO + UART + **Timer**」⇒ ⇒ 实为 **4** 个 IP、**无 Timer**）⇒ ⇒ 漂移源头 = `examples/axi4/axi_soc_example.cpp:4` 的源码头注释 + `:24` 的**零使用** `#include "axi4/peripherals/axi_timer.h"` |
| ⑨ | ✅ **双形态逐拍等价的公共 API 已存在** —— `ch::hier_eq::emit(root, work_dir, granularity)` + `compare(ctx, work_dir, hier_path, ports, inputs, observations, stimulus)`；`ports` **MUST 由 `emit` 派生**（禁手写） | `include/hierarchy_equivalence.h:131,155` |
| ⑩ | ⚠️ **`backend_port_set` 只对层次化产物成立** —— 唯一重载收 `HierarchyEmitResult`；扁平 `toVerilog` 返回 `void` | `include/hierarchy_verilog.h:154` · `include/codegen_verilog.h:65,68` |
| ⑪ | ✅ **双形态等价不被端口面差异影响** —— A 侧 = `VerilatorBackend` 从 ctx 自生成的扁平产物（`src/hierarchy_equivalence_impl.h:258-269`），比对**只经 `inputs` / `observations` 落根端口** ⇒ ⇒ 「A 侧 130 端口 vs B 侧 34 端口」**不影响**等价判定；`port-width-64-exceeded` 不触发（SoC 最宽 32 位） | `src/hierarchy_equivalence_impl.h:258-269` · `include/hierarchy_equivalence.h:155-165`（失败前缀清单） |
| ⑫ | 🔴 **顶层 bundle 不是 Verilog 端口** ⇒ ⇒ **AXI 总线永远不在 SoC 顶层端口上** ⇒ ⇒ SoC 顶层 = 控制面 + 物理外设面的扁平标量集 | ADR-042 决策 5：「bundle 只用于**模块间**连接」·「顶层 bundle 仍**不是** Verilog 端口」（**绝对**陈述，无例外条款） |
| ⑬ | ⚠️ **`ch_mem` 的禁令已被收窄** —— ADR-066 v1.2 §2.7 决策 ⑦：禁令只针对「mem **本体**跨 module」；「**仅实参视线跨层**」是**已支持**的形态 | `docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md:241,246-256` · capability `rtl-hierarchical-output/spec.md:589`（`mem 端口实参跨层 MUST 支持且走父侧落点网`） |
| ⑭ | ⚠️ **`timer_irq` 是根自有死端口** —— `examples/axi4/axi_soc_example.cpp:180` 声明，全文件**零消费**（grep 仅 1 命中 = 声明处）⇒ ⇒ 不属于有效端口面 | ⇒ ⇒ MUST 登记为残留，**MUST NOT** 默认它是可观测面成员 |
| ⑮ | ✅ **`axi_soc_example` 是 tracked target 且有 `main()`** —— `examples/axi4/axi_soc_example.cpp:283` 有 `int main()`；`examples/CMakeLists.txt:69` 的 `CH_TRACKED_EXAMPLE_TARGETS` **含**它 | ⇒ ⇒ 「SoC 不是 example 形态」的说法**不成立** ⇒ ⇒ tsv 的 `target` 列可填 `axi_soc_example` |
| ⑯ | ✅ **兄弟边形态存在但已被覆盖** —— master↔matrix、matrix→gpio（同父子输出 → 子输入）⇒ ⇒ 命中 ADR-070 D1 的 H-A/H-B 覆盖形态（已在 `rv32i-hierarchical-example` 落地，23→0）；**「双类消费者」形态不存在**（每个子输出均单一消费者） | `examples/axi4/axi_soc_example.cpp:204-260` · `docs/adr/ADR-070-...md:59-60` |
| ⑰ | ⚠️ **SoC 自身的端口数**：框架 2 + 根自有 12 = **14**（**D2 落地后**）；**D2 落地前** = 14 + **31 冒泡**（未驱动后代 `ch_in`，事实 ⑤）= **45** ⇒ ⇒ ⚠️ **130 / 68 是 RV32I 的数字**（ADR-072 事实 ⑥）⇒ ⇒ **不是** SoC 的 | 由事实 ②⑤⑭ 复算 · **2026-11-16 dated 订正**（原写 30/44，漏 `master.rst_n`）|

### 1.3 问题

**SoC 的价值恰恰在「IP 互联」** ⇒ ⇒ 但现状里：① IP 边界在产物中**不可见**（事实 ④）⇒ ⇒ 交付的产物与扁平单 IP 产物**无结构区别** ⇒ ⇒ 「SoC 层 RTL 生成」**名义存在、实质缺失**（事实 ③）；② **SoC 本身还没接完**（事实 ⑤：**30** 个未驱动后代输入 + `s1 → uart` 零连线）⇒ ⇒ **它今天连「D1 之后能不能发射」都不成立**。

---

## 2. 决策

### 2.1 **D1** = SoC 的 RTL 产物形态 = **层次化**（每个成层 IP 一个 module）

> SoC 的交付产物 = `toVerilogHierarchical(path, soc_root)` ⇒ ⇒ module 数 == **成层 IP 数 + 1**（4 个成层 IP + 根 module = **5**）。
> ⚠️ **根 module 名 = 发射入口配置的名字（默认 `top`）**（`include/hierarchy_verilog.h:33` 的 `top_module_name = "top"`）⇒ ⇒ **MUST NOT** 读成组件类名 `AxiSocTop`（**2026-11-16 dated 订正**）。

| | 扁平 | **层次化（采用）** |
|---|---|---|
| IP 边界 | **不可见** | **可见** |
| `backend_port_set` | ❌ 无重载（事实 ⑩） | ✅ 有 |
| `hier_eq::compare` | A 侧固定扁平 | ✅ A=扁平 · B=层次化（事实 ⑨⑪） |
| 顶层端口 | 框架 2 + 根自有 12 + **31 冒泡** = 45 | **框架 2 + 根自有 12 = 14**（**前提 = D2**） |

⚠️ **成层声明 MUST 用档 ③**（`ch_layer<T>(instance, module_type_name)`）—— 4 个 IP 均无类级 `CH_HIERARCHY_LAYER`（事实 ④）⇒ ⇒ `include/module.h:171-175` 的 `requires(!has_class_layer_name<T>::value)` 强制第二个参数。
⚠️ **模块类型名 = 明文非契约**（ADR-070 §6 ⑦ 的用户裁决）⇒ ⇒ 改名不构成破坏性变更，但**结构判据（module 数）要重算** ⇒ ⇒ 登记为已知代价。
⚠️ **兄弟边已覆盖**（事实 ⑯）⇒ ⇒ 成层改造**不撞** ADR-070 未修的形态 ⇒ ⇒ 风险低于预期。

### 2.2 **D2** = SoC 层的可发射性前置 = **所有后代 `ch_in` MUST 有驱动源**（新增 · **oracle 第 1 轮 BLOCKER-1**）

> SoC 的**每一个**后代 `ch_in` MUST 有驱动源 ⇒ ⇒ 二选一：
> **① 接到上游 IP 的输出**（真互联）· **② tie-off 到常量**（`x <<= ch_bool(false)` / `ch_uint<N>(0_d)`）

- **事实 ⑤**：SoC 今天有 **31 个**未驱动后代输入（`m1_*` 11 + `uart` AXI 11 + `s1_*` 响应侧 8 + **`master.rst_n` 1**）⇒ ⇒ **ADR-072 D1-f（后代悬空 ⇒ fail-closed）落地后，SoC 产物发不出来** ⇒ ⇒ 所以这条是 SoC 方案与 ADR-072 的**衔接条款**，不是可选项。
- ⚠️ **tie-off 的机理（两侧判据键都要给）**：tie-off 使该 `ch_in` **有内部 driver** ⇒ ⇒ **发射侧**（`src/codegen_verilog.cpp:432-441`）它**不成为顶层端口**、成为内部 `logic` ⇒ ⇒ **不冒泡**；**分析侧**（`src/hierarchy_analysis.cpp:404-415`，**判据键 = `num_srcs() == 0`**）它**不进 `pending_bubbles`** ⇒ ⇒ **不是「悬空」** ⇒ ⇒ 不触发 D1-f 的 fail-closed。
- ⚠️ **惯用法已有**（事实 ⑥）⇒ ⇒ **照抄** `examples/axi4/phase3a_complete.cpp:259-302`（m1/m2/m3 未用主口 + s1/s2/s3 未用从口的 tie-off）⇒ ⇒ **不需发明新机制**。
- ⚠️ **SoC 的具体处置（裁决）**：
  | 组 | 处置 | 理由 |
  |---|---|---|
  | `matrix.m1_*`（**11** 个 `ch_in`） | **tie-off 到常量**（仿 `phase3a_complete.cpp:259-263`） | 只有 1 个 master（`master`）⇒ m1 未用 |
  | `matrix.s1_*` 的 **11** 个请求侧 `ch_out` → `uart` 的 **11** 个 AXI 从机 `ch_in` | **接通**（11 根） | ⇒ ⇒ uart 才在总线上 ⇒ ⇒ T-2 的 UART 场景才可达 |
  | `uart` 的 **8** 个 AXI 响应侧 `ch_out` → `matrix.s1_*` 的 **8** 个响应侧 `ch_in` | **接通**（8 根） | 反向通道 ⇒ ⇒ **这 19 根同时消掉 uart 的 11 与 s1 的 8** |
  | `master.rst_n`（**1** 个 `ch_in`，**此前零驱动**） | **透传**（`master.io().rst_n <<= io().rst_n`，仿 `:263-267`） | ⚠️ **无行为影响**（**2026-11-16 dated 订正**：`AxiLiteTestMaster::describe()` **不消费** `rst_n` —— 声明于 `axi_soc_example.cpp:47`，`describe()` 体（`:39-152`）零引用）⇒ ⇒ 纯**发射前置**（消掉一个未驱动后代输入）⇒ ⇒ 合计 **11 + 19 + 1 = 31** 全覆盖 |
  | `uart.irq`（`ch_out`） | **登记为残留**（无消费者；根的 `timer_irq` 是**另一个 IP 的** IRQ，语义不可混用） | 事实 ⑭ |
  | 根的 `timer_irq`（零消费） | **登记为死端口** | 事实 ⑭ |

### 2.3 **D3** = SoC 层 TB = **独立 Verilog TB + 外部 verilator CLI**；另两条路径各司其职
<!-- ⚠️ **2026-11-17 · v1.2 · 本决策零变更**：(A)/(B)/(C) 的分工与「(C) 用外部 verilator CLI、不走仓内后端」**全部保留** ⇒ ⇒ 变的只是 **(C) 的撰写形态**（手写 ⇒ 由受限 DSL 生成，见 **ADR-074 D1/D2**）⇒ ⇒ **(C) 仍是完全绕开 CppHDL runtime 的独立判据**。**若 ADR-074 被否 ⇒ 本决策原样生效。** -->

| 路径 | 定位 | 产物形态 |
|---|---|---|
| (A) 解释器 / JIT | **逻辑迭代**（改 IP 互联时最快） | 无 |
| (B) 仓内 Verilator 后端 | **逐拍 parity**（= `verilator_e2e` 那套） | **扁平遗留产物**（ADR-072 D1-a） |
| **(C) 独立 Verilog TB** | **交付验证**（SoC 层 TB） | **层次化产物**（D1） |

⇒ ⇒ (C) 用**外部 `verilator` CLI**，**不**走仓内后端 ⇒ ⇒ 理由：TB 要**自由驱动顶层扁平端口 + 采样输出 + 自带判据汇总**，那是**测试台**的职责；仓内后端是**仿真器**（端口面由 `ctx` 推导 ⇒ ⇒ ADR-072 D1-a 已裁定它是遗留路径）。
⇒ ⇒ (B) 与 (C) 的**桥** = `try_initialize_with_verilog(path, backend_port_set(emitted, "top"), dm)`（**只对层次化产物成立**，事实 ⑩）⇒ ⇒ 用途 = **仓内跑层次化 SoC 产物的逐拍等价**（J-4），**不是** TB 的替代。

### 2.4 **D4** = SoC 的 TB **按 ADR-072 D1 之后的端口面写**（框架 2 + 根自有 12 = **14**）
<!-- ⚠️ **2026-11-17 · v1.2 · 数量与口径零变更，端口「名字」要变**：**ADR-075 E1** 把 SoC **顶层**（`AxiSocTop`）的 **12** 个端口迁到具名构造（⚠️ **不是** 35：`AxiLiteTestMaster` 的 23 个是**子 module** 端口，不属产品顶层）⇒ ⇒ 产物端口名 `top_unnamed_input_N` ⇒ ⇒ `top_<成员名>` ⇒ ⇒ **数量仍 = 框架 2 + 根自有 12 = 14**（契约面不变）⇒ ⇒ 变的是**可自解释性**（**产品可见变更** ⇒ ⇒ MUST 记 CHANGELOG + 同步 TB 连线与期望源端口表）。**若 ADR-075 被否 ⇒ 本决策原样生效。** -->

⚠️ **理由 = 避免返工**：D1 落地后 31 个冒泡端口消失 ⇒ ⇒ 今天按「45 端口」写的 TB 会有 31 根连线悬空（`tb_rv32i_soc.sv` 现有 96 根 sink 就是这个代价的实证）。
⚠️ **过渡期**：今天跑 SoC 产物时，**先按 D2 处理掉那 31 个输入**（11 tie-off + 19 接线 + 1 透传）⇒ ⇒ 顶层自然收敛到 **14** ⇒ ⇒ **过渡形态与目标形态一致** ⇒ ⇒ **不需要 sink**。
⚠️ ⇒ ⇒ **J-1 的判据时点 = D2 落地后即可判**（不依赖 ADR-072 D1）⇒ ⇒ 这是本方案能**先行**的关键。

### 2.5 **D5** = SoC 层 TB 的**激励模型分三档**；本 ADR 只交付 **T-2**

| 档 | SoC 形态 | 顶层激励 | TB 要做什么 | 状态 |
|---|---|---|---|---|
| **T-1** | 自含 CPU + 固件 | `prog_we` / `prog_addr` / `prog_data` | 装载手推小程序 · 跑 · 断言写回 | ✅ 已有（`tb_rv32i_soc.sv`） |
| **T-2** | **test master 在 SoC 内部** | `start` / `write` / `addr` / `wdata` | 驱动 4 个控制信号 · 等 `done` · 断言 `rdata` / `gpio_out` / `uart_tx` | ⬅ **本 ADR 交付**（**前提 = D2 的 s1→uart 接通** ⇒ ⇒ UART 场景才可达） |
| **T-3** | CPU 经 AXI 访问外部 DDR | AXI 主口（bundle ⇒ 不上顶层，事实 ⑫） | **需要 AXI 从机 BFM** | 🔴 **独立立项**（D9） |

⚠️ **T-2 为何最省**：SoC 内部自带 master ⇒ ⇒ TB **不实现总线协议** ⇒ ⇒ 只需驱动 4 个控制信号 ⇒ ⇒ 判据来自**地址映射 + 事务序列**（D6）。
⚠️ **T-3 的真实阻塞只有两条**：① 无 BFM（事实 ⑨ 的缺口）② **顶层 AXI 展平策略未定义**（事实 ⑫）⇒ ⇒ ⚠️ **`ch_mem` 禁令**（曾经的第三条理由）**已不成立**（事实 ⑬：禁令只管「mem 本体跨 module」）⇒ ⇒ 不得传递该错误信号。

### 2.6 **D6** = SoC 的**期望源** MUST 是仓内可审计的登记；TB 头注释只作**指针**

> 载体 = `.rddf/checks/soc-rtl-expectations.md`（每 SoC 一节：顶层端口映射表 + 地址映射表 + 事务序列 + **手推结果**）
> ⇒ ⇒ TB 文件头**必须**指向它（照 `tb_rv32i_soc.sv:3` 的 `EXPECT-SRC:` 形态）。

⚠️ **理由**：ADR-072 D4 要求期望值**手推**（禁抄仿真输出）⇒ ⇒ 「手推结果」必须有权威载体 ⇒ ⇒ TB 里的注释**不是**载体（它会与实现一起漂）。
⚠️ **SoC 级与 IP 级的差别**：IP 级 = 「一个端口的行为」；**SoC 级 = 「地址映射 + 事务序列」** ⇒ ⇒ 天然表驱动。

### 2.7 **D7** = SoC 层的产品级判据 = **J-1 … J-5**

| # | 判据 | 载体 |
|---|---|---|
| **J-1** | **顶层端口面** = 框架 2 + 根自有 12 = **14**（**逐字逐序**）⇒ ⇒ **零冒泡端口**（`top_master_*` / `top_matrix_*` / `top_gpio_*` / `top_uart_*` 零命中） | manifest 的 `port_table[]`（ADR-072 D1-c）vs 产物端口表 |
| **J-2** | **IP 边界可见** = 产物 module 数 == 成层 IP 数 + 1 == **5** | `scripts/lib/verilog_tree_extract.py` |
| **J-3** | **lint 干净**：`verilator --lint-only --top-module top` **exit 0** · 零 `%Error` · **禁** `-Wall` / `-Wno-fatal` | 照 `scripts/verify_hierarchical_rtl.sh` 的 R1 口径 |
| **J-4** | **双形态逐拍等价** = 扁平 ≡ 层次化（**逐拍全等**）⇒ ⇒ `ports` **由 `emit` 派生**（**禁手写**）⇒ ⇒ **激励 ≥ 10 拍且值不同** + **观测集 = {`rdata`, `gpio_out`, `uart_tx`}**（⚠️ **不含 `done`**）⇒ ⇒ MUST 有 **≥2** 个随激励变化的观测量 ⇒ ⇒ 否则触发 `no-varying-signal` 豁免 ⇒ ⇒ 判别力天然很弱。<br>⚠️ **`done` MUST NOT 计入观测量**（**2026-11-16 dated 订正**：实测 `done` 是**粘滞**完成标志 —— `axi_soc_example.cpp:149-151` 无清零分支 ⇒ ⇒ 首笔后恒 1 ⇒ ⇒ 不随激励变化 ⇒ ⇒ 计入只会让变化判据**恒绿**） | **`ch::hier_eq::emit` + `compare`**（事实 ⑨⑪）⇒ ⇒ **不需新写比对代码** |
| **J-5** | **独立 TB 可跑**：`RESULT: checks=<N> fails=0`（N ≥ **4**）· 失败**非零退出** | `tb_axi_soc_example.sv` |

### 2.8 **D8** = SoC 层覆盖登记 = tsv 的**新 kind（`kind=soc`）** + **锚点按 kind 分桶**

⚠️ **事实订正**：SoC **不是**「非 example 形态」—— `axi_soc_example` **有 `main()`** 且**是** tracked target（事实 ⑮）⇒ ⇒ tsv 的 `target` 列**填** `axi_soc_example`。
⚠️ **为什么仍要新 kind**：用户 2026-11-16 裁决「**维持首批 10**」⇒ ⇒ SoC **不进**首批 10 的口径 ⇒ ⇒ 若用 `kind=target`，SoC 行会**混进**首批 10 的计数与 status 判定面 ⇒ ⇒ ⇒ **必须**隔离 ⇒ ⇒ ⇒ 新 kind `kind=soc`。
⚠️ **锚点分桶（**本条是裁决，不是待办**）**：现有护栏 ⓪ 只比「数据行数 == `REGISTERED_TARGETS`」（= 10）⇒ ⇒ 加 SoC 行 ⇒ 11 ≠ 10 ⇒ ⇒ 必红 ⇒ ⇒ ⇒ **裁决**：
> - `# REGISTERED_TARGETS: 10` 继续守「**首批 10**」（口径 = `kind ∈ {target, dut}` 的行数）
> - **新增** `# REGISTERED_SOC: 1` 守 SoC 数（口径 = `kind=soc` 的行数）
> - 护栏 ⓪ **按 kind 分桶**判定 ⇒ ⇒ 两个口径各自守恒 ⇒ ⇒ 「首批 10」**仍有人守**（不是靠总数）
>
> ⚠️ **排除表的行 MUST 同步裁决**：SoC TB 交付后，`.rddf/checks/examples-rtl-sim-exclusions.md:36` 的 `axi_soc_example` 行 MUST **移除**（它登记的理由 =「缺 TB」⇒ ⇒ TB 交付后理由消失）⇒ ⇒ **禁**保留（保留 ⇒ ⇒ 排除表与 tsv **双重登记**同一 target ⇒ ⇒ 各说各话）⇒ ⇒ 该行的「Timer」漂移（事实 ⑧）随之**一并消失**（行被删）⇒ ⇒ **但源码头注释**（`axi_soc_example.cpp:4`）与**零使用 include**（`:24`）**仍 MUST 单独订正**（V10）
⇒ ⇒ ⚠️ 护栏的「`target` 列对 `kind=target` 的行取自 `CH_TRACKED_EXAMPLE_TARGETS`」这条**不变** ⇒ ⇒ `kind=soc` 的行不受它约束（但其 target 值仍**必须**是真实存在的 target 名 ⇒ ⇒ **可机械核对** ⇒ ⇒ ⚠️ **但该约束目前只是「约定」**：护栏 `verify_examples_rtl_sim.sh` 现有逻辑**只**做行数守恒 ⓪（`:92-99`），**无** target 列存在性的机械比对 ⇒ ⇒ tracked 集合的机械比对在 `verify_example_test_wiring.sh:151-156` 且**不读** tsv 的 kind 列 ⇒ ⇒ ⇒ `kind=soc` 行不触发任何现有检查）。

### 2.9 **D9** = 「对外 AXI 主口」档（T-3）= **独立立项**（本 ADR 只交付 T-1 / T-2）

理由：① 无 BFM（事实 ⑨ 的缺口）② 顶层 AXI 展平策略**未定义**（事实 ⑫）⇒ ⇒ 两项都是**架构级** ⇒ ⇒ **不在本 ADR 内** ⇒ ⇒ 登记为独立立项（触发条件 = 用户要求「CPU 经 AXI 访问外部存储」）。
⚠️ **不得**再引用「`ch_mem` 跨模块禁令」作为第三条理由（事实 ⑬：禁令已被收窄到「mem 本体跨 module」）。

### 2.10 **D10** = 与 ADR-072（D1）**并行**（写点零交集），但**D2 是 SoC 侧的前置**

| | 本方案写点 | ADR-072 D1 写点 |
|---|---|---|
| 目录 | `examples/**` · `.rddf/checks/**` · `scripts/**` · `tests/**` · `docs/**` | `src/**` · `include/**` · 10 份既有 TB · 两份 capability |
⇒ ⇒ **零交集** ⇒ ⇒ 可并行。
⇒ ⇒ ⚠️ **但 D2 是 SoC 侧的硬前置**：D1（ADR-072）落地后，任何**未 tie-off** 的后代 `ch_in` ⇒ ⇒ 发射 fail-closed ⇒ ⇒ 所以 **D2 MUST 先于 ADR-072 D1 的合并**，否则 SoC 侧被卡死（**不是风险，是必然**）。

---

## 3. 选项对比表

| # | 选项 | 做法 | **结论** | 理由（**真的更差**，不是「不如推荐项」） |
|---|---|---|---|---|
| **①** | **SoC 产物 = 层次化**（每个成层 IP 一个 module） | `ch_layer` 档 ③ + `toVerilogHierarchical` | ✅ **选中** | **唯一**让 IP 边界在产物里可见的形态 ⇒ ⇒ 而 IP 互联**正是 SoC 的价值**（问题 §1.3）⇒ ⇒ 且是 `backend_port_set`（事实 ⑩）与 `hier_eq::compare`（事实 ⑨⑪）**唯一**支持的形态 ⇒ ⇒ 双形态判据 J-4 零新增代码 |
| ② | SoC 产物 = 扁平 | `toVerilog(path, soc)`（带根重载） | ❌ **拒** | IP 边界**不可见** ⇒ ⇒ 与 IP 级产物**无结构区别** ⇒ ⇒ 「SoC 层 RTL 生成」名存实亡；且**无** `backend_port_set` 重载 ⇒ ⇒ J-4 的 A 侧只能靠 `hier_eq` 内部自生成 ⇒ ⇒ 少一条现成的显式端口集通道 |
| ③ | 扁平 + 层次化**两个产物都交付** | 双发 | ❌ **拒** | **双份契约**（两个顶层端口面 + 两份期望源）⇒ ⇒ 长期漂移的经典形态（ADR-072 D7 的双档经验已证） |
| ④ | SoC 层 TB = **仓内后端**驱动（不用独立 TB） | `try_initialize_with_verilog` | ❌ **拒** | 后端是**仿真器**，不是**测试台** ⇒ ⇒ 它**没有**「驱动顶层端口 + 采样 + 判据汇总 + 非零退出」的载体 ⇒ ⇒ 「独立可跑的 TB」这条产品属性落空；⇒ ⇒ 它**有**的正当用途 = J-4 的逐拍等价（D3 的「桥」）⇒ ⇒ **两者职责不同，不是二选一** |
| ⑤ | SoC 顶层**用 bundle 暴露 AXI** | `ch_in<AxiLite> aw` | ❌ **拒** | ADR-042 决策 5：**绝对**禁止（无例外条款）⇒ ⇒ **零个端口生成** ⇒ ⇒ TB 根本连不上 ⇒ ⇒ 不是「风格问题」，是**功能不成立** |
| ⑥ | T-3（对外 AXI）**纳入本 ADR** | 一并做 BFM + 顶层展平 | ❌ **拒** | ① 无 BFM ② 顶层展平策略**未定义** ⇒ ⇒ 两项都是架构级 ⇒ ⇒ **独立立项**（D9）⇒ ⇒ 并入会把范围从「SoC 交付形态」撑成「SoC + 存储子系统」 |
| ⑦ | SoC 的期望源放 **TB 文件注释**里 | 不另立登记 | ❌ **拒** | 期望值必须**手推**且**可审计** ⇒ ⇒ TB 注释会与实现一起漂 ⇒ ⇒ 无独立载体 ⇒ ⇒ 判据不可追溯 ⇒ ⇒ 应是「**指针 + 登记**」，不是二选一 |
| **⑧** | **（新增）SoC 的后代输入「不接、也不 tie-off」** | 保留 31 个未驱动 | ❌ **拒** | ADR-072 D1-f 落地 ⇒ ⇒ 发射 **fail-closed** ⇒ ⇒ **SoC 产物发不出来** ⇒ ⇒ 且 uart 不在总线上 ⇒ ⇒ UART 场景不可达 ⇒ ⇒ **这不是风格问题，是「能不能交付」的问题** |
| **⑨** | **（新增）SoC 只做 2 个 IP**（uart 整体去掉） | 缩到 master + matrix + gpio | ❌ **拒** | ① uart 的 11 个输入仍需 tie-off（否则同样 fail-closed）⇒ ⇒ 省不下工作量 ② 「多 IP + 总线互连」正是本 ADR 的交付目标 ⇒ ⇒ 缩到 2 IP ⇒ ⇒ SoC 与单 IP 无结构区别 ⇒ ⇒ **与选项 ② 同型**（都退化成「看不出是 SoC」）⇒ ⇒ **承接的决策 = D1 + J-2 / V1**（module 数 == 成层 IP 数 + 1 ⇒ ⇒ 缩到 2 IP ⇒ ⇒ 期望 module 数变 3 ⇒ ⇒ 判据随之降级） |

---

## 4. 后果 / 代价

### 4.1 正面

- ✅ **「SoC 层 RTL 生成」从名存实亡变成有交付物**（事实 ③ + D1）。
- ✅ **J-4 零新增比对代码**（事实 ⑨⑪ 的公共 API）⇒ ⇒ 最贵的判据最省。
- ✅ **D2 的 tie-off 惯用法仓内已有**（事实 ⑥）⇒ ⇒ 不需发明机制 ⇒ ⇒ 成本可估。
- ✅ **过渡形态 == 目标形态**（D4）⇒ ⇒ TB 不必写两遍 ⇒ ⇒ 不返工。
- ✅ **与 ADR-072 D1 并行**（D10）⇒ ⇒ 不增加关键路径。

### 4.2 负面（**代价**）

| # | 代价 | 缓解 |
|---|---|---|
| ① | ⚠️ **D2 要改 example 的连线**（**~31 行**：m1 的 11 根 tie-off + s1↔uart 的 19 根**单向**接线（11 正 + 8 反）+ `master.rst_n` 1 根透传）⇒ ⇒ **example 源码改动量大于「只改 4 个 `ch_module`」** | 照 `phase3a_complete.cpp:259-302` 的现成 idiom ⇒ ⇒ 逐行可对照。⚠️ **行为影响分两类**（**2026-11-16 dated 订正**，原写「解释器行为不变」不准确）：<br>· **有行为变化** = `s1↔uart` 的 **19 根真互联** ⇒ ⇒ uart 才在总线上（**这正是本 change 的目的**）⇒ ⇒ 既有测试若依赖「uart 不可达」需复核<br>· **无行为影响** = m1 的 11 根 tie-off（m1 未用）+ `master.rst_n`（`describe()` 不消费） |
| ② | ⚠️ **`kind=soc` + 锚点分桶**（D8）⇒ ⇒ 护栏 ⓪ 的判定逻辑要改（按 kind 分桶）⇒ ⇒ 工具面变更 | 与 D6 同批；⇒ ⇒ 两个锚点各自守恒 ⇒ ⇒「首批 10」仍有人守 |
| ③ | ⚠️ **模块类型名非契约** ⇒ ⇒ 改名 ⇒ J-2 的期望 module 数要重算 | **已接受**（ADR-070 §6 ⑦）⇒ ⇒ 登记为「改名后必须重算结构判据」 |
| ④ | ⚠️ **J-4 只覆盖顶层可见行为**（成层子树的 `ch_out` 在层次化顶层不可观测）⇒ ⇒ **观测盲区** | **已接受**（与 `rtl-hierarchical-output` capability 的同一条边界一致）⇒ ⇒ MUST 在 capability 里显式登记 |
| ⑤ | ⚠️ **`uart.irq` 与根的 `timer_irq` 都是无消费者的残留**（事实 ⑭）⇒ ⇒ SoC 的端口面里有「看起来有效但没有语义」的端口 | **登记**（D2 表的最后两行）⇒ ⇒ **不假装它们可观测** |
| ⑥ | ⚠️ **T-3（对外 AXI）不做** ⇒ ⇒ 「CPU + 外部 DDR」的 SoC 形态**不支持** | 独立立项 + 显式产品边界（D9）⇒ ⇒ **不假装支持** |

### 4.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| ① | 🔴 **D2 未落地而 ADR-072 D1 先合入** ⇒ ⇒ SoC 侧被 fail-closed 卡死 | **D2 MUST 先于 ADR-072 D1 的合并**（D10）⇒ ⇒ 顺序是**硬约束**，不是建议 |
| ② | ⚠️ **`ch_layer` 档 ③ 的模块类型名与 `sanitize_verilog_name` 的撞名**（同 base 不同 type ⇒ 拒绝发射）⇒ ⇒ 4 个名字必须互不同 | 名字在实现期**一次定死**并登记 ⇒ ⇒ J-2 的期望 module 数随之钉住 |
| ③ | ⚠️ **SoC 的兄弟边形态命中 ADR-070 的发射器边界**（事实 ⑯）⇒ ⇒ 但 H-A/H-B 已覆盖 ⇒ ⇒ **不撞未修的路径** | 复用 ADR-070 D1 的落地实现；⇒ ⇒ **若仍撞出第二层失败 ⇒ ⇒ 按 ADR-070 §6 ⑥ 重新裁决，MUST NOT 就地扩权** |
| ④ | ⚠️ **J-2 的 module 数是「数出来的」** ⇒ ⇒ IP 增减 ⇒ 判据漂 | 期望值 MUST 与**成层声明处**同源（⇒ ⇒ 从 `ch_layer` 的调用点派生，禁手写常量） |
| ⑤ | ⚠️ **J-4 的判别力依赖激励设计**（`done` **粘滞** ⇒ ⇒ 不可作变化量；观测集已排除它）⇒ ⇒ 观测集 MUST 含 ≥2 个随激励变化的量 | J-4 的观测集与 **≥10 拍**激励**写进契约**（D7）⇒ ⇒ 不是实施期的自由发挥 |
| ⑥ | ⚠️ **TB 的 `checks=N` 会随场景增减变** ⇒ ⇒ tsv 的 `min_checks` 必须**同批**重登记 | 照 `examples-rtl-sim-coverage` 的既有机制（`min_checks` 下界 + `scenarios` 计数） |

---

## 5. Verification（**前瞻**验收判据 —— 可勾选 · 未跑不勾）

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出）⇒ ⇒ 「两份同源产物一致」**≠** 正确性证据。
> ⚠️ **禁裸数字**：module 数 / 端口数 MUST 从**产物**派生。

- [ ] **V1** 🔴 **J-2 · IP 边界可见**：`AxiSocTop` 的层次化产物 **module 数 == 成层 IP 数 + 1**（= **5**）⇒ ⇒ 4 个 IP 的模块类型名各自出现一次 ⇒ ⇒ **负向对照**：把某个 `ch_layer` 改回 `ch_module` ⇒ ⇒ 该 IP inline ⇒ ⇒ module 数必减 ⇒ ⇒ 必红
- [ ] **V2** 🔴 **J-1 · 顶层端口面**：SoC 产物顶层端口表 == 框架 2 + 根自有 12 = **14**（**逐字逐序**）⇒ ⇒ **零冒泡端口**（`top_master_*` / `top_matrix_*` / `top_gpio_*` / `top_uart_*` 零命中）⇒ ⇒ **负向对照（**已订正**）**：**恢复一根未驱动输入**（如拔掉 `gpio` 的 `rst_n` 驱动）⇒ ⇒ 该输入冒泡 / 发射失败 ⇒ ⇒ 必红 ⇒ ⇒ ⚠️ **「去掉一个 `ch_layer`」不是有效负向对照**（成层 IP 的 `ch_out` 本就不冒泡 ⇒ ⇒ 顶层端口集合不变 ⇒ ⇒ 恒绿）
- [ ] **V3** 🔴 **D2 · 可发射性**：SoC 产物在 ADR-072 D1 落地后**仍能发射**（`ok == true`）⇒ ⇒ 即 **31 个后代 `ch_in` 全部有驱动源**（`m1_*` 11 + `uart` AXI 11 + `s1_*` 响应侧 8 + `master.rst_n` 1）⇒ ⇒ **负向对照**：拔掉一根 tie-off ⇒ ⇒ 发射 **fail-closed**（`ok == false` + error 点名）⇒ ⇒ 必红
- [ ] **V4** 🔴 **J-3 · lint 干净**：`verilator --lint-only --top-module top` **exit 0** · 零 `%Error` ⇒ ⇒ **禁** `-Wall` / `-Wno-fatal` / `-Wno-<单项>`
- [ ] **V5** 🔴 **J-4 · 双形态逐拍等价**：`ch::hier_eq::compare` 判**逐拍全等** ⇒ ⇒ `ports` **由 `emit` 派生**（**禁手写**）⇒ ⇒ **激励 ≥ 10 拍且值不同** ⇒ ⇒ **观测集 == {`rdata`, `gpio_out`, `uart_tx`}** 且含 **≥2 个随激励变化**的量（⇒ ⇒ 否则 `no-varying-signal` 豁免 ⇒ ⇒ 判别力天然很弱）⇒ ⇒ **`done` MUST NOT 计入观测量**（**粘滞** ⇒ ⇒ 首笔后恒 1）⇒ ⇒ **变异自证**：篡改层次化产物的一条 `assign` ⇒ ⇒ 必红
- [ ] **V6** 🔴 **J-5 · 独立 TB 可跑**：`tb_axi_soc_example.sv` 独立运行 ⇒ ⇒ `RESULT: checks=<N> fails=0`（N ≥ **4**）⇒ ⇒ 失败时**非零退出** ⇒ ⇒ **场景覆盖 ≥ 4**（**可观测面**）：① 写 GPIO DATA_OUT ② 读 GPIO DATA_IN ③ 写 UART TX_DATA ④ `rst_n` 拉低后读 GPIO DATA_OUT == 0 ⇒ ⇒ **变异自证**：改坏一个期望值 ⇒ ⇒ 必红且非零退出。<br>⚠️ **「未映射地址 DECERR」已从场景集删除**（**2026-11-16 dated 订正**：① 顶层**无 `bresp` 端口** ⇒ ⇒ DECERR 与 OKAY 不可区分 ② `AxiLiteMatrix2x2` 默认映射**铺满 32 位**（`include/axi4/interconnect_common.h:63,74`）⇒ ⇒ matrix 侧 DECERR **结构性不可触发**）⇒ ⇒ **DECERR 的协议覆盖不失**：归 IP 级（`test_axi_lite_matrix_2x2.cpp` 的非默认映射用例 + `tb_axi4_lite_example.sv`）
- [ ] **V7** 🔴 **期望源在册**：`.rddf/checks/soc-rtl-expectations.md` 有该 SoC 一节（端口映射 + 地址映射 + 事务序列 + **手推结果**）⇒ ⇒ 且 TB 头的 `EXPECT-SRC:` 指针指向它 ⇒ ⇒ **负向对照**：指针指向不存在的锚点 ⇒ ⇒ 必红
- [ ] **V8** 🔴 **覆盖登记**（D8）：tsv 有该 SoC 的 `kind=soc` 行（`target` = `axi_soc_example`）⇒ ⇒ 护栏认这个 kind ⇒ ⇒ 且 **`# REGISTERED_TARGETS: 10` 与 `# REGISTERED_SOC: 1` 两个锚点各自守恒**（⇒ ⇒「首批 10」仍有人守）⇒ ⇒ 登记数锚点**禁**写死单值
- [ ] **V9** ⚠️ **顺序无回归**：本 change **不碰** `src/**` 与 `include/**` ⇒ ⇒ `ctest -LE perf` 全绿 ⇒ ⇒ 既有 10 份 TB 零改动
- [ ] **V10** ⚠️ **事实漂移订正（**源头 + 登记**）**：① `examples/axi4/axi_soc_example.cpp:4` 的源码头注释（「+ **Timer**」）② `:24` 的**零使用** `#include "axi4/peripherals/axi_timer.h"` ③ `.rddf/checks/examples-rtl-sim-exclusions.md:36` ⇒ ⇒ **只改登记不改源头 ⇒ ⇒ 漂移会复发**
- [ ] **V11** ⚠️ **残留登记**：`uart.irq` 与根的 `timer_irq`（零消费者，事实 ⑭）⇒ ⇒ **登记为残留**且**不**进 TB 的有效端口面 ⇒ ⇒ 可 grep 到
- [ ] **V12** ⚠️ **T-3 边界可见**：能力面**明文**声明「**不支持** CPU 经 AXI 访问外部存储」⇒ ⇒ 精确串可 grep ⇒ ⇒ 不得让下游误以为支持

---

## 6. 落点与后续

| # | 事项 | 归属 | 载体 |
|---|---|---|---|
| ① | **本 ADR 的 V1–V12 映射为 change 的 AC / task** | planner | 新 change `soc-rtl-product-and-tb-2026-11` 的 proposal |
| ② | **可枚举改动清单**（架构主题文档 §3） | planner → builder | 架构主题文档 §3 |
| ③ | **`kind=soc` + 锚点分桶**（D8）⇒ ⇒ 护栏 ⓪ 按 kind 分桶 | builder | 与 TB 登记同批 |
| ④ | **T-3（对外 AXI 主口）** | 用户 / planner | **独立立项**（触发条件 = 用户要求「CPU 经 AXI 访问外部存储」） |
| ⑤ | **能力面**：**新增 capability `rtl-soc-output`**（**不**并入 `rtl-hierarchical-output` ⇒ ⇒ 与 ADR-072 的在途 delta **零冲突**）⇒ ⇒ 内含：J-2（IP 边界可见）· J-5（SoC 层 TB 契约）· D6（期望源登记义务）· **J-4 的观测盲区登记** · **V12 的 T-3 不支持声明** | planner | change 的 spec delta（**ADDED** 形态 ⇒ ⇒ 无标题冲突面） |
| ⑥ | **事实漂移订正**（V10） | builder | 独立小项（可与 ③ 同批） |
| ⑦ | **残留登记**（V11） | architect | 台账 §7 + `.rddf/checks/arch-residual-registry.md`（**两侧同批**） |

> ⚠️ **本 ADR 的 Status 在 oracle + critic 全闭前 MUST 保持 `Draft`** ⇒ ⇒ 「把未决问题写成已决」属 `arch` 档 BLOCKER 第四类。
> ⚠️ **D1 的条件式**：若 T1（`ch_layer` 改造）撞出**发射器层面的第二层失败** ⇒ ⇒ 按 ADR-070 §6 ⑥ **重新裁决**，**MUST NOT** 就地扩权。
> ⚠️ **D10 的顺序是硬约束**：**D2（SoC 侧 tie-off / 接通）MUST 先于 ADR-072 D1 的合并** ⇒ ⇒ 否则 SoC 侧被 fail-closed 卡死。
