# Proposal: verilator-hier-input-flattening-fix

> 修 VerilatorBackend 的"层次化输入平铺"bug：带 driver 的子模块 `ch_in` 端口在 Verilog 中被错误展平为顶层 input port，导致 Vtop 永远看到 X（0）值，RV32I / AXI4-Lite 等任何含 `ch_module` 嵌套的设计在 verilator 仿真下功能上完全静默失效。

## Why

`verilator-rv32i-pipeline-e2e` change (2026-10-14 archive) 留下 RV32I `perf_instructions==0` 的 D8 (b) SKIP。Oracle 调查揭示真正根因：`codegen_verilog.cpp::print_header` (L290-374) 把所有 `type_input` 无差别 emit 为顶层 `input` port，包括有内部 driver 的子模块输入；`print_logic` (L475-567) switch 没有 `type_input` 分支 → driven input 在 Verilog 里没有 `assign` → verilator 看到 0。RV32I `instr_data=0` 触发 NOP 循环 → `commit_count=0`；AXI4-Lite `slave.awvalid=0` 触发握手死锁 → `rvalid=0`（`test_axi_lite_verilator_e2e.cpp:276` 断言 `rvalid==0` 是 bug 行为写进测试）。JIT 已正确处理（`src/jit/jit_ir_gen_lnode_io.cpp:31-46` + AGENTS.md "JIT RULES"），verilator 缺这条 codegen 路径。issue #25 修了 2-step 时钟翻转，**残余症状"AXI rvalid=0 / RV32I perf=0"被误诊为寄存器传播**——本 change 重新定位 + 真正修复。

## What Changes

- **Modify** `src/codegen_verilog.cpp::print_header` (L307-316)：filter 掉 `inputimpl::driver() != nullptr`（等价 `num_srcs() > 0`）的 input 节点 —— 这些 input 不应是顶层 port，是内部 `logic` wire
- **Modify** `src/codegen_verilog.cpp::print_decl` (L320-326)：把有 driver 的 input 节点声明为 `logic` 而非顶层 `input`
- **Modify** `src/codegen_verilog.cpp::print_logic` (L486-563)：**新增 `type_input` 分支**，emit `assign <in_name> = <driver_name>;`（driver_name 走 proxy 链 walk，类似 `type_output` 分支的逻辑）
- **Modify** `src/core/verilator_backend.cpp::emit_sim_main_postlude` (L219-280)：`set_input_Vtop` / `get_field_ptr_Vtop` 谓词同步跳过被驱动的 input（否则引用已不存在的 Vtop 字段会编译失败）
- **Modify** `src/core/verilator_backend.cpp::build_port_access_table` (L589-627)：同步跳过被驱动的 input
- **Modify** `tests/test_axi_lite_verilator_e2e.cpp:276`：恢复 `rvalid == 1` + 加 `rdata == 0xDEADBEEF` 断言（修复前 SKIP 掉了这层验证）
- **Modify** `tests/test_verilator_complex_e2e.cpp:374-381`：删除 `ComplexE2E - Rv32iShortInstructionTraceParity` 的 documented SKIP，恢复 `REQUIRE(perf_b_val == 3)` 真实测试
- **Add** capability `verilator-hier-input` under `openspec/specs/verilator-hier-input/spec.md`：定义层次化设计的输入驱动契约（带 driver 的子模块 input 不展平为顶层 port）
- **Modify** `verilator-e2e-coverage` capability：in change 的 `specs/verilator-e2e-coverage/spec.md` 加 `## MODIFIED Requirements`（invariant 5 从"MUST NOT 依赖寄存器传播"放宽为"always_ff 寄存器由 Verilator 自动触发，寄存器驱动的 ch_out 是合法观测点"）。OpenSpec 规则：spec 修改必须在 change 流程内合并，不能直接改 `openspec/specs/`
- **Modify** `AGENTS.md` "VERILATOR BACKEND RULES"：把 issue #25 残余症状从"寄存器传播"更正为"层次化输入平铺"，新增对应 codegen 规则

## 目标

- **G1**：`verilator-reset-initial-value-mismatch` 测试 `BundleMultiField` 非零初值 trace parity 在 `cpu.io().instr_data` 嵌套场景下也成立（即 RV32I fixture 解锁）
- **G2**：`test_axi_lite_verilator_e2e.cpp::WriteReadReg0ThroughBackend` 恢复 `rdata == 0xDEADBEEF` 真实断言（修复前是 SKIP）
- **G3**：`test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` 删除 documented SKIP，`REQUIRE(perf_instructions == 12)` 在两后端都通过（**12 而非 3**：imem 4-word 循环回绕，3 条 ALU 指令 commit_valid 在 20 cycle 内持续累加，cycle 20 时 perf_instructions ≈ 12，详见 design D5）
- **G4**：所有现有 159 个 ctest 0 回归
- **G5**：`openspec validate --specs --strict` 12/12 passed（+1 新 capability `verilator-hier-input`）
- **G6**：`openspec archive verilator-hier-input-flattening-fix` 成功

## 非目标

- **NG1**：不修改 JIT 路径（JIT 已正确处理 driver，AGENTS.md "JIT RULES" `type_input MUST check the driver` 条款）
- **NG2**：不修改 InterpreterBackend（interpreter 通过 `instr_input::eval` 直接走 driver 链，与 verilator codegen 路径无关）
- **NG3**：不修复 `codegen_verilog.cpp:551` 的 `_` 前缀 driver 静默丢弃问题（独立小 bug，留作未来 change）
- **NG4**：不重写 print_header 让 ch_module 子输出也只在"无内部消费者"时提升为顶层 port（无害臃肿问题，留作未来 change）
- **NG5**：不修改 RV32I pipeline 自身（fixture 已是真仿真，问题在 backend 路径）

## 验证

- `cmake --build build -j$(nproc)` 0 errors, 0 warnings on changed files
- `ctest -E "perf_tests|perf_three_way" -j$(nproc)` 全过（159 个 + alias 159 个）
- `ctest -R "test_verilator_(complex_e2e|axi_lite_verilator_e2e)" --output-on-failure` RV32I 与 AXI4-Lite 真跑（非 SKIP）
- `ctest -L verilator -R perf_three_way` 全过
- `openspec validate verilator-hier-input-flattening-fix --strict` valid
- `openspec validate --specs --strict` 12/12 passed（+1 新 capability）
- `openspec archive verilator-hier-input-flattening-fix` 成功