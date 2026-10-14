# Design: verilator-hier-input-flattening-fix

## Context

`verilator-rv32i-pipeline-e2e` change (2026-10-14 archive) 留下 1 个 D8 (b) descope SKIP：`tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` documented SKIP，归因为"verilator M0.5 dispatch 不传播 always_ff 寄存器"。

Oracle 深入调查（2026-10-14 后续诊断）重新定位根因：`codegen_verilog.cpp` 把"带内部 driver 的子模块 `ch_in` 端口"无差别 emit 为顶层 `input` port，但 codegen 没有给这些 input emit `assign` 来把 driver 拉过来。verilator backend 把 `data_map_` 写到 Vtop 字段，但子模块 input 的 driver 关系在 data_map_ 里没有值（verilator 短路解释器，driver 传播 instr 不执行；`data_map_` 对子模块 input 永远是 0），子模块 input 永远 = 0。

**issue #25 的真实历史**：
- 2026-09-22 commit `e19230c` + `9ab7cb5` 修了 2-step 时钟翻转（缺这层时 always_ff 永不触发）
- 残余症状（AXI-Lite rvalid = 0, RV32I perf = 0）**被误诊为"寄存器传播"**，fixture 改 SKIP
- 本 change 重新定位 + 真正修复（"层次化输入平铺"）

### 受影响 fixture

| Fixture | 之前症状 | 修复后 |
|---------|---------|--------|
| `test_verilator_e2e_harness.cpp::E2E CounterSimulatorPortAccess` 等 | ✓ 通过（无 `ch_module` 嵌套） | ✓ 仍通过 |
| `test_verilator_three_way.cpp`（已 rename 为 test_codegen_literal_mask）| ✓ 通过（无 ch_module） | ✓ 仍通过 |
| `test_verilator_e2e_harness.cpp::SamplesCounter4Bit50CyclesMod16` | ✓ 通过 | ✓ 仍通过 |
| `test_verilator_e2e_harness.cpp::RealFiftyCycleCounterSimulator` | ✓ 通过 | ✓ 仍通过 |
| `test_verilator_complex_e2e.cpp::SpiFsmStateTransitionTraceParity` | ✓ 通过（fixture 无 ch_module 子输入驱动） | ✓ 仍通过 |
| `test_verilator_complex_e2e.cpp::Rv32iShortInstructionTraceParity` | ❌ documented SKIP（perf=0 vs interp=12） | ✅ 真实跑通 perf=3 |
| `test_axi_lite_verilator_e2e.cpp::WriteReadReg0ThroughBackend` | ⚠️ 软断言 `rvalid == 0`（被改写为验证 bug 行为） | ✅ 真实断言 `rdata == 0xDEADBEEF` |
| `test_verilator_e2e_harness.cpp::LibVtopSoOnDisk` | ✓ 通过 | ✓ 仍通过 |

## Goals / Non-Goals

详见 `proposal.md`。

## Decisions

### D1：filter 掉有 driver 的 input 节点从 print_header

- 决策：`print_header` (L290-374) 跳过 `inputimpl::driver() != nullptr` 的节点；改在 `print_decl` (L397-473, type_input case L412-415) 把它们 emit 为内部 `logic`（复用 `emit_signal_decl` L257-268）
- 实现（伪代码，oracle 建议的 patch sketch）：
  ```cpp
  // print_header input 追加处 (L336-340):
  for (auto *port_node : ports) {
      if (port_node->type() == ch::core::lnodetype::type_input) {
          // Driven submodule inputs are internal logic, not top ports.
          if (static_cast<ch::core::inputimpl *>(port_node)->driver() == nullptr)
              filtered_ports.push_back(port_node);
      }
  }

  // print_decl type_input case (L412-415):
  case ch::core::lnodetype::type_input:
      if (static_cast<ch::core::inputimpl *>(node)->driver() != nullptr)
          emit_signal_decl(out, node);  // internal `logic`, was top port
      inputs.push_back(node);
      declared_nodes_.insert(node);
      break;
  ```
- 理由：
  - input port 的真正功能是 testbench 驱动；当 fixture 内部 driver 提供了值时，input 不应再出现
  - JIT 端已经做了相同处理（`src/jit/jit_ir_gen_lnode_io.cpp:31-46`，AGENTS.md "JIT RULES" 显式条款），修复对齐两后端语义

### D2：print_logic 新增 type_input 分支

- 决策：与 `type_output` 分支对称 —— 从 input node 的 `driver()` 走 proxy 链 walk 找到真正 driver，emit `assign <in_name> = <driver_name>;`
- 实现（伪代码，oracle 建议的 patch sketch）：
  ```cpp
  // print_logic 新 case (插到 L531 type_output 前):
  case ch::core::lnodetype::type_input: {
      auto *in = static_cast<ch::core::inputimpl *>(node);
      ch::core::lnodeimpl *drv = in->driver();
      // Walk only UNNAMED proxies; named wires are declared+assigned already.
      while (drv && drv->type() == ch::core::lnodetype::type_proxy &&
             !node_names_.count(drv) && drv->num_srcs() > 0)
          drv = drv->src(0);
      if (drv && node_names_.count(drv) && node_names_.count(node))
          out << "    assign " << node_names_[node] << " = "
              << node_names_[drv] << ";\n";
      printed_logic_nodes_.insert(node);
      break;
  }
  ```
- 关键：**无** `driver_name[0] != '_'` 检查（不复制 NG3 bug —— `type_output` 分支 L551 的静默丢弃不应在本 change 传染给 input 路径）
- 理由：driver 链 walk 条件设为"proxy 且无名字"，有名字就直接引用 —— 对 RV32I 的 `instr_data <<= word_sel`（named proxy → mem read data proxy，两者都已声明）和 AXI 的 input-to-input（driver 是顶层 port）都成立

### D3：backend 同步跳过被驱动的 input

- 决策：`emit_sim_main_postlude` (L219-280) 与 `build_port_access_table` (L589-627) 同步过滤"被驱动的 input"——它们不再是顶层 port，不应在 `set_input_Vtop` / `get_field_ptr_Vtop` 的 switch 中出现
- 理由：若不跳过，generated `case <port_id>: <vname> = bits; break;` 会引用不存在的 Vtop 字段，verilator 编译失败
- 实现：
  ```cpp
  for (auto& [id, pa] : port_access_) {
      if (pa.is_input && is_input_with_driver(id)) continue;
      // existing case generation
  }
  ```
- 副作用：被驱动的 input 不再被 `sync_inputs_to_vtop` 写入（因为不在 port_access_ 里）。这正是我们想要的——testbench 不能、也不应该再驱动这些 input

### D4：恢复 AXI4-Lite 真实断言

- 决策：`test_axi_lite_verilator_e2e.cpp:276` 把 `REQUIRE(rvalid_val == 0)` 改为 `REQUIRE(rvalid_val == 1)` + 加 `REQUIRE(rdata_val == 0xDEADBEEF)`
- 理由：原断言是 bug 行为写进测试 —— `rvalid = 0` 是 verilator 输入平铺 bug 的症状，不是 fixture 期望。修复后 `rvalid` 应为 1（write transaction 完成），`rdata` 应为 0xDEADBEEF（之前写入的值）
- 风险：可能还有其他 issue #25 残留（`rdata` 由 always_ff 寄存器驱动，依赖 M0.5 dispatch 正确触发）。**缓解**：D1+D2 修复后，issue #25 的真正症状都消失，always_ff 正确触发，`rdata` 应为 0xDEADBEEF

### D5：删除 RV32I documented SKIP

- 决策：`test_verilator_complex_e2e.cpp:374-381` 删除 `if (perf_a_val != perf_b_val) { SKIP(...); }` 块，改回 `REQUIRE(perf_a_val == perf_b_val); REQUIRE(perf_a_val == 12);`
- 终值 12 而非 3 的原因：fixture imem 4-word 程序循环回绕（`bits<3,2>(cpu.io().instr_addr)` 模 4），3 条 addi/add 指令每条 commit_valid 持续累加；`rv32i_pipeline.h:221` `commit_count->next = commit_count + commit_valid` 每拍无条件累加；20 cycle 中流水线填充 (~5 cycle) 后 commit_count 每 cycle +1，cycle 20 时约累加到 12。`3` 是 `commit_count` 首次被静态读的误解，实际跑 20 cycle 后稳定在 12
- 理由：D1+D4 修复后 RV32I fixture 真跑通；期望值 12（不是 3）已与 fixture 注释 (`test_verilator_complex_e2e.cpp:362-369` "Interpreter produces the correct value (12)") 一致
- 风险：`perf_instructions` 由 `commit_count` 驱动，commit_count 依赖 EX/MEM/WB 流水线寄存器 + hazard unit 组合 + EX 阶段 alu 累加。如果 D1+D4 不完全修复所有 issue #25 残余，RV32I 测试可能仍失败

### D6：spec invariant 5 放宽

- 决策：`openspec/specs/verilator-e2e-coverage/spec.md` invariant 5 "parity fixture MUST NOT 依赖 always_ff 寄存器传播" 改为"parity fixture 的观测点 MUST 为顶层 ch_out 端口；always_ff 寄存器由 Verilator 的 always_ff @posedge clk 自动触发，寄存器驱动的 ch_out 是合法的观测点"
- 理由：本 change 修复后 always_ff 寄存器驱动的 ch_out 在两后端都正确传播

### D7：AGENTS.md issue #25 重新描述

- 决策：AGENTS.md "VERILATOR BACKEND RULES" 中 issue #25 描述从"always_ff 寄存器可能不传播"更正为"层次化设计的子模块 input port 必须有 driver（codegen 必须 emit assign）；否则该 input 永远是 X"
- 理由：之前误诊导致 fixture 写 SKIP，修复后术语应反映真正根因

## Risks / Trade-offs

| 风险 | 等级 | 缓解 |
|------|------|------|
| print_header 改动影响所有 fixture 编译 | **高** | regression gate：159/159 ctest 必过；verilator 缓存自动 invalidate（verilog 内容变 → cache key 变） |
| driver 链 walk 在 type_input 上有特殊 case（如 ch_in 绑给 proxy） | 中 | 与 type_output 分支共用 walk_proxy_chain，已被实战验证；新增 ~10 行代码 |
| emit_sim_main_postlude 跳过被驱动 input，可能影响 verilator 编译 | 低 | port_access_ 谓词同步跳过，case 数量减少；verilator 编译应更快 |
| AXI4-Lite rdata 真断言可能仍失败（其他未发现 issue） | 中 | 阶段 2 GREEN 跑测试时实测；若仍失败 → D8 (b) descope 单个 assertion，**不阻塞 archive** |
| RV32I perf 真断言可能仍失败（issue #25 真正残余） | 中 | 同上；fixture 已 SKIP，能容纳失败 |
| codegen_verilog.cpp 改动面广（header + decl + logic + backend）| 中 | 4 个独立 commit（per D7 AGENTS.md "commit 频繁"） |
| cache key 变化导致 perf_regression baseline 失效 | **不适用** | 扁平设计（Counter / SpiFsm / perf TC-07/08/09）生成的 verilog 和 sim_main 逐字节不变（accept lambda 只在存在 driven input 时才改变输出）→ cache key 不变 → 零重编译。**`tests/benchmark/perf_baseline.json` 不需要重基线**（perf TC 全是扁平设计，verilator 编译时间不在 perf 计量内）。只有层次化 verilator fixture（RV32I e2e、AXI4-Lite TC1/TC2）首次各吃一次冷编译（~30-90s），之后回暖 |

## 验证矩阵

| 验证项 | 命令 | 期望结果 |
|--------|------|----------|
| 编译 | `cmake --build build -j$(nproc)` | exit 0 |
| 现有真仿真不退化 | `ctest -E "perf_tests|perf_three_way"` | 158 + alias 全过 |
| RV32I 真实跑通 | `ctest -R test_verilator_complex_e2e` | 2/2 实际执行，RV32I perf=3 |
| AXI4-Lite rdata 真断言 | `ctest -R test_axi_lite_verilator_e2e` | 2/2 通过（含 rdata==0xDEADBEEF）|
| verilator 三路 perf 不退化 | `ctest -L verilator -R perf_three_way` | 全过 |
| openspec validate change | `openspec validate verilator-hier-input-flattening-fix --strict` | valid |
| openspec validate specs | `openspec validate --specs --strict` | 12/12 |

## TDD 4 阶段骨架（详细在 tasks.md）

### 阶段 1 — RED：确认根因
1. oracle 已完成
2. 手动检查 `/tmp/cpphdl_vl_cx_0ZkT40_cx_rv32i/top.v` 确认"input declared but never driven" pattern

### 阶段 2 — GREEN：codegen + backend 4 处修改
3. print_header filter
4. print_decl logic 声明
5. print_logic type_input 分支
6. emit_sim_main_postlude 跳过被驱动 input
7. build_port_access_table 同步过滤
8. 跑 ctest -R "test_verilator" 验证 0 回归

### 阶段 3 — REFACTOR：fixture 恢复 + spec + AGENTS.md
9. test_axi_lite_verilator_e2e.cpp:276 恢复真断言
10. test_verilator_complex_e2e.cpp:374-381 删除 SKIP
11. openspec/specs/verilator-e2e-coverage/spec.md invariant 5 放宽
12. AGENTS.md issue #25 重述

### 阶段 4 — VERIFY：全量 ctest + archive
1. `ctest -E "perf_tests|perf_three_way"` 159 + alias 全过
2. `openspec validate` 12/12
3. `openspec archive` 成功

## 关联 ADR 引用

- **ADR-035 §M5 e2e tier**："VerilatorBackend 能跑出正确结果"为真仿真基础。本 change 是 §M5 的层次化设计覆盖完善
- **ADR-035 §3.5** SHA-1 缓存：verilog 内容变化 → cache key 变化 → 旧缓存失效（这是设计正确行为，AGENTS.md §3.5 记录）
- **verilator-e2e-coverage spec**：本 change 在其上**放宽 invariant 5**（从"MUST NOT 依赖寄存器传播"变为"always_ff 寄存器由 Verilator 自动触发，寄存器驱动的 ch_out 是合法观测点"）
- **verilator-reg-init spec**（刚 archive）：层次无关，单独修复 ch_reg init 加载
- **JIT RULES** (`include/jit/jit_ir_gen_lnode_io.cpp:31-46`)：JIT 端早已正确处理 input driver，verilog codegen 路径补齐对应规则

## 关联基线

- **verilator-rv32i-pipeline-e2e** change (archive 2026-10-14) —— D8 (b) descope 起源
- **verilator-reset-initial-value-mismatch** change (archive 2026-10-14) —— 提供 t=0 reset pulse
- **issue #25 原始 PR** (commit `e19230c` + `9ab7cb5`) —— 修了 2-step 时钟翻转；残余症状即本次 fix 目标