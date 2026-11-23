# design · S4a（核的 MMIO 通道 + SoC 侧 Lite 互联 + GPIO 回环）

> **Change**：`rv32i-soc-mmio-channel-2026-11`
> **前置**：`rv32i-commit-event-pulse-2026-11`（**S4a-0**）· **MUST 先落地**
> **上游决策**：G-6（`.rddf/checks/decision-request-g6-mmio-ownership-2026-11.md`）
> · G-9（`.rddf/checks/decision-request-g9-core-port-face-v2-2026-11.md` · **含 2026-11-24 订正块**）

## 1. 库层：地址表扩成全表（G-6）

### 1.1 `rv32i_mem_map.h` 的新形状

```cpp
// MMIO 段（每段 4 KB · 自 0x8010_0000 顺延 · G-6 裁决）
inline constexpr uint64_t kMmioSegmentBytes = 0x1000;      // 4 KB
inline constexpr MemRegion kClint{0x80100000ull, kMmioSegmentBytes};
inline constexpr MemRegion kUart {0x80101000ull, kMmioSegmentBytes};
inline constexpr MemRegion kGpio {0x80102000ull, kMmioSegmentBytes};
inline constexpr MemRegion kSpi  {0x80103000ull, kMmioSegmentBytes};
inline constexpr MemRegion kI2c  {0x80104000ull, kMmioSegmentBytes};
inline constexpr MemRegion kPwm  {0x80105000ull, kMmioSegmentBytes};
inline constexpr MemRegion kDma  {0x80106000ull, kMmioSegmentBytes};

inline constexpr uint64_t kMmioBase  = kClint.base;                       // 0x8010_0000
inline constexpr uint64_t kMmioBytes = 7ull * kMmioSegmentBytes;         // 0x7000
```

⇒ ⇒ **MMIO 段是连续的一段** ⇒ ⇒ 跨段常量可**派生**（`kMmioBytes = 7 × 4KB`）⇒ ⇒ **禁手写 0x7000**
⇒ ⇒ `static_assert` 连续性（每段 base = 前一段 base + size）⇒ ⇒ 段表**不可能**再漂移。

### 1.2 分流判据（**G-9 订正后的唯一形式**）

```cpp
// 判据 = 「地址 ∈ MMIO 段」⇒ 旁路；否则 ⇒ 现有 D-L1 通路
constexpr bool in_mmio(uint64_t byte_addr) {
    return byte_addr >= kMmioBase && byte_addr < kMmioBase + kMmioBytes;
}
```

⚠️ **判据方向 MUST 是「MMIO 段」**（不是「D-TCM 段」）⇒ 见 G-9 订正块
⇒ ⇒ **非 MMIO 段的一切访问逐拍不变**（含 TCM 段、含越界访问）⇒ ⇒ 核级零行为变化。

### 1.3 `address_decoder.h` 改派生 + 补片选

- 3 段硬编码（`:43-45`）⇒ 派生自上表
- 补 4 片选（`spi_cs` / `i2c_cs` / `pwm_cs` / `dma_cs`）
- ⇒ ⇒ **总片选 9**（2 TCM + 7 MMIO）+ `error_cs` ⇒ **10** 个输出端口
- ⇒ ⇒ **6 处测试消费者**（`examples/riscv-mini/tests/test_riscv_tests_soc.cpp`）同步更新
- ⚠️ 文件头表格（`:10-19`）仍写「I-TCM 64KB / D-TCM 64KB」⇒ ⇒ 本次**顺手订正**为 16 KB
  （它是**既有漂移** · 与本 change 同文件 ⇒ 零额外成本 ⇒ ⇒ 属「改动最小化」允许的**同文件同步**）

## 2. 核内：pre-cache 分流

### 2.1 分流点

分流发生在 **核顶层 `rv32i_core_top.h` 的 `describe()`** —— 即 pipeline 与数据通路之间：

```
pipeline.io().data_addr / data_req / data_write_en / data_write_data / data_strbe
        │
        ├─ in_mmio(data_addr) ?
        │      ├─ 是 ⇒ 核侧 AXI4-Lite master（新增 `axil_mmio_master.h`）
        │      │        ⇒ ready = master 完成脉冲 · rdata = master 保持寄存器
        │      └─ 否 ⇒ 现有 D-L1 通路（`DCacheSync` → `TcmAxiSlave` → `DataTCM`）**一字不改**
```

⇒ ⇒ **S3 的接线零返工**（TCM 分支原样保留）⇒ ⇒ 核级行为**逐拍不变**。

### 2.2 两个 mux（**只有两处**）

| 信号 | mux |
|---|---|
| `data_ready` | `in_mmio ? master.ready : cache.ready` |
| `data_read_data` | `in_mmio ? master.rdata : cache.rdata` |

⇒ ⇒ ⚠️ **`data_write_en` / `data_write_data` / `data_strbe` MUST NOT 分流**
⇒ ⇒ 理由：MMIO 写也走 master，但 **`data_write_en` 是「观测事件」**（S4a-0 后是脉冲）
⇒ ⇒ **MUST NOT** 把它 mux 成「MMIO 侧的写使能」（否则 store 事件会**重复计**）
⇒ ⇒ ⇒ **`data_write_en` MUST 保持 pipeline 原样透传**（它描述的是「流水线发起了 store」，
不是「store 打到了哪条总线」）⇒ ⇒ store 事件**与路径无关** ⇒ ⇒ 判别力更强。

⚠️ `data_req` 也 **MUST NOT** mux ⇒ ⇒ 理由：`data_req` = 「MEM 级有访存请求」
⇒ ⇒ 它**必须**对两条路径都成立（否则 D-L1 侧会误发 AXI 事务）
⇒ ⇒ ⇒ 但 master 只在 `in_mmio` 时才启动事务 ⇒ ⇒ master 的 `req` 输入 =
`pipeline.data_req & in_mmio` ⇒ ⇒ **在 master 内部做门控**，不改 pipeline 侧的 `data_req`。

### 2.3 核侧 AXI4-Lite master（`axil_mmio_master.h` · **核侧自带**）

**接口**（保持式握手 · 同 cache wrapper 的纪律）：

| 端口 | 方向 | 语义 |
|---|---|---|
| `req` | in | 「发起一次访存」（**MUST 保持到 `ready`**） |
| `we` | in | 写 / 读 |
| `addr` | in | **绝对字节地址** |
| `wdata` | in | 写数据（**车道已对齐**） |
| `strbe` | in | 字节使能（4 bit） |
| `ready` | out | **完成脉冲**（1 拍）⇒ 事务完成的那一拍为 1 |
| `rdata` | out | 读数据（**保持寄存器** · 响应到达后保持到下次读完成） |

**状态机**（AXI4-Lite · AW/W 相位分离）：

```
IDLE ──req&&!we──► AR ──arvalid&&arready──► RWAIT ──rvalid&&rready──► DONE(ready=1) ──► IDLE
IDLE ──req&& we──► AW ──awvalid&&awready──► W ──wvalid&&wready──► BWAIT ──bvalid&&bready──► DONE ──► IDLE
```

⇒ ⇒ **VALID 保持到握手**（AXI 合规 master 的义务）
⇒ ⇒ **AW 与 W 相位分离**（外设的 `aw_done` 是寄存器 ⇒ 同拍 AW+W 时 W **不被接受**
⇒ ⇒ ⇒ master MUST 保持 `WVALID` 到 `WREADY` ⇒ ⇒ 本状态机天然满足）
⇒ ⇒ **`bready` / `rready` 恒拉高**（无背压 · G-9 契约）
⇒ ⇒ **`awprot` / `arprot` 恒 0**
⇒ ⇒ 复位：与核的 `ext_rst`（**高有效**）一致 ⇒ ⇒ **不新增核端口**

⚠️ **`ready` 是「完成脉冲」而非「电平」** ⇒ ⇒ 与 cache 的 `ready`（电平 · 对当拍 addr）**语义不同**
⇒ ⇒ ⇒ 这是**故意的**：MMIO 事务的完成是**事件** ⇒ ⇒ 脉冲
⇒ ⇒ ⇒ ⚠️ **口径 = 「事务完成的那一拍为 1」** ⇒ ⇒ **与 cache 路径完全同口径**
⇒ ⇒ ⇒ ⇒ cache 路径早已是「完成拍」（S2 的真反压）⇒ ⇒ `rv32i_core_top.h:188`
⇒ ⇒ ⇒ ⇒ `data_ready <<= (!pipeline.io().data_req) | dcache.io().ready;`
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ miss 时 `dcache.ready = 0` ⇒ ⇒ `data_ready` 被拉低 ⇒ ⇒ 冻结多拍 ✓
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **零 spec delta**：`rv32i-dcache-timing:111` 的
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 「`data_ready` MUST 为 `!data_req | cache.ready`」是 **cache 接线专属**条款
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ MMIO 路径用 `!data_req | master.ready` **不违反**它 ✓
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 该口径**只影响 MMIO 路径** ⇒ ⇒ 核级零影响 ✓

## 3. SoC 侧装配

### 3.1 `Map::windows[]`（**关键 · 决定 DECERR 是否可达**）

`AxiLiteMatrix2x2` 的解码（`include/axi4/axi4_lite_matrix.h:249-252`）：

```
mapped = h0 | h1        ← 命中**任一**窗口
target = select(h0, false, h1)   ← s0 优先
```

⇒ ⇒ ⚠️ **命中 `windows[1]` ⇒ 路由到 s1 ⇒ s1 未接 ⇒ `m0_awready` 永不拉高 ⇒ 挂死**
⇒ ⇒ ⚠️ **未命中任何窗口 ⇒ `mapped=0` ⇒ 走矩阵自带 error responder（DECERR）**

⇒ ⇒ **裁决**：

| 窗口 | 取值 | 理由 |
|---|---|---|
| `windows[0]` | `{kGpio.base, 4 KB}` = `{0x80102000, 0x1000}` | GPIO 段（G-6 的 `kGpio`）⇒ 从库层地址表**派生** |
| `windows[1]` | `{0xFFFFF000, 0x1000}` | **最高 4 KB** ⇒ 核的 MMIO master **永不**产生该地址 ⇒ ⇒ s1 永不被选中 ⇒ ⇒ s1 的 8 根输入可安全 tie 0<br>⚠️ **MUST NOT** 放低位（如 `{0, 4KB}`）⇒ 那会让**地址 0**（核级程序的 `sw x3,0(x0)`）在**将来某次改动**后路由到未接的 s1 ⇒ ⇒ 挂死 |

⇒ ⇒ **AC8 的 DECERR 判据**（访问 `0x8010_0000` = `kClint.base` · S4a 未接 CLINT）
⇒ ⇒ ⇒ `0x8010_0000` ∉ window[0]（GPIO 段）∉ window[1]（最高 4KB）⇒ ⇒ `mapped=0` ⇒ ⇒ **DECERR** ✓

### 3.2 根端口面

| 项 | S4a 前 | S4a 后 |
|---|---|---|
| 根自有 | 6（`ext_rst` / `prog_we` / `prog_addr` / `prog_data` / `store_count` / `writeback_count`） | **8**（+ `gpio_out`（8 bit）· `gpio_in`（8 bit）） |
| 产品端口 | 8 | **10** |
| 产物 module 集 | 10 | **12**（+ `AxiLiteMatrix2x2` · `AxiLiteGpio`） |

⚠️ `AxiLiteGpio` 的 `gpio_dir`（输出）**不落根**（不是用户可见 IO）⇒ ⇒ 只落 `gpio_out` + `gpio_in`
⚠️ `AxiLiteGpio` 的 `irq`（输出）**不落根**（G-5：IRQ 在 SoC 内部汇聚 · 实施在 S5）⇒ ⇒ **S4a 不接**

### 3.3 复位极性

- 核的 `ext_rst` = **高有效**
- `AxiLiteMatrix2x2.rst_n` / `AxiLiteGpio.rst_n` = **低有效**（AGENTS.md 的 peripheral 规范）
⇒ ⇒ SoC 根驱动：`rst_n <<= !io().ext_rst` ⇒ ⇒ **组合取反**（不需要新根端口）

## 4. 未驱动输入清点（AC6 的载体）

| 组件 | 输入端口 | 数量 | 驱动源 |
|---|---|---|---|
| 核 `Rv32iCoreTop` | `axil_awready` / `axil_wready` / `axil_bresp` / `axil_bvalid` / `axil_arready` / `axil_rdata` / `axil_rresp` / `axil_rvalid` | **8** | matrix 的 `m0_*` 输出 |
| `AxiLiteMatrix2x2` | m1 面：`m1_awaddr` / `m1_awprot` / `m1_awvalid` / `m1_wdata` / `m1_wstrb` / `m1_wvalid` / `m1_bready` / `m1_araddr` / `m1_arprot` / `m1_arvalid` / `m1_rready` | **11** | **tie 0** |
| `AxiLiteMatrix2x2` | s1 面：`s1_awready` / `s1_wready` / `s1_bresp` / `s1_bvalid` / `s1_arready` / `s1_rdata` / `s1_rresp` / `s1_rvalid` | **8** | **tie 0** |
| `AxiLiteMatrix2x2` | `rst_n` | 1 | SoC 根（`!ext_rst`） |
| `AxiLiteGpio` | `rst_n` | 1 | SoC 根（`!ext_rst`） |
| `AxiLiteGpio` | `gpio_in` | 1 | 根端口 `gpio_in` |

⇒ ⇒ **零未驱动** ⇒ ⇒ 发射的 fail-closed 判据绿。
⚠️ **核级 TB**（`examples/riscv-mini/tb_rv32i_soc.cpp` + `.sv`）⇒ ⇒ 核的 **8 根 AXI-Lite 输入 MUST 显式 tie 0**
⇒ ⇒ **核级 24 条判据零变化**（核级程序**不做** MMIO 访问 ⇒ MMIO 通道永不被触发）。

## 5. 手推（SoC 级 · 12 字程序）

程序见 `proposal.md` §程序。**手推链**：

```
word 0  addi x1, x0, 42      ⇒ 写回 x1 = 42
word 2  addi x2, x0, 11      ⇒ 写回 x2 = 11
word 4  add  x3, x1, x2      ⇒ 写回 x3 = 53
word 5  sw   x3, 0(x0)       ⇒ store @ 字节地址 0 · 数据 53      ← **走 D-L1**（write-through 单拍）
word 7  lui  x4, 0x80102     ⇒ 写回 x4 = 0x8010_2000 = kGpio.base
word 8  lw   x6, 4(x4)       ⇒ **MMIO 读** @ 0x8010_2004（DATA_IN）⇒ 写回 x6   ← **走旁路**
word 9  addi x5, x0, 0xA5    ⇒ 写回 x5 = 0xA5
word 10 sw   x5, 0(x4)       ⇒ **MMIO 写** @ 0x8010_2000（DATA_OUT）· 数据 0xA5  ← **走旁路**
word 11 jal  x0, 0           ⇒ rd = x0 ⇒ **不写回** · 自环
```

⇒ ⇒ **写回事件 = 6**（x1 / x2 / x3 / x4 / x6 / x5）· **store 事件 = 2**
⇒ ⇒ **`gpio_out == 0xA5`**（DATA_OUT 的 8 bit 值）

⚠️ **口径（依赖 S4a-0）**：事件 = **事件次数**（与事件时长无关）⇒ ⇒ MMIO 的多拍停顿**不计入**

## 6. 载体清单（**全部要同批重算**）

| 载体 | 位置 | S4a 前 → 后 |
|---|---|---|
| 核端口契约表 | `examples/riscv-mini/AGENTS.md` | 34 → **53** 行 |
| C6 快照 | `scripts/verify_no_duplicate_rv32i_headers.sh` | 32 → **51** 条 |
| 核端口护栏 | `scripts/verify_riscv_soc_core_ports.sh` | 硬编码 34 → **53**（多处） |
| SoC 产品面表 | `examples/riscv-soc/AGENTS.md` | 8 端口 → **10** · module 10 → **12** |
| SoC 期望台账 | `.rddf/checks/soc-rtl-expectations.md#riscv-soc` | §1 产物形态 · §2 程序 · §3 判据（7 → **9** 条）· §4 变异（重做） |
| 核级期望台账 | `.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc` | **零改动**（程序与行为均不变）⇒ ⚠️ 但**核端口面**变了 ⇒ 若该台账登记了端口数 ⇒ 需同步 |
| 覆盖 tsv | `scripts/verify_examples_rtl_sim_coverage.sh` 的登记表 | 追加 S4a 行 |
| 公开面登记 | `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` | 库层 2 个头 |
| ADR-077 | `docs/adr/ADR-077-*.md` | **dated addendum**（V1 8 → 10 端口 · V2 10 → 12 module）⇒ 决策不变 |

## 7. 判别力自证（变异 · MUST 实测转红）

| # | 变异 | 期望转红 |
|---|---|---|
| M1 | **分流判据取反**（写成「D-TCM 窗口 ⇒ D-L1」） | **AC4**（核级 TB 挂死 ⇒ 超时 ⇒ 红）· **AC3**（word 5 的 store 落进 MMIO ⇒ `store_count` 少 1） |
| M2 | MMIO **不走旁路**（接进 D-L1） | **AC2**（MMIO 读挂死 ⇒ 计数冻结） |
| M3 | 两条兄弟边**对调**（`store_event` ↔ `retire_event`） | **AC3**（`(2,6)` ↔ `(6,2)`） |
| M4 | 两条边**接同一源** | **AC3**（两值相同） |
| M5 | `windows[1]` 改成低位 `{0, 4KB}` | **AC4**（核级 `sw x3,0(x0)` 若误入 MMIO ⇒ 路由到未接的 s1 ⇒ 挂死）⇒ ⇒ ⚠️ **此变异只在分流判据同时取反时可见** ⇒ ⇒ 记录为**组合变异** |
| M6 | GPIO 的 `gpio_in` 未驱动 | **AC6**（未驱动判据） |

⇒ ⇒ **每个变异 MUST 先确认仍能编译** + **重建后**复跑（还原源码 ≠ 复原）。

## 8. 残留（如实登记）

| # | 残留 | 理由 |
|---|---|---|
| R1 | **`windows[1]` 指向未接的 s1** ⇒ 若核产生该地址 ⇒ 挂死 | 核的 MMIO master **只发 MMIO 段地址** ⇒ ⇒ 不可达 ⇒ 登记为边界 |
| R2 | **D-TCM 越界访问仍静默别名** | 分流判据是「MMIO 段」⇒ ⇒ 越界访问仍走 D-L1 ⇒ ⇒ 零行为变化 ⇒ 属后续项 |
| R3 | **MMIO 路径的 `data_ready` 语义 = 「完成拍」** | 与 cache 路径**同口径**（cache 早已是完成拍 · S2）⇒ ⇒ **零 spec delta** ⇒ ⇒ 零影响核级（§2.3） |
| R4 | **`AxiLiteGpio.irq` 未接** | G-5：IRQ 实施在 S5 ⇒ ⇒ S4a **不接**（不是死端口 —— 它是**输出**，不接不影响） |
