---
name: spi-fsm-fixture-live-logic-2026-10
priority: P2
source: uart-tx-fsm-live-logic-2026-10（实施期实测挖出 —— 该 change 的 NOTES 顺带订正了「本文件是正确形态范例」的错误说法）
phase: phase-5
category: testing
type: bugfix
created_at: 2026-10-29
---

# Improvement: `test_verilator_complex_e2e.cpp` 的 SPI fixture 从未真正工作过

- **Date drafted**: 2026-10-29
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，归属 `feat-dual-backend-coverage` 的「**覆盖质量**」缺口
- **来源**：`uart-tx-fsm-live-logic-2026-10` 的 NOTES 顺带订正
  （该 change 的 commit message 明确登记：`test_verilator_complex_e2e.cpp`
  的 SPI fixture **自身也有** last-wins 多驱动问题 ⇒ 「正确形态范例」**不准确**）

## 1. Why (背景与动机)

`tests/test_verilator_complex_e2e.cpp::ComplexE2E - SpiFsmStateTransitionTraceParity`
是 capability `verilator-e2e-coverage`「代表性复杂设计」场景（RV32I 流水线 +
SPI 控制器 FSM）的**唯一**实现。

⚠️ **实测：它的 DUT 从未真正工作过。**
（探针 `/tmp/opencode/probe_spi.cpp`，interpreter 后端，逐拍打印）

| 观测量 | 实测值 | 应有值 |
|---|---|---|
| `shift`（8-bit 移位寄存器） | **恒 `0x00`** | 装载发送字节并逐拍左移 |
| FSM 状态 | **卡死在 `TRANSFER(2)`** | IDLE→START→TRANSFER×8→STOP→IDLE |
| `mosi` | **恒 0** | `1,0,0,1,0,1,1,0`（`0x96` 的 MSB-first） |
| `bit_count` | **从不 reset** | 装载时清零，TRANSFER 逐拍 +1 |

⇒ ⇒ **三个叠加的根因**（全部**实测**确认，非推理）：

1. **`reg->next` 是 last-wins**（`regimpl::set_next` = `set_src(0, next)`）
   ⇒ `shift->next` 在 IDLE 与 TRANSFER **各写一次** ⇒ `build()` 遍历所有 state
   ⇒ TRANSFER 后发射 ⇒ **覆盖** IDLE 的「装载 `data_in`」⇒ `shift` 恒 0；
2. **三元 `? :` 在建图期求值** ⇒ `transition_when` 的 target MUST 是**编译期常量**
   ⇒ `last_bit ? STOP : TRANSFER` 恒取 `TRANSFER` ⇒ **`STOP` 永不可达**；
3. **断言只有 parity** ⇒ 两后端由**同一份** fixture 生成 ⇒ 缺陷**同构**
   ⇒ 两边**同时错** ⇒ parity **恒绿** ⇒ 长期「假绿」。

⇒ ⇒ 与 `uart_tx` 的 FSM **同型**（`uart-tx-fsm-live-logic-2026-10` 已确立
**正确形态的 5 条纪律**）⇒ ⇒ 本 change 首次把它实现出来
+ 把断言从 parity 升级为**手推语义金标**。

⚠️ **附带发现（历史欠账）**：该文件**历史上已被改过两次**
（`48ed836` **+140** 行、`957758f` **18** 行）却**从未**加过
`dual-backend-test-coverage` R9 的例外 ⇒ **主 spec 与实现已 drift**
⇒ 本 change 的 R9 delta 是**补登记 + 扩展**。

## 2. What Changes (变更范围)

### 范围内

把 `SpiFsmFixture::describe()` 重写为**正确形态**（5 条纪律）：

| 现状（坏） | 改成 |
|---|---|
| `shift->next` 在 IDLE 与 TRANSFER **各写一次** | **单**一 next 写入点（`select` 组合，写在 `sm.build()` **之后**） |
| `bit_count->next` 在 IDLE 与 TRANSFER **各写一次** | 同上 |
| `transition_when(cond, last_bit ? STOP : TRANSFER)` | **两条** `transition_when`（`last_bit`→STOP / `!last_bit`→TRANSFER） |
| `start_latch` 在 `on_active` 闭包里**现场**建寄存器 | 提到 `describe()` 顶层 |
| 断言只有 `REQUIRE(sa == sb)` | parity **+** **手推**语义金标（状态 / 计数器 / `mosi` 16-bit 序列） |

### 范围外

- **不**改框架 API（`ch_state_machine::build()` 的遍历语义 / `ch_reg` 的
  `next` last-wins 语义）⇒ **框架层能力缺口** ⇒ 独立立项；
- **不**新增测试文件 ⇒ **计数锚点不变**；
- **不**碰 RV32I fixture 与 TEST_CASE 名 / SECTION 结构（R9 要求）。

## 3. Acceptance (验收标准)

- [ ] `describe()` 是**正确形态**（5 条纪律）；
- [ ] **语义断言**（**手推**金标，不得抄仿真输出）：
      状态序列 MUST 等于
      `IDLE, START, TRANSFER×8, STOP, IDLE, IDLE, START, TRANSFER×8, STOP, IDLE…`
      ⇒ **每次传输都必过 STOP**；
      `mosi` 的 8 bit 序列 MUST == **`0x96`** 的 MSB-first（`1,0,0,1,0,1,1,0`）；
      `bit_count` MUST 在**第 2 次**装载时复位（⇒ 传输次数 MUST ≥ **2**）；
- [ ] 两后端**都** MUST 满足金标；
- [ ] **变异验证 ≥ 9 条**（每条先确认**编译成功**）⇒ **9/9 全被抓**；
- [ ] ⭐ **「旧 fixture + 新断言 ⇒ 转红」**；
- [ ] `ctest -LE perf` **191/191**；`run_all_ported_tests.sh` **28/28**；
      `run_all_samples.sh` **35/35**；
- [ ] **计数锚点不变**（202 / 17 / 38 / 191）。

## 4. Capabilities (涉及的能力维度)

- **MODIFIED ×1**：`dual-backend-test-coverage` 的「现有 9 个 verilator 测试 MUST
  保留原状」⇒ 增补第 **3** 条例外（SPI fixture MUST 被允许**受限**变更）。
- **MODIFIED ×1**：`verilator-e2e-coverage` 的「代表性复杂设计在 Verilator 仿真下
  行为契约」⇒ SPI 场景 MUST 断言**绝对**语义金标，**MUST NOT** 仅断言 parity；
  顺带**收紧**发送字节（`0xA5` 是**回文** ⇒ 位序维度零判别力 ⇒ 改 `0x96`）
  与传输次数（1 → 2 ⇒ 复位才可观测）。

## 5. Impact (影响面分析)

- **改动文件**：`tests/test_verilator_complex_e2e.cpp`（1 个）
- **计数**：**不变**（不新增 ctest 条目 / 不新增测试文件）
- **爆炸半径**：**零**（只改测试 fixture ⇒ 不影响任何库/框架/产品代码）
- **风险**：**中** —— 首次实现一个从未工作的 FSM ⇒ 期望值 MUST **手推**
  ⇒ 且 MUST 有变异验证证明断言有判别力
- **⚠️ 判别力踩坑（2 处实测）**：① `0xA5` 是**回文** ⇒ 位序维度零判别力
  ⇒ 改 `0x96`；② 单次传输时「装载时复位计数器」**不可观测** ⇒ 加第 2 次传输。
