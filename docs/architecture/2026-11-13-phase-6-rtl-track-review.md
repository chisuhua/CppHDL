# Phase 6 / RTL 轨复盘 #4（2026-11-13 · architect 域）

> **触发**：用户口令「切architect对完成和后续任务进行梳理，检查下我们要实现的目标，
> 为什么要实现这个目标, 已经完成了哪些，后续要怎么做, 为什么要这么做，目前的任务是什么，
> 然后切planner对后续还有哪些任务内容都梳理一遍，再给出后续任务内容和实施路径，
> 再切builder进行实施」。
> **本文件的定位**：**当前态快照 + 目标链 + 本轮实测发现 + 边界裁决**（架构主题文档 · **非决策文档**）
> ⇒ ⇒ **决策内容零变更**；本轮**不新建 ADR**、**不反转任何已签批决策**。
> **窗口唯一真相** = `.rddf/roadmap/phases/phase-6.md`（**本文件 §2–§6 是 2026-11-13 的实测快照，
> 会过期** ⇒ ⇒ **不作为状态真相使用**）。
> **上游** = `2026-11-12-phase-6-rtl-track-review.md`（复盘 #3 · T2 裁决后）·
> `docs/adr/ADR-069-mem-data-alignment.md`（**Accepted** · T2 裁决）。
> **下游** = `.rddf/roadmap/phase-6-next-steps.md`（planner 域 · **任务内容 + 实施路径**的权威载体）。

---

## 1. 目标链（**做什么 → 为什么**）

### 1.1 顶层目标：为什么要有「RTL 生成能力产品化」这条线

`CppHDL` 的定位是 **C++23 的硬件描述库** ⇒ ⇒ 它的**交付物**不只是「能仿真的 C++ 代码」，还必须包括
**能被下游 FPGA/ASIC 工具链吃掉的产物**（Verilog + 端口清单 + 行为等价性）。

⇒ ⇒ 用户 2026-11-10 拍板的 7 项诉求，**每一条都指向同一个病根**：

| 用户诉求 | 草案 | 病根 |
|---|---|---|
| ① 生成器作为项目提供的能力 | `rtl-emit-product-tooling` | 「**能生成**」与「**是产品**」之间有一整段距离 |
| ② 固件转储器 + ELF 装载 | `firmware-load-chain` | 差一整条装载链路（现 `prog_*` **只**接 I-TCM ⇒ ⇒ 有 `.data` 的程序**装不进去**） |
| ③ examples 的 RTL 生成 + 独立仿真 + 单测 | `examples-rtl-sim-coverage` | 生成产物形态**零测试** ⇒ ⇒ 退化无人知 |
| ④ 层次化 RTL 示例 | （B1 已落地） | 「能生成」≠「能看结构」⇒ ⇒ **本轮实测：RV32I 上无成层示例**（默认 inline 是**契约行为** ⇒ ⇒ 缺口 = **诉求 ④ 未兑现** · §3.3） |
| ⑤ 端口名与 C++ 成员名一致 | `port-naming-ratchet` · `port-name-literal-parity` | 现状端口名不可用（`top_unnamed_input_3`）⇒ ⇒ **本轮实测：产品组件采用率 = 0**（§3.4） |
| ⑥ 收口 RV32I 重复组件 | `rv32i-dead-component-consolidation` | 现状给的是**虚假信心**（两套实现，只有一套被流水线消费） |
| ⑦ RTL 卫生 | （B1 的 C6 已落地） | 「能生成」≠「产物能过 lint / 能综合」⇒ ⇒ 端口面按用户裁决**保持现状**（§4.2 裁决 ②） |

⇒ ⇒ **共同点**：每一条都是「**声称成立但无人可验证**」⇒ ⇒ 所以这条线的统一解法**不是加功能**，
而是**给每个声称配一条可复算的机械判据**（护栏 / 产物级断言 / 变异自证）。

### 1.2 为什么分两批（B0–B4）

`phase-6-rtl-productization.md` 的批次编排**不是按工作量排，是按「交付风险」排**：

· **B0** = 零交付风险（除 F-A 外）⇒ 先把**零消费者**的组件修对 / 退役 ⇒ ⇒ 后面批次的红**不是**它们造成的
· **B1** = 地基（manifest / CLI / 产物护栏）⇒ 下游全部依赖它 ⇒ ⇒ 晚做 ⇒ ⇒ 后面每一批都要重新适配端口面
· **B2** = 消费侧（examples 覆盖）⇒ 只**消费** B1 的产物 ⇒ ⇒ 不改产品语义
· **B3** = 时序契约（D-TCM 寄存读）⇒ 动流水线时序 ⇒ ⇒ golden 会变 ⇒ ⇒ 必须**单独一批**（否则金标算两遍）
· **B4** = 核心重构（组件接入）⇒ 改 ID/EX 级 ⇒ ⇒ **必须在 B3 之后**（同依据）

⇒ ⇒ **硬约束（本轨最容易被违反的一条）**：**B3 → B4 不得乱序**。

### 1.3 前三批的真正价值（不是「覆盖率 +N」）

B0–B2 的名义交付是「组件退役 + 地基 + 覆盖率」，**实际产出是三个被仿真抓出来的真实实现缺陷**
—— 它们在 C++ 侧的 demo 里**全部是绿的**：

· **D5** 手写 FIFO demo 的 `full` 门限错（31 在途 ⇒ 第 17 次写入起地址混叠）⇒ C++ 侧 demo 只写 2 个元素 ⇒ **永远够不到门限**
· **D6** `uart_rx` 的采样点**硬编码** `div == 4`（不随 `prescale` 缩放）⇒ demo 的 `prescale == 9` ⇒ 硬编码**恰好**等于 `prescale/2`
· **D7** `uart_rx` 解出的字节**位反转**（线上 `0x10` ⇒ 解出 `0x08`）⇒ demo 的测试值 **0x5A 是位回文** ⇒ **恰好掩盖**

⇒ ⇒ **D7 教训可外推**：**测试值必须避开对称值**（位回文 / 字节回文），否则位序缺陷被恰好掩盖
⇒ ⇒ 与仓内「期望值 MUST 手推」是同族纪律：**手推的期望值本身也要能判别**。

⇒ ⇒ **B0 的价值同类**：`Rv32iAlu` 的 **SLTU 是真缺陷**（`rv32i_alu.h` 原用 `bits<31,31>(sub_result)`
⇒ ⇒ 手推反证 `0 <u 0xFFFFFFFF` 得 **0** ⇒ ⇒ RISC-V 要求 **1**）⇒ ⇒ 已修（`:103` = `(operand_a < operand_b)`）。

---

## 2. 已完成哪些（**实测快照 · 2026-11-13**）

### 2.1 RTL 轨（7 份草案 · B0–B2）

| 批 | 状态 | 证据 |
|---|---|---|
| **B0** | ✅ **6/6** | `67926cf` + `bd21b45` · `.rddf/reviews/b0-batch-verify-2026-11.md` |
| **B1** | ✅ **11/11** | C4/C1/C3/C5/C6/N1/N2/N3/N4/N5(a)/N6/P1 · `.rddf/reviews/b1-batch-verify-2026-11.md`（**复审 APPROVE**） |
| **B2** | ✅ **E1–E8 全部落地** | E3 = **10/10 verified** · E6 = `pr-verilator-fast` 的**一个 step**（不新建 job）· 护栏**严格模式 PASS** |

⇒ ⇒ **B2 的 10 个 TB = 实测 1038 条断言**（登记下界 782 · 语义场景 54 · spec-derived 676 ⇒ ⇒ **四个口径必须分清**）。

### 2.2 phase-6 轨（P1–P15）

✅ 已闭：**P1 / P2 / P2b / P3 / P4 / P5 / P9 / P10 / P12 / P13 / P14 / P15** ·
⬜ 未闭：**P6**（RV32I ISA 缺口侦察+立项）· **P7**（②⑤ + ④ 闭口）· **P8**（①②④）·
🔒 **P11**（ADR-036 物理移除 · 日历锁最早 **2027-04-17**）。

⇒ ⇒ **本轮实测复核**：P7 的 `set_ab_verification` stub **仍在**（`include/simulator.h:95` + `src/simulator.cpp:529`）·
P6 的 `FENCE` / `illegal` / `unknown_opcode` 在 `include/cpu/pipeline/**` **零命中** · `rv32i_exception.h:132,136`
的 `write_mepc = write_mcause = do_trap_entry` **原样未动** ⇒ ⇒ **mcause 仍不可达**。

### 2.3 残差台账（**23 行** · 护栏自报数）

**10 已收口 / 4 已裁决 / 4 不做 / 2 已知 / 3 未决** ⇒ ⇒ 护栏 `verify_arch_ledger_residuals.sh` **PASS**（双向 1:1 守恒）
⇒ ⇒ 2 条「已知」= 行 **15**（三档 PortPolicy 未分化）+ 行 **9**（Verilator 等价性排除项）
⇒ ⇒ 3 条「未决」= 行 **21 / 21b**（route (c) 字面同名）+ 行 **22**（同名兄弟静默覆盖）。

### 2.4 复盘 #3 之后新闭的 2 件

| # | 事项 | 状态 | 证据 |
|---|---|---|---|
| ① | **T-a / R12 = 12 条漂移一次性扫干净** | ✅ **已提交** | `83cdb89` + `d6c731c` · `.rddf/roadmap/phase-6-next-steps.md` R 表已刷新（R12–R17 + 实施路径 + 判据自检 · metis 19 条处置） |
| ② | **7 份草案 + 索引的审计回填**（planner 独立复核） | ✅ **已落盘**（⚠️ **未提交** ⇒ ⇒ **builder 步的第一动作 MUST 是提交这 7+1 份** ⇒ ⇒ 否则「会话中断」即造新漂移源 · oracle F6） | 7 份草案各插 1 行 `- **审计（2026-11-13 …）**` · 索引 7 行追加指针 · `.rddf/checks/decision-request-port-face-2026-11.md`（**新建** · R1/R2/R3 待裁决）⇒ ⇒ **HARD-GATE 7/7 + 四方一致 7/7 + 零实现面变更** |

### 2.5 本轮**不复述**的部分（避免与上游漂移）

B0–B2 的 7 份草案 **AC 回填 = 73 / 139**（未勾的恰是未开工项 + 真缺口）· 9 份 RTL 轨草案的逐份状态 ·
残差台账逐行内容 · 层次化产物级 R1–R8 判据 —— ⇒ ⇒ **这些一律以各草案 / 台账 / 护栏为准，本文件不复制**。

---

## 3. ★ 本轮**实测发现**（**4 条** · 每条都有可复跑证据）

> 这 4 条是本轮 architect 复盘的**实质产出** —— 不是复述状态，是**实测发现的缺口**。
> ⚠️ **复盘 #3 的 8 条发现已全部闭**（R12 + `83cdb89` / `d6c731c`）⇒ ⇒ **本轮是新一轮的发现**。

### 3.1 发现 1 🔴：**护栏的 F6 = 「当前不适用」，而 AC 已改成可判别形态 ⇒ ⇒ 护栏未实现 AC**

| 位置 | 内容 |
|---|---|
| `rtl-emit-product-tooling` 的 AC（`:204`） | `~~verilator 后端的 work_dir/top.v 与产品产物逐字节相同（F6）~~` ⇒ ⇒ 🔴 **判据恒绿且形态不符**（两者是**同一个** `verilogwriter`）⇒ ⇒ 改为「**`ctest -L verilator_e2e`（77 条）全绿 ＋ `build_port_access_table()` 对每个 `port_access_` 条目都能取到非空 Vtop 字段**」 |
| `scripts/verify_flat_rtl_emit.sh` 实测 | `::notice F6 **当前不适用**（不是 pass）：本仓工具的设计**无寄存器** ⇒ ⇒ verilator backend 不产 top.v ⇒ ⇒ 无可比对象` ⇒ ⇒ 解除条件 = 「工具的设计里**加一个寄存器**」 |

⇒ ⇒ **落差**：**AC 已经改成可判别形态，护栏却仍停在「不适用」** ⇒ ⇒
⇒ **backend 的端口表与产品端口面是否同步，当前无任何判据**（`port_access_` 与 `emit_sim_main_postlude`
是否与 flat 的 114 端口面一致 ⇒ 无护栏 ⇒ ⇒ 未来改端口面时**无人报警**）。

⇒ ⇒ **已立单** `.rddf/checks/decision-request-port-face-2026-11.md` **R2**（用户 2026-11-13 未裁决）。

### 3.2 发现 2 🟡：**`gpio_example` 的断言强度不足（`spec_checks = 0`）**

`scripts/verify_examples_rtl_sim.sh` 实测自标：

```
── gpio_example（target · emit_rtl_gpio · examples/spinalhdl-ported/gpio/tb_gpio_example.sv）
  ⚠️  ②'' spec_checks=0 < 3 ⇒ ⇒ impl-contract only（只能防漂移，不能证明符合规范）
  ✅ gpio_example：RESULT: checks=9 fails=0（scenarios=7 · spec_checks=0）
```

⇒ ⇒ 10 项里 **9 项**有 `spec_checks ≥ 3`（断言**能证明符合规范**），**唯独 `gpio_example` = 0**
⇒ ⇒ 它的 9 个 check 只能证明「产物没漂移」，**不能**证明「产物符合规范」。

⇒ ⇒ **已立单** `decision-request-port-face-2026-11.md` **R3**（待裁决）。

### 3.3 发现 3 🟡：**用户诉求 ④（RV32I 层次化 RTL 输出）仍未兑现** —— ⚠️ **但「退化」本身是契约行为，不是缺陷**

实测（`toVerilogHierarchical(Rv32iPipelineTop)`）：

| 量 | 实测 |
|---|---|
| 返回值 | **`ok = true`**（无错误、无诊断） |
| 产物 module 数 | **1**（层次化应 > 1） |
| 与扁平产物逐字节比较 | **完全相同** ⇒ ⇒ 产物 = 扁平 |
| `CH_HIERARCHY_LAYER` 出现处 | `include/cpu/` + `examples/` = **0 处** |

⇒ ⇒ ⚠️ **措辞订正（oracle F4）**：**「零层设计 ⇒ 产物 inline 落进根 module」是**已签批的契约行为**
（`tests/test_hierarchy_equivalence.cpp:43` 明写「**不**写 `CH_HIERARCHY_LAYER` ⇒ 默认 inline
⇒ ⇒ 全部落进根 module」⇒ ⇒ 且**有测试钉着**它）⇒ ⇒ ⇒ **发射器「静默退化」不是缺陷**
⇒ ⇒ ⇒ **不得**据此去改发射器（那会与该契约正面冲突，并杀死合法的混层用法）。

⇒ ⇒ **真正的缺口 = 诉求 ④ 未兑现**（RV32I 上没有成层示例 ⇒ ⇒ 用户看不到「能看结构」）。

⇒ ⇒ ⚠️ **规模被普遍低估**（oracle F4 的第二层）：**跨 module 边需要「隐式端口合成」时，分析层是
**fail-closed 抛异常**的**（`src/hierarchy_analysis.cpp:7-9` + `:42-45` ⇒ 「T3-④ 的义务尚未实现」
⇒ ⇒ MUST 抛异常，MUST NOT 静默产出不完整的端口表」）⇒ ⇒ **RV32I 只声明一层就会撞这堵墙**
⇒ ⇒ ⇒ 诉求 ④ 的成本**不是「加一行 `CH_HIERARCHY_LAYER`」**，而是「先补隐式端口合成」
⇒ ⇒ ⇒ **必须独立立项**（与复盘 #3 的裁决 ③ 同判据 · 写点不重叠：`src/hierarchy_*` vs `include/cpu/**`）。

### 3.4 发现 4 🟡：**⑤ 端口命名的产品侧采用率实测 = 0**（不是「低」，是零）

```
grep -rhoE 'ch_(in|out)<[^;]*>[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\{"' include/ examples/ tools/
  ⇒ 5 处，全部在 tools/cpphdl-emit-rtl/main.cpp（工具自测载体）
```

⇒ ⇒ **产品组件零采用** ⇒ ⇒ `Rv32iPipelineTop` 的 32 个端口**全部**是 `unnamed_input` / `unnamed_output`
⇒ ⇒ manifest 的 `cpp_member` 字段**全为** `unnamed_*` ⇒ ⇒ **manifest 的「C++ 成员可解析」这层价值目前为零**。

⇒ ⇒ **棘轮的两条约束力要分开看**（oracle F2 订正）：

| 约束 | 状态 | 依据 |
|---|---|---|
| **「债不增」**（新增端口 MUST 具名） | ✅ **今天就是活的** | 护栏 P1 是**全树扫描**（`include/**` + `examples/**`）⇒ ⇒ 任何新增未具名端口 ⇒ ⇒ **红** ⇒ ⇒ 该约束力**不消耗**存量改造 |
| **「基线缩减」**（登记表逐条缩小） | ⬜ **停滞** | 1484 全豁免 ⇒ ⇒ 表格不变 ⇒ ⇒ **无推进力** |

⇒ ⇒ ⚠️ **「棘轮恒绿 ⇒ ⇒ 不构成任何推进力」是错的**（首稿写过头了）⇒ ⇒ 准确表述：
**「不增」这一半有效；「递减」这一半停滞**。

⇒ ⇒ ⚠️ **且「递减」另有顺带路径**：`port-naming-ratchet-2026-11.md:234` 已登记 ——
**B4/I5 改 `Rv32iDecoder` / `Rv32iAlu` / `HazardUnit` 的 `__io` ⇒ ⇒ 触发 P1 ⇒ ⇒ **必须带名**
⇒ ⇒ ⇒ 基线会**随 B4 自然下降** ⇒ ⇒ ⇒ R17 的价值**不在于「唯一推进力」**（见 §4.1 裁决 ① 的重新挂靠）。

⇒ ⇒ ⚠️ **已在册**（R17 + `port-naming-registry.md` §4 + `phase-6-rtl-productization.md` §4「N2 补遗」）
⇒ ⇒ **本轮不重复立单** ⇒ ⇒ **但它仍是本轮唯一「已定对象 + 无前置 + 可完整验证」的动作**（§4.1 裁决 ①）。

### 3.5 ⚠️ 附：本轮**未**成为发现的一条（planner 自我订正）

planner 2026-11-13 的口头审计把「`ext_clk` 是死端口」列为「新发现」⇒ ⇒ **有误** ——
该条**已于 2026-11-12 09:51 立单**（`decision-request-rv32i-ext-clock-dead-port-2026-11.md` · **状态：待裁决** · 建议 A2）。
⇒ ⇒ **教训**：做「独立复核」前 **MUST 先扫 `.rddf/checks/decision-request-*.md` 与 `*-correction-list-*.md`**。

---

## 4. 边界裁决（**architect 域的判断** —— 决定「什么算欠项、什么算门」）

### 4.1 裁决 ①：**R17（首批消化 32 端口）= 本轮交付动作** —— ⚠️ **依据是 architect 本轮裁量，不是「已裁决项的执行」**

> ⚠️ **首稿此处的推导有误，已订正（oracle F1 · BLOCKER）**：首稿写「用户 2026-11-11 已拍板 ⑤ ⇒ ⇒
> A′ 的**定义**就包含『先消化首批』⇒ ⇒ 它不是待裁决项」⇒ ⇒ **与源文件相反**：
> · A′ 的**原始定义**（`port-naming-ratchet-2026-11.md:22` · **用户拍板日 = 2026-11-10**）= 「框架先行 + 基线棘轮
>   —— **不**做一次性全量 codemod」⇒ ⇒ **不含**「必须先消化一批」的时间义务；
> · 该草案 `:8` / `:20` / `:180` **三处**明写：首批消化（32 端口）**未做** ⇒ ⇒ **已登记 §4「N2 补遗」**
>   ⇒ ⇒ **不在本项** ⇒ ⇒ 归属 **独立立项**；
> · `phase-6-next-steps.md` 的 **R17.1** 才是真正的待裁决项：「🔴 **用户裁决**：『**现在做**
>   （占 B3 末段 · 与 L1–L6 同批）』vs『**推到 phase-7**』」。
>
> ⇒ ⇒ **所以 R17.1 是一条真实的待裁决项**，本节 MUST 给它一个可审计的闭单依据，**不得**伪称「已裁决」。

#### 4.1.1 R17.1 的闭单依据（**architect 裁量 · 显式登记 · 可被用户推翻**）

| # | 依据 | 出处 |
|---|---|---|
| ① | **对象已由用户拍板** = `examples/riscv-mini/src/rv32i_pipeline_top.h` 的 **32** 个端口（**用户 2026-11-11 拍板**）⇒ ⇒ **做什么无争议** | `port-naming-ratchet` 草案 · `phase-6-rtl-productization.md` §4 |
| ② | **「现在做」若按 planner 原定位 = 「占 B3 末段 · 与 L1–L6 同批」⇒ ⇒ ⑤ 的兑现会被 B3 全链路（立项 → 实施 → 时序金标重算）阻塞** ⇒ ⇒ **延迟不可控**（B3 尚未立项 ⇒ ⇒ 兑现时点无法预估）⇒ ⇒ 且 R17 与 B3 在 `rv32i_pipeline_top.h` 上**同区 ⇒ MUST 顺序执行** ⇒ ⇒ ⇒ 若把 ⑤ 押在 B3 末段 ⇒ ⇒ **⑤ 的兑现时点 = B3 的时点** ⇒ ⇒ ⇒ 与「用户已在 2026-11-10 提出 ⑤」不相称 | `phase-6-next-steps.md` R17.1 / R16 · `firmware-load-chain-2026-11.md:197` |
| ③ | **用户 2026-11-13 本轮口令**把「**任务选择权**」委托给了 architect（「检查下…目前的任务是什么，然后…再切builder进行实施」）⇒ ⇒ ⇒ **由 architect 选批** ⇒ ⇒ **用户确认检查点 = T-k 交付时**（或 B3 立项前）⇒ ⇒ ⇒ ⚠️ **本条 NOT 含「用户未反对 ⇒ 成立」的推断**（首稿有，已删 —— 「未反对」不是裁决） | 本会话 2026-11-13 两轮往返 |
| ④ | **它是本轮唯一「与用户 ⑤ 直接相关」且「对象已定 + 无前置 + 判据可机械复算」的动作** ⇒ ⇒ ⚠️ **「唯一」限定在 ⑤ 这一条线上**（首稿写成全局唯一，与本文件 §5 矛盾 —— T-d / T-e 同样无前置）⇒ ⇒ **T-d（T-e）为何不选**：二者**与用户 ⑤ 无关**，且 T-e（R4 删 stub）会改 `include/simulator.h` + `src/simulator.cpp` ⇒ ⇒ **不写 `examples/**`** ⇒ ⇒ 与 T-k 零耦合 ⇒ ⇒ ⇒ **可与 T-k 同批或异批**，**不构成替代** | §5 的 T 表 |

⇒ ⇒ **放置 = 独立于 B3 的小批**（**不占 B3 末段**）⇒ ⇒ **理由**：R17.2 要求的同批项
（examples 的 tsv + `examples-rtl-expectations.md` + 10 个 TB + 登记表 + 锚点）**全部**落在
`examples/**` 与 `.rddf/checks/**` ⇒ ⇒ **不写 `include/cpu/**`** ⇒ ⇒ **不碰时序** ⇒ ⇒
⇒ ⇒ **不违反 B3 → B4 硬约束**（§4.4）⇒ ⇒ ⚠️ **且 MUST 与 B3 顺序执行**（写点同区，见下）。

⇒ ⇒ ⚠️ **可回退性**：本批改动**自洽**（改名 + 删 tsv 行 + 锚点 + 改 TB **同批**）⇒ ⇒
⇒ ⇒ 若用户本意是「随 B3 同批」⇒ ⇒ 回退 = **一次 revert** ⇒ ⇒ **无沉没成本** ⇒ ⇒ ⇒ **风险可控**。

⇒ ⇒ ⚠️ **本条是 architect 裁量，不是「用户已裁决」** ⇒ ⇒ **裁决记录必须落盘**
（planner 段：`.rddf/roadmap/phase-6-next-steps.md` §5 的 R17.1 行 + `phase-6-rtl-productization.md` §4）
⇒ ⇒ **用户可随时推翻** ⇒ ⇒ 若推翻 ⇒ ⇒ R17.3（登记为独立立项）路径**仍然有效**。

⇒ ⇒ **实施面已核实**（不是推测）：

| 项 | 实测 | 结论 |
|---|---|---|
| 目标对象 | `examples/riscv-mini/src/rv32i_pipeline_top.h` 的 **32** 个端口声明（`:38`–`:82`） | ✅ 32/32 登记在 `.rddf/checks/port-naming-exempt.tsv`（行 98–129）⇒ ⇒ **身份 = `<文件>\t<__io 块序号>\t<声明文本>`** ⇒ ⇒ 改声明文本 ⇒ ⇒ **必须同步删那 32 行**（否则护栏反向红） |
| 具名后的产物名 | 实测（工具自测载体）`ch_in<ch_uint<8>> data_in{"data_in"}` ⇒ ⇒ 产物端口 = **`top_data_in`**（`<组件路径>_<名字>`） | ⇒ ⇒ 目标端口名 = `top_ext_clk` / `top_prog_we` / … / `top_dbg_load_use_detected` |
| 下游影响面 | `examples/riscv-mini/tb_rv32i_soc.sv` 现用 `.top_unnamed_input` / `.top_unnamed_output_N` **手工对照**（`:173` 起，**并带注释解释哪个是哪个**） | ⇒ ⇒ 这正是「用户写 TB 必须查 port map」痛点的**实证** ⇒ ⇒ 改名后 TB 变成 `.top_ext_clk(ext_clk)` ⇒ ⇒ **可读性收益直接可验** |
| 顺序是否保序 | ⚠️ **机制订正（oracle F3）**：产物端口序 = **node id 升序**（`src/codegen_verilog.cpp:410-448` · `std::sort` 按 `a->id() < b->id()`）⇒ ⇒ **不是**「`__io` 块内声明顺序」（后者仅因根 `__io` 在建模期最先构造而**近似重合**）⇒ ⇒ **纯改名不动 node id ⇒ ⇒ 位置稳定** | ⇒ ⇒ TB 的**位置对应**保持 ⇒ ⇒ 改后 MUST 逐位置比对（旧 `top_unnamed_output_N` ↔ 新 `top_<name>`）作为**独立判据** ⇒ ⇒ ⚠️ **该判据的前提是「不中插 / 不重排端口」** ⇒ ⇒ 若中途改动 `__io` 顺序 ⇒ ⇒ **位置对应失效 ⇒ ⇒ 判据作废** ⇒ ⇒ MUST 改为「按名逐条比对」 |
| 棘轮锚点 | `EXPECTED_EXEMPT_PORTS = 1484`（`scripts/verify_port_naming.sh:50`） | ⇒ ⇒ 消化 32 条 ⇒ ⇒ **1484 → 1452** ⇒ ⇒ **「递减」这一半首次真咬**（债首次减少）⇒ ⇒ ⚠️ **但「不增」那一半本来就活着**（§3.4）⇒ ⇒ ⇒ **本批的价值不靠「唯一推进力」** |

⇒ ⇒ **本批的价值重新挂靠**（oracle F2 订正）：**不是**「棘轮唯一推进力」⇒ ⇒ 而是
① **用户 ⑤ 的原始诉求在真实 CPU 上首次兑现**（`Rv32iPipelineTop` 的端口**全部**具名）⇒ ⇒
② **消掉「用户写 TB 必须查 port map」的痛点**（TB 从 `.top_unnamed_output_7` 变成 `.top_obs_data_addr`）⇒ ⇒
③ **不依赖 T-c（B3 实施）闭环** ⇒ ⇒ 与 B4 的顺带带名路径**并行**、互不阻塞。

⇒ ⇒ **与 B3 的关系**：⚠️ **写点有重叠，本文件首稿判「不重叠 ⇒ 可并行」是错的**（critic 第 1 轮 SHOULD_FIX-1 · 已核实）：
`firmware-load-chain-2026-11.md:197` 的**爆炸半径明确列有** `examples/riscv-mini/src/rv32i_pipeline_top.h`
（「端口契约变化（`prog_addr` 语义 + 可能新增加载端口）」）⇒ ⇒ **R17 与 B3 的 L1–L6 在该文件上同区**
⇒ ⇒ ⇒ planner 的 R16「同区 ⇒ MUST 与 B3 顺序执行」（`phase-6-next-steps.md:255`）**有源文件支撑，不是写法有误**。

⇒ ⇒ **正确的表述**：R17 与 B3 **MUST 顺序执行**（不是并行）⇒ ⇒ 本轮**先做 R17**（B3 尚未立项 ⇒ ⇒ B3 实施在数月之后）
⇒ ⇒ **B3 实施时 MUST 复核**：R17 已把这 32 个端口具名 ⇒ ⇒ B3 若新增加载端口 ⇒ ⇒ MUST 一并具名
（否则触发护栏 P1「新增端口 MUST 具名」⇒ ⇒ 红 ⇒ ⇒ 反而是好事 ⇒ ⇒ **护栏会替 B3 兜住**）。

⇒ ⇒ ⚠️ **两批在同一文件上的改动是**正交的**（R17 只改 `__io` 的**声明文本** + 名字串；
B3 改的是 `prog_addr` 的**语义 / 译码**）⇒ ⇒ ⇒ **不会互相覆盖** ⇒ ⇒ ⇒ **顺序执行无冲突风险**。

### 4.2 裁决 ②：**F6 缺口 / gpio 断言强度 / 端口面登记 —— **一律**不擅自实施**（待用户裁决）

**理由**：三条的正解都涉及**取舍**，不是「照着做就行」：

| # | 事项 | 为什么不能擅自做 |
|---|---|---|
| **R2 / F6** | 给工具的自测设计加一个 `ch_reg` ⇒ ⇒ 「逐字节比对」重新可判 | ⚠️ **有两条路径**（oracle F6 订正）：**①** 直接跑 `ctest -L verilator_e2e`（**不**必须动工具设计 ⇒ ⇒ 纯旁路 ⇒ ⇒ 但护栏新增 `BUILD_VERILATOR=ON` 环境依赖 ⇒ ⇒ **fail-closed 语义需先定**：无 verilator 环境该 exit 0 还是 exit 2）；**②** 给工具的自测设计加一个 `ch_reg`（⇒ ⇒ backend 才会产 `work_dir/top.v` ⇒ ⇒ 「逐字节比对」可判 ⇒ ⇒ 但**工具产物端口面变化** ⇒ ⇒ F3 / F8 的期望集要同步）⇒ ⇒ **两条各有取舍 ⇒ ⇒ 不擅自选** |
| **R3 / gpio** | 补 spec 层断言 | 需先确认**仓内有没有 gpio 的规范依据** ⇒ ⇒ 若无 ⇒ ⇒ 只能退「显式标注 impl-contract only」 |
| **R1 / 端口面** | 把「扁平顶层暴露 80 个内部 `ch_out`」**登记为已知残留** | 这是**用户 2026-11-11 已裁决**的现状（C2 撤回）⇒ ⇒ **不是待裁决项** ⇒ ⇒ 但**台账无行** ⇒ ⇒ 需用户同意「补登记」这一步 |

⇒ ⇒ **已立单** `.rddf/checks/decision-request-port-face-2026-11.md`（R1/R2/R3 + §4 自我订正）
⇒ ⇒ ⇒ **本轮不实施**，等裁决。

### 4.3 裁决 ③：**目标 ④（RV32I 层次化示例）= 独立立项**，不并入 B3 / B4

**理由**：写点是 `src/hierarchy_analysis.cpp` + `src/hierarchy_verilog.cpp`（**发射器 / 分析层**）
⇒ ⇒ 与 B3（`include/cpu/**` 时序）/ B4（`include/cpu/**` 组件接入）**写点不重叠**
⇒ ⇒ 且它有**自己的判别性探针需求**（构造真层次化语料）⇒ ⇒ ⇒ 载体 = 新 improvement 草案（`add-improve` HARD-GATE）。
⇒ ⇒ ⚠️ **范围 MUST 如实登记**（§3.3）：**不是**「发射器的静默退化要修」（默认 inline 是契约行为）
⇒ ⇒ 而是「**诉求 ④ 未兑现**」+「**跨 module 隐式端口合成尚未实现 ⇒ 分析层 fail-closed**」
⇒ ⇒ ⇒ **规模大于「加一行 `CH_HIERARCHY_LAYER`」** ⇒ ⇒ ⇒ **立项时 MUST 先做规模侦察**。

### 4.4 裁决 ④：**B3 → B4 不得乱序**（沿用复盘 #3 · 硬约束）

⇒ ⇒ **B3 的时序金标只算一遍** ⇒ ⇒ 反序 ⇒ 「178 条退休 / 6 次 load-use 停顿」**两次作废**。

---

## 5. 目前的任务是什么（**按可执行性排序**）

> ⚠️ **本节是快照** ⇒ 状态真相仍以 `phase-6.md` + `phase-6-next-steps.md` 为准。

| # | 任务 | 归属 | 前置 | 本轮状态 |
|---|---|---|---|---|
| **T-a′** | ✅ **复盘 #3 的 12 条漂移扫干净 + 任务清单刷新** | planner | — | ✅ **已提交**（`83cdb89` / `d6c731c`） |
| **T-k** | 🔴 **R17 = 首批消化 32 端口**（用户 ⑤ 的**首个真实消费**）⇒ ⇒ 棘轮豁免 1484 → **1452** | **builder** | — | 🎯 **本轮交付**（裁决 ① · **architect 裁量闭 R17.1**，依据见 §4.1.1 ⇒ ⇒ **用户可随时推翻**） |
| **T-b** | 🔴 **B3 立项**（`memory-timing-contract` + `firmware-load-chain` 两个 change）⇒ ⇒ 载体 = `openspec/changes/<name>/{proposal,design,tasks}.md` | planner → builder | T-a′ | ⬜ **未开工**（实测 `openspec/changes/` **只有** `archive/`）⇒ ⇒ ★ **B3 的真起点**（🚪 门 R5 + R6 已闭） |
| **T-c** | 🔴 **B3 实施**（T1+T3 同批 · T6 · T2/T4/T5 · L1–L6）⇒ ⇒ 出口 = **ADR-068 的 V1–V10 + ADR-069 的 V1–V15 全绿** + 时序 golden **手推重算** | builder | T-b | ⬜ 未开工（实测 D-TCM 仍 `aread` 组合读 ⇒ 「可推断 BRAM」未达成） |
| **T-l** | 🟡 **R1/R2/R3 裁决后落地**（端口面登记 / F6 判据 / gpio 断言） | 用户 → planner → builder | **用户裁决** | ⏳ **已立单 · 待裁决**（`decision-request-port-face-2026-11.md`） |
| **T-d** | 🟡 **R3**（P7 的 ②⑤ + Reject 粒度 → change 立项/归档 → ④ 台账行 15 闭口） | builder → architect | — | ⬜ 未开工 |
| **T-e** | 🟡 **R4**（P8 builder 步：删 `set_ab_verification` stub + golden 覆盖下限 + 两份文案订正） | builder | — | ⬜ 未开工（实测 stub 仍在 `include/simulator.h:95` / `src/simulator.cpp:529`） |
| **T-f** | 🟡 **R7 → R11**（P6-0 侦察 ⇒ ⇒ `FENCE` 语义裁决）⇒ ⇒ **planner 无权拍板 ISA 语义** | planner → architect | — | ⬜ 未开工（`FENCE` 在 `openspec/specs/` + `docs/adr/` **零命中**） |
| **T-g** | 🟢 **R8**（`ext_clk` 死端口）⇒ ⇒ 建议 **A2** | architect → **用户** | — | ⏳ **已立单 · 待裁决**（2026-11-12 09:51） |
| **T-h** | 🔴 **B4 实施**（C1 差分 → C2 ALU 编码归一 → C3/C4 组件接入） | builder | **T-c 已闭** | ⬜ 未开工（`Rv32iDecoder` / `Rv32iAlu` **仍未**在流水线实例化） |
| **T-m** | 🟡 **目标 ④ 的层次化示例 = 独立立项**（含「隐式端口合成」这堵墙的规模侦察） | planner → architect | — | ⬜ 未立项（裁决 ③） |
| **T-i** | 🔒 **R9**（P11 · ADR-036 物理移除） | builder | 日历锁 **2027-04-17** | 🔒 锁 |

⇒ ⇒ **T-k 是本轮唯一「与用户 ⑤ 直接相关」且「对象已定 + 无前置 + 判据可机械复算」的动作**
（依据 = architect 裁量，§4.1.1 ⇒ ⇒ **用户可随时推翻**；⚠️ **T-d / T-e 同样无前置** ⇒ ⇒ **不构成替代**，只是与 ⑤ 无关）
⇒ ⇒ **T-b 是 B3 的真起点**（文档门已闭 ⇒ 只差立项）⇒ ⇒ ⚠️ **T-k 与 T-b MUST 顺序执行**（写点同区）。

---

## 6. 后续要怎么做（**为什么**）—— 落点

| # | 事项 | 归属 | 载体 | 为什么 |
|---|---|---|---|---|
| ① | **首批消化 32 端口**（用户 ⑤） | **builder** | `examples/riscv-mini/src/rv32i_pipeline_top.h` + `tb_rv32i_soc.sv` + `port-naming-exempt.tsv` + `verify_port_naming.sh` 锚点 + `examples-rtl-expectations.md` | ⑤ 是**用户原始诉求**、**对象已拍板** ⇒ ⇒ 落地它 = 端口名第一次在**真实 CPU** 上兑现 ⇒ ⇒ 顺带把「用户写 TB 必须查 port map」的痛点**消掉**（TB 从 `.top_unnamed_output_7` 变成 `.top_obs_data_addr`）⇒ ⇒ 且**不依赖 B3 闭环** ⇒ ⇒ ⚠️ **价值不靠「棘轮唯一推进力」**（§3.4 的两半区分）⇒ ⇒ 而是「兑现 + 可读性 + 独立可交付」 |
| ② | **B3 两个 change 立项** | planner → builder | `openspec/changes/{memory-timing-contract,firmware-load-chain}-2026-11/` | ADR-069/068 已 Accepted · 公开面已登记 ⇒ ⇒ 🚪 门齐了 ⇒ ⇒ **只差立项** ⇒ ⇒ 这是「能上板 + 能装真程序」的**唯一路径** |
| ③ | **B3 实施** | builder | 两个 change + ADR-068 V1–V10 / ADR-069 V1–V15 | 交付用户诉求 ② + ⑦ 的**功能面** ⇒ ⇒ ⚠️ 时序 golden **MUST 手推重算**（禁抄仿真输出） |
| ④ | **B4 实施** | builder | 两个 change（C1–C4 + G1–G5） | 交付用户诉求 ⑥ + ⑨（重复实现收口 + ISA 缺口显式化） |
| ⑤ | **R1/R2/R3 裁决后落地** | 用户 → builder | `decision-request-port-face-2026-11.md` | 三条都含**取舍** ⇒ ⇒ planner / architect **不代拍** |
| ⑥ | **目标 ④ 独立立项** | planner → architect | 新 improvement 草案（`add-improve` HARD-GATE） | 裁决 ③ ⇒ ⇒ ⚠️ **不是「修发射器的静默退化」**（默认 inline 是**契约行为**·有测试钉着）⇒ ⇒ **缺口 = 诉求 ④ 未兑现 + 跨 module 隐式端口 fail-closed 那堵墙** ⇒ ⇒ ⇒ **规模比「加一行 `CH_HIERARCHY_LAYER`」大** ⇒ ⇒ 需独立判别性探针 |
| ⑦ | **「同一文档禁止双快照」护栏化** | planner → builder | 新护栏（类比 `verify_roadmap_fragments.sh` 的 vendored 渲染） | 复盘 #3 的发现 1/2/4/5/6/7 **全部**是这一类 ⇒ ⇒ 目前只靠人工 ⇒ ⇒ 复发概率高 |

---

## 7. 判据自检（**可复跑**）

```bash
cd /workspace/project/CppHDL
bash scripts/verify_flat_rtl_emit.sh          # 扁平产物护栏 ⇒ PASS（F6 = 「不适用」⇒ §3.1）
bash scripts/verify_port_naming.sh            # 命名棘轮 ⇒ PASS（豁免数 = 1452 · R17.2 后 · 审计时点为 1484 ⇒ §3.4）
bash scripts/verify_examples_rtl_sim.sh      # 10 个独立 TB ⇒ PASS（gpio 标 impl-contract only ⇒ §3.2）
bash scripts/verify_hierarchical_rtl.sh      # R1–R8 ⇒ PASS
bash scripts/verify_arch_ledger_residuals.sh # 残留登记双向 1:1（23 行）
bash scripts/verify_ledger_evidence.sh       # 台账证据 ⇒ PASS
bash scripts/verify_public_api_visibility.sh # 公开面 ⇒ PASS
bash scripts/verify_roadmap_fragments.sh     # AUTO 块守恒 ⇒ PASS
(cd build && ctest -N | tail -1)             # Total Tests: 263
grep -m1 "^- \*\*Status\*\*" docs/adr/ADR-069-mem-data-alignment.md   # ⇒ Accepted
grep -cE "^- \[ \] \*\*V[0-9]+\*\*" docs/adr/ADR-069-mem-data-alignment.md  # ⇒ 15
grep -cE "^- \[ \] \*\*V[0-9]+\*\*" docs/adr/ADR-068-dtcm-memory-timing-contract.md  # ⇒ 10
ls openspec/changes/ | grep -v archive       # ⇒ 空 ⇒ B3 未立项（§5 T-b）
grep -rc "CH_HIERARCHY_LAYER" include/cpu examples   # ⇒ 0,0 ⇒ RV32I 无成层示例（§3.3）
```

⚠️ **§3.3 的三个实测数值（`ok=true` / 1 module / 与扁平逐字节相同）**由一次临时探针得出，
探针**未落仓** ⇒ ⇒ 复核者**无法直接重跑** ⇒ ⇒ **verifier MUST 重跑一次并把探针落仓**
（载体建议 `tests/hier_rtl_products.cpp` 同族的一个 `Rv32iPipelineTop` 层次化条目 ⇒ ⇒ 一并产出判别性语料）。

---

## 8. 与既有文档的关系

| 文档 | 关系 |
|---|---|
| `2026-11-12-phase-6-rtl-track-review.md`（复盘 #3） | **上游快照**（T2 裁决后时点）⇒ ⇒ 本文件**取代**它作为「当前态」⇒ ⇒ 但**不删**（保留为历史） |
| `2026-11-12-phase-6-b2-midterm-review.md`（复盘 #2） | 历史快照 |
| `2026-11-02-hierarchical-rtl-output.md`（层次化主题文档） | **§7 残差台账**是本轨的登记面（23 行）⇒ ⇒ 本文件**不复制**台账状态格 |
| `phase-6-rtl-productization.md`（批次编排） | **不新增架构义务** ⇒ ⇒ 本文件只给边界与顺序 |
| `phase-6-next-steps.md`（planner 域） | **下游** = 任务内容 + 实施路径的权威载体（**R0–R17** ⇒ 本轮追加 **R18+**） |
| `.rddf/checks/decision-request-port-face-2026-11.md` | **本轮新立**（R1/R2/R3 待裁决 + §4 自我订正） |
| `docs/adr/ADR-069` · `docs/adr/ADR-068` | 两份 **Accepted**（B3 的裁决与契约）· 本文件是其上游复盘 |
| `.rddf/reviews/phase-6-rtl-track-review4-2026-11-13.md` | **本文件的审稿记录**（oracle 第 1 轮 **REJECT**（1 BLOCKER + 2 SHOULD_FIX + 4 NITS）⇒ ⇒ §3.3 / §3.4 / §4.1 / §4.2 已逐条订正 ⇒ ⇒ critic 第 1 轮 **APPROVE_WITH_CONDITIONS**（**0 BLOCKER** · 2 SHOULD_FIX + 2 NITS）⇒ ⇒ §4.1 并行段 + 依据 ②③④ 已再订正）<br>⚠️ **复盘 #3 的审稿记录是另一份** = `.rddf/reviews/phase-6-rtl-track-review-2026-11.md`（**对象不同 · 不得合并**） |
