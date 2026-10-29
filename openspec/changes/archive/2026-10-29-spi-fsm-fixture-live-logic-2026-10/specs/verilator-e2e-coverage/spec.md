# Capability: verilator-e2e-coverage

## Purpose

定义 CppHDL 的 Verilator 端到端覆盖契约（详见主 spec
`openspec/specs/verilator-e2e-coverage/spec.md`）。

本 delta 由 `spi-fsm-fixture-live-logic-2026-10` 引入：
`tests/test_verilator_complex_e2e.cpp` 的 SPI 场景此前**只**断言 parity
⇒ 两后端**同构** ⇒ 同构缺陷恒被掩盖 ⇒ 该场景 MUST 同时断言**绝对**语义金标。

## MODIFIED Requirements

### Requirement: 代表性复杂设计在 Verilator 仿真下行为契约

`tests/test_verilator_complex_e2e.cpp` **MUST** 覆盖至少 2 个复杂设计场景：RV32I 5-stage 流水线（短指令序列 trace parity）和 SPI 控制器 FSM（4-state `ch_state_machine` 状态转移 trace parity）。每个场景 **MUST** 同时跑 `InterpreterBackend` 与 `VerilatorBackend`，对比 `data_map_` 终值或状态转移序列。CMake 注册 TIMEOUT **600s**（`add_catch_test` 不支持 TIMEOUT 参数，必须 `set_tests_properties(... TIMEOUT 600)` 单独设置；verilator 编译 RV32I 流水线较慢，120s 无安全余量）。

**RV32I 简化度选择**：fixture 选 **3 条无 hazard 指令 `addi x1, x0, 1; addi x2, x0, 2; add x3, x1, x2`** 是因为它压测了 EX→EX 与 MEM→WB forwarding 路径（`add x3, x1, x2` 读 x1/x2 分别在 2 条/1 条前指令中写），**不覆盖** load-use hazard / branch flush——后者由现有 `test_rv32i_pipeline` / `test_hazard` 在 interpreter 层覆盖，本测试目标是 backend parity 而非流水线正确性。fixture 观测点用已暴露端口 **`perf_instructions`**（`rv32i_pipeline.h:54`），终值 == 3；不观测 `regs[3]`（深内 reg_file.verilator 不可观测，见 design D1 不变量 4）。

**SPI fixture 声明**：fixture 用 **`ch_state_machine` 新建 4-state FSM**（IDLE→START→TRANSFER→STOP，与 `examples/spinalhdl-ported/spi/spi_controller.cpp` 同构但**非简化**——spi_controller.cpp 用手写 `ch_reg<ch_uint<2>>` + if/else 而非 `ch_state_machine` DSL），shift reg + clock div，发 1 byte **0x96**。

⚠️ **发送字节 MUST 使 MSB-first ≠ LSB-first**：`0xA5`（`10100101`）是**回文**
⇒ 其 MSB-first 与 LSB-first 序列**完全相同**（都是 `1,0,1,0,0,1,0,1`）
⇒ ⇒ 「位序」这一维度**零判别力** ⇒ 「`mosi` 序列 == 字节的 MSB-first 序列」这条
断言**无法**发现位序错 ⇒ ⇒ MUST 改用**非回文**字节（如 `0x96` = `10010110`：
MSB-first = `1,0,0,1,0,1,1,0`，LSB-first = `0,1,1,0,1,0,0,1`）。

**⚠️ parity 断言 MUST NOT 被当作正确性证据（SPI 场景）**：两个后端由**同一份**
fixture 生成 ⇒ 缺陷**同构** ⇒ 两边**同时错** ⇒ parity **恒绿** ⇒
⇒ SPI 场景 **MUST** 在 parity 之外**另**断言**绝对**语义金标（见下方 Scenario）。

#### Scenario: RV32I Pipeline 3 条指令 trace parity

- **WHEN** fixture 为 `Rv32iPipelineFixture`（简化自 `include/cpu/pipeline/rv32i_pipeline.h`，`ch_mem` imem 装 3 条指令 `addi x1,x0,1; addi x2,x0,2; add x3,x1,x2` + 简化 dmem stub）
- **AND** interpreter + verilator 各跑至 3 条指令 WB 完成（约 20 cycle）
- **THEN** `data_map_.at(io().perf_instructions.impl()->id())` 在两后端必须 == 3
- **NOTE**: 不观测 `regs[3]`（verilator 不可观测内部 reg_file，见 design D1 不变量 4）

#### Scenario: SPI FSM 状态转移 trace parity

- **WHEN** fixture 为 `SpiFsmFixture`（基于 `ch_state_machine` 新建 4-state FSM IDLE→START→TRANSFER→STOP，与 `examples/spinalhdl-ported/spi/spi_controller.cpp` 同构但**非简化**——源文件用 `ch_reg<ch_uint<2>>` 手写而非 DSL；shift reg + clock div），发 1 byte **0x96**
- **AND** interpreter + verilator 各跑 32 cycle，完成 **2 次** 1 byte transfer
  （`start` 在 cycle **0** 与 cycle **12** 各拉高一次）
  ⇒ ⚠️ **2 次传输是必需的**：单次传输时「装载时复位计数器」**不可观测**
  （计数器在装载前本来就是 0 ⇒ 复位与否**无差别**）⇒ 该缺陷类**零判别力**
- **THEN** 32 cycle 内 `data_map_` 中 FSM 状态序列在两后端必须一致（观测点：绑独立 `ch_out dbg_state`，不用内部 reg）
- **AND** `mosi` 移出 8 bit 序列在两后端必须 == **0x96** 的 MSB-first 序列
- **AND** ⚠️ **语义金标（不只是 parity）**：两后端**各自** MUST 满足**手推**的
  逐拍序列金标 ——
  - FSM 状态序列 MUST 等于
    `IDLE, START, TRANSFER×8, STOP, IDLE, IDLE, START, TRANSFER×8, STOP, IDLE…`
    ⇒ ⇒ 状态序列 MUST **每次传输都真的经过 `STOP`**（⇒ 目标状态 MUST **可达**）；
  - `mosi` 在**每个** `TRANSFER` 拍上的取值 MUST == `0x96` 的 MSB-first
    （`1,0,0,1,0,1,1,0`；2 次传输 ⇒ 共 **16** 个 `TRANSFER` 拍）；
  - 位计数观测点 MUST 等于
    `0,0,0,1,2,3,4,5,6,7,7,7,7,0,0,1,2,3,4,5,6,7,7…`
    ⇒ ⇒ 证明**确实**移了 8 次，**且确实**在**第 2 次**装载时复位过
    （⇒ 该复位 MUST **可观测** ⇒ 传输次数 MUST ≥ 2）；
  - 期望值 MUST **手推**，MUST NOT 抄仿真输出；
- **AND** ⚠️ **判别力前提**：发送字节 MUST 是**非回文**（MSB-first ≠ LSB-first）
  ⇒ `0xA5` 是**回文** ⇒ **不可**使用（详见上方「SPI fixture 声明」）
- **AND** parity 断言 MUST **保留**（它是**必要**条件，不是**充分**条件）
