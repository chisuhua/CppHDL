# 决策请求 · G-7：平台总线类（AXI4-Lite vs AXI4 full）

> ## ✅ 已裁决（2026-11-23 · 用户）
>
> **裁决 = B（full AXI4 主干）· S3 一次做完（不拆 S3a/S3b）**
>
> 生效范围：
> - S3 的 D-L1 直接接 **AXI4 full 主口 + 4-beat burst**（`ARLEN=3` / `ARSIZE=2`）
> - S4 用 `AxiInterconnect4x4` + `AxiToAxiLite`（每外设组一个桥）· `AxiDma` 全速
> - S6（L2/DRAM）**零返工**
> - ⑥（D-Cache 假 line）的修法 = burst 4-beat fill + **读按 `addr[3:2]` 选 lane**
>   —— 共同部分（读选 lane）**无论协议都必须做**
>
> 未裁决的相关门：**G-5**（核 `irq` 口）· **G-6**（MMIO 归属）· **G-8**（外部总线是否上根端口）
> ⇒ 建议顺序仍是 **G-5 → G-6 → G-8**（G-7 已闭）。

- **日期**：2026-11-23 · builder 起草（**待用户 / architect 裁决**）
- **母 improvement**：`rv32i-soc-platform-2026-11` 的决策门 **G-7**
- **影响批次**：**S3**（核数据侧总线化）· **S4**（互连 + 外设 + DMA）· **S6**（L2 / DRAM）
- **建议裁决顺序**：**G-7 → G-5 → G-6 → G-8**（G-7 决定 S3 的口形态）

## 一、要裁的是什么

> **平台的「主干总线」用 AXI4-Lite，还是 AXI4 full（含 burst）？**

它决定三件事：
1. **S3 的 D-L1 是否现在就支持 burst**（母 improvement 的 S3 已写「wrapper burst 升级」）
2. **S4 用 `AxiLiteMatrix2x2`（77 端口）还是 `AxiInterconnect4x4`（113 端口）**
3. **S6 的 L2 / DRAM 是否返工**

## 二、事实（全部实测 · 带坐标）

| # | 事实 | 证据 |
|---|---|---|
| ① | **6 个外设全是 AXI4-Lite**（`axi_uart` 有 `awaddr`/`araddr`，**无** `awlen`/`arlen`） | `include/axi4/peripherals/axi_uart.h:54/:68`（每外设 23 端口） |
| ② | **`AxiDma` 是 AXI4 full 主**（有 `awlen`/`awsize`/`awburst`） | `include/axi4/peripherals/axi_dma.h:29-30`（19 端口） |
| ③ | **两种互连都有产品级用例**：Lite 矩阵在 `axi_soc_example_top.h:196`；full 4×4 在 `phase3a_top.h:65` | 两者都被 `run_all_ported_tests.sh` 跑到（PASS 29 / FAIL 0） |
| ④ | **full → Lite 桥已有且在 phase3a 产品级使用** | `include/axi4/axi_to_axilite.h:33` · `examples/axi4/phase3a_top.h:74` |
| ⑤ | **cache 的「AXI 口」是自制简化握手**（`axi_addr`/`axi_wdata`/`axi_we`/`axi_valid`/`axi_ready`/`axi_rdata`/`axi_resp_valid`），**不是**标准 AXI4 也不等价于 AXI4-Lite 五通道 | `include/cpu/cache/d_cache_sync.h:68-74` |
| ⑥ | 🔴 **`DCacheSync` 的 line 数据通路是「假 line」**：内部 line = 128 bit（4 字），但 ① fill 是 `zext<128>(axi_rdata)` ⇒ 只填**低 32 位**、其余补零；② 读是 `bits<31,0>(cache_data)` ⇒ **`offset` 完全不参与数据选择** ⇒ **同一 line 的 4 个地址恒读到第一个字** | `d_cache_sync.h:49`（`DATA_WIDTH = 128`）· `:71`（`axi_rdata` 32 bit）· `:132`（`bits<31,0>(cache_data)`）· `:171/:175`（`zext<128>(axi_rdata)`）· `:102`（`offset` 被算出但**只进 index/tag**） |
| ⑦ | **`d_cache_sync.h` 内零 burst 逻辑**（无 `awlen`/`beats`/`last` 任何痕迹）⇒ 加 burst = **新写状态机** | `grep -E "burst\|beats" include/cpu/cache/d_cache_sync.h` ⇒ 0 命中 · 全文 191 行 |
| ⑧ | **ADR-073 ⑫（Accepted）**：顶层 bundle 不是 Verilog 端口 ⇒ **AXI 总线永不在 SoC 顶层** ⇒ 总线只能是**内部**连接 | `docs/adr/ADR-073-soc-rtl-product-and-tb.md:45` |
| ⑨ | 端口数：`AxiLiteMatrix2x2` **77** · `AxiInterconnect4x4` **113** | `awk '/__io\(/,/^    \)/' … \| grep -c` |

**⑥ 是本次调研的新发现**，它把「burst 是性能优化」变成「**line 数据通路的正确性**」：
- 当前 `offset` 只进 index/tag，**不进数据选择** ⇒ 同一 line 的 4 个地址恒读第一个字
  ⇒ ⇒ 这**不是性能问题，是功能错误**（只要 line > 1 字）
- 修法 = **填充**（4 个 lane 都写）+ **读**（按 `addr[3:2]` 选 lane）两处都要做
- burst 的价值：`4-beat × 32-bit` 正好铺满 128-bit line（`ARLEN=3` / `ARSIZE=2`），
  且 `rlast` 天然就是 line 边界 ⇒ 比「4 次独立读 + 手工拼装 + 逐拍数 beat」更干净

⚠️ **该缺陷当前不可观测**：D-Cache **未接入** RV32I 流水线（S1/S2 的核用 TCM）⇒ 一旦 S3 接上，
「同 line 4 地址读到同一个字」会立刻表现为**功能错误**。⇒ ⇒ **S3 必须先修它**（与 G-7 的选择正交）。

## 三、选项对比

| 维度 | **A · Lite 全平台** | **B · full 主干 + Lite 外设**（推荐） | **C · 推迟 burst**（S3 Lite → S6 换 full） |
|---|---|---|---|
| S3 的 D-L1 口 | 自制握手 → AXI4-Lite 桥（新写）· line 填充 = **4 次 32-bit 读 + 拼装** | 自制握手 → **full AXI4 主**（含 burst）⇒ line 填充 = **1 次 4-beat burst** | 同 A（Lite 桥 + 4 次读） |
| S3 工作量 | 桥 + 拼装逻辑 + 双后端验证 | **burst 状态机**（191 行文件内新写）+ 双后端验证 | 同 A |
| S4 互连 | `AxiLiteMatrix2x2`（77 端口，直接可用） | `AxiInterconnect4x4`（113 端口，**过配**：当前 2 主 2 从 ⇒ 其余 tie-off，ADR-073 ⑥ 有成例） | 同 A |
| S4 的 DMA | `AxiDma` 是 full 主 ⇒ **必须降级**（burst 被桥吃掉 ⇒ DMA 退化为逐字搬运） | **全速**（DMA 的核心能力 = burst） | 同 A（DMA 被削） |
| S6（L2 / DRAM） | **返工**：cache 桥与 line 填充逻辑都要改成 burst | **零返工**（本来就在 burst 世界） | **返工**（同 A，只是更晚） |
| 事务数（line 填充） | **4** 次 | **1** 次（4-beat） | 4 次 |
| 新增风险 | 拼装逻辑是新写的（无既有用例） | burst 状态机是新写的（**但有 `Axi4Slave` 的 burst 契约 + `axi4-full-slave-burst` spec 与测试**可依） | 同 A + 一次协议切换 |
| 与母 improvement 的一致性 | 违背 S3 的「wrapper burst 升级」 | **一致** | 把 S3 的一句挪到 S6 |

## 四、推荐：**B（full 主干）**

**核心理由（按权重）**：

1. **⑥ 的正确性**：S3 无论如何都要把 128-bit line 填对。burst 是最自然的通道
   （4-beat = 4×32 = 128），而 A 要新写一段**没有任何既有用例**的拼装逻辑。
2. **②+④ 的资产**：`AxiDma` 是 full 主、`AxiToAxiLite` + `AxiInterconnect4x4` 已在 phase3a
   产品级跑通 ⇒ **full 主干不是新造**，代价集中在 S3 的 cache burst 状态机。
3. **S6 零返工**：L2 / DRAM 必然是 burst 世界（母 improvement 的 S6 也这么写）。
4. **ADR-073 ⑫ 已封住暴露风险**：AXI 不上 SoC 顶层 ⇒ 换协议**不影响产品端口面**。

**代价（诚实列出）**：
- S3 的改动从「写桥」变成「写 burst 状态机 + 双后端覆盖」⇒ **S3 会变重**
- `AxiInterconnect4x4` 113 端口**过配**（4 主 4 从）⇒ 需 tie-off 未用端口（有现成 idiom）
- 若日后确认「外设只有 Lite、DMA 也不用 burst」，B 的复杂度就白付了 ⇒ **但②否证了这点**

**若你想要「S3 尽快跑通」**，可折中：**B 的方向 + S3 拆两个 change**
- `S3a`：cache 接 **AXI4-Lite**（点对点 · 无互连）先跑通「核 → D-L1 → 内存」闭环
  （⚠️ 仍需解决 ⑥ 的 line 填充 ⇒ 4 次读 + 拼装 **或** 口宽 128-bit）
- `S3b`：把 cache 的 Lite 桥换成 **full AXI4 主口 + burst**（独立 change · 独立判据）
⇒ ⇒ 代价 = S3a 的桥在 S3b 被替换（**有意返工**），换来两个都可独立验收的中间态。

## 五、选 B 时 S3 的具体工作量（供裁决参考）

| 项 | 内容 | 规模 |
|---|---|---|
| 1 | `DCacheSync` 的 AXI 口改 full（`awid/awlen/awsize/awburst` + `arlen/arsize/arburst` + `wlast/rlast`） | 口 ~+10 根（**组件级**，不进核端口面） |
| 2 | burst fill 状态机（`ARLEN=3` · 4 beat · `rlast` 收尾 · 依次写 4 个 32-bit lane） | 新写 ~80-120 行 |
| 3 | burst writeback（dirty line 回写 · `AWLEN=3` + `wlast`） | 新写 ~60-80 行 |
| 4 | miss 期间地址锁存 / 反压接线（**S2 的 `data_ready` 在这里第一次真正被拉低**） | ~20 行 + 已在 S2 打通的冻结链 |
| 5 | 双后端覆盖（interpreter + verilator）· 手推 burst 事务（len/size/burst）· hit/miss 拍数契约 | 新测试 ~200 行 |
| 6 | 判据：line 填充后 **4 个字全部可读回**（对 ⑥ 的直接判别） | 手推期望值 |

⇒ ⇒ 估算 **S3 ≈ 1.5–2 个 change 的体量**（远大于 S2）。

## 六、需要你裁决的三件事

1. **总线类**：A（Lite）/ **B（full · 推荐）** / C（推迟 burst）？
2. 若选 B：**S3 一次做完**（burst 一步到位），还是**拆 S3a / S3b**（先 Lite 闭环再换 full）？
3. **⑥ 的修法**（**无论 A/B 都必须做**，因为它是功能错误）：
   - **共同部分**（协议无关）：**读**按 `addr[3:2]` 选 lane + **填充**写满 4 个 lane
   - **协议部分**（这才是 G-7 的分叉）：
     - (i) **burst 4-beat**（选 B）⇒ `rlast` 即 line 边界，逐 beat 写 lane
     - (ii) **4 次独立 32-bit 读**（选 A）⇒ 需在 miss 期间锁存 line 基址 + 数第几拍
     - (iii) ❌ **口宽改 128-bit 不是独立选项** —— 它**解决不了 offset 选择**
       （仍要按 lane 选），只是把「拼装」搬进总线宽度 ⇒ 且 128-bit 总线与 32-bit 外设
       需要宽度转换 ⇒ **不推荐**

> 裁决后我会**立即**把 S3 写成 openspec change（含手推 burst 事务与 hit/miss 拍数契约），
> 不需要再等一轮确认。
