# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约
（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

本 delta 由 `axi4-lite-generic-slave-conformance-2026-10` 引入：
`AxiLiteSlave` 新增**必需**端口 `rst_n` 后，两个内嵌该 slave 的 fixture
MUST 被允许做**受限**变更，否则「不改 → slave 永久复位 → 测试必红」
与「改了 → 违反本 capability 的 spec 锁」构成无解冲突。

## MODIFIED Requirements

### Requirement: 现有 9 个 verilator 测试 MUST 保留原状

`tests/test_verilator_trace_parity.cpp` / `tests/test_verilator_bundle_e2e.cpp` / `tests/test_verilator_stream_e2e.cpp` / `tests/test_verilator_memory_e2e.cpp` / `tests/test_verilator_complex_e2e.cpp` / `tests/test_verilator_e2e_harness.cpp` / `tests/test_axi_lite_verilator_e2e.cpp` / `tests/test_verilator_backend.cpp` / `tests/test_verilator_error_flow.cpp` 9 个测试 MUST 保留原状（例外见下方），不迁移到 `DUAL_BACKEND_TEST` macro。理由：
1. 它们是 macro 的 **reference implementation**，将 macro 与现有 hand-crafted 代码漂移会破坏 pattern 传承
2. `verilator-e2e-coverage` / `verilator-backend-e2e` / `verilator-rv32i-e2e` 等既有 spec 已锁定其 file-level / count-level requirements
3. 这些测试已配置 `CATCH_AMALGAMATED_CUSTOM_MAIN` 与个性化 `main()`（verilator 缺失时 return 0），迁移会导致既有 TIMEOUT 与 exit-code 契约二次破坏

**例外（`axi4-lite-generic-slave-conformance-2026-10` 引入）**：
`tests/test_verilator_trace_parity.cpp` 与 `tests/test_axi_lite_verilator_e2e.cpp`
的 fixture 内嵌 `AxiLiteSlave`，而 `rst_n` 是该 slave 的**必需端口**
（未接线的 `ch_in<ch_bool>` 读出为 0 = 永久复位）→ 这两个文件 MUST 被允许
**受限**变更：

1. fixture 暴露 `rst_n` 端口并透传给 slave；
2. testbench 侧 `sim.set_input_value(io().rst_n, 1)`；
3. 写事务由「AW+W 同拍」改为「AW 一拍 + W 另一拍」（AXI4-Lite slave 的 AW→W 相位分离）；
4. 相应断言按新规范更新（`bvalid` / `rvalid` 改为保持到 `bready` / `rready`）。

除上述四类变更外，这两个文件的 `git diff` MUST 仍仅限"加 `LABELS verilator_e2e` 标签
+ 在文件顶注释声明 `Reference implementation for DUAL_BACKEND_TEST macro`"两类变更。

#### Scenario: 9 个测试保留原状

- **WHEN** 本 capability 的 builder 阶段执行
- **THEN** **除本 Requirement 明确列出的例外外**，上述 9 个文件的 `git diff` MUST 仅限"加 `LABELS verilator_e2e` 标签 + 在文件顶注释声明 `Reference implementation for DUAL_BACKEND_TEST macro`"两类变更
- **AND** 测试体、TEST_CASE 名、Catch2 SECTION、add_catch_test / set_tests_properties 调用 MUST NOT 变化（例外文件适用上方例外）

#### Scenario: 内嵌通用 slave 的两个 fixture 允许受限变更

- **WHEN** `axi4-lite-generic-slave-conformance-2026-10` 的 builder 阶段执行
- **THEN** `tests/test_verilator_trace_parity.cpp` 与 `tests/test_axi_lite_verilator_e2e.cpp`
  的 `git diff` MUST 仅限"加 `LABELS verilator_e2e` 标签 + 顶注释声明 +
  fixture 暴露并透传 `rst_n` + testbench 驱动 `rst_n` + 写事务改两拍 + 相应断言更新"
- **AND** 其余 7 个文件的 `git diff` MUST 仍仅限"加 `LABELS verilator_e2e` 标签 + 顶注释声明"
- **AND** MUST NOT 修改这 9 个文件中的 TEST_CASE 名与 Catch2 SECTION 结构

#### Scenario: 全 9 个测试贴上 verilator_e2e label

- **WHEN** `ctest -N -L verilator_e2e` 执行
- **THEN** 输出 MUST 包含这 9 个测试 + 所有用 `add_dual_backend_test` 注册的新测试
- **AND** 总数 MUST ≥ 12（9 旧 + ≥ 3 新 Phase 1 target）
- **NOTE**: 本 capability 落地后，label `verilator_e2e` 是统一对外标签，取代现有各种临时 label

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
- **THEN** MUST NOT 改 `tests/test_verilator_*.cpp` 测试体（spec 锁定 macro reference impl），
  **除了** `tests/test_verilator_trace_parity.cpp` 的「内嵌 `AxiLiteSlave` 的 fixture
  MUST 暴露并透传 `rst_n` + testbench MUST 驱动 `rst_n` + 写事务 MUST 改两拍
  + 相应断言 MUST 按新规范更新」这一受限例外
- **AND** MUST NOT 改 `tests/test_axi_lite_verilator_e2e.cpp`，
  **除了**同上的受限例外
- **AND** MUST NOT 改 `tests/test_axi_{spi,pwm,i2c}_behavior.cpp`（已 archived）

### Requirement: AXI4-Lite slave peripheral MUST 收敛到相位分离 + 响应保持的规范形态

`AxiLiteUart` / `AxiLiteTimer` / `AxiLiteGpio` 三个 AXI4-Lite slave MUST 同时满足：

1. 具备 `ch_in<ch_bool> rst_n` 端口（低有效，在时钟沿生效）；
2. 所有 `ch_reg` MUST 用 `select(!io().rst_n, <init_val>, ...)` 复位 pattern，
   且 `<init_val>` MUST 显式带寄存器位宽（如 `ch_uint<32>(0_d)`）；
3. 具备 `busy` / `aw_done` / `b_valid` / `r_valid` 状态寄存器组，实现 AW→W 相位分离；
4. `bvalid` MUST 在 WDATA 被接受之后拉高并保持到 BREADY；`rvalid` MUST 保持到 RREADY；
5. MUST NOT 使用 `&&` / `||` 逻辑 overload（ADR-036）；
6. 每个 `ch_reg` 在同一文件内 MUST 只有**一处** `X->next =` 赋值；
7. **MUST 在 AW / AR 握手时锁存地址**（见下方 Scenario）。

`AxiDma`（AXI4 master）不在本 Requirement 范围内。

> **NOTE（`axi4-lite-generic-slave-conformance-2026-10` 引入）**：
> 第 1 条的复位措辞由「低有效**异步**复位」修正为「低有效，**在时钟沿生效**」——
> `select(!io().rst_n, <init_val>, ...)` 落在 `ch_reg` 的 `next` 表达式上，
> 由时钟沿采样，实为沿上生效（`include/core/node_builder.h:180-182` 的 `ch_reg`
> 原生复位接的是 context 的 `default_reset`，组件级 `rst_n` 只能走 `next` 表达式）。
> **同源规则的通用 slave 延伸见 `axi-slave-conformance` capability。**

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
