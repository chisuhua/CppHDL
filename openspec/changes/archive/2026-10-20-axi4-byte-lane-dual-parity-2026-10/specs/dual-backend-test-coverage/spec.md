# Capability: dual-backend-test-coverage

## MODIFIED Requirements

### Requirement: AXI4 peripheral MUST 支持字节写

- **MODIFIED**: 在既有字节道契约上补两条断言 —— (1) 字节道行为 MUST 双后端一致
  （interpreter / verilator）；(2) 亚 32 位寄存器的**有效字节道截断** MUST 双后端一致。
  动机见 change `axi4-byte-lane-dual-parity-2026-10`：字节道掩码的 verilator 半边
  此前从未被跑到（既有 dual 测试的 `wstrb` 恒为 `0xF`），而窄寄存器截断正是
  Verilog codegen 宽度推导与 interpreter 差异最大的地方。

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

#### Scenario: 字节道行为 MUST 双后端一致

- **WHEN** 同一字节道写场景分别在 InterpreterBackend 与 VerilatorBackend 下执行
  （`tests/test_axi_byte_lane.cpp`，6 个 peripheral 各一对）
- **THEN** 写后读回的寄存器值 MUST 逐 peripheral 一致
- **AND** `bresp`（含只读地址的 `DECERR`）MUST 一致

#### Scenario: 亚 32 位寄存器的有效字节道 MUST 双后端一致

- **WHEN** 对 8 bit 寄存器（i2c `TX_DATA`）或 16 bit 寄存器（i2c `PRESCALE`、
  spi `BAUD`）执行部分字节写
- **THEN** 落在**有效字节道**（宽度 W 的寄存器为 `ceil(W/8)` 个低位道）内的写
  MUST 生效，其余字节 MUST 保持原值
- **AND** 落在无效道上的写 MUST 被忽略（值不变）
- **AND** interpreter 与 verilator 的结果 MUST 一致
  （截断到寄存器宽度 W 的行为在两条后端路径上 MUST 相同）
