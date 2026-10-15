# verilator-hier-input-flattening-fix

> **STATUS (2026-10-14)**: 起草中。4 阶段 TDD 结构。
>
> **Phase 依赖**：阶段 1 独立（oracle 已完成 RED 验证）；阶段 2 依赖阶段 1（GREEN：4 处 codegen + backend 修改）；阶段 3 依赖阶段 2（REFACTOR：fixture 恢复真断言 + spec + AGENTS.md）；阶段 4 = verify gate + archive。

## 阶段 1 — RED: 确认根因

- [x] 1.1 检查 `/tmp/cpphdl_vl_cx_*_cx_rv32i/top.v` 与 `/tmp/cpphdl_axi_e2e_*/top.v`：确认存在悬空顶层 input port（`top_unnamed_input`、`top_if_stage_unnamed_input_3` 等）但**无 `assign` 驱动**这些端口
      ✅ Done — `top_if_stage_unnamed_input_5`、`top_cpu_unnamed_input_4/5` 等顶层 input port 在 fix 前无 driver；interp/vl perf 差 12 vs 0。
- [x] 1.2 记录到 `verilator-hier-input-flattening-fix/design.md` Bug H1 段
      ✅ Done — root cause 已记录在 proposal.md / design.md D1/D2。

## 阶段 2 — GREEN: codegen + backend 4 处修改

- [x] 2.1 `src/codegen_verilog.cpp::print_header` filter 掉 `inputimpl::driver() != nullptr` 的节点
      ✅ Done — print_header input case 仅当 `in->driver() == nullptr` 时加入 filtered_ports。
- [x] 2.2 `src/codegen_verilog.cpp::print_decl` 把被驱动的 input 声明为 `logic` wire（走 wires 列表，复用 `emit_signal_decl`）
      ✅ Done — print_decl type_input case：被驱动的 input 走 `wires.push_back(node)`，再 emit `logic`。
- [x] 2.3 `src/codegen_verilog.cpp::print_logic` 新增 `type_input` 分支，emit `assign <in> = <driver>;`
      ✅ Done — print_logic type_input case：walk unnamed proxy chain emit `assign <node_name[in]> = <node_name[driver]>;`。
- [x] 2.4 `src/core/verilator_backend.cpp::emit_sim_main_postlude` 跳过被驱动的 input
      ✅ Done — `is_driven_input` 谓词同步跳过 type_input cases（set_input_Vtop / get_field_ptr_Vtop / get_output_Vtop）。
- [x] 2.5 `src/core/verilator_backend.cpp::build_port_access_table` 同步过滤被驱动的 input
      ✅ Done — `t == type_input` 且 `in->driver() != nullptr` 时 `continue`，不加入 port_access_。
- [x] 2.6 跑 `cmake --build build -j$(nproc)` —— 0 errors, 0 warnings on changed files
      ✅ Done — 编译通过，warning 数从 base 19 降至 18（新增 type_input 分支消除了一个 L486 的 type_input 警告）。
- [x] 2.7 跑 `ctest -E "perf_tests|perf_three_way" -j$(nproc)` —— 159 个 + alias 全过（**注**：RV32I / AXI4-Lite 仍 SKIP / 软断言，本阶段不恢复）
      ✅ Done — 159/159 ctest 全过。RV32I fixture 仍走 SKIP（在 3.2 修复）；AXI4-Lite 仍 REQUIRE rvalid=0（在 3.1 修复）。
- [x] 2.8 跑 `ctest -L verilator -R perf_three_way` 全过（verilator 三路 perf 仍正常）
      ✅ Done — perf_three_way 95s 通过，三路 perf 无回归。
- [ ] 2.9 **commit 1**：`fix(verilog-codegen): don't flatten ch_in with internal driver to top port + emit assign in print_logic`

## 阶段 3 — REFACTOR: fixture 恢复 + spec + AGENTS.md

- [x] 3.1 `tests/test_axi_lite_verilator_e2e.cpp` 改为 `REQUIRE(rvalid_val == 1)` + 加 `REQUIRE(rdata_val == 0xDEADBEEF)`
      ✅ Done — Walk several read cycles while arvalid stays high (兼容 verilator 2-step 时钟翻转额外延迟)；rvalid=1, rdata=0xDEADBEEF 真跑通过。
- [x] 3.2 `tests/test_verilator_complex_e2e.cpp` 删除 D8(b) SKIP 块，改 `REQUIRE(perf_a_val == perf_b_val); REQUIRE(perf_a_val == 12);`
      ✅ Done — 移除 SKIP；跑 20 cycle 后 interp/vl 都是 12；REQUIRE 通过。
- [x] 3.3 跑 `ctest -R "test_verilator_(complex_e2e|axi_lite_verilator_e2e)"` —— RV32I 真跑通 + AXI4-Lite rdata 真断言通过
      ✅ Done — RV32I perf=12 通过；AXI4-Lite rvalid=1 + rdata=0xDEADBEEF 通过。
- [x] 3.4 如果 3.3 任一失败：回到阶段 2 修正 codegen/backend 代码
      ✅ 不需要 — 阶段 3.1 + 3.2 一次通过。
- [x] 3.5 `openspec/specs/verilator-e2e-coverage/spec.md` invariant 5 放宽
      ✅ Done — change 的 specs/verilator-e2e-coverage/spec.md 已加 MODIFIED Requirements；archive 时合并到 openspec/specs/verilator-e2e-coverage/spec.md。
- [x] 3.6 `AGENTS.md` "VERILATOR BACKEND RULES" 增加层次化 input driver 规则
      ✅ Done — 新增 bullet："Hierarchical input driver MUST be emitted by codegen"。
- [ ] 3.7 **commit 2**：`test(verilator): restore axi4lite rdata real assertion + rv32i perf_instructions=12 (no D8(b) SKIP) + spec/AGENTS.md fixup`

## 阶段 4 — VERIFY: 全量 ctest + openspec validate + archive

- [x] 4.1 `cmake --build build -j$(nproc)` 0 errors, 0 warnings on changed files
      ✅ Done — 0 errors，warning 数 18（base 19，新增 type_input 分支消除 1 个 -Wswitch-enum）。
- [x] 4.2 `ctest -E "perf_tests|perf_three_way" -j$(nproc)` 159 + alias 全过
      ✅ Done — 159/159 ctest 全过（0 失败）。
- [x] 4.3 `ctest -R "test_verilator_*"` 至少 14 个 TEST_CASE 全部实际执行
      ✅ Done — 9 verilator test binary 通过，所有 verilator test 全过（counter / axi_lite / rv32i / spi_fsm / bundle / stream / memory 等）。
- [x] 4.4 `ctest -L verilator -R perf_three_way` 全过
      ✅ Done — perf_three_way 95s 通过。
- [x] 4.5 `openspec validate verilator-hier-input-flattening-fix --strict` 通过
      ✅ Done — Change 'verilator-hier-input-flattening-fix' is valid.
- [x] 4.6 `openspec validate --specs --strict` 11/11 passed（archive 前 11；archive 后 +1 verilator-hier-input = 12）
      ✅ Done — 当前 11/11 passed；archive 后新 capability verilator-hier-input 加入 specs/，expect 12/12。
- [ ] 4.7 `openspec archive verilator-hier-input-flattening-fix` 成功

## 关联基线

- **verilator-rv32i-pipeline-e2e** change (archive 2026-10-14) —— D8 (b) descope 起源，本 change 解除
- **verilator-reset-initial-value-mismatch** change (archive 2026-10-14) —— 提供 t=0 reset pulse，本 change 受益
- **issue #25 原始 PR** (commit `e19230c` + `9ab7cb5`, 2026-09-22) —— 修了 2-step 时钟翻转；残余症状即本次 fix 目标
- **JIT RULES** (`include/jit/jit_ir_gen_lnode_io.cpp:31-46`) —— JIT 端已正确处理 input driver
- **verilator-e2e-coverage spec** —— 本 change 在其上放宽 invariant 5
