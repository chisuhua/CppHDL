---
name: rv32i-soc-platform-2026-11
priority: P1
phase: phase-6
category: framework
type: feature
状态: in-progress
进度: "**立项（2026-11-22 · planner）** · ✅ **S0 已闭**（ADR-077 Accepted 2026-11-23）· ✅ **S1 已实施并验收**（`examples/riscv-soc` · verifier **13/13 PASS** · `2cab7fb` + `62337b0` + `c321848`）· 🔜 **下一批 = S3**（S2 为 S3 硬前置）· ⚠️ **S5（IRQ）被 `rv32i-privileged-csr-2026-11` 与「核加 irq 口」双重阻塞** · **2026-11-23 依实测调整了 IP 计划（见 §IP 计划）**"
主题: RV32I 从「示例 SoC」走向「可组合 SoC 平台」—— 核心复合体 / SoC 根两级产品单元 + 总线化演进
依赖: ADR-046（微架构唯一权威 · **ADDENDUM 2026-11-23**）· ADR-061（成层粒度显式声明）· ADR-070（RV32I 成层 + 兄弟边 D4③）· ADR-072（产品端口面 + 四件套）· ADR-073（SoC 产物形态 + SoC 级 TB）· ADR-074（验证单一真相源）· **ADR-077（两级顶层 + 四条形态裁决 · Accepted）** · `rv32i-privileged-csr-2026-11`（**S5 的硬前置** · draft）
来源: 用户 2026-11-22「想把 RV32I 核做成真实可替换单元 —— 后续加 L1/L2 缓冲、MMU、内存控制器、总线」+「riscv-mini 可看成 riscv-soc 的 spike」+「riscv-mini 改造成核复合体，另建 examples/riscv-soc 实施 Rv32iSocTop」+ 用户 2026-11-23「检查 examples/riscv-soc，检查规划要如何调整，特别是整体 SoC 的不同 IP 的计划」
生成时间: 2026-11-22
evidence: probe:scripts/probe_r21_hier_granularity.sh
---

# Improvement: RV32I 可组合 SoC 平台（核心复合体 ↔ SoC 根 · 总线化演进）

**主题**: 真层次化 RTL 输出

> 具体对象 = 把「一个组件既是核又是 SoC」拆成 **两个产品单元**，并给出通往 L1/L2 · 总线的分批路径。
> （frontmatter 的 `主题:` 是完整表述；本行取**路线图主题**以过 `add-improve` 的 HARD-GATE 匹配。）

> 📌 **2026-11-23 调整记录（builder 依实测回填 · 用户指令）**：S0 已闭（ADR-077）· S1 已实施并验收 ·
> **IP 计划按「接口类 × 批次」重排**（§IP 计划）· 新增四条待裁决策门 **G-5…G-8**（因 S1 落地后暴露
> 的端口/映射/总线/根端口面问题）· 原 G-1…G-4 **已由 ADR-077 裁决**（见 §决策门）。

---

## 架构依据

| # | 权威 | 对本项的约束 |
|---|---|---|
| ① | **ADR-046 + dated addendum（2026-11-23）** | 「微架构 MUST 住 `include/cpu/`」**保留**；example 侧允许**两级顶层**（IP 级 / SoC 级）⇒ 由 **ADR-077 D1** 明文解释 |
| ② | **ADR-061 决策①**（成层**显式**声明 · 默认 inline） | 新根与新 IP 的成层声明 MUST 显式写 |
| ③ | **ADR-070 D2 / D3 / D4** | D2 的 7 个成层点不改 · D3 模块类型名（**ADR-077 D2 裁为「非契约」**，改名不构成破坏性变更）· D4③ 产物 MUST 有真实兄弟边 |
| ④ | **ADR-072 D1 / D1-h** | 产品端口面 = 框架 2 + **根自有 IO**；后代 `ch_out` 降为体内 `logic`；冒泡机制已退役 |
| ⑤ | **ADR-073**（SoC 产物 = 层次化 · 每 IP 一 module · 每个后代 `ch_in` 必须有驱动或 tie-off · SoC 级 TB = 独立 Verilog TB） | 新 SoC 根按同一纪律建 |
| ⑥ | **ADR-074** | 期望值 MUST 手推；`.sv` 生成不了 ⇒ 手写 + `generated=no` 终态 |
| ⑦ | **`check_products`**（`include/rtl_emit/product_check.h`） | 四件套自检 = ① module 集 ② 顶层端口数 + 后代未驱动 == 0 ③ lint ④ 双形态逐拍 + **篡改自证** |
| ⑧ | **ADR-077（Accepted · 2026-11-23）** | **D1** 两级顶层分工 + 判据作用域（核 8 module / SoC = 成层 IP 数 + 1）· **D2** 模块类型名 = 非契约（登记在期望源）· **D3** `prog_*` **长期保留**，S3 只**新增**总线路径 · **D4** 单行 `kind=soc` |
| ⑨ | **签批 spec `rtl-soc-output`** | SoC 观测集 MUST 含 **≥2** 个随激励变化量（粘滞信号不计 · 激励 ≥10 拍且值不同）⇒ `min_varying_observations == 2`（**不是 ≥1**） |

### A.1 实测事实

**A.1.a 规划期（2026-11-22）**

| # | 事实 | 证据 |
|---|---|---|
| ① | 核复合体产品 = **34 端口 / 8 module**（7 个 `ch_layer` stage + `top`） | `build/rv32i-rtl-sim/hier/top.v` |
| ② | 核级 24 条判据**全部观测核级信号**（`dbg_*` / `obs_data_*` / `obs_perf_instructions` / `prog_*`） | `examples/riscv-mini/tb_rv32i_soc.cpp:222-258` |
| ③ | **子层不折叠**：`Rv32iPipeline` 成层 ⇒ 9 module（7 子层仍在） | `/tmp` 对照实验 + `rv32i_pipeline.h:140-153` |
| ④ | **MEM 级无反压**：`ch_in<ch_bool> data_ready` 声明于 `rv32i_pipeline.h:69` 但**零消费者** | `grep -n data_ready` 仅命中声明行 |
| ⑤ | TCM 的 `ready` **恒 1** ⇒ 用现有产品永远进不了 stall 分支 | `include/cpu/pipeline/rv32i_tcm.h:73,168` |
| ⑥ | **缓存 wrapper 无 burst 口**（只有标量 `axi_addr/axi_valid/axi_ready/axi_data`） | `include/cpu/cache/i_cache_prefetch.h:75-78` |
| ⑦ | `hier_eq::compare` 机械拒绝：观测集空 / 无变化信号 / 观测量非根 `ch_out` | `include/hierarchy_equivalence.h:43-51` |

**A.1.b S1 实施后（2026-11-23 · 实测回填）**

| # | 事实 | 证据 |
|---|---|---|
| ⑧ | **SoC 级产品 = 10 module / 8 端口**（框架 2 + 根自有 **6**：`ext_rst` · `prog_we` · `prog_addr` · `prog_data` · `store_count` · `writeback_count`） | `examples/riscv-soc/AGENTS.md` 的产品面表 · verifier AC2/AC3 |
| ⑨ | **观测集 = 2 个单调计数**（`store_count` = `obs_data_write_en` · `writeback_count` = `dbg_wb_write_en`）⇒ 两值必须不同（同源/对调必红） | `examples/riscv-soc/src/soc_trace_counter.h` · 期望源 §4 的三条变异 |
| ⑩ | **固件路径已补**：`--firmware` = ELF → `init_data`（`ch_mem` 初值）⇒ **零端口变更** · 与 `prog_*` 并存 | `2cab7fb` / `c321848` · `rv32i_soc_top.h` 构造参数 |
| ⑪ | 新护栏 `scripts/verify_riscv_soc_core_ports.sh`：V1–V5 + **7** 条投毒 | verifier AC6 |
| ⑫ | **核的输入侧只有 4 根**（`ext_rst` / `prog_we` / `prog_addr` / `prog_data`）⇒ **无 `irq`** | `rv32i_pipeline_top.h:62-69` |
| ⑬ | 全仓外设均为 **AXI4-Lite** 且**多数带 `irq`**（gpio/uart/timer/i2c/pwm/spi） | `include/axi4/peripherals/` |
| ⑭ | `Clint<TIMER_DIVIDER>` 已有（MMIO + `mtip`）· `AxiDma` 已有（34 端口）· `AddressDecoder` 已有 | `include/cpu/riscv/clint.h:58-69` · `include/axi4/peripherals/axi_dma.h` |
| ⑮ | **地址映射只有 TCM 两段**（I-TCM `0x8000_0000` / D-TCM `0x8001_0000` · 各 16 KB）⇒ **无 MMIO 段** | `include/cpu/riscv/rv32i_mem_map.h` |

### A.2 已否掉的形态（不要重提）

| 形态 | 为什么否 |
|---|---|
| **只把 TCM 成层**（`pipeline` 仍 inline） | ADR-070 D4 明文反例（兄弟边 = 0） |
| S1 用 `AxiLiteGpio` 全 tie-off 占位 | 恒 0 死组件 + 兄弟边仍 0 + 根无变化输出 ⇒ `hier_eq` 机械拒绝 |
| 把核的 `dbg_*`/`obs_*` 直通为 SoC 根端口 | ADR-072 D1 的**回退** ⇒ ADR-077 D1 明文禁止（观测口 MUST 是根自有语义） |
| S3 删 `prog_*` 根端口 | ADR-077 **D3**：长期保留；删 = 产品端口面破坏性变更 |
| S1 同时建 `kind=soc` + `kind=hier` 两行 | ADR-077 **D4**：SoC 的层次化形态是默认 ⇒ 单行 `soc` |

---

## 术语

| 术语 | 定义 |
|---|---|
| **核复合体** | `Rv32iPipelineTop`（pipeline + ITCM + DTCM）· **34 端口** · `examples/riscv-mini` 的产品单元 |
| **SoC 根** | `Rv32iSocTop` · 产品端口面 = 框架 2 + **根 `__io` 声明数**（S1 = 6 ⇒ 8） |
| **产品单元** | 一个「四件套」（manifest + RTL 产物 + 独立 TB + 覆盖登记） |
| **形态行** | `examples-rtl-sim-targets.tsv` 的一行（`kind` ∈ {target, soc, hier, dut}） |
| **兄弟边** | 两个**成层子 module 之间**的连线（ADR-070 D1 修复的那条分支） |
| **观测口** | SoC 根自己的 `ch_out`，语义由根本身定义（计数 / 状态），**不是**内部信号直通 |

---

## 范围

### IN

- ✅ **S1（已完成）**：`examples/riscv-soc/` + `Rv32iSocTop` + `SocTraceCounter` + SoC 级 TB + 登记（核零改动）
- **S2**：MEM 反压（`data_ready` 真消费 · `ready ≡ 1` 时**时序等价**）
- **S3**：核**数据侧** AXI4 主口 + D-L1 + 缓存 wrapper **burst 升级** + TCM 旁路开关（**新增**路径、**不删** `prog_*`）
- **S4**：总线互联（Lite Matrix）+ 5 个 Lite 外设 + `AxiDma`（boot/装载）+ MMIO 地址解码 + 取指侧总线化 + I-L1
- **S5**：核加 `irq` 输入口（**34 → 35**）+ `Clint` + IRQ 汇聚器
- **S6**：L2 + DRAM 控制器（新 IP）
- **S7**：MMU（Sv32）—— 与 `rv32i-privileged-csr-2026-11` 合并/承接

### OUT（不做 · 已登记归属）

| 项 | 归属 |
|---|---|
| 特权级 / CSR / trap（**S5/S7 的硬前置**） | `rv32i-privileged-csr-2026-11`（draft） |
| 缓存替换策略 / 一致性协议 | 各自独立立项（本项只接**现有** `ICacheSync` / `DCacheSync`） |
| 微架构重构（`include/cpu/pipeline/**` 的算法/时序） | 不在本项（本项只加/改**接口**） |
| 总线互联/IP 的新实现 | 复用 `include/axi4/**`（矩阵 / 桥 / 外设 / DMA） |
| 改名（`Rv32iPipelineTop` / `Rv32iCoreComplex` / `rv32i_pipeline_top.h`） | 单列一批（**R3**）⇒ ⇒ ✅ **已完成并验收**（`openspec/changes/riscv-core-rename-2026-11/` · R3a `870cda5` / R3b `a82eda1` / R3c `2210e9d` · verifier **15/15** ⇒ 可归档 · 报告 `.rddf/reviews/riscv-core-rename-2026-11-verify.md`）⇒ ⇒ 核的三个名字现统一为 **`Rv32iCoreTop`** / **`Rv32iCoreTop`** / **`rv32i_core_top.h`** ⇒ ⇒ **端口面零变化**（34 根逐字不变）· **行为零变化**（核侧三份产物 sha256 逐字节不变）|
| 改名（TB 名 `tb_rv32i_soc`） | 单列一批（**R3d**）· **未立项** ⇒ ⇒ 会动 tsv / CMake / CI / 多护栏 fixture ⇒ ⇒ 与 R3a–c 分离 |

### 写面（按批分级）

| 批 | 允许触及 |
|---|---|
| S1 ✅ | 仅 `examples/riscv-soc/**` + 登记面 + 文档 |
| S2 | + `include/cpu/pipeline/**`（消费 `data_ready`） |
| S3 | + `include/cpu/cache/**`（burst 口）+ `rv32i-memory-timing` delta |
| S4 | + `include/axi4/**`（如需新互联组合）+ `rv32i_mem_map.h`（MMIO 段 · **G-6**） |
| S5 | + 核端口面（**G-5**）+ `include/cpu/riscv/clint.h` |

---

## Why

1. **"核"今天不可替换**：核复合体把 pipeline 与 ITCM/DTCM 绑在一起 ⇒ 换缓存化的核要连带换存储器 / TB / 登记。
2. **后续每个诉求都卡在同一处**：L1/L2 要总线口 · MMU 要特权级 · 内存控制器要总线 —— 而核**没有"对外接口"**（只有内部 `dbg_*`），且**没有中断入口**。
3. **验证面可零重写**：24 条判据全部观测核级信号 ⇒ 核留 `examples/riscv-mini` ⇒ 文件与判据零改动（S1 已证明：AC5 绿）。
4. **框架已具备**：`ch_layer` + ADR-073/077 纪律 + `check_products` + 现成 IP（Lite 矩阵 / 6 个外设 / DMA / CLINT）⇒ 缺的是**装配与登记**，不是新机制。

---

## What Changes

> ✅ **2026-11-23 · R3 已收口（builder 回填）**：核的三个名字已统一
> （**类** `Rv32iCoreTop` · **模块类型名** `Rv32iCoreTop` · **头文件** `rv32i_core_top.h`）
> ⇒ ⇒ **零行为变化**（核侧 flat / 层次化 / 带固件初值三份产物 **sha256 逐字节不变**）
> ⇒ ⇒ **零端口变化**（34 根顶层端口逐字不变 ⇒ ⇒ 下游连线不必改）
> ⇒ ⇒ **spec 零 delta**（`rtl-hierarchical-output` 钉的是成层集合 7 + 交付粒度 8 module ⇒ 本批两者都不碰）
> ⇒ ⇒ ⚠️ **实施期口径订正**：`port-naming-registry.md` §4.1 是**具名消化**活账 ⇒ ⇒
> **改名不是消化 ⇒ MUST NOT 追加行**（已回写 change 的 AC13）。
> ⇒ ⇒ 🔜 **下一批仍是 S2**（MEM 反压）—— 实施期实测供参考：`Rv32iPipeline::data_ready`（`:69`）
> **零消费者** + `MemStage::__io` **无任何 ready/stall 输入** ⇒ ⇒ S2 需**新增** MEM 级停顿通路
> ⇒ ⇒ **时序契约变更 ⇒ 架构面**。

> ✅ **2026-11-23 · R3e 完成（builder 回填）**：核的 **34 端口契约表已归位到核侧**
> （`examples/riscv-mini/AGENTS.md` 新章「核 IP 的 34 端口契约表」）⇒ ⇒ SoC 侧改为**指针**
> ⇒ ⇒ 护栏 `verify_riscv_soc_core_ports.sh` 的期望表指针切到核侧（V1/V2/V3 仍绿 · self-test 7 投毒仍绿）
> ⇒ ⇒ **理由**：契约表跟随**被描述的对象** ⇒ ⇒ 「核 = 独立可验证的 IP」在**文档归属**上也成立。
> ⇒ ⇒ 顺带订正：原前言「期望表由 `verify_examples_rtl_sim.sh` 解析」**指错了脚本**
> ⇒ ⇒ 实测唯一消费者 = `verify_riscv_soc_core_ports.sh` ⇒ ⇒ 已改正。

### 批次 × IP（**2026-11-23 调整后** · IP 计划的主表）

| 批 | 新 IP / 变更 | 主判据 | 前置 |
|---|---|---|---|
| **S1 ✅** | 核复合体 + `SocTraceCounter` | 10 module · 8 端口 · 2 观测量 · 13 AC（verifier 全 PASS） | ADR-077 |
| **S2** | —（内部：MEM 反压） | `ready≡1` 与 S1 **逐拍等价** · `ready=0` wrapper 的 stall **手推拍数** | — |
| **S3** | 核**数据侧** AXI4 主口 + **D-L1**（`DCacheSync`）+ wrapper **burst 升级** | burst 事务手推（len/size/burst）· hit/miss 拍数契约 · TCM 旁路后 `prog_*` 一致性 | S2 |
| **S4** | **`AxiLiteMatrix2x2`** + **`AxiLiteGpio/Uart/Timer/Spi/I2c/Pwm`**（轮询）+ **`AxiDma`**（boot/装载主）+ **MMIO 地址解码** + 取指侧总线化 + **I-L1** | 每 IP 一条总线级 RD/WR 回环（手推地址+值）· 地址映射表 == 手推 · DMA 装载后核能取指 | S3 |
| **S5** | 核 **`irq` 输入口（34→35）** + **`Clint`** + **IRQ 汇聚器（新写）** | IRQ 边沿 → trap 的拍数（手推）· `mtip`/`mie`/`mip` 语义 | **privilege** + S4 |
| **S6** | **L2** + **DRAM 控制器**（新写） | 总线事务 + 时序契约（`rv32i-memory-timing` delta） | S3 · S4 |
| **S7** | **MMU（Sv32）** | 页表走查拍数 · 缺页 trap | S5 · 承接 `rv32i-privileged-csr` |

### §IP 计划（全 SoC 的 IP 地图 · **本次调整的核心**）

#### 两个**结构性卡点**（决定 IP 顺序的不是"想加什么"，而是这两条）

| 卡点 | 实测现状 | 后果 |
|---|---|---|
| **核无总线口** | 核只有 4 个输入（`ext_rst` / `prog_*`），数据与取指都指向 **TCM** | **S3 之前任何外设都不可达**（除 `prog_*` 后门）⇒ 第一优先级是**总线化**，不是"加外设" |
| **核无中断输入** | 34 端口输入侧 **没有 `irq`** | 所有带中断的外设（UART RX / timer / I2C）只能**轮询**；trap 还需 privilege ⇒ 顺序 = **privilege → 核加 `irq` 口 → IRQ 汇聚** |

#### IP 清点（已有 vs 需新写）

| 类 | IP | 状态 | 接口 | 接入批 |
|---|---|---|---|---|
| 核 | 核复合体（pipeline + I/D-TCM） | ✅ 已有 · 34 端口 | 直接信号 | S1 ✅ |
| 观测 | `SocTraceCounter` | ✅ 已有（S1 新写） | 直接信号 | S1 ✅ |
| L1 | `ICacheSync` / `DCacheSync`（+ wrappers） | ✅ 已有 · **无 burst 口** | 标量 `axi_*` | **S3**（含 burst 升级） |
| 互联 | `AxiLiteMatrix2x2` | ✅ 已有（`examples/axi4` 在用） | AXI4-Lite | **S4** |
| 互联 | `AxiInterconnect4x4`（full） | ✅ 已有 | AXI4 full | S6（多主 / burst 场景） |
| 桥 | `AxiToAxiLite` | ✅ 已有 | full → lite | S4 / S6 |
| 外设 | `AxiLiteGpio` / `Uart` / `Timer` / `Spi` / `I2c` / `Pwm` | ✅ 已有 · 均带 `irq` | AXI4-Lite | **S4**（轮询；IRQ 延到 S5） |
| DMA / boot | `AxiDma` | ✅ 已有 · 34 端口 | AXI4-Lite 主 | **S4**（顺带补 ADR-077 D3 的"真实装载路径未定义"） |
| 计时器 | `Clint<TIMER_DIVIDER>`（`mtip`） | ✅ 已有 | MMIO + `mtip` | **S5** |
| IRQ 汇聚 | — | ❌ **不存在** | — | **S5**（新写：OR 汇聚 + 使能掩码） |
| L2 / DRAM | — | ❌ 不存在 | — | S6（新写） |
| MMU | — | ❌ 不存在 | — | S7 |
| 地址解码 | `AddressDecoder` + `rv32i_mem_map.h`（**仅 TCM 两段**） | ✅ 部分 | — | **S4**（MMIO 段须定义 · **G-6**） |

#### 两条硬顺序约束（不是偏好）

1. **S3 必须先于任何外设** —— 核无总线口 ⇒ 外设在 S3 前**不可达**。
2. **S5 必须先于任何 IRQ 功能** —— 核无 `irq` 口 + 无 privilege ⇒ **trap 落不下来**（现状 = 冻结/忽略）。

⇒ ⇒ 对照用户列的「L1/L2 · MMU · 内存控制器 · 总线」：**总线化（S3/S4）是地基**；L2 / MMU 在其上。
⇒ ⇒ **不建议**先做 L2：L2 挂在总线上，没有 S3/S4 它无处可挂（且会迫使 S3 的 burst 口返工）。

#### 每批的「IP 级」验收模板（S4 起统一）

1. 单个 IP 的**总线回环**：写一个已知值到该 IP 的 MMIO 偏移 ⇒ 读回同值（手推地址 + 值）
2. 该 IP 的**根可观测面**（若需要）：根自有语义（计数/状态），**不是**内部信号直通（ADR-072 D1）
3. 地址映射表条目 == 手推（`G-6` 定下后为**单一真相**）
4. 该 IP 的 `--check-products`（若其单独构成产品单元）或并入 SoC 产品单元的行数

---

## Acceptance

> ⚠️ 期望值 MUST **手推**（禁抄仿真输出）· 计数 MUST 从产物/清单派生（禁裸数字）。

### S1（✅ **已完成** · 2026-11-23 · verifier 13/13 PASS）

> 判据原文与逐条证据见 `openspec/changes/riscv-soc-skeleton-2026-11/proposal.md` 的 `## 验收标准`（AC1–AC13）
> 与 `.rddf/reviews/riscv-soc-skeleton-2026-11-verify.md`。实施期与规划的**偏差**（已接受）：
> ① 根端口 **8**（不是 7）—— 签批 spec 要求 **≥2** 个变化观测量 ⇒ `SocTraceCounter` 出**两个**计数；
> ② 观测 IP 名 `SocTraceCounter`（不是 `SocStoreCounter`）；③ 新增 `--check-products` 与 `--firmware` 两种模式；
> ④ 新护栏 `verify_riscv_soc_core_ports.sh`（V1–V5 + 7 投毒）。

### S2

- [ ] **ACs2-1** 🔴 `ready ≡ 1`（TCM 默认）时，S2 产物与 S1 产物**逐拍等价**（在 `examples/riscv-mini` 的 34 端口产品上判 —— SoC 只有 2 个观测量，不足以判等价）
- [ ] **ACs2-2** 🔴 **`ready=0` 的测试 wrapper**（TCM ready 恒 1 ⇒ 不 wrapper 就**永远**进不了 stall 分支）+ **stall 拍数手推值**；判别力自证：删 stall 逻辑 ⇒ 必红
- [ ] **ACs2-3** 24 条核级判据**零改动**仍绿（回归锚点）

### S3

- [ ] **ACs3-1** 🔴 AXI4 **burst** 事务的手推期望（`len` / `size` / `burst` / `addr`）· wrapper 的 burst 口到位（否则 S6 返工）
- [ ] **ACs3-2** 🔴 cache hit/miss **拍数契约**（写进 `rv32i-memory-timing` 的 delta）
- [ ] **ACs3-3** 🔴 **TCM 旁路后 `prog_*` 一致性**：装载写进"核实际读的存储体"（否则装载成功但核读到 0）
- [ ] **ACs3-4** `prog_*` 根端口**仍在**（ADR-077 D3）

### S4

- [ ] **ACs4-1** 🔴 每个外设**一条总线回环**（手推偏移 + 值）· 全部 5–6 个 Lite 外设逐个可达
- [ ] **ACs4-2** 🔴 **MMIO 地址映射表** == 手推（单一真相 · G-6 定归属）
- [ ] **ACs4-3** 🔴 `AxiDma` 装载：DMA 把镜像写进 TCM 后**核能取指**（boot 路径闭环 · 补 ADR-077 D3 的未定义项）
- [ ] **ACs4-4** 取指侧总线化后 **I-L1** 的 hit/miss 契约

### S5

- [ ] **ACs5-1** 🔴 核的 `irq` 口（**34 → 35**）⇒ **C6 快照 / 34 端口表 / `verify_riscv_soc_core_ports.sh` 期望表 / SoC 8 端口表**同批重算（**G-5**）
- [ ] **ACs5-2** 🔴 IRQ 边沿 → trap 的**拍数**（手推）· `mtip` → `mip.MTIP` → trap 的链路
- [ ] **ACs5-3** IRQ 汇聚器的**掩码/优先级**语义 == 手推

### S6 / S7

| 批 | 判据方向 |
|---|---|
| S6 | 总线事务（含 burst）· L2 命中率/拍数 · DRAM 控制器时序 · 与 `rv32i-memory-timing` delta |
| S7 | 页表走查拍数 · 缺页 trap · 与 `rv32i-privileged-csr` 的 capability 合并/承接 |

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| `examples-rtl-sim-coverage` | 已 MODIFIED（S1 新增 1 行 `kind=soc`） | 后续批只涨 `interp_checks` / `min_checks` / `scenarios` |
| `rtl-soc-output` | 无 delta（S1 满足既有条款） | S3 起若新增「总线口」条款 ⇒ 评估 delta |
| `rv32i-memory-timing` | S1/S2 无 delta · **S3/S6 需要** | cache hit/miss 与 DRAM 时序 |
| `example-component-boundary` | 无 delta | 新头生在 `examples/riscv-soc/src/` 内 |
| `rv32i-privilege` | **S5/S7 依赖** | 由 `rv32i-privileged-csr-2026-11` 提供 |

---

## Impact

| 面 | 影响 |
|---|---|
| **已落地（S1）** | `examples/riscv-soc/`（6 文件）· 跟踪 28 → **29** · `29 == 17 ∪ 12` · `REGISTERED_SOC` 1 → 2 · 新护栏 1 个 |
| **后续（S3–S7）** | 核端口面（S5，**G-5**）· `rv32i_mem_map.h` 的 MMIO 段（S4，**G-6**）· 缓存 wrapper 的 burst 口（S3）· 新 IP（IRQ 汇聚 / L2 / DRAM / MMU） |
| **观测面** | 🔴 SoC 级**看不到** `dbg_*`/`obs_*`（ADR-072 D1）⇒ 核级 24 条只留 `examples/riscv-mini`；SoC 级靠**根自有观测口** |
| **文档** | 每批：`examples/riscv-soc/AGENTS.md`（端口/模块表）· root `AGENTS.md` · `docs/developer_guide/rv32i-rtl-delivery-and-sim.md` · `docs/CHANGELOG.md` |

---

## 风险与回滚

| # | 风险 | 处置 |
|---|---|---|
| **R1** | "零行为变化"只靠声明 | 用**提交区间** + base SHA（S1 已证明可行）+ 24 条 `RESULT` 逐字 |
| **R2** | 无变化观测量 ⇒ `hier_eq` 机械拒绝 | 观测口 MUST 是**单调计数**（S1 的 `store_count`/`writeback_count`） |
| **R3** | 命名漂移 | 改名单独一批（改名的义务 = 重新 lint + 结构判据重算 · ADR-077 D2） |
| **R4** | S2 的"功能等价"不成立 | S2 = `ready≡1` 逐拍等价 + `ready=0` wrapper 的 stall 手推 |
| **R5** | S3 撞反压不足 | S2 是 S3 **硬前置** |
| **R6** | 新增 example "新增即漏" | 登记 8 处全部列成机械判据（S1 已验证） |
| **R7** | 装载期复位极性接反 ⇒ "装载成功但永不执行" | 计数断言是唯一能发现它的判据 ⇒ MUST 手推 |
| **R8** 🆕 | **IRQ 被双重阻塞**（无 `irq` 口 + 无 privilege） | S5 的入口条件写死：`rv32i-privileged-csr` 完成 **且** G-5 裁决 **且** S4 验收 |
| **R9** 🆕 | **根端口暴露外部总线**时 bundle 不是顶层端口（`AGENTS.md` 的 bundle 限制） | 若要把外部 AXI 上根端口 ⇒ MUST 标量拆口（~30 根）⇒ 先过 **G-8** |
| **R10** 🆕 | 先做 L2（跳过 S3/S4） | 明令不做：L2 无处可挂且会迫使 burst 口返工（§IP 计划 的两条硬顺序约束） |

**回滚**：S1 = 删目录 + 撤登记（核零改动）；S2 起每批独立 revert。

---

## 决策门

### 已闭（ADR-077 · 2026-11-23）

| # | 决策 | 裁决 |
|---|---|---|
| ~~G-1~~ | IP 级 / SoC 级 top 分工 | ✅ **ADR-077 D1**（ADR-046 addendum · 判据作用域：核 8 module / SoC = 成层 IP 数 + 1） |
| ~~G-2~~ | 模块类型名批准权 | ✅ **ADR-077 D2**：模块类型名 = **非契约**（登记在期望源；改名的义务 = 重新 lint + 结构判据重算） |
| ~~G-3~~ | `prog_*` 归属 | ✅ **ADR-077 D3**：**长期保留**；S3 只**新增**总线路径 |
| ~~G-4~~ | 新产品 `kind` | ✅ **ADR-077 D4**：单行 `kind=soc`（`REGISTERED_SOC` 1→2） |

### 待裁（S1 落地后暴露 · **architect**）

| # | 决策 | 为什么不能由 builder/planner 定 | 影响批 |
|---|---|---|---|
| **G-5** | **核加 `irq` 输入口（34 → 35）** | 端口契约变更：C6 快照 · 34 端口表 · `verify_riscv_soc_core_ports.sh` 期望表 · SoC 8 端口表 · tsv 全部同批重算 ⇒ 架构面 | S5 |
| **G-6** | **MMIO 地址映射的归属** —— 扩展 `include/cpu/riscv/rv32i_mem_map.h`（库层 = 平台契约）vs SoC 侧新建（example 侧 = 产品契约） | 决定"内存图"是库层单一真相还是每 SoC 一份 ⇒ 影响 S4/S6 与 `address_decoder.h` | S4 |
| **G-7** | **总线类** —— S4 用 **Lite**（现成 6 个外设 + 矩阵）还是直接上 **full AXI4**（`AxiInterconnect4x4`） | 决定 S3 的 burst 升级是否提前、S6 是否返工 | S3 / S4 |
| **G-8** | **SoC 是否把外部总线口暴露到根端口** | 若暴露 ⇒ bundle 不是顶层端口（须标量拆口 ~30 根）⇒ 与 ADR-072 D1「根自有 IO」的相互作用 ⇒ 架构面 | S4 / S6 |

> 建议裁决顺序：**G-7 → G-5 → G-6 → G-8**（G-7 决定 S3 的口形态；G-5 决定 S5 能否开工）。

---

## 待粘贴索引行（builder 在首笔提交补进根 `improvement-suggestions.md`）

```
| [rv32i-soc-platform-2026-11](.rddf/improvements/rv32i-soc-platform-2026-11.md) | P1 | 用户 2026-11-22「核做成真实可替换单元 + L1/L2/MMU/内存控制器/总线」· S1 已完成（ADR-077） | 2026-11-22 | 🟡 **in-progress**（S0 已闭 · S1 验收 13/13 · 下一批 S3） |
```

---

## metis 第 1 轮 gap 分析与处置（2026-11-22）

> metis 输出 **4 P0 + 7 P1 + 5 P2**，**全部已采纳**（处置明细保留在 `6358ebf` 的版本历史中）。
> 其中 **P2-20**（`--skip-specs`）**未采纳 metis 的建议**：实测先例 6/6 都是 no `specs/`（本项加一行数据 ⇒ 无 delta）。

## 2026-11-23 第二轮自我审查（依 S1 实测 + 用户「检查 IP 计划」指令）

| # | 发现 | 处置 |
|---|---|---|
| ① | 原批次表把 S4/S5 写成「只留索引」 | ✅ 改为**带 IP 的完整批次**（S3–S7），因为 S1 已证明框架能力够用 |
| ② | 原规划**没识别「核无 irq 口」**这个卡点 | ✅ 新增 §IP 计划 的「两个结构性卡点」+ **G-5** |
| ③ | 原规划**没识别「核无总线口 ⇒ 外设在 S3 前不可达」** | ✅ 写成**硬顺序约束**（S3 先于一切外设） |
| ④ | 原规划把「L2/内存控制器」放在 S4（早于总线化） | ✅ 调到 **S6**（必须晚于 S3/S4），并加 **R10** 明令不做"先做 L2" |
| ⑤ | 原规划的观测 IP 名/端口数与实现不符（7 端口 / `SocStoreCounter`） | ✅ 按实测回填（8 端口 / `SocTraceCounter` / 2 观测量） |
| ⑥ | 原规划未登记「MMIO 段缺失」 | ✅ 新增事实⑮ + **G-6** |
| ⑦ | 原规划未提「外部总线口 vs bundle 顶层端口限制」 | ✅ 新增 **G-8** + **R9** |
