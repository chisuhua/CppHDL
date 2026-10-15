# strengthen-verilator-e2e-coverage

> **STATUS (2026-10-14)**: 起草中。7 阶段 TDD 结构 + 当前进度跟踪。
>
> **阶段依赖**：阶段 1 独立；阶段 2 / 3 / 6 / 7 依赖阶段 1 的 fixture 模板（`ch::ch_device<T>` + `make_temp_dir` helper）；阶段 4 独立；阶段 5 是 verify gate。

## 阶段 1 — RED: trace parity 单测

> 单测成本最低、信号最强的 verilator 回归门。interpreter 是 ground truth，verilator 是被验证对象。

- [ ] 1.1 创建 `tests/test_verilator_trace_parity.cpp`
      ⏳ Pending
- [ ] 1.2 提取共享 helper `tests/external_tool_helpers.h`（含 `make_temp_dir(suffix)` 从 `test_verilator_backend.cpp` / `test_verilator_e2e_harness.cpp` / `test_axi_lite_verilator_e2e.cpp` 复用 + `tool_available` / `run_tool` / `write_to_tempfile` / `ToolResult` 从 `test_verilog_external.cpp` 复用；**4 份 make_temp_dir 全部合并为单一签名**，3 份 tool_available 全部去重）
      ⏳ Pending
- [ ] 1.3 TEST_CASE `TraceParity - CounterFixture32TwentyCycles`：interpreter + verilator 跑 20 cycle，逐 cycle 对比 `io().out` 值
      ⏳ Pending
- [ ] 1.4 TEST_CASE `TraceParity - AxiLiteTopTenCycles`：interpreter + verilator 跑 10 cycle 含完整 AXI4-Lite write+read 握手，逐 cycle 对比所有 handshake signal
      ⏳ Pending
- [ ] 1.5 TEST_CASE `TraceParity - BundleMultiFieldTwentyCycles`：interpreter + verilator 跑 20 cycle 多字段 bundle，对比所有 output port
      ⏳ Pending
- [ ] 1.6 `tests/CMakeLists.txt` 注册 `add_catch_test(test_verilator_trace_parity test_verilator_trace_parity.cpp)`，**三件套**：`target_compile_definitions(test_verilator_trace_parity PRIVATE CATCH_AMALGAMATED_CUSTOM_MAIN)`（CI 全 SKIP 时 main 返回 0，避免 exit 4）+ `set_tests_properties(test_verilator_trace_parity PROPERTIES LABELS "base" RESOURCE_LOCK "verilator_backend" ENVIRONMENT "${_vl_path_env}")`（verilator 编译串行 + PATH 注入）
      ⏳ Pending
- [ ] 1.7 `ctest -R test_verilator_trace_parity --output-on-failure` 全通过
      ⏳ Pending
- [ ] 1.8 **commit 1**：`test(verilator): trace parity single-test for Counter/AxiLite/Bundle (3 fixture × 20 cycle)`
      ⏳ Pending

## 阶段 2 — GREEN: bundle 仿真 fixture

> bundle 是 SpinalHDL-style API 的核心卖点，但 verilator 仿真 0 覆盖。阶段 2 独立 commit。

- [ ] 2.1 创建 `tests/test_verilator_bundle_e2e.cpp`
      ⏳ Pending
- [ ] 2.2 TEST_CASE `BundleE2E - SingleLayerMultiFieldAssign`：`bundle_base` 派生 struct（3×`ch_uint<8>` + `CH_BUNDLE_FIELDS_T`）+ 3 个独立 `ch_out` + 字段赋值，1 cycle verilator
      ⏳ Pending
- [ ] 2.3 TEST_CASE `BundleE2E - NestedTwoLayerFieldAccess`：`InnerBundle` 嵌套为 `OuterBundle` 字段 + `io().out = outer.inner.payload`
      ⏳ Pending
- [ ] 2.4 TEST_CASE `BundleE2E - BundleArrayIndexAccess`：`XBundle bundles[3]` C 数组 + `bundles[1].field` 索引（先 interpreter 验证）
      ⏳ Pending
- [ ] 2.5 TEST_CASE `BundleE2E - BundleAssignmentChain`：同型 `A <<= B; B <<= C;` + `C.field <<= reg` 输出，5 cycle
      ⏳ Pending
- [ ] 2.6 `tests/CMakeLists.txt` 注册 `add_catch_test(test_verilator_bundle_e2e test_verilator_bundle_e2e.cpp)`，**三件套**：`CATCH_AMALGAMATED_CUSTOM_MAIN` + `LABELS "base"` + `RESOURCE_LOCK "verilator_backend"` + `ENVIRONMENT "${_vl_path_env}"`
      ⏳ Pending
- [ ] 2.7 `ctest -R test_verilator_bundle_e2e --output-on-failure` 全通过
      ⏳ Pending
- [ ] 2.8 **如发现疑似 VerilatorBackend bug** → **D8 (b) descope**：该 TEST_CASE 转 `SKIP("backend bug #N; see follow-up change <name>")` + tasks 记录 `⏸️ Deferred to <follow-up-change-name>`；**本 change 不阻塞 archive**
      ⏳ Pending
- [ ] 2.9 **commit 2**：`test(verilator): bundle e2e fixtures (single-layer/nested/array/chain × 4 scenarios)`
      ⏳ Pending

## 阶段 3 — GREEN: Stream/FIFO 仿真 fixture

> Stream 是 SpinalHDL-style API 的核心 handshake 模式。back-pressure 是 FIFO 设计的灵魂。阶段 3 独立 commit。

- [ ] 3.1 创建 `tests/test_verilator_stream_e2e.cpp`
      ⏳ Pending
- [ ] 3.2 复用 `samples/stream_handshake_example.cpp` 的 `StreamFifoComponent` 模式（`examples/spinalhdl-ported/fifo/` 的 `FiFo` 为裸 din/push/pop 端口、非 ch_stream，不可复用），简化 fixture
      ⏳ Pending
- [ ] 3.3 TEST_CASE `StreamE2E - HandshakeSingleTransaction`：producer 发 1 笔 0xCAFEBABE，consumer cycle 2 ready，5 cycle verilator
      ⏳ Pending
- [ ] 3.4 TEST_CASE `StreamE2E - BackpressureMultiTransaction`：producer 持续发 8 笔，consumer 偶数 cycle 不 ready，16 cycle verilator
      ⏳ Pending
- [ ] 3.5 `tests/CMakeLists.txt` 注册 `add_catch_test(test_verilator_stream_e2e test_verilator_stream_e2e.cpp)`，**三件套**：`CATCH_AMALGAMATED_CUSTOM_MAIN` + `LABELS "base"` + `RESOURCE_LOCK "verilator_backend"` + `ENVIRONMENT "${_vl_path_env}"`
      ⏳ Pending
- [ ] 3.6 `ctest -R test_verilator_stream_e2e --output-on-failure` 全通过
      ⏳ Pending
- [ ] 3.7 **如发现疑似 VerilatorBackend bug** → **D8 (b) descope**：documented SKIP + follow-up change
      ⏳ Pending
- [ ] 3.8 **commit 3**：`test(verilator): stream/fifo e2e fixtures (handshake single + backpressure multi × 2 scenarios)`
      ⏳ Pending

## 阶段 4 — REFACTOR: lint 升级 + 改名

> 把 40+ 字符串断言测试中至少 8 个升级为真 lint；把 `test_verilator_three_way` 改名（诚信修复）。阶段 4 独立 commit。

- [ ] 4.1 创建 `tests/external_tool_helpers.h`，提取 `tool_available` / `run_tool` / `write_to_tempfile` / `ToolResult` 4 个 helper（从 `test_verilog_external.cpp` 复制）
      ⏳ Pending
- [ ] 4.2 修改 `tests/test_verilog_external.cpp`，删除内联 helper，改为 `#include "external_tool_helpers.h"`，diff 应只减少代码行不改变行为
      ⏳ Pending
- [ ] 4.3 修改 `tests/test_verilog_arith.cpp::VerilogGen - ArithAdd` 末尾追加 lint 断言
      ⏳ Pending
- [ ] 4.4 修改 `tests/test_verilog_arith.cpp::VerilogGen - ArithMul` 末尾追加 lint 断言
      ⏳ Pending
- [ ] 4.5 修改 `tests/test_verilog_compare.cpp::VerilogGen - CompareEq` 末尾追加 lint 断言
      ⏳ Pending
- [ ] 4.6 修改 `tests/test_verilog_shift.cpp::VerilogGen - Shl` 末尾追加 lint 断言
      ⏳ Pending
- [ ] 4.7 修改 `tests/test_verilog_shift.cpp::VerilogGen - Shr` 末尾追加 lint 断言
      ⏳ Pending
- [ ] 4.8 修改 `tests/test_verilog_gen.cpp::VerilogGen - RegisterWithComplexLogic` 末尾追加 lint 断言
      ⏳ Pending
- [ ] 4.9 修改 `tests/test_verilog_gen.cpp::VerilogGen - AndReduce` 末尾追加 lint 断言
      ⏳ Pending
- [ ] 4.10 修改 `tests/test_verilog_gen.cpp::VerilogGen - Zext` 末尾追加 lint 断言
      ⏳ Pending
- [ ] 4.11 验证 `ctest -R "test_verilog_(arith|compare|shift|gen)" --output-on-failure` 至少 8 个新加 lint 断言全过
      ⏳ Pending
- [ ] 4.12 `git mv tests/test_verilator_three_way.cpp tests/test_codegen_literal_mask.cpp`，修改顶行注释
      ⏳ Pending
- [ ] 4.13 修改 `tests/test_codegen_literal_mask.cpp` 内 `[verilog][verilator]` tag 为 `[verilog][codegen][literal-mask]`
      ⏳ Pending
- [ ] 4.14 修改 `tests/CMakeLists.txt`：把 `add_catch_test(test_verilator_three_way test_verilator_three_way.cpp)` 改为 `add_catch_test(test_codegen_literal_mask test_codegen_literal_mask.cpp)`
      ⏳ Pending
- [ ] 4.15 同一处追加旧名 alias（CTest 无 ALIASES 属性）：`add_test(NAME test_verilator_three_way COMMAND test_codegen_literal_mask)` + `set_tests_properties(test_verilator_three_way PROPERTIES LABELS "base")`
      ⏳ Pending
- [ ] 4.16 验证 `ctest -R test_codegen_literal_mask` 全过 AND `ctest -R test_verilator_three_way` 仍命中（add_test alias 验证）
      ⏳ Pending
- [ ] 4.17 **commit 4a**：`refactor(verilog-test): share external_tool_helpers.h + 8 lint 升级 (arith/mul/eq/shl/shr/always_ff/reduce/zext)`
      ⏳ Pending
- [ ] 4.18 **commit 4b**：`chore(tests): rename test_verilator_three_way → test_codegen_literal_mask (诚信修复误导命名)`
      ⏳ Pending

## 阶段 5 — VERIFY: 全量 ctest 0 回归 + openspec validate

> 验证 4 阶段 commit 不破坏任何现有测试，并完成 openspec 流程。

- [ ] 5.1 `cmake --build build -j$(nproc)` 0 errors, 0 warnings
      ⏳ Pending
- [ ] 5.2 `ctest -E perf_tests --output-on-failure -j$(nproc)` 154 → 158 全过（+3 新 test binary + 1 旧名 add_test alias；阶段 6/7 完成后由阶段 8 升级到 160）
      ⏳ Pending
- [ ] 5.3 `ctest -R "test_verilator_(trace_parity|bundle_e2e|stream_e2e)" --output-on-failure` 至少 6 个实际执行（非 SKIP）
      ⏳ Pending
- [ ] 5.4 `ctest -R "test_verilog_(arith|compare|shift|gen)" --output-on-failure` 至少 8 个新加 lint 断言全过
      ⏳ Pending
- [ ] 5.5 `ctest -R test_codegen_literal_mask` 全过 AND `ctest -R test_verilator_three_way` 仍命中
      ⏳ Pending
- [ ] 5.6 `ctest -R perf_tests --output-on-failure`（已跑过的全量 perf）仍全过
      ⏳ Pending
- [ ] 5.7 `./run_all_ported_tests.sh` 28/28 仍通过
      ⏳ Pending
- [ ] 5.8 `openspec validate strengthen-verilator-e2e-coverage --strict` 通过
      ⏳ Pending
- [ ] 5.9 `openspec validate --specs --strict` 8/8 passed（+1 新 capability `verilator-e2e-coverage`）
      ⏳ Pending
- [ ] 5.10 阶段 5 = 阶段 1-4 commit 后的回归 gate（build + ctest + validate change），**不 archive**；archive 留到阶段 8
      ⏳ Pending

## 阶段 6 — GREEN: 代表性 Memory 设计仿真

> 35 种设计中 memory 类（ch_mem + pointer + full/empty）和 burst 类（AXI4-Full）是 verilator 仿真 0 覆盖的典型。本阶段独立 commit。

- [ ] 6.1 创建 `tests/test_verilator_memory_e2e.cpp`
      ⏳ Pending
- [ ] 6.2 fixture 1: `SyncFifoFixture`（简化自 `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp::FiFo`，`ch_mem<N>` + rd/wr pointer + full/empty 标志）
      ⏳ Pending
- [ ] 6.3 TEST_CASE `MemoryE2E - SyncFifoTraceParity` ：interpreter + verilator 跑 16 cycle 含 1 笔 data round-trip，逐 cycle 对比 `full`/`empty`/data
      ⏳ Pending
- [ ] 6.4 fixture 2: `AxiFullFixture`（简化自 `examples/axi4/axi4_full_example.cpp` 5-channel + burst=4 笔 write/read）
      ⏳ Pending
- [ ] 6.5 TEST_CASE `MemoryE2E - AxiFullBurstWriteRead`：interpreter + verilator 跑 burst write 4 笔 + burst read 4 笔，对比 `rdata` 终值（4 笔与写入顺序一致）
      ⏳ Pending
- [ ] 6.6 `tests/CMakeLists.txt` 注册 `add_catch_test(test_verilator_memory_e2e test_verilator_memory_e2e.cpp)`，**三件套**：`CATCH_AMALGAMATED_CUSTOM_MAIN` + `LABELS "base"` + `RESOURCE_LOCK "verilator_backend"` + `ENVIRONMENT "${_vl_path_env}"` + `set_tests_properties(... TIMEOUT 300)`（AxiFull 5-channel 编译非平凡）
      ⏳ Pending
- [ ] 6.7 `ctest -R test_verilator_memory_e2e --output-on-failure` 全通过
      ⏳ Pending
- [ ] 6.8 **如发现疑似 VerilatorBackend bug** → **D8 (b) descope**：documented SKIP + follow-up change
      ⏳ Pending
- [ ] 6.9 **commit 5**：`test(verilator): memory e2e fixtures (sync_fifo + axi4_full burst × 2 scenarios)`
      ⏳ Pending

## 阶段 7 — GREEN: 代表性复杂设计仿真

> 35 种设计中复杂类（CPU pipeline + FSM + shift reg）是 verilator 仿真 0 覆盖的另一典型。本阶段独立 commit。RV32I 编译慢需 TIMEOUT 120s。

- [ ] 7.1 创建 `tests/test_verilator_complex_e2e.cpp`
      ⏳ Pending
- [ ] 7.2 fixture 1: `Rv32iPipelineFixture`（简化自 `include/cpu/pipeline/rv32i_pipeline.h`，3 条指令 `addi x1,x0,1; addi x2,x0,2; add x3,x1,x2` 压 EX→EX / MEM→WB forwarding，**不压** load-use / branch flush——后者由 interpreter 层 `test_rv32i_pipeline` 覆盖；`ch_mem` imem + 简化 dmem stub）
      ⏳ Pending
- [ ] 7.3 TEST_CASE `ComplexE2E - Rv32iShortInstructionTraceParity`：interpreter + verilator 跑 ~20 cycle 直至 3 条指令完成，**观测 `perf_instructions` 终值 == 3**（不用 `regs[3]`，见 design D1 不变量 4）
      ⏳ Pending
- [ ] 7.4 fixture 2: `SpiFsmFixture`（基于 `ch_state_machine` 新建 4-state FSM IDLE→START→TRANSFER→STOP，与 `examples/spinalhdl-ported/spi/spi_controller.cpp` 同构但**非简化**——源用 `ch_reg<ch_uint<2>>` 手写而非 DSL；shift reg + clock div）
      ⏳ Pending
- [ ] 7.5 TEST_CASE `ComplexE2E - SpiFsmStateTransitionTraceParity`：interpreter + verilator 跑 32 cycle 完整 1 byte transfer，观测绑独立 `ch_out dbg_state`，对比 FSM 状态转移序列
      ⏳ Pending
- [ ] 7.6 `tests/CMakeLists.txt` 注册 `add_catch_test(test_verilator_complex_e2e test_verilator_complex_e2e.cpp)`，**三件套**：`CATCH_AMALGAMATED_CUSTOM_MAIN` + `LABELS "base"` + `RESOURCE_LOCK "verilator_backend"` + `ENVIRONMENT "${_vl_path_env}"` + `set_tests_properties(test_verilator_complex_e2e PROPERTIES TIMEOUT 600)`（**`add_catch_test` 不支持 TIMEOUT 参数**；RV32I 编译 120s 无安全余量，按 perf_subprocess_isolation 300→1800 先例给 2× 余量；仍超时视为环境性能问题，调 TIMEOUT 而非降 fixture）
      ⏳ Pending
- [ ] 7.7 `ctest -R test_verilator_complex_e2e --output-on-failure` 全通过
      ⏳ Pending
- [ ] 7.8 **如发现疑似 VerilatorBackend bug** → **D8 (b) descope**：documented SKIP + follow-up change
      ⏳ Pending
- [ ] 7.9 **commit 6**：`test(verilator): complex e2e fixtures (rv32i_pipeline + spi_fsm × 2 scenarios)`
      ⏳ Pending

## 阶段 8 — VERIFY: 全量 ctest 0 回归 + openspec validate（最终）

> 验证 6 阶段 commit（阶段 1-4 + 6 + 7）不破坏任何现有测试，阶段 5 的 verify 已经在 commit 1-3 完成后跑过；本阶段是 archive 前的最终全量回归。

- [ ] 8.1 `cmake --build build -j$(nproc)` 0 errors, 0 warnings
      ⏳ Pending
- [ ] 8.2 `ctest -E perf_tests --output-on-failure -j$(nproc)` 154 → 160 全过（+5 新 test binary + 1 旧名 add_test alias）
      ⏳ Pending
- [ ] 8.3 `ctest -R "test_verilator_(trace_parity|bundle_e2e|stream_e2e|memory_e2e|complex_e2e)" --output-on-failure` 至少 12 个实际执行（非 SKIP；3+4+2+2+2 = 13 fixture 减去 SKIP ≥ 12）
      ⏳ Pending
- [ ] 8.4 `ctest -R "test_verilog_(arith|compare|shift|gen)" --output-on-failure` 至少 8 个新加 lint 断言全过
      ⏳ Pending
- [ ] 8.5 `ctest -R test_codegen_literal_mask` 全过 AND `ctest -R test_verilator_three_way` 仍命中
      ⏳ Pending
- [ ] 8.6 `./run_all_ported_tests.sh` 28/28 仍通过
      ⏳ Pending
- [ ] 8.7 `openspec validate strengthen-verilator-e2e-coverage --strict` 通过
      ⏳ Pending
- [ ] 8.8 `openspec validate --specs --strict` 9/9 passed（archive 之后 `openspec/specs/verilator-e2e-coverage/` 才出现，archive 前只有 8/8）—— **先 archive 再 validate --specs**
      ⏳ Pending
- [ ] 8.9 `openspec archive strengthen-verilator-e2e-coverage`（CLI 自动合并 `specs/verilator-e2e-coverage/spec.md` 的 `## ADDED Requirements` 到 `openspec/specs/verilator-e2e-coverage/spec.md`）
      ⏳ Pending

## 后续 PR（独立 change，不在本 scope）

- 修复 `test_verilator_bundle_e2e` / `test_verilator_stream_e2e` 在阶段 2/3 中发现的 VerilatorBackend bug（独立 change）
- trace parity 进一步扩展到 FIFO depth / 状态机 / 流水线（独立 change）
- perf three-way 升级为 trace diff 而非 perf 数字对比（独立 change）
- Windows MSVC 兼容（当前 helper 用 `popen` / `mkstemp`，仅 POSIX）

## 关联基线

- **ADR-035 §M5 e2e tier**：在 e2e tier 上**增加覆盖广度**而非修改后端
- **verilator-backend-e2e spec**：低层契约已确立，本 change 在其上新增覆盖
- **verilator-error-flow spec**：8 个错误码，新测试在 verilator 失败时 SKIP
- **tests/AGENTS.md**：no misleading naming（D5 改名的依据）
- **f2 fix-perf-subprocess-isolation**：trace parity 不复用 cross-DUT state（每个 fixture 重新 `ch_device<>` 构造）
