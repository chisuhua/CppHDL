# Design: uart-tx-fsm-live-logic-2026-10

> 本 change = **首次实现**一个从未工作的 FSM + 加自检断言（example 层）。
> **不改框架、不改契约** ⇒ `skip_specs: true`。

## 0. 一句话结论

`examples/spinalhdl-ported/uart/uart_tx.cpp` 的状态机**从未工作过**（生成的 Verilog 里
输出是**两个常量**、状态寄存器**自环**）⇒ 本 change 用**正确形态**（5 条承重约束）
首次实现它 ⇒ 并加**自检断言**（不匹配 ⇒ `return 1` ⇒ 运行护栏能抓到）
⇒ ⇒ **变异 7/7 全被抓** ⇒ 断言有判别力。

## 1. 缺陷机制（三个叠加的根因）

### 1.1 `if (ch_bool)` 建图期恒假 ⇒ 条件逻辑全不发射

`ch_bool`/`ch_uint` 的 `operator uint64_t()` **只对 const 节点返回真值**：

```cpp
// src/lnode/bool.cpp:44-49
ch_bool::operator uint64_t() const {
    if (this->node_impl_ && this->node_impl_->is_const()) return lit value;
    return 0;          // ← 非 const 节点（任意运算结果）恒 0
}
```

⇒ `if (io().frame_start == ch_bool(true))` 的 `==` 是**非 const 节点**
⇒ 建图期恒 **false** ⇒ C++ 语义下走 **else**：

| `if` 形态 | 后果 |
|---|---|
| **无 else**（IDLE 的 `if (…) { load + transition_to(START) }`） | 分支体**从不发射** ⇒ `frame_start` 比较成死节点、`transition_to` **从未注册** |
| **有 else**（START/DATA/STOP 的 `if (baud==433) {…} else {baud+1}`） | **只发射 else** ⇒ 计数器**永远自增、永不归零** |

⚠️ 框架 `include/chlib/state_machine.h:34-38` **已文档化**这只坑（应使用
`transition_when`）⇒ 该 example 用的是**已文档化的反模式**。

### 1.2 多状态重复驱动同一 `ch_out` ⇒ 最后发射者获胜

`ch_state_machine::build()`（`include/chlib/state_machine.h:292-304`）**遍历所有状态**
执行各自 `on_active`；4 个状态都**无条件**写 `io().uart_tx`/`io().busy`，而
`ch_out::operator=` 是 `set_src(0, src)`（**覆盖**）⇒ **最后发射的 STOP 获胜**：

| 顺序 | 状态 | `uart_tx` | `busy` |
|---|---|---|---|
| 1 | IDLE | `1` | `0` |
| 2 | START | `0` | `1` |
| 3 | DATA | `(shift_reg & 1) != 0` | `1` |
| 4 | **STOP**（生效） | **`1`** | **`0`** |

⇒ 与实测（`uart_tx=1`/`busy=0` 恒定 4500 拍）**逐字吻合**。

### 1.3 `reg->next` 是 **last-wins** ⇒ 「每状态各写一次」无法表达多状态 FSM

`regimpl::set_next`（`include/ast/ast_nodes.h:47-54`）：

```cpp
void set_next(lnodeimpl *next) {
    if (!next) return;
    if (num_srcs() > 0) set_src(0, next);   // ← 覆盖，不是累加
    else add_src(next);
}
```

⇒ 同一寄存器被多状态各写一次 ⇒ **最后发射者赢** ⇒ ⇒ **每个寄存器 MUST 只有一个
写入点**，各状态的候选值 MUST 用 `select` 组合成**单一表达式**。

### 1.4 附带发现的 off-by-one（数据位取错位）

原注释写「移位寄存器（**9 位：起始位 + 8 数据位****）」⇒ `bit0` = 起始位、
`bit1..bit8` = 8 个数据位 ⇒ ⇒ **期望发 `bit1..bit8`**
⇒ 而原代码在 DATA 里发 `shift_reg & 1`（= `bit0` = 起始位）
⇒ ⇒ **少发 `data[7]`、多发一个起始位 0** ⇒ ⇒ 对 `data[7] = 0` 的值**碰巧正确**
⇒ ⇒ 本 change 改发 `(shift_reg >> 1) & 1`。
⚠️ 因该 FSM **从未工作过** ⇒ 不存在「既有行为」需保持 ⇒ 属「**首次实现**正确行为」。

## 2. 修复形态（5 条承重约束）

| # | 约束 | 本 change 的落实 |
|---|---|---|
| ① | 条件逻辑 MUST 用硬件原语（`transition_when` / `select`），**不得**用 C++ 控制流 | `on_active` 里**只**写 `transition_when`；所有条件值用 `select` |
| ② | **每个寄存器只有一个 `next` 写入点** | 3 个寄存器各一条 `select` 组合表达式，写在 `sm.build()` **之后** |
| ③ | **每个输出只驱动一次** | `uart_tx` / `busy` 各一条 `select(is_in(…), …)` 表达式 |
| ④ | **`transition_when` 的条件是电平敏感**（每拍求值、为真即走），**不是**「事件/完成」语义 | 「持续 N 拍后转移」MUST 与计数器条件**合取**（例：DATA→STOP 用 `bit_last & baud_done`）—— ⚠️ **本 change 的第 1 轮审稿 BLOCKER 正是漏了这条** |
| ⑤ | **不把 parity 当正确性证据** | 两后端**同构** ⇒ 同构缺陷恒被 parity 掩盖（`test_verilator_complex_e2e.cpp` 的 `STOP` 不可达即被 `REQUIRE(sa == sb)` 掩盖） |

⚠️ **纪律 ① 的完整枚举**（第 1 轮审稿补）：除 `if` 与 `? :` 外，**同类建图期求值陷阱**
还包括 —— `switch (硬件值)`（写 FSM 的人最自然会试）、`while` / `for` 的**行程条件**、
early `return`、把硬件值传给 **`bool` 形参**（⇒ 触发 `explicit operator bool`）。
⇒ ⇒ 凡「硬件值参与 C++ 控制流 / 布尔上下文」处，一律 MUST 改用硬件原语。

⚠️ **纪律 ① 的边界**：`transition_when(const ch_bool&, …)` 的条件 MUST 是**硬件节点**；
若用 C++ bool 语境算出（如 `.to_bool()` 或常量折叠）⇒ 退化为**常量** ⇒ 转移条件恒定。
⚠️ 且 `port<ch_bool>` **根本到不了**这里（无 `ch_bool` 转换 ⇒ 编译不过 ⇒ 见 §2.4）。

### 2.1 状态转移（`on_active` 内，**不**碰任何寄存器）

```
IDLE  : transition_when(frame_start,        START)
START : transition_when(baud_done,          DATA)
DATA  : transition_when(bit_last & baud_done, STOP)   // ⚠️ MUST 合取（纪律 ④）
STOP  : transition_when(baud_done,          IDLE)
```

⚠️ **纪律 ④ 的由来**（第 1 轮审稿 BLOCKER）：初稿写的是 `transition_when(bit_last, STOP)`
⇒ `bit_last` 是**电平**（`bit_counter` 一变 7 就持续为真）⇒ **下一拍就离开 DATA**
⇒ **第 8 个数据位只发 1 拍**（而非 434 拍）⇒ 停止位提前 433 拍。
⚠️ 而**采样网格对此零判别力**（每位样本落在位段**末尾** ⇒ 位段被截短时采样值仍可能正确）
⇒ ⇒ 必须另加**时长**断言（§3）。

### 2.2 寄存器 next（`build()` 之后，各**唯一**写入点）

```
baud_counter->next = select(is_in(START)|is_in(DATA)|is_in(STOP),
                            select(baud_done, 0, baud_counter+1),
                            baud_counter)

bit_counter->next  = select(is_in(DATA) & baud_done,
                            select(bit_last, 0, bit_counter+1),
                            select(is_in(START) & baud_done, 0, bit_counter))

shift_reg->next    = select(is_in(DATA) & baud_done,
                            (shift_reg>>1)|1,                 // 移出下一位
                            select(is_in(STOP) & baud_done, 511,
                              select(is_in(IDLE) & frame_start,
                                     (data<<1)|0,              // 载入 {data, 起始位}
                                     shift_reg)))
```

### 2.3 输出（各**唯一**驱动点）

```
uart_tx <<= select(is_in(START), 0,
               select(is_in(DATA),  ((shift_reg>>1)&1)!=0, 1))
busy    <<= select(is_in(START)|is_in(DATA), 1, 0)
```

### 2.4 ⚠️ 实现期踩到的**框架事实**：`port<ch_bool>` 无 `ch_bool` 转换

`include/core/io.h:232` 只有 `ch_uint` 有转换 ⇒ 实测（探针
`/tmp/opencode/probe_conv.cpp`）：

| 写法 | 编译 |
|---|---|
| `ch_bool(io().frame_start)` | ❌ **编译不过** |
| `ch_uint<1>(io().frame_start)` | ❌ **编译不过** |
| `ch_reg<ch_bool> latch; latch->next = io().frame_start; ch_bool v = latch;` | ✅ **可行**（范例 `test_verilator_complex_e2e.cpp:128-130` 同款） |

⇒ ⇒ `frame_start` MUST 经**寄存器**中转 ⇒ 代价：**条件晚 1 拍**被看到
（UART 不在意；且自检以 `busy` 上升沿为锚点 ⇒ 与该延迟无关）。

## 3. 自检断言（**判别力是承重件**）

| 项 | 设计 |
|---|---|
| 锚点 | `busy` **上升沿** = 进 START 的那一拍 = 起始位开始（**语义**信号，非实现细节） |
| 采样 | 每 **434** 拍采一次 `uart_tx`（波特分频 434 = `baud_counter` 0..433） |
| 期望（**手推**） | `[0, 1,0,1,0,0,1,0,1, 1]` = 起始位(0) + `0xA5` 的 8 位 LSB-first + 停止位(1) |
| 数据选择 | ⚠️ MUST 用 **`0xA5`**（`10100101` ⇒ `bit7=1`）⇒ `0x55`（`bit7=0`）与起始位相同 ⇒ **抓不到** off-by-one |
| 帧后 | MUST 回到 IDLE（`busy=0` 且 `uart_tx=1`） |
| **`busy` 高电平总拍数** | MUST = **3906**（= 9 位 × 434）⇒ ⚠️ **这条对「位段长度」有判别力**，而采样网格**没有**（第 1 轮审稿 BLOCKER 的检出依赖它） |
| 失败信号 | `return 1` ⇒ ⚠️ 运行护栏**只查退出码** ⇒ 返回 0 会让「行为退化」被判 PASS（这正是它长期「假绿」的机制） |

## 4. 实测证据

### 4.1 行为：改动前 vs 改动后

| | 改动前 | 改动后 |
|---|---|---|
| `uart_tx` | **恒 1**（常量） | 有 0/1 两种值 ⇒ **真实帧** |
| `busy` | **恒 0**（常量） | START/DATA 期间为 1，帧后回 0 |
| 帧序列 | **不存在** | 起始位(0) + 8 数据位 + 停止位(1)，每 434 拍一位 |
| 自检 | **无**（恒 PASS） | `[PASS]` / 失败 ⇒ exit 1 |

### 4.2 生成的 Verilog（3 条 AC 的硬证据）

| AC | 证据 |
|---|---|
| 状态寄存器**无自环** | `top_uart_tx_inst_reg_3 <= top_uart_tx_inst_mux_select_11[2:0];`（原为 `q <= q` 自环）⇒ **接的是 select 树** |
| `is_in(状态)` 的比较节点**有消费者** | `eq_eq_4` **11** 次 / `eq_eq_5` **6** 次 / `eq_eq_6` **5** 次 / `eq_eq_7` **5** 次（原为**零消费者**死节点） |
| 输出**只驱动一次** | `assign top_uart_tx_inst_unnamed_output = top_uart_tx_inst_mux_select_1;`（**单条** assign ⇒ 36 个 `mux_select` 节点构成组合树） |

### 4.3 变异验证 **7/7 全被抓**（每条先确认**编译成功** ⇒ 无假绿）

| # | 变异 | 结果 |
|---|---|---|
| **M1** | 恢复 off-by-one（数据位发 `shift_reg & 1`） | ✅ 转红：**5 个位**错位（第 1/2/4/5/7 位）⇒ **证明 `0xA5` 的选择是必要的**（`0x55` 时第 8 位会碰巧相同 ⇒ 抓不到） |
| **M2** | 数据位序反（发 `bit8` 而非 `bit1`） | ✅ 转红：第 3/6/8 位错 |
| **M3** | 起始位改成 `1` | ✅ 转红：第 0 位错 |
| **M4** | 停止位改成 `0` | ✅ 转红：第 9 位错 **+** 「帧后未回 IDLE」 |
| **M5** | 波特分频改 `433`（段长错） | ✅ 转红：第 1/2/3 位错（采样点错位） |
| **M6** | DATA 右移改 2 位（少移一位） | ✅ 转红：第 2/3/6 位错 |
| **M7** | **DATA→STOP 去掉 `& baud_done` 门控**（= 第 1 轮审稿 BLOCKER 的形态） | ✅ **转红**：`busy` 高电平 **3473 ≠ 3906** ⇒ ⚠️ **10 个采样值全对** ⇒ 证实「采样网格对位段长度零判别力」⇒ **必须**有拍数断言 |

⚠️ **第 1 轮审稿 BLOCKER 的闭合**：初稿的转移条件 `transition_when(bit_last, STOP)`
漏了 `baud_done` 门控 ⇒ 第 8 个数据位只发 1 拍 ⇒ 而**初稿的断言表**（§3）里
**没有**「位段长度 / busy 时长」这条 ⇒ ⇒ 缺陷**从断言的缝里漏过去**
⇒ ⇒ 根因是**草案契约被静默丢弃**：草案 §2.2 列了「每位持续时间 MUST = 434 拍 ±1」
与「busy MUST 在 START→STOP 期间为 1」两条，实现时**未落进断言表且无丢弃记录**
⇒ ⇒ **诚实登记**：这是本次的**流程缺陷**（不是「实现疏忽」）⇒ ⇒ 现已补上
（`busy` 高电平总拍数 = 3906）⇒ ⇒ 并把它变成**第 7 条变异**（M7）⇒ 证明它有判别力。

⚠️ **数值订正**：critic 的逐拍推演给出「错误版 busy 高 = 3493 拍」，**实测为 3473 拍**
⇒ 以**实测**为准（差 20 拍）⇒ 其推演的**结论**（缺陷成立、采样网格掩盖）经实测确认 ✓。

⚠️ **注入器 fail-closed**：变异用**行号**注入（`/tmp/opencode/mutate.py`），
行号越界 / 目标子串不匹配 ⇒ 报 `MUTATE_FAIL` 且 **exit 2** ⇒ 杜绝
「注入失败却跑旧代码 ⇒ 假绿」。⚠️ 首次尝试 M2 时**正是**用字符串替换失败
（旧串不匹配）⇒ 跑的是 M1 的残留 ⇒ **已修正为行号注入**（此为留痕）。

### 4.4 验收

| 项 | 结果 |
|---|---|
| `ctest -LE perf` | ✅ **191/191** |
| `run_all_ported_tests.sh` | ✅ **28/28** |
| `scripts/run_all_samples.sh` | ✅ **35/35** |
| 9 个 `scripts/verify_*.sh` + 3 个 `--self-test` | ✅ 全 exit 0 |
| 计数锚点 | ✅ `CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 / `verilator_e2e` 38 / non-perf 191 |
| 编译 | ✅ 0 error；warning 只剩 `include/utils/logger.h:194 nonnull-compare`（**既有噪声**：强制重编**未改动**的 `test_module` 同样出现 3 处） |

## 5. 顺带订正（**上一 commit 里的不准确说法**）

`8b40b1e` 的 commit message 与 `AGENTS.md` NOTES 把
`tests/test_verilator_complex_e2e.cpp:119-170` 称为「**正确形态范例**」——
⚠️ **该范例自身也有 last-wins 多驱动问题**（`shift->next` 在 IDLE `:134` 与
TRANSFER `:153` **两处**都发射 ⇒ TRANSFER 后发射 ⇒ **覆盖 IDLE 的加载**
⇒ 「IDLE 载入 `data_in`」**永不生效**；`bit_count->next` 同理）
⇒ 且它的断言是 **parity**（`REQUIRE(sa == sb)`）⇒ 两后端**同构** ⇒ **抓不到**
⇒ ⇒ 已在 `AGENTS.md` NOTES 与本 change 的 NOTES 中**订正**。

⇒ ⇒ **教训**：正确形态需要 **4** 条纪律（本文 §2 的 3 条 + 「不把 parity 当正确性证据」）。

## 6. 范围外（**明确不做**）

- **不**动框架 API（`ch_state_machine` / `ch_reg` 的 `next` 语义）——
  「`next` 多驱动」是**框架层能力缺口**，独立立项；
- **不**修 `test_verilator_complex_e2e.cpp` 的 SPI 多驱动缺陷（独立立项）；
- **不**把 example 升级为 Catch2 测试（会改 `CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES`
  锚点 ⇒ 独立立项）；
- **不**扩 `example-assertion-quality` 的适用范围（**planner** 议题：
  main() 型 example 的语义输出目前无人校验）。
