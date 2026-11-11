# examples 的 RTL 期望登记（`examples-rtl-sim-coverage` **E3**）

> 本文件是 **TB 的期望值出处**。TB 头部 MUST 写
> `EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#<anchor>`，
> **禁止**把被测 `<impl>.cpp` 写成出处（那是**转录**不是推导 —— metis 指出的循环论证形态）。

## 约定：期望分两级

| 级别 | 来源 | 计入 `spec_checks` |
|---|---|---|
| **spec-derived（强）** | ① **上游参考实现**（SpinalHDL Scala 原文 —— 它是**被移植的参考**，不是 C++ 实现）<br>② **外部规范**（UART 8N1 帧格式 / AXI4-Lite 协议 / RISC-V ISA 编码） | ✅ 计入 |
| **impl-contract（弱）** | 延迟拍数 · tie-off 值 · 复位值 · 端口组合形态 —— 这些**本来就是实现选择** | ❌ 不计入 |

⇒ ⇒ `spec_checks >= 3` 才算真正 `verified`（见 `.rddf/checks/examples-rtl-sim-targets.tsv`）。

## 通用推导前提（所有 TB 共用，属 impl-contract）

- 「**一拍**」= 一个 `posedge default_clock`。
- **复位释放 MUST 在 `negedge`** —— 若在 `posedge` 同一时刻（0 delta）拉低 `rst`，
  该沿的 `always_ff` 采样会看到**新的** `rst` 值 ⇒ 复位拍被吃掉（实测：手推 1、实测 2）。
- **采样时机**：`@(posedge clk); #1;` ⇒ 读的是沿**后**的值（`always_ff` 在沿上更新）。
- 计数器/位置寄存器宽 8 / 16 位 ⇒ **模 2^W 回绕**（属 spec-derived：位宽来自 Scala 的 `UInt(8 bits)`）。

---

## counter_simple_example {#counter-simple-example}

### 上游参考（**spec-derived**）

来源：SpinalHDL 原示例（`examples/spinalhdl-ported/counter/counter_simple.cpp` 头部逐字转录的 Scala）：

```scala
class Counter extends Component {
  val io = new Bundle {
    val value = out UInt(8 bits)
  }
  val counterReg = RegInit(U(0, 8 bits))
  counterReg := counterReg + 1
  io.value := counterReg
}
```

⇒ 语义：**复位值 0 · 每拍 +1 · 8 位无符号（模 256 回绕）· 输出 = 寄存器**。

### 输入序列 → 期望输出（无输入 ⇒ 只随拍变化）

| 拍（复位释放后） | 期望 `value` |
|---|---|
| 1 | 1 |
| 2 | 2 |
| 3 | 3 |
| N（1..255） | N |
| 256 | **0**（回绕） |

### metamorphic（相对判据，不依赖绝对值）

- **步进恒为 1**：任意相邻两拍 `(v[n+1] - v[n]) mod 256 == 1`。
- **周期 256**：`v[n] == v[n+256]`。

**首次跑红**：无（首跑即绿）。

---

## counter_example {#counter-example}

### 上游参考（**spec-derived**）

Scala 原文（同目录 `counter.cpp` 头部）：

```scala
val counterReg = RegInit(U(0, 8 bits))
when(io.clear) {
  counterReg := 0
} elsewhen(io.enable) {
  counterReg := counterReg + 1
}
io.value := counterReg
```

⇒ 语义：**clear 优先**（不是「异步清零」，是**同步**优先级）；否则 enable ⇒ +1；否则**保持**。

### 输入序列 → 期望输出

| enable | clear | 期望 `value` |
|---|---|---|
| 0 | 0 | 保持 |
| 1 | 0 | 上一拍 + 1（模 256） |
| 1 | 1 | **0**（clear 胜） |
| 0 | 1 | **0** |

### metamorphic

- **静止性**：`enable=0 ∧ clear=0` 的任意连续区间内，`value` 恒定。
- **clear 后重建**：`clear` 一拍后 `value == 0`，且随后每拍步进仍为 1。

**首次跑红**：无。

**⚠️ 已登记的跨路径分叉**：`counter.cpp:211-214` 的 C++ 侧注释自认
「enable/clear may not fully work yet」，但 **RTL 实测四项全对** ⇒ ⇒
该注释对 RTL **过时** ⇒ ⇒ **interpreter 路径与 RTL 路径可能分叉**，且 examples 的
C++ 侧**无任何断言** ⇒ 分叉不会被任何测试发现（已记入清单 `note`，待独立立项）。

---

## pwm_example {#pwm-example}

### 上游参考（**spec-derived**）

Scala 原文（同目录 `pwm.cpp` 头部）：

```scala
val counter = Reg(UInt(8 bits)) init(0)
counter := counter + 1
io.pwm = (counter < io.value) & io.enable
```

⇒ 语义：**`pwm = enable ∧ (counter < duty)`**；counter 自由运行（模 256）。

### 输入序列 → 期望输出

| enable | duty | 拍 N（counter = N mod 256） | 期望 `pwm_out` |
|---|---|---|---|
| 1 | 4 | 1,2,3 | 1 |
| 1 | 4 | 4,5,6 | 0 |
| 0 | 任意 | 任意 | **0** |
| 1 | 0 | 任意 | **0**（`counter < 0` 恒假） |
| 1 | 255 | 任意（counter ≠ 255） | 1 |

### metamorphic

- **占空比周期性**：连续 **256 拍**里高电平拍数 **== duty**（这是强判据 ——
  只依赖「counter 跑满 0..255」与 `pwm = en ∧ (counter < duty)`）。
- **enable 支配**：`enable=0` ⇒ 输出恒 0（与 counter 无关）。

**首次跑红**：无。

---

## gpio_example {#gpio-example}

### ⚠️ **上游 Scala 与被测 C++ 实现语义不同**（因此**不算** spec-derived）

Scala 原文（同目录 `gpio.cpp` 头部）：

```scala
val sync = Reg(Bool()) init(false)
sync := io.pin
val rise = io.pin & !sync
val fall = !io.pin & sync
io.interrupt := rise | fall
io.value := io.pin ## sync ## ...
```

与 C++ 实现的**实质差异**：

| 维度 | 上游 Scala | 被测 C++ 实现 |
|---|---|---|
| 同步级数 | **1 级**（`sync`） | **2 级**（`sync_stage1` / `sync_stage2`） |
| 边沿判定 | **单 bit**（`io.pin` 是 `Bool`） | **8 位逐位**（`gpio_in & ~sync_stage2` ⇒ 非零即中断） |
| 端口 | `pin` / `interrupt` / `value` | 多出 `output_enable` / `interrupt_enable`，且有 `output_reg` 锁存 |
| 输出 | `io.value := io.pin ## sync` | `gpio_out = oe ? output_reg : 0` |

⇒ ⇒ **本 TB 的期望值只能锁「当前 C++ 实现」的行为** ⇒ 级别 = `impl-contract`
⇒ ⇒ `spec_checks = 0`。
⇒ ⇒ ⚠️ **待裁决**：这些差异是「有意的扩展」还是「移植缺陷」？
⇒ ⇒ 在裁决前，本 TB 的作用是**防止行为静默漂移**（不是「证明它符合上游」）。

### 输入序列 → 期望输出（按**当前实现**手推）

记 `s1` / `s2` = 两级同步寄存器（**复位值 0**，注意与 quadrature 不同）。

| 场景 | 输入变化 | 期望 `interrupt` | 期望 `gpio_out` |
|---|---|---|---|
| A | 无变化（全 0） | 0 | 0 |
| B | `0x00 → 0xFF` | 第 1 拍 **1**（`rise = 0xFF & ~0x00`），第 2 拍 0 | 0 |
| C | `0xFF → 0x00` | 第 1 拍 **1**（`fall = ~0x00 & 0xFF`），第 2 拍 0 | 0 |
| D | `oe=1`，`0x00 → 0xA5` | **1**（⚠️ 场景切换本身是边沿） | 0xA5 |
| E | `oe=0`，`0xA5 → 0x5A` | **1**（`rise = 0x5A & ~0xA5 = 0x5A ≠ 0`） | 0 |
| F | `interrupt_enable=0` | **0**（边沿仍发生，被屏蔽） | — |

### metamorphic

- **静止性**：输入恒定 ⇒ `interrupt` 恒 0（不依赖边沿定义）。
- **屏蔽性**：`interrupt_enable=0` ⇒ `interrupt` 恒 0（无论输入怎么跳）。

**首次跑红**：**有** —— 首版手推把场景 D/E 的 `interrupt` 写成 0；
实测 `fails=2` ⇒ 定位到「**场景切换本身就是一次边沿**」⇒ 修的是**手推表**，不是实现。

---

## quadrature_encoder_example {#quadrature-encoder-example}

（`pending` —— 见清单的「缺什么」；本节将在实现 TB 时补齐）

## fifo_stream_example {#fifo-stream-example}

（`pending` —— 缺 ready/valid 握手期望，须引 `chlib-stream-semantics`）

## uart_tx_example {#uart-tx-example}

（`pending` —— 缺 **8N1 帧格式**：idle 高 / start 低 1 位 / LSB first / 1 stop / `T = clk/div`）

## uart_rx_example {#uart-rx-example}

（`pending` —— 缺 8N1 + **采样点相位**；metamorphic 可用与 `uart_tx` 的 loopback）

## axi4_lite_example {#axi4-lite-example}

（`pending` —— 缺 AXI4-Lite 从端握手：**VALID 不得等 READY** / 响应必须先有请求 /
`valid&&!ready` 时 payload 稳定 / 无请求时 `*VALID` 必 0）

## riscv-mini-rv32i-soc {#riscv-mini-rv32i-soc}

（`pending` · `kind=dut` —— smoke 口径：复位 pc / 首条指令取到 / `minitest` retire 数 / `sw` 落到 tohost）
