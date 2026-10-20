# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

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

#### Scenario: file 命名约束

- **WHEN** 新增 AXI4 peripheral behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_axi_<peripheral>.cpp`
- **AND** 原 `test_axi_<peripheral>.cpp` MUST NOT 修改

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
- **THEN** MUST NOT 改 `tests/test_verilator_*.cpp` 测试体（spec 锁定 macro reference impl）
- **AND** MUST NOT 改 `tests/test_axi_lite_verilator_e2e.cpp`（spec 锁定 reference impl）
- **AND** MUST NOT 改 `tests/test_axi_{spi,pwm,i2c}_behavior.cpp`（已 archived）
