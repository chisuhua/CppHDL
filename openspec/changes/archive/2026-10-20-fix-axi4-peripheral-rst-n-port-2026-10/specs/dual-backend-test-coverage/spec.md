# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

## MODIFIED Requirements

### Requirement: AXI4-Lite slave peripheral MUST 收敛到相位分离 + 响应保持的规范形态

`AxiLiteUart` / `AxiLiteTimer` / `AxiLiteGpio` 三个 AXI4-Lite slave MUST 同时满足：

1. 具备 `ch_in<ch_bool> rst_n` 端口（低有效异步复位）；
2. 所有 `ch_reg` MUST 用 `select(!io().rst_n, <init_val>, ...)` 复位 pattern，
   且 `<init_val>` MUST 显式带寄存器位宽（如 `ch_uint<32>(0_d)`）；
3. 具备 `busy` / `aw_done` / `b_valid` / `r_valid` 状态寄存器组，实现 AW→W 相位分离；
4. `bvalid` MUST 在 WDATA 被接受之后拉高并保持到 BREADY；`rvalid` MUST 保持到 RREADY；
5. MUST NOT 使用 `&&` / `||` 逻辑 overload（ADR-036）；
6. 每个 `ch_reg` 在同一文件内 MUST 只有**一处** `X->next =` 赋值；
7. **MUST 在 AW / AR 握手时锁存地址**（见下方 Scenario）。

`AxiDma`（AXI4 master）不在本 Requirement 范围内。

#### Scenario: 三个 peripheral 含 rst_n 端口

- **WHEN** reviewer 阅读 `axi_uart.h` / `axi_timer.h` / `axi_gpio.h` 的 `__io` section
- **THEN** 每个 MUST 含 `ch_in<ch_bool> rst_n;`

#### Scenario: 状态寄存器组与相位分离

- **WHEN** reviewer 阅读三个 peripheral 的 `describe()`
- **THEN** MUST 存在 `aw_done` / `b_valid` / `r_valid` / `busy` 四个 `ch_reg<ch_bool>`
- **AND** `w_handshake` MUST 由 `io().wvalid` 与 `aw_done` 组合得出
  （W MUST 可在 AW 之后的任意周期被接受）
- **AND** `aw_done` MUST 仅在 `b_valid && bready` 时清零
  （MUST NOT 在 `bready` 上直接清零 —— master 常态拉高 BREADY 时会导致 W 永远不被接受）

#### Scenario: 地址锁存

- **WHEN** master 在 AW 握手完成后改写 `AWADDR`（AXI 只要求 AWADDR 保持到 AW 握手）
- **THEN** peripheral MUST 仍把 WDATA 写入 **AW 握手时锁存的地址**指向的寄存器
- **AND** 写侧地址解码 MUST 基于锁存寄存器，MUST NOT 基于活的 `io().awaddr`
- **AND** 读侧同理：`ARADDR` MUST 在 AR 握手时锁存，读数据 MUST 基于锁存地址
- **AND** MUST 存在回归测试覆盖"AW 握手后改写 awaddr，写目标不变"

#### Scenario: 响应保持语义

- **WHEN** master 在 AW/W 握手后撤掉 AWVALID/WVALID，改为等待 BVALID
- **THEN** `bvalid` MUST 保持为 1 直到 master 拉高 BREADY
- **AND** `rvalid` MUST 保持为 1 直到 master 拉高 RREADY
- **AND** MUST NOT 出现 `bvalid` 恒 0 且 `awready` 恒 0 的死锁状态

#### Scenario: 复位语义

- **WHEN** `rst_n` 拉低
- **THEN** 所有 `ch_reg` MUST 在下一周期回到各自 init 值
- **AND** `busy` / `aw_done` / `b_valid` / `r_valid` MUST 回到 0
- **AND** MUST 提供复位逃生路径（事务卡死时可被复位打断）

#### Scenario: 禁止逻辑 overload

- **WHEN** 对 `include/axi4/peripherals/axi_{uart,timer,gpio}.h` 执行
  `grep -nE 'io\(\)\.[a-z_]+ *(&&|\|\|)'`
- **THEN** MUST 命中 0 行
- **AND** 编译 MUST NOT 产生 `-Wdeprecated-declarations`

#### Scenario: 单处 next 赋值

- **WHEN** reviewer 对每个 `ch_reg` 统计 `X->next =` 出现次数
- **THEN** MUST 恰好为 1（禁止后赋值覆盖前赋值 —— 现状 `axi_uart.h` 的 `status_reg` 有 2 处）

#### Scenario: dual backend parity 解锁前置

- **WHEN** 本 change 落地后
- **THEN** `AxiLiteUart` / `AxiLiteTimer` / `AxiLiteGpio` MUST 具备可运行的
  interpreter 侧回归测试（`tests/test_axi_{uart,timer,gpio}.cpp`）
- **AND** `dual-backend-axi4-peripherals-extended-2026-10` 的 `_behavior` 测试
  MUST NOT 出现因 busy 锁死或 BVALID 不保持导致的 fixture 卡死
