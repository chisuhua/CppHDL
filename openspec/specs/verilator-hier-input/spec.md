# verilator-hier-input Specification

## Purpose
TBD - created by archiving change verilator-hier-input-flattening-fix. Update Purpose after archive.
## Requirements
### Requirement: VerilatorBackend MUST 不把有 driver 的 ch_in 展平为顶层 port

`codegen_verilog.cpp::print_header()` MUST filter out `inputimpl` 节点（`type_input`）的 lnodes whose `driver() != nullptr`（等价 `num_srcs() > 0`）—— 这些 input 在 Verilog 中 MUST NOT 出现为顶层 `input` port，而 MUST 声明为内部 `logic` wire；其值由对应的 `assign <name> = <driver_name>;` 语句提供（driver name 走 proxy chain walk，类似 `type_output` 分支逻辑）。`emit_sim_main_postlude` 与 `build_port_access_table` MUST 同步过滤这些 input，否则 `set_input_Vtop` / `get_field_ptr_Vtop` switch case 会引用不存在的 Vtop 字段，verilator 编译失败。

#### Scenario: 顶层 ch_in + ch_module 子输入 + ch_reg + 顶层 ch_out 层次化设计

- **WHEN** fixture 为 `ch_in<ch_uint<32>> a` + `ch_in<ch_bool> b` + `ch::ch_module<SubComp>` + `sub.io().a_in <<= a`（带 driver） + `sub.io().b_in <<= b`（带 driver） + `sub.io().out <<= sub.reg`（reg-driven ch_out）
- **AND** 跑 verilator 仿真
- **THEN** 生成的 Verilog `top.v` 中 `input [31:0] a` 与 `input top_b` **仍存在**（顶层 ch_in 无 driver 保持为 input port），但**子模块输入被降级**：`sub.io().a_in` 不再是顶层 input port 而是 `logic [31:0] sub_io_a_in;`，并 emit `assign sub_io_a_in = a;`（driver 通过 fixture 内部传递）
- **AND** `sub.reg` 的 always_ff @posedge clk 触发后，`sub.io().out`（ch_out 端口）的 Vtop 字段在 `sync_outputs_from_vtop()` 后可被 `data_map_[id]` 读出
- **NOTE**: 这个 invariant 是 verilator-rv32i-pipeline-e2e (2026-10-14) 与 test_axi_lite_verilator_e2e WriteReadReg0 真正缺失的能力，issue #25 的残余症状（"M0.5 dispatch 不传播寄存器"）是被误诊的

#### Scenario: 无 driver 的顶层 ch_in 仍为 input port

- **WHEN** fixture 只有 `ch_in<ch_uint<32>> a`（无内部 driver，靠 testbench `set_input_value` 驱动）
- **THEN** 生成的 Verilog `top.v` 中**存在** `input [31:0] a;`（不 filter），`verilator_backend.cpp::sync_inputs_to_vtop` 正常工作
- **NOTE**: 这与有 driver 的 ch_in 形成对比

#### Scenario: CounterFixture（无 ch_module 嵌套）继续工作

- **WHEN** fixture 为 `ch_reg<ch_uint<32>> reg(0_d); reg->next = reg + 1_d; io().out = reg;`（无任何 ch_module 嵌套）
- **THEN** 修复不引入回归，CounterFiftyCycles / SamplesCounter4Bit50CyclesMod16 仍通过
- **NOTE**: 修复只影响 `ch_module` 嵌套场景的子模块 input；顶层无 driver 的 input 仍为 input port

### Requirement: type_input codegen 必须 emit assign from driver

`codegen_verilog.cpp::print_logic()` MUST 新增 `type_input` 分支：检测 input 节点 `driver() != nullptr` 时，walk proxy chain 找到真正 driver，emit `assign <in_name> = <driver_name>;`。driver chain walk 的实现 MUST 与 `type_output` 分支共用（避免逻辑漂移）。

#### Scenario: ch_in <<= ch_in 跨 ch_module 边界

- **WHEN** fixture 为 `cpu.io().instr_data <<= imem_data`（ch_in 绑 ch_uint，ch_module 内部 input port）
- **AND** 生成 Verilog
- **THEN** 生成的 `top.v` 包含 `assign <cpu_instr_data_name> = <imem_data_name>;`（cpu 的 instr_data 内部 wire 接到 imem_data 节点）

#### Scenario: ch_in <<= ch_bool（ch_bool 字面量）

- **WHEN** fixture 为 `cpu.io().instr_ready <<= ch_bool(true)`（ch_in 绑 ch_bool 字面量，ch_module 内部 input port）
- **AND** 生成 Verilog
- **THEN** 生成的 `top.v` 包含 `assign <cpu_instr_ready_name> = 1'b1;`（driver name walk 到 ch_bool 字面量节点）

#### Scenario: 无 driver 的 ch_in 在 print_logic 中被跳过

- **WHEN** fixture 为 `ch_in<ch_uint<32>> a`（无内部 driver）
- **THEN** `print_logic` 对该节点不 emit `assign`（依赖 testbench 驱动），但端口仍由 `print_header` emit 为 `input`

### Requirement: 修复后 RV32I / AXI4-Lite fixture MUST NOT 走 documented SKIP / 软断言

`tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` MUST NOT 走 D8 (b) documented SKIP after the fix lands；`REQUIRE(perf_a_val == 3)` MUST 在两后端都通过。`tests/test_axi_lite_verilator_e2e.cpp:276` 的 `REQUIRE(rvalid_val == 0)` MUST 改为 `REQUIRE(rvalid_val == 1)` + `REQUIRE(rdata_val == 0xDEADBEEF)`。修复后两个 fixture 都跑真实断言，不再有任何"被改写为验证 bug 行为"的软断言。

#### Scenario: RV32I 真实 trace parity

- **WHEN** fixture 装 3 条短指令 + imem ch_mem sread
- **AND** 跑 20 cycle
- **THEN** interpreter `perf_instructions == 12`（imem 4 word 循环，3 条指令重复执行 4 次）或 == 3（如果 imem 大小调整）；verilator 端**必须**也 == 相同值（不再为 0）
- **AND** `REQUIRE(perf_a_val == perf_b_val)` 通过

#### Scenario: AXI4-Lite rdata 真断言

- **WHEN** fixture 写 0xDEADBEEF 到 reg0，然后读 reg0
- **AND** 跑完整 write+read 握手
- **THEN** `rvalid == 1`（verilator 端握手正常完成，不再因 input 平铺 bug 而恒为 0）
- **AND** `rdata == 0xDEADBEEF`（verilator 端 always_ff reg 正确传播）

