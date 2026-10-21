# Capability: dual-backend-test-coverage

## MODIFIED Requirements

### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

`tests/test_axi_<peripheral>_behavior.cpp` MUST 用 `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro
（`_behavior` suffix 区别原 `test_axi_<peripheral>.cpp` 接口结构测试）。

该 Requirement 的 peripheral 列表 MUST 与实际存在 `_behavior` 测试的 AXI4-Lite slave
一一对应：`AxiLiteSpi` / `AxiLitePwm` / `AxiLiteI2c` / `AxiLiteTimer` / `AxiLiteGpio` /
`AxiLiteUart`。

`AxiDma`（AXI4 **master**，非 AXI4-Lite slave，复位契约不同）不在本 Requirement 范围；
其 dual 化尚未排期（`axi4-peripheral-followups-2026-10` 已 archive，未包含该测试）。

原 `tests/test_axi_<peripheral>.cpp` MUST NOT 修改（interpreter-only baseline）。

**例外（`legacy-axi-peripheral-test-cleanup-2026-10` 引入）**：
`tests/test_axi_{spi,pwm,i2c}.cpp` 三个遗留文件 MUST 被允许**受限**变更：

1. 删除**零调用**的 helper（`axi_lite_write` / `axi_lite_read`）——
   它们含无界握手等待（`while (ready == 0) tick();`），一旦被调用即 hang 到
   ctest TIMEOUT（而非断言失败）；
2. 所有**仿真**了 device（构造了 `Simulator`）的用例 MUST 驱动 `rst_n = 1`
   （未接线的 `ch_in<ch_bool>` 读出为 0 = **永久复位** = 零编译错误的静默破坏）；
   ⚠️ 只建 device 但**不建 Simulator** 的纯 codegen 用例不适用（端口根本没被求值）；
3. MUST NOT 削弱 / 删除 / 改名任何既有 TEST_CASE 或断言。

除上述三类变更外，这三个文件的 `git diff` MUST 仍为空。

#### Scenario: file 命名约束

- **WHEN** 新增 AXI4 peripheral behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_axi_<peripheral>.cpp`
- **AND** 原 `test_axi_<peripheral>.cpp` MUST NOT 修改
  （**例外**：`test_axi_{spi,pwm,i2c}.cpp` 的受限清理，见上方例外条款）

#### Scenario: peripheral 列表与 `_behavior` 文件一一对应

- **WHEN** 枚举 `tests/test_axi_*_behavior.cpp` 的覆盖对象
- **THEN** 结果 MUST 与上述列表完全一致（6 个 AXI4-Lite slave）
- **AND** 列表 MUST NOT 含无 `_behavior` 测试的类（如 `AxiDma`）

#### Scenario: macro 接口契约

- **WHEN** 测试访问 `ch_out<ch_uint<N>>` 等端口值
- **THEN** MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`
- **AND** MUST NOT 用 `port.impl()` 跳过 ch_out 接口（违反 macro R3）

#### Scenario: 依赖前置 design 自洽

- **WHEN** 本 change 启动
- **THEN** 前置 `fix-axi4-peripheral-rst-n-port-2026-10` MUST 已 archive
- **AND** `AxiLiteTimer` / `AxiLiteGpio` / `AxiLiteUart` design MUST 满足规范形态
  （`rst_n` + AW→W 相位分离 + 响应保持）

#### Scenario: 不破既有 spec 锁

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `tests/test_verilator_*.cpp` 测试体（spec 锁定 macro reference impl），
  **除了** `tests/test_verilator_trace_parity.cpp` 的「内嵌 `AxiLiteSlave` 的 fixture
  MUST 暴露并透传 `rst_n` + testbench MUST 驱动 `rst_n` + 写事务 MUST 改两拍
  + 相应断言 MUST 按新规范更新」这一受限例外
- **AND** MUST NOT 改 `tests/test_axi_lite_verilator_e2e.cpp`，
  **除了**同上的受限例外
- **AND** MUST NOT 改 `tests/test_axi_{spi,pwm,i2c}_behavior.cpp`（已 archived）
- **AND**（`axi4-full-slave-burst` 追加的例外）
  MUST 允许改 `tests/test_verilator_memory_e2e.cpp`：
  接入 `Axi4Slave` 新增的 `rst_n` + 4-beat 写补数据断言 + 相应断言更新
- **AND**（`legacy-axi-peripheral-test-cleanup-2026-10` 追加的例外）
  MUST 允许改 `tests/test_axi_{spi,pwm,i2c}.cpp`：
  删除零调用死 helper + 驱动 `rst_n`（且 MUST NOT 削弱既有断言）

#### Scenario: 未接线 rst_n MUST 有自动化护栏

- **WHEN** 新增带**必需** `rst_n` 端口的组件，或新增其实例化点
- **THEN** `scripts/verify_rst_n_wiring.sh` MUST 在 CI 中被调用并通过
  （该脚本按 **per test case** 粒度检查：仿真了这类组件的用例 MUST 驱动 `rst_n`；
  wrapper MUST 透传 `rst_n <<=`；只认**真实驱动**，注释里提到 `rst_n` 不算数）
- **AND** MUST 至少存在一个**双后端** Catch2 用例，断言
  「不驱动 `rst_n` → 无 B/R 响应 + 寄存器保持初值」
  （把「未接线 = 永久复位」钉成可执行契约，而不只是文档里的一句话）
