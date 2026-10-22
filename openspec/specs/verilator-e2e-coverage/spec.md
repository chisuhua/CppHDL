# verilator-e2e-coverage Specification

## Purpose
TBD - created by archiving change strengthen-verilator-e2e-coverage. Update Purpose after archive.

## Requirements

### Requirement: Verilator 与 Interpreter trace parity 单测覆盖 ≥ 3 个 fixture

- **MODIFIED**: 移除 invariant 5 中"trace fixture MUST NOT 依赖 always_ff 寄存器传播（issue #25）"的旧误诊。issue #25 的真正残余症状是"层次化设计的子模块 input port 被错误展平为顶层 input port 而无 assign"（由 `verilator-hier-input-flattening-fix` 修复）。修复后 always_ff 寄存器由 Verilator 的 2-step 时钟翻转自动触发，寄存器驱动的 ch_out 是合法观测点。

#### Scenario: 修复后寄存器驱动的 ch_out 是合法观测点

- **WHEN** fixture 用 `ch_reg<ch_uint<32>> reg(0_d); reg->next = reg + 1_d; io().out = reg;`（CounterFixture，reg-driven ch_out）
- **AND** 跑 50 cycle
- **THEN** interpreter 与 verilator 端 `data_map_[io().out.id()]` 在 cycle 50 都 == 50（修复前 verilator 也通过，因为无 ch_module 嵌套；修复后仍通过）
- **AND** 同样的 reg-driven ch_out 在有 ch_module 嵌套的 RV32I fixture 中（perf_instructions = commit_count）也必须正确传播到 verilator 端（修复前 = 0，修复后 = 累加值）

#### Scenario: 修复前 vs 修复后行为对照

- **WHEN** 比较 verilator-rv32i-pipeline-e2e change (archive 2026-10-14) 的 `tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` 修复前后
- **THEN** 修复前：`perf_instructions == 0`（D8 (b) SKIP）
- **AND** 修复后：`perf_instructions == 12`（真实跑通）
- **NOTE**: 这是"verilator-hier-input-flattening-fix" change 的 success criteria 之一

### Requirement: bundle 构造在 Verilator 仿真下行为契约

`tests/test_verilator_bundle_e2e.cpp` **MUST** 覆盖至少 4 个 bundle 场景，每个在 `VerilatorBackend` 仿真下验证 `data_map_` 中的 `ch_out` 值与 `InterpreterBackend` 终值一致。所有 bundle 场景的观测点 **MUST** 为独立 `ch_out` 端口（遵守 AGENTS.md "bundle 不作 IO，字段绑独立端口"约定）。

#### Scenario: 单层多字段 bundle `<<=` 全部字段

- **WHEN** 定义 `struct AbcBundle : public bundle_base<AbcBundle> { ch_uint<8> a, b, c; CH_BUNDLE_FIELDS_T(a, b, c) };`（CRTP 模式见 `tests/test_bundle.cpp:18-29`），3 个独立 `ch_out` 端口分别绑 `a, b, c`，对字段赋值（`a = 1; b = 2; c = 3;`）
- **AND** 用 `VerilatorBackend` 跑 1 cycle
- **THEN** `data_map_` 中 3 个 output port 分别 == 1, 2, 3

#### Scenario: 双层嵌套 bundle 字段访问

- **WHEN** `struct InnerBundle : public bundle_base<InnerBundle> { ch_uint<8> payload; CH_BUNDLE_FIELDS_T(payload) };` 作为 `OuterBundle`（同模式派生，含 `ch_uint<4> tag; InnerBundle inner;`）的字段，`io().out = outer.inner.payload;`
- **AND** `outer.tag = 0xa; outer.inner.payload = 0x42;`
- **AND** 用 `VerilatorBackend` 跑 1 cycle
- **THEN** `data_map_.at(io().out.impl()->id())` == 0x42
- **NOTE** 嵌套字段的 `CH_BUNDLE_FIELDS_T(tag, inner)` 注册路径先在 interpreter 下验证（无既有测试先例）

#### Scenario: bundle 数组索引访问

- **WHEN** C 数组 `XBundle bundles[3];`（`XBundle` 为 `bundle_base` 派生 struct；bundle 数组无既有先例，先在 interpreter 下验证），对 `bundles[i].field` 各自赋值，`io().out = bundles[1].field;`
- **AND** 用 `VerilatorBackend` 跑 1 cycle
- **THEN** `data_map_.at(io().out.impl()->id())` 等于 `bundles[1].field` 的赋值

#### Scenario: bundle `<<=` 链

- **WHEN** 同型 bundle `A <<= B; B <<= C;`（`operator<<=` 仅接受同型 bundle 左值，`bundle_base.h:119`），`C.field <<= ch_reg<ch_uint<8>>(0)` 的输出（寄存器每 cycle +1），`A.field` 绑到独立 `ch_out` 端口 `io().out`
- **AND** 用 `VerilatorBackend` 跑 5 cycle
- **THEN** `data_map_.at(io().out.impl()->id())` == cycle 数（即 A 的字段 == C 的字段值）

### Requirement: Stream/FIFO 构造在 Verilator 仿真下行为契约

`tests/test_verilator_stream_e2e.cpp` **MUST** 覆盖至少 2 个 Stream/FIFO 场景。

#### Scenario: Stream handshake 单笔

- **WHEN** producer 经 `m2sPipe()`（1-cycle valid/payload 寄存级，`include/bundle/stream_bundle.h:54`）连接 consumer，发送 1 笔 `ch_stream<ch_uint<32>>` payload = 0xCAFEBABE
- **AND** consumer 在 cycle 2 ack ready
- **AND** 用 `VerilatorBackend` 跑 5 cycle
- **THEN** consumer 端在 `fire()`（valid && ready）为真的首个 cycle 收到 payload == 0xCAFEBABE

#### Scenario: Stream back-pressure 多笔

- **WHEN** producer 持续发 8 笔，consumer 偶数 cycle 不 ready
- **AND** 用 `VerilatorBackend` 跑 16 cycle
- **THEN** consumer 端在 ready 拉高的 cycle 收到对应 payload，且无 payload 丢失或乱序

### Requirement: 代表性 Memory 设计在 Verilator 仿真下行为契约

`tests/test_verilator_memory_e2e.cpp` **MUST** 覆盖至少 2 个 memory/burst 类场景：sync FIFO（`ch_mem` + rd/wr pointer + full/empty）和 AXI4-Full Slave（5-channel handshake + burst=4 笔 write/read）。每个场景 **MUST** 同时跑 `InterpreterBackend` 与 `VerilatorBackend`，对比 `data_map_` 终值；burst 写读场景验证 `rdata` 序列与写入顺序一致。

#### Scenario: Sync FIFO trace parity 16 cycle

- **WHEN** fixture 为 `SyncFifoFixture`（`ch_mem<N>` + rd/wr pointer + `full`/`empty` 标志绑独立 `ch_out`），写入 1 笔、读出 1 笔
- **AND** interpreter + verilator 各跑 16 cycle
- **THEN** 16 cycle 内 `data_map_` 中 `full`/`empty` 状态序列在两后端必须一致
- **AND** 读出 data 终值在两后端必须等于写入 data

> ⚠️ **归因订正（2026-10-22，`chlib-stream-semantics-2026-10`）**：原文本称该 fixture
> 「简化自 `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp::FiFo`」——
> **该归因不准确**：fixture **已实现**空/满门控
> （`will_write = wr_en & !full_reg` / `will_read = rd_en & !empty_reg`，双指针受门），
> 而它所「简化自」的 example **没有**该门控 ⇒ example 的指针在满/空时照常前进 ⇒
> cycle 5 起进入**假满**并永久卡死。
> ⇒ fixture 与 example **不是**同一语义；example 只能作为「0-cycle 数据通路」的参照，
> **MUST NOT** 被当作「正确实现」的参照。

#### Scenario: AXI4-Full burst write+read 4 笔

- **WHEN** fixture 为 `AxiFullFixture`（简化自 `examples/axi4/axi4_full_example.cpp`，5-channel handshake + burst=4 笔），先后发 1 次 burst write 4 笔（`0x11, 0x22, 0x33, 0x44`）+ 1 次 burst read 4 笔
- **AND** interpreter + verilator 各跑到 burst read 完成
- **THEN** `rdata` 序列（4 笔）在两后端必须 == `[0x11, 0x22, 0x33, 0x44]`（按写入顺序）

#### Scenario: chlib stream FIFO 的 FWFT 逐拍 parity

- **WHEN** fixture 为 `chlib::stream_fifo`（`include/chlib/stream.h`），序列覆盖
  push / pop / 空 FIFO / 满 FIFO
- **AND** 用 `DUAL_BACKEND_TEST_PAIR` 跑 interpreter + verilator
- **THEN** 两后端的 `payload` 与 `valid` **逐拍**序列必须完全一致

> ⚠️ FWFT 的 0-cycle 语义是历史上漂移过的地方
> （`sim-mem-read-timing-align-2026-10` 才发现 interpreter 与 Verilog 差 1 拍）
> ⇒ 只跑 interpreter 的断言 MUST NOT 被当作该契约已被守住的证据。
> 详见 capability `chlib-stream-semantics` 的「`chlib::stream_fifo` 的行为 MUST 有
> 双后端 parity 覆盖」。

### Requirement: 代表性复杂设计在 Verilator 仿真下行为契约

`tests/test_verilator_complex_e2e.cpp` **MUST** 覆盖至少 2 个复杂设计场景：RV32I 5-stage 流水线（短指令序列 trace parity）和 SPI 控制器 FSM（4-state `ch_state_machine` 状态转移 trace parity）。每个场景 **MUST** 同时跑 `InterpreterBackend` 与 `VerilatorBackend`，对比 `data_map_` 终值或状态转移序列。CMake 注册 TIMEOUT **600s**（`add_catch_test` 不支持 TIMEOUT 参数，必须 `set_tests_properties(... TIMEOUT 600)` 单独设置；verilator 编译 RV32I 流水线较慢，120s 无安全余量）。

**RV32I 简化度选择**：fixture 选 **3 条无 hazard 指令 `addi x1, x0, 1; addi x2, x0, 2; add x3, x1, x2`** 是因为它压测了 EX→EX 与 MEM→WB forwarding 路径（`add x3, x1, x2` 读 x1/x2 分别在 2 条/1 条前指令中写），**不覆盖** load-use hazard / branch flush——后者由现有 `test_rv32i_pipeline` / `test_hazard` 在 interpreter 层覆盖，本测试目标是 backend parity 而非流水线正确性。fixture 观测点用已暴露端口 **`perf_instructions`**（`rv32i_pipeline.h:54`），终值 == 3；不观测 `regs[3]`（深内 reg_file.verilator 不可观测，见 design D1 不变量 4）。

**SPI fixture 声明**：fixture 用 **`ch_state_machine` 新建 4-state FSM**（IDLE→START→TRANSFER→STOP，与 `examples/spinalhdl-ported/spi/spi_controller.cpp` 同构但**非简化**——spi_controller.cpp 用手写 `ch_reg<ch_uint<2>>` + if/else 而非 `ch_state_machine` DSL），shift reg + clock div，发 1 byte 0xA5。

#### Scenario: RV32I Pipeline 3 条指令 trace parity

- **WHEN** fixture 为 `Rv32iPipelineFixture`（简化自 `include/cpu/pipeline/rv32i_pipeline.h`，`ch_mem` imem 装 3 条指令 `addi x1, x0, 1; addi x2, x0, 2; add x3, x1, x2` + 简化 dmem stub）
- **AND** interpreter + verilator 各跑至 3 条指令 WB 完成（约 20 cycle）
- **THEN** `data_map_.at(io().perf_instructions.impl()->id())` 在两后端必须 == 3
- **NOTE**: 不观测 `regs[3]`（verilator 不可观测内部 reg_file，见 design D1 不变量 4）

#### Scenario: SPI FSM 状态转移 trace parity

- **WHEN** fixture 为 `SpiFsmFixture`（基于 `ch_state_machine` 新建 4-state FSM IDLE→START→TRANSFER→STOP，与 `examples/spinalhdl-ported/spi/spi_controller.cpp` 同构但**非简化**——源文件用 `ch_reg<ch_uint<2>>` 手写而非 DSL；shift reg + clock div），发 1 byte 0xA5
- **AND** interpreter + verilator 各跑 32 cycle 完整 1 byte transfer
- **THEN** 32 cycle 内 `data_map_` 中 FSM 状态序列在两后端必须一致（观测点：绑独立 `ch_out dbg_state`，不用内部 reg）
- **AND** `mosi` 移出 8 bit 序列在两后端必须 == 0xA5 的 MSB-first 序列

### Requirement: codegen 模板测试 MUST 升级为 verilator --lint-only 真 lint

至少 8 个 `tests/test_verilog_{arith,compare,shift,gen}.cpp` 的 `TEST_CASE` **MUST** 在原有 regex 字符串断言基础上，**追加**调用 `popen("verilator --lint-only -Wno-fatal <tmp>.v 2>&1")`，要求 exit code == 0。被升级的 8+ 个 TEST_CASE **SHALL** 从 `test_verilog_{arith,compare,shift,gen}` 各自至少 1 个，覆盖 add / mul / eq / shl / shr / always_ff / reduce / extend 8 类操作。

#### Scenario: VerilogGen - ArithAdd lint 升级

- **WHEN** `tests/test_verilog_arith.cpp::VerilogGen - ArithAdd` 跑通原有 regex 断言后
- **AND** 追加 `popen("verilator --lint-only -Wno-fatal")` 真 lint
- **THEN** `verilator --lint-only` 退出码 == 0

#### Scenario: VerilogGen - RegisterWithComplexLogic lint 升级

- **WHEN** `tests/test_verilog_gen.cpp::VerilogGen - RegisterWithComplexLogic` 跑通原有 regex 断言后
- **AND** 追加真 lint
- **THEN** `verilator --lint-only` 退出码 == 0（确认 `always_ff @(posedge clk)` 模板可解析）

#### Scenario: 8 个升级 TEST_CASE 全部 lint 通过

- **WHEN** `ctest -R "test_verilog_(arith|compare|shift|gen)" --output-on-failure`
- **THEN** 至少 8 个 TEST_CASE 包含 lint 断言，全部通过
- **NOTE**: 保留原 regex 断言作为 fast path；lint 断言在 verilator 不可用时 SKIP（同 `test_verilog_external.cpp` 模式）

### Requirement: codegen 模板测试 MUST 共享外部工具 helper

`tests/external_tool_helpers.h` **MUST** 提供 3 个 helper：`tool_available(const char* cmd)` / `run_tool(const std::string& cmd)` / `write_to_tempfile(const std::string& body, const std::string& suffix)`，由 `test_verilog_external.cpp` + `test_verilog_{arith,compare,shift,gen}.cpp` + 本 change 新增 test 共享。

#### Scenario: helper 函数签名稳定

- **WHEN** 阅读 `tests/external_tool_helpers.h`
- **THEN** 3 个 helper 函数签名与 `test_verilog_external.cpp` 当前实现完全一致（`bool tool_available(const char* cmd)`、`ToolResult run_tool(const std::string& cmd)`、`std::string write_to_tempfile(...)`）
- **AND** `test_verilog_external.cpp` 行为 0 改动（diff 仅限删除内联 helper + 新增 `#include "test_helpers.h"`）

#### Scenario: helper 跨平台 POSIX

- **WHEN** helper 内部使用 `popen` / `mkstemp` / `WEXITSTATUS` / `WIFEXITED`
- **THEN** 在 macOS / Linux 上都通过编译（与 `test_verilog_external.cpp` 现状一致，不引入 Windows 支持）

### Requirement: 误导命名的 verilator 测试 MUST 改名

`tests/test_verilator_three_way.cpp` **MUST** 改名为 `tests/test_codegen_literal_mask.cpp`，CTest target 名同步改为 `test_codegen_literal_mask`，并**保留**旧名 ctest alias（`add_test(NAME test_verilator_three_way COMMAND test_codegen_literal_mask)`，因 CTest 无 ALIASES 测试属性）以维持历史 ctest 命令兼容。文件内容除顶行注释与 TEST_CASE tag 行外 0 改动；tag 从 `[verilog][verilator]` 改为 `[verilog][codegen][literal-mask]`。

#### Scenario: 文件名 + target 同步

- **WHEN** 阅读 `tests/CMakeLists.txt` 的 `add_catch_test` 注册
- **THEN** 存在 `add_catch_test(test_codegen_literal_mask test_codegen_literal_mask.cpp)` 行
- **AND** 紧随其后 `add_test(NAME test_verilator_three_way COMMAND test_codegen_literal_mask)`（旧名 alias）
- **AND** `tests/test_verilator_three_way.cpp` 文件不存在

#### Scenario: ctest alias 兼容

- **WHEN** 运行 `ctest -R test_verilator_three_way --output-on-failure`
- **THEN** 命中 alias test 并执行 `test_codegen_literal_mask` 可执行文件，全部原 TEST_CASE 通过（行为不变）

#### Scenario: 行为 0 改动

- **WHEN** `git diff` 本 change 涉及 `tests/test_codegen_literal_mask.cpp`（原 `test_verilator_three_way.cpp`），排除顶行注释与 TEST_CASE tag 行
- **THEN** 其余部分 diff 为空（允许变化仅：顶行 `// tests/test_codegen_literal_mask.cpp` 1 行 + tag 从 `[verilog][verilator]` 改为 `[verilog][codegen][literal-mask]`）

### Requirement: Verilator 仿真覆盖深度 MUST 在 ctest 中可见

`ctest -N` **MUST** 列出 6 个新 test：`test_verilator_trace_parity` / `test_verilator_bundle_e2e` / `test_verilator_stream_e2e` / `test_verilator_memory_e2e` / `test_verilator_complex_e2e` / `test_codegen_literal_mask`（原 `test_verilator_three_way` 改名），且每个新 test 在 `BUILD_VERILATOR=ON` 时**至少 1 个** TEST_CASE 在有 verilator on PATH 时**实际执行**（非全 SKIP）。

#### Scenario: 新 test 在 verilator 不可用时优雅 SKIP

- **WHEN** `BUILD_VERILATOR=OFF` 或 verilator 不在 PATH
- **THEN** 6 个新 test 全部 SKIP 退出码 0（不污染 ctest 矩阵）
- **AND** `ctest -R test_verilator_e2e` 在该环境下仍以 exit 0 完成

#### Scenario: 新 test 在 verilator 可用时实际执行

- **WHEN** `BUILD_VERILATOR=ON` 且 verilator 在 PATH
- **AND** `ctest -R "test_verilator_(trace_parity|bundle_e2e|stream_e2e|memory_e2e|complex_e2e)" --output-on-failure`
- **THEN** 每个 test 至少 1 个 TEST_CASE 显示 `Passed`（非 `Skipped`）
- **AND** 总通过数 ≥ 12（trace parity 3 + bundle 4 + stream 2 + memory 2 + complex 2 - 部分 SKIP = 至少 12 个实际执行）

### Requirement: 8 阶段 tasks.md MUST 包含全部覆盖补足（6 实施阶段 + 2 verify gate）

`tasks.md` **MUST** 按 8 阶段组织：阶段 1 (trace parity) / 阶段 2 (bundle 仿真) / 阶段 3 (Stream/FIFO 仿真) / 阶段 4 (lint 升级 + 改名) / 阶段 5 (阶段 1-4 verify gate，**不 archive**) / 阶段 6 (Memory 设计：sync_fifo + axi4_full) / 阶段 7 (复杂设计：rv32i_pipeline + spi_fsm) / 阶段 8 (全量 ctest 0 回归 + `openspec validate --specs` + 唯一一次 `openspec archive`)。每阶段独立可 commit。

#### Scenario: 8 阶段 tasks 完整

- **WHEN** 阅读 `openspec/changes/archive/2026-10-14-strengthen-verilator-e2e-coverage/tasks.md`
- **THEN** 包含 8 个 `##` 阶段 heading（阶段 1-4 + 阶段 5 verify gate + 阶段 6/7 + 阶段 8 最终 verify + archive），每阶段含 ≥3 个 `- [ ]` task 且以 commit 或 verify 任务收尾
- **AND** 所有 task 最终状态为 `[x]` 后才能 `openspec archive`（archive 只在阶段 8 一次，阶段 5 仅有 verify gate 不 archive）

#### Scenario: archive 前置 openspec validate 通过

- **WHEN** `openspec validate strengthen-verilator-e2e-coverage --strict`
- **THEN** 输出 `Change 'strengthen-verilator-e2e-coverage' is valid`
- **AND** `openspec validate --specs --strict` 输出 8/8 passed（+1 新 capability `verilator-e2e-coverage`）
