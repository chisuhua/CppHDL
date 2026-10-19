# Capability: dual-backend-test-coverage

## ADDED Requirements

### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

`tests/test_axi_*_behavior.cpp`（MUST 用 `_behavior` suffix 区别原 `test_axi_*.cpp` 接口结构测试） MUST 用 `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro。

#### Scenario: file 命名约束

- **WHEN** 新增 AXI4 peripheral behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix（如 `test_axi_spi_behavior.cpp`）区别原 `test_axi_*.cpp` 接口结构测试；不允许覆盖现有 `test_axi_*.cpp` 行为契约

#### Scenario: macro 接口契约

- **WHEN** 测试访问 `ch_out<ch_uint<N>>` 等端口值
- **THEN** MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`；MUST NOT 用 `port.impl()` 跳过 ch_out 接口（违反 macro R3 契约）

#### Scenario: 原 test_axi_*.cpp 不动

- **WHEN** 本 change 涉及 `tests/test_axi_spi.cpp` 或其他原 test_axi_*.cpp
- **THEN** MUST NOT 修改原文件 —— 原文件是接口结构测试（interpreter-only），保留为 fixture 一致性 baseline