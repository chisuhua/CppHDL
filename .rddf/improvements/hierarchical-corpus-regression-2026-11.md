---
name: hierarchical-corpus-regression-2026-11
priority: P2
source: 台账 §7 残留 1b（语料级回归缺口 · 2026-11-06 由 oracle 从残留 1 拆出）+ §7 残留 11（权威登记）+ roadmap `feat-hierarchical-rtl-output` §③ 后续立项清单（残留 1b 行）
phase: phase-5
category: test
type: chore
created_at: 2026-11-07
evidence: probe:docs/architecture/2026-11-02-hierarchical-rtl-output.md#7
---

# Improvement: 层次化路径的语料级回归（残留 1b）

- **Date drafted**：2026-11-07（**v2** —— v2 吸收 metis 第 1 轮 gap 分析的 18 条 finding）
- **Drafted by**：build-agent（planner 角色）
- **Status**：📝 草稿 v2（**待用户拍板** ⇒ 拍板后转 builder 流程）
- **权威登记**：台账 §7 残留 **1b**（= **语料级**回归缺口；★ **不是**残留 1 那个「用户面缺口」
  ⇒ ⇒ **用户面缺口 = P8**，**两者 MUST NOT 合并**）
- **裁决**：§11.1 **A4**（2026-11-05）⇒ 独立立项 · **不为此立新 ADR**
- ⚠️ **MUST NOT 并入 P8**：P8 的 done 判据是「公共 helper 可用」⇒ ⇒ **不含**给既有语料补层次化覆盖
  ⇒ ⇒ 并进去 ⇒ 按 P8 的判据交付后该缺口**静默留存** ⇒ ⇒ **独立 change**（台账 §7 残留 1b 行 +
  oracle O2）
- ⚠️ **口径待用户拍板**（见 §3.6）：台账 §7 残留 1b 的原文是「**行为**覆盖」⇒ ⇒ 本 change
  的**分层方案**（T0 产物级 + T1 行为级）⇒ ⇒ 是否接受「行为覆盖 = 精选结构类 + 产物级覆盖
  = 全语料栅栏」这个**双层口径**，还是 MUST 坚持「全语料逐拍比对」？

---

## 1. Why (背景与动机)

### 1.1 缺口：**层次化路径在既有语料上零覆盖**

台账 §7 残留 1b（2026-11-06 由 oracle 从残留 1 拆出）：

> **语料级回归缺口**：层次化产物在全仓测试语料上的行为覆盖 = 仅 T3-005 的 **5 个专用
> DUT**（`DUAL_BACKEND_TEST_PAIR` **不参数化粒度** ⇒ 既有语料**一律只跑扁平**）⇒ ⇒
> 层次化路径在**既有语料**上零覆盖

⇒ ⇒ 现状：**发射器 / 分析层 / 层次化 backend** 的任何改动，只能被 T3-005 那 **5 个
为它量身定做的 DUT** 抓住 ⇒ ⇒ 那 5 个 DUT 是「**按层次化的形状写出来的**」
⇒ ⇒ 它们**不会**携带既有语料的真实拓扑（AXI 矩阵 · RV32I stage · pipeline chain ·
cache wrapper · 模块链）⇒ ⇒ **既有语料里的真实拓扑缺陷对层次化路径不可见**。

⇒ ⇒ **真实痛点不是「少几条测试」，而是「层次化路径没有回归栅栏」**：改发射器 ⇒ 只有
5 个专用 DUT 报警 ⇒ ⇒ 真实拓扑（多层嵌套 / 兄弟边 / mem / bundle）**无人报警**。

### 1.2 ⚠️ 本轮 planner 的**实测普查** + metis 第 1 轮 ⇒ 方案被改写

⚠️ **naive 做法（「给 `DUAL_BACKEND_TEST_PAIR` 加粒度参数 ⇒ 全部 289 处调用都跑层次化」）
在实测普查后被证伪**：

| 事实 | 实测 | 后果 |
|---|---|---|
| **语料规模** | **66** 个源文件 · **289** 处 `DUAL_BACKEND_TEST_PAIR` 调用 · **132** 个不同 DUT 类型 | 改 macro 签名 = **289** 处调用点改动（**巨大 churn**） |
| **★ 只有 10 个 DUT 有子组件** | `CsrBankDut`/`ClintDut`/`Rv32iPcDut`（`tests/test_rv32i_arch_dual.cpp`）· `ImmConsistencyDut`（`tests/test_rv32i_stage_dual.cpp`）· `StageDut`/`ChainDut`（`tests/chlib/test_pipeline_dual.cpp`）· `ModuleValueTransferTop`/`ModuleChainTop`/`ModuleIoDirectionTop`（`tests/test_module.cpp`）· `ICachePrefetch`（`examples/riscv-mini/tests/test_cache_smoke.cpp`） | ⇒ ⇒ **122 / 132** 是**叶子 DUT** ⇒ ⇒ `ForceLayer` 下**仍只有 1 个 module**（root 恒为顶层 module）⇒ ⇒ **逐拍**等价上**结构性零判别力** |
| **零跨模块 `ch_mem`** | 全仓 `.instance().<mem>` 形态只有 **4** 处，**全部**在层次化专用测试（`tests/test_hierarchy_verilog.cpp:937` · `tests/test_hierarchy_friendliness_diagnosis.cpp:296` · `tests/hier_rtl_products.cpp:167` · `docs/usage_guide/05-component-design.md:163`） | ⇒ ⇒ dual-backend 语料**零**跨模块 mem ⇒ ⇒ 分析层的「跨模块 mem」硬拒绝（`src/hierarchy_analysis.cpp:543-553`）**不被语料覆盖** |
| **零成层声明** | `CH_HIERARCHY_LAYER` 出现的 **14** 个文件与 **66** 个 parity 文件的**交集为空** | ⇒ ⇒ 既有语料**一律不声明成层** ⇒ ⇒ `RespectDeclarations` 下**全 inline** ⇒ ⇒ **有判别力的粒度只有 `ForceLayer`** |
| **一个 DUT 已超 64bit 硬顶** | `CounterWide<96>`（`tests/test_dual_backend_64_bit.cpp:72`，顶层 `ch_out<ch_uint<96>>` 见 `:33`） | ⇒ ⇒ Verilator 合法拒绝（`include/core/verilator_backend.h:40`）⇒ ⇒ 既有 macro 已 WARN 跳过 verilator 半边 ⇒ ⇒ **不可入选** |
| **一个 DUT 已被专门覆盖** | `Rv32iPipelineTop`（`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）⇒ 已被 `tests/test_hierarchy_equivalence.cpp:1104`「the deepest design」覆盖；且 `InstrTCM`/`DataTCM` 持 `1<<20` 字 `ch_mem`（`include/cpu/pipeline/rv32i_tcm.h:53,100`）⇒ verilator 下 4 MB 展开数组 ⇒ 编译成本不可接受 | ⇒ ⇒ 归**后续项** |
| **DUT 类在匿名命名空间内** | `tests/test_rv32i_arch_dual.cpp:79` · `tests/chlib/test_pipeline_dual.cpp:88` · `tests/test_rv32i_stage_dual.cpp:91` ⇒ 全部 `namespace {` … `}` | ⇒ ⇒ 层次化用例**必须落在该文件内**（或把 DUT 抽到头文件 ⇒ **大 churn**）⇒ ⇒ 选「**同文件新增用例**」 |

⇒ ⇒ **结论**：残留 1b **MUST NOT** 做成「全语料逐拍铺开」⇒ ⇒ 那是「289 处 churn 换
122/132 零判别力 + CI 编译时间翻倍」⇒ ⇒ **MUST** 做成「**分层（产物级 + 行为级）**」。

### 1.3 ⚠️ **metis 第 1 轮的关键修正**：**「行为等价」与「发射器回归」是两件事**

⚠️ **v1 的隐藏偏差**：v1 把「逐拍等价」当成了本 change 的**唯一**机制 ⇒ ⇒ 但：

| 机制 | 能抓住 | 抓不到 | 相对成本 |
|---|---|---|---|
| **逐拍行为等价**（flat vs hier） | **语义**错误：兄弟边接错信号 · 端口方向互换 · clock/reset 接错 · 1 拍错位 · 宽度一致的错连 | —（这些正是 T3-005 那 5 个结构 DUT 的职责） | **高**（每 DUT **2 次** verilator 编译 + 链接 + 仿真） |
| **产物级检查**（发射成功 + lint + 端口/映射自洽 + module 数 + 确定性） | 分析层/发射器的**全部硬失败**：`ok=false` · 跨模块 mem 误判 · 自驱 `ch_in` · 宽度不一致 · bubbling 崩 · **module 名冲突/编号** · 端口集与 `signal_map` 不自洽 · lint 失败 · 确定性漂移 | lint 认可的**语义**错误（同上） | **低一个数量级**（**不需要** verilator 编译 / 链接 / 仿真） |

⇒ ⇒ **v2 ⇒ 两层方案**：

| 层 | 覆盖 | 抓什么 | 成本 |
|---|---|---|---|
| **T0 产物级** | **10 个复合 DUT**（`ForceLayer` 下真分层）⇒ ⇒ + **栅栏机制**（见 §2.4） | 发射器/分析层的**全部硬失败** | 低（无 verilator 编译） |
| **T1 行为级** | **3 个结构上有新意的 DUT**（T3-005 的 5 个专用 DUT **结构上未覆盖**的形态） | **语义**错误 | 中（每 DUT 2 次编译） |

⇒ ⇒ ⚠️ **「122 叶子 DUT 零价值」这个结论只对「逐拍**行为**等价」成立** ⇒ ⇒ 对**产物级**
检查**它们并非零价值**（分析层改了 ⇒ 叶子的端口集 / 冒泡 / 命名也会错）⇒ ⇒ v1 的
「122 叶子一律不覆盖」**过宽** ⇒ ⇒ v2 改为：**叶子 DUT 归「全语料产物级」批次（backlog）**，
首批只做**复合 DUT**（分层真的发生时才有增量）。

### 1.4 T1 的**结构类清单**（替换 v1 的「≥ 7 个」数字 —— v1 那个数字**没有判据来源**）

⚠️ metis finding：v1 的 A1「首批 ≥ 7 个 DUT」= 「**通过筛选的个数**」而不是「**够用的个数**」
⇒ ⇒ 既不能判「够了」也不能判「多了」⇒ ⇒ v2 改为「**每个结构类 ≥ 1**」：

| 结构类 | 它守什么 | 代表 DUT | T3-005 是否已覆盖 |
|---|---|---|---|
| **C1 双子组件各自带寄存器** | 多 module 划分 + 各自的状态推进 | `ImmConsistencyDut`（`tests/test_rv32i_stage_dual.cpp:225`）⇒ `ForceLayer` 下 **3 个 module** | ★ **否**（T3-005 的 `EqDeepTop` 是「中间层成层 + 叶子 inline」，**不是**「两个子各自成层」） |
| **C2 兄弟跨模块边** | LCA 中转 wire（`src/hierarchy_verilog.cpp:577-657`）+ **同类型多实例的 module 编号**（`:809-850`） | `ModuleChainTop`（`tests/test_module.cpp:117,132`）⇒ 语料里**唯一**有兄弟边的 DUT | ★ **否**（T3-005 的 `EqDeepTop` 有兄弟边但**无同类型多实例**） |
| **C3 root 持 `ch_mem` + 子组件带 AXI 端口** | mem **在模块内自用**、跨层只走端口的**合法**形态（对照 `src/hierarchy_analysis.cpp:543-553` 的拒绝） | `ICachePrefetch`（`include/cpu/cache/i_cache_prefetch.h:66`） | ★ **否**（T3-005 的 `EqMemTop` 是「mem 在成层 module 内」，**不是**「root 持 mem + 子组件带 AXI」） |

⚠️ **C1–C3 的共同点**：它们都是 T3-005 那 5 个专用 DUT **结构上未覆盖**的形态
⇒ ⇒ 「T1 = 3 个 DUT」**不是**「挑 3 个方便的」⇒ ⇒ 而是「**每个未覆盖的结构类各取 1 个**
⇒ ⇒ **P1–P5 与 T3-005 的 `EqTop`/`EqPipeTop` 同构**（单子组件 + 1 个可观测寄存器输出）
⇒ ⇒ 它们的**逐拍等价增量极小** ⇒ ⇒ ⇒ **P1–P5 只进 T0（产物级），不进 T1**。

### 1.5 判别力上限（MUST 登记 —— 台账已点名，本轮普查**逐条落到具体 DUT**）

| # | 陷阱 | 语料内实例（实测） | 处置 |
|---|---|---|---|
| **①** | **组合逻辑 DUT**（子组件 **0 reg** ⇒ 时序缺陷**结构性测不出**） | `ModuleValueTransferTop`（`tests/test_module.cpp:85`）· `ModuleChainTop`（`:117`）· `ModuleIoDirectionTop`（`:167`）⇒ 子组件 `SimpleModule<4>`（`:14`）/ `ModuleDataProcessor`（`:151`）均 **0 reg 0 mem** | ⇒ C2 入选 ⇒ ⇒ MUST **登记**：「子组件的时序缺陷在本形态下不可测」⇒ ⇒ 判据只覆盖**连线语义** |
| **②** | **恒相关信号**（逐拍同值 ⇒ 「逐拍全等」分不清**两个信号被交换**） | `ImmConsistencyDut`：`dec_imm` ≡ `id_imm`（DUT 自身契约，`tests/test_rv32i_stage_dual.cpp:426` 断言相等）+ `equal` ≡ `dec_imm==id_imm`（`:257`）⇒ **3 个观测里 2 个冗余**<br>`CsrBankDut`：`interrupt_req` = `timer_interrupt_pending \| external_interrupt_pending`（`include/cpu/riscv/rv32i_csr.h:243`）⇒ 1 个观测是另 2 个的确定函数 | ⇒ C1 入选但 MUST 登记上限：`dec_imm`/`id_imm` 互换**不可判**；`equal` 冗余 ⇒ ⇒ 观测集 MUST **显式声明**哪些信号「可互换不可判」⇒ ⇒ **判据 MUST NOT 声称「全等 ⇒ 无缺陷」** |
| **③** | **顶层只有 1 个输出** ⇒ 「观测集合内部互换」这一维**不存在** | `Rv32iPcDut` · `StageDut` · `ChainDut` · `ModuleChainTop` · `ModuleValueTransferTop` · `ModuleIoDirectionTop` | ⇒ MUST 登记（「不是缺陷，但要写明上限」）⇒ ⇒ ★ **C2 的 `ModuleChainTop` 也在此列**（v1 只写了 P2/P4/P5 ⇒ **v2 补齐**） |
| **④** | **观测 ≡ 激励**（信号被逐字复制 ⇒ 不携带信息） | `DCacheWriteBuffer`（`include/cpu/cache/d_cache_write_buffer.h:120-122`）· `DCacheSync` · `chlib::DCacheAsync` ⇒ **均不入选**（且它们是叶子 ⇒ 逐拍零判别力） | ⇒ 不入选 |
| **⑤** | **观测值本身带已知偏差** | `ICachePrefetch`：内层 `miss`(1 拍 `sread`) 与 `axi_addr`(当拍地址) 存在 1 拍错位（`include/cpu/AGENTS.md` 的 cache 债务登记）⇒ **比对时 MUST NOT 把它当缺陷** | ⇒ C3 入选但 MUST 登记 |
| **⑥** | **>64bit 端口** ⇒ Verilator 合法拒绝 ⇒ 判据空转 | `CounterWide<96>`（`tests/test_dual_backend_64_bit.cpp:72`） | ⇒ **不入选** |
| **⑦**（v2 新增） | **同类型多实例的 module 编号**（发射器按 `sanitize(module_type_name)` 分组并编号 ⇒ 产出 `<Type>` / `<Type>_1` 两份同体 module） | `ModuleChainTop`（`mod1`/`mod2` 都是 `SimpleModule<4>`）⇒ `src/hierarchy_verilog.cpp:809-850` | ⇒ C2 的**真正判别点**（分组 + 编号）⇒ ⇒ MUST 登记 ⇒ ⇒ v1 把 C2 的价值说成「LCA 中转 wire」⇒ **两者都要写进登记表** |
| **⑧**（v2 新增） | **字面量跨模块边**（literal 无组件归属 ⇒ 归顶层 + CHWARN ⇒ 边是「父→子」⇒ 合法但贡献告警垃圾） | `ImmConsistencyDut` 的 `ids.io().pc <<= ch_uint<32>(0_d)` 等（`tests/test_rv32i_stage_dual.cpp`）⇒ 语料里**唯一**的「跨模块常量」实例 | ⇒ C1 入选但 MUST 登记（它是**价值**不是**噪音**） |
| **⑨**（v2 新增） | **未驱动子 `ch_in` 冒泡成顶层输入**（**无 C++ 句柄** ⇒ 只能靠 Verilator 默认 0 ⇒ ⇒ 两侧「碰巧相等」⇒ ⇒ 正是根 `AGENTS.md` 反复警告的「未驱动端口读到未定义值 ⇒ 碰巧通过」） | `StageDut`/`ChainDut` 的子 `PipelineStage::clk` 未驱动（`tests/chlib/test_pipeline_dual.cpp:96-120`） | ⇒ MUST 登记 ⇒ ⇒ 且 MUST 加「顶层输入集合守恒」判据（§3.3）⇒ ⇒ **MUST NOT** 靠「碰巧相等」 |

---

## 2. What Changes (变更范围)

### 2.1 新增 1 个**语料级层次化回归** fixture（**不**改既有 `DUAL_BACKEND_TEST_PAIR`）

⚠️ **不改既有 macro 的理由**（v2 改写 —— v1 把它写成「硬约束」，metis 指出那是**结论**而非约束）：

⇒ ⇒ 真正成立的理由 = 「既有 macro 的 body 是**扁平后端语义**（`sim` 是 interpreter 或
verilator 后端，body 直接读 `dev.instance().io().xxx`）⇒ ⇒ **观测集无法从 body 推断**
⇒ ⇒ 层次化比对 MUST 有**独立的观测集 + 独立的 stimuli** ⇒ ⇒ **无法复用既有 body**
⇒ ⇒ ⇒ **与「289 处 churn」无关**」。

⇒ ⇒ **形态**：**新增**一个 fixture 头（`tests/hier_corpus_fixture.h` · 形态见 §2.4）⇒ ⇒
**只**在 §1.4 / §1.5 的 DUT 所在**同文件内**新增用例 ⇒ ⇒ ⇒ 既有 DUT 定义**零改动**
⇒ ⇒ ⇒ 既有 289 处调用**零改动** ⇒ ⇒ ⇒ `DUAL_BACKEND_PARITY_PAIRS: 289` /
`DUAL_BACKEND_PARITY_TOTAL: 66` **两个锚点自然不变**（⇒ 因为新用例**不写**这个 macro
⇒ ⇒ 护栏 `scripts/verify_dual_backend_parity.sh` 数的就是这个 macro 的调用数）。

### 2.2 粒度：`ForceLayer`（**唯一有判别力的选择**）+ **端口集的精确口径**

⚠️ **v2 改写**（v1 的措辞两种读法都成立 ⇒ ⇒ 实现者极易去观测子组件端口）：

| 端口 | 扁平侧 | 层次化侧（`ForceLayer`） | 规则 |
|---|---|---|---|
| **根 `io()` 的 `ch_in`** | 顶层端口（`src/codegen_verilog.cpp:93-101` 只过滤「有 driver 的 input」） | **恒为顶层端口**（`include/hierarchy_verilog.h:136-139`：`backend_port_set` 只取根 module 端口，且 MUST 含 clock/reset） | ⇒ ⇒ `DrivenInput` **只能**取根 `ch_in` |
| **根 `io()` 的 `ch_out`** | 顶层端口 | **恒为顶层端口**（同上） | ⇒ ⇒ `Observation` **只能**取根 `ch_out` |
| **子组件的 `ch_out`** | **多出**为顶层端口（扁平发射器把全 ctx 的 `type_output` 都当顶层端口） | **不在**顶层（移进子 module ⇒ ADR-058 §4 风险 6 的盲区） | ⇒ ⇒ **MUST NOT** 观测 |
| **未驱动的子 `ch_in`** | 顶层输入（默认 0） | 冒泡成根 module 输入（**无 C++ 句柄** ⇒ 只能默认 0） | ⇒ ⇒ **MUST** 登记（§1.4 ⑨）⇒ ⇒ **MUST NOT** 靠「碰巧相等」 |

⇒ ⇒ **fixture 契约**（MUST 写进 `tests/hier_corpus_fixture.h` 的注释）：
- `DrivenInput` **只能**取根 `ch_in`；`Observation` **只能**取根 `ch_out`
- ⇒ ⇒ 且 MUST 通过 `signal_map[i].hier_top_port_name` **非空**机械断言（⇒ ⇒ 观测的信号
  **真的**在层次化顶层可见 ⇒ ⇒ 堵住「观测了一个不可见信号」）
- ⇒ ⇒ **禁忌**：根 `ch_in` **MUST NOT** 被内部驱动（`io().x <<= ...`）⇒ ⇒ 那会让它在
  **两侧**都从顶层消失 ⇒ ⇒ 反向破坏端口集（既有语料无此写法 ⇒ 新用例也不许写）

⇒ ⇒ **其它必须登记的副作用**（v2 新增）：
- **`ForceLayer` 的 `CHWARN` 不可断言**：回落 `typeid` 的告警由**per-instance** 成员
  `force_layer_warned_`（`include/component.h:246`）控制 ⇒ ⇒ **不复位** ⇒ ⇒ 同一 DUT 的
  第二次 emit 起**不再告警** ⇒ ⇒ 且若同进程内别的用例先以 `RespectDeclarations` 调过
  `module_type_name()` ⇒ ⇒ **`ForceLayer` 根本不告警** ⇒ ⇒ ⇒ **任何「MUST 有 CHWARN」的
  判据都不稳定** ⇒ ⇒ **MUST NOT** 写这种判据。
- **两侧 MUST 各自走完整初始化**（含 reset pulse）：扁平侧走 `VerilatorBackend::initialize`
  （`src/core/verilator_backend.cpp:464`）· 层次化侧走 `try_initialize_with_verilog`
  （`:689`）⇒ ⇒ **MUST NOT** 复用同一 backend 实例或跳过初始化（否则「INIT 未加载 ⇒
  第 1 拍就分歧」）。
- **`ForceLayer` 下 module 名回落 `typeid`**（`src/component.cpp:174-194`）⇒ ⇒ module 名
  **实现相关**（同一编译器 / 同一 ABI 内才可比）⇒ ⇒ 判据 MUST 只在**同一次运行内**比较
  ⇒ ⇒ **MUST NOT** 跨编译器比对产物字节。

### 2.3 落地形态：**T0 / T1 两层**（v1 的「档 1 / 档 2」分档前提**是事实错误** ⇒ 已删）

⚠️ **v1 事实错误**：v1 说「`examples/` 的测试**不在** `ctest` 里」⇒ ⇒ **实测证伪**：
`riscv_mini_cache_smoke` **在** ctest 里（`ctest -N` = **Test #16** · `examples/riscv-mini/CMakeLists.txt:147-150`
带 `LABELS "base;verilator_e2e"` + `RESOURCE_LOCK "verilator_backend"` + `TIMEOUT 600`）
⇒ ⇒ ⇒ **P8（`ICachePrefetch`）与 P1–P6 的 CI 接线无本质差别** ⇒ ⇒ v1 的「P8 推迟」理由
（伪事实）**不成立**。

| 层 | DUT | 所在文件 | 形态 |
|---|---|---|---|
| **T0**（产物级 · 10 个复合 DUT） | `CsrBankDut` · `ClintDut` · `Rv32iPcDut`（`tests/test_rv32i_arch_dual.cpp`）· `ImmConsistencyDut`（`tests/test_rv32i_stage_dual.cpp`）· `StageDut` · `ChainDut`（`tests/chlib/test_pipeline_dual.cpp`）· `ModuleValueTransferTop` · `ModuleChainTop` · `ModuleIoDirectionTop`（`tests/test_module.cpp`）· `ICachePrefetch`（`examples/riscv-mini/tests/test_cache_smoke.cpp`） | ⇒ ⇒ **4** 个 `tests/` 文件 + **1** 个 `examples/` 文件（★ v1 写的「6 个 `tests/` 文件」**错** ⇒ ⇒ P1–P3 同文件 · P4–P5 同文件） | 每个文件内新增 1 个**产物级**用例（**不需要** verilator 编译） |
| **T1**（行为级 · 3 个结构类各 1 个） | C1 `ImmConsistencyDut` · C2 `ModuleChainTop` · C3 `ICachePrefetch` | 同上 3 个文件 | 每个文件内新增 1 个**行为级**用例（**需要** verilator 编译 ⇒ 每 DUT **2 次**：flat + hier） |

⇒ ⇒ ⚠️ **T0 与 T1 的 DUT 有重叠**（T1 的 3 个也在 T0 的 10 个里）⇒ ⇒ **同文件内两个用例**
⇒ ⇒ ⇒ 这正是「一个文件一个 target ⇒ `TIMEOUT` 按 target 调」的来源（§3.4）。

### 2.4 ⚠️ **栅栏机制**（v2 新增 —— 这才是「回归栅栏」与「一批一次性测试」的分界）

⚠️ **metis 第 1 轮的核心发现**：v1 的 C2 只核对「登记表 ↔ 已写用例」⇒ ⇒ **检不出**「新加了
一个带子组件的 DUT 但没人登记」⇒ ⇒ 半年后语料多了 20 个 DUT，栅栏仍只有 10 个，且
**没有任何红灯** ⇒ ⇒ ⇒ **固定清单是 snapshot，不是 fence**。

⇒ ⇒ **v2 ⇒ 登记表升级为双向核对 + 豁免表**（形态同仓内既有：
`scripts/verify_input_port_required_registry.sh` / `scripts/verify_header_self_contained.sh` 的
豁免名单双向核对）：

| 核对方向 | 判据 |
|---|---|
| **登记表 → 实际** | 登记的每个 DUT ⇒ MUST 有对应的层次化用例 ⇒ 否则红 |
| **实际 → 登记表** | 每个层次化用例 ⇒ MUST 在登记表里 ⇒ 否则红 |
| **★ 语料 → 登记表** | **所有**含 `ch_module<` 的 dual-backend DUT ⇒ MUST 在登记表里（或在**显式豁免表**里 + 原因）⇒ ⇒ 否则红 ⇒ ⇒ ★ **这条才是让栅栏不随时间退化**的机制 |

⇒ ⇒ **豁免表**的每条 MUST 有原因 ⇒ ⇒ ⇒ 防止「豁免表变成垃圾桶」。

### 2.5 **登记表**（本 change 的**核心交付物之一**）

新增 `tests/hier_corpus_registry.md`（★ v2 **写死**为 `.md` ⇒ ⇒ 因为「判别力上限」是自由文本
⇒ ⇒ ⇒ **机器可读**的部分单独放一个**数据文件**（如 `tests/hier_corpus_registry.tsv`）
⇒ ⇒ ⇒ 护栏核对**数据文件** ↔ 实际用例 ⇒ ⇒ ⇒ **自由文本**由人读、**事实字段**由机器核）。

| 列（机器可读） | 内容 |
|---|---|
| DUT 类型 + 语料文件:行号 | 定位 |
| `ch_module` 子组件数 | `0` ⇒ 叶子（⇒ 逐拍零判别力 ⇒ 归 T0-only）· `≥1` ⇒ 复合 |
| `ch_mem` 形态 | 有无 · 是否跨模块 |
| 时序 / 组合 | 决定「时序缺陷是否可测」 |
| 顶层 `ch_out` 数 + **哪些随激励变化** | 决定观测集 |
| 顶层输入集合 vs 根 `ch_in` 集合的差 | 冒泡端口（§1.4 ⑨）⇒ 逐项登记「不可由 C++ 驱动」的理由 |
| **T0 / T1 覆盖标记** | 哪几层被覆盖 |

| 列（自由文本 · 人读） | 内容 |
|---|---|
| **判别力上限** | §1.4 的 ①–⑨ 中适用的项 |
| 已知的观测值偏差 | 如 `ICachePrefetch` 的 1 拍错位 |

⇒ ⇒ **维护者与触发条件**（v2 新增 · metis finding）：**谁**在新增语料 / 新增 `ch_module` DUT
时更新 ⇒ ⇒ **护栏**会提醒（§2.4 的「语料 → 登记表」方向）⇒ ⇒ ⇒ 文档 MUST 写明。

### 2.6 护栏

- **新增**「§2.4 的三向双向核对」护栏 + `--self-test` 投毒（★ 没有投毒 ⇒ 恒绿 ⇒ ⇒ 护栏无判别力）
- ⚠️ **既有护栏不动**：`scripts/verify_dual_backend_parity.sh` 的两个锚点**自然不变**
  （新用例不写 `DUAL_BACKEND_TEST_PAIR`）⇒ ⇒ ⇒ 护栏 PASS

### 2.7 文档

- 登记「哪些既有语料已被层次化覆盖、哪些**故意没**覆盖（叶子 ⇒ 逐拍零判别力 · >64bit ⇒
  合法拒绝 · `Rv32iPipelineTop` ⇒ 已被覆盖 + 编译成本）**为什么**」
  ⇒ ⇒ ⚠️ **必须写明「未覆盖」是**已知边界**而非遗漏** ⇒ ⇒ 否则后人重复普查
  ⇒ ⇒ ⇒ **这正是台账的登记要求**（oracle N1 的同类诉求）
- ⇒ ⇒ **口径待用户拍板**（见 §3.6）

### 2.8 明确不做（Non-Goals）

- ✗ **不**给 `DUAL_BACKEND_TEST_PAIR` 加粒度参数（既有 body 是**扁平后端语义** ⇒ ⇒ 观测集
  **无法从 body 推断** ⇒ ⇒ 必须独立用例 ⇒ ⇒ 与 churn 无关）
- ✗ **不**对 122 个叶子 DUT 做**逐拍**比对（**结构性零判别力**）⇒ ⇒ 它们归「全语料产物级」
  **backlog**（T2）
- ✗ **不**覆盖 `CounterWide<96>`（>64bit ⇒ Verilator 合法拒绝 ⇒ 判据空转）
- ✗ **不**覆盖 `Rv32iPipelineTop`（已被 T3-005 覆盖 + TCM 的 4 MB 展开数组 ⇒ 编译成本不可接受）
- ✗ **不**把跨模块 `ch_mem` 纳入语料（语料里**零例** ⇒ ⇒ 那是 T3-005 专用 DUT 的职责）
- ✗ **不**复用既有语料的「手推期望 / 金标表」做**绝对正确性**断言（★ 那是另一个能力 ⇒ ⇒
  且与「逐拍全等 ≠ 正确」的立场冲突 ⇒ ⇒ v1 的 §1.3 把它当「入选理由」是**无关优势**⇒ ⇒ 已删）
- ✗ **不**宣称「逐拍全等 ⇒ 层次化路径正确」（★ 只判「层次化**变换**引入的分叉」⇒ ⇒ 两产物同源
  ⇒ ⇒ 同构缺陷仍会一起错 ⇒ ⇒ 归既有 parity + 手推期望值义务）
- ✗ **不**新增 ADR（§11.1 A4）
- ⚠️ **不**改台账 §7 的措辞（★ 台账 = **architect 域** ⇒ ⇒ planner 只在 feature 文件登记 ⇒ ⇒
  若口径要改 ⇒ ⇒ 归 architect 的 backlog ⇒ ⇒ **本 change MUST NOT** 单方面改）

---

## 3. Acceptance (验收判据 —— MUST 可勾选)

> ⚠️ 判据原则：**每条 MUST 可机械复算**（禁「看起来对」）。
> ⚠️ **每条 MUST 能被「用户少一个能力」触发**。

### 3.1 T0（产物级）覆盖

- [ ] **A1** **10 个复合 DUT** 全部有 T0 覆盖 ⇒ ⇒ **每个**都在其**同文件**内
      ⇒ ⇒ **改动的文件数 = 5**（4 个 `tests/` + 1 个 `examples/`）
- [ ] **A2** **★「分层真的发生了」守门**：每个接入的 DUT 在 `ForceLayer` 下的产物
      **含 ≥ 2 个 module** ⇒ ⇒ ⇒ **叶子 DUT 在此必红** ⇒ ⇒ 从结构上杜绝「接叶子充数」
      ⇒ ⇒ **数 module 的口径 MUST 写死**（★ metis：文本解析 `^module` 与 `port_provenance`
      去重 `module` **不等价**（后者漏掉无端口的子 module）⇒ ⇒ ⇒ MUST 用**发射器自己的
      module 清单**，不得自研文本解析器）
- [ ] **A2'** **判别力守门**（v2 新增）：每个 T0 DUT 的 module 边界上**至少有一条信号穿越
      ≥ 1 个 `ch_reg`** ⇒ ⇒ ⇒ **纯组合 passthrough**（如 `ModuleChainTop`）**只覆盖连线语义**
      ⇒ ⇒ ⇒ MUST 登记其「时序判别力 = 0」（⇒ 对应 §1.4 ①）
- [ ] **A3** 每个 DUT 的层次化产物**发射成功**（`ok=true`）且 `error` 为空
      ⇒ ⇒ ⇒ 撞硬拒绝（跨模块 mem / 自驱 `ch_in` / 宽度不一致 / 根带合成端口）⇒ ⇒ **红**
      ⇒ ⇒ ⇒ **零跨模块 mem** 由本条承担（★ v1 的 A5「grep 逐 DUT 确认」是**无效判据**
      ⇒ ⇒ grep 判不出「mem 与使用者是否同 module」⇒ ⇒ 已删）
- [ ] **A4** 层次化产物**≠**扁平产物（★ 正向证据：v1 的 A2 只数 module ⇒ ⇒ 缺「产物真的变了」）
      ⇒ ⇒ 且差异**落在 module 边界**（至少一条实例连接 / `assign`）
- [ ] **A5** 层次化产物**过 lint**（复用既有 `scripts/verify_hierarchical_rtl.sh` 的 lint 门
      ⇒ ⇒ **MUST NOT** 自研 lint 口径）
- [ ] **A6** 端口集**自洽**：`products.ports` 的 **input 集合 == 根 `io()` 的 `ch_in` 集合**
      ⇒ ⇒ 否则**逐项登记**差集（冒泡端口）+ 「不可由 C++ 驱动」的理由
      ⇒ ⇒ ⇒ **MUST NOT** 靠「未驱动端口两侧都默认 0 ⇒ 碰巧相等」（§1.4 ⑨）
- [ ] **A7** **确定性**：同一 DUT 两次发射 ⇒ 产物**逐字节相同** + 映射表**逐项相同**

### 3.2 T1（行为级）覆盖 · **每个结构类 ≥ 1**

- [ ] **B0** **结构类覆盖**：C1（双子组件各自带 reg）· C2（兄弟跨模块边 + 同类型多实例编号）·
      C3（root 持 `ch_mem` + 子组件带 AXI 端口）⇒ ⇒ **每类 ≥ 1 个 DUT**
      ⇒ ⇒ ⇒ **每个类 MUST 写清「它守什么」**（⇒ ⇒ 这是「够用」的判据，替代 v1 的「≥7 个」）
- [ ] **B1** 每个 T1 DUT 的**逐拍等价**成立（`equal=true` · `mismatch_count==0`）
      ⇒ ⇒ 且**观测集非空**
- [ ] **B2** **观测集覆盖度**：该粒度下**全部可见的根 `ch_out`** MUST 被观测 ⇒ ⇒ 否则
      MUST **显式豁免** + 理由 ⇒ ⇒ ⇒ **MUST NOT** 静默缩小到只剩 1 个
      （★ 既有 T3-005 记录过「O 缩水却不报错」的事故 ⇒ `tests/test_hierarchy_equivalence.cpp:297`）
- [ ] **B3** **至少一个信号随激励变化** ⇒ ⇒ ⇒ ⚠️ **这是新增工作量，不是既有能力**
      （★ 现有 `compare_products`（`tests/hier_equivalence.h:132-209`）**不校验观测集**
      ⇒ ⇒ 空 / 恒定观测集 ⇒ `first_mismatch` 为空 ⇒ `equal=true, ok=true` ⇒ **恒绿**
      ⇒ ⇒ 「`any_varies`」目前只存在于**测试体**（`tests/test_hierarchy_equivalence.cpp:486-500`）
      ⇒ ⇒ ⇒ 本 change MUST 把「两次 stimuli + `any_varies` + 观测集非空 + 根 `ch_out` 全覆盖/
      显式豁免」**上提到共享 fixture**（★ 顺带解决 §2.4 的 fixture 形态：把
      `ObsPlan` / `check_observation_surface` 从 `test_hierarchy_equivalence.cpp` 的**文件局部**
      代码上提）
- [ ] **B4** **负向对照（行为侧）**：破坏层次化产物 ⇒ 对应用例**转红**
      ⇒ ⇒ ⇒ **破坏点 MUST 位于被观测信号的通路上**（★ 破坏一个**未被观测**的信号仍会绿
      ⇒ ⇒ ⇒ 自证失败）
- [ ] **B5** **负向对照（发射器侧）**：★ **v2 新增** ⇒ ⇒ 一次性人工验证：把 `ForceLayer`
      **临时退化**成 `ForceInline`（或反转兄弟边合成方向）⇒ ⇒ 对应语料用例**必须红**
      ⇒ ⇒ ⇒ 证明「栅栏真的能抓住**发射器回归**」（★ B4 只证明「`compare` 非空转」）
      ⇒ ⇒ ⇒ 结果登记到 §7（★ 「还原源码 ≠ 复原」⇒ ⇒ 还原后 MUST **重建 + 复绿**）

### 3.3 登记表 + 栅栏

- [ ] **C1** 登记表的**机器可读字段**与**自由文本字段**分离（§2.5）⇒ ⇒ 且全部字段齐全
- [ ] **C2** **三向双向核对**（§2.4）：登记表 → 实际 · 实际 → 登记表 · **语料 → 登记表**
      ⇒ ⇒ ⇒ **「语料 → 登记表」是让栅栏不退化的机制** ⇒ ⇒ MUST 存在
- [ ] **C3** **豁免表**每条 MUST 有原因 ⇒ ⇒ ⇒ 防「豁免表变垃圾桶」
- [ ] **C4** 护栏有 `--self-test` 投毒 ⇒ ⇒ ⇒ **无投毒 ⇒ 恒绿 ⇒ ⇒ 护栏无判别力**
- [ ] **C5** 登记表**明确列出**未覆盖的类别及**原因**（122 叶子 ⇒ 逐拍零判别力 · >64bit ⇒
      合法拒绝 · `Rv32iPipelineTop` ⇒ 已被覆盖 + 编译成本）⇒ ⇒ ⇒ 防后人重复普查
- [ ] **C6** **维护者与触发条件**已写入文档（谁在新增语料 / 新增 `ch_module` DUT 时更新）

### 3.4 零回归 + CI 预算

- [ ] **D1** **既有 289 处 `DUAL_BACKEND_TEST_PAIR` 调用零改动** ⇒ ⇒ ⇒
      `DUAL_BACKEND_PARITY_PAIRS: 289` / `DUAL_BACKEND_PARITY_TOTAL: 66` **两个锚点不变**
      ⇒ ⇒ ⇒ `scripts/verify_dual_backend_parity.sh` PASS
- [ ] **D2** **既有 DUT 定义零改动**（只**新增**用例）⇒ ⇒ ⇒ `git diff` MUST NOT 触及任何
      DUT 类的 `describe()` / `__io(...)`
- [ ] **D3** `ctest -LE perf` 全绿 · samples 全绿 · examples 全绿
- [ ] **D4** **CI 时间预算**（★ v2 改写 —— v1 的锚点算错了）：
      - ⚠️ `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` **两个锚点都** **不变**（★ `add_test` 是
        **每 target 一条**（`tests/CMakeLists.txt:14`）⇒ ⇒ 新 `TEST_CASE` 加进**既有** target
        ⇒ ⇒ **不产生新 ctest 条目** ⇒ ⇒ ⇒ 只有**新增 target** 才 +1 且必须走
        `add_dual_backend_test`（继承 `verilator_e2e` / `RESOURCE_LOCK` / `_vl_path_env`）
      - ⚠️ **每 T1 DUT = 2 次 verilator 编译**（flat + hier ⇒ 两套产物 ⇒ 两个缓存 key）
        ⇒ ⇒ 冷缓存单次 21–130 s ⇒ ⇒ **冷缓存 target wall-clock MUST ≤ 其 `TIMEOUT` 的 1/2**
      - ⇒ ⇒ **按 target 提升 `TIMEOUT`**（★ `TIMEOUT` 是 **target 级** ctest 属性 ⇒ ⇒
        **MUST NOT** 写「每个新用例独立 TIMEOUT」—— 同一文件内的新 `TEST_CASE` **无法**单独设）
      - ⇒ ⇒ 实测冷缓存 wall-clock 并登记
- [ ] **D5** **verilator 不可用 ⇒ SKIP 而非红**：★ v2 新增 ⇒ ⇒ 新用例**不走**
      `DUAL_BACKEND_TEST_PAIR` ⇒ ⇒ **不会**自动 WARN-skip（`tests/dual_backend_fixture.h:158-166`）
      ⇒ ⇒ ⇒ 在无 verilator 的机器上（含 macOS ⇒ 根 `AGENTS.md` KNOWN LIMITATIONS）
      **会直接红** ⇒ ⇒ ⇒ MUST 显式 `ch_test::tool_available("verilator")` + 提前返回 / `SKIP`

### 3.5 文档

- [ ] **E1** 「哪些既有语料已被覆盖 / 哪些**故意没**覆盖 + 为什么」**已登记**
- [ ] **E2** MUST 明写「**逐拍全等 ≠ 层次化路径正确**」（★ 两产物同源 ⇒ ⇒ 同构缺陷一起错
      ⇒ ⇒ 归既有 parity + 手推期望值义务）⇒ ⇒ 与 P8 的 F1/F2 同款

### 3.6 ⚠️ **待用户拍板的口径**（**blocking**）

⚠️ 台账 §7 残留 1b 的原文是「**行为**覆盖」⇒ ⇒ 本 change 的 T0（产物级）+ T1（行为级）
双层方案 ⇒ ⇒ **T0 本身不满足「行为覆盖」** ⇒ ⇒ 三个选项：

| 选项 | 内容 | 代价 |
|---|---|---|
| **X1** | 接受「**行为覆盖 = 精选结构类（T1）+ 产物级覆盖 = 全语料栅栏（T0 + 豁免表）**」⇒ ⇒ ⇒ **需要改台账措辞**（★ architect 域 ⇒ ⇒ 归 architect backlog ⇒ ⇒ 本 change 只在 feature 文件登记） | 台账措辞与本 change 的 done 判据不一致 ⇒ ⇒ 需显式登记为「已知口径差」 |
| **X2** | 坚持「**全语料逐拍**」⇒ ⇒ ⇒ 需覆盖 132 个 DUT ⇒ ⇒ ⇒ 每 DUT 2 次编译 ⇒ ⇒ ⇒ **CI 时间不可接受**（且 122 个**零判别力** ⇒ ⇒ 纯浪费）⇒ ⇒ ⇒ 不可行 | 不可行 |
| **X3** | 本 change **只**做 T1（3 个 DUT · 行为级）⇒ ⇒ ⇒ 台账措辞**不动** ⇒ ⇒ ⇒ 但栅栏退化风险（无「语料 → 登记表」方向 ⇒ ⇒ 新 DUT 不被拦）⇒ ⇒ ⇒ 「snapshot ≠ fence」问题保留 | 覆盖窄 · 栅栏会退化 |

⇒ ⇒ **planner 建议 X1**（并显式登记口径差 ⇒ ⇒ 归 architect backlog）⇒ ⇒ **待用户拍板**。

---

## 4. Capabilities (契约影响)

| capability | delta |
|---|---|
| `hierarchical-corpus-regression` | **ADDED ×N**（新建 ⇒ 归档**禁** `--skip-specs`） |

⚠️ **规范所有者边界**：产物契约（层次化产物的结构 / 端口命名 / **等价性判据本体**）归
`rtl-hierarchical-output` ⇒ ⇒ 本 capability **只**管「**语料覆盖与判别力边界**」
（哪些既有语料 MUST 被层次化覆盖 · 登记表 MUST 记什么 · **判别力上限 MUST 登记**）
⇒ ⇒ **MUST NOT** 复述等价性判据本体 ⇒ ⇒ 两份规范并存必然漂移。

⚠️ 与既有 capability / change 的关系（**须在 delta 正文点名**）：

| 既有对象 | 本 capability 的关系 |
|---|---|
| `rtl-hierarchical-output` | 复用它的等价性判据（**不**复述） |
| `hierarchical-rtl-equivalence-2026-11`（T3-005，已归档） | ★ **互补**：T3-005 = **5 个专用 DUT**（判据本体）⇒ 本项 = **既有语料**（覆盖面 + 判别力上限 + **栅栏**）⇒ ⇒ **MUST NOT** 声称覆盖了 T3-005 的范围 |
| `hierarchical-equivalence-public-helper`（P8，**独立**） | ★ **MUST NOT** 合并 ⇒ ⇒ 本项用**仓内** harness（`tests/`）⇒ ⇒ P8 的公共面**不**是本项的前置 ⇒ ⇒ ⇒ 但若 P8 先落地 ⇒ ⇒ 本项可顺带改用**公共 API** ⇒ ⇒ 反而多一层「公共面够用」的验证 |

**建议的 delta Requirement 条目**（实施期定稿 ⇒ 数量以 design.md 为准）：

1. 层次化路径 MUST 在**既有语料**上有**可机械核验**的覆盖 ⇒ ⇒ 且 MUST 用
   「`ForceLayer` 下产物含 ≥ 2 个 module」作为「分层真的发生」的守门
2. 覆盖 MUST 有**登记表** ⇒ ⇒ 且登记表 ↔ 实际用例 MUST **三向双向核对**
   ⇒ ⇒ ★ 含「**语料 → 登记表**」方向 ⇒ ⇒ ⇒ 栅栏 MUST NOT 随语料增长而退化
3. **判别力上限 MUST 登记** ⇒ ⇒ 每条覆盖 MUST 记录其观测集的**上限**
   （恒相关信号不可判 / 组合形态时序缺陷不可测 / 顶层输出数 ⇒ 内部互换维度不存在 /
   观测 ≡ 激励 / 已知观测偏差 / 同类型多实例编号 / 字面量跨模块边 / 未驱动子 `clk` 冒泡）
   ⇒ ⇒ ⇒ **MUST NOT** 用一句话覆盖全部
4. 覆盖 MUST **显式登记未覆盖的类别及原因** ⇒ ⇒ ⇒ 防后人重复普查
5. 层次化行为等价 MUST 有**观测集非空 + 至少一个随激励变化的信号 + 全部可见根 `ch_out`
   覆盖或显式豁免** ⇒ ⇒ ⇒ **MUST NOT** 允许空 / 恒定 / 静默缩小的观测集（★ 现状
   `compare_products` 对此**恒绿** ⇒ ⇒ 这是**新增**能力，不是既有能力）

---

## 5. Impact (影响面)

| 面 | 影响 | 风险 |
|---|---|---|
| **既有语料** | **5** 个文件各新增 1–2 个用例（4 个 `tests/` + 1 个 `examples/`）⇒ ⇒ 既有 DUT 定义**零改动** · 既有 289 处调用**零改动** | 低（但须严守「不写 `DUAL_BACKEND_TEST_PAIR` 调用」⇒ ⇒ 否则锚点漂移） |
| **CI 时间** | T0 = **无** verilator 编译（纯产物级）· T1 = 每 DUT **2 次**编译（flat + hier）⇒ ⇒ **3 个 T1 DUT ⇒ 6 次冷编译**（单次 21–130 s）⇒ ⇒ ⇒ **MUST** 按 target 提升 `TIMEOUT` 并实测冷缓存 wall-clock | **中** ⇒ ⇒ 落 T1 的 3 个 DUT 分属 **3 个不同 target** ⇒ ⇒ 各自 +2 次编译 |
| **计数锚点** | `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / parity 两锚点 **全部不变**（★ 新 `TEST_CASE` 进既有 target ⇒ ⇒ **不产生新 ctest 条目**） | 低 |
| **护栏** | 新增「三向双向核对」护栏 + `--self-test` 投毒 + 接进 CI | 中 |
| **文档** | 登记覆盖范围与**未覆盖**原因 + 口径差（§3.6） | 低 |
| **发射器 / 分析层 / backend** | **零行为改动**（纯测试侧） | 无 |
| **与 P8 的关系** | **零耦合**（本项用仓内 harness）⇒ ⇒ ⇒ 可**并行**推进 | ⚠️ 若 P8 先落地 ⇒ ⇒ 本项改用**公共 API** ⇒ ⇒ 顺带证明公共面够用 ⇒ ⇒ ⇒ **建议 P8 先做** |

---

## 6. metis 第 1 轮 gap 分析的处置（逐条）

> metis 第 1 轮（2026-11-07）· **本 improvement 总计 1 轮**（预算用尽）。
> ⚠️ metis 输出的**事实断言**已**逐条实测复核**（不凭 subagent 结论直接采信）：

| metis 断言 | 实测复核 | 结论 |
|---|---|---|
| `riscv_mini_cache_smoke` **在 ctest 里**（Test #16 · `verilator_e2e` · `RESOURCE_LOCK` · `TIMEOUT 600`） | `ctest -N` ⇒ Test #16；`examples/riscv-mini/CMakeLists.txt:147-150` ⇒ LABELS/RESOURCE_LOCK/TIMEOUT | ✅ **成立** ⇒ ⇒ **v1 的「examples 不在 ctest」是事实错误** ⇒ ⇒ v2 删掉该前提 |
| `compare_products` **不校验观测集** ⇒ 空/恒定 ⇒ `equal=true` | `tests/hier_equivalence.h:132-209` 无观测集校验；`any_varies` 只在 `tests/test_hierarchy_equivalence.cpp:486-500`（**测试体**内） | ✅ **成立** ⇒ ⇒ B3 标为**新增工作量** + 列入 §4 Requirement ⑤ |
| `CTEST_TOTAL` 数的是 **target** 不是 TEST_CASE | `tests/CMakeLists.txt:14` 的 `add_test(NAME ${TEST_NAME})` = 每 target 一条；实测 `add_catch_test` 147 + `add_dual_backend_test` 59 + 其它 ⇒ 252 | ✅ **成立** ⇒ ⇒ v1 的 D4「+N」**错** ⇒ ⇒ v2 改为「**两个锚点都不变**」 |
| `force_layer_warned_` 是 **per-instance 且不复位** | `include/component.h:246` + `src/component.cpp:181-182`（置位后不再告警）· `:224`（拷贝构造复制）⇒ ⇒ **无复位路径** | ✅ **成立** ⇒ ⇒ §2.2 新增「`CHWARN` **不可断言**」 |
| 根 `io()` 的 `ch_in`/`ch_out` 在**层次化侧恒为顶层端口** | `include/hierarchy_verilog.h:136-139`（`backend_port_set` 只取根 module 端口 + MUST 含 clock/reset）· 既有 `EqDeepTop` ForceLayer 用例（`tests/test_hierarchy_equivalence.cpp:1030-1033`）用 `root.io().din` 驱动并通过 | ✅ **成立** ⇒ ⇒ §2.2 表格化端口口径 + fixture 契约 |

| # | metis finding | 处置 |
|---|---|---|
| **M1** | **P0 · 隐藏意图**：台账原文是「**行为**覆盖」，但真问题含「发射器对真实组件代码的回归」⇒ ⇒ 「行为等价」与「发射器回归」是两件事 ⇒ ⇒ 「122 叶子零价值」只对**逐拍**成立 | ✅ **采纳** ⇒ §1.3 的**两层方案**（T0 产物级 / T1 行为级）+ §1.2 结论句的修正 |
| **M2** | **P0 · 「≥ 7 个」没有判据来源** ⇒ ⇒ 换成「**每个结构类 ≥ 1**」 | ✅ **采纳** ⇒ §1.4 的 C1–C3 结构类清单 + B0 |
| **M3** | **P0 · §2.2 端口集口径两种读法** ⇒ ⇒ 写清「根 `io()` 恒在顶层；变的是**子组件**的 `ch_out` 与冒泡的子 `ch_in`」 | ✅ **采纳** ⇒ §2.2 的四行表格 + fixture 契约 + 「根 `ch_in` MUST NOT 内部驱动」禁忌 |
| **M4** | **P0 · §2.3 档 2 前提是事实错误**（P8 也在 ctest）⇒ ⇒ 「P8 推迟」理由不成立 | ✅ **采纳** ⇒ v2 删掉「档 2」概念 + 修正 P8 的归属 |
| **M5** | **P0 · D4 的锚点算错** ⇒ ⇒ `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` **都不变** | ✅ **采纳** ⇒ D4 重写 |
| **M6** | **P0 · B3 是新增工作量，未如实登记** | ✅ **采纳** ⇒ B3 标注 + §4 Requirement ⑤ + §2.4 的 fixture 形态（上提 `ObsPlan`） |
| **M7** | **P0 · 缺「栅栏能抓住**发射器回归**」的负向对照** ⇒ ⇒ B1 只证明 `compare` 非空转 | ✅ **采纳** ⇒ **B5**（临时退化 `ForceLayer`→`ForceInline` ⇒ 语料用例必须红） |
| **M8** | **P0 · 缺「层次化产物事实上 ≠ 扁平产物」的正向证据** | ✅ **采纳** ⇒ **A4**（产物不等 + 差异落在 module 边界） |
| **M9** | **P0 · 缺 CI 时间预算的**数值门**；`TIMEOUT` 是 target 级 ⇒ 「每个新用例独立 TIMEOUT」**不可实现** | ✅ **采纳** ⇒ D4 的「冷缓存 ≤ TIMEOUT/2」+ 「按 target 提升」 |
| **M10** | **P0 · 缺「verilator 不可用 ⇒ SKIP 而非红」**（新用例不走既有 macro ⇒ 不自动 skip） | ✅ **采纳** ⇒ **D5** |
| **M11** | **P0 · 缺「顶层输入全集」判据**（冒泡端口无 C++ 句柄 ⇒ 靠默认 0 ⇒ 碰巧相等） | ✅ **采纳** ⇒ **A6** + §1.4 ⑨ |
| **M12** | **P1 · 「固定清单是 snapshot 不是 fence」⇒ ⇒ 缺「语料 → 登记表」方向** | ✅ **采纳** ⇒ §2.4 的**三向**双向核对 + C2 |
| **M13** | **P1 · A2 是**选择**守门不是**判别力**守门** ⇒ ⇒ P7 纯组合也满足 A2 | ✅ **采纳** ⇒ **A2'**（module 边界上 ≥1 个 `ch_reg` 穿越）+ §1.4 ① 的登记 |
| **M14** | **P1 · B2「ForceInline 逐字节相同」在真实 DUT 上未验证** | ⚠️ **部分采纳**：v1 的 B2 是「`ForceInline` 下与扁平逐字节相同」⇒ ⇒ **改为「待验证风险」**（若红 ⇒ 先判是发射器缺陷还是预期差异 ⇒ ⇒ **MUST NOT** 直接当 AC 阻塞）⇒ ⇒ 且该条**不是** §1.4 的核心 ⇒ ⇒ **降级为 §7 的一次性核实项**（不进 §3 的阻塞判据） |
| **M15** | **P1 · A5「grep 逐 DUT 确认」是无效判据** | ✅ **采纳** ⇒ 删掉 A5 ⇒ ⇒ 「零跨模块 mem」由 A3 承担 |
| **M16** | **P1 · §2.5「锚点不变」是结论而非约束** ⇒ ⇒ 会诱导更笨的实现 | ✅ **采纳** ⇒ §2.1 改写为「真正成立的理由 = 既有 body 是**扁平后端语义** ⇒ 观测集无法推断」 |
| **M17** | **P1 · §1.3 把「手推期望 / 金标表」当入选理由是**无关优势** | ✅ **采纳** ⇒ 删除该理由 + §2.8 新增「不**复用**金标做绝对正确性断言」 |
| **M18** | **P1 · §1.4 ③「顶层只有 1 个输出」⇒ P7 也在此列** | ✅ **采纳** ⇒ §1.4 ③ 补 `ModuleChainTop` |
| **M19** | **P1 · 三处事实错误：「6 个文件」/「P7 在 examples」/「档 2 含 P7」** | ✅ **采纳** ⇒ §2.3 改写（**4 个 `tests/` + 1 个 `examples/`**）· P7 归 `tests/test_module.cpp` |
| **M20** | **P1 · §1.4 新增三条边界：同类型多实例编号 / 字面量跨模块边 / 未驱动子 `clk` 冒泡** | ✅ **采纳** ⇒ §1.4 ⑦⑧⑨ |
| **M21** | **P1 · §2.6 / E1 补「登记表维护者 + 触发条件」** | ✅ **采纳** ⇒ §2.5 末段 + **C6** |
| **M22** | **P1 · A4 的观测集覆盖度未定义**（⇒ 「O 缩水却不报错」的事故） | ✅ **采纳** ⇒ **B2**（全部可见根 `ch_out` MUST 被观测或显式豁免）|
| **M23** | **P1 · §2.4 登记表机器可读 vs 自由文本的二选一** | ✅ **采纳** ⇒ 拆成 `.tsv`（机器可读）+ `.md`（自由文本） |
| **M24** | **P1 · C2「登记 DUT 数 == 实际用例数」无稳定标识**（改名 / 一 DUT 多用例 / 计数相等可用错 DUT 凑） | ✅ **采纳** ⇒ 改用**三向双向核对**（按 DUT 类型 / 用例标识 ⇒ ⇒ 不靠计数相等） |
| **M25** | **P1 · A2 未说「怎么数 module」**（文本解析 ≠ provenance 去重；后者漏无端口 module） | ✅ **采纳** ⇒ A2 写死「用**发射器自己的** module 清单 ⇒ ⇒ **MUST NOT** 自研文本解析器」 |
| **M26** | **P1 · B1 未说破坏哪一处** | ✅ **采纳** ⇒ B4 写死「破坏点 MUST 位于被观测信号的通路上」 |
| **M27** | **P1 · 缺「examples 断言质量护栏」的兼容性核对**（`CH4_ANCHOR` 指向 `test_cache_smoke.cpp`） | ✅ **采纳** ⇒ 追加到 **D3**（`examples` 断言质量护栏 PASS ⇒ ⇒ 含其 `CH4_ANCHOR` 仍成立） |
| **M28** | **P2 · §5 的 CI 时间低估为「额外一次」** | ✅ **采纳** ⇒ §5 改为「每 DUT **2 次**」+ D4 的数值门 |
| **M29** | **P2 · P8 到底首批做不做自相矛盾**（§1.3 列了 · §2.3 说可推迟 · A1 只要 P1–P7） | ✅ **采纳** ⇒ 统一：**P8 = C3 = 首批做**（其「推迟」理由是伪事实 ⇒ ⇒ 已删）⇒ ⇒ T1 = C1/C2/C3 **三个都首批** |
| **M30** | **P2 · 登记表维护者 + 触发条件缺失** | ✅ **采纳**（同 M21） |
| **M31** | **P0 · Q3 的判断：产物级作为第 0 层是划算的，但**不满足**台账「行为覆盖」原文** ⇒ ⇒ 需用户拍板口径 | ✅ **采纳** ⇒ §3.6 的 X1/X2/X3 三选项 + **planner 建议 X1** + §2.8 的「不单方面改台账」 |
| **M32** | **P0 · Q1：根 `io()` 端口在层次化侧成立（仅根）** | ✅ **采纳**（已实测复核）⇒ ⇒ §2.2 的表格化口径 + fixture 契约 |
| **M33** | **P0 · Q2：B3 是新增能力** | ✅ **采纳**（同 M6） |

---

## 7. 实施期发现（如实登记）

（实施期追加 ⇒ ★ 只增不改本文件的决策内容。）

> ⚠️ **v2 预置**（★ 一次性核实项 · **不进** §3 的阻塞判据 · metis M14）：
> **「真实 DUT 在 `ForceInline` 下与扁平产物逐字节相同」尚未验证** ⇒ ⇒ T3-005 只对
> `EqInlineTop` 验过字节相等（`tests/test_hierarchy_equivalence.cpp:390-410`）⇒ ⇒
> bundle / mem / pipe 类真实 DUT **只验过行为、未验过字节** ⇒ ⇒ ⇒ 若 T0 实测发现**不等**，
> ⇒ ⇒ 先判「是发射器缺陷」还是「预期差异」⇒ ⇒ **MUST NOT** 直接当缺陷报出 ⇒ ⇒ ⇒ 登记结论。
