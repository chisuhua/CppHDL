# ADR-078: RV32I 平台双产品线（mini / full）—— 第二条产品线的边界与分批

- **Status**: **Accepted**（2026-11-27 · architect role（build agent）· **oracle 第 1 轮 REJECT** ⇒ 6 条 finding 全闭 · **critic 第 1 轮 REJECT** ⇒ 1 BLOCKER + 7 SHOULD_FIX + 4 NIT 全闭 · **critic 第 2 轮（闭合复审）APPROVE**（mergeable: yes · scope_exceeded: false · 0 BLOCKER · 0 SHOULD_FIX）⇒ 复审后按残留 3 条 NIT 做了措辞/证据精度订正（**无实质变化**）⇒ 关联 change **尚未立项**（登记为 `backlog: rv32i-soc-platform-2026-11`）⇒ 详见 `.rddf/reviews/ADR-078-dual-product-lines-and-full-line-scope.md`）
- **Date**: 2026-11-27
- **Author**: architect role（build agent）
- **Parent**: **ADR-077**（两级顶层与 SoC 组合 · D3）· **ADR-073**（SoC RTL 产品面与 TB）· **ADR-075**（产品顶层端口具名化）· **ADR-041**（cache 变体族）
- **关联 change**: **`rv32i-full-product-line-2026-11`**（`openspec/changes/rv32i-full-product-line-2026-11/` · **已立项** 2026-11-27 · **范围 = S4c-0**）⇒ ⇒ **AC 映射** = 该 change 的 **AC1 / AC2 / AC3 / AC4 / AC5 / AC6 / AC7 / AC8 / AC9 / AC11 / AC12**（对应本 ADR 的 **AC2 / AC3 / AC4 / AC13 / AC17 / AC18 / AC19**；逐条对照见该 change 的 `proposal.md` §「与 ADR-078 的 AC 映射」）⇒ ⇒ 母 improvement = `rv32i-soc-platform-2026-11`（**S4c** 批次 · S4c-1..3 各自另立）
  ⇒ ⇒ **2026-11-28 更新**：S4c-0 的 **AC1–AC12 已实现**（P1 marker + P2 元护栏 + P5 full 线空壳
  + T3.5/T3.6 两护栏登记 + T4 锚点分组/逐线 scope/散文表治理）⇒ ⇒ 两处**措辞订正**已落该
  change 的 proposal：① **AC5** 的判据是「**两态退出码相同**」（实测 5 个 B 类**零改动**即满足）
  ② **AC9** 去掉 `--check-products`（S4c-0 的 full 线**不发 RTL** ⇒ 改为「组件端口面登记 +
  可构建可运行」）⇒ ⇒ **决策内容一字未改**（只订正判据口径）
- **审稿记录**: `.rddf/reviews/ADR-078-dual-product-lines-and-full-line-scope.md`
- **超长自检**: 正文 **< 250 行** ⇒ 无需拆分
- **实施序**: **S4c-0（产品线抽象）先于本 ADR 的任何实施批次**

---

## 1. 背景与问题

### 1.1 诉求来源

用户 2026-11-27 裁决：把已冻结的「TCM 形态」线（`examples/riscv-mini` + `examples/riscv-soc`）与新的
「总线化 + cache」线**分成两条产品线** ⇒ 新线承载 **取指侧总线化 + I-L1 + `AxiDma` boot 路径**及
「更全面的 SoC 互联与多级缓存」。同时采纳本会话给出的命名建议（**旧名保留**）。

### 1.2 实测事实（2026-11-27 · 可复算）

| # | 事实 | 证据 |
|---|---|---|
| ① | 核**无总线化取指** —— IF 级 `pc_reg → instr_addr` 直连 `InstrTCM`（1 拍 `sread`） | `examples/riscv-mini/src/rv32i_core_top.h:190-209` · `include/cpu/pipeline/stages/if_stage.h:55-57,243` |
| ② | 核**无 AXI4-full 主口** —— 只有 19 根 **AXI4-Lite**（`axil_*`）；核内 AXI4-full 是**内部链路**（`DCacheSync → TcmAxiSlave → DataTCM`），`TcmAxiSlave` 是 slave 且不出顶层 | `rv32i_core_top.h:115-149,183-186` |
| ③ | IF 级 skid buffer + `primed` **建立在 imem 1 拍 `sread` 上**（`primed` 明文「`instr_ready` 恒为 1，不携带信息」）⇒ 总线化取指要求 `instr_ready` 成**真反压** ⇒ 是**加协议**而非「加可选输入」 | `if_stage.h:242-252,305-339,370-385` |
| ④ | `ICacheSync` 存在但核**不实例化**；其 AXI 侧是**单拍读**（`arlen=0`）· 无 burst · 无 ID；消费者只有 wrapper + tests ⇒ **零生产消费者** | `include/cpu/cache/i_cache_sync.h:69-85` · `i_cache_prefetch.h:62` |
| ⑤ | **协议侧缺口** = fill 效率（单拍读是合法 AXI4-full 事务；`TcmAxiSlave` **忽略 ID 值**）⇒ **不是**正确性问题 | `include/axi4/tcm_axi_slave.h:40` |
| ⑤′ | **另有一个与协议无关的读侧缺陷（I 侧「假 line」）**：读 = `select(hit, bits<31,0>(cache_data), …)` ⇒ **恒取 line 低 32 位**；`:116` 算出的 `offset` 在读路径**零使用**；fill = `zext<128>(axi_data)` 只填低 32 位 ⇒ ⇒ **同一 line 的 4 个地址恒读到首字** | `include/cpu/cache/i_cache_sync.h:116,151,179,183` |
| ⑤″ | **对照 S3 修好的 D 侧**：`lane_sel = bits<3,2>(addr)` + 按 lane 选 4 个 word；头注明写「旧版 fill 是 `zext<128>`、读是 `bits<31,0>` ⇒ 新版 `addr[3:2]` 参与**读选择**与写更新」⇒ ⇒ **S3 先例 = burst fill + 读选 lane 两半** | `include/cpu/cache/d_cache_sync.h:46-48,159,183-185` |
| ⑤‴ | **G-7 对同一缺陷形态的已签批定性** = 「**不是性能问题，是功能错误**」「读选 lane **无论协议都必须做**」⇒ ⇒ ⑤′ 的修复**独立于**协议选择，且**不得**被「只是性能」的说法带过 | `.rddf/checks/decision-request-g7-bus-class-2026-11.md` §二⑥ |
| ⑤⁵ | **既有 cache 测试的地址全部 16 字节对齐**（`0x1000` / `0x1100` / `0x2000` / `0x3000` / `0x5000` / `0x7000` / `0x9000` / `0xA000` / `0xB000` / `0x100000` / `0x200000` ⇒ `addr[3:0]` **恒 0** ⇒ **lane 恒 0**）⇒ ⑤′ 的缺陷**从未被观测** ⇒ 接进 full 线取指（顺序取指必踩 offset 4/8/12）⇒ 核立即跑飞 | `tests/chlib/test_cache_sync.cpp` · `test_cache_complete.cpp` · `test_cache_simple.cpp` · `test_cache_pipeline_integration.cpp` 的地址取值 |
| ⑥ | **S3 已对 D 侧做过一模一样的两处升级**（burst fill + 读选 lane），而 D 侧**有**生产消费者 | `include/cpu/AGENTS.md`（S3 条目）· `rv32i_core_top.h:185` |
| ⑦ | **I-L1 没有 capability** | `openspec/specs/rv32i-dcache-timing/spec.md:13` |
| ⑧ | `AxiDma` 是 full AXI4 master（46 端口 · 7 态 FSM）；控制面是**裸线** `reg_start/reg_stop/reg_src_addr/reg_dst_addr/reg_length` + `status_*`/`irq` ⇒ CPU **无法经 MMIO 访问** | `include/axi4/peripherals/axi_dma.h:23-59` |
| ⑨ | `AxiDma` 现有 **11** 个 TEST_CASE **全部只断言 `dma.name()`**；**零 example 实例化** | `tests/test_axi_dma.cpp:31-151` |
| ⑩ | `TcmAxiSlave` 是**完整 full AXI4 slave**（五通道 + burst + ID + **支持写**）⇒ `AxiDma` 可直连它，**无需 full 互连** | `include/axi4/tcm_axi_slave.h:44-88` |
| ⑪ | `AxiInterconnect4x4`（4×4 · full · burst + ID）只被 `examples/axi4/phase3a_top.h` 使用；SoC 现用 `AxiLiteCrossbar`（**2×6** · Lite）⇒ 两者**无法直接共存** | `include/axi4/axi_interconnect_4x4.h:22-28` · `include/axi4/axi_lite_crossbar.h:53-57` |
| ⑫ | `AxiLiteCrossbar` 从口数**硬编码 6**（端口 MUST 具名）⇒ 每个 slave 面 = **19 根**（主→从 11 + 从→主 8）⇒ 加 s6 = **+19 根**；实现 **791** 行 | `axi_lite_crossbar.h:57,129-147` · capability `axi-interconnect-routing/spec.md:297-309` |
| ⑬ | `kDma` = `0x8010_6000`；`SocCrossbarMap.windows[6]` → **`kErr`** 且被 `static_assert` 钉死；`lite.m1_*` 全 tie 0 | `include/cpu/riscv/rv32i_mem_map.h:102` · `examples/riscv-soc/src/rv32i_soc_top.h:63,99-100,367-380` |
| ⑭ | **路径引用面（源码树口径）**：`examples/riscv-mini` **317** 处（openspec 131 · .rddf 97 · docs 40 · examples 20 · scripts 15 · tests 7 · include 5），`examples/riscv-soc` **45** 处 ⇒ 复算：`grep -rlI "examples/riscv-mini" --exclude-dir='build*' --exclude-dir=.git .` | 同左 |
| ⑮ | 其中 **268 处是不可变的历史决策记录**（已归档 change / `.rddf/checks` 台账 / `docs/adr`）⇒ 改名会**强迫回改历史** | ⑭ 的目录分布 |
| ⑯ | **写死路径的护栏 = 12 sh + 2 py**（`rv32i_obs_vector.py` / `dual_backend_parity.py`；第 3 个 py `docs_link_scope.py` 写死 `docs/riscv-mini/` ⇒ **路径族不同**，登记时分开）⇒ `verify_no_duplicate_rv32i_headers.sh` 的 **C1**（同 basename）/ **C6**（51 核自有快照）写死 `examples/riscv-mini/src/**`；`verify_riscv_soc_core_ports.sh` 的检查 = **V1/V2/V3**（**无 C6**）⇒ 53 行表核对在 **V1/V2** | `scripts/verify_no_duplicate_rv32i_headers.sh:11-35` · `scripts/verify_riscv_soc_core_ports.sh:11-16` |
| ⑰ | **全局整数锚点**（根 `AGENTS.md` NOTES 的 CI anchor 段）：`CTEST_TOTAL` · `VERILATOR_E2E_TOTAL` · `DUAL_BACKEND_PARITY_TOTAL` · `DUAL_BACKEND_PARITY_PAIRS` · `EXAMPLES_TEST_SOURCES` ⇒ 新线加测试**必然**改动它们 | 根 `AGENTS.md` |
| ⑱ | **散文表已在漂移**：`examples/riscv-soc/AGENTS.md` 的 module 表仍写「`AxiLiteMatrix2x2` · `AxiLiteGpio` = 12」，而磁盘已是 `AxiLiteCrossbar` + 6 外设 ⇒ 同周内漂移且**无护栏捕获** | 该文件 · `rv32i_soc_top.h` |
| ⑲ | 根 `AGENTS.md` 的「imem MUST `sread`（1 拍）」· `include/cpu/AGENTS.md` 的同款红线 · capability `rv32i-data-forwarding` 的 load-use 停顿矩阵 ⇒ **全是 mini 线专属不变量**，却写成线无关权威 | 三处 |
| ⑳ | G-7 已裁「**full AXI4 主干**」· `AxiDma` 全速；**S4 的 Lite 互联形态已由用户 2026-11-27 改裁为「③ 新写 N×M」**（非级联）；ADR-077 D3：`prog_*` **长期保留** · 总线装载 **MUST 只新增路径** · 装载期 MUST 保持 `ext_rst = 1` | `decision-request-g7-...md` 文首 · `decision-request-g6-...md` 文首 · `docs/adr/ADR-077-...md:90-102` |

### 1.3 问题

1. **形态**：如何在**不破坏 mini 线**（268 处历史记录、15 个写死路径的护栏、5 个全局整数锚点）的前提下引入第二条产品线？
2. **共享边界**：full 线与 mini 线共享到哪一层？full 核是**演进**还是**新核**？
3. **批量**：母 improvement 的 S4c 是三块 + 「更全面的 SoC 互联与多级缓存」⇒ 串行依赖且代价差一个量级 ⇒ 如何切批？
4. **命名与改名**：新线叫什么？旧线是否改名（会回改 268 处历史记录）？

---

## 2. 决策

### 2.1 **D1** = 新线**新增**，**旧名一律不改**

事实 ⑭⑮ —— 改名成本集中在**不可变历史记录**；收益仅为命名整齐。

### 2.2 **D2** = full 线命名 = **`examples/riscv-full/`（IP 级）+ `examples/riscv-full-soc/`（SoC 级）**

与既有 `examples/riscv-mini/`（IP 级）+ `examples/riscv-soc/`（SoC 级）**语义对称**；**不采用**
`riscv-full-ip` / `riscv-mini-ip` 四元组（那需给旧线补 `-ip` 后缀 ⇒ 触发 D1 被否的改名面）。

### 2.3 **D3** = 共享边界 = **库层共用，但取指侧与 pipeline 顶层**不共用

| 层 | 共享？ | 说明 |
|---|---|---|
| ALU / 译码 / 寄存器堆 / 存储子系统 / cache 库 | ✅ 共用 | 留在 `include/cpu/**` |
| **取指接口（IF 级）+ pipeline 顶层** | ❌ **不共用** | 见 **D14**（层次化产物会发射内部 `ch_layer` 的端口 ⇒ 给 `IfStage` 加端口 ⇒ **mini 产物也变**；未驱动端口会被层次化发射器判红 ⇒ 「加可选输入」**不成立**） |

### 2.4 **D4** = full 核 = mini 核的**演进**（不是新核）

复用 ALU / 译码 / 寄存器堆；分叉点 = ① 取指接口（**fork** · D14）② cache 层（接 I-L1）。

### 2.5 **D5** = 多级缓存（L2 / DRAM）**不属本 ADR 实施范围** ⇒ 归 **S4c-3**，且 **L2 置于 SoC 层**

依赖 full 互连 + 外部 DRAM 模型 ⇒ 代价与 S4c-1/2 差一个量级 ⇒ ⇒ 不进核。

### 2.6 **D6** = `AxiDma` 的 MMIO 面 = **新写 Lite 寄存器包装**（`AxiDmaLiteRegs`）

事实 ⑧ ⇒ 否则 CPU 无法配置 DMA。寄存器映射（地址 / 读回值 / 写副作用）MUST **手推**并落 capability。

### 2.7 **D7** = I-L1 = **原地升级 `include/cpu/cache/i_cache_sync.h`**（改选 · 范围 = **S3 完整形态**：两处一起改）

- **改两处**：① **fill 路径** = 4-beat burst + ID（单拍 → burst）② **读路径** = 按 `addr[3:2]` **选 lane**（`bits<31,0>` → 4 选 1）⇒ ⇒ 事实 ⑤′ 的「假 line」缺陷**一并闭合**（G-7 已把该形态定性为「无论协议都必须做」· 事实 ⑤‴）⇒ ⇒ **不得**只做 ①。
- **hit 延迟不变**（1 拍 `sread`）⇒ mini 线语义零变化；`i_cache_async.h` **零 diff** ⇒ ADR-041 的变体族（命中延迟维度）**不扩张**。
- **先例**：事实 ⑥ —— S3 对 **D 侧**做过**一模一样**的两处升级（且 D 侧**有**生产消费者都升了）。
- **爆炸半径（完整）**：① `i_cache_prefetch.h:92,104-108` 实例化 `ICacheComplete` 并逐一接线其 5 根简易握手 AXI 端口 ⇒ wrapper 的接线与端口面**必须同批重写** ② `tests/test_cache_smoke.cpp`（双后端）与 `tests/chlib/test_cache_complete.cpp` 的 ICacheSync fill 用例**必须改写**（burst 下旧激励完不成 fill）③ 事实 ⑤⁵ —— 既有测试 offset 全 0 ⇒ 对 lane 选择**零判别力** ⇒ MUST 新增 **offset≠0** 的手推判别性用例（AC8）。

### 2.8 **D8** = 分批 = **S4c-0 → S4c-1 → S4c-2 → S4c-3**（串行）

| 批 | 内容 | 前置 |
|---|---|---|
| **S4c-0** | **产品线抽象**：**per-line marker**（每条线一个 marker 文件）+ **元护栏**（发现集 == 登记集 + 配置完备性校验）+ 12 sh + 2 py 的**逐个处置登记** + 5 个全局整数锚点的**归组** + **逐线 scope 标注**（事实 ⑲）+ **散文表治理**（事实 ⑱）+ full 线**空壳**（不改 mini 行为） | — |
| **S4c-1** | full 核：**取指接口 fork**（D14）+ **I-L1 两处升级**（D7）+ 核新增 AXI4-full 取指口 | S4c-0 |
| **S4c-2** | **DMA boot 闭环**：`AxiDmaLiteRegs` + `AxiDma` → `TcmAxiSlave` → I-TCM + full-soc 装配 + 判据「DMA 装载后核能取指」 | S4c-1 |
| **S4c-3** | full-soc **全互联**（`AxiInterconnect4x4` + `AxiToAxiLite`）+ **多级缓存（L2 / DRAM）** —— **内容另立**（D5） | S4c-2 |

⇒ ⇒ **S4c-0 MUST 先于其余全部**（事实 ⑯⑰⑱⑲）⇒ ⇒ 产出判据 = 「加一条线的成本 = 该线自有文件
+ 登记条目（per-line marker）+ **全局锚点重算**」。

### 2.9 **D9** = 旧线改名 = **不做**（登记为独立 backlog）

若将来做，MUST 只改**活文档与脚本**，历史记录**保留旧路径**并在 CHANGELOG 记一条。

### 2.10 **D10** = full-soc 的 Lite 互连 = **扩 `AxiLiteCrossbar` 到 2×7**（`s6` = DMA 寄存器面）

事实 ⑫ —— 从口数硬编码 6 ⇒ full-soc 需要第 7 个 Lite 从口给 DMA 寄存器（D6）。
**成本清单（完整）**：① `kNumSlaves` 6→7 + 新增 **19** 根具名端口（每个从口面 = 19 根 · capability
`spec.md:297-309`）② 测试 fixture 加 `set7/get7` + `CrossTestMap` 加**第 8** 窗口（现为 7：6 slave + 1 ERR）
③ capability `axi-interconnect-routing/spec.md:304`「2 master × 6 slave」→「× 7」同批 ④ **mini 源必须
新增 s6 面的 tie-off**（**19** 根 = 11 + 8：crossbar **输出**的 11 根在实例化点侧**无消费者**（tie-off
依据 = lint 纪律 + G-7 `:81` 的 tie-off 成例）· crossbar **接收**的 8 根**无源**（依本仓纪律「新增组件
输入端口 ⇒ 全部实例化点 MUST 显式驱动」））⑤ 「12 → 14 对」判据改写。
⇒ ⇒ **本 ADR 唯一改动 mini 线回归面的决策**；mini 线**行为**不变，但 `AxiLiteCrossbar` 的**产物端口表**
在**两条线**都增 19 根 `s6_*` ⇒ ⇒ **P1 MUST 写「除 D10 外零回归面」**。

### 2.11 **D11** = `AxiDma` 的**数据面**在本批 = **直连 `TcmAxiSlave` → I-TCM**（不经 full 互连）

事实 ⑩ ⇒ 1 master × 1 slave **无需互连 IP** ⇒ ⇒ full 互连留到 S4c-3。

### 2.12 **D12** = `AxiDma` 的**行为测试** MUST 补齐（事实 ⑨ 零判别力）

⇒ S4c-2 MUST 先补**行为断言**（FSM 推进 / burst 切分 / 长度 / 错误路径）⇒ ⇒ **在新行为测试就位前，
MUST NOT 把 `AxiDma` 的任何行为写进 AC**。

### 2.13 **D13** = full 线沿用 ADR-077 的**两级顶层**，且**组件 / 头文件 basename MUST 独立**

① **两级顶层**：`examples/riscv-full-soc/` 顶层 = 核复合体（内含 full 核 + 内部 `ch_layer`）+ SoC 组合层。
② **basename 独立**：full 线用 `Rv32iFullCoreTop` / `rv32i_full_core_top.h` 等 ⇒ ⇒ 事实 ⑯ 的 **C1
判据在不改护栏的前提下天然不冲突** ⇒ ⇒ **N1 的改造面收窄**（C1 无需按产品线分组；仍需分组的是
**C6**（51 核自有快照）与 `verify_riscv_soc_core_ports.sh` 的 **V1/V2**）。

### 2.14 **D14** = 取指接口 = **fork 出独立变体**（不动共享 `IfStage`）

- **机制选定**：① pipeline 加模板参数（**拒**：改 `IfStage` 的模块名 ⇒ mini 产物变）② `IfStage` 内部
模式开关（**拒**：给 `IfStage` 加端口 ⇒ mini 产物端口表变；未驱动端口还会被层次化发射器判红 ⇒
「可选输入」不成立）③ **fork 独立取指变体**（✅ 采用）⇒ full 线新增取指接口组件 + 对应的 pipeline 变体；
**共享 `IfStage` 一字不改**。
- **代价**：pipeline 层也**不共用**（D3 的例外面由此确定）。
- **验证形态**：AC15（端口集**集合相等 / 零 diff**）+ AC16。

---

## 3. 选项对比表

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| **①** | **新增 full 线（旧名不动）** = D1 + D2 | ✅ **采用** | 不回改 268 处历史记录 · 不动 12 sh + 2 py 的既有语义 · mini 线行为与产物（除 D10）逐字不变 |
| ② | 先给旧线改名（四元组）+ 新线用四元组 | ❌ 拒（D9） | 改名面 = 362 处引用 + 12 sh；其中 268 处是**不可变历史决策记录** ⇒ 回改历史 = 失真 |
| ③ | 只在**同一个** SoC 上加「full 模式」开关 | ❌ 拒 | **真实理由 = 契约单数性**：端口契约表是「核可替换的唯一物证」⇒ 模式相关端口面会让 V1/V2/C6/R8′ 全部按模式分叉；且 mode-conditional `describe()` 正撞本仓最高频缺陷族（建图期控制流）|
| ④ | full 线 **fork 掉整个 `include/cpu/**`** | ❌ 拒 | 库层 fork ⇒ ALU / pipeline / decoder 双份 ⇒ 每次 ISA 变更做两遍；mini 线的修复无法自动惠及 full 线 |
| ⑤ | **库层共用 + `examples/**` 分叉**（D3 + D4，取指侧除外） | ✅ **采用**（与 ① 组合） | 符合仓内既有分层；差异面收敛到取指接口 + cache 层 + 顶层装配 |
| ⑥ | full-soc 另写一份 Lite 互连（复制 `AxiLiteCrossbar`） | ❌ 拒 | 复制 **791** 行实现 + ~1000 行双后端测试 + **M1–M7 共 8 行变异自证**的复制与逐条再验证 ⇒ 判别逻辑三分叉（路由锁存 / 仲裁 / 在途门控 —— 正是那 8 行守的缺陷族）|
| ⑦ | 扩 `AxiLiteCrossbar` 到 2×7（D10） | ✅ **采用** | 单一实现保判别力（8 行变异继续守同一实现；M2 的 fanout 用例随 14 对扩展）；代价**有界且可枚举**（§2.10 的 5 项）|
| ⑧ | full-soc 用既有 `AxiLiteMatrix2x2`（2×2）做**一级分流**（`m0`=CPU · `s0`=2×6 crossbar · `s1`=DMA 寄存器） | ❌ 拒 | 优点真实：**共享代码零改动、mini 真·逐字不变**；但 ① **地址译码拆进两个 `Map`** ⇒ G-6 §五 把「① 级联（4×4 + 二级 matrix）」列为 S4 **可行**形态 ⇒ **不违反 G-6**（G-6 裁的是段表**归属** = 库层全表 + SoC 装配，**不是**「唯一译码点」——该措辞全仓仅见于本 ADR 及其审稿记录）⇒ ⇒ **真实否决依据 = 用户 2026-11-27 已裁 S4 形态 = ③ 新写 N×M 而非级联** + 过渡结构 + 多一跳 ⇒ 架构性代价 > D10 的工程性代价 |
| ⑨ | DMA 寄存器**并入某个已有外设的窗口** | ❌ 拒 | 语义独立 ⇒ 污染该外设的寄存器契约与既有判据 |
| ⑩ | I-L1 在**现有** `i_cache_sync.h` 上**原地升级**（burst fill **+ 读选 lane**） | ✅ **采用**（D7 · **改选**） | 循 **S3 对 D 侧的既有先例**（事实 ⑤″⑥）· 事实 ④：I 侧**零生产消费者**⇒ 爆炸半径有界 · 事实 ⑤‴：**读选 lane 无论协议都必须做** ⇒ ⇒ 与 G-7 一致，且**消除**「I-cache 第二份实现」 |
| ⑪ | 先做 S4c-1/2（功能），护栏参数化留到最后 | ❌ 拒 | 事实 ⑯⑰ ⇒ 每加一条线的成本随线数线性增长 ⇒ 必须在**第一条**新线之前解决 |
| ⑫ | 多级缓存（L2 / DRAM）并入 S4c | ❌ 拒（D5） | 依赖 full 互连 + 外部 DRAM 模型 ⇒ 代价差一个量级 ⇒ 独立 S4c-3 |

---

## 4. 后果 / 代价

### 4.1 正面

| # | 后果 |
|---|---|
| P1 | **除 D10 外，mini 线零回归面**（顶层端口表 + `Rv32iCoreTop` module 端口表 + 行为逐字不变）⇒ 285 条 ctest / 5 个护栏 / 28 个 tracked example 不受影响。⚠️ **D10 例外**：`AxiLiteCrossbar` 的产物端口表在**两条线**都增 19 根 `s6_*` |
| P2 | ADR-077 D3 的「真实装载路径仍未定义」**首次有实现路径**（S4c-2）⇒ 且 `prog_*` 仍保留（装载期 `ext_rst = 1`）|
| P3 | 加一条线的成本压到「该线自有文件 + 登记条目（per-line marker）+ **全局锚点重算**」⇒ 且**登记表自身有元护栏**⇒ ⇒ 不会「静默不覆盖新线」 |
| P4 | D11 使 S4c-2 **不引入** full 互连 ⇒ 判别面与仲裁复杂度不膨胀 |
| P5 | D7（原地升级，hit 延迟不变）使 mini 线**库层依赖面零变更**，且**不产生 I-cache 第二份实现** |
| P6 | D13 ② 使 C1 判据**无需改造** ⇒ N1 的改造面收窄到 C6 + V1/V2 + 2 个 py lib + 2 个 sh |

### 4.2 负面 / 代价

| # | 代价 | 量级 |
|---|---|---|
| N1 | **D8 的 S4c-0**：per-line marker 机制 + 元护栏 + 12 sh + 2 py 的**逐个处置** + 5 个全局整数锚点的归组 + 逐线 scope 标注（事实 ⑲）+ 散文表治理（事实 ⑱）⇒ **本 ADR 的最大项** | 1 批 |
| N2 | **D10**：§2.10 的 ⑤ 项成本（库层 `kNumSlaves` 6→7 + **19** 根具名端口 + fixture `set7/get7` + `CrossTestMap` **第 8** 窗口 + capability `spec.md:304` + mini 源 s6 面 **19** 根 tie-off + 「12→14 对」判据）⇒ **mini 线唯一回归面** | 1 批 |
| N3 | **D6**：新写 `AxiDmaLiteRegs`（寄存器映射 + 写副作用 + 读回语义） | 1 件 |
| N4 | **D12**：`AxiDma` 行为测试从零判别力补到可用（11 个 `name()` 断言 → 行为断言） | 1 批 |
| N5 | **D14**：取指接口 **fork** ⇒ 共享的 pipeline 层也分叉 ⇒ 流水线层的修复**不自动惠及**另一条线 ⇒ 两条线各自的取指时序**要各自验证** | 长期 |
| N6 | **D7 的同批重写**：`i_cache_prefetch.h` wrapper 的接线与端口面 + `test_cache_smoke.cpp` + `test_cache_complete.cpp` 的 fill 用例 ⇒ **既有 cache 测试不可能「不改全绿」** ⇒ AC8 的载体 MUST 改为「既有测试**已同批改写** + 新增 offset≠0 判别性用例」 | 1 批 |
| N7 | 两条线的 **CI 成本翻倍**（各自 example 构建 / RTL 仿真 / parity / 锚点） | 持续 |
| N8 | **命名不完全对称**（`riscv-mini` / `riscv-soc` vs `riscv-full` / `riscv-full-soc`）⇒ 读者需知道「mini 线是历史名」 | 文档义务（D9 已登记 backlog） |

### 4.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| R1 | **N1 漏做** ⇒ 后续每条新线都要手改 7+ 处 + 重算 5 个锚点 ⇒ 改动面失控 | D8 把 S4c-0 设为**硬前置**；AC3 / AC18 机械可判 |
| R2 | **N2 误以为「扩从口不影响 mini」** ⇒ 忘了改「12→14 对」/ fixture / capability ⇒ 判据与实现漂移 | AC5 写成**等式**（含 5 项成本逐条）；变异自证覆盖 |
| R3 | **N5 的双线流水线漂移** ⇒ 共享的 pipeline 修复只落在一条线 ⇒ 两线行为分叉 | 取指时序**各自**验证（AC16）+ 逐线 scope 标注（AC17）⇒ ⇒ **本 ADR 不声称两线取指时序等价** |
| R4 | **`AxiDma` 零判别力测试被误当成「DMA 可用」的证据** | D12：新行为测试就位前 MUST NOT 把 DMA 行为写进 AC |
| R5 | **散文 / 表格漂移**（事实 ⑱ 已实测一次）⇒ 「哪条线是权威」不明 | S4c-0：散文表纳入 `--check-products` 期望源（或删表留指针）+ per-line marker 登记面（AC19）|
| R6 | **治理红线的「线无关」假象**（事实 ⑲）⇒ 未来每个会话都读到自相矛盾的「红线」 | S4c-0 的逐线 scope 标注（AC17）|
| R7 | **D7 只做 fill 不做读选 lane** ⇒ 事实 ⑤′ 的「假 line」缺陷被冻结进组件 ⇒ full 线取指**直接功能失败**（顺序取指必踩 offset 4/8/12）⇒ 且既有测试 offset 全 0 ⇒ **判据全绿** | D7 明文「**两处一起改**」+ AC8 的 offset≠0 判别性用例（修复前 MUST 红）|

---

## 5. Verification（可勾选验收判据 · **前瞻**）

> ⚠️ 本章是**决策的前瞻判据**，不是实测日志。实施批次的 AC 由对应 openspec change 承接并回填映射。
> ⚠️ **载体**列给出该判据的**可 grep 载体**（AC2 是**门槛判据**：存在 ≠ 正确）。

| # | 判据 | 载体 |
|---|---|---|
| **AC1** | 仓内**零处** `examples/riscv-mini` / `examples/riscv-soc` 的**目录改名**（D1）⇒ 旧路径在 `openspec/changes/archive/**`、`.rddf/checks/**`、`docs/adr/**` 中**逐字保留** | 归档目录 grep |
| **AC2** | `examples/riscv-full/` 与 `examples/riscv-full-soc/` 存在（D2）⇒ 且**不与**任何既有 example 目录同名 | 目录存在（**门槛判据**） |
| **AC3** | 加一条线的成本 = **该线自有文件 + 登记条目（per-line marker）+ 全局锚点重算**（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` / `EXAMPLES_TEST_SOURCES`）⇒ 且 **0 处护栏脚本改动**；且 **12 sh + 2 py 的处置逐个登记**（mini-only by design / 纳入 / 已参数化）；`docs_link_scope.py` 的 `docs/riscv-mini/` 路径族**单独登记** | `.rddf/checks/` 的产品线登记面 + 根 `AGENTS.md` 锚点段 |
| **AC4** | 两线的端口快照**各自**与各自产物比对、**互不串扰** ⇒ mini = `Rv32iCoreTop` **53** 端口（**V1/V2**）+ **C6** 的 51 核自有快照；full = 其自身端口集（含新增 AXI4-full 取指口） | `verify_riscv_soc_core_ports.sh` **V1/V2** · `verify_no_duplicate_rv32i_headers.sh` **C6** |
| **AC5** | crossbar 判据 = **14 对**（D10）⇒ 且 fixture `set7/get7` + `CrossTestMap` **第 8** 窗口 + capability `axi-interconnect-routing/spec.md:304`「2 × 6」→「2 × 7」**同批**一致 | `tests/test_axi_lite_crossbar.cpp` · `openspec/specs/axi-interconnect-routing/spec.md` |
| **AC6** | mini 线**顶层端口表 + `Rv32iCoreTop` module 端口表 + 行为**逐字不变（**除 D10 的 `AxiLiteCrossbar` `s6_*`**）⇒ 与 P1 的限定语一致 | `--check-products` + V1/V2 + ctest |
| **AC7** | `AxiDmaLiteRegs` 的寄存器映射**手推**并落 capability（D6）⇒ 含「写 `reg_start` 的副作用」与「读 `status_*` 的值」 | capability `axi-interconnect-routing` 的新节 |
| **AC8** | `i_cache_sync.h` 的 **fill = 4-beat burst + ID** 且 **读按 `addr[3:2]` 选 lane**（D7 两处一起）⇒ 且 **hit 延迟（1 拍 `sread`）不变**；`i_cache_async.h` **零 diff**；wrapper `i_cache_prefetch.h` 与 `test_cache_smoke.cpp` / `test_cache_complete.cpp` 的 fill 用例**已同批改写**；**新增 offset≠0 的手推判别性用例**（burst fill 一行 4 个不同字 ⇒ 读 offset 4/8/12 各返回对应字）⇒ 且该用例在**修复前 MUST 红**（自证缺陷真实 · 事实 ⑤′⑤⁵） | `include/cpu/cache/i_cache_sync.h` + `tests/chlib/test_cache_complete.cpp` 的新增用例 |
| **AC9** | `AxiDma` 存在**行为断言**（≠ `dma.name()`）且覆盖 FSM 推进 / burst 切分 / 长度 / 错误路径（D12） | `tests/test_axi_dma.cpp` |
| **AC10** | 「DMA 装载后核能取指」的判据存在且**期望值手推**（S4c-2）⇒ 装载期 `ext_rst = 1`（ADR-077 D3）⇒ 且 `prog_*` 仍保留 | `examples/riscv-full-soc/` 的 TB + capability |
| **AC11** | **近期实施批次（S4c-0/1/2）不含 full 互连**；**S4c-3 的内容另立**（D5）⇒ ⇒ 本 ADR 的实施批次不含 `AxiInterconnect4x4` | openspec change 的 `tasks.md` 分批 |
| **AC12** | 本 ADR 的 Status 在 **oracle + critic 双审闭合**后才转 `Accepted`（arch 档判据：缺备选方案 / 缺回滚路径 / 自相矛盾 / 把未决写成已决 ⇒ 任一命中即 BLOCKER） | `.rddf/reviews/ADR-078-*.md` |
| **AC13** | full 线的核组件 / 头文件 basename 与 mini 线**全不相同**（D13 ②）⇒ ⇒ C1 判据**天然绿** | `verify_no_duplicate_rv32i_headers.sh` C1 |
| **AC14** | `examples/riscv-full-soc/` 的顶层 = **两级**（核复合体 + SoC 组合层）⇒ 与 ADR-077 D1 一致（D13 ①） | 产物 module 集 + `--check-products` |
| **AC15** | **`IfStage` 端口面快照护栏**判「**集合相等 / 零 diff**」（**不是** additive-only）⇒ ⇒ 被 D14 否决的「给 `IfStage` 加端口」在该护栏下**必红**（D14）⇒ 且 mini 线绿 | `scripts/` 新增快照判据（含 `--self-test` 投毒自证）|
| **AC16** | 共享 `IfStage` 对 full 线**零影响**：既有 `IfStage` 组件测试**不改动**全绿（D14 下 `IfStage` **没有**新端口 ⇒ 可判内核 = 既有测试零 diff 且全绿） | `tests/test_rv32i_pipeline_behavior.cpp` 零 diff |
| **AC17** | **逐线 scope 标注**：imem-`sread` 红线 / `rv32i-data-forwarding` 停顿矩阵标注「mini 线」；full 线对应物随 S4c-1 建立 | 根 `AGENTS.md` · `include/cpu/AGENTS.md` · capability |
| **AC18** | **登记表自身的元护栏**存在（发现集 == 登记集 + 配置完备性校验）⇒ ⇒ 半创建 / 忘登记的产品线**必红**（防「静默不覆盖新线」） | `.rddf/checks/` 的元护栏 |
| **AC19** | `examples/riscv-soc/AGENTS.md` 的 module 集表纳入 `--check-products` 期望源（或删表留指针）⇒ ⇒ 散文表漂移（事实 ⑱）被捕获 | 该文件 + `--check-products` 期望源 |

---

## 6. 边界与不变量（**不是决策**）

- **不改**：`prog_*` 根端口面（ADR-077 D3 · 长期保留）· `AxiLiteCrossbar` 的**数据面语义**（D10 只加从口数）· `rv32i_mem_map.h` 的段表（8 段不动）· mini 线的行为。
- **不改历史**：已归档 change / `.rddf/checks` 台账 / `docs/adr` 中的**旧路径引用**（AC1）。
- **ADR-041 的变体族不扩张**（D7：命中延迟维度不变 · `i_cache_async.h` 零 diff）⇒ ⇒ **不产生 I-cache 第二份实现**。
- **capability 表述随实现演进**：D10 会让 capability `axi-interconnect-routing` 的「2 × 6」变「2 × 7」⇒ 这是**归档机制**的正常演进（`openspec/specs/**` 是活的单一真相），**不是**回改已归档的 change（后者受 AC1 约束）。
- **两线不声称等价**：D3/D5/D14 明确两线在取指时序、cache 层级、互连拓扑上**不同构** ⇒ ⇒ 判据**不跨线复用**。
- **实施序**：**S4c-0 → S4c-1 → S4c-2 → S4c-3**（串行 · D8）。
- **归属**：本 ADR 只裁「形态 / 边界 / 分批」⇒ 逐文件实施清单归 openspec change 的 `tasks.md`；推导与时序证据归 `design.md`。
- **归档前**：`Draft` → `Accepted` + 关联 change **已立项**并回填 **AC 映射**。

---

## 7. 订正记录（**dated** · 不改决策）

> 约定：本节只记**表述与实现的收敛**。§2 的决策**不变**；真要改决策 MUST 走「取代」流程。

### 7.1 2026-11-28 · S4c-1a —— I-L1 的 AXI 侧 = **full 读通道 11 根**（无 ID）

- **订正对象**：§2.7 与 **AC8** 的「fill = 4-beat burst **+ ID**」表述。
- **实施形态**（**决策未变**：仍是 full 协议 + 4-beat burst + 读选 lane）：
  `ICacheSync` / `ICachePrefetch` 的 AXI 侧 **只读** ⇒ **无 AW/W/B**；
  读通道 **11 根** = `ar_addr` / `ar_len` / `ar_size` / `ar_burst` / `ar_valid` / `ar_ready`
  + `r_data` / `r_resp` / `r_last` / `r_valid` / `r_ready` ⇒ ⇒ **无 `ar_id` / `r_id`**。
- **三条依据**：① **I 侧只读出** ⇒ 写通道无落点；② **与 S3 先例同构** ——
  `d_cache_sync.h` 的读通道同样是 11 根**且无 ID**（事实 ⑥ 所指的「S3 完整形态」的**实际**形态）；
  ③ 单主场景下 ID 无路由作用，且下游 `TcmAxiSlave` **忽略 ID 值**（事实 ⑤）。
- **不得被视为放松的项**：hit 延迟仍 **1 拍 `sread`**；`ready` 仍是 I 侧语义
  （「请求已被接受」⇒ 数据**下一拍**到 · **不**照搬 D 侧组合 ready）；`i_cache_async.h` **零 diff**。
- **落地**：change `rv32i-i-l1-axi-full-2026-11`（`design.md` §1.1 ① / `tasks.md` T1.1 · AC1–AC8）。
