# 提案：`test_verilator_complex_e2e.cpp` 的 SPI fixture 从未真正工作过 ⇒ 首次实现正确行为

- **架构依据**：无（**测试层** bugfix；不改框架 API、不改库行为）
- **5 段草案**：`.rddf/improvements/spi-fsm-fixture-live-logic-2026-10.md`
- **roadmap 归属**：`feat-dual-backend-coverage`（双后端覆盖的**质量**缺口：
  parity 断言恒绿，但 DUT 是坏的）
- **规格影响**：**有**（**2** 个 MODIFIED delta）⇒ 归档**禁**用 `--skip-specs`
- **规模 / 风险**：**中**（重写一个 FSM + 把 parity 断言升级为语义断言 + 变异验证）

## Why

`tests/test_verilator_complex_e2e.cpp::ComplexE2E - SpiFsmStateTransitionTraceParity`
是 capability `verilator-e2e-coverage`「代表性复杂设计」场景的**唯一**实现。
⚠️ **实测：它的 DUT 从未真正工作过。**

探针（`/tmp/opencode/probe_spi.cpp`，复现同一份 fixture 形态，interpreter 后端，
逐拍打印）实测结果：

| 观测量 | 实测值 | 应有值 |
|---|---|---|
| `shift`（8-bit 移位寄存器） | **恒 `0x00`** | 载入发送字节并逐拍左移 |
| FSM 状态 | **卡死在 `TRANSFER(2)`** | IDLE→START→TRANSFER×8→STOP→IDLE |
| `mosi` | **恒 0** | `1,0,0,1,0,1,1,0`（`0x96` 的 MSB-first） |
| `bit_count` | **从不 reset** | IDLE 载入时清零，TRANSFER 逐拍 +1 |

⇒ ⇒ **三个叠加的根因**（详见 `design.md` §1）：

1. **`reg->next` 是 last-wins**（`regimpl::set_next` = `set_src(0, next)`，
   `include/ast/ast_nodes.h:47-54`）⇒ `shift->next` 在 IDLE 与 TRANSFER **两处**
   都发射 ⇒ TRANSFER 后发射 ⇒ **覆盖** IDLE 的「载入 `data_in`」
   ⇒ ⇒ 「IDLE 载入 `data_in`」**永不生效** ⇒ `shift` 恒 0；
2. **三元 `? :` 在建图期求值** ⇒ `transition_when` 的 target MUST 是**编译期常量**
   （`ch_state_machine::build()` 按 state 遍历 `on_active` 注册转移）⇒ 原代码
   `last_bit ? STOP : TRANSFER` 的 `last_bit` 在**建图期**被求值
   （`ch_bool::operator uint64_t()` **只对 const 节点返真值**）⇒ 恒取 `TRANSFER`
   ⇒ ⇒ **`STOP` 永不可达** ⇒ 状态卡死；
3. **断言只有 parity**（`REQUIRE(sa == sb)`）⇒ 两后端**同构** ⇒ **同构缺陷恒被
   parity 掩盖** ⇒ 「全绿」不等于「对」。

⇒ ⇒ 与 `uart_tx` 的 FSM **同型**（上一 change `uart-tx-fsm-live-logic-2026-10`
已确立**正确形态的 5 条纪律**）⇒ ⇒ 本 change 首次把它实现出来
+ 把断言从 parity 升级为**手推语义金标**。

## What Changes

### 范围内

把 `SpiFsmFixture::describe()` 重写为**正确形态**（5 条纪律 + 手推期望值）：

| 现状（坏） | 改成 |
|---|---|
| `shift->next` 在 IDLE 与 TRANSFER **各写一次** | **单**一 next 写入点（`select` 组合，写在 `sm.build()` **之后**） |
| `bit_count->next` 在 IDLE 与 TRANSFER **各写一次** | 同上 |
| `transition_when(cond, last_bit ? STOP : TRANSFER)` | **两条** `transition_when`（`last_bit` → STOP / `!last_bit` → TRANSFER；target MUST 是编译期常量） |
| `start_latch` 在 `on_active` 闭包里**现场**建寄存器 | 提到 `describe()` 顶层（闭包只写 `transition_when`） |
| 断言只有 `REQUIRE(sa == sb)` | parity **+** **手推**语义金标（状态序列 / 计数器 / `mosi` 8-bit 序列） |

⚠️ **不引入新端口**：`dbg_state` / `mosi_out` / `dbg_bit_count` 三个观测点已足够
⇒ ⇒ `ch_out` 端口数不变 ⇒ Verilator 侧的可观测面不变。

### 范围外

- **不**改 `ch_state_machine` / `ch_reg` 的框架 API（「`reg->next` 多驱动」是
  **框架层能力缺口** ⇒ 独立立项，planner 议题）；
- **不**改 `examples/spinalhdl-ported/spi/spi_controller.cpp`（那是 **example 层**的
  手写 FSM，与本 fixture 无共享代码）；
- **不**新增测试文件 ⇒ **计数锚点不变**；
- **不**碰 `tests/test_verilator_complex_e2e.cpp` 的 RV32I fixture 与 TEST_CASE 名
  （R9 要求 TEST_CASE 名 / SECTION 结构不变）。

## Acceptance

- [ ] `SpiFsmFixture::describe()` MUST 是**正确形态**：条件逻辑只用硬件原语、
      每寄存器**单** next 写入点、每输出**只**驱动一次、**禁**三元选 target；
- [ ] **语义断言**（**手推**金标，不得抄仿真输出）：
      FSM 状态序列 MUST 等于
      `IDLE, START, TRANSFER×8, STOP, IDLE, IDLE, START, TRANSFER×8, STOP, IDLE…`
      （⇒ **每次传输都必过 STOP**）；`mosi` 的 8 bit 序列 MUST == **`0x96`** 的
      MSB-first（`1,0,0,1,0,1,1,0`；⚠️ 发送字节 MUST **非回文** —— `0xA5` 是回文 ⇒
      MSB-first 与 LSB-first **相同** ⇒ 位序维度**零判别力** ⇒ 抓不到位序错）；
      `bit_count` 序列 MUST 等于 `0,0,0,1,2,3,4,5,6,7,7,7,7,0,0,1,…`
      （⇒ 复位**可观测** ⇒ 传输次数 MUST ≥ **2**）；
- [ ] 两后端**都** MUST 满足上述金标（不只是彼此相等）；
- [ ] **变异验证 ≥ 9 条**（每条先确认**编译成功**，否则跑的是旧二进制 ⇒ 假绿）：
      装载失效 / target 三元 / STOP 不可达 / 位序反 / 移位量 / 计数不复位 /
      少发一位 / 输出门控缺失 / 计数溢出；
- [ ] ⭐ **「旧 fixture + 新断言 ⇒ 转红」**（假绿被打破的直接证据）；
- [ ] `ctest -LE perf` **191/191**；`run_all_ported_tests.sh` **28/28**；
      `run_all_samples.sh` **35/35**；
- [ ] **计数锚点不变**：`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 /
      `verilator_e2e` 38 / non-perf 191；
- [ ] 12 个 `scripts/verify_*.sh`（含 3 个 `--self-test`）全 exit 0。


## Capabilities

- **MODIFIED ×1**：`dual-backend-test-coverage` 的「现有 9 个 verilator 测试 MUST
  保留原状」⇒ 增补第 **3** 条例外（`test_verilator_complex_e2e.cpp` 的 SPI fixture
  MUST 被允许**受限**变更：修 fixture + 把断言升级为语义金标；TEST_CASE 名 /
  SECTION 结构 MUST NOT 变化）。
  ⚠️ **前置**：本文件**历史上已被改过两次**（`48ed836` +140 行、`957758f` 18 行）
  却**从未**加过例外 ⇒ **主 spec 与实现已 drift**（详见 `design.md` §5）
  ⇒ 本 delta 是**补登记 + 扩展**，不是**新开**口子。
- **MODIFIED ×1**：`verilator-e2e-coverage` 的「代表性复杂设计在 Verilator 仿真下
  行为契约」⇒ SPI 场景 MUST 断言**绝对**语义金标（状态序列 MUST 经过 STOP /
  `mosi` 8-bit 序列 MUST == 发送字节的 MSB-first / 位计数 MUST 在第 2 次装载时
  复位），**MUST NOT** 仅断言 parity
  ⇒ ⚠️ **两后端同构 ⇒ 同构缺陷恒被 parity 掩盖**。
  ⇒ 顺带**收紧**两处：① 发送字节 `0xA5` → **`0x96`**（`0xA5` 是**回文** ⇒
  位序维度零判别力）；② 传输次数 **1 → 2**（单次传输时「装载时复位计数器」
  **不可观测** ⇒ 该缺陷类零判别力）。

## Impact

- **改动文件**：`tests/test_verilator_complex_e2e.cpp`（1 个）
- **计数**：**不变**（不新增/删除 ctest 条目；不新增测试文件 ⇒ `EXAMPLES_TEST_SOURCES` 不变）
- **爆炸半径**：**零**（只改测试 fixture ⇒ 不影响任何库/框架/产品代码）
- **风险**：**中** —— 首次实现一个从未工作的 FSM ⇒ 期望值 MUST **手推**
  （不得抄仿真输出）⇒ 且 MUST 有变异验证证明断言有判别力

## Non-Goals

- **不**改框架 API（`ch_state_machine` 的 `build()` 遍历语义 / `ch_reg` 的
  `next` last-wins 语义）—— 那是**框架层能力缺口** ⇒ 独立立项；
- **不**改 `examples/spinalhdl-ported/spi/spi_controller.cpp`；
- **不**新增 Catch2 测试文件（会改 `CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` 锚点）；
- **不**扩 `example-assertion-quality` 的适用范围（**planner** 议题）。
