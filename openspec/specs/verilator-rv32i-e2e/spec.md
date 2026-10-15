# verilator-rv32i-e2e Specification

## Purpose
TBD - created by archiving change verilator-rv32i-pipeline-e2e. Update Purpose after archive.
## Requirements
### Requirement: RV32I 5-stage 流水线 MUST 在 VerilatorBackend 下与 InterpreterBackend trace parity

`tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` MUST 在同一 fixture 上分别用 `InterpreterBackend` 与 `VerilatorBackend` 跑 ≥ 20 cycle，对比顶层 `ch_out` 端口 `perf_instructions` 的终值；interpreter 与 verilator 两端的 `perf_instructions` 终值 MUST 相等且 == 3。fixture MUST 装 3 条无 hazard 短指令（`addi x1, x0, 1; addi x2, x0, 2; add x3, x1, x2`），imem MUST 用 `ch_mem` sread（posedge 对齐，与 aread 0-cycle comb 区分），dmem 必须是简化 stub（write=0, read_data=0，因为 3 条短指令不触发 load/store）。

#### Scenario: RV32I 3 条无 hazard 指令 trace parity

- **WHEN** fixture 装 3 条指令编码：
    - `addi x1, x0, 1` = `0x00000094`
    - `addi x2, x0, 2` = `0x00000113`
    - `add x3, x1, x2` = `0x002081B3`
- **AND** 分别用 interpreter 与 verilator 跑 20 cycle
- **THEN** `data_map_.at(io().perf_instructions.impl()->id())` 在两后端都必须 == 3
- **NOTE**: 观测点 `perf_instructions` 是顶层 `ch_out<ch_uint<48>>` 端口（rv32i_pipeline.h:54），verilator 可观测

#### Scenario: RV32I imem 用 ch_mem sread（非 aread）

- **WHEN** fixture 用 `ch_mem<ch_uint<32>, 4>` 初始化 3 条指令 + 1 个 padding
- **AND** imem 通过 sread 端口返回 `instr_data`（posedge 对齐）
- **THEN** trace parity 成立（vs aread 0-cycle comb 会破坏 parity，见 verilator-e2e-coverage spec §Requirement 1 invariant 3）

#### Scenario: RV32I dmem stub 不影响 perf_instructions 验证

- **WHEN** 3 条无 hazard 短指令全部是 ALU 操作（addi / add），不触发 load/store
- **THEN** dmem stub（write=0, read_data=0）的存在不改变 perf_instructions 终值
- **AND** trace parity 仍然成立

#### Scenario: RV32I pipeline 5 阶段不依赖 M0.5 issue #25

- **WHEN** perf_instructions 通过 ch_out<ch_uint<48>> 暴露
- **AND** commit_count 是 always_ff reg 但其值通过顶层 ch_out 端口被 verilator 的 Vtop 字段访问器读出
- **THEN** 即使内部流水线寄存器 (IF/ID, ID/EX, EX/MEM, MEM/WB) 在 M0.5 dispatch 下传播不可靠（issue #25），只要 perf_instructions 的最终 commit_count 累加正确，trace parity 仍然成立
- **NOTE**: 仍依赖 issue #25 影响 eval_sequential 的 register update 路径；commit_count 累加路径在 `rv32i_pipeline.h:221` 是 `commit_count + commit_valid`，如果 issue #25 影响 commit_valid 的寄存器传播，perf_instructions 仍可能错——但这不在本 fixture scope

#### Scenario: RV32I 不触发 CSR / exception / interrupt 路径

- **WHEN** 3 条短指令全部是 ALU 操作，无 ECALL / EBREAK / CSR 读写
- **THEN** RV32I 内部 CSR bank / exception handler / CLINT 路径不触发，fixture 不依赖这些组件的正确性

### Requirement: RV32I e2e test 必须有 explicit InterpreterBackend pinning

`tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` interpreter 一侧 MUST 显式调用 `sim_a.set_backend(std::make_unique<InterpreterBackend>())` —— 默认 `Simulator` 在 `CH_JIT_ENABLE=ON`（默认）下实际走 JIT（`include/simulator.h:569`），parity 测试的 interpreter 侧 MUST 显式 `set_backend()`，否则失败时无法归因 JIT vs verilator。

#### Scenario: InterpreterBackend 显式 pin

- **WHEN** fixture 构造 `Simulator` 实例
- **THEN** interpreter 一侧的 `Simulator` 调用 `set_backend(std::make_unique<InterpreterBackend>())` 显式锁定
- **AND** verifier 一侧的 `Simulator` 调用 `set_backend(std::make_unique<VerilatorBackend>(workdir))`

#### Scenario: CH_JIT_ENABLE=OFF 也能跑

- **WHEN** 环境变量 `CH_JIT_ENABLE=OFF` 或 cmake `-DCH_JIT_ENABLE=OFF`
- **THEN** RV32I e2e test 不受影响（fixture 不依赖 JIT）
- **AND** `ctest -R test_verilator_complex_e2e` 在该环境下仍通过

### Requirement: RV32I e2e test TIMEOUT 必须显式 600s

`tests/CMakeLists.txt` 注册 `test_verilator_complex_e2e` 时 MUST `set_tests_properties(test_verilator_complex_e2e PROPERTIES TIMEOUT 600)`（`add_catch_test` 不支持 TIMEOUT 参数；RV32E 测试 vis RV32I 命令数 > 200，cold verilator compile 约 1-3 min，warm cache ~5-10s，TIMEOUT 600s 给 2× warm 余量）。

#### Scenario: cold cache 编译在 TIMEOUT 内

- **WHEN** `$HOME/.cache/cpphdl/verilator/` 不含 RV32I 设计的 cache key
- **AND** `ctest -R test_verilator_complex_e2e`
- **THEN** 整个测试在 600s 内完成（cold compile + 2 TEST_CASE 运行）

#### Scenario: warm cache 立即恢复

- **WHEN** `$HOME/.cache/cpphdl/verilator/` 已含 RV32I 设计的 cache key（previous test run）
- **AND** `ctest -R test_verilator_complex_e2e`
- **THEN** 整个测试在 60s 内完成（warm cache + 2 TEST_CASE 运行）

