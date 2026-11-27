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
> ⚠️ **两侧判据 MUST 逐条同值**（AC7 的 `interp_checks == min_checks == L`）：本段 **16** 条 · C++ 侧 16 · `.sv` 侧 16。
> ⚠️ **2026-11-24 · S4a（`rv32i-soc-mmio-channel-2026-11`）**：程序 **7 → 12 字**（含 2 次 MMIO 访问）·
>   判据 **7 → 9**（+A7 MMIO 写通路的值 · +A8 DECERR 不挂死）· 程序**与核级分叉**（核级 AXI-Lite 输入全 tie 0）。
> ⚠️ **2026-11-24 · critic BLOCKER 后追加 A9（判据 9 → 10）**：读数据 mux 的选择线**错拍**
>   （机理与闭合证据见 §4 的「A9 的『先红后绿』」）⇒ ⇒ 原 9 条判据对该缺陷**结构性失明**
>   （读值不在 SoC 根端口上 · `gpio_in` 又被 tie 0 ⇒ ⇒ 错源与正源**碰巧同值**）。
> ⚠️ **2026-11-27 · S4b P4（判据 10 → 16）**：新增 **A10–A14** = s1–s5 五个从口的
>   **总线级回环**（写外设寄存器 → 读回 → 写 GPIO DATA_OUT ⇒ 判 `gpio_out`）。S4a 只验证过
>   **s0**（GPIO）⇒ ⇒ 其余 5 个从口在本批之前**从未被实际访问**（crossbar 的路由正确性由
>   P1 单测 `test_axi_lite_crossbar` 承担；本段判的是 **SoC 侧装配**）。
> ⚠️ **2026-11-27 · P4 实测发现的判据缺口 ⇒ 追加 A15（判据 15 → 16）**：A10–A14 全是
>   「写 → 读回」⇒ ⇒ 若某窗口**单点错位**到「同偏移也可写」的外设，**写和读都落到同一个
>   错误外设** ⇒ 读回值一致 ⇒ 该条仍绿。**实测**：把 PWM 窗口改成指向 GPIO 从口
>   ⇒ 写 `PERIOD` 实际写 `DATA_OUT` ⇒ **15 条全绿**。⇒ ⇒ A15 用
>   **PWM `PERIOD` 的复位值 0xFF**（其他外设 word 0 复位值全是 0）作**地址特征**判据
>   ⇒ 任何 PWM 窗口错位都得到 ≠ 0xFF ⇒ 必红。

### 1. 产物形态（手推 · 禁从 `ch_layer` 调用点派生）

| 项 | 期望 | 推导 |
|---|---|---|
| module 集合 | `top` · `Rv32iCoreTop` · `SocTraceCounter` · `IfStage` · `IdStage` · `ExStage` · `MemStage` · `WbStage` · `HazardUnit` · `ForwardingMux_32_` · `AxiLiteCrossbar` · `AxiLiteGpio` · `AxiLiteUart` · `AxiLiteTimer` · `AxiLiteSpi` · `AxiLiteI2c` · `AxiLitePwm` = **17** | 根 1 + 核复合体**内部**的 7 个 `ch_layer`（`rv32i_pipeline.h:140-153`）⇒ ⇒ 子层**不折叠**；**SoC 级 8 个 `ch_layer`**（`Rv32iCoreTop` · `SocTraceCounter` · **2×6 Lite 互联** + **6 个 MMIO 外设**）⇒ ⇒ ⚠️ **S4b：12 − 1（矩阵换成互联）+ 1（互联）+ 5（新外设）= 17**（不是「12 + 6」—— 那把「替换」当成了「新增」）|
| 顶层端口 | **10** = 框架 2 + 根自有 8（`ext_rst` / `prog_we` / `prog_addr` / `prog_data` / `store_count` / `writeback_count` / **`gpio_out`** / **`gpio_in`**） | ADR-077 V1 + **S4a dated addendum** · ADR-072 D1（后代 `ch_out` 不冒泡） |
| `Rv32iCoreTop` module 端口 | **53** = 框架 2 + 核自有 51（**12 in + 39 out** ⇒ S4a 加 19 根 `axil_*`） | 核自有 32 = `Rv32iPipelineTop` 的 `__io`（**已由 `verify_no_duplicate_rv32i_headers.sh` 的 C6 冻结**）· +2 = 核内 **inline** 的 TCM `sread` 寄存器落在该 module ⇒ 需时钟/复位；端口表单一真相 = `examples/riscv-mini/AGENTS.md`（护栏 V1 逐字逐序）|

### 2. 程序（与核级同源 · 编码**手推**）

| word | 指令 | 编码 | 手推结果 |
|---|---|---|---|
| 0 | `addi x1, x0, 42` | `0x02A00093` | 写回 x1 = 42 |
| 1 | `nop` | `0x00000013` | — |
| 2 | `addi x2, x0, 11` | `0x00B00113` | 写回 x2 = 11 |
| 3 | `nop` | `0x00000013` | — |
| 4 | `add x3, x1, x2` | `0x002081B3` | 写回 x3 = 42 + 11 = **53** |
| 5 | `sw x3, 0(x0)` | `0x00302023` | store 字节地址 **0** ⇒ **走 D-L1** |
| 6 | `nop` | `0x00000013` | — |
| 7 | `lui x4, 0x80102` | `0x80102237` | 写回 x4 = **0x8010_2000** = `kGpio.base` |
| 8 | `lw x6, 4(x4)` | `0x00422303` | **MMIO 读** @ `0x8010_2004`（DATA_IN）⇒ 写回 x6 |
| 9 | `addi x5, x0, 0xA5` | `0x0A500293` | 写回 x5 = **0xA5** |
| 10 | `sw x5, 0(x4)` | `0x00522023` | **MMIO 写** @ `0x8010_2000`（DATA_OUT）· 数据 0xA5 |
| 11 | `jal x0, 0` | `0x0000006F` | 自环；`rd = x0` ⇒ **不写回** |

⇒ ⇒ **写回事件 6 次**（x1 / x2 / x3 / x4 / x6 / x5）· **store 事件 2 次**（word 5 走 D-L1 · word 10 走 MMIO）
⇒ ⇒ **`gpio_out == 0xA5`**（word 10 的值落到 GPIO 的 DATA_OUT）
⇒ ⇒ ⚠️ **口径 = 事件次数**（与事务时长无关）⇒ ⇒ MMIO 的多拍停顿**不计入**（依赖 S4a-0 的脉冲化）

**A8 的独立 4 字程序**（DECERR 场景 · 不污染 A1–A7 的计数 ⇒ 先 `default_reset`/`rst` 清计数再装）：

| word | 指令 | 编码 | 手推结果 |
|---|---|---|---|
| 0 | `lui x1, 0x80100` | `0x801000B7` | 写回 x1 = **0x8010_0000** = `kClint.base`（S4a **未接 CLINT** ⇒ 未映射） |
| 1 | `lw x2, 0(x1)` | `0x0000A103` | **MMIO 读未映射窗口** ⇒ MUST 得到 DECERR（而非挂死） |
| 2 | `addi x3, x0, 7` | `0x00700193` | 写回 x3 = 7（若上一条挂死 ⇒ **永不写回**） |
| 3 | `jal x0, 0` | `0x0000006F` | 自环 |

⇒ ⇒ **写回事件 3 次**（x1 / x2 / x3）⇒ ⇒ DECERR 未返回 ⇒ 冻结在 **1** ⇒ 必红。

### 3. 判据（16 条 · 逐条手推）

| # | 判据 | 期望 | 级别 | 推导 |
|---|---|---|---|---|
| A1a | 复位后 `store_count` | **0** | impl-contract | 计数器复位清零 |
| A1b | 复位后 `writeback_count` | **0** | impl-contract | 同上 |
| A2 | 装载期（`ext_rst = 1`）两计数保持 0 | **0 / 0** | impl-contract | `ext_rst` 是**核**复位 ⇒ 装载动作不进流水线 ⇒ 无写回/store；**接反则装载被当指令执行 ⇒ 必红** |
| A3 | 释放后 `store_count` | **2** | **spec-derived** | ISA 语义：程序里恰有 2 条 `sw`（word 5 D-L1 · word 10 MMIO）⇒ **两条路径各一次** |
| A4 | 释放后 `writeback_count` | **6** | **spec-derived** | ISA 语义：x1 / x2 / x3 / x4 / x6 / x5 各写回一次；`jal x0,0` 的 `rd = x0` 不写回 |
| A5 | 两计数单调不减 | **真** | impl-contract | 计数语义 |
| A6 | 尾段（第 85 拍后）不再增长 | **真** | **spec-derived** | `jal x0, 0` 自环 ⇒ 无新的写回/store |
| A7 | `gpio_out` | **0xA5** | **spec-derived** | ISA 语义 + GPIO 寄存器表：word 10 把 x5 = 0xA5 写进 `DATA_OUT`（reg0）⇒ ⇒ 只判计数**判不出**「写错段 / 数据错位」 |
| A8 | DECERR 场景的 `writeback_count` | **3** | impl-contract | ISA 语义：独立 4 字程序有 3 条写回；**DECERR 未返回 ⇒ `lw` 永久停顿 ⇒ 冻结在 1** |
| A9 | 读值回路的 `gpio_out` | **0x5A** | **spec-derived** | ISA 语义：`lw` 读 DATA_IN（`gpio_in` = 0x5A）→ 一条**非 MMIO** 指令 → `sw` 回 DATA_OUT ⇒ ⇒ 读值 MUST 走 MMIO 通路；若 mux 用**后继指令**的地址选路径 ⇒ ⇒ 读到 `dcache.rdata`（本场景无 D-L1 读 ⇒ 0）⇒ 0 ≠ 0x5A ⇒ 必红 |
| A10 | UART 回环的 `gpio_out` | **0x53** | **spec-derived** | ISA 语义 + UART 寄存器表：`sw` 写 `CTRL`（字节 `0x0C` = word 3）→ `lw` 读回 → `sw` 写 GPIO `DATA_OUT`。**值 ≠ 复位值 0x00** ⇒ 「写没生效」必红 |
| A11 | TIMER 回环的 `gpio_out` | **0x6A** | **spec-derived** | 同上：`LOAD`（`0x00` = word 0）· 复位值 0x00 ⇒ 写 0x6A 可判别 |
| A12 | SPI 回环的 `gpio_out` | **0x17** | **spec-derived** | 同上：`BAUD`（`0x10` = word 4）· ⚠️ **复位值 0x64（100）** ⇒ 写 0x17 与复位值不同（否则「写没生效」会撞值） |
| A13 | I2C 回环的 `gpio_out` | **0x2C** | **spec-derived** | 同上：`PRESCALE`（`0x10` = word 4）· ⚠️ **复位值 0x64（100）** ⇒ 同上 |
| A14 | PWM 回环的 `gpio_out` | **0xE1** | **spec-derived** | 同上：`PERIOD`（`0x00` = word 0）· ⚠️ **复位值 0xFF（255）** ⇒ 同上 |
| A15 | **PWM `PERIOD` 的复位值**（不写） | **0xFF** | **spec-derived** | PWM 寄存器表：`period_reg` 复位 **255**，而其他外设 word 0 的复位值**全是 0**（`TX_DATA` / `LOAD` / `TX_DATA` / `CTRL` / `DATA_OUT`）⇒ ⇒ 「不写直接读」即可判定**地址真的落在 PWM**。⚠️ **这是「窗口错位」唯一的判别面** —— A10–A14 对该形态**结构性失明**（实测 15 条全绿） |

⇒ **spec-derived = 11**（A3 / A4 / A6 / A7 / A9 / A10–A15）· impl-contract = 5（A1a / A1b / A2 / A5 / A8）。
⇒ ⚠️ **A8 归 impl-contract 的口径**：`writeback_count == 3` 本身由 ISA 语义推出，但「**DECERR 会让事务完成**」是**互联/总线**语义（不是 RV32I ISA 的一部分）⇒ ⇒ 该判据的可推导性依赖本实现的 error responder ⇒ ⇒ 保守归 impl-contract（与 tsv 的 `spec_checks=5` 同一口径 · critic 轮次 2 的 NIT）。

### 4. 变异自证（判别力 · **六条全部实测** · 2026-11-24）

| # | 变异 | 靶子 | 期望 | 实测 |
|---|---|---|---|---|
| M1 | **分流判据取反**（`addr_in_mmio` 加 `!`） | 核级 AC4 + SoC A7 | 核级挂死/错值 · SoC `gpio_out` 不再被写 | ✅ 核级 **fails=3**（S4 写回 x3 / S5 store@0）· SoC **fails=1**（A7） |
| M2 | **MMIO 走 D-L1**（master `preq` 拉 0 + `dcache.req` 全收） | AC2/AC3/AC7/AC8 | MMIO 读挂死（Lite 无 `r_last` ⇒ fill 永不完成） | ✅ SoC **fails=4**（A3/A4/A7/A8） |
| M3 | **两条兄弟边对调**（`store_event` ↔ `retire_event`） | AC3 | 计数互换（store=6 · wb=2） | ✅ SoC **fails=3** |
| M4 | **两条边接同一源** | AC3 | 两计数同源 ⇒ wb=2 ≠ 6 | ✅ SoC **fails=2** |
| M5 | **`windows[1]` 改低位 `{0,4KB}`**（**与 M1 组合**） | AC3 | 地址 0 被路由到未接的 s1 ⇒ 挂死 | ✅ SoC **fails=3**（A3/A4/A7） |
| M6 | **`gpio_in` 未驱动** | AC6 | 发射 **fail-closed** | ✅ `层次化发射失败：1 undriven descendant input(s) are rejected`（`top.gpio` 的 `unnamed_input_3`）· exit 1 |

⇒ ⚠️ **M5 单独无效**（恒绿）：分流正确时地址 0 根本不进 matrix ⇒ ⇒ 低位窗口**今天**观察不到差异
⇒ ⇒ 它防的是**将来**某次改动（例如分流判据被放宽）⇒ ⇒ 故 MUST 与 M1 组合才有判别力（design §3.1 的原话）。
⇒ ⚠️ **M1 的 SoC 侧只有 A7 转红**：计数仍到 (2,6)（两条路径都还在响应 `data_ready`）
⇒ ⇒ **这正是 A7 存在的理由** —— 只判计数**判不出**「写错段 / 数据没落到外设」。
⇒ ⚠️ **M2 / M3 / M4 能被抓住的前提 = 两个手推值不同（2 vs 6）**；若两者同值 ⇒ 接反/同源时观测值不变 ⇒ **恒绿**。
⇒ ⚠️ 每条变异均**先确认编译通过**（0 error）再跑；**还原源码后 MUST 重建**（二进制也是状态）。

#### A9 的「先红后绿」（critic BLOCKER 的闭合证据 · 2026-11-24）

| 阶段 | 现象 | 结论 |
|---|---|---|
| ① 先加 A9、**不改实现** | `RESULT: checks=10 fails=1`（只有 A9 红） | 缺陷**实锤**：9 条旧判据全绿，而读值是错的 |
| ② 修实现（选择线改为与 M2 元数据**同门控锁存**的 `rd_path_mmio`） | `checks=10 fails=0` · 核级仍 `checks=24 fails=0` | 闭合 |

⇒ **缺陷机理**：`data_read_data` 在 **M2 级组合消费**（`rv32i_pipeline.h:345` 的 `load_lane.raw_word`），
而消费那一拍的 `data_addr` 已是**后继**指令在 EX/MEM 的地址 ⇒ ⇒ 用当拍 `addr_in_mmio` 选路径 = **选错源**；
两条路径各有**保持寄存器** ⇒ ⇒ 错源不是随机值，而是**上一条走该路径的指令的旧值** ⇒ ⇒ 波形上也难辨认。
⇒ **核级 24 判据零变化与该缺陷完全相容**（核级程序全程 ∉ MMIO ⇒ 永远选 D-L1）⇒ ⇒ 旧判据全绿**不构成反证**。

#### 无效变异（如实登记 · 2026-11-24）

| # | 变异 | 现象 | 判定 |
|---|---|---|---|
| M7 | `rd_path_mmio->next` 的 `select` 两分支**对调**（stall 时捕获 · 前进时保持） | A9 **仍绿**（`checks=10 fails=0`） | ⚠️ **无效变异**（不改变本场景可观测行为）：单次 MMIO load 在**停顿期**地址仍是它自己 ⇒ ⇒ 两种写法都捕获到正确的 tag。
⇒ ⇒ 该 mux 的**专属判别力**由 A9 的「先红」证明（修复前用**当拍**地址 ⇒ `fails=1`），**不是**由 M7 证明。
⇒ ⇒ **已知边界**：M7 与正确写法只在「MMIO load 完成后的**下一条**指令也在 MEM 停顿」时才有差异；本 change 的程序没有这种形态（不新增该判据）⇒ ⇒ 如实登记为覆盖不足。

#### P4 的变异自证（2026-11-27 · **三条全部实测**）

| # | 变异 | 靶子 | 期望 | 实测 |
|---|---|---|---|---|
| M-P4-1 | PWM 窗口 `windows[5].target` **5 → 0**（+ 同步改 `static_assert`，否则编译失败） | A14 / A15 | A14 **仍绿**（写读同落 `DATA_OUT`）· A15 **必红** | ✅ `checks=16 fails=1` —— **只有 A15 红** |
| M-P4-2 | UART 窗口 `windows[1].target` **1 → 2**（指向 TIMER） | A10 | TIMER word 3 = `STATUS` **只读** ⇒ 写 DECERR + 读回 0 ≠ 0x53 | ✅ `checks=16 fails=1`（A10 红） |
| M-P4-3 | SPI 窗口 `windows[3].target` **3 → `kErr`**（从口未接） | A12 | 写/读都 DECERR ⇒ 读回 0 ≠ 0x17 | ✅ `checks=16 fails=1`（A12 红） |

⇒ ⚠️ **M-P4-1 是 A15 存在的理由**：同一变异下 **A10–A14 全绿**（写和读都落到同一个错误外设
⇒ 读回值一致）⇒ ⇒ 只有 A15 转红 ⇒ ⇒ 证明「写 → 读回」形态对「窗口错位到**同偏移也可写**
的外设」**结构性失明**，必须由**地址特征**判据（PWM 复位值 0xFF）补上。
⇒ ⚠️ 每条变异均**先确认编译通过**（0 error）再跑；**还原源码后 MUST 重建**（二进制也是状态）；
还原后复测 `checks=16 fails=0` ✅

### 5. 已知盲区（如实登记）

- ⚠️ **SoC 级看不到核的任何内部信号**（`dbg_*` / `obs_*` 不是顶层端口 · ADR-072 D1）⇒ ⇒
  核级 24 条判据**只能**留在 `examples/riscv-mini` ⇒ ⇒ 本产品只有 **2 个**观测量。
- ⚠️ **观测盲区**（ADR-073 要求显式登记）：层次化只覆盖**顶层可见行为** ⇒ ⇒
  `Rv32iCoreTop` / `SocTraceCounter` 的**内部** `ch_out` 在顶层不可观测 ⇒ ⇒
  「子树内部算错但顶层输出恰好相同」这类缺陷**结构性测不出**。
- ⚠️ **`bresp` / `rresp` 被 master 忽略**（2026-11-24 · critic SHOULD_FIX）：`AxilMmioMaster` 不消费
  两个响应码 ⇒ ⇒ DECERR / SLVERR 对软件**不可见**（A8 恰好依赖「错误响应当成功完成、数据取 0」
  这一未写下来的语义）⇒ ⇒ 错误观测面 / 陷入口归 **S5**（与 `irq` 同批）。
- ⚠️ **`prog_*` 是运行期后门**（ADR-077 D3 长期保留）⇒ ⇒ 真实芯片形态的装载路径未定义。
- ⚠️ **A10–A14 的「单点错位」残留**（2026-11-27 · S4b P4 · **已部分闭合**）：
  若某窗口**单点**错位到「同偏移也可写」的外设（例：TIMER 窗口 → UART 从口 · 两者
  word 0 均可写）⇒ ⇒ 该条判据仍可能绿（写和读都落在同一个错误外设上 ⇒ 读回值一致）。
  ⇒ **A15 闭合了 PWM 面**（0xFF 是 word 0 唯一的非零复位值）；**UART / TIMER / SPI / I2C
  面仍无 SoC 级判别力**（四者的 word 0 复位值都是 0，word 4 的 SPI/I2C 又同为 0x64）。
  该形态由 P1 的**逐从口单测**（`tests/test_axi_lite_crossbar.cpp` 的 20 用例 / 508 断言）
  与 `SocCrossbarMap` 的 8 条 `static_assert` + 7 条 target 断言共同承担；
  本段判的是 **SoC 侧装配可用性**（6 个从口的事务通路），不重复承担路由本身的判别力。
