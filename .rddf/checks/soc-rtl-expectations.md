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

### 1. 顶层端口表（**14** 个 = 框架 2 + 根自有 12）

⚠️ **口径（2026-11-17 · ADR-075）**：根 `__io` 的 12 个端口**全部具名**
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
| 10 | `top_timer_irq` | in | 1 | `timer_irq` | **零消费死端口**（ADR-073 事实⑭）⇒ TB 须接 `0` |
| 11 | `top_rdata` | out | 32 | `rdata` | 读数据 |
| 12 | `top_done` | out | 1 | `done` | 完成标志 ⚠️ **粘滞**（首笔后恒 1）⇒ **禁**用它判完成 |
| 13 | `top_gpio_out` | out | 8 | `gpio_out` | GPIO 输出 |
| 14 | `top_uart_tx` | out | 1 | `uart_tx` | UART 发送（**空闲高**） |

⚠️ **14 个端口 MUST 全部连接**（否则严格 flags 下 `%Warning-PINMISSING` **致命** ——
仓内实证）。

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
| `timer_irq` | `1'b0` | **零消费死端口** ⇒ 接 0（不接 ⇒ PINMISSING 致命） |
| `rst_n` | `1'b1`（初始）→ 场景④ 拉 0 | 低有效 |
| `default_reset` | `1'b1` → **negedge** 释放 | 通用前提 |

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
