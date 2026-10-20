# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

## MODIFIED Requirements

### Requirement: AXI4 peripheral MUST 支持字节写

- **MODIFIED**: 适用范围 MUST 扩到**通用 AXI slave**（`AxiLiteSlave` / `Axi4Slave`）——
  两者的写路径 MUST 消费 `wstrb`，MUST NOT 整字覆盖寄存器；并**合并**
  `axi4-byte-lane-dual-parity-2026-10`（archive
  `2026-10-20-axi4-byte-lane-dual-parity-2026-10`）加入的 2 个双后端 Scenario
  —— openspec 的 `MODIFIED` 是**整条替换**，不显式合并会丢掉那两条 MUST 断言。

AXI4 slave 的写路径 MUST 消费 `wstrb` 字节掩码，MUST NOT 整字覆盖寄存器。
字节道语义 MUST 按 `include/axi4/AGENTS.md` CONVENTIONS 登记的规则执行。

适用对象（本次扩到通用 AXI slave）：

- `include/axi4/peripherals/*.h` 的 6 个 AXI4-Lite slave peripheral
  （`AxiLiteGpio` / `AxiLiteUart` / `AxiLiteTimer` / `AxiLitePwm` / `AxiLiteI2c` / `AxiLiteSpi`）
- `include/axi4/axi4_lite_slave.h` 的 `AxiLiteSlave`（通用 AXI4-Lite slave）
- `include/axi4/axi4_full.h` 的 `Axi4Slave`（通用 AXI4 Full slave）

#### Scenario: 部分字节写只改选中字节

- **WHEN** master 以非全 1 的 `wstrb` 写一个 32 位寄存器
- **THEN** `wstrb` 未选中的字节 MUST 保持原值
- **AND** 选中的字节 MUST 被写入 `wdata` 对应字节
- **AND** 写全 1 掩码时行为 MUST 等价于整字写（既有测试前提）

#### Scenario: 只读寄存器不被字节写改写

- **WHEN** master 写一个只读寄存器（如 `DATA_IN` / `RX_DATA`）
- **THEN** 该寄存器 MUST 保持原值
- **AND** MUST NOT 产生部分字节改写

#### Scenario: 通用 AXI4-Lite slave MUST 消费 wstrb

- **WHEN** master 以非全 1 的 `wstrb` 写 `AxiLiteSlave` 的任一寄存器
- **THEN** 未选中的字节 MUST 保持原值
- **AND** 写全 1 掩码时行为 MUST 等价于整字写

#### Scenario: 通用 AXI4 Full slave MUST 在每次 W 握手取当拍 wstrb

- **WHEN** master 对 `Axi4Slave` 发一次写事务，AW 拍与 W 拍的 `wstrb` 不同
- **THEN** 写路径 MUST 采用 **W 握手拍** 的 `wstrb`（MUST NOT 在 AW 拍锁存）
- **AND** 未被该 `wstrb` 选中的字节 MUST 保持原值
- **AND** 多 beat burst 的逐 beat 语义当前**未实现**（`Axi4Slave` 写状态机每笔事务
  只接受 1 个 W beat）—— MUST NOT 被本 Requirement 隐含为已实现，单独立项

#### Scenario: 字节道索引相对被访问的字

- **WHEN** master 写一个位于字内某字节道上的寄存器
- **THEN** 字节道索引 MUST 相对**被访问的字**解释，MUST NOT 相对寄存器重新编号
- **AND** 亚 32 位寄存器 MUST 位于所在字的首字节道（当前全部寄存器均如此）

#### Scenario: 掩码为空的写是空写

- **WHEN** master 以 `wstrb == 0` 写一个已解码地址
- **THEN** 目标寄存器 MUST 保持原值
- **AND** 写响应 MUST 仍为 `OKAY`（地址已解码）

#### Scenario: 通用 slave 与 peripheral 行为 MUST 一致

- **WHEN** 同一 `wstrb` 序列分别作用于 `AxiLiteSlave` 与任一 peripheral
- **THEN** 寄存器的字节级结果 MUST 一致（同一套字节道语义）
- **AND** 32 bit 路径的既有行为 MUST NOT 因掩码实现泛化而变化

#### Scenario: 字节道行为 MUST 双后端一致

- **WHEN** 同一字节道写场景分别在 InterpreterBackend 与 VerilatorBackend 下执行
  （peripheral 侧：`tests/test_axi_byte_lane.cpp`；通用 slave 侧：`tests/test_axi_slave_byte_lane.cpp`）
- **THEN** 写后读回的寄存器值 MUST 逐对象一致
- **AND** `bresp`（含只读地址的 `DECERR`）MUST 一致

#### Scenario: 亚 32 位寄存器的有效字节道 MUST 双后端一致

- **WHEN** 对 8 bit 寄存器（i2c `TX_DATA`）或 16 bit 寄存器（i2c `PRESCALE`、
  spi `BAUD`）执行部分字节写
- **THEN** 落在**有效字节道**（宽度 W 的寄存器为 `ceil(W/8)` 个低位道）内的写
  MUST 生效，其余字节 MUST 保持原值
- **AND** 落在无效道上的写 MUST 被忽略（值不变）
- **AND** interpreter 与 verilator 的结果 MUST 一致
  （截断到寄存器宽度 W 的行为在两条后端路径上 MUST 相同）
