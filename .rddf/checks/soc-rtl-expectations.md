---
evidence: probe:scripts/verify_examples_rtl_sim.sh
---

# SoC 层 RTL 期望登记（`soc-rtl-product-and-tb-2026-11` · **T4**）

> 本文件是 **SoC 层 TB 的期望值出处**。TB 头部 MUST 写
> `EXPECT-SRC: .rddf/checks/soc-rtl-expectations.md#soc-example`，
> **禁止**把被测 `examples/axi4/axi_soc_example_top.h` 或产物 `top.v` 写成出处
> （那是**转录**不是推导 —— 循环论证）。
>
> ⚠️ **期望分两级**（沿用 `.rddf/checks/examples-rtl-expectations.md` 的约定）：
> **spec-derived**（计入）= AXI4-Lite 协议 / UART 8N1 帧格式 / matrix 的默认地址窗口；
> **impl-contract**（不计入）= 拍数余量 · 端口名编号 · tie-off 值。

---

## 通用推导前提（impl-contract · 所有 SoC TB 共用）

- 「**一拍**」= 一个 `posedge default_clock`。
- **复位释放 MUST 在 `negedge`** —— 在 `posedge` 同一时刻（0 delta）拉低 `rst` ⇒ 该沿的
  `always_ff` 会采到**新的** `rst` 值 ⇒ 复位拍被吃掉（实测：手推 1、实测 2）。
- **采样时机**：`@(posedge clk); #1;` ⇒ 读的是沿**后**的值。
- ⚠️ **顶层端口已具名**（ADR-075 · 2026-11-17）⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`
  ⇒ ⇒ **无语义编号 ⇒ 不再需要「按位宽序列反推编号」那张对照表**
  ⇒ ⇒ 下面的映射表改为**直接登记**（成员名 → 产物端口名）。

---

## axi_soc_example {#soc-example}

### 1. 顶层端口表（**13** 个 = 框架 2 + 根自有 11）

⚠️ **口径（2026-11-17 · ADR-075）**：根 `__io` 的 11 个端口**全部具名**
⇒ ⇒ 产物端口名 = `top_<C++ 成员名>` ⇒ ⇒ **不需要**「按声明顺序 + 位宽序列反推编号」
⇒ ⇒ 下表是**直接登记**（权威 = `manifest.port_table[]`，与产物端口表**同源**）。

| # | 产物端口名 | 方向 | 位宽 | C++ 成员（根 `__io`） | 语义 |
|---|---|---|---|---|---|
| 1 | `default_clock` | in | 1 | （框架注入） | 时钟 |
| 2 | `default_reset` | in | 1 | （框架注入） | 复位（高有效） |
| 3 | `top_start` | in | 1 | `start` | 启动一笔事务 |
| 4 | `top_write` | in | 1 | `write` | 1=写 · 0=读 |
| 5 | `top_addr` | in | 32 | `addr` | 事务地址 |
| 6 | `top_wdata` | in | 32 | `wdata` | 写数据 |
| 7 | `top_rst_n` | in | 1 | `rst_n` | **AXI 级低有效**复位（透传给 gpio/matrix/uart） |
| 8 | `top_gpio_in` | in | 8 | `gpio_in` | GPIO 外部输入 |
| 9 | `top_uart_rx` | in | 1 | `uart_rx` | UART 接收（**空闲高**） |
| 10 | `top_rdata` | out | 32 | `rdata` | 读数据 |
| 11 | `top_done` | out | 1 | `done` | 完成标志 ⚠️ **粘滞**（首笔后恒 1）⇒ **禁**用它判完成 |
| 12 | `top_gpio_out` | out | 8 | `gpio_out` | GPIO 输出 |
| 13 | `top_uart_tx` | out | 1 | `uart_tx` | UART 发送（**空闲高**） |

⚠️ **13 个端口 MUST 全部连接**（否则严格 flags 下 `%Warning-PINMISSING` **致命** ——
仓内实证）。

⚠️ **2026-11-21**：原第 10 行 `top_timer_irq`（**零消费死端口** · ADR-073 事实⑭）已
**物理删除**（台账 id=27 · ADR-074 §7.7 裁决 = 删 ⇒ 用户 2026-11-20 追认）⇒ ⇒
端口数 14 → 13 · 根自有 `__io` 12 → 11。

### 2. 冻结的产物 module 期望集（**手推** · 禁从成层声明处派生）

| # | module 名 | 来源 |
|---|---|---|
| 1 | `AxiLiteTestMaster` | 档③ 模块类型名逐字 |
| 2 | `AxiLiteMatrix2x2_32_32_` | `"AxiLiteMatrix2x2<32,32>"` 经 `sanitize_verilog_name`（非字母数字 ⇒ `_`） |
| 3 | `AxiLiteGpio_8_` | `"AxiLiteGpio<8>"` 同上 |
| 4 | `AxiLiteUart_16_` | `"AxiLiteUart<16>"` 同上 |
| 5 | `top` | 根 module 名（发射入口配置，默认 `top`） |

⚠️ **MUST NOT** 从 `ch_layer` 调用点派生期望集 ⇒ ⇒ 「漏给某个 IP 加成层声明」会**同时**改变
期望 ⇒ ⇒ 判据**恒绿**（metis 的原话；故本表手推并冻结）。

### 3. 地址映射表（spec-derived）

**matrix 的默认 2×2 窗口**（`include/axi4/interconnect_common.h:74-79` 的
`AxiLiteDefaultMatrixMap`）：

| 窗口 | base | size | 从设备口 | 挂在 |
|---|---|---|---|---|
| 0 | `0x0000'0000` | `0x8000'0000` | `s0` | **GPIO** |
| 1 | `0x8000'0000` | `0x8000'0000` | `s1` | **UART** |

⇒ ⇒ `addr[31] == 0` ⇒ GPIO；`addr[31] == 1` ⇒ UART。

**寄存器映射**（`include/axi4/peripherals/axi_gpio.h:7-10` · `axi_uart.h:8-15`）：

| 设备 | 偏移 | 寄存器 | 读/写 | 备注 |
|---|---|---|---|---|
| GPIO | `0x00` | `DATA_OUT` | W / R | 写 ⇒ `gpio_out` = 低 8 位 |
| GPIO | `0x04` | `DATA_IN` | **只读** | 写它 ⇒ DECERR（且**不可**作为场景） |
| GPIO | `0x08` | `DIRECTION` | W / R | 1=输出 |
| GPIO | `0x0C` | `INTERRUPT` | W / R | 状态 |
| UART | `0x00` | `TX_DATA` | W / R | 写 ⇒ **启动一帧发送**（仅低 8 位有效） |
| UART | `0x04` | `RX_DATA` | **只读** | 读走即清 `rx_valid` |
| UART | `0x08` | `STATUS` | **只读** | |
| UART | `0x0C` | `CTRL` | **只读** | 保留 |

### 4. 事务序列 + 手推结果（4 个场景）

⚠️ **完成判定 MUST 用「按已知延迟采样」**（**禁**等 `done` 上升沿 —— 它**粘滞**：
`examples/axi4/axi_soc_example_top.h:142-144` 的 `done_reg` **无清零分支** ⇒ 首笔后恒 1）。

#### 手推：事务延迟（impl-contract）

| 事务 | 推导 | 完成拍（从拉高 `start` 起） |
|---|---|---|
| **写** | 拍0 组合出 `awvalid`/`wvalid`；拍1 沿 master 见 `awready`（组合）⇒ `aw_done=1` ⇒ gpio 拍1 锁地址；gpio 的 `wready = wvalid & aw_done` ⇒ **拍2** 组合成立 ⇒ 拍2 沿收 `wdata` ⇒ 拍3 更新 `data_out_reg` ⇒ 拍3 `b_valid=1` ⇒ 拍4 沿 master 见 `bvalid` | **≤ 4** |
| **读** | 拍0 组合出 `arvalid`；拍1 沿 master 见 `arready` ⇒ `ar_done=1` ⇒ gpio 拍1 锁地址 ⇒ 拍2 `r_valid=1` ⇒ 拍3 沿 master 捕获 `rdata` | **≤ 4** |

⇒ ⇒ TB 的等待余量取 **16 拍**（= 4 × 上界）⇒ ⇒ 不会采到未完成的值。

#### 场景 ①：写 GPIO `DATA_OUT`

| 项 | 值 |
|---|---|
| 事务 | `write=1` · `addr=0x0000_0000` · `wdata=0x0000_00A5` |
| 等待 | 16 拍 |
| **期望** | `gpio_out == 8'hA5`（`data_out_reg` 是 32 位 ⇒ `gpio_out` 取**低 8 位**） |

#### 场景 ②：读 GPIO `DATA_IN`

| 项 | 值 |
|---|---|
| 事务 | `write=0` · `addr=0x0000_0004`（DATA_IN） |
| 等待 | 16 拍 |
| **期望** | `rdata == 32'h0000_005A`（`gpio_in` = 8'h5A ⇒ **零扩展**到 32 位：`axi_gpio.h:164` 的 `ch_uint<32>(ch_uint<NUM_GPIOS>(io().gpio_in))`） |

#### 场景 ③：写 UART `TX_DATA` ⇒ 观察 8N1 帧

| 项 | 值 |
|---|---|
| 事务 | `write=1` · `addr=0x8000_0000`（UART 窗口 + `TX_DATA`）· `wdata=0x0000_005A` |
| 帧格式（spec-derived） | 起始位(0) + **8 数据位（LSB 先出）** + 停止位(1)；**bit 周期 = 16 拍**（`UART_BIT_PERIOD`）⇒ 帧长 **160 拍** |
| **手推数据位** | `0x5A` = `0101'1010` ⇒ LSB 先出 = **`0 1 0 1 1 0 1 0`** |
| **期望** | 下降沿（起始位）后：起始位 == 0 · 8 个数据位 == 上表序列 · 停止位 == 1 |
| 等待 | 写事务后 **≥ 200 拍**（16 + 160 + 余量）⇒ ⇒ TB MUST **持续监视** `uart_tx`（**禁**「等完再采样」—— 帧已过） |
| 定位 | 以 `uart_tx` 的**高→低下降沿**为帧起点（`axi_uart.h:220` 的空闲高 + 起始位 0）⇒ 起始位中点 = 沿 + 8；数据位 k 中点 = 沿 + 8 + 16×(k+1)；停止位中点 = 沿 + 8 + 16×9 |

⚠️ **这个场景同时证明 `matrix.s1_* ↔ uart` 的 19 根接线是通的** ⇒ ⇒ 写 `TX_DATA` 必须经
matrix 的 `m0 → s1` 路由 ⇒ ⇒ uart 才收得到。

#### 场景 ④：`rst_n` 拉低 ⇒ GPIO 输出寄存器清零

| 项 | 值 |
|---|---|
| 动作 | `rst_n = 0` ⇒ 透传给 gpio/matrix/uart（`axi_soc_example_top.h:293-300`） |
| 等待 | 2 拍（`data_out_reg->next = select(!rst_n, 0, ...)` ⇒ **下一个沿**即清零） |
| **期望** | `gpio_out == 8'h00`（且 `uart_tx` 回**空闲高**） |

⚠️ **本场景 MUST 放最后**（`rst_n` 拉低后整个 SoC 复位 ⇒ 后续事务不可用）。

### 5. TB 的输入常量（impl-contract）

| 信号 | 值 | 理由 |
|---|---|---|
| `gpio_in` | `8'h5A` | 场景 ② 的被读值 ⇒ 非零才能证明读通道真的取到数据 |
| `uart_rx` | `1'b1` | **空闲高**（`axi_uart.h:220`：未发送时 `uart_tx` 恒高 ⇒ 接收侧同理）⇒ 拉低会被当成起始位 |
| `rst_n` | `1'b1`（初始）→ 场景④ 拉 0 | 低有效 |
| `default_reset` | `1'b1` → **negedge** 释放 | 通用前提 |

### 5b. 验证源登记（`.verif` · **机器可读边**）

验证源 = `examples/axi4/tb_axi_soc_example.verif`（ADR-074 D1 · R13 闭口的唯一语义载体）。

VERIF-CHECKS: 15

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读的第一边**（由**人**登记 ⇒ ⇒ **禁**由生成器输出
回填 ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ 判据恒绿）。组成：**1**（场景① 写 GPIO）
+ **1**（场景② 读 GPIO）+ **11**（场景③：`WAIT` 帧起点 1 + 起始位 1 + 数据位 8 + 停止位 1）
+ **2**（场景④ 复位 2 条）= **15**。

⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**（粒度 = scenario）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `bringup_and_gpio_write` | §4 场景① + §5 输入常量（`gpio_in` / `uart_rx`） |
| `gpio_read_data_in` | §4 场景② + §3 地址映射（`0x04` = GPIO `DATA_IN`） |
| `uart_tx_frame_8n1` | §4 场景③（帧格式 + 位中点 = 帧起点 + 8 + 16×k） |
| `low_active_reset_clears_gpio` | §4 场景④（`rst_n` 拉低 ⇒ GPIO 清零 + `uart_tx` 空闲高） |

#### scenario: bringup_and_gpio_write

写 GPIO `DATA_OUT`（`addr 0x00`）= `0xA5` ⇒ `gpio_out == 0xA5`（§4 场景①）。

#### scenario: gpio_read_data_in

读 GPIO `DATA_IN`（`addr 0x04`）⇒ `rdata == 0x0000_005A`（= `gpio_in` 的 0x5A · §4 场景②）。

#### scenario: uart_tx_frame_8n1

写 UART `TX_DATA`（`addr 0x8000_0000`）= `0x5A` ⇒ 8N1 帧：**帧起点由 `WAIT uart_tx low`
锚定**（ADR-074 §7.3 D3′ 受裁用法），随后起始位 / 8 数据位（LSB 先出 `0 1 0 1 1 0 1 0`）/
停止位的中点 = 帧起点 + 8 + 16×k（§4 场景③）。

#### scenario: low_active_reset_clears_gpio

`rst_n` 拉低 ⇒ `gpio_out == 0` 且 `uart_tx == 1`（空闲高）（§4 场景④）。

### 6. 判别力自证（负向对照）

| 变异 | 预期转红 |
|---|---|
| 把 `uart` 的 `ch_layer` 改回 `ch_module` | 场景③ 的帧**不出现**（uart 不在矩阵的 `s1` 路由上）⇒ 帧判据红 |
| 拔掉 `matrix.m1_*` 的 11 根 tie-off 之一 | 发射 **fail-closed**（后代悬空 ⇒ 拒绝）⇒ 产物根本发不出来 |
| 把 `wdata` 的期望值改错 | 场景① 的 `gpio_out` 判据红 |
| 把 `gpio_in` 改成 0 | 场景② 的 `rdata` 判据红（`rdata` 会是 0 而非 0x5A） |

### 6b. **双形态等价的判据规格**（T7 · ADR-073 J-4 / V5）

> **载体** = `tb_axi_soc_example --check-products` 的 **CHECK 4**；**执行者** = 护栏 ④″。

| 项 | 值 / 形态 |
|---|---|
| 等价方向 | **A 侧固定是「扁平产物」**（`ch::hier_eq::compare` 的语义）⇒ ⇒ 本判据 = 「**扁平 ≡ 层次化**」（层次化侧按 `RespectDeclarations` ⇒ 4 个 IP 成层） |
| `ports` | **MUST 由 `ch::hier_eq::emit` 派生**（禁手写 ⇒ 否则与 emitter 真相脱钩） |
| 观测集 | {`rdata`, `gpio_out`, `uart_tx`} —— ⚠️ **不含 `done`**（粘滞 ⇒ 恒 1 ⇒ 计入只会让变化判据恒绿） |
| 变化量 | **≥2 个**随激励变化的观测量（`uart_tx` 无帧时恒 1 ⇒ **不**贡献变化量 ⇒ 实际由 `rdata` + `gpio_out` 承担） |
| 激励 | **14 拍**（≥10）· 每拍 6 个值：`rst_n` · `start` · `write` · `addr` · `wdata` · `gpio_in` |
| 事务 | ① 拍0 写 GPIO DATA_OUT（`addr=0x0` · `wdata=0xA5`）⇒ `gpio_out` 变 ② 拍5 读 GPIO DATA_IN（`addr=0x4`）⇒ `rdata` 变 ③ 拍9 改 `gpio_in` → 拍10 再读 ⇒ `rdata` 再变 |

⚠️⚠️ **AXI payload MUST 在事务期间保持稳定**（**实测踩到** · 判据级的硬约束）：

- master 的 `awaddr` / `wdata_axi` 是**组合**跟随顶层 `addr` / `wdata` 的
  ⇒ ⇒ 激励若在下一行把 `wdata` 清零 ⇒ ⇒ **W 通道送出去的是 0 而不是 0xA5**
  ⇒ ⇒ `gpio_out` 恒 0，**且不报任何错**（等价检查仍会「成立」⇒ ⇒ **恒绿陷阱**）。
- ⇒ ⇒ 事务期间 `addr` / `wdata` **MUST 原样保持**（这正是仓内 `tb_axi4_lite_example.sv`
  的 `send_aw` / `send_w` 分开送、且 `valid && !ready` 期间 payload 不变 的同型规则）。
- ⇒ ⇒ **等价检查 MUST 自带变异自证**（否则「等价成立」可能只是「两边都没动」）。

⚠️ **变异自证的形态（ADR-073 V5）**：篡改**层次化产物**里**驱动某个被观测端口**的那条
`assign`（取反其 RHS）⇒ ⇒ MUST 判不等（`first_mismatch` 给出 cycle 与观测名）。
- ⚠️ **MUST NOT** 直接取反**顶层端口**：顶层端口名出现在**实例端口连接**的右侧
  （`.child_port(top_port)`）⇒ ⇒ 取反它 = **组合环** ⇒ ⇒ Verilator 直接报错
  ⇒ ⇒ 那是「编译失败」而不是「行为差异」⇒ ⇒ 判别力更弱。
- ⇒ ⇒ 故取反点在**子 module 内**（`assign child_port = ~X;`）⇒ ⇒ **真行为差异**。
  实测：`first_mismatch=cycle 0, observation 'top.rdata': flat=0x0 hier=0xFFFFFFFF`。

### 7. 已知盲区（如实登记）

- ⚠️ **`done` 粘滞** ⇒ TB **不能**用它判完成 ⇒ 所有场景用固定延迟采样 ⇒ ⇒
  「事务是否真的完成」只能由**数据值**间接证明（**不是**由 `done` 证明）。
- ⚠️ **`uart_rx` 未被激励** ⇒ UART 的**接收**方向（`s1 → matrix` 的 8 根反向线）在本 TB 中
  **不被行为覆盖** ⇒ ⇒ **只覆盖**「matrix → uart」的 11 根正向线（场景③）。
  ⇒ ⇒ 那 8 根反向线的覆盖 **MUST 登记为盲区**（ADR-073 §4.2 代价④ 的同型）。
- ⚠️ **`bresp`/`rresp` 不可观测**（顶层无这两个端口）⇒ ⇒ **DECERR / OKAY 在 SoC 层不可观测**
  （与 ADR-073 v1.1 订正⑥ 同源）⇒ ⇒ 协议面归 IP 级 TB（`tb_axi4_lite_example.sv`）。
- ⚠️ **`timer_irq` 是零消费死端口** ⇒ ⇒ 它在端口面里但**没有语义** ⇒ **MUST NOT** 假装它可观测。

---

## riscv_soc {#riscv-soc}

> **产品**：`examples/riscv-soc/` 的 `Rv32iSocTop`（SoC 级顶层 · ADR-077 D1）
> **TB**：`tb_riscv_soc.cpp`（默认模式 · 解释器路径）+ `tb_riscv_soc.sv`（独立 Verilog · `generated=no`）
> **上游登记**：`.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc` —— **程序与写回/存储手推台账同源**
>
> ⚠️ **两侧判据 MUST 逐条同值**（AC7 的 `interp_checks == min_checks == L`）：本段 **7** 条 · C++ 侧 7 · `.sv` 侧 7。

### 1. 产物形态（手推 · 禁从 `ch_layer` 调用点派生）

| 项 | 期望 | 推导 |
|---|---|---|
| module 集合 | `top` · `Rv32iCoreTop` · `SocTraceCounter` · `IfStage` · `IdStage` · `ExStage` · `MemStage` · `WbStage` · `HazardUnit` · `ForwardingMux_32_` = **10** | 根 1 + 2 个 `ch_layer` + 核复合体**内部**的 7 个 `ch_layer`（`rv32i_pipeline.h:140-153`）⇒ ⇒ 子层**不折叠** |
| 顶层端口 | **8** = 框架 2 + 根自有 6（`ext_rst` / `prog_we` / `prog_addr` / `prog_data` / `store_count` / `writeback_count`） | ADR-077 V1 · ADR-072 D1（后代 `ch_out` 不冒泡） |
| `Rv32iCoreTop` module 端口 | **34** = 框架 2 + 核自有 32（4 in + 28 out） | 核自有 32 = `Rv32iPipelineTop` 的 `__io`（**已由 `verify_no_duplicate_rv32i_headers.sh` 的 C6 冻结**）· +2 = 核内 **inline** 的 TCM `sread` 寄存器落在该 module ⇒ 需时钟/复位 |

### 2. 程序（与核级同源 · 编码**手推**）

| word | 指令 | 编码 | 手推结果 |
|---|---|---|---|
| 0 | `addi x1, x0, 42` | `0x02A00093` | 写回 x1 = 42 |
| 1 | `nop` | `0x00000013` | — |
| 2 | `addi x2, x0, 11` | `0x00B00113` | 写回 x2 = 11 |
| 3 | `nop` | `0x00000013` | — |
| 4 | `add x3, x1, x2` | `0x002081B3` | 写回 x3 = 42 + 11 = **53** |
| 5 | `sw x3, 0(x0)` | `0x00302023` | store 字节地址 0 · 数据 53 |
| 6 | `jal x0, 0` | `0x0000006F` | 自环；`rd = x0` ⇒ **不写回** |

⇒ ⇒ **写回事件 3 次**（word 0 / 2 / 4）· **store 事件 1 次**（word 5）。

### 3. 判据（7 条 · 逐条手推）

| # | 判据 | 期望 | 级别 | 推导 |
|---|---|---|---|---|
| A1a | 复位后 `store_count` | **0** | impl-contract | 计数器复位清零 |
| A1b | 复位后 `writeback_count` | **0** | impl-contract | 同上 |
| A2 | 装载期（`ext_rst = 1`）两计数保持 0 | **0 / 0** | impl-contract | `ext_rst` 是**核**复位 ⇒ 装载动作不进流水线 ⇒ 无写回/store；**接反则装载被当指令执行 ⇒ 必红** |
| A3 | 释放后 `store_count` | **1** | **spec-derived** | ISA 语义：程序里恰有 1 条 `sw` |
| A4 | 释放后 `writeback_count` | **3** | **spec-derived** | ISA 语义：x1 / x2 / x3 各写回一次；`jal x0,0` 的 `rd = x0` 不写回 |
| A5 | 两计数单调不减 | **真** | impl-contract | 计数语义 |
| A6 | 尾段（第 40 拍后）不再增长 | **真** | **spec-derived** | `jal x0, 0` 自环 ⇒ 无新的写回/store |

⇒ **spec-derived = 3**（A3 / A4 / A6）· impl-contract = 4。

### 4. 变异自证（判别力 · 三条全部实测转红）

| # | 变异 | 期望 | 实测 |
|---|---|---|---|
| M1 | 把 `writeback_count` 移出 `make_observations` | CHECK 4 红（变化量 1 < 2） | ✅ `CHECK 4 ... FAIL (变化量=1/1)` |
| M2 | 两条兄弟边**对调**（`store_event` ↔ `retire_event`） | A3 + A4 双红 | ✅ `fails=2`（store=3 · wb=1） |
| M3 | 两条边接**同一源** | A4 红 | ✅ `fails=1`（wb=1 ≠ 3） |

⇒ ⚠️ **M2 / M3 能被抓住的前提 = 两个手推值不同（1 vs 3）**。若两者同值（例如用「1 次写回」的短程序），
两条边接反时观测值不变 ⇒ ⇒ **恒绿**。⇒ ⇒ 程序 MUST 保持「写回 3 次 · store 1 次」这一比例。

### 5. 已知盲区（如实登记）

- ⚠️ **SoC 级看不到核的任何内部信号**（`dbg_*` / `obs_*` 不是顶层端口 · ADR-072 D1）⇒ ⇒
  核级 24 条判据**只能**留在 `examples/riscv-mini` ⇒ ⇒ 本产品只有 **2 个**观测量。
- ⚠️ **观测盲区**（ADR-073 要求显式登记）：层次化只覆盖**顶层可见行为** ⇒ ⇒
  `Rv32iCoreTop` / `SocTraceCounter` 的**内部** `ch_out` 在顶层不可观测 ⇒ ⇒
  「子树内部算错但顶层输出恰好相同」这类缺陷**结构性测不出**。
- ⚠️ **`prog_*` 是运行期后门**（ADR-077 D3 长期保留）⇒ ⇒ 真实芯片形态的装载路径未定义。
