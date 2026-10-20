# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

## ADDED Requirements

### Requirement: AXI4 master peripheral MUST 有独立于 slave 的复位契约

`AxiDma`（`include/axi4/peripherals/axi_dma.h`，AXI4 master）MUST 具备
`ch_in<ch_bool> rst_n` 端口，且所有 `ch_reg` MUST 有**显式列出**的复位值。
`AxiDma` 的复位契约 MUST NOT 复用 AXI4-Lite slave 的 Requirement
（master 复位的是自身 FSM 状态，而非从设备寄存器值）。

#### Scenario: `AxiDma` 含 rst_n 端口

- **WHEN** reviewer 阅读 `axi_dma.h` 的 `__io` section
- **THEN** MUST 含 `ch_in<ch_bool> rst_n;`

#### Scenario: 复位值表显式

- **WHEN** reviewer 阅读 `axi_dma.h` 的 `describe()`
- **THEN** 每个 `ch_reg` MUST 有显式的复位值（不允许 `init_val` 之类占位描述）
- **AND** `state` 的复位值 MUST 为 0（与 `is_idle` 判定一致）

#### Scenario: 复位逃生路径

- **WHEN** `rst_n` 拉低
- **THEN** `state` MUST 回到 idle（0）
- **AND** `data_vld` / `err_flag` / `irq_en` MUST 回到 false
- **AND** MUST 提供复位逃生路径（事务卡死时可被复位打断）

### Requirement: AXI4 peripheral 参考实现 MUST 与规范形态一致

`include/axi4/peripherals/axi_{spi,pwm,i2c}.h` MUST 与
`fix-axi4-peripheral-rst-n-port-2026-10` 确立的规范形态一致：

1. MUST 有 `b_valid` 寄存器；`bvalid` MUST 由其驱动；
2. `bvalid` MUST 在 **WDATA 被接受之后**才拉高，并保持到 BREADY
   （MUST NOT 是 `bvalid = aw_done` —— 那会早于 WDATA）；
3. `aw_done` MUST 仅在 `b_valid && bready` 时清零
   （MUST NOT 在裸 `bready` 上清零 —— master 常态拉高 BREADY 时 W 永不被接受）；
4. MUST 在 AW / AR 握手时锁存地址，写侧/读侧解码 MUST 基于锁存寄存器；
5. 每个文件 MUST 只有一处 `bvalid` 赋值。

#### Scenario: 消除 bvalid 双赋值

- **WHEN** 对 `axi_pwm.h` / `axi_i2c.h` 执行 `grep -c "bvalid = " <file>`
- **THEN** 每个文件 MUST 命中 1
- **AND** MUST NOT 出现后赋值覆盖前赋值
  （`axi_pwm.h` L116/L153、`axi_i2c.h` L149/L290 现状各 2 处）

#### Scenario: BVALID 响应保持且不早于 WDATA

- **WHEN** master 在 AW/W 握手后等待 BVALID
- **THEN** `bvalid` MUST 保持为 1 直到 BREADY
- **AND** MUST NOT 出现 `bvalid` 恒 0 的死锁状态
- **AND** `bvalid` MUST NOT 在 WDATA 被接受之前拉高

#### Scenario: aw_done 清零条件

- **WHEN** reviewer 检查 `aw_done->next` 的赋值
- **THEN** 清零条件 MUST 包含 `b_valid`（即 `b_valid && bready`）
- **AND** MUST NOT 直接依赖裸 `io().bready`

#### Scenario: 地址锁存

- **WHEN** 对 `axi_{spi,pwm,i2c}.h` 执行
  `grep -cE 'addr_bits = io\(\)\.aw?r?addr' <file>`
- **THEN** MUST 命中 0 行（解码 MUST 基于锁存寄存器）
- **AND** MUST 存在 `addr_reg` / `ar_addr_reg`

### Requirement: AXI4 peripheral MUST 遵守字节写与响应错误契约

`include/axi4/peripherals/*.h` MUST：

1. 在写路径消费 `wstrb`（字节掩码），MUST NOT 整字覆盖；
2. 驱动 `bresp`（MUST NOT 悬空）；
3. 对未解码地址返回 `DECERR`，MUST NOT 返回 OKAY。

#### Scenario: wstrb 被消费

- **WHEN** master 以部分字节掩码写寄存器
- **THEN** 未选中字节 MUST 保持原值
- **AND** 选中字节 MUST 被写入

#### Scenario: bresp 不悬空

- **WHEN** 对 `include/axi4/peripherals/*.h` 执行 `grep -c "io().bresp = " <file>`
- **THEN** 每个文件 MUST 命中 ≥ 1
- **AND** 现状（`axi_spi.h` 完全未驱动 BRESP）MUST NOT 保留

#### Scenario: 未映射地址返回 DECERR

- **WHEN** master 访问未解码的寄存器地址
- **THEN** `bresp` MUST 为 `DECERR`
- **AND** MUST NOT 为 OKAY（与 `include/axi4/AGENTS.md` 的 ANTI-PATTERN 一致）

### Requirement: AXI4 peripheral 写侧 / 读侧地址映射 MUST 一致

`include/axi4/peripherals/*.h` 的写侧地址解码与读侧地址解码 MUST 指向**同一组**寄存器，
且 MUST 与该 peripheral 头部注释中登记的寄存器映射一致。

#### Scenario: GPIO 写读映射对齐

- **WHEN** reviewer 对照 `axi_gpio.h` 的写侧解码（`sel_*`）与读侧解码（`read_sel_*`）
- **THEN** 两者 MUST 指向同一组寄存器
- **AND** MUST 与头部注释的映射一致：
  `0x00→DATA_OUT`、`0x04→DATA_IN`、`0x08→DIRECTION`、`0x0C→INTERRUPT`
- **AND** 写侧对只读寄存器（DATA_IN）的写入 MUST 被忽略
- **AND** 现状（写侧 `0x04→DIRECTION` / `0x08→INTERRUPT`，与读侧错位一档）MUST NOT 保留

#### Scenario: 测试与映射同步

- **WHEN** 写侧映射被修正
- **THEN** `tests/test_axi_gpio.cpp` MUST 同步更新（MUST NOT 保留只对旧写侧映射成立的断言）
