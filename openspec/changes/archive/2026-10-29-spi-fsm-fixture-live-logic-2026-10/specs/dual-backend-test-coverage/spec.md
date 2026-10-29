# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约
（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

本 delta 由 `spi-fsm-fixture-live-logic-2026-10` 引入：
`tests/test_verilator_complex_e2e.cpp` 的 `SpiFsmFixture` 是一个**从未真正工作过**
的 DUT（其断言**只有 parity** ⇒ 两后端**同构** ⇒ 同构缺陷恒被掩盖 ⇒ 长期「假绿」），
且该文件**历史上已被改动两次却从未加过本 Requirement 的例外** ⇒ **主 spec 与实现
已 drift** ⇒ 本 delta 是**补登记 + 扩展**（不是新开口子）。

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

**例外（`axi4-full-slave-burst` 引入）**：
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

**例外（SPI fixture 语义修正引入）**：
`tests/test_verilator_complex_e2e.cpp` 的 `SpiFsmFixture` 是一个**从未真正工作过**的
DUT（实测：移位寄存器恒 `0x00`、FSM 状态卡死在 `TRANSFER`、`STOP` 永不可达、
`mosi` 恒 0），而它的断言**只有 parity**（两后端由**同一份** fixture 生成 ⇒
**同构** ⇒ 同构缺陷恒被 parity 掩盖 ⇒ 长期「假绿」）→ 该文件 MUST 被允许
**受限**变更：

1. `SpiFsmFixture::describe()` MUST 收敛到**正确形态** —— 条件逻辑**只用**硬件原语
   （`transition_when` / `select`；**禁** `if (硬件值)` 与三元 `? :`）；
   每个寄存器 MUST **只有 1 个** `next` 写入点（各状态候选值用 `select` 组合成
   单一表达式，写在 `sm.build()` **之后**）；每个输出 MUST **只驱动 1 次**；
   `transition_when` 的 target MUST 是**编译期常量**（⇒ 目标状态 MUST 用**多条**
   `transition_when` 表达，**禁**用三元选 target）；
2. 相应断言 MUST 从「仅 parity」升级为「parity **+** 语义金标」
   （FSM 状态序列 MUST **真的经过** `STOP`；`mosi` 的 8 bit 序列 MUST == 发送字节的
   **MSB-first** 序列；期望值 MUST **手推**，MUST NOT 抄仿真输出）；
3. TEST_CASE 名与 Catch2 SECTION 结构 MUST NOT 变化；
4. 该文件的**观测端口集合** MUST NOT 变化（⇒ Verilator 侧可观测面不变）。

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

- **WHEN** `axi4-full-slave-burst` 的 builder 阶段执行
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

#### Scenario: SPI fixture 允许受限变更（语义修正）

- **WHEN** 本 capability 的 builder 阶段修正 `SpiFsmFixture` 的 FSM 语义
- **THEN** `tests/test_verilator_complex_e2e.cpp` 的 `git diff` MUST 仅限
  "加 `LABELS verilator_e2e` 标签 + 顶注释声明 + `SpiFsmFixture::describe()`
  收敛到正确形态 + 相应断言升级为语义金标"
- **AND** MUST NOT 修改该文件的 TEST_CASE 名与 Catch2 SECTION 结构
- **AND** MUST NOT 增删该文件的观测端口
- **AND** ⚠️ **parity 断言 MUST 保留**（它是**必要**条件）⇒ 但 MUST NOT 被当作
  **充分**条件（同构缺陷恒被 parity 掩盖 ⇒ MUST 另有**绝对**语义金标）
