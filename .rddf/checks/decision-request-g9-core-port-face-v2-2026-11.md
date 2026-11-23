# 决策请求 · G-9 + G-5：核端口面 v2（外设住址 · MMIO 通道 · `irq` 口）

> ## ✅ 已裁决（2026-11-24 · 用户）
>
> **G-9 = Y（外设住 SoC 层 · 核引 MMIO 通道）· G-9.2 = AXI4-Lite 主口 19 根 · G-9.3 = 核内 pre-cache 分流（MMIO 不被 D-L1 缓存）· G-9.4 = 核侧自带 MMIO master（不进库层公开面）· G-5 = `irq` 1 根（电平 · 高有效）**
>
> 生效范围：
> - **核端口面 v2** = 34 → **53**（S4a 后）→ **54**（S5 后）；契约表见 §五
> - **核 = pipeline + I-TCM + D-L1 + D-TCM + 新增 MMIO 旁路通道**
>   ⇒ ⇒ **S3 的 `pipeline → DCacheSync → TcmAxiSlave → DataTCM` 零返工**
> - **SoC 层** = 核的 MMIO 通道 → Lite 互联 → 外设 + 外设 IO 落根端口
> - **`irq` 契约现在定、实施在 S5**（与 `rv32i-privileged-csr-2026-11` 同批）
>   ⇒ ⇒ **S4a 不得先加这根口**（理由见 §三 G-5 的「死端口」先例）
> - ⚠️ **公开面变更**（`rv32i_mem_map.h` 扩段 · `address_decoder.h` 改派生 + 补片选）⇒ MUST 登记
>   `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md`；核端口面变更 ⇒ MUST 过 critic
> - ⚠️ **未裁决（随 S4a design）**：SoC 侧 Lite 互联的**形态**（2×2 + 级联 / 新写 N×M）
>   ⇒ ⇒ 见 §六 的三个待定项
> - ⚠️ **裁决顺序**：`G-7 ✅ → G-6 ✅ → G-9 ✅ / G-5 ✅（契约）→ (S4a/S4b/S4c) → G-5 实施（S5）→ G-8（S6 前）`

- **日期**：2026-11-24 · builder 起草（**待用户 / architect 裁决**）
- **母 improvement**：`rv32i-soc-platform-2026-11` 的决策门 **G-9**（新增）+ **G-5**（改判）
- **影响批次**：**S4**（核端口面 + SoC 装配）· **S5**（`irq` 口实施）· **S6**（根端口面）
- **前置已闭**：G-6（MMIO 归属 = 库层全表 + SoC 装配 · 2026-11-23）· G-7（总线类 = full AXI4 主干 · 2026-11-23）

## 一、要裁的是什么

> **S4 的外设住在哪里？核边界因此长成什么样？**

三个子问题：

| # | 问题 | 裁决 |
|---|---|---|
| **G-9.1** | 外设住**核内**还是**SoC 层**？ | **Y = SoC 层**（核引出 MMIO 通道） |
| **G-9.2** | 核边界的 MMIO 通道是**什么总线**？ | **AXI4-Lite 主口 · 19 根** |
| **G-9.3** | MMIO 与核内 D-L1 的关系？ | **pre-cache 分流** ⇒ MMIO **不进** D-L1 |
| **G-9.4** | 核侧 MMIO master 的代码住哪？ | **核侧自带**（不进库层公开面） |
| **G-5** | 核的 `irq` 输入口（契约 + 实施时机） | **契约现在定**（1 根 · 电平 · 高有效）· **实施在 S5** |

## 二、事实（全部实测 · 带坐标）

| # | 事实 | 证据 |
|---|---|---|
| ① | **核现在是 34 端口** = 框架 2（`default_clock` / `default_reset`）+ 核自有 32（4 IN + 28 OUT）⇒ 契约表住在**核的家** | `examples/riscv-mini/AGENTS.md`「核 IP 的 34 端口契约表」· `examples/riscv-mini/src/rv32i_core_top.h:48-113` |
| ② | **核内数据通路** = `pipeline → DCacheSync → TcmAxiSlave → DataTCM`（S3 接线）⇒ **TCM 完全在核内** | `rv32i_core_top.h:179-230` |
| ③ | 🔴 **`DCacheSync` 是 read-allocate + write-through**：读 miss ⇒ **4-beat burst fill**（`ARLEN=3` / `ARSIZE=2`）· 写 ⇒ 单 beat 透传 | `include/cpu/cache/d_cache_sync.h:7-8`（文件头规格）· `:209-213`（AR 发 4 beat） |
| ④ | 🔴 **fill 的完成条件 = `r_fire & fill_active & r_last`** ⇒ **slave 永不拉 `r_last` ⇒ fill 永不完成** | `d_cache_sync.h:233`（`fill_complete`） |
| ⑤ | 🔴 **AXI4-Lite 没有 `rlast`** ⇒ **Lite 外设不可能终结 cache 的 burst fill** ⇒ **MMIO 若走 D-L1 ⇒ 第一次 MMIO load 就永久挂死**（不是「读到陈旧值」，是**死锁**） | 事实③ + 事实④ + `include/axi4/peripherals/axi_gpio.h:35-66`（23 端口 · 无 `rlast`） |
| ⑥ | ⇒ ⇒ **MMIO MUST 绕过 D-L1** ⇒ 核内需要 **pre-cache 地址分流**（TCM 区间 → 现有 D-L1 通路；其余 → MMIO 通道）⇒ **S3 零返工** + **不改库层 `DCacheSync`** | 事实②③④⑤ |
| ⑦ | ✅ **`AxiLiteMatrix2x2` 的 m0 面 = 恰好 19 根**（`awaddr/awprot/awvalid/awready` · `wdata/wstrb/wvalid/wready` · `bresp/bvalid/bready` · `araddr/arprot/arvalid/arready` · `rdata/rresp/rvalid/rready`）⇒ 与 6 个 Lite 外设**同型** ⇒ 核边界直接接 m0，**零桥接** | `include/axi4/axi4_lite_matrix.h:41-63` |
| ⑧ | ✅ **Lite 互联自带 DECERR error responder** ⇒ **未映射地址返回错误响应**（不挂死） | `axi4_lite_matrix.h:8`（文件头）· `:301-302` · `:535`（读回 1 个 DECERR beat） |
| ⑨ | ⚠️ **Lite 互联只有 2 个 slave 窗口**（`Map::windows[0..1]`）⇒ S4 全量需 **CLINT + 6 外设 + DMA(full) = 8 个 slave** ⇒ 2×2 **不够** ⇒ 须级联或新写 N×M | `axi4_lite_matrix.h:239-250`（`static_assert` 只校验 windows[0..1]）· `include/axi4/peripherals/`（7 个外设） |
| ⑩ | ✅ **`AxiToAxiLite`（full→lite）强制 `LEN=0` / `BURST=INCR`** ⇒ **full↔lite 桥不承载 burst** ⇒ 平台的 **MMIO 侧天然是单拍** ⇒ 核边界不必是 full | `include/axi4/axi_to_axilite.h:304-314` |
| ⑪ | ⚠️ **库层没有 AXI master 组件**（只有 `AxiToAxiLite` full→lite 与 `AxiLiteToAxiAdapter` lite→full 两个**桥**）⇒ 核侧 MMIO master **必须新写** | `ls include/axi4/`（无 master）· `axi_to_axilite.h:221`（`AxiLiteToAxiAdapter`） |
| ⑫ | ✅ **有可参照的 master 实现**：`examples/axi4/axi_soc_example_top.h` 的 `AxiLiteTestMaster`（AW/W 相位分离 · VALID 保持到握手 · B/R 响应捕获）⇒ 但它是 **example 专属 + 事务式接口**（`start`/`write`，非 `req` 保持式）⇒ 不可直接复用 | `examples/axi4/axi_soc_example_top.h:37-140` |
| ⑬ | 🔴 **`AxiLiteGpio` 有 3 个必需端口**：`rst_n`（低有效复位 · **必须驱动**）· `gpio_in`（**必须驱动**）· `irq`（输出） | `include/axi4/peripherals/axi_gpio.h:60-66` |
| ⑭ | ✅ **Lite 矩阵**不重定基**（`s0_awaddr` 直通 `m0_awaddr`）⇒ 外设看到**绝对地址** ⇒ 与 `AxiLiteGpio` 的低位译码（reg0=DATA_OUT / reg1=DATA_IN / reg2=DIR / reg3=IRQ）**兼容** | `axi4_lite_matrix.h:327-329`（`fwd2` 直通）· `axi_gpio.h:113-115` |
| ⑮ | ✅ **`Map::windows[]` 已是可注入策略**（`AxiLiteMatrix2x2` / `AxiInterconnect4x4` 同型）⇒ 「库层定段（G-6）· SoC 装配」**零新机制** | `axi4_lite_matrix.h:29-32`（模板参数）· `:239-250`（编译期校验） |
| ⑯ | ⚠️ **D-TCM 窗口是 16 KB**（`kTcmAddrWidth=12` → `kTcmBytes=16 KB`）⇒ 今日**数据侧访问越界会静默截断别名**到 D-TCM ⇒ pre-cache 分流后这类访问改走 MMIO 通道 ⇒ **落进未映射窗口 ⇒ DECERR**（**可诊断化 · 有意的行为变化**） | `include/cpu/riscv/rv32i_mem_map.h:50/62/65`（文件头 `:13-26` 明写「静默截断」）· 事实⑧ |
| ⑰ | ⚠️ **SoC 根端口面会随外设增长**（ADR-072 D1：产品端口面 = 框架 2 + 根自有）⇒ 外设 IO 落根 ⇒ S4a 加 `gpio_out`/`gpio_in` ⇒ 根 8 → **10**；S4b 再加 UART/SPI/I2C/PWM ⇒ 增量追加 | `examples/riscv-soc/src/rv32i_soc_top.h:42-49`（根自有 6）· 先例 `examples/axi4/axi_soc_example_top.h`（根有 `gpio_out`/`gpio_in`/`uart_tx`/`uart_rx`） |
| ⑱ | ⚠️ **死端口先例**：`ext_clk` 因「被赋值但**零消费者**」被**物理删除**（2026-11-14）⇒ **在 S4a 先加 `irq` 口 = 复活同型死端口** | `examples/riscv-mini/src/rv32i_core_top.h:50-63`（删除理由的完整记录） |

## 三、选项对比

### G-9.1 · 外设住址

| 形态 | 核端口 | 后果 |
|---|---|---|
| X · 外设住核内 | 34 → 34+外设IO | 核变「SoC-in-a-core」⇒ 偏离 ADR-077 D1 两级顶层；AXI 仍藏在核内（与 G-7 的「全速主干」脱节） |
| **Y · 外设住 SoC 层（核引 MMIO 通道）** ✅ | 34 → 53 → 54 | 符合 ADR-077 D1 分工；**核保持可替换**（换核只需守 19 根 AXI-Lite 主口契约）；外设 IO 自然落根（ADR-072 D1） |
| Z · D-L1 + TCM 也移到 SoC | 大改 | 核最瘦，但 S3 刚接好的线要拆 ⇒ 且**已被否**（用户 2026-11-24） |

⇒ **X 并未避免端口变更**：`gpio_out` / `uart_tx` 终究要出现在顶层 ⇒ X 只是「把 AXI 藏起来」，代价是核不再可替换。

### G-9.2 · 核边界的总线类型

| 维度 | **AXI4-Lite 19 根（推荐）** | AXI4 full 25 根 | 简易 MEM 接口 7 根 |
|---|---|---|---|
| 与外设同型 | ✅ 6 个 Lite 外设 + 矩阵 m0 **直接对接** | ❌ 需 full→lite 桥（每个外设组一个） | ❌ 需在 SoC 侧写 master |
| 端口数 | 19 | 25（+6） | 7（−12） |
| burst | ❌ Lite 无 burst | ✅ 但**平台 MMIO 侧天然单拍**（事实⑩）⇒ 用不上 | 无 |
| 「可替换核」契约 | ✅ 标准总线 | ✅ 标准总线 | ❌ 自定义协议 ⇒ 换核要重写适配 |
| master 代码量 | 一样（都要写） | 一样 | 一样（挪到 SoC 侧） |
| 结论 | ✅ **推荐** | 端口 +6 且要桥 | 边界非标准 ⇒ 弱化 Y 的可替换性 |

### G-9.3 · MMIO 与 D-L1 的关系

| 形态 | 后果 |
|---|---|
| **pre-cache 分流（推荐）** | 核内按「地址 ∈ D-TCM 窗口？」分流 ⇒ TCM 走**现有** D-L1 通路（**S3 零返工**）· MMIO 走旁路 ⇒ **不被缓存** ⇒ 语义正确。代价：核内新增 1 个地址判据 + 1 组 ready/读数据 mux |
| 给 `DCacheSync` 加 PMA | 库层改公开面（加输入端口）⇒ 且 cache 与 MMIO 共用一个 AXI 主口 ⇒ 边界口被迫是 full ⇒ 变成 G-9.2 的 full 列 + 库层改 ⇒ **双重代价** |
| **承认 MMIO 被缓存** | ❌ **不可接受**：事实⑤ ⇒ 第一次 MMIO load **永久挂死**（fill 永不完成） |

### G-9.4 · 核侧 MMIO master 的代码归属

| 形态 | 后果 |
|---|---|
| **核侧自带（推荐）** | 它是「**核的数据通路适配器**」（`req`/`we`/`addr` → AXI 事务），不是通用总线 IP ⇒ 归属核 ⇒ **不进库层公开面**（省掉 S4a 的公开面登记 + critic 面）· 换核时新核自带 master ⇒ **SoC 侧零改动** ⇒ 恰好强化 Y 的可替换性 |
| 库层 `include/axi4/axi_lite_master.h` | 通用可复用 ⇒ 但要为 S4a 引入**新的公开面**（登记 + CHANGELOG + critic）⇒ 与「改动最小化」冲突。**后续有第二个消费者时再上移**（提升路径写进本材料） |

### G-5 · `irq` 口的契约与实施时机

| 项 | 裁决 |
|---|---|
| 端口 | `irq` · **INPUT** · 1 bit |
| 语义 | **电平触发 · 高有效**（「高 = 有待处理中断」）⇒ 边沿检测属 S5 的 `CsrBank` 内部职责 ⇒ **核口保持电平** |
| 驱动方 | SoC 侧的 **IRQ 汇聚器**（S5 新写）⇒ **SoC 根不需要暴露 `irq`**（外设 `irq` 输出全在 SoC 内部汇聚）⇒ 根端口面 8 → 10（S4a）**不受 irq 影响** |
| 消费方 | 核内的 trap 入口（依赖 `rv32i-privileged-csr-2026-11` ⇒ **仍是 draft**） |
| **实施批次** | **S5**（与 privilege 同批）⇒ **S4a 不加这根口** |
| **为什么 S4a 不能先加** | 事实⑱：`ext_clk` 因「被赋值但零消费者」被物理删除 ⇒ 先加 `irq` = **复活同型死端口** ⇒ 且零消费者 ⇒ 违反 ZERO-DEBT |

## 四、推荐（= 已裁决）

**G-9.1 = Y · G-9.2 = AXI4-Lite 19 根 · G-9.3 = pre-cache 分流 · G-9.4 = 核侧自带 · G-5 = 契约现在定 / 实施在 S5**

核的形态（裁决后的定稿）：

```
                    ┌──────────── Rv32iCoreTop（核 IP · 53 端口 → S5 后 54）────────────┐
  取指：            │  I-TCM（内部）                                                    │
  pipeline ────────►│     │                                                             │
  数据：            │  ┌──┴──────────────────────┐                                      │
  data_addr ──┬────►│  │ 判据：地址 ∈ D-TCM 窗口？│（派生自 rv32i_mem_map.h）              │
              │     │  └──┬──────────────────┬───┘                                      │
              │  TCM │     │ 是               │ 否                                     │
              │     ▼     ▼                  ▼                                          │
              │  D-L1（DCacheSync）    **MMIO 通道（新）**                              │
              │  → AXI4 full（内部）   → 核侧 AXI4-Lite master（新）                      │
              │  → TcmAxiSlave          │  axil_*（19 根 · 核边界）                    │
              │  → DataTCM             └──────────────────────────────────────────┤
              └─────────────────────────────────────────────────────────────────────┘
```

⇒ **S3 的接线零返工**（TCM 分支原样保留）⇒ **MMIO 不经 D-L1**（事实⑤ 的硬要求）⇒ **核边界 = 19 根标准 AXI4-Lite 主口**。

## 五、核端口面 v2（契约表 · 演进 34 → 53 → 54）

### 5.1 保留（34 根 · 一字不改）

框架 2（`default_clock` / `default_reset`）+ 核自有 32（4 IN：`ext_rst` / `prog_we` / `prog_addr` / `prog_data` · 28 OUT：`obs_*` 10 + `dbg_*` 17 + `obs_data_rdata` 1）
⇒ **契约表原文**见 `examples/riscv-mini/AGENTS.md`「核 IP 的 34 端口契约表」。

### 5.2 新增 · AXI4-Lite 主口（19 根 · **S4a 落地**）

| # | 端口（产物名前缀 `top_core_`） | 方向 | 位宽 | 语义 |
|---|---|---|---|---|
| 1 | `axil_awaddr` | OUTPUT | 32 | AW 地址（**绝对地址** · 不重定基） |
| 2 | `axil_awprot` | OUTPUT | 2 | 保护属性（**恒 0** · 平台无特权区分） |
| 3 | `axil_awvalid` | OUTPUT | 1 | AW 有效（**保持到 `awready`**） |
| 4 | `axil_awready` | INPUT | 1 | AW 就绪 |
| 5 | `axil_wdata` | OUTPUT | 32 | 写数据（**车道已对齐** · 同 `data_write_data` 约定） |
| 6 | `axil_wstrb` | OUTPUT | 4 | 字节使能（`SB`/`SH` 只写选中车道） |
| 7 | `axil_wvalid` | OUTPUT | 1 | W 有效（**保持到 `wready`**） |
| 8 | `axil_wready` | INPUT | 1 | W 就绪 |
| 9 | `axil_bresp` | INPUT | 2 | 写响应（`OKAY` / `SLVERR` / `DECERR`） |
| 10 | `axil_bvalid` | INPUT | 1 | 写响应有效 |
| 11 | `axil_bready` | OUTPUT | 1 | 写响应就绪（**恒拉高** · 无 back-pressure 需求） |
| 12 | `axil_araddr` | OUTPUT | 32 | AR 地址（绝对地址） |
| 13 | `axil_arprot` | OUTPUT | 2 | 保护属性（**恒 0**） |
| 14 | `axil_arvalid` | OUTPUT | 1 | AR 有效（保持到 `arready`） |
| 15 | `axil_arready` | INPUT | 1 | AR 就绪 |
| 16 | `axil_rdata` | INPUT | 32 | 读数据（**完成那拍捕获 · 保持**） |
| 17 | `axil_rresp` | INPUT | 2 | 读响应 |
| 18 | `axil_rvalid` | INPUT | 1 | 读数据有效 |
| 19 | `axil_rready` | OUTPUT | 1 | 读就绪（**恒拉高**） |

⇒ **无 ID**（Lite 无 ID）⇒ 无 `awlen`/`arlen`（Lite 无）⇒ 无 `rlast`/`wlast`（Lite 无）。

### 5.3 新增 · `irq`（1 根 · **S5 落地**）

| # | 端口 | 方向 | 位宽 | 语义 |
|---|---|---|---|---|
| 20 | `irq` | INPUT | 1 | **电平触发 · 高有效**（高 = 有待处理中断）· 驱动方 = SoC 侧 IRQ 汇聚器 |

### 5.4 计数与载体

| 时点 | 框架 | 核自有 | 核自有 IN | 核自有 OUT | **核端口总数** |
|---|---|---|---|---|---|
| 现状 | 2 | 32 | 4 | 28 | **34** |
| **S4a 后** | 2 | 51 | 4 + 8 = 12 | 28 + 11 = 39 | **53** |
| **S5 后** | 2 | 52 | 13 | 39 | **54** |

⇒ ⚠️ **两次端口表变更都是「增量追加」**（+19 行 · 再 +1 行）⇒ **不是全量重算两遍**
⇒ 必须同批重算的载体（**S4a 一次 / S5 一次**）：

| 载体 | 位置 | S4a | S5 |
|---|---|---|---|
| 核端口契约表 | `examples/riscv-mini/AGENTS.md` | 34 → 53 | 53 → 54 |
| C6 快照 | `scripts/verify_no_duplicate_rv32i_headers.sh` | 32 → 51 条 | 51 → 52 条 |
| 期望表 | `scripts/verify_riscv_soc_core_ports.sh`（V1/V2 从契约表解析） | 随契约表 | 随契约表 |
| SoC 根端口表 | `examples/riscv-soc/AGENTS.md` + `rv32i_soc_top.h` | 8 → **10**（+`gpio_out` / `gpio_in`） | 不变（`irq` 在 SoC 内部汇聚） |
| 覆盖 tsv | `scripts/verify_examples_rtl_sim_coverage.sh` 的登记表 | 追加 S4a 行 | — |
| 公开面登记 | `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` | 库层 2 个头 | — |

## 六、S4a 的连带工作量（供 design 参考 · 不属本裁决）

| 项 | 归属 | 内容 |
|---|---|---|
| 库层 `rv32i_mem_map.h` 扩 7 段 | G-6 落点 | `kClint` / `kUart` / `kGpio` / `kSpi` / `kI2c` / `kPwm` / `kDma`（每段 4 KB）· **公开面变更** |
| `address_decoder.h` 改派生 + 补 4 片选 | G-6 落点 | 7 片选 + `error_cs` · 6 处测试消费者（`test_address_decoder`）同步 |
| 核内 pre-cache 分流 | S4a design | 判据派生自 `rv32i_mem_map.h` 的 `kDtcm` ⇒ 单一真相；`data_ready` / `data_read_data` 各需一个 mux |
| 核侧 AXI4-Lite master | S4a design | 新写（参照 `AxiLiteTestMaster` 的握手纪律，但接口是**保持式** `req`）· 落 `examples/riscv-mini/src/` |
| SoC 侧 Lite 互联 | S4a design | ⚠️ **三个待定项**：<br>① **形态**：2×2 只有 2 slave（事实⑨）⇒ S4a 只接 1 个外设够用，但全量需 8 ⇒ 级联 or 新写 N×M<br>② **未用 slave 口的处理**：`s1_*` 是矩阵的 `ch_in` ⇒ **必须驱动** ⇒ 若 window[1] 落在真实 MMIO 区则访问会**挂死** ⇒ 建议 window[1] 放在**真实 MMIO 之外** ⇒ 未接地址落进事实⑧ 的 **DECERR**（可诊断、不挂死）<br>③ **DMA（full 主）** ⇒ 需 `AxiLiteToAxiAdapter`（lite→full）⇒ 归 S4c |
| GPIO 端到端 | S4a | 写 `0x8010_2000`（`kGpio.base`）⇒ `gpio_out == 0xA5`（手推）· 读 `0x8010_2004` ⇒ `DATA_IN` |
| 未驱动端口清点 | S4a | `AxiLiteGpio.rst_n` / `gpio_in` · `AxiLiteMatrix2x2.rst_n` / `s1_*`（8 根）· 核内 master 的复位来源 ⇒ **全部必须显式驱动**（本仓 ANTI-PATTERN） |

## 七、裁决后的落点

| 裁决 | 落点 |
|---|---|
| G-9 全部 | 核端口契约表 §5（本材料）⇒ 实施在 **S4a**（AXI 部分）+ **S5**（`irq` 部分） |
| G-5 | 契约见 §3 G-5 表 · 实施在 **S5**（与 privilege 同批）⇒ **S4a 不得先加** |
| 全部 | ⇒ **S4a 立项**（proposal / design / tasks）⇒ 范围 = §六 的 7 行 |
| 全部 | ⇒ S4a 涉及**公开面变更**（`rv32i_mem_map.h` / `address_decoder.h`）⇒ MUST 登记 `public-api-surface.md` + `docs/CHANGELOG.md`；核端口面变更 ⇒ MUST 过 **critic**（`review_profile: build`） |
