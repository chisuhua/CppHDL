# Design: spi-fsm-fixture-live-logic-2026-10

## 0. 一句话结论

`test_verilator_complex_e2e.cpp` 的 SPI fixture 是一个**从未工作过**的 DUT
（`shift` 恒 0 / `mosi` 恒 0 / 状态卡死 TRANSFER / `STOP` 不可达），
而它的断言**只有 parity** ⇒ 两后端同构 ⇒ 同构缺陷恒被掩盖 ⇒ 长期「假绿」；
本 change 用**正确形态的 5 条纪律**首次实现它，并把断言升级为**手推语义金标**。

## 1. 缺陷机制（三个叠加的根因 —— 全部**实测**确认，非推理）

探针：`/tmp/opencode/probe_spi.cpp`（复现同一份 fixture 形态，interpreter 后端，
逐拍打印 `state` / `bit_count` / `mosi` / `shift`）。

### 1.1 `reg->next` 是 last-wins ⇒ 「IDLE 载入 `data_in`」永不生效

- `regimpl::set_next` = `set_src(0, next)`（`include/ast/ast_nodes.h:47-54`）
  ⇒ 同一寄存器被多次赋值 ⇒ **最后一次覆盖**；
- 原 fixture 在 **IDLE**（`shift->next = select(start_now, io().data_in, shift)`）
  与 **TRANSFER**（`shift->next = shift << 1_d`）**各写一次**
  ⇒ `ch_state_machine::build()`（`include/chlib/state_machine.h:296-301`）
  **按 state index 遍历**执行全部 `on_active` ⇒ TRANSFER（index 2）**后发射**
  ⇒ ⇒ **覆盖** IDLE（index 0）的载入 ⇒ ⇒ `shift` **恒 0**；
- `bit_count->next` 同型（IDLE 的「清零」被 TRANSFER 的「+1」覆盖）
  ⇒ ⇒ `bit_count` **从不 reset**；
- **实测**：`shift` 恒 `0x00`、`bit_count` 从不回到 0。

### 1.2 三元 `? :` 在建图期求值 ⇒ `STOP` 永不可达

- `transition_when(const ch_bool& cond, StateEnum target)`（`state_machine.h:228-231`）
  的 `target` MUST 是**编译期常量**（`build()` 按 state 遍历 `on_active` 注册转移
  ⇒ 注册时就固化 target）；
- 原代码：`sm.transition_when(ch_bool(true), last_bit ? SpiState::STOP
  : SpiState::TRANSFER);`
  ⇒ 三元在**建图期**求值 ⇒ `last_bit` 走 `ch_bool::operator uint64_t()`
  （`src/lnode/bool.cpp:44-49`：**只对 const 节点返真值**）⇒ **恒取 `TRANSFER`**
  ⇒ ⇒ **`STOP` 永不可达** ⇒ 状态**卡死**在 TRANSFER；
- **实测**：状态序列在 TRANSFER 之后**再也不变**。

### 1.3 断言只有 parity ⇒ 同构缺陷恒被掩盖

- 原断言：`REQUIRE(sa == sb)` / `REQUIRE(ba == bb)`（interpreter vs verilator）；
- 两后端由**同一份 C++ fixture**生成 ⇒ 缺陷**同构** ⇒ 两边**同时错** ⇒ parity **恒绿**；
- ⇒ ⇒ 「绿 ≠ 对」。这是 discipline ⑤「不把 parity 当正确性证据」的**教科书案例**。

### 1.4 附带发现：主 spec 与实现**已 drift**（历史欠账）

`tests/test_verilator_complex_e2e.cpp` **历史上已被改过两次**，却**从未**加过
`dual-backend-test-coverage` R9 的例外：

| commit | 改动 | 是否加过 R9 例外 |
|---|---|---|
| `48ed836` | **+140** 行（RV32I fixture 大改） | ❌ |
| `957758f` | **18** 行（退休数金标 17 → 16） | ❌ |

⇒ ⇒ R9「9 个测试 MUST 保留原状」在**事实上**已被违反 ⇒ **主 spec 与实现 drift**
⇒ 本 change 的 R9 delta 是**补登记 + 扩展**（不是新开口子）⇒ 如实登记于此。

## 2. 修复形态（5 条纪律 —— 与 `uart-tx-fsm-live-logic-2026-10` 同型）

| # | 纪律 | 本 fixture 的落实 |
|---|---|---|
| ① | 条件逻辑**只用硬件原语**（`transition_when` / `select`），**禁**一切「硬件值参与 C++ 控制流」（`if` / `? :` / `switch` / `while`-`for` 行程条件 / early `return` / 传给 `bool` 形参） | 全部条件写成 `ch_bool` 硬件节点；**无** `if`、**无**三元 |
| ② | 每寄存器**只有 1 个** `next` 写入点（各状态候选值用 `select` 组合成单一表达式，写在 `sm.build()` **之后**） | `bit_count` / `shift` 各 1 处，写在 `sm.build()` 之后 |
| ③ | 每输出**只驱动 1 次** | `dbg_state` / `mosi_out` / `dbg_bit_count` 各 1 处 |
| ④ | `transition_when` 的条件是**电平敏感**（每拍求值、为真即走）⇒ 「持续 N 拍后转移」MUST 与计数器条件**合取** | 本 fixture 每状态**恰好 1 拍** ⇒ 条件为常量 `ch_bool(true)` 或**互斥**的两条（`last_bit` / `!last_bit`）⇒ 无电平敏感陷阱 |
| ⑤ | **不把 parity 当正确性证据** | 追加**手推**语义金标（状态 / 计数器 / `mosi` 8-bit 序列） |

### 2.1 状态转移（`on_active` 内，**不**碰任何寄存器）

```
IDLE    : transition_when(start_now,        START)
START   : transition_when(true,              TRANSFER)
TRANSFER: transition_when(last_bit,         STOP)      ← 拆成两条（target 必须是编译期常量）
TRANSFER: transition_when(!last_bit,        TRANSFER)
STOP    : transition_when(true,              IDLE)
```

⚠️ `last_bit` 与 `!last_bit` **互斥** ⇒ `build()` 的 select 树「后注册者优先」
（`state_machine.h:303-322`）**不产生歧义**。

### 2.2 寄存器 next（`build()` 之后，各**唯一**写入点）

```
bit_count <= select(in_idle & start_now,        0,
                select(in_transfer & !last_bit,  bit_count + 1,
                       bit_count))
shift     <= select(in_idle & start_now,        data_in,
                select(in_transfer,              shift << 1,
                       shift))
```

⚠️ **`in_transfer & !last_bit`**：最后一个 bit（`bit_count == 7`）**不再 +1**
⇒ `bit_count` 稳定在 7（3-bit ⇒ 不回绕）⇒ 观测面无歧义。

### 2.3 输出（各**唯一**驱动点）

```
dbg_state     = state_reg
mosi_out      = select(in_transfer, shift[7], 0)     ← MSB-first
dbg_bit_count = bit_count
```

### 2.4 实现期用到的框架事实（**实测**，非推理）

| 事实 | 证据 | 后果 |
|---|---|---|
| `port<ch_bool, input_direction>` **无** `ch_bool` 转换 | `include/core/io.h:232`（**只有** `ch_uint` 有） | `start` MUST 经 `ch_reg<ch_bool>` 中转 ⇒ 条件**晚 1 拍** |
| `transition_when` 的 `target` MUST 是**编译期常量** | `include/chlib/state_machine.h:228-231` + `build()` 的注册机制 | **禁**三元选 target |
| `regimpl::set_next` = `set_src(0, next)` | `include/ast/ast_nodes.h:47-54` | 每寄存器**单**写入点 |
| `ch_bool::operator uint64_t()` 只对 const 节点返真值 | `src/lnode/bool.cpp:44-49` | 一切 `if` / 三元在**建图期**恒假 |
| `instr_reg::eval()` 用 `assign_truncate` | `src/ast/instr_reg.cpp:56` | 4-bit 的 `+1` 中间值落进 3-bit 寄存器**安全截断**（⇒ 无宽度歧义） |
| 寄存器 proxy 在**组合表**里 | `src/simulator_init.cpp:139-143`（`default:` ⇒ comb） | tick 结束时 comb 重算 ⇒ 输出反映**边沿后**的寄存器值 ⇒ 采样点 = 「tick 之后」 |
| `add_op::result_width = max(M,N)+1` | `include/lnode/operators_ext.h:13-18` | `bit_count + 1` 是 **4-bit** ⇒ 落进 3-bit 寄存器时被截断（**本设计里恒不溢出**，见 §2.2） |
| `ch_op::shl` 三后端齐备 | interpreter `src/ast/ast_nodes.cpp:130` / Verilog `src/codegen_verilog.cpp:210,799` | `shl<8>(shift, 1_d)` 安全 |

## 3. 语义断言（**手推**金标 —— 不得抄仿真输出）

### 3.1 逐拍时间线（**手推**）

TB：`start` 在 cycle **0** 与 cycle **12** 各拉高一次（⇒ **2 次**传输）；
`data_in` 恒 **`0x96`**。
采样点在**每次 `tick(1)` 之后**（⇒ 反映**边沿后**的寄存器值，见 §2.4）。

| tick | 状态 | `bit_count` | `shift` | `mosi` | 依据 |
|---|---|---|---|---|---|
| 0 | IDLE(0) | 0 | 0x00 | 0 | `start_latch` 本拍才采到 `start=1` ⇒ 条件仍是旧值 0 ⇒ 不转移 |
| 1 | START(1) | 0 | **0x96** | 0 | IDLE 态看到 `start_latch=1` ⇒ 载入 `data_in` ⇒ 下一拍进 START |
| 2 | TRANSFER(2) | 0 | 0x96 | **1** | 进入 TRANSFER 的那一拍 `shift` 已是 0x96 ⇒ `shift[7]`=1 |
| 3 | TRANSFER | 1 | 0x2C | 0 | `0x96 << 1` = 0x12C ⇒ 截断到 8 bit = 0x2C；`0x2C[7]`=0 |
| 4 | TRANSFER | 2 | 0x58 | 0 | `0x2C << 1` = 0x58；`0x58[7]`=0 |
| 5 | TRANSFER | 3 | 0xB0 | 1 | `0x58 << 1` = 0xB0；`0xB0[7]`=1 |
| 6 | TRANSFER | 4 | 0x60 | 0 | `0xB0 << 1` = 0x160 ⇒ 截断 = 0x60；`0x60[7]`=0 |
| 7 | TRANSFER | 5 | 0xC0 | 1 | `0x60 << 1` = 0xC0；`0xC0[7]`=1 |
| 8 | TRANSFER | 6 | 0x80 | 1 | `0xC0 << 1` = 0x180 ⇒ 截断 = 0x80；`0x80[7]`=1 |
| 9 | TRANSFER | 7 | 0x00 | 0 | `0x80 << 1` = 0x100 ⇒ 截断 = 0x00；`0x00[7]`=0 |
| 10 | **STOP(3)** | 7 | 0x00 | 0 | `bit_count==7` ⇒ 本拍转移 ⇒ 离开 TRANSFER ⇒ `mosi` 回 0 |
| 11 | IDLE(0) | 7 | 0x00 | 0 | STOP 态无条件回 IDLE |
| 12 | IDLE(0) | 7 | 0x00 | 0 | **第 2 次** `start` 被 `start_latch` 采到（条件仍用旧值 0 ⇒ 本拍不转移） |
| 13 | START(1) | **0** | 0x96 | 0 | IDLE 态看到 `start_latch=1` ⇒ 装载 ⇒ **计数器复位（此处可观测！）** |
| 14 | TRANSFER(2) | 0 | 0x96 | **1** | 第 2 次传输的第 1 位 |
| 15..21 | TRANSFER | 1..7 | 0x2C..0x00 | 0,0,1,0,1,1,0 | 与第 1 次传输逐拍同构 |
| 22 | **STOP(3)** | 7 | 0x00 | 0 | `bit_count==7` ⇒ 本拍转移 |
| 23..31 | IDLE(0) | 7 | 0x00 | 0 | `start_now`=0 ⇒ 全保持 |

⇒ **每次传输的 8 个 TRANSFER 拍的 `mosi`** = `1,0,0,1,0,1,1,0`
⇒ = **`0x96` 的 MSB-first**（`0x96` = `1001 0110` ⇒ bit7..bit0 = `1,0,0,1,0,1,1,0`）✔
⇒ **2 次传输 ⇒ 16 个 TRANSFER 拍 ⇒ 两段完全相同** ✔

⚠️ **为什么必须做 2 次传输（实测踩到）**：单次传输时「**装载时复位计数器**」
这条契约**不可观测** —— 计数器在装载前本来就是 0 ⇒ 复位与否**无差别**
⇒ ⇒ 变异 **M6（去掉复位）⇒ 全绿**（实测）⇒ ⇒ 该缺陷类**零判别力**
⇒ ⇒ 加一次第 2 次传输 ⇒ 计数器在装载前是 7 ⇒ 复位**可观测**
（tick 13：期望 0）⇒ ⇒ M6 转红。

### 3.2 断言的三层（**判别力是承重件**）

1. **状态序列金标** ⇒ **必过 STOP**（缺陷版恒卡 TRANSFER ⇒ 直接转红）；
2. **`mosi` 8-bit 序列金标** ⇒ `0x96` 的 MSB-first（缺陷版恒 0 ⇒ 直接转红）；
3. **`bit_count` 序列金标** ⇒ 证明**确实**移了 8 次（且**确实**在装载时复位过）。

⚠️ **判别力的前提（实测踩到）**：发送字节 MUST 使 **MSB-first ≠ LSB-first**。
原 spec 写的 `0xA5` = `1010 0101` 是**回文** ⇒ 其 MSB-first 与 LSB-first
**完全相同**（都是 `1,0,1,0,0,1,0,1`）⇒ ⇒ 「位序」这一维度**零判别力**
⇒ 「`mosi` 序列 == 字节的 MSB-first 序列」这条断言**抓不到**位序错
⇒ ⇒ 本 change 把发送字节改为 **`0x96`** = `1001 0110`
（MSB-first = `1,0,0,1,0,1,1,0`，LSB-first = `0,1,1,0,1,0,0,1` ⇒ **不同**）
⇒ ⇒ 这是对 spec 的**收紧**（判别力增强），已写进 `verilator-e2e-coverage` 的
MODIFIED delta。

⚠️ **与 uart 案例的区分**：`uart-tx-fsm-live-logic-2026-10` 用 `0xA5` 是**对的**
—— 那里的判别轴是「**哪一位是起始位**」（off-by-one），不是「位序」
⇒ `0xA5` 在那里有判别力、在**这里**没有 ⇒ **不可**照搬。

### 3.3 parity 断言**保留**（不删）

parity 仍 MUST 断言（两后端**逐拍**一致）⇒ 但它**只是必要条件**，不是充分条件
⇒ ⇒ 两条并存。

## 4. 实测证据

### 4.1 缺陷版（HEAD）的实测输出 —— **只读**探针

探针 `/tmp/opencode/probe_spi.cpp`（复现 HEAD 的 fixture 形态，interpreter 后端）：

| 观测量 | 实测值 | 应有值 |
|---|---|---|
| `shift`（8-bit 移位寄存器） | **恒 `0x00`** | 装载发送字节并逐拍左移 |
| FSM 状态 | **卡死在 `TRANSFER(2)`** | IDLE→START→TRANSFER×8→STOP→IDLE |
| `mosi` | **恒 0** | `1,0,0,1,0,1,1,0`（`0x96` 的 MSB-first） |
| `bit_count` | **从不 reset** | 装载时清零，TRANSFER 逐拍 +1 |

### 4.2 修复版的逐拍输出（探针 `/tmp/opencode/probe_spi_fixed.cpp`）

⚠️ **金标的来源是手推（§3.1）**；此处是**事后核对** ⇒ **逐项吻合**：

```
tick | state    | bit_count | mosi | shift
   0 | IDLE     |         0 |    0 | 0x00
   1 | START    |         0 |    0 | 0x96
   2 | TRANSFER |         0 |    1 | 0x96
   3 | TRANSFER |         1 |    0 | 0x2C
   4 | TRANSFER |         2 |    0 | 0x58
   5 | TRANSFER |         3 |    1 | 0xB0
   6 | TRANSFER |         4 |    0 | 0x60
   7 | TRANSFER |         5 |    1 | 0xC0
   8 | TRANSFER |         6 |    1 | 0x80
   9 | TRANSFER |         7 |    0 | 0x00
  10 | STOP     |         7 |    0 | 0x00
  11 | IDLE     |         7 |    0 | 0x00
  12 | IDLE     |         7 |    0 | 0x00
  13 | START    |         0 |    0 | 0x96      ← 计数器复位（可观测）
  14 | TRANSFER |         0 |    1 | 0x96
  ...
  22 | STOP     |         7 |    0 | 0x00
  23 | IDLE     |         7 |    0 | 0x00
```

### 4.3 ⭐ 「旧 fixture + 新断言 ⇒ 转红」= 假绿被打破的直接证据

脚本 `/tmp/opencode/revert_describe.py`：把当前文件的 `describe()` 换回
**HEAD（旧、坏）**版本，但**保留**新的 TB（2 次传输 + `0x96`）与新的语义断言。

| 组合 | 结果 |
|---|---|
| 旧 fixture + **旧断言**（仅 parity） | **绿**（长期「假绿」） |
| 旧 fixture + **新断言**（parity + 金标） | **红** —— 首个失败 = `count_seq[0]`：**1 ≠ 0**（`REQUIRE` 在首个失败处中止 ⇒ 只显示 1 个失败） |
| 新 fixture + 新断言 | **绿** |

⇒ ⇒ 「同构缺陷恒被 parity 掩盖」这条纪律被**实证**。

### 4.4 变异验证 **9/9 全被抓**（每条先确认**编译成功** ⇒ 无假绿）

注入器 `/tmp/opencode/mutate_spi.py`（**fail-closed**：行号越界 / 目标子串不匹配
⇒ `MUTATE_FAIL` + exit 2）；驱动 `/tmp/opencode/run_spi_mutations.sh`
（每条：注入 → 编译（**必须 0 error**）→ 运行（**必须红**）→ 复原）。

| # | 变异 | 抓到的断言 |
|---|---|---|
| M1 | 装载条件的状态写错（IDLE → START）⇒ 装载永不发生 | `mosi_seq` |
| M2 | **恢复三元选 target**（`last_bit ? STOP : TRANSFER`）⇒ **原缺陷形态** | `state_seq` |
| M3 | `STOP` 的条件恒假 ⇒ `STOP` 不可达 | `state_seq` |
| M4 | 位序反（`shift[7]` → `shift[0]`，= LSB-first） | `mosi_seq`（**只因 `0x96` 非回文才转红**） |
| M5 | 移位量 1 → 2 | `mosi_seq` |
| M6 | 装载时**不复位**计数器 | `count_seq`（**只因做 2 次传输才转红**） |
| M7 | `last_bit` 判据 7 → 6 ⇒ 少发一位 | `state_seq` |
| M8 | `mosi` 的非 TRANSFER 分支改成 MSB ⇒ 输出门控缺失 | `mosi_seq` |
| M9 | 去掉 `& !last_bit` ⇒ 计数器在最后一位也 +1（回绕） | `count_seq` |

⇒ ⇒ **9/9 全被抓**，每条编译错误数 = **0**。

⚠️ **M6 的实测过程（如实登记）**：第一版只做 **1 次传输** ⇒ M6 **全绿**
⇒ ⇒ 定位到「单次传输下『装载时复位计数器』不可观测」⇒ ⇒ **加第 2 次传输**
⇒ ⇒ M6 转红。⚠️ 这正是「绿 ≠ 对」的另一个实例：**测试全绿**（连同 8 条变异中的
8 条）**不代表**断言有判别力。

### 4.5 生成的 Verilog（结构性硬证据）

（`/tmp/cpphdl_vl_cx_76Zg6B_cx/top.v`，由 `VerilatorBackend` 生成）

| 纪律 | 证据 |
|---|---|
| ② 每寄存器**单** next 写入点 | `top_reg <= top_mux_select_5;` · `top_shift <= top_mux_select_9;` · `top_bit_count <= top_mux_select_7;` · `top_start_latch <= top_unnamed_input;` —— **全部由 mux / 输入驱动** ⇒ **无自环**（缺陷版是 `q <= q`） |
| ③ 每输出**只**驱动一次 | `assign top_unnamed_output = top_reg;` · `assign top_unnamed_output_1 = top_mux_select;` · `assign top_unnamed_output_2 = top_bit_count;` ⇒ **各 1 条** |
| ① `is_in` 比较节点**有消费者** | `eq_eq_1..8` 各出现 **5–7** 次（⇒ 不是死节点；缺陷版为零消费者） |

### 4.6 验收

（见 `tasks.md` §4；验收数字填于该文件）

## 5. 顺带订正与如实登记

- **§1.4 的历史 drift**：本文件此前两次改动**未**加 R9 例外 ⇒ 本 delta 补登记。
- **框架层缺口（独立立项，planner 议题）**：
  ① `ch_reg` 只能有**一个** next 源（`regimpl::set_next` 是 `set_src(0,…)`）
  ⇒ 「每状态各写一次」**无法**表达多状态 FSM；
  ② `ch_state_machine::build()` **遍历所有 state** 执行各自 `on_active`
  ⇒ 任何写在 `on_active` 内的 `reg->next` 赋值都会被后执行者覆盖
  ⇒ ⇒ 框架**本身**鼓励的写法（`on_active` 内写寄存器）在多状态下**必然**出错
  ⇒ ⇒ 本 change **只**在 fixture 侧规避，**不**改框架语义。
- **`uart-tx-fsm-live-logic-2026-10` 的「正确形态范例」说法已被订正**
  （上一 commit 的 `AGENTS.md` NOTES 已记）⇒ 本 change 是那笔订正的**兑现**。

## 6. 范围外（**明确不做**）

- **不**改 `ch_state_machine` / `ch_reg` 的框架 API（⇒ 独立立项）；
- **不**改 `examples/spinalhdl-ported/spi/spi_controller.cpp`；
- **不**新增 Catch2 测试文件（⇒ 计数锚点不变）；
- **不**扩 `example-assertion-quality` 的适用范围（⇒ planner 议题）。
