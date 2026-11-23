> ## ✅ 已裁决（2026-11-23 · 用户）
>
> **G-6.1 = A（库层全表 + SoC 装配）· G-6.2 = 按 4KB 顺延 · G-6.3 = 改派生 + 补 4 片选**
>
> 生效范围：
> - 库层 `include/cpu/riscv/rv32i_mem_map.h` **扩成全表**（新增 7 段，`MemRegion` 形态）：
>   `kClint` `0x8010_0000` / `kUart` `0x8010_1000` / `kGpio` `0x8010_2000` /
>   `kSpi` `0x8010_3000` / `kI2c` `0x8010_4000` / `kPwm` `0x8010_5000` / `kDma` `0x8010_6000`
>   ⇒ **每段 4 KB**（与既有三段连续 · 均匀间距）
> - `include/cpu/riscv/address_decoder.h` **改派生自该表**（消除 3 段硬编码）+ **补齐四个片选**
>   （`spi_cs` / `i2c_cs` / `pwm_cs` / `dma_cs` ⇒ 共 **7 片选 + `error_cs`**）
> - **SoC 侧只负责装配**：用 `AxiLiteMatrix2x2` / `AxiInterconnect4x4` 的 `Map::windows[]`
>   模板参数（已是现成机制 · 事实⑦⑧）⇒ **不在库层给默认 `Map`**（那是被否的选项 C）
> - ⚠️ **公开面变更** ⇒ MUST 登记 `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md`
> - ⚠️ **未裁决（随 S4a design）**：S4 的**互联形态**（级联矩阵 / 单窗口 + 二级译码 / 新写 N×M）
>   ⇒ 见 §五 的三种可选
> - ⚠️ **裁决顺序修正**：`G-6 → S4a/S4b/S4c → G-5（S5 前）→ G-8（S6 前）`
>   （G-5 推迟的理由见 §六 末段：S5 被 `rv32i-privileged-csr-2026-11`（draft）阻塞，
>   现在加 `irq` 口只会留下**无人驱动**的悬空端口）
>
> 落点：**S4a 立项** ⇒ MMIO 映射 + 互联 + **GPIO 一个外设** ⇒ 打通一条 MMIO 写→读回环。

# 决策请求 · G-6：MMIO 地址映射的**归属**（库层 vs SoC 侧）

- **日期**：2026-11-23 · builder 起草（**待用户 / architect 裁决**）
- **母 improvement**：`rv32i-soc-platform-2026-11` 的决策门 **G-6**（`:346`）
- **影响批次**：**S4**（MMIO 解码 + 互联 + 6 外设 + DMA）· **S6**（L2 / DRAM 的新段）
- **前置已闭**：G-7（总线类 = **full AXI4 主干** · 2026-11-23）
- **建议顺序**（母 improvement `:350` 原文）：`G-7 → G-5 → G-6 → G-8` ⇒ 本请求 = 第三项
  ⇒ ⚠️ 但 builder 建议 **G-5 推迟到 S5 前**（理由见 §六，不影响本请求）

## 一、要裁的是什么

> **MMIO 的地址表（各外设的 base / size 常量）放在**
> **① 库层 `include/cpu/riscv/rv32i_mem_map.h`（平台契约）**，还是
> **② SoC 侧新建（`examples/riscv-soc/src/**` · 产品契约）**？

它决定三件事：
1. **`address_decoder.h` 的既有 3 段硬编码**（CLINT / UART / GPIO）怎么处置 —— 收敛进表 vs 删 vs 留着漂移
2. **S4 的互联（matrix / interconnect）的 `Map::windows[]` 从哪来**
3. **S6 加 L2 / DRAM 段时**是改库层还是改一个 SoC

## 二、事实（全部实测 · 带坐标）

| # | 事实 | 证据 |
|---|---|---|
| ① | **`rv32i_mem_map.h` 已是「TCM 单一真相」**：`kTcmAddrWidth`(12) → `kTcmBytes`(16KB) → `kItcm` / `kDtcm` / `kPcReset`，全部**派生**（禁手写字面量） | `include/cpu/riscv/rv32i_mem_map.h:50-68` |
| ② | 但它**只覆盖 TCM 两段** ⇒ **MMIO 段不在表内** | 同上（只有 `kItcm` / `kDtcm`） |
| ③ | 🔴 **地址真相分两处**：`address_decoder.h` **独立硬编码** CLINT `0x8010_0000` / UART `0x8010_1000` / GPIO `0x8010_2000`，而 TCM 两段已派生自 `rv32i_mem_map.h` | `address_decoder.h:43-45`（硬编码）· `:41-42`（派生） |
| ④ | 🔴 **`AddressDecoder` 只有 5 个片选**（`i_tcm` / `d_tcm` / `clint` / `uart` / `gpio`）+ `error` ⇒ **缺 SPI / I2C / PWM / DMA 四段**（S4 要 7 个 slave：6 外设 + CLINT） | `address_decoder.h:62-68` |
| ⑤ | **`AddressDecoder` 零生产消费者** ⇒ 只有 `examples/riscv-mini/tests/test_riscv_tests_soc.cpp`（6 处实例化） | `grep -rn AddressDecoder include/ src/ examples/` ⇒ 仅测试 |
| ⑥ | **`AddressDecoder` 的形态是「one-hot 片选 + `local_addr`」，没有 AXI 主端** ⇒ 不能直接驱动 AXI4-Lite 外设 | `address_decoder.h:62-68`（无 `arvalid`/`awvalid` 类端口） |
| ⑦ | ✅ **`AxiLiteMatrix2x2<ADDR_WIDTH, DATA_WIDTH, Map = AxiLiteDefaultMatrixMap>` 已有可注入的地址映射策略**：`Map::windows[N]` + **编译期**校验（size 幂 / base 对齐 / 位宽不越界） | `include/axi4/axi4_lite_matrix.h:29-32`（模板）· `:239-250`（`static_assert`）· `interconnect_common.h:61-76`（默认 Map） |
| ⑧ | ✅ **`AxiInterconnect4x4` 同型**（`Map::windows[0..3]`） | `include/axi4/axi_interconnect_4x4.h:266-269` |
| ⑨ | ✅ **6 个 Lite 外设 + `AxiDma` 全部已存在**（无 `0x8010` 之类基址常量 ⇒ 基址由上层决定） | `ls include/axi4/peripherals/` ⇒ 7 个 · `grep -rn "0x8010" include/axi4/` ⇒ 0 命中 |
| ⑩ | **S3 后的核内路径 = `pipeline → DCacheSync → TcmAxiSlave → DataTCM`** ⇒ TCM **不走** SoC 的互联；S4 要让 MMIO 可达 ⇒ `DCacheSync` 的 AXI 主口必须改接「互联」，TCM 降为互联的一个 slave | `examples/riscv-mini/src/rv32i_core_top.h`（S3 接线）· `include/cpu/cache/d_cache_sync.h`（AXI4 full 主口） |
| ⑪ | **`clint.h` 在库层**（`include/cpu/riscv/clint.h`）⇒ 它的寄存器偏移与「它被映射到哪」天然是库层语义 | 文件位置 + `CLINT` 段在 `address_decoder.h` 库层 |
| ⑫ | **ADR-046**：`include/cpu/riscv/` 是 **RV32I 微架构的单一权威** | `docs/adr/ADR-046-…` |

⚠️ **⑩ 的连带结论（重要 · 影响 S4 设计，但不影响本裁决）**：
S4 的互联需要 **≥ 7~8 个 slave 窗口**（TCM + CLINT + 6 外设）；而
`AxiLiteMatrix2x2` **2 窗口** · `AxiInterconnect4x4` **4 窗口** ⇒ **都不够**。
可行形态（S4 design 决定，见 §五）：级联矩阵 / 单窗口 + 二级译码 / 新写 N×M 互联。

## 三、选项对比

| 维度 | **A · 库层全表 + SoC 装配**（推荐） | **B · 全在 SoC 侧** | **C · 库层连 `Map` 一起给** |
|---|---|---|---|
| **单一真相** | ✅ 段坐标**一处**（TCM + MMIO 同一张表） | ❌ **仍分两处**（TCM 在库 · MMIO 在 SoC）⇒ 事实③ 的问题**延续** | ✅ 一处 |
| **`address_decoder.h` 的 3 段硬编码** | 改**派生自表** ⇒ 消除（且顺带补齐 SPI/I2C/PWM/DMA 段） | 变成**孤儿**（要么删、要么标 deprecated、要么留着自己漂移） | 改派生 ⇒ 消除 |
| **多 SoC 复用** | ✅ 段坐标可复用；各 SoC 只决定「装配哪些段」 | ❌ 每个 SoC 各写一份段常量 ⇒ 易漂移（事实③ 的成因就是这个） | ✅ 但窗口布局**锁死** |
| **产品可定制性**（真实 SoC 想用非标准布局） | ✅ 装配在 SoC ⇒ 可只接自己有的段 | ✅ 完全自由 | ❌ 想调窗口就得改库层 |
| **库层公开面变更** | +MMIO 段常量（需 `public-api-surface` 登记） | 无 | +段常量 **+ 默认 `Map`**（更重的公开面） |
| **S4 的落点** | 库层给段 · SoC 给 `Map::windows[]`（**`AxiLiteMatrix2x2` 的 `Map` 参数已是现成机制**，事实⑦） | SoC 新建段表 + `Map` ⇒ 与 ① 的事实③ 重复一次 | 库层给 `Map` ⇒ S4 几乎零工作量，但窗口布局不可调 |
| **`clint.h`（库层）的语义自洽** | ✅ 地址在库层 | ❌ 库层组件依赖 SoC 侧的地址 | ✅ |
| **S6 加 L2 / DRAM 段** | 改库层一处（**预期内的变更**） | 改各 SoC（多份） | 改库层（但连带 `Map`） |

## 四、推荐：**A（库层全表 + SoC 装配）**

理由（按权重）：

1. **事实③ 是既成缺陷，选 B 无法修它** —— `address_decoder.h` 的 3 段硬编码**已经在库层**；
   选 B 后它要么留着（继续漂移）、要么删（但 TCM 两段仍在库层 ⇒ 表还是分裂的）。
   ⇒ ⇒ 只有「库层一处定义 + 各方派生」能真正收敛。
2. **`Map` 已经是可注入策略（事实⑦⑧）** ⇒ 「库层定段、SoC 装配」**不需要新机制**，
   只是把 `Map::windows[]` 的常量值从库层的段派生出来。
3. **`clint.h` 在库层（事实⑪）** ⇒ 它的地址若在 SoC 侧，库层组件就依赖下游 ⇒ 倒挂。
4. **不选 C 的理由**：窗口布局是**产品决策**（§五 的 8 个 slave 怎么分窗口），
   锁死在库层会让 S4/S6 的每次调整都变成库层公开面变更。
5. **成本可控**：库层只加**段常量**（`MemRegion` 结构已存在），`address_decoder.h` 改为派生
   ⇒ 与 `firmware-load-chain` L1 收敛 TCM 的做法**同型**（那次已验证） 。

## 五、选 A 时 S4 的具体工作量（供裁决参考 · 标出哪些属 G-6、哪些属 S4 design）

| 项 | 属谁 | 内容 |
|---|---|---|
| 库层 `rv32i_mem_map.h` 加 MMIO 段 | **G-6 裁决的直接落点** | `kClint` / `kUart` / `kGpio` / `kSpi` / `kI2c` / `kPwm` / `kDma`（`MemRegion` · base + size）⇒ **取值沿用 `address_decoder.h` 的既有三段 + 新增四段**（新增段的取值需在同一批裁决 / 或按「每段 4KB 顺延」定） |
| `address_decoder.h` 改派生 + 补片选 | 同上 | 3 段改为派生；补 `spi_cs` / `i2c_cs` / `pwm_cs` / `dma_cs`（**7 个片选 + error**） |
| SoC 侧 `Map`（`windows[]` 装配） | **S4 design** | 从库层段派生 `AxiAddrWindow windows[N]`；**N ≥ 8** ⇒ 需先定互联形态（见下） |
| 互联形态 | **S4 design**（G-7 已定 full 主干，但在 MMIO 侧仍是 Lite） | `DCacheSync.AXI(full) → AxiToAxiLite → Lite 互联 → {各外设}`；TCM 仍走 `TcmAxiSlave`（需与 AXI 走同一条路 ⇒ 要么 TCM 也挂 Lite 互联、要么在 `DCacheSync` 前分流） |

⚠️ **互联形态的三种可选**（S4 design 再裁，此处只登记）：
① **级联**：4×4 顶层（TCM / CLINT / 外设组 A / 外设组 B）+ 二级 matrix
② **单窗口 + 二级译码**：一个 2 MB 窗口覆盖 `0x8000_0000..0x801F_FFFF`，内部再译码
③ **新写 N×M Lite 互联**（~8 slave）

## 六、需要你裁决的事

| # | 问题 | 选项 |
|---|---|---|
| **G-6.1** | **MMIO 段的归属** | **A 库层全表 + SoC 装配（推荐）** / B 全在 SoC / C 库层连 Map 一起给 |
| **G-6.2** | 若选 A：**新增四段（SPI / I2C / PWM / DMA）的取值** | ① 按 `0x8010_3000` 起每段 4KB 顺延（推荐 · 与既有三段连续）② 按各外设寄存器文件大小定（需先看每个外设的偏移上限）③ 本批只定「既有三段 + GPIO/UART/CLINT」，其余随 S4b/S4c 再加 |
| **G-6.3** | `address_decoder.h` 的处置 | ① 改派生 + 补 4 片选（推荐 · 保留它的「片选」形态给非 AXI 场景）② 标 `[[deprecated]]`（AXI 侧统一用互联，decoder 只服务老的片选形态）③ 删除（零生产消费者） |

> ⚠️ **G-5 的时机建议（不是本次裁决项，供参照）**：母 improvement 把 G-5 排在 G-6 前，
> 但 builder 建议**推迟到 S5 前** —— 理由：① G-5 的影响批是 **S5**，而 S5 的入口条件含
> `rv32i-privileged-csr-2026-11` **完成**，该 improvement **仍是 draft**（未开工、无 change）；
> ② 现在加 `irq` 口后**无人驱动**（IRQ 汇聚器要到 S5 才写）⇒ 悬空输入端口读到**未定义值**
> ⇒ 本仓 ANTI-PATTERN 明令禁止的形态；③ 端口表重算（C6 快照 / 34 端口表 /
> `verify_riscv_soc_core_ports.sh` / SoC 8 端口表 / tsv）换不来任何交付。
> ⇒ ⇒ **建议顺序修正为 `G-6 → (S4a/S4b/S4c) → G-5（S5 前）→ G-8（S6 前）`**。

## 七、裁决后的落点

| 裁决 | 落点 |
|---|---|
| G-6.1 = A | 库层 `include/cpu/riscv/rv32i_mem_map.h` 扩段（**公开面变更** ⇒ `public-api-surface.md` 登记 + `docs/CHANGELOG.md`）；`address_decoder.h` 改派生 |
| G-6.1 = B | 新建 `examples/riscv-soc/src/rv32i_soc_mem_map.h`；`address_decoder.h` 处置见 G-6.3 |
| G-6.2 | 段常量取值（含 `static_assert` 对齐 / `axi_window_is_valid` 约束的一致性） |
| G-6.3 | `address_decoder.h` 的形态与它的 6 处测试消费者（`test_address_decoder`）同步 |
| 全部 | ⇒ **S4a 立项**：MMIO 映射 + 互联 + **GPIO 一个外设** ⇒ 打通「一条 MMIO 写→读回环」（手推地址 + 值）⇒ 总线路径端到端可用 |
