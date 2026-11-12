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

- **场景 G（自反性）**：输入恒定 8 拍 ⇒ `interrupt` 恒 0
  ⇒ 抓「中断卡在 1」「边沿检测退化成电平跟随」（**不需要任何期望值**）。
- 场景 F 的**屏蔽性**（`interrupt_enable=0` ⇒ 恒 0）是绝对判据，不计入 spec。

⚠️ **被否决的一个写法（留档）**：曾试图断言「**一次输入跳变 ⇒ 恰好 1 拍脉冲**」
⇒ ⇒ 实测 `pulse_count=0` ⇒ ⇒ 该判据与本 TB 前面的场景状态**强耦合**
（同步链相位不确定）⇒ ⇒ **不可靠** ⇒ 改为上面的自反性判据
（无状态依赖，且仍能抓「电平跟随」这一具体缺陷）。

**首次跑红**：**有** —— 首版手推把场景 D/E 的 `interrupt` 写成 0；
实测 `fails=2` ⇒ 定位到「**场景切换本身就是一次边沿**」⇒ 修的是**手推表**，不是实现。

---

## quadrature_encoder_example {#quadrature-encoder-example}

### 上游参考（**spec-derived**）

该 example 头部原文（无 Scala 源码，只有功能约定）：

> · **4 倍频计数**（上升沿 + 下降沿）
> · **A 相领先 B 相 = 顺时针 (CW)** · B 相领先 A 相 = 逆时针 (CCW)
> · **每个周期产生 4 个计数边沿**
> · 索引 (Z) 信号清零

⇒ ⇒ 规范只保证**幅值**（一周期 = 4 计数）与**方向名**（CW/CCW）；
**「CW 对应 +1 还是 -1」是计数器的符号约定 ⇒ 属 impl-contract**（本 TB **不硬判符号**）。

### 判据（语义级，不逐拍抄中间值）

| 判据 | 级别 | 期望 |
|---|---|---|
| 复位后 position | spec-derived | **0** |
| 正转 **2 个完整周期** | spec-derived（幅值） | `(pos - pos0) mod 65536 ∈ {+8, -8}` |
| 正转 2 周期后**反转 2 周期** | **metamorphic** | 回到 `pos0`（符号无关） |
| 正转 1 周期 | spec-derived（幅值） | `∈ {+4, -4}`（确认此刻确实非 0） |
| **Z 相上升沿** | spec-derived | `position == 0` |
| `a_sync` / `b_sync` 静止在 A=1/B=1 | impl-contract | 均为 1 |

### impl-contract 前提

- **同步寄存器初值是 `true`**（`a_sync1(true)` 等）⇒ 静止初态 **A=1 · B=1**
  ⇒ ⇒ TB 输入初值 MUST 也是 1（否则第一拍就有**启动伪边沿**）。
- 边沿用 `sync1/sync2`（2 拍延迟）⇒ 每个相位 MUST 维持 **≥4 拍**。

**首次跑红**：有 —— 首版按 `count_up/down` 的公式推出「11→10 是 +1」，
但实测是 **-1** ⇒ ⇒ 判定：**符号属 impl-contract**（规范未规定）⇒
改为**符号无关的幅值判据** + metamorphic（正反转回原点）。
（另：严格 flags 下还抓到 TB 自身的 `PINMISSING`（inline 后代输出未连）与
`WIDTHEXPAND`（`integer` vs 16 位）⇒ 均已修。）

## fifo_stream_example {#fifo-stream-example}

### ⚠️ 被测接口（实测 · 2026-11-12）

`examples/spinalhdl-ported/fifo/stream_fifo_example.cpp:28-34` 的顶层端口是
`din` / `push` / `pop` / `dout` / `empty` / `full` ⇒ ⇒ **没有 ready/valid**
⇒ ⇒ 原清单「缺 ready/valid 握手期望」的口径**已订正**（R3-a）⇒ ⇒
本项的判据对象 = **push/pop + empty/full**。

### 上游参考（**spec-derived** · FIFO 的定义性语义）

规格面 = `openspec/specs/chlib-stream-semantics/spec.md`（仓内唯一的 FIFO 语义 capability）
⇒ ⚠️ **口径**：那份 spec 治理的是 `chlib::sync_fifo` / `chlib::stream_fifo`（**库组件**）
⇒ ⇒ 本 TB 引它的**语义条款**（尤其「消费者 MUST 在 pop 沿**之前**采样才能拿到
正在被弹出的那个元素」+「pop 沿之后 payload 指向**下一个**队首」）
⇒ ⇒ **不引**它的**实现条款**（空/满自保护门控）—— 见下方「已知偏离」。

### 判据

| 判据 | 级别 | 期望 |
|---|---|---|
| 复位后 `empty` | spec-derived | `1`（无在途） |
| 在途为 0 时 `full` | impl-contract | `0` |
| push A,B,C 后依次 pop | spec-derived | 依次得到 **A,B,C**（顺序保持） |
| 取走全部后 `empty` | spec-derived | `1` |
| **16 个在途**（= 容量 = N）全部取回 | spec-derived | 顺序一致、**不丢数据** |
| `full` 门限（在途 15 / 16） | spec-derived（容量 = N） | `0` / `1`（**修复后**，见 D5） |
| `dout` 在 pop 沿**之前**采样 | spec-derived（FWFT 采样点） | 拿到的是**正在被弹出**的那个元素 |
| inline 后代端口（`top_fifo_inst_unnamed_output*`）与顶层端口 | impl-contract | 逐拍同源 |

### metamorphic（**相对判据** · 不依赖任何绝对期望值）

- **R1 置换性**：push N 个 ⇒ pop 出来的序列 **逐项恒等**于写入序列
  （TB 侧记录写入值再比较 ⇒ 不判任何绝对常量）。
- **R2 `empty` 不变量**：每次操作后 `empty` == 「TB 侧在途计数是否为 0」
  ⇒ ⇒ 抓「`empty` 卡住 / 乱跳」。

### ✅ D5 **已修复**（2026-11-12 · 缺陷 → 修复）

**原缺陷**：`full` 的比较用的是**完整 5 位指针**（`ch_reg<ch_uint<addr_width+1>>` = 5 位，
地址取低 4 位）⇒ `full = (wr+1 == rd)` ⇒ **门限 = 31 个在途**，
而内存只有 **16 槽** ⇒ ⇒ **第 17 次写入起地址混叠**（最老的元素被覆盖丢失）。

**实测证据**（修复前 · 用内部指针对齐时序后复跑）：
push 20 个（`10..23`）后逐个 pop ⇒ 得到 `20 21 22 23 14 15 ... 1f 20 21 22 23`
⇒ ⇒ 最老的 4 个（`10 11 12 13`）**已丢失**；且 `full` 在 20 次 push 内**从不置位**。

**根因与范围**：该写法**不是移植引入的** —— **上游参考** `samples/fifo_example.cpp`
（`FiFo<ch_uint<2>, 2>`）用的是**同一公式** ⇒ ⇒ **同型缺陷 ⇒ 两处同改**
（仓内 Zero-Debt「同型缺陷一处不留」）。
⚠️ 库组件 `chlib::sync_fifo`（`include/chlib/fifo.h`）**本身是对的**
（`tests/chlib/test_fifo.cpp` 断言满态）⇒ ⇒ 缺陷只在**手写 demo**。

**修法（经典方案）**：`full` = **地址位相等 + wrap 位不同**
⇒ ⇒ 容量 = N（16）⇒ ⇒ 与 `empty`（`rd == wr`，全部位相等）自洽。

**修复后的 TB 判据（正面判，不再回避）**：

| 判据 | 级别 | 期望 |
|---|---|---|
| 在途 ≤ 15 | spec-derived（容量 = N） | `full == 0` |
| 在途 == 16 | spec-derived（容量 = N） | `full == 1` |
| 取出一个后（在途 15） | spec-derived | `full == 0`（重新有空间） |
| push 16 个后全部取回 | spec-derived | 顺序一致、**不丢数据** |

⚠️ **不判**「满后再 push」 ⇒ ⇒ 本手写 demo **无自保护**（空/满不门控指针）
⇒ ⇒ 那是库组件 `chlib::sync_fifo` 的契约（`chlib-stream-semantics` 的自保护条款）
⇒ ⇒ **不适用**于本 example ⇒ ⇒ 判它 = 要求一个本 example 没有的能力。

**变异自证 v2**（修复后 · 对 DUT 变异 ⇒ TB MUST 转红）：

| 变异 | 期望 | 实测 |
|---|---|---|
| M1：删掉 wrap 位条件（`full` = 仅地址相等） | 转红 | ✅ `fails=1` |
| M2：读地址 `rd+1`（`dout` 指向下一个） | 转红 | ✅ `fails=25` |
| M3：`empty` 粘住（`rd == 5'h1F`） | 转红 | ✅ `fails=6` |
| M4：写地址 `wr+1`（写入偏移） | 转红 | ✅ `fails=27` |
| **M5：`full` 回退成旧公式**（`(wr+1 == rd)` 5 位） | **转红** | ✅ `fails=1` |

⇒ ⇒ **M5 是承重证据**：修复前的实现正是 M5 ⇒ ⇒ 修复**可被本 TB 判出** ⇒ ⇒
修完的 `full` 门限**不是**固化，而是**可判的**。

**首版 TB 的处置留档**：修复前 TB **刻意不判** `full` 在 15/16 在途处的取值
（当时连「两种正确实现」都不一致 ⇒ 判它 = 固化偶然选择）⇒ ⇒ 修复后判据才**正面放开**。


## uart_tx_example {#uart-tx-example}

### 上游参考（**spec-derived** · UART 8N1 帧格式）

UART 是**外部协议**（不是本仓实现）⇒ ⇒ 帧格式本身是 spec-derived：

| 位 | 约定 |
|---|---|
| 空闲 | **高**电平 |
| 起始位 | **1 位低**电平 |
| 数据 | **8 位，LSB first** |
| 停止位 | **1 位高**电平 |
| 帧长 | 共 **10 位** |

### impl-contract（本实现的取值）

- **位宽 = 434 拍**（`baud_counter == 433` ⇒ 0..433 共 434 拍 ⇒ 见 `uart_tx.cpp` 注释
  「波特率计数器 (16 位)：0..433 ⇒ 每位 434 拍」）。
- `busy`：START + DATA 期间为 **1**，IDLE / STOP 期间为 **0**。
- `frame_start` 需**恰好 1 拍脉冲**（内部有锁存器 `start_latch` ⇒ 1 拍延迟；
  锁存是**电平敏感**的 ⇒ 拉长会在帧结束后**再触发一帧**）。

### 判据（TB 的测量方法 = **边沿时间戳**，位宽是**测出来的**）

| 判据 | 级别 | 期望 |
|---|---|---|
| 空闲期 `uart_tx` | spec-derived | **恒高** |
| 帧首出现**低**电平（起始位） | spec-derived | 存在 |
| 起始位宽度 | spec-derived（1 位） | 与其余位**等宽** |
| 9 个可测段（起始位 + 8 数据位）宽度**全等** | **metamorphic** | 全等（**不依赖 434**） |
| 起始位取值 | spec-derived | **低** |
| 8 个数据位解码 | spec-derived（**LSB first**） | **回驱动字节**（0x55 / 0x5A） |
| 停止位取值 | spec-derived | **高** |
| 位宽绝对值 | impl-contract | **434 拍** |
| 帧 B 的位宽与帧 A 一致 | **metamorphic** | 一致（**不写死 434**） |
| `busy` 覆盖范围 | impl-contract | 起始位 + 数据位期间 = 1 · 停止位期间 = 0 |

⚠️ **停止位的宽度在本接口上不可观测**（STOP 与 IDLE 的 `uart_tx` 都是 1 ⇒ 停止位结束处
**没有边沿**）⇒ ⇒ TB **不判**停止位宽度，只判「停止位存在且为高」。

⚠️ **两帧用不同字节是刻意的**：
· **0x55**（0101_0101 ⇒ 交替位）⇒ **每个位边界都有边沿** ⇒ 可**直接测**位宽与各位取值；
· **0x5A**（0101_1010）⇒ 含**相邻同值位** ⇒ 边界无边沿 ⇒ 改用「按**测得**的位宽
  在位格**中点采样**」解码 ⇒ ⇒ 抓「位序 / LSB-first 错」（若按 MSB first 解 ⇒ 0xA5 ⇒ 红）。

### 变异自证（对 DUT 变异 ⇒ TB MUST 转红）

| 变异 | 期望 | 实测 |
|---|---|---|
| M1：DATA→STOP 条件去掉 `baud_done`（**第 8 个数据位只发 1 拍** —— 源码注释点名过的那类缺陷） | 转红 | ✅ `fails=5` |
| M2：数据位取 `shift_reg[8]`（**MSB first**） | 转红 | ✅ `fails=3` |
| M3：起始位改**高** | 转红 | ✅ `fails=3` |
| M4：停止位改**低** | 转红 | ✅ `fails=2` |
| M5：波特到期常量 433 → 216（**位宽减半**） | 转红 | ✅ `fails=1` |

⇒ ⇒ 全部 5 条**各自转红** ⇒ ⇒ TB 对「位宽 / 位序 / 起始位 / 停止位 / 位不等宽」都有判别力。

## uart_rx_example {#uart-rx-example}

（`pending` —— 缺 8N1 + **采样点相位**；metamorphic 可用与 `uart_tx` 的 loopback）

## axi4_lite_example {#axi4-lite-example}

（`pending` —— 缺 AXI4-Lite 从端握手：**VALID 不得等 READY** / 响应必须先有请求 /
`valid&&!ready` 时 payload 稳定 / 无请求时 `*VALID` 必 0）

## riscv-mini-rv32i-soc {#riscv-mini-rv32i-soc}

（`pending` · `kind=dut` —— smoke 口径：复位 pc / 首条指令取到 / `minitest` retire 数 / `sw` 落到 tohost）
