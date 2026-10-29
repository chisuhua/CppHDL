# 提案：`uart_tx_example` 的状态机从未工作过 ⇒ 首次实现正确行为

- **架构依据**：无（example 层 bugfix；不改框架、不改契约）
- **5 段草案**：`.rddf/improvements/uart-tx-fsm-live-logic-2026-10.md`
- **roadmap 归属**：examples 真实性缺口（Unmapped ③）
- **规格影响**：**无** ⇒ 归档用 `--skip-specs`
- **规模 / 风险**：**中**（重写一个 FSM + 加自检断言 + 变异验证）

## Why

`examples/spinalhdl-ported/uart/uart_tx.cpp` 是**被跟踪**的 example，但它
**从未工作过**（生成的 Verilog 里输出是**两个常量**、状态寄存器**自环**、
`is_in` 比较节点**零消费者**）⇒ 且 **exit 0** ⇒ 运行护栏判 PASS ⇒ **长期「假绿」**。

⚠️ **三个叠加的根因**（详见 `design.md` §1）：

1. **`if (ch_bool)` / `if (ch_uint == lit)` / 三元 `? :` 建图期求值**
   ⇒ 条件逻辑全不发射（框架 `include/chlib/state_machine.h:34-38` **已文档化**）；
2. **多状态重复驱动同一 `ch_out`** ⇒ `set_src(0,…)` 覆盖 ⇒ **最后发射者获胜**；
3. **`reg->next` 是 last-wins** ⇒ 「每状态各写一次」**无法**表达多状态 FSM。

⇒ ⇒ **正确形态需要 5 条纪律**（见 `design.md` §2）⇒ ⇒ 本 change 首次实现它
+ 加**自检断言**（⇒ 该 example 从「恒绿」变成「可红」）。

## What Changes

### 范围内

把 `UartTx::describe()` 重写为**正确形态**（5 条承重约束 + 手推期望值）：

| 现状（坏） | 改成 |
|---|---|
| `if (cond) { sm.transition_to(S); }` | `sm.transition_when(cond, S);`（`on_active` 里**只**写 transition） |
| `if (cond) { reg->next = A; } else { reg->next = B; }` | `reg->next = select(cond, A, B);` |
| 4 个状态各写同一 `reg->next` | **每寄存器一个写入点**（`select(is_in(S), v_S, …)`），写在 `sm.build()` **之后** |
| 4 个状态各写同一 `ch_out` | **只驱动一次**（`select(is_in(…), …)`） |
| 数据位发 `shift_reg & 1`（= `bit0` = 起始位） | 发 `(shift_reg >> 1) & 1`（= `bit1..bit8` ⇒ **完整** 8 位） |

⚠️ **off-by-one 的裁决**：原注释写「移位寄存器（**9 位：起始位 + 8 数据位**）」
⇒ `bit0` = 起始位、`bit1..bit8` = 数据位 ⇒ ⇒ **期望发 `bit1..bit8`**
⇒ 原代码发 `bit0` ⇒ **少发 `data[7]`、多发一个起始位 0** ⇒ ⇒ 对 `data[7]=0`
的值**碰巧正确** ⇒ ⇒ 本 change 改发 `(shift_reg >> 1) & 1`
⇒ ⇒ 因该 FSM **从未工作过** ⇒ 不存在「既有行为」需保持 ⇒ 属「**首次实现**正确行为」。

### 范围外

- **不**改 `samples/` 的 6 处（等 ADR，见 `input-port-assignment-antipattern-fix-2026-10`）；
- **不**把 example 升级为 Catch2 测试（那会改 `CTEST_TOTAL`/`EXAMPLES_TEST_SOURCES`
  锚点 ⇒ 独立立项）；
- **不**扩 `example-assertion-quality` 的适用范围（**planner** 议题）；
- **不**动 `ch_state_machine` / `ch_reg` 的框架 API（那属**框架契约变更**）。

## Acceptance

- [ ] 生成的 Verilog 里**不再有**「状态寄存器自环」（`q <= q`）；
- [ ] 生成的 Verilog 里 `is_in(状态)` 的比较节点**有消费者**（不再是死节点）；
- [ ] `uart_tx` **不再是常量**；
- [ ] **自检断言**（不匹配 ⇒ `return 1` ⇒ 守护脚本能抓到）：
      以 `busy` 上升沿为帧起点、每 **434** 拍采样 ⇒ `uart_tx` 序列 MUST 等于
      「起始位 `0` + `0xA5` 的 8 位 LSB-first + 停止位 `1`」；
- [ ] **判别性**：数据 MUST 用 **`0xA5`**（`bit7=1`）⇒ `0x55`（`bit7=0`）**抓不到**
      off-by-one ⇒ **不可**用 `0x55`；
- [ ] **变异验证 ≥ 5 条**（每条先确认**编译成功**，否则跑的是旧二进制 ⇒ 假绿）：
      off-by-one / 位序反 / 起始位错 / 停止位错 / 波特分频错；
- [ ] `run_all_ported_tests.sh` **28/28**；`run_all_samples.sh` **35/35**；
      `ctest -LE perf` **191/191**；
- [ ] **计数锚点不变**：`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 /
      `verilator_e2e` 38 / non-perf 191；
- [ ] 根 `AGENTS.md` NOTES 记录（并**订正**上一 commit 里把
      `test_verilator_complex_e2e.cpp` 称为「正确形态范例」的说法 ——
      它**自身**也有 last-wins 多驱动问题）。

## Capabilities

- **不涉及**：本 change **不改任何契约**（example 的可观察契约此前是「什么都不做」
  ⇒ 现在首次真实工作 ⇒ 无条款被改动）⇒ 归档用 `--skip-specs`
- ⚠️ `example-runtime-coverage` / `example-assertion-quality` 逐字核对后判定
  **无需 MODIFIED**（前者只管「纳入运行护栏」，后者只管「**examples 内的测试**
  MUST 断言语义输出」⇒ main() 型 example 不在其内 ⇒ 该**边界缺口**属 planner 议题）

## Impact

- **改动文件**：`examples/spinalhdl-ported/uart/uart_tx.cpp`（1 个）
- **计数**：**不变**（不新增/删除 ctest 条目；跟踪集合不变）
- **爆炸半径**：**零**（example 代码，无库/框架改动 ⇒ 不影响任何测试）
- **风险**：**中** —— 首次实现一个从未工作的 FSM ⇒ 期望值 MUST **手推**
  （不得抄仿真输出）⇒ 且 MUST 有变异验证证明断言有判别力

## Non-Goals

- **不**改框架 API（`ch_state_machine` / `ch_reg` 的 `next` 语义）
- **不**修 `tests/test_verilator_complex_e2e.cpp` 的 SPI 多驱动缺陷（独立立项）
- **不**升级为 Catch2 测试（会改计数锚点）
