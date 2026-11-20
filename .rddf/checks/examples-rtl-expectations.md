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

### ✅ **D4 结案（2026-11-12）**：**无 interpreter / RTL 分叉** ⇒ ⇒ **不独立立项**

⇒ ⇒ 起因：`counter.cpp` 原有一句「Note: enable/clear may not fully work yet」
（怀疑 `select()` 吃 `ch_in<ch_bool>` 端口不生效）⇒ ⇒ **该怀疑是错的**：
实测（**interpreter 与独立 RTL 仿真两侧一致**）：
· `clear` 优先（clear=1 ⇒ 立即归零）· `enable=0` ⇒ 保持 · `enable=1` ⇒ +1
· 计到 255 ⇒ **255** ✓ · 再进一拍 ⇒ 溢出回 **0** ✓
⇒ ⇒ ⇒ `select(io().clear, …, select(io().enable, …))` **直接吃 `ch_in<ch_bool>` 是可行的**
（`uart_tx.cpp` 那条「端口无 `ch_bool` 转换 ⇒ MUST 经寄存器中转」只约束
`transition_when()` 的用法，**不**约束 `select()`）⇒ ⇒ 旧注释已**过时** ⇒ ⇒ 已更正
⇒ ⇒ ⇒ **结论：不是缺陷，是过时注释** ⇒ ⇒ **无需独立立项**（D4 结案）。

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

### ✅ **D1 裁决（2026-11-12 · 用户「按你的建议推进」）**：判为「**移植简化**」

⇒ ⇒ **不追平**上游 Scala 的逐位边沿语义 ⇒ ⇒ `spec_checks` **保持 0**（只判 impl-contract）
⇒ ⇒ ⇒ 备选（「追平 Scala」）**未采用** ⇒ ⇒ 若将来要追平 ⇒ ⇒ 需另独立项
（它会**改产品的可观测行为** ⇒ ⇒ 属行为变更 ⇒ ⇒ 须走公开面登记）。
⇒ ⇒ 裁决记录见本文「D1」条 + `.rddf/checks/examples-rtl-sim-targets.tsv` 的 `gpio_example` note。

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

### 上游参考（**spec-derived** · UART 8N1 + 中点采样）

| 判据 | 约定 |
|---|---|
| 收到的字节 | MUST **等于**发出的字节（线上 **LSB first**） |
| `valid` | MUST 是**单拍脉冲**（每字节一次） |
| 采样点 | MUST 落在**位的中段**（否则真实串口的时序抖动会直接采错） |

### impl-contract（本实现的取值）

- 位周期 = **`prescale + 1`** 拍；采样点 = **`div == prescale/2`**。
- `data` / `valid` 的复位值 = 0。

### ✅ 已修复的两个缺陷（2026-11-12 · 由本 TB 挖出）

| # | 缺陷 | 现象 | 修法 |
|---|---|---|---|
| **D6** | **采样点硬编码 `div == 4`** | 只对 `prescale == 9` 成立 ⇒ 任何别的 `prescale` 都在**位的前沿**采样 ⇒ 抖动即采错 | 改为 `div == (prescale >> 1)` ⇒ `prescale == 9` 时**行为不变**（9/2 = 4） |
| **D7** | **解出的字节被位反转** | 线上 `0x10` ⇒ 解出 `0x08` · `0x11` ⇒ `0x88` | 累加改为**右移 + 新位进 MSB** ⇒ 首个采样位落在 bit0 |

⚠️ **D7 为什么能藏这么久**：example 自带的 C++ demo 用的是 **0x5A** ⇒
**0x5A 是位回文**（反转后还是 0x5A）⇒ ⇒ **恰好掩盖**了缺陷 ⇒ ⇒
demo 的测试值已改为 **0x4B**（非回文）⇒ ⇒ 实测：重新注入缺陷后 demo **FAILURE（got 0xd2）**。

### 判据

| 判据 | 级别 | 期望 |
|---|---|---|
| 复位后 `valid` / `data` | impl-contract | 0 / 0 |
| 收 0x96 ⇒ `data` | spec-derived | **0x96**（⚠️ **非位回文**，否则掩盖 D7） |
| 收 0x69 ⇒ `data` | spec-derived | **0x69** |
| 连发 4 字节 ⇒ 收到的序列 | **metamorphic** | **逐项恒等**（相对比较，不写死值） |
| `valid` 为高的总拍数 | **metamorphic** | == 收到的字节数（**单拍脉冲**） |
| 帧间隔 | impl-contract | == **10 × 位周期** |
| **采样相位**（长位周期 `prescale = 99` + 3 个探针） | **spec-derived** | 采样点 ∈ **[1/3, 2/3)** ⇒ 中点采样 |

⚠️ **采样相位的夹逼怎么做的**：`prescale = 99`（位周期 100 拍）⇒
· 探针 A：数据位 0 在**前 1/3** 为 1 ⇒ MUST 读到 **0** ⇒ 采样点**不在**前 1/3
· 探针 B：数据位 0 在**前 2/3** 为 0 ⇒ MUST 读到 **0** ⇒ 采样点**不在**后 1/3
· 探针 A′：数据位 0 在**前 1/3** 为 0 ⇒ MUST 读到 **1** ⇒ 采样点**晚于**前 1/3（与 A 互证）
⇒ ⇒ 采样点 ∈ [1/3, 2/3) ⇒ ⇒ 旧实现（硬编码 `div == 4` ⇒ 采样点在 ≈1/20 处）⇒ ⇒ **红**。

⚠️ **不判**「停止位错误 ⇒ 不产生 valid」⇒ ⇒ 本实现**不校验停止位**（无 framing check）
⇒ ⇒ 那是协议健壮性要求 ⇒ ⇒ 判它 = 要求一个没有的能力 ⇒ ⇒ 已登记为已知偏离。

### 变异自证（对 DUT 变异 ⇒ TB MUST 转红）

| 变异 | 期望 | 实测 |
|---|---|---|
| **M1：位移累加改回左移（= 重新注入 D7 位反转）** | 转红 | ✅ `fails=7` |
| **M2：采样点改回硬编码 `div == 4`（= 重新注入 D6）** | 转红 | ✅ `fails=2` |
| M3：`valid` 不延迟（组合输出） | 转红 | ✅ `fails=7` |
| M4：位周期 +1（`div_done` 比 `prescale+1`） | 转红 | ✅ `fails=6` |
| M5：`cnt_done` 8 → 7（少采一位） | 转红 | ✅ `fails=8` |

## axi4_lite_example {#axi4-lite-example}

### 上游参考（**spec-derived** · capability `axi-slave-conformance` + `dual-backend-test-coverage`）

| Requirement | 条款 | TB 判据 |
|---|---|---|
| 低有效复位 | 「MUST 提供**低有效复位**」+「复位清零全部状态寄存器」 | 运行后拉低 `rst_n` ⇒ `bvalid`/`rvalid`=0 且寄存器**清零** |
| 锁存地址 | 「MUST **锁存地址**」（W 可延后 ⇒ 握手时采样） | AW 握手后改 `awaddr` ⇒ 写入仍落在**锁存**的地址 |
| AW/W 相位分离 | 「MUST 支持 **AW 与 W 相位分离**」 | 只有 AW ⇒ `bvalid` MUST 仍为 0 |
| 写响应保持到 BREADY | 「`bvalid` MUST 跨周期保持」+「MUST NOT 接受下一笔写事务（`awready` 保持 0）」 | 不拉 BREADY ⇒ `bvalid` 保持 + `awready` 保持 0 |
| BREADY 后清除 | 「握手后 `bvalid` MUST 清除」+「MUST 能接受下一笔事务」 | 握手后 `bvalid`=0 |
| 读响应保持到 RREADY | 同上（读侧） | 不拉 RREADY ⇒ `rvalid` 保持 + `arready` 保持 0 |
| 未映射地址 | 「MUST 返回 **DECERR**」 | 写/读 0x10 ⇒ `bresp`/`rresp` = `2'b11` |
| 字节写 | 「MUST 消费 `wstrb`」+「部分字节写只改选中字节」 | `wstrb`=0b0011 ⇒ 只改低 2 字节；`wstrb`=0（空写）⇒ MUST NOT 改 |

### impl-contract（本实现的取值）

- 寄存器窗口 = **0x00..0x0C**（4 个寄存器，步长 4 字节）⇒ 0x10 起窗口外。
- `wstrb` bit i ⇒ 第 i 个字节道（**从 LSB 起** ⇒ `wstrb` bit 0 = 最低字节）。
- `awready`/`arready`/`wready` 是**组合**的（= `valid & !busy`）⇒ 握手**同拍**完成。
- 从端实现 = `include/axi4/axi4_lite_slave.h` 的 `AxiLiteSlave<32,32,4>`。

### 判据

| 判据 | 级别 | 期望 |
|---|---|---|
| 运行后拉低 `rst_n` ⇒ `bvalid`/`rvalid` | spec-derived | 均为 0 |
| 运行后拉低 `rst_n` ⇒ 寄存器 | spec-derived | **清零** |
| AW 握手后改 `awaddr` ⇒ 写入落点 | spec-derived | **锁存**的地址（0x00），0x08 不受影响 |
| 只有 AW、无 W ⇒ `bvalid` | spec-derived | 0 |
| 写 4 个寄存器后读回 | **metamorphic** | **写入什么读回什么**（相对比较） |
| 写/读映射地址 ⇒ 响应 | spec-derived | `2'b00`（OKAY） |
| 写/读 0x10（窗口外） | spec-derived | `2'b11`（DECERR）· 读数据 MUST NOT 呈现别的寄存器值 |
| 不拉 BREADY ⇒ `bvalid` | spec-derived | **跨周期保持 1** |
| `bvalid` 挂起期间拉高 `awvalid` ⇒ `awready` | spec-derived | 仍为 0（不接受下一笔写） |
| BREADY 握手后 ⇒ `bvalid` | spec-derived | 清除 |
| 不拉 RREADY ⇒ `rvalid` | spec-derived | **跨周期保持 1** |
| `rvalid` 挂起期间拉高 `arvalid` ⇒ `arready` | spec-derived | 仍为 0 |
| RREADY 握手后 ⇒ `rvalid` | spec-derived | 清除 |
| 部分字节写（`wstrb`=0b0011） | spec-derived | 只改低 2 字节（`0x11223344` ⇒ `0x1122CCDD`） |
| 空写（`wstrb`=0） | spec-derived | 响应仍 OKAY 且寄存器**不变** |

### ⚠️ **端口映射 MUST 以产物为准**（本 TB 的第一课 · 2026-11-17 起**已消解**）

axi4 example 此前顶层输入**编号** = 组件 `describe()` 里**首次引用**的顺序 ⇒
TB 首版把 `awaddr`/`wdata` **互换** ⇒ ⇒ 15 个判据全红 ⇒ ⇒ 差点误判成「实现缺陷」
⇒ ⇒ 幸好先读了产物的寄存器/多路器链才定位到是 TB 自己写错。

⇒ ⇒ **2026-11-17 起该坑已消解**（ADR-075）：`AxiLiteTop` 的 20 个顶层端口**全部具名**
⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`（`top_awaddr` / `top_wdata` / …）
⇒ ⇒ TB 的连线**按名**写，不再依赖「编号 + 位宽序列反推」⇒ ⇒ 「猜错」这一类缺陷**不可能**再发生。
⇒ ⇒ ⚠️ 而**期望值仍 MUST 手推**（端口名可解析 ≠ 期望值可抄 ⇒ 两者无关）。

### 变异自证（对 DUT 变异 ⇒ TB MUST 转红）

| 变异 | 期望 | 实测 |
|---|---|---|
| M1：`bresp` 恒 OKAY（去掉 DECERR） | 转红 | ✅ `fails=1` |
| M2：**地址不锁存**（地址寄存器永不装载） | 转红 | ✅ `fails=4` |
| M3：`rst_n` 复位路径失效（寄存器不清零） | 转红 | ✅ `fails=1` |
| M4：字节道 lane1 恒有效 | 转红 | ✅ `fails=1` |
| M5：`bvalid` 退化为**单拍脉冲**（不保持） | 转红 | ✅ `fails=6`（**不挂死** ⇒ 等待有界） |

⚠️ **等待 MUST 有界**：首版用 `do @(posedge clk); while (!bvalid);` ⇒ 坏 DUT 会**挂死**
⇒ 只能靠护栏的 TIMEOUT（120s）抓住 ⇒ ⇒ 改为 `do … while (!x && g < 64)`
⇒ ⇒ 坏 DUT **快速失败**。
⚠️ **有界等待 MUST 用 do-while**（不能用 `for`）：`for` 的条件在**边沿之前**求值
⇒ 组合 ready 时**一次边沿都不走** ⇒ 握手不会发生 ⇒ ⇒ 实测直接踩到（9 个判据误红）。

### 验证源（`.verif` · 单一语义来源 · 2026-11-17）

**断言条数（人工登记）= 113** ⇒ ⇒ 三方等式：本行 == `tb_axi4_lite_example.verif` 的
静态展开（`EXPECT`+`WAIT` 计数）== 运行期 `RESULT: checks`。

VERIF-CHECKS: 113

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记 ⇒ ⇒ 禁由生成器
输出回填 ⇒ ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ ⇒ 判据恒绿）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**（粒度 = scenario）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `bringup_and_low_active_reset` | 「低有效复位」行 + 「复位清零」行 |
| `aw_w_phase_split_and_addr_latch` | 「AW/W 相位分离」+「锁存地址」行 |
| `write_read_roundtrip` | 「写 4 个寄存器后读回」（**metamorphic**） |
| `unmapped_addr_decerr` | 「写/读 0x10（窗口外）」行 |
| `bvalid_hold_until_bready` | 「不拉 BREADY ⇒ `bvalid`」+「`awready` 仍为 0」行 |
| `rvalid_hold_until_rready` | 「不拉 RREADY ⇒ `rvalid`」+「`arready` 仍为 0」行 |
| `wstrb_byte_lanes` | 「部分字节写」+「空写」行 |

#### scenario: bringup_and_low_active_reset

期望值见上表第 1 行的两条判据（均为 **spec-derived**）。

#### scenario: aw_w_phase_split_and_addr_latch

期望值见上表第 2 行的两条判据（**spec-derived**）。

#### scenario: write_read_roundtrip

**metamorphic**：写入什么就读回什么（相对判据 ⇒ 不锁死绝对值）。

#### scenario: unmapped_addr_decerr

写/读 `0x10`（窗口外）⇒ 响应 `2'b11`；读数据 MUST NOT 呈现别的寄存器值。

#### scenario: bvalid_hold_until_bready

不拉 `BREADY` ⇒ `bvalid` 跨周期保持 1，且 `awready` 保持 0；握手后清除。

#### scenario: rvalid_hold_until_rready

不拉 `RREADY` ⇒ `rvalid` 跨周期保持 1，且 `arready` 保持 0；握手后清除。

#### scenario: wstrb_byte_lanes

`wstrb`=0b0011 ⇒ 只改低 2 字节（`0x11223344` ⇒ `0x1122CCDD`）；`wstrb`=0 ⇒ 不变但响应仍 OKAY。

## riscv-mini-rv32i-soc {#riscv-mini-rv32i-soc}

> `kind=dut`（**不是** `CH_TRACKED_EXAMPLE_TARGETS` 的条目 ⇒ 单列）

### ⚠️ **与原清单的口径差异（重要）**

清单原写「`minitest` retire 数 / `sw` 落到 tohost」⇒ ⇒ 那要求外部 TB 装载 minitest 固件
⇒ ⇒ 期望值将依赖**仓内那个金标**（178 条退休）⇒ ⇒ **B3 的 T5 明确要重算该金标**
⇒ ⇒ 硬判它**必然在 B3 落地时转红**。

⇒ ⇒ **本 TB 改为：TB 装载自己的小程序**（经顶层的 `prog_we`/`prog_addr`/`prog_data` 装载口）
⇒ ⇒ 期望值**完全由 ISA 语义手推** ⇒ ⇒ **不依赖任何仓内金标** ⇒ ⇒ B3 改金标时不会红。
⇒ ⇒ 「retire 数」因此只判「单调递增且 > 0」（impl-contract），**不锁死具体值**。

### 小程序（7 个字 · 编码**手推**）

| 字 | 指令 | 编码 | 编码推导 |
|---|---|---|---|
| 0 | `addi x1, x0, 42` | `0x02A00093` | imm[11:0]=0x02A · rs1=0 · funct3=000 · rd=1 · opcode=0010011 |
| 1 | `nop` | `0x00000013` | 等 x1 写回（本流水线**无数据前推**） |
| 2 | `addi x2, x0, 11` | `0x00B00113` | imm=0x00B · rs1=0 · funct3=000 · rd=2 · opcode=0010011 |
| 3 | `nop` | `0x00000013` | 等 x2 写回 |
| 4 | `add x3, x1, x2` | `0x002081B3` | funct7=0 · rs2=2 · rs1=1 · funct3=000 · rd=3 · opcode=0110011 ⇒ **x3 = 42+11 = 53** |
| 5 | `sw x3, 0(x0)` | `0x00302023` | imm[11:5]=0 · rs2=3 · rs1=0 · funct3=010 · imm[4:0]=0 · opcode=0100011 ⇒ 写 **53** 到**字节地址 0** |
| 6 | `jal x0, 0` | `0x0000006F` | 自环（停机） |

⇒ ⇒ 编码口径与仓内 `rv32i-isa-semantics` capability 的样例交叉核对过
（该 capability 给出 `addi x21, x0, -8` = `0xFF800A93` ⇒ 同一套编码规则）✓

### 判据

| 判据 | 级别 | 期望 |
|---|---|---|
| 复位后第一次取指的 `dbg_id_pc` | spec-derived | **0x80000000**（`stages/if_stage.h:71` 的 `pc_reg` 复位值；I-TCM 从**字 0** 取指） |
| 程序里**每个**字都被取到（7 个） | spec-derived | 全部出现 |
| 取指 pc 按 **+4** 递增（前 6 段） | spec-derived | +4（第 7 个字是 `jal` ⇒ 会跳转 ⇒ 不判） |
| 写回 **x1 = 42** / **x2 = 11** / **x3 = 53**（rd 与数据各判） | spec-derived | ISA 语义手推 |
| 写存储：**数据 = 53** 且**地址 = 0** | spec-derived | `sw x3, 0(x0)` ⇒ 地址 = x0+0 = 0 |
| 退休计数单调递增 且 > 0 | **impl-contract** | **不锁死具体值**（见上「口径差异」） |

### ✅ 2026-11-13 · 端口名已具名（R17.2 · 用户 ⑤ 的原始诉求）

顶层 `Rv32iPipelineTop` 的 **32** 个端口**全部**带**显式名** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`
⇒ ⇒ **本 TB 不再需要「查 port map」**（`.top_obs_data_addr(obs_data_addr)` 取代 `.top_unnamed_output_1(obs_data_addr)`）。

⇒ ⇒ ⚠️ **判据面零变化**：本节的期望值**只依赖信号语义**，**不依赖端口名** ⇒ ⇒
⇒ ⇒ ⇒ **B3 改金标时不会红**（同「口径差异」段的结论）⇒ ⇒ 本次改名**不改变任何判据**。

⇒ ⇒ ⚠️ **仍有未具名端口** = **后代冒泡到顶层**的 `top_pipeline_*` / `top_itcm_*` / `top_dtcm_*`
（**80** 个）⇒ ⇒ 它们**不是**根组件自有 IO ⇒ ⇒ **不在本批范围** ⇒ ⇒ 归属**后续批次**。

### ✅ 发现 **D8**（**已裁决并落地** · 2026-11-14 · R8 改判 · 用户裁决 **A2**）：`ext_clk` 是**死端口** ⇒ ⇒ **已删**

顶层头文件曾写着 `pipeline.io().clk <<= io().ext_clk;` ⇒ ⇒ 但产物里
**所有**流水线寄存器都锁在框架 `default_clock`（`always_ff @(posedge default_clock …)`），
而流水线的 `clk` 端口**被赋值但无消费者**（产物中 `top_pipeline__unnamed_input_1` 只有赋值、无使用）
⇒ ⇒ ⇒ **`ext_clk` 对行为零影响** ⇒ ⇒ ⇒ 用户以为「SoC 由 `ext_clk` 驱动」，实际是 `default_clock`。

⇒ ⇒ 这是 E3 这类「按名字驱动外部端口」的覆盖**才能暴露**的问题
⇒ ⇒ 已登记为发现（见 `.rddf/checks/decision-request-rv32i-ext-clock-dead-port-2026-11.md`）。

⇒ ⇒ ✅ **2026-11-14 · B4 · C3 订正**：`ExStage` 的算术核心改由库组件 `Rv32iAlu` 承担
⇒ ⇒ **产物顶层端口 114 → 118**（+5 = ALU 组件内部节点冒泡）⇒ ⇒ **根组件自有端口仍是 31**
⇒ ⇒ 原 113 个端口**名一字未改** ⇒ ⇒ 本 TB 只需**新增** 5 根连接（无重映射）
⇒ ⇒ ⚠️ 「未知 opcode ⇒ ALU 结果 = `rs1 + rs2`」有**显式断言**（`test_rv32i_stage_dual.cpp`）

⇒ ⇒ ✅ **2026-11-15 · B4 · C4 订正**：`IdStage` 的立即数改由库组件 `Rv32iDecoder` 承担
⇒ ⇒ **产物顶层端口 118 → 126**（+8 = decoder 的 8 个输出冒泡：opcode 7 · rd 5 · funct3 3 ·
rs1 5 · rs2 5 · funct7 7 · imm 32 · instr_type 3）⇒ ⇒ **根组件自有端口仍是 31**
⇒ ⇒ 原 118 个端口**名一字未改** ⇒ ⇒ 本 TB 只需**新增** 8 根连接（无重映射）
⇒ ⇒ 行为零变化**实测**：独立 Verilog TB `checks=24 fails=0`（与 C3 后**同一组判据**）

⇒ ⇒ ✅ **2026-11-15 · B4 · G1+G4 订正**：SoC 顶层新增 `dbg_illegal_reason`（2 bit）
⇒ ⇒ **产物端口 126 → 129**（+3 = SoC 自有 `dbg_illegal_reason` + pipeline 的
`top_pipeline_dbg_illegal_reason` + ID 级的 `top_pipeline_id_stage_illegal_reason`
⇒ ⇒ **全部具名** ⇒ ⇒ 既有 unnamed 端口编号**零位移** ⇒ ⇒ TB 只新增 3 根连接）
⇒ ⇒ 根组件自有端口 **31 → 32**（C6 快照 +1）
⇒ ⇒ 独立 Verilog TB `checks=24 fails=0`（装载器同时按 G3 纪律把尾部填成 NOP）
⇒ ⇒ ⚠️ **环境注意**：本机 `build/verilator-install/include/verilated.mk` 的
`CFG_CXXFLAGS_PCH_I` **为空**（verilator configure 未识别出编译器 ⇒ `CFG_CXX_VERSION = ""`）
⇒ ⇒ ⇒ 任何**未命中缓存**的模型编译都会以
`g++: error: <prefix>__pch.h.fast: linker input file not found` 失败
⇒ ⇒ ⇒ （与本次改动**无关**；属**环境缺陷**）⇒ ⇒ 本地修法 = 该行补 `-include`（`build/` 是
未跟踪产物）⇒ ⇒ CI 侧是否同样缺陷**未验证**（CI 自建 verilator ⇒ 其检测路径不同）

⇒ ⇒ ✅ **2026-11-14 裁决 = A2（删端口）** ⇒ ⇒ **已落地**：
`Rv32iPipelineTop::ext_clk` 与 `riscv::Rv32iPipeline::clk` **双双删除**
⇒ ⇒ 顶层端口面 **32 → 31**（产物 **114 → 113** 端口 · **6** 输入 + **107** 输出）
⇒ ⇒ ⚠️ **删前针对性实测**（R8.2 判据）：`ext_clk` **0/1 交替翻转**
⇒ ⇒ **60 拍 × 27 观测逐拍全等**（1351 断言）⇒ ⇒ 零行为影响**实测**支撑
⇒ ⇒ ⚠️ **TB 必须删掉 `top_ext_clk` 那根线**（不删 ⇒ verilator `PINMISSING` **致命**）
⇒ ⇒ **真时钟源 = 框架 `default_clock`**（本 TB 的 `clk`）。

### 变异自证（对 DUT 变异 ⇒ TB MUST 转红）

| 变异 | 期望 | 实测 |
|---|---|---|
| M1：`pc_reg` 复位值 0x80000000 → 0 | 转红 | ✅ `fails=1` |
| M2：`dbg_id_valid` 恒 0 | 转红 | ✅ `fails=8` |
| M3：`obs_data_write_en` 恒 0 | 转红 | ✅ `fails=1` |
| M4：退休计数恒 0 | 转红 | ✅ `fails=1` |
| M5：**装载口数据恒 0**（程序全 0） | 转红 | ✅ `fails=14` |

### ✅ 层次化形态（`kind=hier` · 2026-11-17 · 诉求 ⑤「层次化输出示例」）

**同一份期望源**适用于**两种发射形态**（扁平 `/ kind=dut` · 层次化 `/ kind=hier`）
⇒ ⇒ 二者共用**同一个独立 Verilog TB**（`examples/riscv-mini/tb_rv32i_soc.sv`）
⇒ ⇒ 理由 = **顶层端口表同源**：两种形态都走 `make_flat_module_view`
⇒ ⇒ 端口名 + 方向 + 宽度 + 顺序**逐字逐序相同**（实测 **34** = 6 in + 28 out）。

| 面 | 扁平（`kind=dut`） | 层次化（`kind=hier`） |
|---|---|---|
| 发射入口 | `emit_rtl_rv32i_soc`（`toVerilog`） | `emit_rtl_rv32i_soc_hier`（`--hierarchical` ⇒ `toVerilogHierarchical`） |
| 产物形态 | 单 module（`top`） | **8 module**（7 stage + `top`）· 单文件 |
| 顶层端口 | 34（6 in + 28 out） | **34**（6 in + 28 out）⇒ ⇒ 逐字逐序同源 |
| 期望值 | 本节的判据表 | **同一张表**（同源 ⇒ 无需另写一份） |
| 实测 | `RESULT: checks=24 fails=0` | `RESULT: checks=24 fails=0` |

⚠️ **不新增独立的期望源**：层次化形态**不引入新语义** ⇒ ⇒ 若另写一份判据，
两份**必然漂移**。⇒ ⇒ 本节的判据表是**唯一真相**。

⚠️ **层次化形态的专属护栏判据**（不属于期望值，属**形态**）：
`scripts/verify_examples_rtl_sim.sh` 的 **⑧'** —— 产物 MUST 含 **≥ 2** 个 module
且含根 module `top` ⇒ ⇒ 否则「层次化」已**静默退化成扁平**
（⚠️ 端口表判据 ⑧ 对这条**结构性失明**：退化后端口表照样相同）。

## tb_phase3a_example {#phase3a-example}

> ⚠️ **2026-11-20 新增**（change `phase3a-rtl-product-2026-11`）—— Phase 3A 集成单元
> （4×4 Interconnect + AXI-to-Lite Converter + AXI4-Lite Slave）进 RTL 产品面。
> ⇒ ⇒ 期望值**手推**（**禁**从仿真输出 / 从被测实现转录）。

### 1. 顶层端口表（**9** 个 = 框架 2 + 根自有 7）

⚠️ **口径 = SoC 的同款**（`soc-rtl-expectations.md` §1「顶层端口表（**14** 个 = 框架 2 + 根自有 12）」）
⇒ ⇒ **不是 7**（根 `__io` 只 7 个，但产品端口面**含框架注入的 2 个**）。

| # | 产物端口名 | 方向 | 位宽 | C++ 成员（根 `__io`） | 语义 |
|---|---|---|---|---|---|
| 1 | `default_clock` | in | 1 | （框架注入） | 时钟 |
| 2 | `default_reset` | in | 1 | （框架注入） | 复位（**高有效**） |
| 3 | `top_start` | in | 1 | `start` | 事务发起（1 拍脉冲） |
| 4 | `top_is_write` | in | 1 | `is_write` | 写 / 读选择（1 = 写） |
| 5 | `top_addr` | in | 32 | `addr` | 地址（事务期间保持稳定） |
| 6 | `top_wdata` | in | 32 | `wdata` | 写数据 |
| 7 | `top_rst_n` | in | 1 | `rst_n` | AXI 级低有效复位（**与框架复位不是同一个**） |
| 8 | `top_rdata` | out | 32 | `rdata` | 读回数据 |
| 9 | `top_done` | out | 1 | `done` | 事务完成（**注册一拍** ⇒ 非组合脉冲） |

⇒ ⇒ 权威 = manifest `port_table[]`（与产物端口表**同源**）；护栏 ⑧ 逐字逐序比对。

### 2. module 集（**1** 个 = `{top}`）

⚠️ **phase3a 的 `describe()` 里无 `ch_layer`**（三个 `ch_module` 都是**内联**组件）⇒ ⇒
**层次化产物 = 扁平产物 = 1 个 module**（两者逐字节相同）⇒ ⇒ 期望 module 集 = **`{top}`**。

⇒ ⇒ 护栏 ⑧'（「层次化产物 MUST ≥ 2 module」）对本单元**结构性失明** ⇒ ⇒ 故归 `target` 桶
（`soc` 桶 = **有显式 `ch_layer` 成层声明**的装配 ⇒ 本单元无 ⇒ 不属该桶）。

### 3. 断言集（**6** 条 = `min_checks`）

**外部规范 = AXI4-Lite 协议**（AW/W/B/AR/R 五通道 + 寄存器读写语义 + 复位语义）⇒ ⇒ 标
**spec-derived** 的断言有 **3** 条（②③④ ⇒ ⇒ `spec_checks = 3`）；上电初值与两种复位 =
**impl-contract**（①⑤⑥ ⇒ **不计入**）；拍数余量亦为 impl-contract。

⚠️ **两种复位语义不同（MUST 分别驱动 · 2026-11-20 手推修正）**：

| 复位 | 极性 | 作用面 | 手推依据 |
|---|---|---|---|
| `rst_n`（根自有） | **低有效** | **只**透传 AXI 子系统（converter → slave / interconnect）⇒ ⇒ **清 slave 的寄存器** ⇒ ⇒ **不影响**内部 FSM | `converter.io().axil_rst_n = io().rst_n`；slave 的寄存器走 `select(!io().rst_n, <init>, …)` |
| `default_reset`（框架注入） | **高有效** | **所有 `ch_reg`**（含内部 FSM：`done_reg` / `mstate` / …） | `include/core/context.h:242`「所有寄存器共享 `default_clock_` 和 `default_reset_`」 |

⇒ ⇒ ⚠️ **初稿曾把断言 5 写成「`rst_n` 拉低 ⇒ `done == 0`」⇒ ⇒ 错**（`done_reg` 的复位源是
框架 `default_reset`，**不是**根 `rst_n` ⇒ ⇒ 拉低 `rst_n` **不会**清 `done`）⇒ ⇒ 已订正。
⇒ ⇒ ⚠️ **二次订正（2026-11-20 · 实测后）**：初稿的第二版写成「`rst_n` 拉低 ⇒ **再读同址**
⇒ `rdata == 0`」⇒ ⇒ **也错** —— 实测读回**仍是 0xAB** ⇒ ⇒ 根因 = 拉低 `rst_n` 后 master 的
FSM **仍在等 R 响应**（不受 `rst_n` 影响）⇒ ⇒ 而 slave 已被复位 ⇒ ⇒ **响应永不到来**
⇒ ⇒ 读事务**根本不完成** ⇒ ⇒ `rdata` 不会被更新 ⇒ ⇒ 「读回 0」**不可达** ⇒ ⇒ 已订正为
断言 ⑥（「读事务**不完成** ⇒ `done` 仍 == 0」）⇒ ⇒ 该形态**可观测且有判别力**。

**断言时序（6 条的顺序不可换）**：① 上电 ⇒ ② 写 ⇒ ③ 读 ⇒ ④ 读回值 ⇒ ⑤ 框架复位（脉冲后
**释放**）⇒ ⑥ `rst_n` 拉低 + 读 ⇒ ⇒ ④ 必须在 ⑤ **之前**（否则读回已是 0）⇒ ⇒ ⑤ 必须在
②③ **之后**（否则 `done` 恒 0 ⇒ 无判别力）⇒ ⇒ ⑤ 之后**必须释放**框架复位（`rst` 是 level
有效）⇒ ⇒ 否则 ⑥ 的读事务压根发不出去 ⇒ ⇒ ⑥ 的判别力就来自 `rst_n` 而非「框架仍复位」。

| # | 断言 | 期望 | 类别 | 手推依据 |
|---|---|---|---|---|
| 1 | 复位后初始 `done` | `0` | impl-contract | `done_reg` 的 INIT = `ch_bool(false)`；框架复位分支把它清 0 ⇒ ⇒ 上电即 0 |
| 2 | 写事务完成 | `start=1, is_write=1, addr=0x00, wdata=0xAB` ⇒ **有界拍数内** `done == 1` | **spec-derived** | AXI4-Lite 写 = AW+W+B 三段握手 ⇒ B 响应必须回来 ⇒ 完成信号必须拉高 |
| 3 | 读事务完成 | `start=1, is_write=0, addr=0x00` ⇒ **有界拍数内** `done == 1` | **spec-derived** | AXI4-Lite 读 = AR+R 两段握手 ⇒ R 响应必须回来 |
| 4 | 读回值 | 断言 3 之后 `rdata == 32'h0000_00AB` | **spec-derived** | AXI4-Lite 寄存器语义 = **同址写后读回同值** ⇒ 写 0xAB 到 0x00 ⇒ 读 0x00 ⇒ 0xAB |
| 5 | 框架复位生效 | `default_reset` 拉高（高有效）跨过 1 个时钟沿 ⇒ `done == 0` | impl-contract | `ch_reg` **共享框架 `default_reset`**（`include/core/context.h:242`）⇒ ⇒ 内部 FSM（`done_reg` / `mstate`）被框架复位清零 ⇒ ⇒ 此刻 `done` **曾为 1**（断言 ③ 之后）⇒ ⇒ **有判别力** |
| 6 | AXI 级复位 ⇒ 事务被丢弃 | `rst_n` 拉低（跨过 ≥1 沿）⇒ 发起一笔读 ⇒ **有界拍数内** `done` **仍 == 0** | impl-contract | `rst_n`（低有效）经 converter 透传到 `AxiLiteSlave` ⇒ ⇒ slave 的 `ch_reg` 被 `select(!rst_n, <init>, …)` 清零 ⇒ ⇒ 但 **master 的 FSM（`mstate` / `done_reg`）不受 `rst_n` 影响**（其复位源是**框架** `default_reset`）⇒ ⇒ master 仍在等 R 响应 ⇒ ⇒ 而 slave 已被复位 ⇒ ⇒ 响应永不到来 ⇒ ⇒ **事务被丢弃 ⇒ `done` 永不拉高** ⇒ ⇒ **判别力**：若 `rst_n` 链路失效 ⇒ 读事务正常完成 ⇒ `done == 1` ⇒ 转红 |

⚠️ **拍数余量 = impl-contract**（不计入 `spec_checks`）：断言 2/3 的「有界」上界取 **64 拍**
（**余量常量**，**不是**期望值 —— 期望值是「完成」这个事件本身；上界只保证「有界」⇒ ⇒
**不**用它判语义）⇒ ⇒ 实际拍数由实现决定 ⇒ ⇒ 上界只用于「挂死 ⇒ 失败」。

⚠️ **断言 4 的判别力来源**：`rdata` 是**读回数据** ⇒ 若 interconnect 路由 / converter 相位分离 /
AXI4-Lite slave 任一处出错 ⇒ 读回值 ≠ 0xAB ⇒ ⇒ 断言 4 转红 ⇒ ⇒ 这是本单元**最强的判据**。

⚠️ **验收边界**：16 对路由的验收由 `tests/test_axi_interconnect_4x4.cpp`（**组件级**）承担
⇒ ⇒ 本 TB **不是** 16 对路由的验收手段（既有纪律）。

### 4. 变异自证（判别力自证 · 登记于本段）

⚠️ **实测结果（2026-11-20 · 变异打在**临时产物**上 ⇒ 仓内产物零改动）**：

| # | 变异形态 | 实测 | 抓的是 |
|---|---|---|---|
| M1 | `assign top_rdata = top_reg;` ⇒ `= 32'h0`（读回通路断） | ✅ **断言 4 转红**（`got=0x00000000 want=0x000000ab`）· `checks=6 fails=1` | 读回语义（**最强判据**） |
| M2 | `assign top_done = top_reg_1;` ⇒ `= 1'b0`（完成信号断） | ✅ **断言 2 + 3 同时转红** · `checks=6 fails=2` | 完成信号 |
| M3 | 复位失效（`done` 不被框架复位清零） | **未做**（本轮只做 2 条 ⇒ 满足「≥2 条」门槛） | 复位路径（留待后续） |

⇒ ⇒ 还原后复跑 ⇒ `RESULT: checks=6 fails=0` ⇒ ⇒ 变异未污染产物。
⚠️ 每条变异 MUST **先确认仍能编译**（编译失败 ⇒ 跑的是**旧二进制** ⇒ 假绿）。

⚠️ 每条变异 MUST **先确认仍能编译**（编译失败 ⇒ 跑的是**旧二进制** ⇒ 假绿）。

### 5. C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（解释器路径，`tb_phase3a_example.cpp`）= **2** 条 ⇒ ⇒ 清单 `interp_checks = 2`。
- TB 侧（Verilog 产物）= **6** 条 ⇒ ⇒ 清单 `min_checks = 6`。

⇒ ⇒ **两者不等是预期**（SoC 行同型）⇒ ⇒ ⚠️ MUST NOT 用一侧代用另一侧
（ADR-074 §7.6 的 AC-R13′-2 口径）⇒ ⇒ 护栏 ⑨ 判 C++ 侧（`interp_checks`）、⑥ 判 TB 侧（`min_checks`）。

---

## tb_stream_mux_demo {#stream-mux-demo}

### 上游语义（**spec-derived** · capability `chlib-stream-ready-ownership`）

被测单元 = `examples/stream/stream_mux_top.h`（`chlib::stream_mux<4, ch_uint<8>>` 的具名端口包装）。
`stream_mux` 的契约（组合 0-cycle，无内部状态）：

| 端口 | 语义 |
|---|---|
| `out_payload` | `in[select].payload`（多路选择的定义） |
| `out_valid` | `in[select].valid`（**只有被选中路**的 valid 能到达输出） |
| `in[i].ready` | `(select == i) & out_ready`（反压**只**扇出到被选中路） |

### impl-contract（本实现的取值）

- **框架复位**把全部数据输入置 0（`run_verif` 的 `apply_frame_reset` 与渲染式 TB 的
  `write_tb_reset_pulse` 同构）⇒ ⇒ 复位后 `select=0` / 四路 `valid=0` / `out_ready=0`。
- 组合逻辑 ⇒ ⇒ `SET → SETTLE → EXPECT` 即可观测（**无需** `TICK`）。
- `select` 宽 2 bit 且 `N = 4` ⇒ ⇒ **不存在**越界选择。

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 复位后 `out_valid` / 四路 `in*_ready` | impl-contract | 全 0（输入全 0 ⇒ 选中的 `valid`=0 · `out_ready`=0） |
| 2 | `select=0..3` 时 `out_payload` | spec-derived | 依次 = `0x11` / `0x22` / `0x33` / `0x44` |
| 3 | 四路都有效时 `out_valid` | spec-derived | 1 |
| 4 | `out_ready=1` 时 `in[i].ready` | spec-derived | 仅 `i == select` 为 1，其余为 0 |
| 5 | `out_ready=0` 时 `in[select].ready` | spec-derived | 0（反压生效） |
| 6 | `out_ready=0` 时 `out_valid` | spec-derived | 仍为 1（valid 与 ready 独立） |
| 7 | 未被选中路 `valid=1`、被选中路 `valid=0` ⇒ `out_valid` | spec-derived | 0（valid 不外泄） |

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 24** ⇒ ⇒ 三方等式：本行 == `tb_stream_mux_demo.verif` 的
静态展开（`EXPECT` + `WAIT` 计数）== 运行期 `RESULT: checks`。

VERIF-CHECKS: 24

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记 ⇒ ⇒ 禁由生成器
输出回填 ⇒ ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ ⇒ 判据恒绿）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**（粒度 = scenario）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `reset_defaults` | 判据 1（impl-contract） |
| `payload_follows_select` | 判据 2 · 3 · 4 |
| `backpressure_only_selected_ready` | 判据 5 · 6 |
| `unselected_valid_does_not_leak` | 判据 7 |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个 —— 本设计无 `ch_layer` ⇒ 层次化 = 扁平）。
- **顶层端口表 = 18 根** = 框架 2（`default_clock` · `default_reset`）+ 根自有 **16**
  （4 路 × {payload[7:0], valid, ready} + `select[1:0]` + {out_payload[7:0], out_valid, out_ready}）。
- 产物端口名 = `top_<C++ 成员名>`（ADR-075 E1 · 端口全部具名 ⇒ 无 `top_unnamed_*`）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（解释器路径，`tb_stream_mux_demo.cpp` 默认模式）= **24** 条 ⇒ ⇒ `interp_checks = 24`。
- TB 侧（Verilog 产物，`tb_stream_mux_demo.sv`）= **24** 条 ⇒ ⇒ `min_checks = 24`。
- **两侧同源**（同一份 `.verif` 派生）⇒ ⇒ 相等是预期 ⇒ ⇒ 但 MUST **各自登记**
  （护栏 ⑨ 判 C++ 侧、⑥ 判 TB 侧 ⇒ ⇒ 禁一侧代另一侧）。

#### scenario: reset_defaults

框架复位后：`out_payload == 0` · `out_valid == 0` · `in0..in3_ready == 0`（6 条）。

#### scenario: payload_follows_select

四路 payload 分别为 `0x11/0x22/0x33/0x44`、四路 `valid=1`、`out_ready=1`；
`select` 依次取 0/1/2/3 ⇒ 输出跟随选中路，且只有选中路 `ready=1`（10 条）。

#### scenario: backpressure_only_selected_ready

`select=2`、`in2.valid=1`：`out_ready=0` ⇒ `in2_ready=0` 且 `out_valid=1`；
`out_ready=1` ⇒ `in2_ready=1` 且 `in0_ready=0`（5 条）。

#### scenario: unselected_valid_does_not_leak

`select=1`、`in1.valid=0`、`in0.valid=1` ⇒ `out_valid=0`、`in1_ready=1`；
`in1.valid=1` ⇒ `out_valid=1`（3 条）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测 | 抓的是 |
|---|---|---|---|
| M1 | `assign top_out_valid = 1'b0;`（valid 通路断） | ✅ `fails=3`（`payload_follows_select#1` · `backpressure_only_selected_ready#1` · `unselected_valid_does_not_leak#2`） | valid 选择语义 |
| M2 | `assign top_in0_ready = 1'b1;`（反压扇出断） | ✅ `fails=4`（`reset_defaults#2` · `payload_follows_select#6` · `backpressure_only_selected_ready#2/#4`） | ready 扇出规则 |

⇒ ⇒ 还原后复跑 ⇒ `RESULT: checks=24 fails=0` ⇒ ⇒ 变异未污染产物。
⚠️ 每条变异 MUST **先确认仍能编译**（两条实测 `compile_rc=0`）⇒ ⇒ 否则跑的是**旧二进制** ⇒ 假绿。

---

## tb_stream_demux_demo {#stream-demux-demo}

### 上游语义（**spec-derived** · capability `chlib-stream-ready-ownership` 的「分流 / 分叉类」条 · ADR-056）

被测单元 = `examples/stream/stream_demux_top.h`（`chlib::stream_demux<ch_uint<8>, 4>` 的具名端口包装）。
`stream_demux` 的契约（组合 0-cycle，无内部状态）：

| 端口 | 语义 | 出处 |
|---|---|---|
| `out[i].valid` | `in.valid & (select == i)` | ADR-056 D3（数据面只依赖 `input.valid`） |
| `out[i].payload` | `(select == i) ? in.payload : 0` | `chlib::demux` 的数据面（未选中路取 0） |
| `in.ready` | `in_range & out[select].ready` | ADR-056 D1（**消费端 ⇒ 上游**） |
| `out[i].ready` | **默认恒就绪 + 消费端 `<<=` 覆盖** | ADR-054 形态 E（capability「输出 ready MUST 是『默认值 + 可覆盖』」） |

⚠️ **「非选中路不影响上游」**（capability 的隔离条）：`in.ready` 只由**被选中路**的
ready 决定 ⇒ ⇒ 未选中路的 ready 传什么都无所谓 ⇒ ⇒ 判据 MUST 覆盖「把未选中路拉低
**不**影响上游」这一面（只断言被选中的一路不足以证明隔离）。

⚠️ **「越界 ⇒ 挡住上游」条在本设计不可达**：`select` 宽 2 bit 且 `N = 4` ⇒ ⇒
`select ∈ [0,4)` **恒成立** ⇒ ⇒ capability 的越界条在此**不适用**（**如实登记**，
不假装覆盖）。

### impl-contract（本实现的取值）

- **框架复位**把全部数据输入置 0（`run_verif` 的 `apply_frame_reset` 与渲染式 TB 的
  `write_tb_reset_pulse` 同构）⇒ ⇒ 复位后 `select=0` / `in_valid=0` / 四路 `out*_ready=0`。
- 组合逻辑 ⇒ ⇒ `SET → SETTLE → EXPECT` 即可观测（**无需** `TICK`）。
- `select` 宽 2 bit 且 `N = 4` ⇒ ⇒ **不存在**越界选择（同上）。

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 复位后四路 `out*_valid` | impl-contract | 全 0（`in_valid=0` ⇒ 数据面无效） |
| 2 | 复位后 `out0_payload` / `in_ready` | impl-contract | 0（`select=0` ⇒ `out0.payload = in.payload = 0`；`out0_ready=0` ⇒ 上游停顿） |
| 3 | `select=k` 时 `out[k].valid` | spec-derived | 1（其余三路 0） |
| 4 | `select=k` 时 `out[k].payload` | spec-derived | `in.payload`（`0xAB`） |
| 5 | `select=k` 时**未选中路**的 `payload` | spec-derived | 0（**不是**广播） |
| 6 | 四路 ready=1 时 `in.ready` | spec-derived | 1 |
| 7 | `select=2` 且 `out2_ready=0`（未选中路 ready 仍为 1）⇒ `in.ready` | spec-derived | **0**（隔离 + 反压方向） |
| 8 | `out2_ready=0` 时 `out2.valid` / `out2.payload` | spec-derived | 1 / `0xCD`（数据面不依赖 ready） |
| 9 | `out2_ready` 恢复 1 ⇒ `in.ready` | spec-derived | 1 |
| 10 | `in_valid=0` ⇒ 四路 `out*_valid` | spec-derived | 全 0 |
| 11 | `in_valid=0` 时 `in.ready` | spec-derived | 1（ready 不依赖 valid） |

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 38** ⇒ ⇒ 三方等式：本行 == `tb_stream_demux_demo.verif` 的
静态展开（`EXPECT` + `WAIT` 计数）== 运行期 `RESULT: checks`。

VERIF-CHECKS: 38

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记 ⇒ ⇒ 禁由生成器
输出回填 ⇒ ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ ⇒ 判据恒绿）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**（粒度 = scenario）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `reset_defaults` | 判据 1 · 2（impl-contract） |
| `payload_follows_select` | 判据 3 · 4 · 5 · 6 |
| `backpressure_reaches_upstream` | 判据 7 · 8 · 9 |
| `no_valid_all_outputs_idle` | 判据 10 · 11 |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个 —— 本设计无 `ch_layer` ⇒ 层次化 = 扁平）。
- **顶层端口表 = 18 根** = 框架 2（`default_clock` · `default_reset`）+ 根自有 **16**
  （输入流 {payload[7:0], valid, ready} + `select[1:0]` + 4 路输出流 × {payload[7:0], valid, ready}）。
- 产物端口名 = `top_<C++ 成员名>`（ADR-075 E1 · 端口全部具名 ⇒ 无 `top_unnamed_*`）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（解释器路径，`tb_stream_demux_demo.cpp` 默认模式）= **38** 条 ⇒ ⇒ `interp_checks = 38`。
- TB 侧（Verilog 产物，`tb_stream_demux_demo.sv`）= **38** 条 ⇒ ⇒ `min_checks = 38`。
- **两侧同源**（同一份 `.verif` 派生）⇒ ⇒ 相等是预期 ⇒ ⇒ 但 MUST **各自登记**
  （护栏 ⑨ 判 C++ 侧、⑥ 判 TB 侧 ⇒ ⇒ 禁一侧代另一侧）。

#### scenario: reset_defaults

框架复位后：四路 `out*_valid == 0` · `out0_payload == 0` · `in_ready == 0`（6 条）。

#### scenario: payload_follows_select

`in_payload=0xAB` / `in_valid=1` / 四路 `out*_ready=1`；
`select` 依次取 0/1/2/3 ⇒ 仅被选中路 `valid=1` 且 `payload=0xAB`，
未选中路 `valid=0` 且 `payload=0`；`in_ready=1`（23 条）。

#### scenario: backpressure_reaches_upstream

`select=2`、`in_payload=0xCD`、`in_valid=1`、`out2_ready=0`（未选中路 ready 仍为 1）
⇒ ⇒ `in_ready=0`（反压到达上游 + 非选中路隔离）· `out2_valid=1` · `out2_payload=0xCD`；
`out2_ready` 恢复 1 ⇒ ⇒ `in_ready=1`（4 条）。

#### scenario: no_valid_all_outputs_idle

`in_valid=0`（四路 `out*_ready=1`、`select=1`）⇒ ⇒ 四路 `out*_valid == 0` · `in_ready == 1`（5 条）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测 | 抓的是 |
|---|---|---|---|
| M1 | `assign top_out0_valid = 1'b0;`（valid 通路断） | ✅ `fails=1`（`payload_follows_select#0`） | 分发的 valid 语义 |
| M2 | `assign top_in_ready = 1'b1;`（反压方向断：上游恒就绪） | ✅ `fails=2`（`reset_defaults#5` · `backpressure_reaches_upstream#0`） | 消费端 ⇒ 上游 的方向 |
| M3 | `assign top_mux_select_1 = top_in_payload;`（未选中路 payload 广播） | ✅ `fails=1`（`payload_follows_select#5`） | 未选中路 payload MUST 为 0 |

⇒ ⇒ 还原后复跑 ⇒ `RESULT: checks=38 fails=0` ⇒ ⇒ 变异未污染产物。
⚠️ 每条变异 MUST **先确认仍能编译**（三条实测 `compile_rc=0`）⇒ ⇒ 否则跑的是**旧二进制** ⇒ 假绿。

---

## tb_stream_fork_demo {#stream-fork-demo}

### 上游语义（**spec-derived** · capability `chlib-stream-ready-ownership` 的「分流 / 分叉类」条 · ADR-057）

被测单元 = `examples/stream/stream_fork_top.h`（`chlib::stream_fork<ch_uint<8>, 3>` 的具名端口包装）。
`stream_fork` 的契约（组合 0-cycle，无内部状态 · **同步语义**）：

| 端口 | 语义 | 出处 |
|---|---|---|
| `out[i].valid` | `in.valid`（**广播**） | ADR-057 D3（数据面不依赖任何 ready） |
| `out[i].payload` | `in.payload`（**广播**） | ADR-057 D3 |
| `in.ready` | `AND(out[0].ready, out[1].ready, out[2].ready)` | ADR-057 D1（**消费端 ⇒ 上游**） |
| `out[i].ready` | **默认恒就绪 + 消费端 `<<=` 覆盖** | ADR-054 形态 E |

⚠️ **AND 语义 MUST 覆盖两面**（capability 明文要求）：「全部为高 ⇒ 1」（场景 2）
与「**任一**为低 ⇒ 0」（场景 3）⇒ ⇒ 只测一面时无法与「OR」「只看第一路」区分
⇒ ⇒ 场景 3 把**中间那一路**（`out1_ready`）拉低 ⇒ ⇒ 「只看第一路」的实现会红。

⚠️ **只覆盖同步语义**：`synchronous = false` 在建图期**响亮拒绝**
（`CHFATAL_EXCEPTION` · ADR-057 D2）⇒ ⇒ 异步 fork **不在本示例的判据面内**
⇒ ⇒ **如实登记**（不假装覆盖）。

### impl-contract（本实现的取值）

- **框架复位**把全部数据输入置 0（`run_verif` 的 `apply_frame_reset` 与渲染式 TB 的
  `write_tb_reset_pulse` 同构）⇒ ⇒ 复位后 `in_valid=0` / 三路 `out*_ready=0`。
- 组合逻辑 ⇒ ⇒ `SET → SETTLE → EXPECT` 即可观测（**无需** `TICK`）。

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 复位后三路 `out*_valid` | impl-contract | 全 0（`in_valid=0`） |
| 2 | 复位后三路 `out*_payload` | impl-contract | 全 0（`in_payload=0` ⇒ 广播 0） |
| 3 | 复位后 `in.ready` | impl-contract | 0（三路 `out*_ready=0` ⇒ AND=0） |
| 4 | `in_valid=1` 时三路 `out*_valid` | spec-derived | 全 1（**广播**） |
| 5 | `in_payload=0x5A` 时三路 `out*_payload` | spec-derived | 全 `0x5A`（**广播**） |
| 6 | 三路 `out*_ready=1` 时 `in.ready` | spec-derived | 1（**全部为高** ⇒ AND=1） |
| 7 | `out1_ready=0`（其余为 1）时 `in.ready` | spec-derived | **0**（**任一为低** ⇒ AND=0） |
| 8 | `out1_ready=0` 时三路 `out*_valid` / `out*_payload` | spec-derived | 不变（1 / `0x5A`）⇒ ⇒ 数据面不依赖 ready |
| 9 | `out1_ready` 恢复 1 时 `in.ready` | spec-derived | 1 |
| 10 | `in_valid=0` 时三路 `out*_valid` | spec-derived | 全 0 |
| 11 | `in_valid=0` 时 `in.ready` | spec-derived | 1（ready 不依赖 valid） |

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 26** ⇒ ⇒ 三方等式：本行 == `tb_stream_fork_demo.verif` 的
静态展开（`EXPECT` + `WAIT` 计数）== 运行期 `RESULT: checks`。

VERIF-CHECKS: 26

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记 ⇒ ⇒ 禁由生成器
输出回填 ⇒ ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ ⇒ 判据恒绿）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**（粒度 = scenario）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `reset_defaults` | 判据 1 · 2 · 3（impl-contract） |
| `broadcast_payload_and_valid` | 判据 4 · 5 · 6 |
| `ready_is_and_of_outputs` | 判据 7 · 8 · 9 |
| `no_valid_outputs_idle` | 判据 10 · 11 |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个 —— 本设计无 `ch_layer` ⇒ 层次化 = 扁平）。
- **顶层端口表 = 14 根** = 框架 2（`default_clock` · `default_reset`）+ 根自有 **12**
  （输入流 {payload[7:0], valid, ready} + 3 路输出流 × {payload[7:0], valid, ready}）。
- 产物端口名 = `top_<C++ 成员名>`（ADR-075 E1 · 端口全部具名 ⇒ 无 `top_unnamed_*`）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（解释器路径，`tb_stream_fork_demo.cpp` 默认模式）= **26** 条 ⇒ ⇒ `interp_checks = 26`。
- TB 侧（Verilog 产物，`tb_stream_fork_demo.sv`）= **26** 条 ⇒ ⇒ `min_checks = 26`。
- **两侧同源**（同一份 `.verif` 派生）⇒ ⇒ 相等是预期 ⇒ ⇒ 但 MUST **各自登记**
  （护栏 ⑨ 判 C++ 侧、⑥ 判 TB 侧 ⇒ ⇒ 禁一侧代另一侧）。

#### scenario: reset_defaults

框架复位后：三路 `out*_valid == 0` · 三路 `out*_payload == 0` · `in_ready == 0`（7 条）。

#### scenario: broadcast_payload_and_valid

`in_payload=0x5A` / `in_valid=1` / 三路 `out*_ready=1` ⇒ ⇒ 三路 `valid=1` 且
`payload=0x5A`；`in_ready=1`（7 条）。

#### scenario: ready_is_and_of_outputs

`out0_ready=1` · `out1_ready=0` · `out2_ready=1` ⇒ ⇒ `in_ready=0`（任一为低 ⇒ AND=0）·
三路 `valid=1` 且 `payload=0x5A`（数据面不依赖 ready）；`out1_ready` 恢复 1 ⇒ ⇒
`in_ready=1`（8 条）。

#### scenario: no_valid_outputs_idle

`in_valid=0`（三路 `out*_ready=1`）⇒ ⇒ 三路 `out*_valid == 0` · `in_ready == 1`（4 条）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测 | 抓的是 |
|---|---|---|---|
| M1 | `assign top_in_ready = 1'b1;`（AND 断成恒就绪） | ✅ `fails=2`（`reset_defaults#6` · `ready_is_and_of_outputs#0`） | ready 的 AND 语义 |
| M2 | `assign top_out1_valid = 1'b0;`（valid 广播断） | ✅ `fails=2`（`broadcast_payload_and_valid#1` · `ready_is_and_of_outputs#2`） | valid 广播 |
| M3 | `assign top_out1_payload = 8'd0;`（payload 广播断） | ✅ `fails=2`（`broadcast_payload_and_valid#4` · `ready_is_and_of_outputs#5`） | payload 广播 |

⇒ ⇒ 还原后复跑 ⇒ `RESULT: checks=26 fails=0` ⇒ ⇒ 变异未污染产物。
⚠️ 每条变异 MUST **先确认仍能编译**（三条实测 `compile_rc=0`）⇒ ⇒ 否则跑的是**旧二进制** ⇒ 假绿。
