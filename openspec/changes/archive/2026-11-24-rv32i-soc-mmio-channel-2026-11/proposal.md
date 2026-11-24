# Change: RV32I 核的 MMIO 通道 + SoC 侧 Lite 互联 + GPIO 端到端回环（S4a）

> **Change 名**：`rv32i-soc-mmio-channel-2026-11`
> **母 improvement**：`rv32i-soc-platform-2026-11`（批次 **S4a** · 状态 `in-progress`）
> **决策依据**：
> - **G-6 已裁决** = MMIO 地址表归属 **库层全表 + SoC 装配**（2026-11-23 · 用户）
>   ⇒ `.rddf/checks/decision-request-g6-mmio-ownership-2026-11.md`
> - **G-9 已裁决** = **Y（外设住 SoC 层 · 核引 AXI4-Lite 主口 19 根）· pre-cache 分流 · 核侧自带 master**（2026-11-24 · 用户）
>   ⇒ `.rddf/checks/decision-request-g9-core-port-face-v2-2026-11.md`
> **日期**：2026-11-24 · builder
> **性质**：**新增产品能力**（核的 MMIO 通道 + SoC 侧互联与首个外设）· 含 spec **ADDED**（新 capability `rv32i-mmio-channel`）
> ⇒ ⚠️ **含公开面变更**（`rv32i_mem_map.h` / `address_decoder.h`）⇒ MUST 登记
> `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md`
> ⇒ ⇒ ⚠️ **含核端口面变更**（34 → **53**）⇒ MUST 同批重算 C6 快照 / 契约表 / 护栏期望 ⇒ MUST 过 **critic**
> ⇒ ⇒ 🔴 **硬前置**：`rv32i-commit-event-pulse-2026-11`（**S4a-0**）—— 见 §Why 第 5 条

---

## ⚠️ 订正块（2026-11-24 · S4a-0 实施期实测 · **本块优先于下文任何相冲突的表述**）

> **触发**：S4a-0（硬前置）的实施期实测**改变了「store 事件」的实现形态** ⇒ ⇒ 本 change 的
> **§Why 第 6 条**与 **design §2.2** 的相关表述 MUST 一并订正。
> ⇒ ⇒ **AC3 的手推终值 `(2,6)` 不变**（每个 store / 每个写回各计一次）⇒ ⇒ **裁决结论不变**。

| # | 原表述 | 订正 |
|---|---|---|
| **D1** | §Why 第 6 条：「`obs_data_write_en` = `exmem_is_store`（`rv32i_pipeline.h:693`）⇒ **电平**」 | **观测面**已由 S4a-0 改为 **`data_write_en & data_ready_calc`**（**核顶层总线侧**派生 · `examples/riscv-mini/src/rv32i_core_top.h`）⇒ ⇒ **脉冲** ⇒ ⇒ §Why 第 6 条的「电平」描述**只对功能信号** `pipeline.io().data_write_en` 成立（它**仍是电平**，且 **MUST 保持电平**） |
| **D2** | design §2.2：「`data_write_en` MUST NOT 分流」 | ✅ **结论不变**（功能信号**不分流** ⇒ ⇒ store 事件**与路径无关** ⇒ ⇒ 判别力更强）⇒ ⇒ **但**派生式里的 `data_ready_calc` **必须**是**路径 mux 后**的就绪（`in_mmio ? master.ready : cache.ready`）⇒ ⇒ 本 change 实施时 MUST 用**同一个** mux 后的表达式 ⇒ ⇒ 见 tasks 的 **T2.5** |
| **D3** | 「store 事件的判别力」 | S4a-0 落地时**今日不可判别**（今日 store 路径不反压 ⇒ **恒绿** ⇒ 恒绿判据已撤除）⇒ ⇒ **判别力归本 change**（MMIO 事务必然多拍 ⇒ 那时才可判别）⇒ ⇒ 见本 change 的 **AC3** |

### 订正块（2026-11-24 · critic 轮次 1 的 BLOCKER 闭合）

| # | 原表述 | 订正 | 依据 |
|---|---|---|---|
| **D4** | AC7 的「`interp_checks == min_checks == **9**`」 | ⇒ **10**（+A9 读值回路） | critic BLOCKER：`data_read_data` 的 mux 用**当拍** `addr_in_mmio` 选路径 ⇒ 而消费发生在 **M2**（那拍已是后继指令的地址）⇒ **静默错源**。原 9 条判据对它**结构性失明**（读值不在 SoC 根端口 · `gpio_in` tie 0 ⇒ 错源与正源碰巧同值） |
| **D5** | AC2「读回的**值**在 SoC 级**不可观测** ⇒ 归 `AxiLiteGpio` 的组件级用例」 | ⇒ **不成立**：缺陷不在 GPIO（它的 `rdata` 是对的），而在核顶层 mux + M2 消费时序这一**集成段** ⇒ ⇒ 新增 **A9** 在 SoC 级直接判读值回路（`gpio_in`=0x5A → `lw` → 非 MMIO 后继 → `sw` → `gpio_out` MUST 回 0x5A） | 同上 |
| — | （无需改的）AC1 / AC3 / AC4 / AC5 / AC6 / AC8 / AC9 | 全部保持原值 | — |

> ⚠️ **D4/D5 是判据面的追加，不改任何决策**（G-6 / G-9 的实施形态不变）⇒ ⇒ 已同步
> `.rddf/checks/soc-rtl-expectations.md` §3/§4 · `examples-rtl-sim-targets.tsv` · 两个 TB 的 `checks=10`。
> ⇒ ⇒ **planner 复核点**：A9 属「新判据」而非「改 AC 语义」⇒ 若 planner 认为它应独立成 change，可回退为独立项。

## Why

1. **外设至今不可达**：核在 S3 后有了 AXI4 主口，但那条口**只接到核内的 `TcmAxiSlave`** ⇒ ⇒
   **SoC 级的任何外设都够不着**（核只有 `prog_*` 后门能写 I-TCM）⇒ ⇒ 这是 S4 的**第一前提**。

2. 🔴 **MMIO MUST 绕开 D-L1 —— 否则第一次 MMIO load 就永久挂死**（不是「读到陈旧值」，是**死锁**）：
   - `DCacheSync` 的 fill 完成条件 = `r_fire & fill_active & r_last`
     （`include/cpu/cache/d_cache_sync.h:233`）⇒ ⇒ **必须**收到 `r_last`；
   - AXI4-Lite **没有 `rlast`**（6 个 Lite 外设的端口表里没有它 ·
     `include/axi4/peripherals/axi_gpio.h:35-66`）⇒ ⇒ **Lite 外设不可能终结 4-beat burst fill**；
   - ⇒ ⇒ **MMIO 走 D-L1 ⇒ fill 永不完成 ⇒ `ready` 恒 0 ⇒ 流水线永久冻结**。
   ⇒ ⇒ 核内 MUST 有 **pre-cache 分流**：**MMIO 段**不进 D-L1。

3. ⚠️ **分流判据的方向（MUST 是「MMIO 段」而不是「TCM 段」）**：
   - `kDtcm = {0x8001_0000, 16 KB}`（`include/cpu/riscv/rv32i_mem_map.h:65`）
     ⇒ ⇒ **字节地址 0 ∉ D-TCM 窗口**；
   - 核级与 SoC 级程序**都**含 `sw x3, 0(x0)`（**字节地址 0**）
     ⇒ ⇒ 若判据写成「地址 ∈ D-TCM 窗口？」⇒ ⇒ 该 store 会走 **MMIO 旁路**
     ⇒ ⇒ 核级 TB 无 MMIO 从设备（AXI 输入 tie 0）⇒ ⇒ **核级 TB 永久挂死**；
   - ⇒ ⇒ 判据 MUST 是「**地址 ∈ MMIO 段 ⇒ 旁路；其余 ⇒ 现有 D-L1 通路**」
     ⇒ ⇒ **零行为变化** ⇒ ⇒ 核级 24 判据 / 核级 `.sv` TB / 既有期望台账**全不改**。
   ⇒ ⇒ 详见 G-9 决策材料的**文首订正块**（2026-11-24 · `a467df1`）。

4. **G-6 / G-9 已裁但未落地**：MMIO 地址表的归属（库层全表）与核边界形态（AXI4-Lite 主口 19 根）
   已在 2026-11-23 / 2026-11-24 裁完 ⇒ ⇒ 本 change 是它们的**实施**。

5. 🔴 **S4 的互联窗口不够（连带事实）**：S4 全量需 **6 外设 + DMA(full) = 7 个 slave**，
   而 `AxiLiteMatrix2x2` 只有 **2** 个（`include/axi4/axi4_lite_matrix.h:239-250` 的
   `static_assert` 只校验 `windows[0..1]`）⇒ ⇒ 全量形态（级联 / 新写 N×M）归 **S4b**；
   ⇒ 本 change 只接 **1 个外设（GPIO）** ⇒ 2×2 够用。

6. 🔴 **硬前置 = 「提交事件端口脉冲化」（S4a-0）**：
   - `obs_data_write_en` = `exmem_is_store`（`include/cpu/pipeline/rv32i_pipeline.h:693`）
     而 `exmem_is_store` 在 `mem_stall` 时**冻结**（`:568`）⇒ ⇒ **电平**；
   - `dbg_wb_write_en` = `wb_stage.io().write_en`（`:891`）而 `write_en` 依赖 `mem_valid`
     （`include/cpu/pipeline/stages/wb_stage.h:110`）⇒ ⇒ MEM/WB 冻结 ⇒ ⇒ **电平**；
   - ⇒ ⇒ **MMIO 事务必然多拍 ⇒ 必然 `mem_stall` ⇒ 必然灌水**
     ⇒ ⇒ 「事件次数」被污染成「事件时长之和」⇒ ⇒ **计数终值不可手推**；
   - ⇒ ⇒ 今日之所以绿（实测 `store_count == 1`）是**巧合** —— 现有程序**只有 `sw`**
     （write-through 单拍 ⇒ 无 fill ⇒ 无 `mem_stall`）；
   - ⇒ ⇒ **S4a MUST 先做 S4a-0**（change `rv32i-commit-event-pulse-2026-11`
     ⇒ ⇒ 零行为变化 · 零端口面变化 · 零 spec ADDED）。

7. ⚠️ **本 change 会改动已签批 ADR-077 的两个数字** ⇒ ⇒ 需 **dated addendum**（不改决策）：
   - **V1**「根 `__io` == 6 ⇒ 产品端口 == 8」⇒ S4a 后根 `__io` == **8** ⇒ 产品端口 == **10**
     （外设 IO 落根 = ADR-072 D1 的必然结果，因 G-9 把外设放到了 SoC 层）；
   - **V2**「产物 module 集合 == 10」⇒ S4a 后 == **12**（新增 `AxiLiteMatrix2x2` / `AxiLiteGpio` 两个成层 IP）。
     ⇒ ⇒ **决策本身（两级顶层 / `prog_*` 长期保留 / 模块类型名非契约）不变** ⇒ ⇒ 只补数字的 dated addendum。

## What Changes

| 面 | 内容 |
|---|---|
| **库层** `include/cpu/riscv/rv32i_mem_map.h` | **扩成全表**：新增 7 个 MMIO 段（`kClint` / `kUart` / `kGpio` / `kSpi` / `kI2c` / `kPwm` / `kDma`，每段 **4 KB**，自 `0x8010_0000` 顺延）+ **MMIO 跨段常量**（`kMmioBase` / `kMmioBytes` + 连续性 `static_assert`）⇒ **公开面变更** |
| **库层** `include/cpu/riscv/address_decoder.h` | 3 段硬编码（`:43-45`）改**派生**自上表 + 补 4 个片选（`spi_cs` / `i2c_cs` / `pwm_cs` / `dma_cs`）⇒ ⇒ **总片选 9**（2 TCM + 7 MMIO）+ `error_cs` ⇒ **10** 个片选输出端口 ⇒ **公开面变更** + 6 处测试消费者同步 |
| **核内**（新增头）`examples/riscv-mini/src/axil_mmio_master.h` | **核侧 AXI4-Lite 主口**（新写）：`req`/`we`/`addr`/`wdata`/`strbe` → 19 根 AXI4-Lite 主口；AW/W **相位分离**；VALID **保持到握手**；B/R 响应捕获；`ready` = 完成脉冲 |
| **核内** `examples/riscv-mini/src/rv32i_core_top.h` | ① **pre-cache 分流**（**MMIO 段 ⇒ 旁路；其余 ⇒ 现有 D-L1 通路** ⇒ **S3 接线零返工**）② **+19 根核边界端口**（`axil_*`）③ `data_ready` / `data_read_data` 各加一个 mux |
| **SoC** `examples/riscv-soc/src/rv32i_soc_top.h` | +2 根根端口（`gpio_out` / `gpio_in`）· 装配 `AxiLiteMatrix2x2<32,32,Map>` + `AxiLiteGpio<8>`（**成层 IP** ⇒ 产物 module 集合 10 → 12）· `Map::windows[]` 从库层段派生 |
| **判据** `examples/riscv-soc/tb_riscv_soc.cpp` + `tb_riscv_soc.sv` | SoC 级程序扩 **4 字**（12 字 · 见 §程序）⇒ ⇒ 计数期望 `(1,3)` → **`(2,6)`**（手推 · **事件次数**口径）⇒ 新增 2 条判据（写值 / 读完成）⇒ 两端**逐条同值** ⇒ ⇒ 判据条数 **7 → 9** |
| **核级** `examples/riscv-mini/tb_rv32i_soc.cpp` + `tb_rv32i_soc.sv` | 核的 8 根 AXI-Lite **输入**端 MUST 显式 tie（`set_input_value` / `.sv` 侧 tie 0）⇒ ⇒ 核级 24 判据**零变化**（程序与行为均不变） |
| **登记面** | 核端口契约表 34 → **53**（`examples/riscv-mini/AGENTS.md`）· C6 快照 32 → **51** 条（`scripts/verify_no_duplicate_rv32i_headers.sh`）· `scripts/verify_riscv_soc_core_ports.sh` 的硬编码 34 → **53** · SoC 产品面表 8 → **10** 端口 / 10 → **12** module（`examples/riscv-soc/AGENTS.md` + `.rddf/checks/soc-rtl-expectations.md#riscv-soc`）· `public-api-surface.md` + `docs/CHANGELOG.md` · ADR-077 dated addendum |

### 程序（SoC 级 · **12 字** · 与核级**分叉**）

⚠️ **SoC 级程序 MUST 与核级程序分叉** —— 理由（**结构性**，不是偏好）：
核级（`examples/riscv-mini`）**没有 MMIO 通路**（核级 TB 的 AXI-Lite 输入全 tie 0）
⇒ ⇒ 核级程序 **MUST NOT** 做 MMIO 访问（否则挂死）
⇒ ⇒ SoC 级程序**额外**含 MMIO 访问 ⇒ ⇒ 两个 TB 的程序**必然不同**
⇒ ⇒ ⇒ `examples/riscv-soc/tb_riscv_soc.sv` 头部「⚠️ **程序与手推台账同源于核级产品**」
**MUST 改写** ⇒ ⇒ `.rddf/checks/soc-rtl-expectations.md#riscv-soc` §2 **MUST 独立登记**。

| word | 指令 | 编码（**手推**） | 效果 |
|---|---|---|---|
| 0 | `addi x1, x0, 42` | `0x02A00093` | 写回 x1 = 42 |
| 1 | `nop` | `0x00000013` | — |
| 2 | `addi x2, x0, 11` | `0x00B00113` | 写回 x2 = 11 |
| 3 | `nop` | `0x00000013` | — |
| 4 | `add x3, x1, x2` | `0x002081B3` | 写回 x3 = 42 + 11 = **53** |
| 5 | `sw x3, 0(x0)` | `0x00302023` | store 字节地址 **0**（**走 D-L1** · write-through 单拍）· 数据 53 |
| 6 | `jal x0, 0` → **移至 word 11** | `0x0000006F` | 自环（rd = x0 ⇒ 不写回） |
| 7 | `lui x4, 0x80102` | `0x80102237` | 写回 x4 = `0x80102 << 12` = **0x8010_2000** = `kGpio.base` |
| 8 | `lw x6, 4(x4)` | `0x00422303` | **MMIO 读**（`0x8010_2004` = DATA_IN）· 写回 x6 |
| 9 | `addi x5, x0, 0xA5` | `0x0A500293` | 写回 x5 = **0xA5**（165 < 2048 ⇒ 单条 `addi` 足够） |
| 10 | `sw x5, 0(x4)` | `0x00522023` | **MMIO 写**（`0x8010_2000` = DATA_OUT）· 数据 **0xA5** |
| 11 | `jal x0, 0` | `0x0000006F` | 自环（rd = x0 ⇒ 不写回） |

⇒ ⇒ **写回事件 = 6**（x1 / x2 / x3 / x4 / x6 / x5）· **store 事件 = 2**（word 5 · word 10）
⇒ ⇒ **`jal x0,0` 的 `rd = x0` ⇒ 不写回** ⇒ ⇒ `lw x6` **写回**（load 也写回 rd）
⇒ ⇒ ⚠️ **两条路径都被覆盖**：word 5 走 **D-L1**（地址 0）· word 8/10 走 **MMIO 旁路**
⇒ ⇒ ⇒ ⇒ **分流判据写反 ⇒ word 5 落进 MMIO ⇒ store 事件少 1 ⇒ 转红** ⇒ ⇒ 判别力自带。
⇒ ⇒ ⚠️ **编码 MUST 机器复验**（本仓铁律：手推编码不许靠肉眼）⇒ ⇒ 实施时在 TB 里
`static_assert(kProgram[i] == kHandEncoded[i])`（同 `examples/riscv-mini/tb_rv32i_soc.cpp` 的形态）
⇒ ⇒ ⚠️ **本次起草时机器复验抓到一处笔误**：`lw` 是 **I 型**（`imm[11:0]` 占 bits[31:20]），
初稿误按 **S 型**算成 `0x00622223`（实测解回是 `STORE`）⇒ ⇒ 已订正为 `0x00422303`
⇒ ⇒ ⇒ **「机器复验」不是形式，是能抓错的**。

## 用户可见变更

> 本节是归档门 `verify_public_api_visibility` **R1** 的载体（标题 MUST 逐字为「用户可见变更」）。

| 面 | 变更 | 影响 |
|---|---|---|
| **核端口契约**（`examples/riscv-mini/AGENTS.md`） | **34 → 53**：+19 根 `axil_*`（AXI4-Lite 主口 · 11 OUT + 8 IN） | 换核 / 接线方 MUST 处理 19 根新端口；核级 TB 的 8 根输入 MUST tie 0 |
| **SoC 根端口**（ADR-077 V1 的 dated addendum） | **8 → 10**：+`gpio_out` / `gpio_in`（8 bit） | 根消费者可见新 GPIO 物理面；`gpio_dir` / `irq` **不落根** |
| **SoC 产物 module 集**（ADR-077 V2 的 dated addendum） | **10 → 12**：+2×2 Lite 互联 + GPIO 外设 | 层次化产物多两个 module |
| **库层公开符号** | +`riscv::kMmioBase` / `kMmioBytes` / `kMmioSegmentBytes` · 7 个 MMIO 段（`kClint` … `kDma`）· `riscv::in_mmio()` · `riscv::LUI()` · `ADDR_{SPI,I2C,PWM,DMA}_*` | **纯新增**（既有取值逐项不变 ⇒ 既有消费者零改动）；`LUI` 补 `AUIPC` 的另一半（与 PC 无关的基址装载） |
| **`riscv::AddressDecoder` 端口面** | **+4 输出**（`spi_cs` / `i2c_cs` / `pwm_cs` / `dma_cs` ⇒ 总片选 **9** + `error_cs`） | 新增片选 **MUST** 被消费或**显式 tie 0**；既有 5 个片选取值**逐项不变** |
| **行为** | MMIO 段（`0x8010_0000` 起 7×4 KB）**可达**：写通路、读通路、未映射地址回 **DECERR**（程序继续） | **非 MMIO 路径零行为变化**（核级 24 判据 · 既有 SoC 计数语义不变） |
| **潜在缺陷面（已修）** | MMIO 请求**不进 D-L1**（否则 store-to-MMIO 会在 D-TCM 的 AXI 从口发起**别名地址写** ⇒ 静默污染） | 下游若自行改造分流点，MUST 保留该门控 |

## spec delta（**ADDED** · 不改任何已签批 Requirement）

- **新增 capability `rv32i-mmio-channel`**（核的 MMIO 通道契约）：
  1. 核 MUST 暴露 **AXI4-Lite 主口**作为 MMIO 通道（19 根 · 无 ID · 无 burst）；
  2. **MMIO 段 MUST NOT 由 D-L1 服务**（理由 = fill 靠 `r_last` ⇒ Lite 终结不了 ⇒ 挂死）；
  3. 分流判据 MUST **派生自库层地址表**且方向 MUST 是「**MMIO 段**」（禁手写字面量）；
  4. 通道 MUST 是**单拍**语义（Lite 无 burst）。
- ⚠️ **不改** `cpu-cache-subsystem`（它没有「cache 服务所有地址」的条款 ⇒ 无冲突）
- ⚠️ **不改** `rv32i-memory-timing`（它约束的是 **TCM 的读口**（1 拍 `sread`）⇒ MMIO 通道在其**之外**）
- ⚠️ **不改** `rtl-soc-output`（该 capability 的端口面 / module 数条款是**计数无关**的
  ⇒ 「顶层端口表 == 框架 2 + 根自有 `__io`」「module 数 == 成层 IP 数 + 1」**自动成立** ⇒ 零 delta）
- ⚠️ **不改** `example-component-boundary`（新头生在 `examples/riscv-mini/src/` ⇒ 符合「example 专用头生在该 example 目录内」）

## 验收标准

> 期望值 MUST **手推**（禁抄仿真输出）· 事务参数 MUST 从端口声明 / 地址表派生。
> ⚠️ **手推台账**：`.rddf/checks/soc-rtl-expectations.md#riscv-soc`（SoC 级 · **本 change 改写 §2 / §3**）
> 与 `.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc`（核级 · 本 change **不改**）。
> 🔴 **计数口径 = 「事件次数」**（与事件**时长**无关）⇒ ⇒ **依赖 S4a-0 已落地**。

- [ ] **AC1** 🔴 **MMIO 写通路 · 值**：SoC 级程序把 `0xA5` 写到 `0x8010_2000`（`kGpio.base` + 0
  = DATA_OUT 寄存器）⇒ ⇒ 根端口 `gpio_out == 0xA5`（**手推**：`AxiLiteGpio` 的 reg0 = DATA_OUT，
  `include/axi4/peripherals/axi_gpio.h:113`）
- [ ] **AC2** 🔴 **MMIO 读通路 · 完成**：SoC 级程序读 `0x8010_2004`（DATA_IN）⇒ ⇒ 事务**完成**
  ⇒ ⇒ 程序继续 ⇒ ⇒ **计数到达手推终值 `(store=2, wb=6)`**（挂死 ⇒ 计数冻结 ⇒ 必红）
  ⇒ ⚠️ **读回的**值在 SoC 级**不可观测**（`x6` 不是根端口 ⇒ 后代信号不可观测）⇒ ⇒
  **值**的正确性归 `AxiLiteGpio` 的既有组件级用例承担 ⇒ ⇒ 本 AC 只判「**完成**」
- [ ] **AC3** 🔴 **手推计数终值（事件次数口径）**：`store_count == 2`（2 条 `sw`）·
  `writeback_count == 6`（`x1`/`x2`/`x3`/`x4`/`x6`/`x5`；`jal x0` 与 `rd=x0` **不写回** ⇒ 不计）
  ⇒ ⇒ **两值不同** ⇒ ⇒ 事件源接反 / 同源时必红
  ⇒ ⇒ 🔴 **口径 MUST 是「事件次数」**（**不是**「电平拍数」）⇒ ⇒ **MUST 先落地 S4a-0**
  ⇒ ⇒ ⇒ 变异自证 = 去掉 `& !mem_stall` ⇒ ⇒ 计数变为「3 + 停顿拍数」⇒ ⇒ **必红**
- [ ] **AC4** 🔴 **核级 24 判据零变化**：`examples/riscv-mini` 的 `RESULT: checks=24 fails=0`
  ⇒ ⇒ **程序与计数均不变** ⇒ ⇒ 证明 pre-cache 分流**没有**改动非 MMIO 路径的行为
  ⇒ ⇒ （核级 TB 的 8 根 AXI-Lite 输入 tie 0 ⇒ ⇒ **无 MMIO 通路** ⇒ ⇒ 核级程序 MUST NOT 做 MMIO 访问）
- [ ] **AC5** 🔴 **端口面** = 核 **53** 根（框架 2 + 核自有 51 = 12 IN + 39 OUT）
  · SoC 根 **10** 根（框架 2 + 根自有 8）· 产物 module 集合 **12**
  ⇒ ⇒ `scripts/verify_riscv_soc_core_ports.sh` V1/V2/V3 全绿
- [ ] **AC6** 🔴 **未驱动输入 = 0**：SoC 装配的**每一个**后代输入 MUST 有驱动源
  ⇒ ⇒ 逐项清点（**11 根 tie 0** 见 design §4）：
  | 组件 | 输入端口 | 驱动源 |
  |---|---|---|
  | 核 `Rv32iCoreTop` | 8 根 `axil_*` 输入（`awready` / `wready` / `bresp` / `bvalid` / `arready` / `rdata` / `rresp` / `rvalid`） | matrix 的 `m0_*` 输出 |
  | 核侧 master | — | 核内部（**不新增核端口**） |
  | `AxiLiteMatrix2x2` | **m1 面 11 根**（`awaddr` / `awprot` / `awvalid` / `wdata` / `wstrb` / `wvalid` / `bready` / `araddr` / `arprot` / `arvalid` / `rready`） | **tie 0**（m1 永不使用） |
  | `AxiLiteMatrix2x2` | **s1 面 8 根**（`awready` / `wready` / `bresp` / `bvalid` / `arready` / `rdata` / `rresp` / `rvalid`） | **tie 0**（s1 未接 slave） |
  | `AxiLiteMatrix2x2` | `rst_n` | SoC 根（`!ext_rst`） |
  | `AxiLiteGpio` | `rst_n` · `gpio_in` | SoC 根（`!ext_rst`）· 根端口 `gpio_in` |
  ⇒ ⇒ 发射 **fail-closed** 判据绿（零未驱动）
- [ ] **AC7** 🔴 **双形态逐拍全等**：SoC 的扁平 ≡ 层次化 ⇒ ⇒ `check_products` 4 项全绿
  （module 集 / 顶层端口 / 未驱动 / lint / 双形态）+ **`.sv` TB 与 C++ 侧逐条同值**
  （`interp_checks == min_checks == 9`）
- [ ] **AC8** 🔴 **未映射地址 ⇒ DECERR（不挂死）**：SoC 级程序访问 `0x8010_0000`（`kClint.base`
  · **S4a 未接 CLINT** ⇒ 该窗口落在 `Map::windows[]` 之外）⇒ ⇒ 事务 MUST **返回 DECERR**
  ⇒ ⇒ 程序 MUST **继续**（⇒ ⇒ 挂死 ⇒ 必红）
  ⇒ ⇒ ⚠️ **该判据在 S4b 接上 CLINT 后 MUST 改**（登记在 S4b 的 tasks 里）
- [ ] **AC9** 🔴 **零回归**：`ctest` 全绿（**275**）· `run_all_ported_tests` PASS · **全部 `verify_*.sh` 全绿**
  ⇒ ⇒ 既有 `test_address_decoder`（6 处实例化）随片选扩展同步更新

## Out of Scope（本 change 明确不做）

- **其余 5 个外设**（UART / Timer / SPI / I2C / PWM）+ `AxiDma` ⇒ **S4b / S4c**
- **取指侧总线化 + I-L1** ⇒ **S4c**
- **Lite 互联的全量形态**（≥ 7 slave：级联 / 新写 N×M）⇒ **S4b design**
- **`irq` 口**（G-5 的实施）⇒ ⇒ **S5**（与 `rv32i-privileged-csr-2026-11` 同批）
- **外部总线口上根端口**（G-8）⇒ 未裁
- 🔴 **数据侧 TCM 窗口收紧**（今日越界访问会**静默别名**进 D-TCM · `rv32i_mem_map.h:13-26` 已登记）
  ⇒ ⇒ 本 change **不修**（分流判据是「MMIO 段」⇒ ⇒ 越界访问**仍走 D-L1** ⇒ ⇒ 行为**零变化**）
  ⇒ ⇒ 登记为后续项
- 🔴 **提交事件端口脉冲化**（`obs_data_write_en` / `dbg_wb_write_en` 由电平改脉冲）
  ⇒ ⇒ **S4a-0**（`rv32i-commit-event-pulse-2026-11`）· **本 change 的 AC3 依赖它**
