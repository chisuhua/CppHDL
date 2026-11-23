---
evidence: probe:scripts/verify_examples_rtl_sim.sh
---

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

## tb_counter_simple_example {#counter-simple-example}

### 上游参考（**spec-derived**）

来源：SpinalHDL 原示例（设计定义现为 `counter/counter_simple_top.h` 头部逐字转录的 Scala）：

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

| 拍（**回绕点之后**） | 期望 `value` |
|---|---|
| 0（回绕点） | 0 |
| 1 | 1 |
| 2 | 2 |
| 3 | 3 |
| N（1..255） | N |

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 回绕存在 | spec-derived | 300 拍内 `value == 0`（`WAIT low`） |
| 2 | 逐拍绝对计数 | spec-derived | 回绕点后第 N 拍 `value == N`（N = 1..255） |
| 3 | metamorphic：**周期 256** | spec-derived | 任意 512 个连续采样里 `v[k] == v[k-256]`（256 对） |
| 4 | 步进恒为 1 | spec-derived | **由判据 2 覆盖**（无 enable 端口 ⇒ `EXPECT_SEQ_STEP` 不适用） |

⚠️ **相位无关锚定**：本设计**没有输入端口**可锚，而框架复位的末尾 `@(posedge)` 已让
自由运行计数器走了一拍（场景入口 `value == 1`，两侧同源）⇒ ⇒ 绝对判据 MUST 先
`WAIT value low 300` 锚到**回绕点**（= 0），此后段长确定。

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 258** ⇒ ⇒ 三方等式：本行 == `tb_counter_simple_example.verif`
的静态展开（`EXPECT` + `WAIT` 计数）== 运行期 `RESULT: checks`。

VERIF-CHECKS: 258

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记 ⇒ ⇒ 禁由生成器
输出回填 ⇒ ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ ⇒ 判据恒绿）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**（粒度 = scenario）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `absolute_count` | 判据 1 · 2 · 4 |
| `period_256` | 判据 3（metamorphic） |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个 —— 本设计无 `ch_layer` ⇒ 层次化 = 扁平）。
- **顶层端口表 = 3 根** = 框架 2（`default_clock` · `default_reset`）+ 根自有 **1**（`value[7:0]`）。
- 产物端口名 = `top_<C++ 成员名>`（ADR-075 E1 · 端口全部具名 ⇒ 无 `top_unnamed_*`）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（解释器路径，`tb_counter_simple_example.cpp` 默认模式）= **258** 条
  ⇒ ⇒ `interp_checks = 258`。
- TB 侧（Verilog 产物，`tb_counter_simple_example.sv`）= **258** 条 ⇒ ⇒ `min_checks = 258`。
- **两侧同源**（同一份 `.verif` 派生）⇒ ⇒ 相等是预期 ⇒ ⇒ 但 MUST **各自登记**
  （护栏 ⑨ 判 C++ 侧、⑥ 判 TB 侧 ⇒ ⇒ 禁一侧代另一侧）。

#### scenario: absolute_count

先 `WAIT value low 300` 锚到回绕点（1 条），随后逐拍断言 `value == N`（N = 1..255，255 条），
共 257 条。

#### scenario: period_256

连续 512 个采样 ⇒ `EXPECT_SEQ_SELF value 256 256`（1 条，覆盖 256 对相邻周期比较）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测（`RESULT: checks=258 fails=N`） | 抓的是 |
|---|---|---|---|
| M1 | 自增改 `+0`（计数器卡住） | ✅ `fails=255`（`absolute_count#10..#255`） | 逐拍绝对计数 |
| M2 | 自增改 `+2`（步长 2） | ✅ `fails=255`（`absolute_count#…`） | 逐拍绝对计数 |

⚠️ **M2 暴露的已知局限（登记不修）**：`EXPECT_SEQ_SELF value 256 256` **抓不到**
「步长 2」（周期 128 **整除** 256 ⇒ `v[k] == v[k-256]` 仍成立）⇒ ⇒ 该性质由**绝对判据**
承担。原手写 TB 的周期判据有**同一局限**（未降级）。

---

## tb_counter_example {#counter-example}

### 上游参考（**spec-derived**）

Scala 原文（设计定义现为 `counter/counter_top.h` 头部）：

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

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 复位初值 | impl-contract | 0 |
| 2 | `clear=1 ∧ enable=1` | spec-derived | 0（**clear 胜**） |
| 3 | `clear=0 ∧ enable=1` | spec-derived | 逐拍 +1 |
| 4 | `enable=0 ∧ clear=0` | spec-derived | 保持 |
| 5 | 溢出回绕 | spec-derived | 255 + 1 == 0 |
| 6 | metamorphic：**静止性** | spec-derived | `enable=0 ∧ clear=0` 的 63 对相邻项全等 |
| 7 | metamorphic：**Δ == enable** | spec-derived | 40 对相邻差逐对等于**较早端**的 `enable` |

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 9** ⇒ ⇒ 三方等式：本行 == `tb_counter_example.verif` 的
静态展开（`EXPECT` + `WAIT` 计数）== 运行期 `RESULT: checks`。

VERIF-CHECKS: 9

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记 ⇒ ⇒ 禁由生成器
输出回填 ⇒ ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ ⇒ 判据恒绿）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**（粒度 = scenario）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `reset_value` | 判据 1（impl-contract） |
| `clear_priority` | 判据 2 · 3 · 4 |
| `wrap_255_to_zero` | 判据 5 |
| `hold_while_disabled` | 判据 6（metamorphic） |
| `step_equals_enable` | 判据 7（metamorphic） |

⚠️ **`EXPECT_SEQ_STEP` 的同采样点前提**（ADR-076 D5）：每拍 `RECORD value` →
`SET enable <下一拍的使能>` → `SETTLE` → `RECORD enable` ⇒ ⇒ 采样点 k 的 `enable`
是**下一拍**的使能 ⇒ 与「ΔV 由**较早端** E 造成」的对齐一致。

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个 —— 本设计无 `ch_layer` ⇒ 层次化 = 扁平）。
- **顶层端口表 = 5 根** = 框架 2 + 根自有 **3**（`enable` · `clear` · `value[7:0]`）。
- 产物端口名 = `top_<C++ 成员名>`（ADR-075 E1 · 端口全部具名 ⇒ 无 `top_unnamed_*`）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（解释器路径，`tb_counter_example.cpp` 默认模式）= **9** 条 ⇒ ⇒ `interp_checks = 9`。
- TB 侧（Verilog 产物，`tb_counter_example.sv`）= **9** 条 ⇒ ⇒ `min_checks = 9`。
- **两侧同源**（同一份 `.verif` 派生）⇒ ⇒ 相等是预期 ⇒ ⇒ 但 MUST **各自登记**。

#### scenario: reset_value

框架复位后 `value == 0`（1 条）。

#### scenario: clear_priority

`enable=1 ∧ clear=1` ⇒ 0；`clear=0` ⇒ 1、2；`enable=0` 两拍 ⇒ 仍 2（4 条）。

#### scenario: wrap_255_to_zero

clear 后连续 255 拍 +1 ⇒ 255；再一拍 ⇒ 0（2 条）。

#### scenario: hold_while_disabled

`enable=0 ∧ clear=0` 连续 64 个采样 ⇒ 63 对相邻项全等（1 条）。

#### scenario: step_equals_enable

确定性伪随机使能（`(_i*17+3)` 的第 4 位）⇒ 41 个采样点 / 40 对相邻差逐对等于
较早端 `enable`（1 条）。

### ✅ 历史结案（D4）与迁移后的状态

- **D4（2026-11-12 结案）**：旧 `counter.cpp` 的注释自称「enable/clear may not fully work yet」
  ⇒ ⇒ 实测（interpreter 与独立 RTL **两侧一致**）**四项全对** ⇒ ⇒ 那是**过时注释**
  ⇒ ⇒ 不是缺陷。
- **迁移后的状态**：C++ 侧不再是「无断言」（旧登记项担忧的分叉**已消失**）——
  本行两侧由**同一份 `.verif`** 派生 ⇒ ⇒ 判据同源。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测（`RESULT: checks=9 fails=N`） | 抓的是 |
|---|---|---|---|
| M1 | clear 优先级被去掉（`clear` 不再压过 `enable`） | ✅ `fails=6`（`clear_priority#0..#3` · `wrap_255_to_zero#0/#1`） | 判据 2 / 5 |
| M2 | enable 门控被去掉（计数恒 +1） | ✅ `fails=4`（`reset_value#0` · `clear_priority#3` · `hold_while_disabled#0` · `step_equals_enable#0`） | 判据 4 / 6 / 7 |
| M3 | 自增改 `+2`（步长 2） | ✅ `fails=5`（`clear_priority#1..#3` · `wrap_255_to_zero#0` · `step_equals_enable#0`） | 判据 3 / 7（**相对判据**抓步长） |

---

## tb_pwm_example {#pwm-example}

### 上游参考（**spec-derived**）

Scala 原文（原 `pwm.cpp` 头部；设计定义现为 `pwm_top.h`）：

```scala
val counter = Reg(UInt(8 bits)) init(0)
counter := counter + 1
io.pwm = (counter < io.value) & io.enable
```

⇒ 语义：**`pwm = enable ∧ (counter < duty)`**；counter 自由运行（模 256）。

### 输入序列 → 期望输出

| enable | duty | counter | 期望 `pwm_out` |
|---|---|---|---|
| 1 | 4 | 0..3 | 1（**连续恰好 4 拍**） |
| 1 | 4 | 4..255 | 0 |
| 0 | 任意 | 任意 | **0** |
| 1 | 0 | 任意 | **0**（`counter < 0` 恒假） |
| 1 | 255 | 0..254 | 1（**256 拍里恰好 255 拍高**） |

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | duty=4 的高电平段长度 | spec-derived | **恰好 4 拍**（上升沿后连续 4 拍高，第 5 拍低） |
| 2 | `enable=0` 时输出 | spec-derived | 恒 0（连续 8 拍 + **整周期 256 拍计数 == 0**） |
| 3 | `duty=0` 时输出 | spec-derived | 恒 0（连续 8 拍 + **整周期 256 拍计数 == 0**） |
| 4 | metamorphic：duty=200 的周期性 | spec-derived | 任意连续 256 拍里高电平拍数 == **200** |
| 5 | metamorphic：duty=255 的周期性 | spec-derived | 任意连续 256 拍里高电平拍数 == **255** |

⚠️ **相位无关性**：counter 自由运行 ⇒ ⇒ 「复位后第 N 拍」的绝对期望**不可移植**
⇒ ⇒ 场景 1 先 `WAIT low` 再 `WAIT high` 锚到**上升沿**（`WAIT` 判的是**电平** ⇒ ⇒
单条 `WAIT high` 在起跑已为高时会立刻命中 ⇒ ⇒ 段长判据不成立），场景 4/5 用
**任意 256 拍窗口**（counter 必取遍 0..255）⇒ ⇒ 两条都不依赖内部相位。

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 29** ⇒ ⇒ 三方等式：本行 == `tb_pwm_example.verif` 的
静态展开（`EXPECT` + `WAIT` 计数）== 运行期 `RESULT: checks`。

VERIF-CHECKS: 29

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记 ⇒ ⇒ 禁由生成器
输出回填 ⇒ ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ ⇒ 判据恒绿）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**（粒度 = scenario）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `duty_run_length` | 判据 1 |
| `enable_zero` | 判据 2 |
| `duty_zero` | 判据 3 |
| `duty_ratio` | 判据 4（metamorphic） |
| `duty_max_ratio` | 判据 5（metamorphic） |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个 —— 本设计无 `ch_layer` ⇒ 层次化 = 扁平）。
- **顶层端口表 = 5 根** = 框架 2（`default_clock` · `default_reset`）+ 根自有 **3**
  （`enable` · `duty_cycle[7:0]` · `pwm_out`）。
- 产物端口名 = `top_<C++ 成员名>`（ADR-075 E1 · 端口全部具名 ⇒ 无 `top_unnamed_*`）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（解释器路径，`tb_pwm_example.cpp` 默认模式）= **29** 条 ⇒ ⇒ `interp_checks = 29`。
- TB 侧（Verilog 产物，`tb_pwm_example.sv`）= **29** 条 ⇒ ⇒ `min_checks = 29`。
- **两侧同源**（同一份 `.verif` 派生）⇒ ⇒ 相等是预期 ⇒ ⇒ 但 MUST **各自登记**
  （护栏 ⑨ 判 C++ 侧、⑥ 判 TB 侧 ⇒ ⇒ 禁一侧代另一侧）。

#### scenario: duty_run_length

`enable=1` / `duty=4`：先等低段再等上升沿（相位无关锚定），随后
`pwm_out=1` 保持 3 拍（counter 1,2,3），第 5 拍（counter=4）转低（7 条）。

#### scenario: enable_zero

`enable=0`：输出恒 0 —— 连续 8 拍 + 整周期 256 拍计数 == 0（10 条）。

#### scenario: duty_zero

`duty=0`：输出恒 0 —— 连续 8 拍 + 整周期 256 拍计数 == 0（10 条）。

#### scenario: duty_ratio

**metamorphic**：`duty=200`，连续 256 拍里高电平拍数 == 200（1 条）。

#### scenario: duty_max_ratio

**metamorphic**：`duty=255`，连续 256 拍里高电平拍数 == 255（1 条）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测（`RESULT: checks=29 fails=N`） | 抓的是 |
|---|---|---|---|
| M1 | `assign top_pwm_inst_lt_lt = 1'b0;`（去掉 `counter < duty` ⇒ 输出恒低） | ✅ `fails=4`（`duty_run_length#1` timeout · `#2` · `duty_ratio#0` · `duty_max_ratio#0`） | 「输出会变高」+ 两条 metamorphic |
| M2 | `mux_select = enable ? 1'b1 : bool_lit`（enable 直通 ⇒ 失去占空比调制） | ✅ `fails=6`（`duty_run_length#0` timeout · `#4` · `duty_zero#0` · `duty_zero#2` · `duty_ratio#0` · `duty_max_ratio#0`） | 占空比调制（段长 / 恒低 / 两条 metamorphic） |
| M3 | 比较写成 `<=`（段长 +1） | ✅ `fails=4`（`duty_run_length#4` · `duty_zero#2` · `duty_ratio#0` · `duty_max_ratio#0`） | 段长**等式**（不是「存在高电平」）+ 整周期计数 |

⚠️ **M3 的实测教训**：场景 3 初版只有「连续 8 拍恒 0」⇒ ⇒ M3 **漏网**（8 拍窗口未覆盖
`counter == 0` 那一拍）⇒ ⇒ 已补**整周期 256 拍计数 == 0**（任意 256 拍必然覆盖全部
counter 相位）⇒ ⇒ M3 现在抓在 `duty_zero#2`。**8 拍窗口单独不够**，这不是风格问题。
⚠️ **每条变异 MUST 先确认仍能编译**（三条实测 `compile_rc=0`）⇒ ⇒ 否则跑的是**旧二进制** ⇒ 假绿。

⇒ ⇒ 还原后复跑 ⇒ `RESULT: checks=29 fails=0` ⇒ ⇒ 变异未污染产物。
⚠️ 每条变异 MUST **先确认仍能编译** ⇒ ⇒ 否则跑的是**旧二进制** ⇒ 假绿。

---

## tb_gpio_example {#gpio-example}

### ✅ D1 裁决（2026-11-12 · 用户「按你的建议推进」）：判为「**移植简化**」

⇒ ⇒ **不追平**上游 Scala 的逐位边沿语义 ⇒ ⇒ `spec_checks` **保持 0**（只判 impl-contract）。
上游参考（`gpio.cpp` 头部的 Scala）与本实现的差异：1 级 vs 2 级同步 · 单 bit vs 逐位边沿 ·
端口集不同。

### impl-contract（本实现的取值）

| 项 | 契约 |
|---|---|
| 同步 | `sync_stage1/2` 两级（INIT 0） |
| 边沿 | `rise = gpio_in & ~sync2` · `fall = ~gpio_in & sync2` ⇒ 归约成 **1 位** |
| `interrupt` | `interrupt_enable ? (edge_detect != 0) : 0` ⇒ **组合**（边沿当拍可见） |
| `gpio_out` | `output_enable ? output_reg : 0`，`output_reg->next = gpio_in` ⇒ **1 拍延迟** |

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 复位态 | impl-contract | `gpio_out=0` · `interrupt=0` |
| 2 | 输出 = 输入寄存 1 拍 | impl-contract | 改输入后第一拍输出仍是旧值，第二拍才跟随 |
| 3 | 输出使能=0 | impl-contract | `gpio_out` 恒 0（与输入无关） |
| 4 | 上升沿 ⇒ 中断 | impl-contract | 边沿当拍 `interrupt=1`；同步追上后回 0 |
| 5 | 中断使能=0 | impl-contract | 有边沿也 `interrupt=0` |
| 6 | **metamorphic**：输入恒定 ⇒ 中断恒 0 | impl-contract | 8 个连续采样全 0 |

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 11** ⇒ ⇒ 三方等式。

VERIF-CHECKS: 11

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `reset_state` | 判据 1 |
| `output_is_delayed_input` | 判据 2 |
| `output_disabled_is_zero` | 判据 3 |
| `interrupt_on_edge` | 判据 4 |
| `interrupt_disabled_is_zero` | 判据 5 |
| `steady_input_no_interrupt` | 判据 6（metamorphic） |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个）。
- **顶层端口表 = 7 根** = 框架 2 + 根自有 **5**（`gpio_in[7:0]` · `output_enable` ·
  `interrupt_enable` · `gpio_out[7:0]` · `interrupt`）。
- 产物端口名 = `top_<C++ 成员名>`（端口全部具名 ⇒ 无 `top_unnamed_*`）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（`tb_gpio_example.cpp` 默认模式）= **11** 条 ⇒ ⇒ `interp_checks = 11`。
- TB 侧（`tb_gpio_example.sv`）= **11** 条 ⇒ ⇒ `min_checks = 11`。

#### scenario: reset_state

`gpio_out==0` · `interrupt==0`（2 条）。

#### scenario: output_is_delayed_input

`output_enable=1`：改输入后的**第一拍**输出仍是旧值，第二拍才跟随（3 条）⇒ ⇒ 1 拍延迟。

#### scenario: output_disabled_is_zero

`output_enable=0` + 输入 0xFF ⇒ 输出 0（1 条）。

#### scenario: interrupt_on_edge

先等同步追上（0 干扰），再把输入 0→1 ⇒ 边沿当拍 `interrupt=1`；3 拍后回 0（3 条）。

#### scenario: interrupt_disabled_is_zero

`interrupt_enable=0` + 输入 0→1 ⇒ `interrupt=0`（1 条）。

#### scenario: steady_input_no_interrupt

输入恒定 0x5A ⇒ 8 个连续采样的 `interrupt` 全 0（1 条）⇒ ⇒ **metamorphic**（自反性）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测（`RESULT: checks=11 fails=N`） | 抓的是 |
|---|---|---|---|
| M1 | `interrupt` 恒 0 | ✅ `fails=1`（`interrupt_on_edge#1`） | 判据 4 |
| M2 | 中断**忽略使能**（直连 `edge_detect`） | ✅ `fails=1`（`interrupt_disabled_is_zero#0`） | 判据 5 |
| M3 | 输出**不寄存**（直连输入） | ✅ `fails=2`（`output_is_delayed_input#0` · `output_disabled_is_zero#0`） | 判据 2 / 3 |

---

## tb_quadrature_encoder_example {#quadrature-encoder-example}

### 上游参考（**spec-derived** · 正交编码 4 倍频）

| 项 | 契约 |
|---|---|
| 同步 | A/B/Z 各两级（**INIT = true**，Z 的 INIT = false） |
| 计数 | A/B 的**每个边沿**都计数 ⇒ 一个正交周期 = **4 计数** |
| 方向 | `direction = a_sync2 ^ b_sync2`（1 = CW） |
| 索引 | `enc_z` 上升沿 ⇒ `position <= 0` |

### impl-contract（本实现的取值）

- 同步延迟：输入变更后 **2 拍**到 `a_sync2/b_sync2`；边沿检测在 `a_sync1 vs a_sync2`。
- **确定性起点**：框架复位把 A/B tie-off 为 0 ⇒ 同步器跟着走 ⇒ 位置计数**不是 0**
  ⇒ ⇒ 每个场景先用 **Z 上升沿**清零、并把 A/B 置到空闲态 `(1,1)`。

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | Z 清零后的初态 | spec + impl | `position=0` · `a_sync=1` · `b_sync=1` |
| 2 | **正向 4 倍频** | spec-derived | 11→10→00→01→11 ⇒ **+4** |
| 3 | **metamorphic**：往返恒等 | spec-derived | 再走反序列（11→01→00→10→11）⇒ **回 0** |
| 4 | 方向指示 | spec-derived | `direction = a_sync2 ^ b_sync2`（相位差半周期） |
| 5 | Z 上升沿清零 | spec-derived | `position` 变 0 并保持 0 |

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 15** ⇒ ⇒ 三方等式。

VERIF-CHECKS: 15

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `reset_state` | 判据 1 |
| `forward_4x` | 判据 2 |
| `reverse_returns_to_origin` | 判据 3（metamorphic） |
| `direction_follows_phase` | 判据 4 |
| `z_clears_position` | 判据 5 |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个）。
- **顶层端口表 = 9 根** = 框架 2 + 根自有 **7**（`enc_a` · `enc_b` · `enc_z` ·
  `position[15:0]` · `direction` · `a_sync` · `b_sync`）。
- 产物端口名 = `top_<C++ 成员名>`（端口全部具名）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（`tb_quadrature_encoder_example.cpp` 默认模式）= **15** 条 ⇒ ⇒ `interp_checks = 15`。
- TB 侧（`tb_quadrature_encoder_example.sv`）= **15** 条 ⇒ ⇒ `min_checks = 15`。

#### scenario: reset_state

Z 上升沿清零后：`position==0` · `a_sync==1` · `b_sync==1`（3 条）。

#### scenario: forward_4x

11→10→00→01→11 ⇒ `position==4`（2 条：起点 0 + 终点 4）。

#### scenario: reverse_returns_to_origin

先正向一个周期（4），再反向一个周期 ⇒ 回 0（3 条）⇒ ⇒ **metamorphic**（往返恒等）。

#### scenario: direction_follows_phase

11（0）→ 10（1）→ 00（0）⇒ `direction` 跟随相位差（3 条）。

#### scenario: z_clears_position

先走 2 个边沿（计数非 0），再打 Z 上升沿 ⇒ `position==0` 且保持（2 条）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测（`RESULT: checks=15 fails=N`） | 抓的是 |
|---|---|---|---|
| M1 | `count_up` 恒 0（加法通路断） | ✅ `fails=3`（`forward_4x#1` · `reverse_returns_to_origin#1/#2`） | 判据 2 / 3 |
| M2 | Z 清零失效 | ✅ `fails=10`（含 `reset_state#0` —— 起点不再确定） | 判据 1 / 5 |
| M3 | `direction` 取反 | ✅ `fails=3`（`direction_follows_phase#1/#2/#3`） | 判据 4 |

---

## tb_fifo_stream_example {#fifo-stream-example}

### 上游参考（**spec-derived** · `samples/fifo_example.cpp` 的经典方案）

| 项 | 契约 |
|---|---|
| 深度 | **16**（`addr_width = 4`） |
| `empty` | 读指针 == 写指针（**全部位**相等） |
| `full` | **地址位相等 + wrap 位不同**（⇒ 在途 16 ⇒ `full=1`） |
| `dout` | `mem.aread(rd)` ⇒ **0 拍组合读**（FWFT：非空即可见队首） |
| 顺序 | 出队顺序 == 入队顺序（FIFO 定义） |

### impl-contract（本实现的取值）

- `empty` / `full` 的复位值 = 1 / 0（指针归零 ⇒ 空）。
- 组合读 ⇒ `SET`/`TICK` 之后 MUST `SETTLE` 再 `EXPECT`。

### ✅ D5 回归（2026-11-12 修）

旧写法 `full = (wr + 1 == rd)` 在**完整指针**空间比较 ⇒ 门限 31 ⇒ 第 17 次写入起地址混叠
（最老元素被覆盖丢失）。现改为经典方案 ⇒ 判据 = 「在途 15 ⇒ full=0 · 在途 16 ⇒ full=1」。

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 复位后 `empty` / `full` | impl-contract | 1 / 0 |
| 2 | 顺序保持（3 元素） | spec-derived | 出队顺序 == 入队顺序 |
| 3 | **metamorphic**：5 元素序列恒等 | spec-derived | 出队序列逐项等于入队序列 |
| 4 | 容量边界 | spec-derived | 在途 15 ⇒ `full=0` · 16 ⇒ `full=1` · 取回 16 ⇒ `empty=1` |

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 20** ⇒ ⇒ 三方等式。

VERIF-CHECKS: 20

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `reset_empty` | 判据 1 |
| `order_preserved` | 判据 2 |
| `sequence_identity` | 判据 3（metamorphic） |
| `capacity_16` | 判据 4 |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个）。
- **顶层端口表 = 8 根** = 框架 2 + 根自有 **6**（`din[7:0]` · `push` · `pop` · `dout[7:0]` · `empty` · `full`）。
- 产物端口名 = `top_<C++ 成员名>`（端口全部具名 ⇒ 无 `top_unnamed_*`）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（`tb_fifo_stream_example.cpp` 默认模式）= **20** 条 ⇒ ⇒ `interp_checks = 20`。
- TB 侧（`tb_fifo_stream_example.sv`）= **20** 条 ⇒ ⇒ `min_checks = 20`。

#### scenario: reset_empty

复位后 `empty==1` · `full==0`（2 条）。

#### scenario: order_preserved

入队 0x11/0x22/0x33 ⇒ 出队逐个相等 + 空标志（6 条）。

#### scenario: sequence_identity

入队 0xA1/0xB2/0xC3/0xD4/0xE5 ⇒ 出队序列逐项恒等（6 条）⇒ ⇒ **metamorphic**。

#### scenario: capacity_16

在途 15 ⇒ `full=0`；在途 16 ⇒ `full=1`、`empty=0`；取回 15 ⇒ `empty=0` + 队首值；
取回第 16 ⇒ `empty=1`（6 条）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测（`RESULT: checks=20 fails=N`） | 抓的是 |
|---|---|---|---|
| M1 | `full` 恒 0 | ✅ `fails=1`（`capacity_16#1`） | 判据 4 |
| M1b | **重新注入 D5 旧公式** `full = (wr+1 == rd)` | ✅ `fails=1`（`capacity_16#1`，门限 31 ⇒ 永不置位） | 判据 4（**D5 回归承重**） |
| M2 | `pop` 不推进读指针 | ✅ `fails=11`（`order_preserved#2/#3/#4` · `sequence_identity#0` 等） | 判据 2 / 3 / 4 |
| M3 | `push` 不推进写指针 | ✅ `fails=14`（`order_preserved#0/#1/#2` · `capacity_16#4/#5` 等） | 判据 2 / 3 / 4 |

---

## tb_uart_tx_example {#uart-tx-example}

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

- **位宽 = 434 拍**（`baud_counter == 433` ⇒ 0..433 共 434 拍）。
- `busy`：START + DATA 期间为 **1**，IDLE / STOP 期间为 **0**。
- `frame_start` 需**恰好 1 拍脉冲**（内部 `start_latch` ⇒ 1 拍延迟）。

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 空闲期 `uart_tx` | spec-derived | 恒高（16 个连续采样全 1） |
| 2 | 空闲期 `busy` | impl-contract | 0 |
| 3 | 起始位取值 | spec-derived | **低**（帧首） |
| 4 | 8 个数据位解码 | spec-derived（**LSB first**） | 回驱动字节（0x55 / 0x5A） |
| 5 | 停止位取值 | spec-derived | **高** |
| 6 | 每个位窗**首/尾两拍**的取值 | spec-derived + impl-contract 434 | 位窗 `[S+434k, S+434k+433]` 首尾同值且等于该位 |
| 7 | `busy` 覆盖范围 | impl-contract | 起始位期间 = 1 · 停止位期间 = 0 |

⚠️ **方法与旧手写 TB 的差异（已登记）**：旧 TB 用**边沿时间戳**「测出」位宽再断言
「9 个可测段等宽」；`.verif` 原语集**没有时间戳**（只有值与历史）⇒ ⇒ 改为
**用 impl-contract 434 锚定位窗 + 每窗首/尾两拍各断言一次**。
判别力**不降**（每位被钉在恰好 434；任何一位多/少一拍都会让某窗首或尾取值错），
代价是 **434 变成必须与实现同步的常量**（改位宽 ⇒ 同步改 `.verif`）。

⚠️ **停止位的宽度不可观测**（STOP 与 IDLE 的 `uart_tx` 都是 1 ⇒ 无边沿）⇒ ⇒ 只判取值。

⚠️ **两帧用不同字节是刻意的**：0x55（交替位 ⇒ 每位边界都有跳变）· 0x5A（含相邻同值位
⇒ 抓位序 / LSB-first 错）。

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 49** ⇒ ⇒ 三方等式：本行 == `tb_uart_tx_example.verif` 的
静态展开（`EXPECT` + `WAIT` 计数）== 运行期 `RESULT: checks`。

VERIF-CHECKS: 49

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记 ⇒ ⇒ 禁由生成器
输出回填）。⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `idle_line` | 判据 1 · 2 |
| `send_0x55` | 判据 3 · 4 · 5 · 6 · 7 |
| `send_0x5A` | 判据 3 · 4 · 5 · 6 · 7（不同字节 ⇒ 抓位序） |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个 —— 本设计无 `ch_layer` ⇒ 层次化 = 扁平）。
- **顶层端口表 = 6 根** = 框架 2 + 根自有 **4**（`frame_start` · `data[7:0]` ·
  `uart_tx` · `busy`）。
- 产物端口名 = `top_<C++ 成员名>`（ADR-075 E1 · 端口全部具名 ⇒ 无 `top_unnamed_*`）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（`tb_uart_tx_example.cpp` 默认模式）= **49** 条 ⇒ ⇒ `interp_checks = 49`。
- TB 侧（`tb_uart_tx_example.sv`）= **49** 条 ⇒ ⇒ `min_checks = 49`。
- **两侧同源**（同一份 `.verif`）⇒ ⇒ 相等是预期 ⇒ ⇒ 仍 MUST **各自登记**。

#### scenario: idle_line

`uart_tx` 恒高 + `busy == 0`；16 个连续采样全为 1（`EXPECT_SEQ_COUNT`）（3 条）。

#### scenario: send_0x55

帧 0x55：起始位低（首/尾 + busy=1）· 8 个数据位（LSB first）每窗首/尾各一条 ·
停止位高（首/尾 + busy=0）（23 条）。

#### scenario: send_0x5A

帧 0x5A：同型（23 条）—— 含相邻同值位 ⇒ 抓 MSB-first 错。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测（`RESULT: checks=49 fails=N`） | 抓的是 |
|---|---|---|---|
| M1 | 数据位取 `shift_reg[8]`（**MSB first**） | ✅ `fails=16`（`send_0x55#4/#5/#12/#13/#16/#17` 等） | 判据 4 / 6 |
| M2 | 起始位改**高** | ✅ `fails=25`（`send_0x55#0` timeout 等） | 判据 3 |
| M3 | 波特到期常量 433 → 216（位宽减半） | ✅ `fails=16`（`send_0x55#3/#11/#14/#15/#18/#19` 等） | 判据 6（位窗锚定） |
| M4 | 停止位改**低** | ✅ `fails=2`（`idle_line#0` · `idle_line#2`） | 判据 1 / 5（停止位与空闲同值 ⇒ 也一起红） |

---

## tb_uart_rx_example {#uart-rx-example}

### 上游参考（**spec-derived** · UART 8N1 + 中点采样）

| 判据 | 约定 |
|---|---|
| 收到的字节 | MUST **等于**发出的字节（线上 **LSB first**） |
| `valid` | MUST 是**单拍脉冲**（每字节一次） |
| 采样点 | MUST 落在**位的中段**（否则真实串口的时序抖动会直接采错） |

### impl-contract（本实现的取值）

- 位周期 = **`prescale + 1`** 拍；采样点 = **`div == prescale/2`**。
- `data` / `valid` 的复位值 = 0；`idle` / `rx_d` 的 **INIT = true**。

### ✅ 已修复的两个缺陷（2026-11-12 · 由旧 TB 挖出）

| # | 缺陷 | 修法 |
|---|---|---|
| **D6** | 采样点硬编码 `div == 4`（只对 `prescale == 9` 成立） | 改为 `div == prescale/2` |
| **D7** | 解出的字节**位反转**（旧 demo 的 0x5A 是**位回文** ⇒ 掩盖了它） | 右移 + 新位进 MSB |

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 复位态 `data` / `valid` | impl-contract | 0 / 0 |
| 2 | 收 0x96 ⇒ `data` | spec-derived | **0x96**（**非位回文**） |
| 3 | 收 0x69 ⇒ `data` | spec-derived | **0x69** |
| 4 | 连发 4 字节 ⇒ 收到的序列 | **metamorphic** | **逐项恒等**（0x96 / 0x69 / 0x4B / 0xA5） |
| 5 | `valid` 的单拍性 | **metamorphic** | 每个字节：脉冲当拍 = 1、下一拍 = 0 |
| 6 | **采样相位**（`prescale = 99` + 3 个探针） | **spec-derived** | 采样点 ∈ **[1/3, 2/3)** |

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 32** ⇒ ⇒ 三方等式。

VERIF-CHECKS: 32

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `reset_state` | 判据 1 |
| `recv_two_bytes` | 判据 2 · 3 · 5 |
| `recv_sequence` | 判据 4 · 5（metamorphic） |
| `phase_probe_a` / `phase_probe_a_prime` / `phase_probe_b` | 判据 6（夹逼） |

### ⚠️ 两条**实施期发现**（都按「两侧同源」处理，不是分叉）

1. **复位要载入寄存器 INIT**：框架复位的最后一拍复位已释放、且数据输入被 tie-off 为 0
   ⇒ ⇒ 接收器（`idle`/`rx_d` INIT=true）**真的**启动一帧「全 0」的接收。
   ⇒ ⇒ `.verif` 的 `reset_state` 用 `prescale=0`（1 拍/位）+ 12 拍把它**排空**，
   再断言 `data==0` / `valid==0`。**两侧行为完全相同**（实测 RTL 侧同样发生）。
2. **解释器必须驱动 reset 节点**（而不是只调 `Simulator::reset()`）：后者把**所有**节点清 0
   ⇒ ⇒ `INIT != 0` 的寄存器起在错误初态 ⇒ ⇒ 与 RTL 分叉（实测：解码全错）。
   已修为「驱动 reset 4 拍 + 释放 1 拍」（`src/rtl_emit/verif_run.cpp`）⇒ ⇒ 两侧一致。

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个）。
- **顶层端口表 = 6 根** = 框架 2 + 根自有 **4**（`rx` · `prescale[15:0]` · `data[7:0]` · `valid`）。
- 产物端口名 = `top_<C++ 成员名>`（端口全部具名）。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（`tb_uart_rx_example.cpp` 默认模式）= **32** 条 ⇒ ⇒ `interp_checks = 32`。
- TB 侧（`tb_uart_rx_example.sv`）= **32** 条 ⇒ ⇒ `min_checks = 32`。

#### scenario: reset_state

排空复位释放那一拍误触发的帧后：`data == 0` · `valid == 0`（2 条）。

#### scenario: recv_two_bytes

收 0x96 / 0x69：每个字节 4 条（`WAIT valid` + `data` + `valid` 单拍性两条）（8 条）。

#### scenario: recv_sequence

连发 0x96 / 0x69 / 0x4B / 0xA5：逐字节同 4 条（16 条）⇒ ⇒ **序列逐项恒等**（metamorphic）。

#### scenario: phase_probe_a

`prescale=99`：bit0 前 1/3 为 1 ⇒ 读 0（2 条）。

#### scenario: phase_probe_a_prime

`prescale=99`：bit0 前 1/3 为 0 ⇒ 读 1（与 A 互证）（2 条）。

#### scenario: phase_probe_b

`prescale=99`：bit0 前 2/3 为 0 ⇒ 读 0（不在后 1/3）（2 条）。

`prescale=99`（100 拍/位）下把 bit0 的翻转点放在 1/3 与 2/3 ⇒ 三个探针把采样点
夹在 **[1/3, 2/3)**（每个探针 2 条：`WAIT valid` + `data`）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测（`RESULT: checks=32 fails=N`） | 抓的是 |
|---|---|---|---|
| M1 | 位移累加改回**左移**（= 重新注入 D7 位反转） | ✅ `fails=7`（`recv_two_bytes#1` · `recv_sequence#1/#5/#9` · 三个探针的数据断言） | 判据 2/3/4 |
| M2 | 采样点改回硬编码 `div == 4`（= 重新注入 D6） | ✅ `fails=2`（`phase_probe_a#1` · `phase_probe_a_prime#1`） | 判据 6（夹逼） |
| M3 | `valid` 恒 1（单拍性破坏） | ✅ `fails=16`（每个字节的「下一拍应为 0」） | 判据 5 |

⚠️ **M2 只有探针红**（2 条）—— 这正是探针存在的理由：`prescale=9` 时 `div==4` 与
`prescale/2` **恰好相等** ⇒ 普通字节收发的判据对 D6 **结构性失明**。

---

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

> `kind=target`（**自发行** · 2026-11-22 起）—— 发射入口 = TB 自身
> （`tb_rv32i_soc` 的 `--emit-rtl` 模式）⇒ ⇒ 它**在** `CH_TRACKED_EXAMPLE_TARGETS` 里
> ⇒ ⇒ 本节同时是**C++ 侧（解释器路径）**与**`.sv` 侧（Verilog 路径）**的**共同**期望源。

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

### ✅ C++ 侧（解释器路径）的**同 24 条判据**（2026-11-22 · `tb_rv32i_soc`）

⚠️ **本节是「同一张判据表」在 C++ 侧的展开** ⇒ ⇒ **期望值零新造**
⇒ ⇒ C++ 侧与 `.sv` 侧**互为跨后端佐证**（解释器路径 vs Verilog 路径 ⇒ ⇒
**发射底座 / 编码底座的同构缺陷**在两侧都会一致地错 ⇒ ⇒ **两侧一致 ≠ 正确**
⇒ ⇒ 期望值 MUST **手推**、禁抄仿真输出 —— 见仓内 `AGENTS.md` §ANTI-PATTERNS）。

| 形态 | `.sv` 侧（手写 TB） | C++ 侧（`tb_rv32i_soc` 默认模式） | 条数 |
|---|---|---|---:|
| **S1** 首事件捕获（首个 `dbg_id_valid` 拍的 pc） | `fetch_pc[0] == 0x80000000` | `S1a` 至少取到一条指令 + `S1b` 首次取指 pc == `0x80000000` | 2 |
| **S2** 窗口内集合成员（7 个字**每个**都被取到） | `fetch_cnt[k] > 0`（k = 0..6） | `S2` 程序字 k 被取到（k = 0..6）⇒ ⇒ **按 `(pc, instr)` 配对**判定，**不用**指令值单判（NOP 别名：字 1 / 字 3 / 字 7..63 全是 `0x00000013` ⇒ 单判会把「取到字 7」误判成「取到字 1」⇒ ⇒ 判据被放宽） | 7 |
| **S3** 序列前缀的相邻差（取指 pc 增量 == 4） | `fetch_pc[i] - fetch_pc[i-1] == 4`（前 6 段） | `S3` 取指 pc 第 i 段递增 +4（i = 1..6；字 6 是 `jal` ⇒ 跳转 ⇒ 不再判） | 6 |
| **S4** 跨端口合取（∃ 写回 `rd == xN ∧ data == V`） | 写回 `x1=42` / `x2=11` / `x3=53`（rd 与 data 各判） | `S4` 写回 rd=x1/x2/x3（3 条）+ 对应数据 == 42/11/53（3 条） | 6 |
| **S5** 跨端口合取（∃ store `data == 53 ∧ addr == 0`） | 写存储 数据 = 53 且 地址 = 0 | `S5` 写存储 addr=0 且 data=53 | 1 |
| **S6** 跨拍不变量（退休计数**单调不减** + `> 0`） | 单调不减 且 `> 0` | `S6a` 单调不减 + `S6b` `> 0` | 2 |
| **合计** | **24** | **24**（`RESULT: checks=24 fails=0`） | **24** |

⚠️ **为什么 C++ 侧能覆盖 `.sv` 侧的全部 24 条**：`.verif` 的**封闭原语集**
（ADR-074 D2 + ADR-076）表达不了 S1 / S2 / S3 / S6（见下节「为什么该 TB MUST 保持手写」）
⇒ ⇒ 故 `.sv` TB **MUST 保持手写**（`generated=no` = **终态**）⇒ ⇒
**但 C++ 侧是普通 C++**（**无**封闭原语集）⇒ ⇒ 那 4 个形态在此**全部可表达**
⇒ ⇒ **判据面零缺口**，且两侧**实现独立**（一个跑解释器、一个跑 Verilog）。

⚠️ **C++ 侧的程序字来源** = 库编码器 `riscv::ADDI/ADD/SW/JAL/NOP`
（`include/cpu/riscv/rv32i_asm_encode.h` · 全仓**唯一**一份编码器）
⇒ ⇒ 且与**手算常量**做**编译期**交叉校验（`static_assert` × 7）
⇒ ⇒ 保留「编码器 × 手算常量」的交叉校验证据（仓内纪律：只用 helper 断言 helper ⇒ ⇒ 自证 ⇒ ⇒ 零判别力）。

⚠️ **装载纪律（G3）**：程序尾部（字 7..63）**必须填 NOP**（`0x00000013`），
**不能填 0**（`0x00000000` 的 opcode = `0x00` ⇒ ⇒ **未知 opcode** ⇒ ⇒ 流水线在此冻结）
⇒ ⇒ C++ 侧与 `.sv` 侧**同纪律**。

### ✅ 2026-11-13 · 端口名已具名（R17.2 · 用户 ⑤ 的原始诉求）

顶层 `Rv32iCoreTop` 的 **32** 个端口**全部**带**显式名** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`
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

**同一份期望源**适用于**两种发射形态**（扁平 `/ kind=target` · 层次化 `/ kind=hier`）
⇒ ⇒ 二者共用**同一个独立 Verilog TB**（`examples/riscv-mini/tb_rv32i_soc.sv`）
⇒ ⇒ 理由 = **顶层端口表同源**：两种形态都走 `make_flat_module_view`
⇒ ⇒ 端口名 + 方向 + 宽度 + 顺序**逐字逐序相同**（实测 **34** = 6 in + 28 out）。

| 面 | 扁平（`kind=target`） | 层次化（`kind=hier`） |
|---|---|---|
| 发射入口 | `tb_rv32i_soc --emit-rtl`（`toVerilog`） | `tb_rv32i_soc --emit-rtl --hierarchical`（⇒ `toVerilogHierarchical`） |
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

### ⛔ 为什么该 TB **MUST 保持手写**（`.verif` 原语的**表达力边界** · 2026-11-21 实测）

⚠️ **结论先说**：RV32I SoC 的 TB（`examples/riscv-mini/tb_rv32i_soc.sv`）**不迁 `.verif`**
⇒ ⇒ `generated=no` 是**终态**，**不是欠账**（原 note 写「属后续改造范围」⇒ ⇒ 会让人反复开项
⇒ ⇒ 已订正为结论性表述 + 复核条件）。

**因为它 24 条断言里有 4 个形态是封闭原语集（ADR-074 D2：`SET` / `TICK` / `SETTLE` /
`WAIT` / `EXPECT` / `EXPECT_STABLE` / `OBSERVE` / `RESET` + `REPEAT`/`_i`）表达不了的**：

| # | 形态（TB 里的实现） | 需要的语义 | 现原语集 |
|---|---|---|---|
| **S1** | 首个 `dbg_id_valid === 1` 拍的 pc（`fetch_pc[0]`） | **首事件捕获** | ❌ 只能按**已知拍号** `EXPECT`；而 valid 的出现拍**依赖 stall** ⇒ 不可预知 |
| **S2** | 7 个程序字**每个**都被取到（`fetch_cnt[k] > 0`） | **窗口内集合成员（∃）** | ❌ 无历史 / 集合原语 |
| **S3** | 取指 pc 的**相邻差 == 4**（前 6 段） | **序列前缀的相邻差** | ❌ 无序列原语 |
| **S4** | ∃ 写回 `rd==x1 ∧ data==42`（x2 / x3 同型，共 6 条） | 窗口内 ∃（配对） | ⚠️ **部分可（可行性判断 · 未实测）**：`WAIT dbg_wb_write_en low/high` 电平序列 + `EXPECT` **应当**能判「**第 k 次**写回」（语义略强于 ∃ ⇒ 可接受）⇒ ⚠️ 但「电平是否真的逐次翻转」取决于该 DUT 的脉冲形态 ⇒ ⇒ **迁移前 MUST 先实测**（本行不是实测结论） |
| **S5** | ∃ store `data==53 ∧ addr==0` | 窗口内 ∃ | ⚠️ 同上（`WAIT obs_data_write_en` 电平序列 ⇒ 第 1 次 store）⇒ ⇒ 同属**未实测量可行性判断** |
| **S6** | `obs_perf_instructions` **单调不减** + `> 0` | **跨拍不变量（相邻两拍比较）** | ❌ `EXPECT_STABLE` 只判「保持不变 n 拍」⇒ 判不了「不减」 |

⇒ ⇒ S1 / S2 / S3 / S6 **不可表达**（4 / 6 形态）⇒ ⇒ 剩余 S4 / S5 能表达也不够覆盖全部判据。

⚠️ **为什么不「按拍号硬编码」凑出来**：那会把**停顿时间线**钉死
（B3 的 T5 会重算 `load_use_stalls` 6 → 8 ⇒ 断言必然转红）⇒ ⇒ 与本 TB 的既有设计目标
（**不依赖停顿拍数**：退休数只判「单调 + > 0」）直接冲突。

**扩展处方（若将来要迁）**：封闭集需新增**三类**原语 ——
① 历史窗口（如 `EXPECT_SEEN <port> <value> [窗口]` ⇒ 解 S2）·
② 序列相邻差（如 `EXPECT_DELTA <port> <d> [窗口]` ⇒ 解 S3）·
③ 跨拍不变量（如 `EXPECT_MONOTONIC <port>` ⇒ 解 S6）·
④ 首事件捕获（如 `WAIT_VALUE <port> <value> <max>` ⇒ 解 S1）。
⇒ ⇒ ⚠️ 这是**架构决策**（ADR-074 D2 的封闭集是刻意封闭的）⇒ ⇒ 不在本仓的 builder 裁量内。

#### ✅ 2026-11-21 · **ADR-076 后复核**（登记的复核条件**已触发** ⇒ ⇒ 重开评估 ⇒ ⇒ 结论：**仍不可表达**）

⚠️ 本节原登记的**复核条件** = 「ADR-074 D2 的封闭集新增『历史 / 集合 / 序列』原语 ⇒ 重开迁移评估」
⇒ ⇒ **ADR-076 已新增 4 条**（`RECORD` / `EXPECT_SEQ_SELF` / `EXPECT_SEQ_STEP` / `EXPECT_SEQ_COUNT`）
⇒ ⇒ **复核条件已触发** ⇒ ⇒ 本次**逐形态**重评（结论：6 个形态里 **5 个仍不可表达**、
**1 个只能降级表达** ⇒ ⇒ 结论不变）：

| 形态 | ADR-076 后 | 理由（**逐条**） |
|---|---|---|
| **S1** 首事件捕获（首个 `dbg_id_valid` 拍的 pc） | ❌ 仍不可表达 | ADR-076 **无「条件捕获」**原语 ⇒ `RECORD` 是**无条件**的 ⇒ ⇒ 抓不到「valid 那拍」 |
| **S2** 7 个字**每个**都被取到（窗口内集合成员） | ⚠️ **只能降级** ⇒ ⇒ **不可接受** | `EXPECT_SEQ_COUNT` 只能数「**等于某值**」的项数 ⇒ ⇒ 而本判据要的是「**valid 拍**的 pc 集合」⇒ ⇒ `RECORD dbg_id_pc`（无条件）会把**无效拍**的 pc 也计入 ⇒ ⇒ 判据被**放宽** ⇒ ⇒ 违反「不得降级判据面」 |
| **S3** 取指 pc 的**相邻差 == 4** | ❌ 仍不可表达 | `EXPECT_SEQ_STEP` 的右侧 MUST 是**另一个端口**的同位项 ⇒ ⇒ 本设计**无**恒为 4 的端口 ⇒ ⇒ 表达不了「差 == 常量」 |
| **S4 / S5** ∃ 写回（`rd` ∧ `data` **配对**）/ ∃ store（`data` ∧ `addr` **配对**） | ❌ 仍不可表达 | 需要**跨端口合取**（`rd == x1 ∧ data == 42`）⇒ ⇒ 原语集**无合取** ⇒ ⇒ `EXPECT_SEQ_COUNT` 只能单端口计数 |
| **S6** 退休计数**单调不减** | ❌ 仍不可表达 | `EXPECT_SEQ_SELF` 判的是**逐项恒等**⇒ ⇒ 判不了「不减」（`EXPECT_STABLE` 判的是「保持不变」） |

⇒ ⇒ **结论不变**：该 TB **MUST 保持手写**（`generated=no` = **终态**，不是欠账）。
⇒ ⇒ ⚠️ **复核条件收窄**（避免被同一前提反复开项）：重开的触发 = **新增原语能表达
『条件捕获』或『跨端口合取』或『差 == 常量』或『单调不减』** ⇒ ⇒
单纯新增「历史 / 集合 / 序列」**不再**构成重开理由（2026-11-21 已评过）。

### ✅ 2026-11-21 · 顶层端口表的**独立手推判据**（R8′ · 台账 id=25 的下游实施项）

⚠️ **本节之前的顶层端口面判据有两条，但它们都「与产物同源」**：

1. 层次化顶层端口行 == **扁平**顶层端口行的前缀（逐字逐序）；
2. 层次化顶层端口行 == `manifest.port_table[]`（逐字逐序）。

⇒ ⇒ 扁平与 manifest **都来自同一次发射** ⇒ ⇒ 「发射底座算错」时**两侧一起错**
⇒ ⇒ **抓不到**（仓内纪律：「**两后端一致不能当作正确性证据**」在端口面上的同型缺口）。

⇒ ⇒ 补第三条 = **手推期望集**（**独立于产物**）⇒ ⇒ 判据载体 =
`scripts/verify_hierarchical_rtl.sh` 的 **R8′** 块（期望集写在护栏内）。
⇒ ⇒ **推导源** = `examples/riscv-mini/src/rv32i_pipeline_top.h` 的 `__io`
（**4 in + 28 out** ⇒ **声明顺序即端口顺序**；端口名 = `top_<C++ 成员名>` · ADR-075 E1）
⇒ + 框架 2（`default_clock` · `default_reset`）⇒ ⇒ 共 **34** 根
⇒ ⇒ 期望集**禁**从产物输出抄（抄了 = 判据自证 ⇒ 恒绿）。
⇒ ⇒ 判别力自证：投毒改产物端口宽度（`top_dbg_id_pc` 32 → 16）⇒ ⇒ R8′ 必红
（`scripts/verify_hierarchical_rtl.sh --self-test` 的第 11 条投毒）。

## tb_phase3a_example {#phase3a-example}

> ⚠️ **2026-11-20 新增**（change `phase3a-rtl-product-2026-11`）—— Phase 3A 集成单元
> （4×4 Interconnect + AXI-to-Lite Converter + AXI4-Lite Slave）进 RTL 产品面。
> ⇒ ⇒ 期望值**手推**（**禁**从仿真输出 / 从被测实现转录）。

### 1. 顶层端口表（**9** 个 = 框架 2 + 根自有 7）

⚠️ **口径 = SoC 的同款**（`soc-rtl-expectations.md` §1「顶层端口表（**13** 个 = 框架 2 + 根自有 11）」）
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

### ⛔ 为什么该 TB **MUST 保持手写**（`.verif` 原语的**表达力边界** · 2026-11-21 实测）

⚠️ **结论先说**：phase3a 的 TB（`examples/axi4/tb_phase3a_example.sv`）**不迁 `.verif`**
⇒ ⇒ `generated=no` 是**终态**，**不是欠账**（原 note 写「迁移属 ADR-074 §7 的实施范围」
⇒ ⇒ 会让人反复开项 ⇒ ⇒ 已订正为结论性表述 + 精确的复核条件）。

**因为它 6 条断言里有 1 条是封闭原语集（ADR-074 D2）表达不了的**：

| # | 断言 | 需要的语义 | 现原语集 | 判定 |
|---|---|---|---|---|
| 5 | **框架复位生效**（`default_reset` 拉高**跨过 1 个时钟沿** ⇒ `done == 0`） | **运行期中框架复位** | ❌ `RESET default_reset` 是**静默空操作**（两端同证 · 见下）⇒ ⇒ **不可表达** | **不可表达** |

⇒ ⇒ 其余 5 条（① 上电 ② 写完成 ③ 读完成 ④ 读回值 ⑥ `rst_n` 级复位）**形态上可表达**
⇒ ⇒ 但 ⑤ 不可表达 ⇒ ⇒ 迁 `.verif` 会**丢掉 ⑤** ⇒ ⇒ **判据面降级** ⇒ ⇒ **禁止迁移**。

⚠️ **ADR-076 后复核**（该 change 新增 `RECORD` / `EXPECT_SEQ_SELF` / `EXPECT_SEQ_STEP` /
`EXPECT_SEQ_COUNT`）⇒ ⇒ **不解** ⑤（⑤ 要的是「**复位语义**」，不是「历史 / 序列」）
⇒ ⇒ 复核结论 = **仍不可表达** ⇒ ⇒ 结论不变。

#### ⚠️ `RESET default_reset` 为什么是**静默空操作**（2026-11-21 实测 · 两端同证）

- **解释器**：`build_register` 给寄存器的 `rst_val` 是 **nullptr**
  （`include/core/node_builder.h`）⇒ ⇒ 复位期间 `instr_reg::eval` **只 return、不赋值**
  （`src/ast/instr_reg.cpp`）⇒ ⇒ 寄存器**保持原值**；
- **渲染 TB**：`RESET` 的**释放动作**与**最后一个**时钟沿**同拍**⇒ ⇒ 该沿走 `else` 分支
  ⇒ ⇒ 刚写下的复位值被**同拍覆盖**。

⇒ ⇒ 实测（gpio **真实产品 DUT** + **真 verilator**）：`RESET default_reset high N`
（N = 1 / 2 / 4 / 5）⇒ ⇒ **两端都不复位**⇒ ⇒ 「看起来像复位、实际不复位」
⇒ ⇒ 与「两端结论相反」不同（两端**一致地**什么都不做）⇒ ⇒ 但**同样危险**
⇒ ⇒ 作者会以为复位发生了 ⇒ ⇒ 可能把期望值改成「不复位的值」⇒ ⇒ **假绿**。

⇒ ⇒ ✅ **已硬化（2026-11-21）**：`.verif` 的 `RESET` 指向**框架复位端口**
（manifest `role == "reset"`）⇒ ⇒ **两端同源 fail-closed**
（`src/rtl_emit/verif_internal.h::verif_reset_port_violation` + 两端各一处调用
+ `tests/test_verif_wait_level.cpp` 的「两端同结论」用例 + 两条变异自证）
⇒ ⇒ 既有 `.verif` **零处**用 `RESET`（实测 grep）⇒ ⇒ 零破坏。

#### 复核条件（避免被同一前提反复开项）

重开的触发条件 = **框架侧**能表达「运行期中框架复位」（例如新增 `FRAME_RESET` 原语
⇒ ⇒ 解释器按 `INIT` 回填 + 渲染侧把释放动作与最后一个沿**分离**）⇒ ⇒
单纯新增「历史 / 序列」原语**不**构成重开理由（2026-11-21 已评过 ⇒ ⇒ 结论不变）。

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

---

## tb_stream_arbiter_demo {#stream-arbiter-demo}

### 上游语义（**spec-derived** · capability `chlib-stream-arbitration` + `chlib-stream-ready-ownership`）

被测单元 = `examples/stream/stream_arbiter_top.h`（`chlib::stream_arbiter_round_robin<ch_uint<8>, 4>` 的具名端口包装）。
⚠️ **本组合子是【时序】的**（内部有 `last_grant_reg` 轮转反馈寄存器）⇒ ⇒ 与
mux / demux / fork 三个**纯组合**示例不同 ⇒ ⇒ 判据 MUST 用 `TICK` / `WAIT` 推进拍。

| 端口 | 语义 | 出处 |
|---|---|---|
| `out.valid` | 任一输入 `valid` | `round_robin_selector` 的 `valid` |
| `out.payload` | **被授予通道**的 payload | capability「输出 payload MUST 是该通道的 payload」 |
| `selected` | 被授予的通道索引（**纯组合输出** = `priority_encoder(grant)`） | capability「选择 MUST 用硬件原语表达」 |
| `in[i].ready` | `(selected == i) & out_ready & any_valid` | capability「转发类的上游 MUST 真的收到反压」 |
| 轮转 | 扫描起点 = **上次授予的下一个** ⇒ ⇒ 持续有效的通道**轮流**被服务 | capability「轮转仲裁器 MUST 真正轮转（MUST NOT 饿死持续有效的通道）」 |

### ⚠️⚠️ **相位口径（本行的关键口径决策）**

capability `chlib-stream-arbitration` 明文：

> MUST NOT 要求某一具体相位（首拍授予谁取决于实现的状态初值）

⇒ ⇒ **本行【不锁相位】**：判据一律写成**效果属性**（「每路在有限拍内被服务」），
**不是**逐拍字面量 ⇒ ⇒ 逐拍金标已在 `tests/chlib/test_stream.cpp` 的 `[rotation]`
判别力用例里（golden = `0,2,0,2,…`）⇒ ⇒ 本产品面**不重复**锁它 ⇒ ⇒ **不**把
「相位」冻进产品契约。

⇒ ⇒ **相位无关的表达方式**：`WAIT <in[i]_ready> <上界>`
（`in[i].ready` 为 1 ⟺ 通道 i 被授予 ⇒ ⇒ 「该路在有限拍内被服务」= 零值测试可表达）
⇒ ⇒ 上界取**轮转周期的紧界**（4 路 ⇒ 4 拍；2 路 ⇒ 2 拍）⇒ ⇒ 「恒选一路」的缺陷必超时红。

⚠️ **观测点的「一拍偏移」**：`selected` 是**纯组合**输出 ⇒ ⇒ 沿上 `last_grant_reg`
推进后组合**重新求值** ⇒ ⇒ 沿后读到的 `selected` 是**下一拍**将授予的通道
⇒ ⇒ `WAIT` 观察到的也是「下一拍」的授予 ⇒ ⇒ **界仍成立**（周期不变）。

### impl-contract（本实现的取值）

- **框架复位**把全部数据输入置 0 ⇒ ⇒ 复位后四路 `valid=0` / `out_ready=0`。
- `no_valid_no_grant` 场景里的 `selected == 0` = `priority_encoder(0)` 的取值
  ⇒ ⇒ **不是**契约条款（契约只要求「不授予」）⇒ ⇒ 计入 impl-contract。

### 判据

| # | 判据 | 级别 | 期望 |
|---|---|---|---|
| 1 | 唯一有效通道（in1）被选中 | spec-derived | `selected == 1`（**逐拍**，相位无关） |
| 2 | 输出 payload = **该通道**的 payload | spec-derived | `0x21`（不是别的通道的） |
| 3 | 只有被授予的一路 `ready=1` | spec-derived | `in1_ready=1`，其余 3 路 `=0` |
| 4 | 两路（0 / 2）持续有效 ⇒ 每路在 **2 拍内**被服务 | spec-derived | `WAIT` 紧界 ⇒ ⇒ 「恒选一路」必超时红 |
| 5 | 四路持续有效 ⇒ 每路在 **4 拍内**被服务 | spec-derived | `WAIT` 紧界 ⇒ ⇒ 「固定优先级」必超时红 |
| 6 | 每路被服务时 `out.payload` = **该路**的 payload | spec-derived | `0x10` / `0x21` / `0x32` / `0x43` |
| 7 | `out_ready=0` ⇒ 四路 `in*_ready` 全 0 | spec-derived | 上游停顿 = **可观测** |
| 8 | `out_ready=0` 时 `out.valid` | spec-derived | 仍为 1（valid 与 ready 独立） |
| 9 | 四路 `valid` 全 0 ⇒ `out.valid=0` · 四路 `ready=0` | spec-derived | 无授予 ⇒ 无反压 |
| 10 | 四路 `valid` 全 0 时 `selected` | **impl-contract** | 0（= `priority_encoder(0)` 的取值，**非**契约） |

### 验证源（`.verif` · 单一语义来源）

**断言条数（人工登记）= 38** ⇒ ⇒ 三方等式：本行 == `tb_stream_arbiter_demo.verif` 的
静态展开（`EXPECT` + `WAIT` 计数）== 运行期 `RESULT: checks`。

VERIF-CHECKS: 38

⚠️ 上面的 `VERIF-CHECKS:` 行是**机器可读**的**第一边**（由**人**登记 ⇒ ⇒ 禁由生成器
输出回填 ⇒ ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ ⇒ 判据恒绿）。
⚠️ 本节的 `#### scenario:` 锚点与验证源的 `@scenario` **双向 1:1**（粒度 = scenario）。

| scenario | 判据出处（本节的哪一段） |
|---|---|
| `single_valid_channel_held` | 判据 1 · 2 · 3 |
| `two_channels_alternate` | 判据 4 · 6 |
| `four_channels_all_served` | 判据 5 · 6 |
| `backpressure_stalls_upstream` | 判据 7 · 8 |
| `no_valid_no_grant` | 判据 9（spec-derived）· 10（impl-contract） |

### 产物形态（**手推**）

- **module 集 = {`top`}**（1 个 —— 本设计无 `ch_layer` ⇒ 层次化 = 扁平）。
- **顶层端口表 = 18 根** = 框架 2（`default_clock` · `default_reset`）+ 根自有 **16**
  （4 路输入流 × {payload[7:0], valid, ready} + 输出流 {payload[7:0], valid, ready} + `selected[1:0]`）。
- 产物端口名 = `top_<C++ 成员名>`（ADR-075 E1 · 端口全部具名 ⇒ 无 `top_unnamed_*`）。
- ⚠️ **产物含 1 个寄存器**（`top_rr_last_grant` · 4 bit）⇒ ⇒ 本行是**首个时序**产品单元。

### C++ 侧 vs TB 侧（**两侧各自登记**）

- C++ 侧（解释器路径，`tb_stream_arbiter_demo.cpp` 默认模式）= **38** 条 ⇒ ⇒ `interp_checks = 38`。
- TB 侧（Verilog 产物，`tb_stream_arbiter_demo.sv`）= **38** 条 ⇒ ⇒ `min_checks = 38`。
- **两侧同源**（同一份 `.verif` 派生）⇒ ⇒ 相等是预期 ⇒ ⇒ 但 MUST **各自登记**
  （护栏 ⑨ 判 C++ 侧、⑥ 判 TB 侧 ⇒ ⇒ 禁一侧代另一侧）。

#### scenario: single_valid_channel_held

只有 `in1` 有效 ⇒ ⇒ `selected == 1`（逐拍）· `out.valid == 1` · `out.payload == 0x21` ·
`in1_ready == 1` 且其余三路 `ready == 0`（13 条）。

#### scenario: two_channels_alternate

只有 `in0` / `in2` 有效 ⇒ ⇒ 轮转周期 = 2 ⇒ ⇒ 每路 MUST 在 **2 拍内**被服务
（`WAIT in0_ready 2` / `WAIT in2_ready 2` 各两轮）⇒ ⇒ 每路被服务时 `out.payload`
= 该路 payload（`0x10` / `0x32`）（6 条）。

#### scenario: four_channels_all_served

四路全有效 ⇒ ⇒ 轮转周期 = 4 ⇒ ⇒ 每路 MUST 在 **4 拍内**被服务
（`WAIT in*_ready 4`）⇒ ⇒ 每路被服务时 `out.payload` = 该路 payload
（`0x10` / `0x21` / `0x32` / `0x43`）（8 条）。

#### scenario: backpressure_stalls_upstream

`out_ready=0` ⇒ ⇒ 四路 `in*_ready == 0`（上游停顿可观测）· `out.valid == 1`（5 条）。

#### scenario: no_valid_no_grant

四路 `valid` 全 0 ⇒ ⇒ `out.valid == 0` · 四路 `in*_ready == 0` · `selected == 0`
（**impl-contract**）（6 条）。

### 变异自证（判别力自证 · 打在**临时产物**上 ⇒ 仓内产物零改动）

| # | 变异形态 | 实测 | 抓的是 |
|---|---|---|---|
| M1 | `assign top_out_valid = 1'b0;`（valid 通路断） | ✅ `fails=3`（`single_valid_channel_held#1/#8` · `backpressure_stalls_upstream#4`） | 输出 valid |
| M2 | `assign top_selected = 2'h0;`（选择恒定） | ✅ `fails=3`（`single_valid_channel_held#0/#7/#11`） | 授予索引 |
| M3 | `top_rr_last_grant <= 4'h0;`（**轮转反馈恒 0** ⇒ 轮转失效 ⇒ 退化为固定优先级） | ✅ `fails=9`（**5 条 `WAIT` 超时**：`two_channels_alternate#0/#4` · `four_channels_all_served#0/#4/#6` + 4 条 payload 判据） | **轮转 / 无饿死**（历史缺陷形态） |

⇒ ⇒ 还原后复跑 ⇒ `RESULT: checks=38 fails=0` ⇒ ⇒ 变异未污染产物。
⚠️ 每条变异 MUST **先确认仍能编译**（三条实测 `compile_rc=0`）⇒ ⇒ 否则跑的是**旧二进制** ⇒ 假绿。
⚠️ **M3 是本行的核心判别力**：它正是 capability 记载的历史缺陷形态
（「`last_grant` 恒传 `0_d` ⇒ ⇒ 轮转从不发生 ⇒ ⇒ 低位通道饿死」）⇒ ⇒
**相位无关**的 `WAIT` 判据能抓住它 ⇒ ⇒ 这就是「不锁相位」仍能有判别力的证据。
