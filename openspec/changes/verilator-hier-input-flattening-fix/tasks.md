# verilator-hier-input-flattening-fix

> **STATUS (2026-10-14)**: 起草中。4 阶段 TDD 结构。
>
> **Phase 依赖**：阶段 1 独立（oracle 已完成 RED 验证）；阶段 2 依赖阶段 1（GREEN：4 处 codegen + backend 修改）；阶段 3 依赖阶段 2（REFACTOR：fixture 恢复真断言 + spec + AGENTS.md）；阶段 4 = verify gate + archive。

## 阶段 1 — RED: 确认根因

- [ ] 1.1 检查 `/tmp/cpphdl_vl_cx_*_cx_rv32i/top.v` 与 `/tmp/cpphdl_axi_e2e_*/top.v`：确认存在悬空顶层 input port（`top_unnamed_input`、`top_if_stage_unnamed_input_3` 等）但**无 `assign` 驱动**这些端口
      ⏳ Pending
- [ ] 1.2 记录到 `verilator-hier-input-flattening-fix/design.md` Bug H1 段
      ⏳ Pending

## 阶段 2 — GREEN: codegen + backend 4 处修改

- [ ] 2.1 `src/codegen_verilog.cpp::print_header` (L307-316) filter 掉 `inputimpl::driver() != nullptr` 的节点
      ⏳ Pending
- [ ] 2.2 `src/codegen_verilog.cpp::print_decl` (L397-473, type_input case L412-415) 把被驱动的 input 声明为 `logic` wire（调用 `emit_signal_decl` L257-268）
      ⏳ Pending
- [ ] 2.3 `src/codegen_verilog.cpp::print_logic` (L486-563) 新增 `type_input` 分支，emit `assign <in> = <driver>;`
      ⏳ Pending
- [ ] 2.4 `src/core/verilator_backend.cpp::emit_sim_main_postlude` (L219-280) 跳过被驱动的 input
      ⏳ Pending
- [ ] 2.5 `src/core/verilator_backend.cpp::build_port_access_table` (L589-627) 同步过滤被驱动的 input
      ⏳ Pending
- [ ] 2.6 跑 `cmake --build build -j$(nproc)` —— 0 errors, 0 warnings on changed files
      ⏳ Pending
- [ ] 2.7 跑 `ctest -E "perf_tests|perf_three_way" -j$(nproc)` —— 159 个 + alias 全过（**注**：RV32I / AXI4-Lite 仍 SKIP / 软断言，本阶段不恢复）
      ⏳ Pending
- [ ] 2.8 跑 `ctest -L verilator -R perf_three_way` 全过（verilator 三路 perf 仍正常）
      ⏳ Pending
- [ ] 2.9 **commit 1**：`fix(verilog-codegen): don't flatten ch_in with internal driver to top port + emit assign in print_logic`

## 阶段 3 — REFACTOR: fixture 恢复 + spec + AGENTS.md

- [ ] 3.1 `tests/test_axi_lite_verilator_e2e.cpp:276` 改为 `REQUIRE(rvalid_val == 1)` + 加 `REQUIRE(rdata_val == 0xDEADBEEF)`
      ⏳ Pending
- [ ] 3.2 `tests/test_verilator_complex_e2e.cpp:374-381` 删除 `if (perf_a_val != perf_b_val) { SKIP(...); }` 块，改回 `REQUIRE(perf_a_val == perf_b_val); REQUIRE(perf_a_val == 12);`（**12 非 3**：imem 4-word 循环回绕，3 条 ALU 指令 commit_valid 持续累加 20 cycle，cycle 20 时 perf_instructions ≈ 12，详见 design D5）
      ⏳ Pending
- [ ] 3.3 跑 `ctest -R "test_verilator_(complex_e2e|axi_lite_verilator_e2e)"` —— RV32I 真跑通 + AXI4-Lite rdata 真断言通过
      ⏳ Pending
- [ ] 3.4 如果 3.3 任一失败：**不**用 D8 (b) descope（这是连续 4 个 change 的第 4 个，D8 (b) 已被过度使用）—— 回到阶段 2 修正 codegen/backed 代码，**必须**让 RV32I perf==12 与 AXI4-Lite rdata==0xDEADBEEF 真实通过后才 archive。本 change 是 issue #25 真正根因修复，不能再 descope
      ⏳ Pending
- [ ] 3.5 `openspec/specs/verilator-e2e-coverage/spec.md` invariant 5 放宽（从"MUST NOT 依赖 always_ff 寄存器传播"改为"always_ff 寄存器由 Verilator 自动触发，寄存器驱动的 ch_out 是合法观测点"）
      ⏳ Pending
- [ ] 3.6 `AGENTS.md` "VERILATOR BACKEND RULES" 把 issue #25 残余症状从"always_ff 寄存器可能不传播"更正为"层次化设计的子模块 input port 必须有 driver（codegen 必须 emit assign）"
      ⏳ Pending
- [ ] 3.7 **commit 2**：`test(verilator): restore axi4lite rdata real assertion + rv32i perf_instructions=3 (no D8(b) SKIP) + spec/AGENTS.md fixup`

## 阶段 4 — VERIFY: 全量 ctest + openspec validate + archive

- [ ] 4.1 `cmake --build build -j$(nproc)` 0 errors, 0 warnings on changed files
      ⏳ Pending
- [ ] 4.2 `ctest -E "perf_tests|perf_three_way" -j$(nproc)` 159 + alias 全过
      ⏳ Pending
- [ ] 4.3 `ctest -R "test_verilator_(trace_parity|bundle_e2e|stream_e2e|memory_e2e|complex_e2e|axi_lite_verilator_e2e)" --output-on-failure` 至少 14 个 TEST_CASE 全部实际执行
      ⏳ Pending
- [ ] 4.4 `ctest -L verilator -R perf_three_way` 全过
      ⏳ Pending
- [ ] 4.5 `openspec validate verilator-hier-input-flattening-fix --strict` 通过
      ⏳ Pending
- [ ] 4.6 `openspec validate --specs --strict` 12/12 passed（含新 capability `verilator-hier-input`）
      ⏳ Pending
- [ ] 4.7 `openspec archive verilator-hier-input-flattening-fix` 成功

## 关联基线

- **verilator-rv32i-pipeline-e2e** change (archive 2026-10-14) —— D8 (b) descope 起源，本 change 解除
- **verilator-reset-initial-value-mismatch** change (archive 2026-10-14) —— 提供 t=0 reset pulse，本 change 受益
- **issue #25 原始 PR** (commit `e19230c` + `9ab7cb5`, 2026-09-22) —— 修了 2-step 时钟翻转；残余症状即本次 fix 目标
- **JIT RULES** (`include/jit/jit_ir_gen_lnode_io.cpp:31-46`) —— JIT 端已正确处理 input driver
- **verilator-e2e-coverage spec** —— 本 change 在其上放宽 invariant 5