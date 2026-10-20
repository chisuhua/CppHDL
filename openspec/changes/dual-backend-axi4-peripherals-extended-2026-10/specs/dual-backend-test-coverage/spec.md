# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

## ADDED Requirements

### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR（4 peripheral 扩展）

`tests/test_axi_<peripheral>_behavior.cpp`（_behavior suffix 区别原 `test_axi_<peripheral>.cpp` 结构测） MUST 用 `DUAL_BACKEND_TEST_PAIR` macro 覆盖 peripherals: `AxiLiteTimer` / `AxiLiteGpio` / `AxiLiteDma` / `AxiLiteUart`（设计自洽的扩展集）。**原** `tests/test_axi_<peripheral>.cpp` **MUST NOT 修改**（保留 interpreter-only baseline per spec R7）。

#### Scenario: file 命名 + space 互斥

- **WHEN** 新增 AXI4 peripheral behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_axi_<peripheral>.cpp`
- **AND** 原 `test_axi_<peripheral>.cpp` MUST NOT 修改

#### Scenario: macro 接口约束

- **WHEN** 测试访问 `ch_out<ch_uint<32>>` 等端口值
- **THEN** MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`
- **AND** MUST NOT 用 `port.impl()` 跳过 ch_out 接口（违反 macro R3）

#### Scenario: 依赖前置 design 自洽

- **WHEN** 本 change 启动
- **THEN** 前置 `fix-axi4-peripheral-rst-n-port-2026-10` MUST 已 archive
- **AND** 4 个 peripheral design MUST 含 rst_n port + busy self-clear

#### Scenario: 不破既有 spec 锁

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `tests/test_verilator_*.cpp` 测试体（spec 锁定 macro reference impl）
- **AND** MUST NOT 改 `tests/test_axi_lite_verilator_e2e.cpp`（spec 锁定 reference impl）
- **AND** MUST NOT 改 `tests/test_axi_{spi,pwm,i2c}_behavior.cpp`（已 archived）