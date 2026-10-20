# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

## MODIFIED Requirements

### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

- **MODIFIED**: 补全 peripheral 列表，并修正错误类名。列表 MUST 只包含**实际存在
  `_behavior` 测试**的 peripheral：`AxiLiteI2c` / `AxiLitePwm` / `AxiLiteUart` /
  `AxiLiteTimer` / `AxiLiteGpio`。
  - `AxiLiteDma` MUST 移除 —— 该类**从不存在**（真实类名是 `AxiDma`，且它是 AXI4 **master**
    而非 AXI4-Lite slave）；`AxiDma` 尚无 `_behavior` 测试，其 dual 化归
    `axi4-peripheral-followups-2026-10`
  - 补全 `AxiLiteUart` / `AxiLiteTimer` / `AxiLiteGpio`（本 change 新增 3 个 `_behavior` 测试）
  - 现状主 spec 无 peripheral 列表 —— 上一个已归档 change `2026-10-20-dual-backend-axi4-peripherals`
    用了 `--skip-specs`，其 delta 被静默丢弃；本 MODIFIED 显式修正

#### Scenario: file 命名约束

- **WHEN** 新增 AXI4 peripheral behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_axi_*.cpp` 接口结构测试
- **AND** 原 `test_axi_*.cpp` MUST NOT 修改

#### Scenario: macro 接口契约

- **WHEN** 测试访问 `ch_out<ch_uint<N>>` 等端口值
- **THEN** MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`
- **AND** MUST NOT 用 `port.impl()` 跳过 ch_out 接口（违反 macro R3）

#### Scenario: 依赖前置 design 自洽

- **WHEN** 本 change 启动
- **THEN** 前置 `fix-axi4-peripheral-rst-n-port-2026-10` MUST 已 archive
- **AND** 3 个 peripheral design MUST 满足规范形态（`rst_n` + AW→W 相位分离 + 响应保持）

#### Scenario: 不破既有 spec 锁

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `tests/test_verilator_*.cpp` 测试体（spec 锁定 macro reference impl）
- **AND** MUST NOT 改 `tests/test_axi_lite_verilator_e2e.cpp`（spec 锁定 reference impl）
- **AND** MUST NOT 改 `tests/test_axi_{spi,pwm,i2c}_behavior.cpp`（已 archived）
