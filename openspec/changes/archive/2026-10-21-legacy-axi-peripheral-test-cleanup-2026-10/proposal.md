# Change: legacy-axi-peripheral-test-cleanup-2026-10 — 清理 3 个遗留 AXI peripheral 测试（死代码 + 永久复位）+ 补 `rst_n` 接线护栏

## Why

2026-10-21 实施 `axi4-full-generic-slave-burst-2026-10` 时做了一次全仓
「`rst_n` 接线」清点（新增必需端口 → 未接线的实例化点 = **永久复位** = 零编译错误的
静默破坏），连带发现 3 个**遗留 AXI4-Lite peripheral 测试**已经是空壳：

| 文件 | 事实（实测） |
|---|---|
| `tests/test_axi_spi.cpp` | `axi_lite_write` / `axi_lite_read` 两个 helper **定义后零调用**（死代码）；其中含 **5 处无界** `while (ready == 0) tick();` 握手等待；用例名叫 `SpiTransfer8Bit` / `FullTransactionSequence` 但**从不发起任何 AXI 事务**（只断言端口 impl 非空 + tick 100 次）；不驱动 `rst_n` → DUT 恒为复位态 |
| `tests/test_axi_pwm.cpp` | 同样 2 个**零调用**的死 helper；不驱动 `rst_n` |
| `tests/test_axi_i2c.cpp` | **零** `set_input_value`（只查 name / 端口 / verilog 生成）；唯一的 `ch_device` 只用于 **codegen**（不建 Simulator）→ 该文件当前**没有仿真用例**，故不构成「永久复位」违规（但缺 `rst_n` 仍是未来扩展时的隐患） |

三重问题：

1. **零编译错误的静默破坏**：这三个文件实例化的 DUT `rst_n` 未接线 → 读出为 0 →
   **恒为永久复位**。今天不炸，是因为这些用例**根本不驱动任何信号**；
   一旦有人按用例名所暗示的语义去补事务，就会踩「永久复位 + 无界等待」=
   **hang 到 ctest TIMEOUT**（而不是断言失败）。
2. **死代码违反 ZERO-DEBT POLICY**（"No dead code: Delete stubs, skeleton files, and
   unused includes immediately"）：两个零调用的 helper 带 5 处无界循环。
3. **覆盖是空的**：它们的 `_behavior` 双后端双胞胎
   （`test_axi_{spi,pwm,i2c}_behavior.cpp`）**已经**接了 `rst_n` 并真跑 AXI 事务
   → 这 3 个遗留文件已被完全覆盖，却仍以「interpreter-only baseline」的名义占位。

同时，**「未接线 `rst_n`」这个陷阱至今没有任何自动化护栏**：
它靠人记住（AGENTS.md 的 CONVENTIONS 里写了），下一次新增 peripheral 仍可能踩。
本 change 补上**两道护栏**：静态（脚本 + CI）与运行时（双后端回归用例）。

## What Changes

### D1: 删除死 helper（3 个文件）

- `tests/test_axi_spi.cpp`：删除 `axi_lite_write` / `axi_lite_read`
  （零调用 → 连带消除 5 处无界握手等待）
- `tests/test_axi_pwm.cpp`：删除同名 2 个零调用 helper

### D2: 让 DUT 不再处于永久复位态

所有**仿真**了 device（构造了 `Simulator`）的用例 MUST 驱动 `rst_n = 1`
（`sim.set_input_value(io().rst_n, 1)`）。
→ 这些 DUT 的复位 pattern（`select(!io().rst_n, <init>, ...)`）不再把状态钉在初值，
「tick 100 次状态机没卡死」这类断言才**有意义**。

⚠️ **只建 device 但不建 Simulator 的纯 codegen 用例不适用**（端口根本没被求值）——
`tests/test_axi_i2c.cpp` 的唯一 device 属此类；`tests/test_axi_dma.cpp` 更进一步
（只创建 standalone `Component`，从不仿真）→ 二者都**不是**违规点。

### D2.1: 护栏的判定边界（避免误报）

`scripts/verify_rst_n_wiring.sh` 的三条规则（详见脚本头注释）：

- **R1（per test case）**：**仿真**了带必需 `rst_n` 端口的组件的用例 MUST 驱动它。
  「仿真」的判据 = 构造了 `Simulator` **或** 使用 `DUAL_BACKEND_TEST_PAIR` 宏。
  驱动可以出现在用例体内，也可以经由**文件作用域 helper**
  （如 `gpio_init(sim, io)`）——护栏会解析 helper 并接受。
- **R2（per file）**：用 `ch_module` 包装带必需 `rst_n` 端口的组件的 wrapper
  MUST 透传该端口（`slave.io().rst_n <<= io().rst_n;`）。
- **R3（per file）**：文件级兜底：仿真了这类组件的文件 MUST 在某处驱动 `rst_n`。

⚠️ **只认真实驱动**（`set_input_value(...rst_n...)` / `rst_n <<=`）——
**注释里提到 `rst_n` 不算数**（否则护栏会被注释骗过，实测过）。
⚠️ 组件清单在脚本头部 `COMPONENTS_W_RST_N`，新增带 `rst_n` 的组件时 MUST 同步维护。

### D3: 静态护栏 —— `scripts/verify_rst_n_wiring.sh` + CI step

新增脚本：对全仓（排除组件自身定义目录）扫描「实例化了带**必需** `rst_n` 端口的组件」
的文件，MUST 同时出现 `rst_n`；否则报错退出 1。
挂到 CI（与 `scripts/verify_test_counts.sh` 并列，同 step 或紧邻 step）。

⚠️ 组件清单必须与「带必需 `rst_n` 端口」的组件清单**同步维护**：
`AxiLiteSlave` / `Axi4Slave` / `AxiToAxiLiteConverter` / `AxiLiteGpio` /
`AxiLiteUart` / `AxiLiteTimer` / `AxiLitePwm` / `AxiLiteI2c` / `AxiLiteSpi` /
`AxiDma`。

### D4: 运行时护栏 —— `tests/test_axi_rst_n_guard.cpp`

双后端（interpreter + verilator）用例：**故意不驱动 `rst_n`**，断言

- 写事务**不产生** B 响应（`bvalid` 恒 0），
- 目标寄存器**保持初值**（读回 0），
- 读事务**不产生** R 响应（`rvalid` 恒 0，`rdata` 恒 0）。

→ 把「未接线 = 永久复位」从「文档里的一句话」变成**可执行契约**。
（覆盖 `AxiLiteSlave` 与 `Axi4Slave` 两个通用 slave；6 个 peripheral 由 D3 的静态护栏覆盖。）

### D5: spec delta

`dual-backend-test-coverage` 的「AXI4 peripheral behavior test SHALL 走
DUAL_BACKEND_TEST_PAIR」Requirement 里那句
「原 `tests/test_axi_<peripheral>.cpp` MUST NOT 修改（interpreter-only baseline）」
→ 追加**受限例外**：允许对 `test_axi_{spi,pwm,i2c}.cpp` 做
「删除零调用死 helper + 驱动 `rst_n`」的清理，
**但 MUST NOT 削弱任何既有断言**、MUST NOT 改名 / 删除 TEST_CASE。

## Non-Goals（非目标）

1. **不删除这 3 个文件**：它们仍是「接口结构 / 组件创建 / verilog 生成」层面的
   interpreter-only baseline，删除会超出本 change 的范围。
2. **不把 AXI 事务测试搬进这 3 个文件**：真实的事务覆盖由 `_behavior` 双胞胎承担
   （避免两处重复维护同一套协议合规 master）。
3. **不改 6 个 peripheral 的实现**：本 change 只动测试与护栏。

## Impact

| 项 | 影响 |
|---|---|
| `tests/test_axi_spi.cpp` | 删 2 个死 helper（连带 5 处无界等待）；用例驱动 `rst_n` |
| `tests/test_axi_pwm.cpp` | 删 2 个死 helper；用例驱动 `rst_n` |
| `tests/test_axi_i2c.cpp` | 用例驱动 `rst_n`（若实例化了 device） |
| `tests/test_axi_rst_n_guard.cpp` | **新增**（1 个 ctest 条目） |
| `scripts/verify_rst_n_wiring.sh` | **新增**（CI 护栏） |
| `.github/workflows/ci.yml` | 新增 1 个 CI step |
| `openspec/specs/dual-backend-test-coverage/spec.md` | MODIFIED ×1（解锁那 3 个文件） |
| 根 `AGENTS.md` + `include/axi4/AGENTS.md` | NOTES + CONVENTIONS（接线护栏的存在与用法） |
| 风险 | ⚠️ **低**。① 只动测试与脚本，不动库代码；② 删的是**零调用**的函数（编译器不会报，但可静态核实）；③ 驱动 `rst_n` 会让 DUT 从「永久复位」变成「正常运行」→ **可能暴露既有的隐藏失败**（若某用例其实依赖了复位态行为）→ Task 3 逐个跑并记录 |
