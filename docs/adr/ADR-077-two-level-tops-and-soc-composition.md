# ADR-077: RV32I 的**两级顶层**（IP 级 / SoC 级）与 SoC 组合的四条形态裁决

- **Status**: **Accepted**（2026-11-23 · 审稿 = oracle 1 轮 + critic 3 轮全闭；轮次与 BLOCKER 记录见 §审稿记录）
- **Date**: 2026-11-23
- **Author**: architect role（build agent · 用户 2026-11-23 指令「选择 A」= 过 S0 决策门）
- **Parent**: **ADR-046**（微架构唯一归属）· **ADR-061**（成层粒度显式声明）· **ADR-070**（RV32I 成层 · D3 模块类型名 · D4 条件③）· **ADR-072**（产品端口面 D1）· **ADR-073**（SoC 产物形态 D1/D2/D3）· **ADR-074**（验证单一真相源）
- **关联 change**: `openspec/changes/riscv-soc-skeleton-2026-11/`（**已立项** · 本 ADR 是其 **S0 决策门**）
  ⇒ ⇒ **AC 映射** = 该 change 的 **AC2 / AC3 / AC4 / AC6 / AC9**（module 集 / 根端口集 / 逐拍等价 / 核端口契约 / 登记 `kind`）
- **上游规划**: `.rddf/improvements/rv32i-soc-platform-2026-11.md`（S1–S3 · metis 第 1 轮已处置）
- **审稿记录**: `.rddf/reviews/ADR-077-two-level-tops.md`（oracle → critic，顺序不可颠倒；critic 第 3 轮 = 用户批准的第 3 轮窄复审）
- **Amends**: ADR-046 §D1（dated addendum · **2026-11-23 已落地**于该 ADR D1 第 1 类条目的行尾追记；无决策变更）
- **超长自检**: 正文 < 250 行 · 每个 Decision 子节 ≤ 40 行 · 推导与实测数据见 §2.2 / 关联 change 的 proposal

---

## 1. 背景与问题

### 1.1 触发

用户 2026-11-22/23 的目标：把 RV32I 核做成**真实可替换单元**，后续加 L1/L2、MMU、内存控制器、总线；
并明确「`riscv-mini` 可看成 `riscv-soc` 的 spike」「`riscv-mini` 改造成核复合体，另建 `examples/riscv-soc`」。

planner 已立项（`.rddf/improvements/rv32i-soc-platform-2026-11.md` + `riscv-soc-skeleton-2026-11`），
并把 **S0 决策门 G-1…G-4** 留给出 architect。本 ADR 就是这四条裁决。

### 1.2 与既有签批决策的**接缝**（这就是为什么要 ADR，而不是 planner 直接拍）

| 既有决策 | 原文 | 与本目标的张力 |
|---|---|---|
| **ADR-046** §D1 | 「`include/cpu/` 是微架构唯一权威。`examples/riscv-mini/src/` 收敛后**只留 SoC 顶层**」 | 本目标产生**第二个** example 侧顶层（SoC 根） |
| **`rtl-hierarchical-output`**（capability · 用户 2026-11-15 裁决） | 「交付粒度下产物 MUST 含 **8 个 module**（7 stage + 顶层 `top`）」· 「模块类型名**不是**契约」 | 新 SoC 根的产品是 **10 module** ⇒ 该条款的**作用域**必须先钉死，否则"两个数字互相打架" |
| **ADR-070 D3** | 「7 个模块类型名 = 产物级契约名」 | 已被上条 capability 的用户裁决改为**明文非契约** ⇒ 本 ADR 必须按**非契约**处理新名字 |
| **ADR-073 D1** | 「SoC 产物 module 数 == 成层 IP 数 + 1」 | 与本目标一致 ⇒ 新 SoC 根直接适用 |
| **ADR-072 D1** | 产品端口面 = 框架 2 + **根自有 IO**；后代 `ch_out` 一律降为体内 `logic` | ⇒ 核成子 module 后，`dbg_*`/`obs_*` **不再**是顶层端口 ⇒ SoC 级观测面必须重新设计 |

### 1.3 问题（四条 · 精确表述）

| 门 | 问题 | 不裁的后果 |
|---|---|---|
| **G-1** | 两个 example 侧顶层（IP 级核复合体 / SoC 级根）的**分工与判据作用域** | ADR-046 被读成"禁止第二个顶层" ⇒ S1 无法立项；或 8/10 module 两条判据互相打架 |
| **G-2** | 新模块类型名（`Rv32iCoreComplex` / `SocTraceCounter`）的**契约强度与登记载体** | 名字被误当契约 ⇒ 改名成本被高估；或名字无登记 ⇒ 产物 module 集判据无权威 |
| **G-3** | 固件装载口 `prog_*` 的**归属与生命周期** | S3 若删根端口 ⇒ 产品端口面破坏性变更（ADR-072 D1 的兼容性断裂） |
| **G-4** | 新 SoC 产品的 **`kind` 登记口径** | 守恒等式（`29 == 17 ∪ 12`）与锚点不可判 |

---

## 2. 决策

### 2.1 **D1（G-1）** = 两级顶层分工 + 判据作用域

> **`examples/riscv-mini/src/rv32i_pipeline_top.h` = IP 级 top（核复合体）；
> `examples/riscv-soc/src/rv32i_soc_top.h` = SoC 级 root。两者是两个产品单元，不是两份实现。**

- **ADR-046 的解释（dated addendum）**：其 D1 的**意图**是消除「`examples/riscv-mini/src/` 与
  `include/cpu/` 的**同名重复**」（同 §1.2 的原始症状），**不是**「全仓只许一个 example 侧顶层」。
  ⇒ ⇒ 本 ADR 以 addendum 明确：**微架构组件仍 MUST 住 `include/cpu/`**（原意保留）；
  新增的 SoC 根只做**装配**，MUST NOT 复制任何微架构实现。
- **判据作用域（关键）**：
  | 产品 | 判据 | 数字 |
  |---|---|---|
  | **核复合体**（`Rv32iPipelineTop`） | `rtl-hierarchical-output` 的「交付粒度 8 module」 | **8**（7 stage + `top`）· **不变** |
  | **SoC 根**（`Rv32iSocTop`） | **ADR-073 D1**「module 数 == 成层 IP 数 + 1」 | S1 = 9 个成层 IP + 根 = **10** |
  ⇒ ⇒ 「8」**只**约束核复合体；SoC 根**从不**受「8」约束 ⇒ 两条判据**不打架**。
- **根自有的观测口（≥2 · 这是签批 spec 的硬要求）**：`openspec/specs/rtl-soc-output` 明文要求
  SoC 交付物的观测集 MUST 含 **≥ 2 个**随激励变化的量（粘滞完成标志 MUST NOT 计入 · 激励 MUST ≥10 拍且值不同）
  ⇒ ⇒ S1 的根自有输出 = **两个计数器**（同属一个 IP `SocTraceCounter`）：

  | 根端口 | 计数事件（IP 输入） | 手推变化 |
  |---|---|---|
  | `store_count[15:0]` | `core.io().obs_data_write_en` | 0 → 1（程序里有 1 条 `sw`） |
  | `writeback_count[15:0]` | `core.io().dbg_wb_write_en` | 0 → **≥2**（写回条数按手推台账） |

  ⇒ ⇒ 两者都是**单调计数**（不是"置位后不清零的完成标志"）⇒ 不落入粘滞排除条款
  ⇒ ⇒ `min_varying_observations` MUST == **2**（**不是** ≥1）。
- ⚠️ **不得**把核的 `dbg_*`/`obs_*` 直接上抬为 SoC 根端口（那是 ADR-072 D1 的**回退**）⇒
  观测口 MUST 是**根自有**语义（如计数/状态），不是内部信号直通。

### 2.2 **D2（G-2）** = 模块类型名 = **当前产品名（非契约）**，登记在**产品登记面**

> **新名字 `Rv32iCoreComplex` / `SocTraceCounter` = 当前产品名，非契约。
> 登记载体 = `.rddf/checks/soc-rtl-expectations.md#riscv-soc` + TB 的冻结期望集；不进 capability。**

- **与既有裁决对齐**：`rtl-hierarchical-output` 已由用户 2026-11-15 裁为「模块类型名**不是**契约」
  （改名不构成破坏性变更）⇒ ⇒ 本 ADR **不**制造第二套口径。
- **改名的唯一义务**（继承既有裁决）：① 产物**重新 lint 干净**（仍须是合法 Verilog 标识符）；
  ② **结构判据重算**（module 集与 module 数）⇒ 这是"非契约"的**代价**，MUST 显式承担。
- **登记两处、单一真相**：`.rddf/checks/soc-rtl-expectations.md` 的表 = **唯一真相**；
  TB 的 `kExpectedModules` MUST **逐字**取自该表（禁从 `ch_layer` 调用点派生 ⇒ 否则"漏一个声明"同时改期望 ⇒ 恒绿）。

### 2.3 **D3（G-3）** = `prog_*` **长期保留**为 SoC 根端口；S3 只**新增**总线路径

> **`prog_we` / `prog_addr` / `prog_data` MUST 长期是 SoC 根的 `ch_in`（固件装载后门）。
> S3 引入总线装载（boot ROM / DMA）时 MUST **新增**路径，MUST NOT 删除根端口。**

- **理由 1（契约兼容性）**：删根端口 = 产品端口面**破坏性变更**（ADR-072 D1 的端口面是产品契约）。
- **理由 2（可判性）**：装载是 TB 获得**确定性初始状态**的**唯一保持产品固件无关**的运行期手段
  （另一条路 `--firmware-init` 把初值**烘焙进产物** ⇒ 与"运行期装载"是两回事）；总线装载需要外部
  master 模型 ⇒ 会把 SoC 级 TB 的复杂度推到"要先实现一个 master"。
- **理由 3（边界）**：若将来确实要"只保留总线装载"（例如真实芯片形态）⇒ 那是**独立 ADR**
  （破坏性变更 + 迁移路径），不在本 ADR 预支。
- ⚠️ **装载期时序 MUST 写进产品文档**：装载期保持核 `ext_rst = 1`（`rv32i_pipeline_top.h` 的既有约定）
  ⇒ 否则"部分装载被执行"⇒ 计数不可复现。

### 2.4 **D4（G-4）** = 新产品用**单行 `kind=soc`**

> **`kind` 编码「产品类型」，不编码「形态」。SoC 产品的层次化形态是 ADR-073 的默认 ⇒ 不额外建行。**

| `kind` | 语义（本 ADR 钉死） | 先例 |
|---|---|---|
| `target` | 一个被跟踪 example target 的**扁平**产品 | `tb_counter_example` |
| `soc` | **SoC 层**产品（多 IP 装配的根） | `tb_axi_soc_example` |
| `hier` | **同一 `target` 的层次化补充行**（不是独立产品类型） | `riscv-mini-rv32i-soc-hier` |
| `dut` | 非 example target 的 DUT（**历史形态**） | 已随 2026-11-22 收敛改为 `target`；⚠️ tsv 表头仍写 `kind ∈ {target, dut}` ⇒ 以登记面**实际口径**为准，spec 修订时顺手对齐 |

- 新行 ⇒ `REGISTERED_SOC` **1 → 2**；`REGISTERED_HIER` **不动**。
- **为什么不是双行**（⚠️ **不是**"算两次"—— 那机制是错的：仓内已有同一设计占两个桶的先例
  `tb_rv32i_soc`（`target`）+ `riscv-mini-rv32i-soc-hier`（`hier`），守恒是**逐桶**计数）：
  `hier` 桶的口径 = 「同一设计的**非默认**另一种发射形态」；SoC 产品按 ADR-073 的层次化形态**就是默认**
  ⇒ ⇒ 再建 `hier` 行只是把**同一件产物**描述两次、零信息增量 ⇒ 故单行 `soc`。

---

## 3. 选项对比（每条含结论 + 理由）

### D1 的四条候选

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **两级顶层 + 判据按产品分工**（本文） | ✅ **选中** | 唯一同时满足"核可替换"与"ADR-046 原意（微架构单源）"的形态；且 8/10 两条判据各有明确作用域 |
| 2 | 全仓只许一个 example 侧顶层（严读 ADR-046） | ❌ 否决 | 与 ADR-073 的 `AxiSocTop` 先例**直接冲突**（它已在 example 侧）；严读会把既有签批决策判成违规 |
| 3 | 把 SoC 根放进 `include/`（当库组件） | ❌ 否决 | SoC 是**产品**不是库能力；且 `example-component-boundary` 要求 example 专用头住 example 内 |
| 4 | 核也放进 `examples/riscv-soc`，`riscv-mini` 退役 | ❌ 否决 | 会把 24 条判据与 8-module 判据同时搬走 ⇒ 回归锚点消失（违反"spike 不许退化"） |

### D2 的三条候选

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **非契约 + 产品登记面**（本文） | ✅ **选中** | 与用户 2026-11-15 的既有裁决一致（不造第二套口径）；登记在两处但单一真相在 expectations 表 |
| 2 | 把新名字写进 capability（契约名） | ❌ 否决 | 与 `rtl-hierarchical-output` 的"明文非契约"**相反**；且会让 S1 产生无谓的 spec delta |
| 3 | 名字不登记（只写进 TB） | ❌ 否决 | 无权威 ⇒ module 集判据成为"TB 里的魔法值"；且改名时无重算义务的载体 |

### D3 的三条候选

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **长期保留根端口 + S3 新增总线**（本文） | ✅ **选中** | 端口面零破坏 + TB 保持确定性装载 + 不预支"真实芯片形态" |
| 2 | S3 起改走总线、删根端口 | ❌ 否决（本 ADR 内） | 破坏性变更（ADR-072 D1）⇒ 应独立 ADR + 迁移路径 |
| 3 | 现在就双路径（根端口 + 总线） | ❌ 否决 | 总线主口还不存在（L1/反压未落地）⇒ 现在做是空壳 |

### D4 的三条候选

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **单行 `kind=soc`**（本文） | ✅ **选中** | `kind` = 产品类型；与 `tb_axi_soc_example` 先例一致 |
| 2 | 单行 `kind=hier` | ❌ 否决 | 会把"SoC"这个类型信息丢掉 ⇒ 与 `tb_axi_soc_example` 的分类不一致 |
| 3 | `soc` + `hier` 两行 | ❌ 否决 | `hier` 桶语义 = 「非默认的另一种发射形态」；SoC 的层次化是**默认** ⇒ 双行零信息增量（**不是**"算两次"—— 守恒逐桶） |

---

## 4. 后果 / 代价

### 正面

- **"可替换单元"有了物证**：核复合体的 module 端口表（34）成为产品契约的**可核对面**，
  也是 S3（总线化）的**变更基线**。
- **判据不再互相打架**：8（核复合体）/ 10（SoC 根）各有作用域与出处。
- **零回归**：核复合体的 24 条判据与 8-module 判据**一字不动**（本 ADR 不改核）。

### 负面

- **SoC 级观测面仍小**：S1 只有**两个**计数观测量（满足签批 spec 的 ≥2 下限）⇒ "核内部行为"的细粒度
  SoC 级断言无法表达 ⇒ 后续每加一个根自有观测量都是**一次产品端口面变更**（需登记）。
- **两份文档义务**：核级与 SoC 级的期望源分居 `examples-rtl-expectations.md` 与
  `soc-rtl-expectations.md` ⇒ 维护者必须知道"哪个产品看哪份"。
- **`prog_*` 成为长期债**：它是"后门"，不是真实芯片形态 ⇒ 真实形态的装载路径仍未定义（登记为后续）。
- **`min_varying_observations` 必须 == 2**（与 axi4 先例同档）⇒ ⇒ 这是**签批 spec 的硬要求**，不是可调参数：
  少一个变化观测口 ⇒ S1 产品**不合规**。

### 风险

| # | 风险 | 缓解 |
|---|---|---|
| R1 | 改名（非契约）后结构判据未重算 ⇒ 判据失效 | D2 的"改名义务 ②" + TB 的冻结期望集（改名必红 ⇒ 强制重算） |
| R2 | `kind` 被当形态用 ⇒ 出现 `soc`+`hier` 双行 | D4 的反向判据 + `verify_examples_rtl_sim_coverage.sh` 的守恒 |
| R3 | SoC 级无变化观测 ⇒ `hier_eq` 拒绝 / 不满足签批 spec 的 ≥2 | D1 强制**两个**随激励变化的根自有 `ch_out`（`store_count` + `writeback_count`）⇒ `min_varying == 2` |
| R4 | 两个顶层被当成"两份实现" ⇒ 未来有人复制微架构到 SoC 侧 | D1 的"MUST NOT 复制微架构实现" + **把 `verify_no_duplicate_rv32i_headers.sh` 的扫描面扩到 `examples/riscv-soc/src/`**（S1 的 AC11 承接）⇒ ⇒ ⚠️ **未扩前该风险无护栏**（现扫描面 = `include/cpu/**` vs `examples/riscv-mini/src/**`，结构性看不见 riscv-soc 侧） |

> ⚠️ **登记不执行**：`openspec/specs/rtl-hierarchical-output` 的「交付粒度 8 module」条款**迟早**要加半句
> 作用域限定（"核复合体产品"）—— 本 ADR 用 V2/V11 已把它 **operationalize**（不改 spec），
> **第三次装配（如 L2 SoC）落地时**是自然修订点 ⇒ ⇒ 现在不动 spec，避免为一次装配付 delta 成本。

---

## 4.5 落点与同步义务

| # | 事项 | 归属 | 载体 |
|---|---|---|---|
| ① | **proposal 同步**：AC3（根端口 7 → **8**）· AC4（`min_varying ≥1` → **==2** + 激励 ≥10 拍）· 组件名 `SocStoreCounter` → **`SocTraceCounter`**（本 ADR 的 V1/V2/V4 为**权威数字**） | **planner** | `openspec/changes/riscv-soc-skeleton-2026-11/proposal.md` 的 `## 验收标准` |
| ② | `verify_no_duplicate_rv32i_headers.sh` 扫描面扩到 `examples/riscv-soc/src/` | builder | S1 的 AC11 |
| ③ | 子 module 端口表提取（V5 的载体） | builder | S1 的 AC6 |
| ④ | 「登记面 ↔ TB 期望集逐字比对」（V6 的主语） | builder | S1 的 AC11 |
| ⑤ | `rtl-hierarchical-output` 的「8 module」加作用域限定 | **第三次装配时**（登记不执行） | §4 末尾注记 |

---

## 5. Verification（**前瞻**判据 · 可勾选 · 由 S1 的 AC 承接）

- [ ] **V1** `examples/riscv-soc/src/rv32i_soc_top.h` 存在，root `__io` 声明数 == **6**（`ext_rst` / `prog_we` / `prog_addr` / `prog_data` / `store_count` / `writeback_count`）⇒ 产品端口 == **8**（框架 2 + 根自有 6）
- [ ] **V2** 产物 module **集合相等** == `{top, Rv32iCoreComplex, SocTraceCounter, IfStage, IdStage, ExStage, MemStage, WbStage, HazardUnit, ForwardingMux_32_}`（**10** · 手推 · `check_products` CHECK 1）
- [ ] **V3** `check_products` CHECK 2：顶层端口数 == **8**（`manifest.port_table.size()` · 含框架 2）**且** 后代未驱动端口 == 0
- [ ] **V4** `check_products` CHECK 4：双形态逐拍等价 + **篡改自证**（观测 = `store_count` + `writeback_count` · **`min_varying_observations == 2`** · 激励 **≥10 拍且值不同** · 粘滞信号不计入）
- [ ] **V5** `Rv32iCoreComplex` module 的端口表 == **34** = **框架 2**（`default_clock`/`default_reset`，核内 **inline** 的 TCM `sread` 寄存器落在该 module）+ **核自有 32**（4 in + 28 out）· 表落在 `examples/riscv-soc/AGENTS.md`
  ⇒ ⇒ ⚠️ **判据载体 MUST 新建**：`manifest.port_table[]` **只覆盖顶层**（ADR-072 D1-c）⇒ 子 module 端口表需**独立提取**（S1 承接：`scripts/lib/verilog_tree_extract.py` 或等价），且 MUST 带**判别力自证**（删一个端口 ⇒ 判据必红）
- [ ] **V6** `Rv32iCoreComplex` / `SocTraceCounter` 两名字出现在 `.rddf/checks/soc-rtl-expectations.md#riscv-soc` 的表里 **且** 与 TB 的 `kExpectedModules` 逐字一致
  ⇒ ⇒ **主语 = 一条机械判据**（S1 承接：在 `verify_examples_rtl_sim.sh` 或 `_coverage` 增「登记面 ↔ TB 期望集逐字比对」）⇒ ⇒ 不得只靠"任务清单人工核对"
- [ ] **V7** `examples-rtl-sim-targets.tsv` 新行 `kind=soc` · `REGISTERED_SOC == 2` · `REGISTERED_HIER == 1`（不动）
- [ ] **V8** S1/S2 产物里 `prog_*` 仍是**根端口**（根端口表 grep 命中）
- [ ] **V9** `docs/adr/ADR-046-rv32i-canonical-home.md` 含 dated addendum（日期 + 「无决策变更」性质 + 指向本 ADR）
- [ ] **V10** `git diff <S1-base-sha> -- examples/riscv-mini/ include/` **为空**（核复合体零改动）
- [ ] **V11** 核复合体产品的 `rtl-hierarchical-output` 判据仍绿：**8 module** + 7 个 stage module 名齐（既有 Scenario 不变）
- [ ] **V12** 反向判据：① **变异施加在观测集**（只留 `store_count`）⇒ **V4 必红**（变化观测量 = 1 < 2）⇒ ⚠️ **MUST NOT** 改 C++ 侧（编译期先失败 ⇒ 假绿）② 断开 `SocTraceCounter` 的任一 `ch_in`（`store_event` / `retire_event`）⇒ **V3 必红**（后代未驱动 ≠ 0 ⇒ ADR-072 D1-f fail-closed）
