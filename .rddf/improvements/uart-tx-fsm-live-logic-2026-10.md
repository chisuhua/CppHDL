---
name: uart-tx-fsm-live-logic-2026-10
priority: P2
source: input-port-assignment-antipattern-fix-2026-10（实施期实测挖出）
phase: phase-5
category: testing
type: bugfix
created_at: 2026-10-28
---

# Improvement: `uart_tx_example` 的状态机从未工作过（C++ `if` 恒走 else）

- **Date drafted**: 2026-10-28
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，归属 feature 的「examples 真实性」缺口
- **来源**：`input-port-assignment-antipattern-fix-2026-10` 的 `design.md` §4
  （该 change 只修 4 处失效赋值，**不**修本缺陷）

## 1. Why (背景与动机)

`examples/spinalhdl-ported/uart/uart_tx.cpp` 是**被跟踪**的 example
（`CH_TRACKED_EXAMPLE_TARGETS` ⇒ 受 `run_all_ported_tests.sh` 守护，28/28 之一）。

⚠️ **实测：它生成的设计图里，输出是两个常量，状态机自环。**
（复现：在 `/tmp/opencode/uart/` 下运行该 binary ⇒ 生成 `uart_tx.v`）

```verilog
assign top_uart_tx_inst_bool_lit   = 1'b1;   // uart_tx ≡ 1
assign top_uart_tx_inst_bool_lit_1 = 1'b0;   // busy   ≡ 0

always_ff @(posedge default_clock or posedge default_reset) begin
    if (!default_reset) top_uart_tx_inst_reg_3 <= top_uart_tx_inst__reg_3;
    else                top_uart_tx_inst_reg_3 <= 2'h0;   // 状态寄存器自环
end
assign top_uart_tx_inst__reg_3 = top_uart_tx_inst_reg_3;
```

而 `is_in(状态)` 的 4 个比较节点（`eq_eq_4`..`eq_eq_7` = `reg_3 == 0/1/2/3`）
**被发射但零消费者**（死节点）。

### 1.1 根因

`ch_bool` / `ch_uint` 的 `operator uint64_t()` **只对 const 节点返回真值**：

```cpp
// src/lnode/bool.cpp:44-49
ch_bool::operator uint64_t() const {
    if (this->node_impl_ && this->node_impl_->is_const()) return lit value;
    return 0;          // ← 非 const 节点（任意运算结果）恒 0
}
```

⇒ `if (io().frame_start == ch_bool(true))` 里的 `==` 是**非 const 节点**
⇒ **建图期恒为 false** ⇒ C++ 语义下走 **else**。

⇒ ⇒ 两个后果（都被 `uart_tx.v` 证实）：

| `if` 形态 | 后果 |
|---|---|
| **无 else**（IDLE 的 `if (…) { load + transition_to(START) }`） | 分支体**从不发射** ⇒ `frame_start` 比较成死节点、`transition_to` **从未注册** |
| **有 else**（START/DATA/STOP 的 `if (baud==433) {…} else {baud+1}`） | **只发射 else** ⇒ 计数器**永远自增、永不归零**（Verilog 里 `reg <= reg + 1`） |

### 1.2 为什么 `uart_tx` 是「常量 1」

`ch_state_machine::build()`（`include/chlib/state_machine.h:292-304`）**遍历所有状态**
执行各自 `on_active` 一次；4 个状态都**无条件**写 `io().uart_tx` / `io().busy`，
而 `ch_out::operator=` 是 `set_src(0, src)`（**覆盖**）⇒ **最后发射的 STOP 获胜**：

| 顺序 | 状态 | `uart_tx` | `busy` |
|---|---|---|---|
| 1 | IDLE | `1` | `0` |
| 2 | START | `0` | `1` |
| 3 | DATA | `(shift_reg & 1) != 0` | `1` |
| 4 | **STOP**（生效） | **`1`** | **`0`** |

⇒ 与实测（`uart_tx=1` / `busy=0` 恒定 4500 拍）**逐字吻合**。

### 1.3 为什么长期无人发现

- 该 example **exit 0** ⇒ `run_all_ported_tests.sh`（只查退出码）**判 PASS**；
- 它只**打印**信号，**零断言**；
- 它**不是** Catch2 测试 ⇒ 不在 `example-assertion-quality` capability 的
  「examples 内的测试 MUST 断言语义输出」范围内。

⇒ ⇒ 这是「**绿 ≠ 对**」的又一实例（先例：`fifo_bundle` 的读通路、
`i_cache_prefetch.h` 的分支不发射）。

### 1.4 ⚠️ 同一只坑的**三种形态**（全仓扫描实测，2026-10-28）

`operator uint64_t()` 只对 const 节点返回真值 ⇒ **任何把硬件表达式塞进
C++ 控制流的写法都恒走「假」分支**。实测仓内出现三种形态：

| # | 形态 | 位置 | 现状 |
|---|---|---|---|
| ① | `if (ch_bool_expr)` | `uart_tx.cpp:72` 等 **5 处**（examples 内**唯一**的文件） | **坏**（§1.1） |
| ② | `if (ch_uint == lit)` | 同上（`baud_counter == 433` ×3、`bit_counter == 7` ×1） | **坏**（只发射 else） |
| ③ | **三元条件** `cond ? A : B` | `tests/test_verilator_complex_e2e.cpp:150` 的 `last_bit ? SpiState::STOP : SpiState::TRANSFER` | ⚠️ **`STOP` 状态不可达**（恒选 `TRANSFER`） |

⚠️ **形态 ③ 为什么至今是绿的**：该 TEST_CASE 的断言是
`REQUIRE(sa == sb)`（interpreter 与 verilator 的 `dbg_state` **逐拍相等**）——
而「`STOP` 不可达」在**建图期**就决定了 ⇒ **两个后端的图完全相同**
⇒ ⇒ parity **恒真** ⇒ **抓不到**
⇒ ⇒ 这是「**parity 通 ≠ 正确**」的又一实例（同
`feat-rv32i-isa-completeness` 的核心论点）。

⇒ ⇒ **正确形态的既成范例**在同一个文件里（`test_verilator_complex_e2e.cpp:119-170`）：

```cpp
sm.transition_when(start_now, SpiState::START);                  // 不用 if
shift->next = select(start_now, io().data_in, shift);            // 不用 if/else
bit_count->next = select(start_now, make_literal<0>(), bit_count);
ch_bool in_transfer = sm.is_in(SpiState::TRANSFER);              // 输出按状态多路选择
io().mosi_out = select(in_transfer, msb, ch_bool(false));        // 且**只驱动一次**
```

⇒ 该范例**避开**了 `uart_tx` 的三个错误（`if` 条件、`if/else` 赋值、多状态重复驱动
同一 `ch_out`）⇒ ⇒ 本 issue 的实施应**照此改写**。
⚠️ 但形态 ③（三元）**同文件里也有**（`:150`）⇒ 照抄范例时 MUST **同时**修掉它
（改成 `transition_when(last_bit_cond, STOP)` 与 `transition_when(!last_bit_cond, TRANSFER)`
两条，或用 `select` 生成目标枚举）。

## 2. What Changes (变更范围)

⚠️ **方案未定**（可按 §2.1–§2.3 三步走，也可拆分）：

### 2.1 修 FSM（核心）

框架**已有**正式 API（`include/chlib/state_machine.h:34-48` 已文档化这只坑）：

| 现状（坏） | 应改成 |
|---|---|
| `if (cond) { sm.transition_to(S); }` | `sm.transition_when(cond, S);`（发射 select 树） |
| `if (cond) { reg->next = A; } else { reg->next = B; }` | `reg->next = select(cond, A, B);` |
| `if (cond) { out = X; }`（`ch_out`） | `out <<= select(cond, X, out_hold);` 或按状态多路选择 |
| 4 个状态各自无条件写同一个 `ch_out` | **只驱动一次**：`out <<= select(sm.is_in(S0), v0, select(sm.is_in(S1), v1, …))` |

⚠️ **承重约束**：`sm.is_in(S)` 读的是 `state_reg`（构造时即存在）⇒ 可在
`build()` **之后**用；`on_active` 在 `build()` 内执行 ⇒ **不要**在 `on_active`
里读 `is_in` 再赋同一状态机控制的输出（会引入顺序依赖）⇒ 建议在
`sm.build()` **之后**统一做组合输出。

### 2.2 加**自检断言**（否则「修好」不可验证）

该 example 是 main() 程序 ⇒ 用「不匹配则 `return 1`」的方式加自检：

- 采集发送期间 `uart_tx` 的**位序列** ⇒ MUST 等于
  「起始位 `0` + `0x55` 的 8 位 LSB-first（`1,0,1,0,1,0,1,0`）+ 停止位 `1`」；
- 每位**持续时间** MUST = 波特率分频（434 拍 ±1）；
- `busy` MUST 在 START→STOP 期间为 1、在 IDLE 为 0；
- 帧完成后 MUST 回到 IDLE（`busy=0`、`uart_tx=1`）。

⚠️ 期望值**手推**（不得抄仿真输出）。

### 2.3 可选：把该 example 升级为 Catch2 测试

⇒ 这样它就能被 `ctest` 与 `dual-backend` 体系覆盖（而非只靠退出码）。
⚠️ 但那会改变 `CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` 锚点 ⇒ 需同步。

## 3. Acceptance (验收标准)

- [ ] 生成的 Verilog 里**不再有**「状态寄存器自环」（`q <= q`）
- [ ] 生成的 Verilog 里 `is_in(状态)` 的比较节点**有消费者**（不再是死节点）
- [ ] `uart_tx` **不再是常量** ⇒ 一帧内的位序列逐位匹配手推期望
- [ ] `busy` 的时序匹配（START→STOP 期间为 1）
- [ ] **自检断言**存在且**判别力可证**（变异验证：任一位错、位宽错、
      波特率分频错 ⇒ MUST 转红）
- [ ] `bash scripts/run_all_ported_tests.sh` 全绿（28/28，或升级为 CTest 后
      同步锚点与跟踪集合）
- [ ] 若把 example 升级为 Catch2 测试 ⇒ **同步** `CTEST_TOTAL` /
      `EXAMPLES_TEST_SOURCES` 锚点 + 3 处引用（`AGENTS.md` NOTES /
      `scripts/verify_test_counts.sh` / `scripts/verify_example_test_wiring.sh`）

## 4. Capabilities (涉及的能力维度)

- `example-runtime-coverage` —— 可能 MODIFIED（若要把「main() 型 example 的
  **语义输出**」纳入护栏；当前它只管「纳入运行护栏 + 退出码」）
- `example-assertion-quality` —— 可能 MODIFIED（其 Requirement 写的是
  「**examples 内的测试** MUST 断言语义输出」⇒ 需裁决 main() 型程序是否纳入）
- ⚠️ 两者都是**扩契约** ⇒ 需 planner 裁决（build 不擅改）

## 5. Impact (影响面分析)

- **改动文件**：`examples/spinalhdl-ported/uart/uart_tx.cpp`（1 个）
- **计数**：视 2.3 是否执行 ⇒ 若升级为 Catch2 则 `CTEST_TOTAL` +1
- **风险**：**中** —— 重写 FSM 会改变生成 Verilog 的结构（当前是死图）；
  且该 example 是 17 个 spinalhdl-ported 之一 ⇒ 其修法可作为**其余 16 个的模板**
  （需**先**盘点其余 16 个是否同型，因为 `if (ch_bool)` 是已文档化的坑）
- **下游**：⚠️ **建议先做一次全仓扫描** —— 其余 16 个 `spinalhdl-ported` example
  是否也有 `if (ch_bool)` / `if (ch_uint == lit)` 形态？
  若有，本 issue 的范围远大于 1 个文件。
