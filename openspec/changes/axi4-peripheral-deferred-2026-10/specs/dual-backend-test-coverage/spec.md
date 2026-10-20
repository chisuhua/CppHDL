# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

## ADDED Requirements

### Requirement: AXI4 peripheral MUST 支持字节写

`include/axi4/peripherals/*.h` 的写路径 MUST 消费 `wstrb` 字节掩码，
MUST NOT 整字覆盖寄存器。字节道语义 MUST 按
`include/axi4/AGENTS.md` CONVENTIONS 登记的规则执行。

#### Scenario: 部分字节写只改选中字节

- **WHEN** master 以非全 1 的 `wstrb` 写一个 32 位寄存器
- **THEN** `wstrb` 未选中的字节 MUST 保持原值
- **AND** 选中的字节 MUST 被写入 `wdata` 对应字节
- **AND** 写全 1 掩码时行为 MUST 等价于整字写（既有测试前提）

#### Scenario: 只读寄存器不被字节写改写

- **WHEN** master 写一个只读寄存器（如 `DATA_IN` / `RX_DATA`）
- **THEN** 该寄存器 MUST 保持原值
- **AND** MUST NOT 产生部分字节改写

### Requirement: UART peripheral MUST 提供串行收发引擎

`include/axi4/peripherals/axi_uart.h` MUST 提供 TX 移位引擎与 RX 接收引擎，
使 `uart_tx` / `uart_rx` / `rx_data_reg` / `rx_valid` / `irq` 具备真实串行语义。

#### Scenario: TX 输出可还原的串行帧

- **WHEN** master 写 `TX_DATA`
- **THEN** `uart_tx` MUST 输出含起始位 + 8 个数据位 + 停止位的帧
- **AND** 采样 `uart_tx` MUST 能还原出写入的字节
- **AND** `tx_empty` MUST 反映移位寄存器是否为空

#### Scenario: RX 接收并置位 rx_valid

- **WHEN** 外部在 `uart_rx` 上发送一个合法帧
- **THEN** `rx_data_reg` MUST 收到该字节
- **AND** `rx_valid` MUST 置 1，`status_reg` bit1 MUST 反映 `rx_valid`
- **AND** `irq` MUST 由 `rx_valid` 驱动
- **AND** master 读走 `RX_DATA` 后 `rx_valid` MUST 清零
