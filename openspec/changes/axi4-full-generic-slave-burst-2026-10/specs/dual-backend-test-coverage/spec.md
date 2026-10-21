# Capability: dual-backend-test-coverage

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

**例外（`axi4-full-generic-slave-burst-2026-10` 引入）**：
`tests/test_verilator_memory_e2e.cpp` 内嵌 `Axi4Slave`（`Axi4Slave<32,32,4,16,16>`），
而 `rst_n` 是该 slave 的**必需端口**（未接线的 `ch_in<ch_bool>` 读出为 0 = 永久复位）
→ 该文件 MUST 被允许**受限**变更：

1. fixture 暴露 `rst_n` 端口并透传给 slave；
2. testbench 侧 `sim.set_input_value(io().rst_n, 1)`；
3. 该文件既有的 4-beat 写用例 MUST 补**数据断言**
   （当前只比较握手信号 → 多 beat burst 的后续 beat 被静默丢弃时测试仍绿）；
4. 相应断言按新规范更新（`bresp` / `rresp` 语义化、`bvalid` 保持到 `bready`）。

除上述四类变更外，该文件的 `git diff` MUST 仍仅限"加 `LABELS verilator_e2e` 标签
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
- **AND** 除本 Requirement 明确列出的例外文件外，其余文件的 `git diff` MUST 仍仅限"加 `LABELS verilator_e2e` 标签 + 顶注释声明"
- **AND** MUST NOT 修改这 9 个文件中的 TEST_CASE 名与 Catch2 SECTION 结构

#### Scenario: 内嵌 Axi4Slave 的 fixture 允许受限变更（burst change）

- **WHEN** `axi4-full-generic-slave-burst-2026-10` 的 builder 阶段执行
- **THEN** `tests/test_verilator_memory_e2e.cpp` 的 `git diff` MUST 仅限
  "加 `LABELS verilator_e2e` 标签 + 顶注释声明 + fixture 暴露并透传 `rst_n`
  + testbench 驱动 `rst_n` + 4-beat 写补数据断言 + 相应断言更新"
- **AND** 其余 8 个文件的 `git diff` MUST 仍仅限"加 `LABELS verilator_e2e` 标签 + 顶注释声明"
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
- **AND**（`axi4-full-generic-slave-burst-2026-10` 追加的例外）
  MUST 允许改 `tests/test_verilator_memory_e2e.cpp`：
  接入 `Axi4Slave` 新增的 `rst_n` + 4-beat 写补数据断言 + 相应断言更新

### Requirement: AXI4 peripheral MUST 支持字节写

适用范围 MUST 覆盖**通用 AXI slave**（`AxiLiteSlave` / `Axi4Slave`）——
两者的写路径 MUST 消费 `wstrb`，MUST NOT 整字覆盖寄存器；
本条同时覆盖 `axi4-byte-lane-dual-parity-2026-10` 加入的 2 个双后端 Scenario
（openspec 的 `MODIFIED` 是**整条替换**，不显式合并会丢掉那两条 MUST 断言）。

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
- **AND** 多 beat burst 的逐 beat 语义由独立 capability `axi4-full-slave-burst`
  承载（`openspec/specs/axi4-full-slave-burst/spec.md`）——
  本 Requirement 只约束「每次 W 握手取当拍 `wstrb`」这一条，
  MUST NOT 被本 Requirement 隐含为 burst 语义已实现

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
