# Proposal: verilator-reset-initial-value-mismatch

> 修 VerilatorBackend 在 `ch_reg` 非零初值场景下的真仿真错误：interpreter 跑出正确值，verilator 跑出 0 (Verilog 上电零) + add-addend 值。

## Why

`strengthen-verilator-e2e-coverage` change (2026-10-14 archive) 实施中发现**真实的 verilator 后端 bug**——后续通过 oracle 调查定位 (`openspec/changes/strengthen-verilator-e2e-coverage/design.md` D8 (b) 触发)：

| 场景 | interpreter | verilator | 差异 |
|------|-------------|-----------|------|
| `ch_reg<ch_uint<8>>(0_d); r->next = r + 1_d;` (tick 1) | 0 | 0 | 0 (巧合) |
| `ch_reg<ch_uint<8>>(10_d); r->next = r + 2_d;` (tick 1) | 12 | 2 | **10** |
| `ch_reg<ch_uint<8>>(20_d); r->next = r + 3_d;` (tick 1) | 23 | 3 | **20** |

`regression_bug` 来源（oracle 调查结论）：

1. **真因**：`VerilatorBackend::try_initialize()` (`src/core/verilator_backend.cpp:365-373`) 只把 `default_reset` 强制置低（保持 0)，**从不脉冲它**。`VerilatorBackend::reset()` (`src/core/verilator_backend.cpp:791-813`) 有正确的 1→eval→0 脉冲，但 `Simulator::tick()` 与 `Simulator::set_backend()` **从不调用** `reset()`。Verilog 的 `always_ff @(posedge clk or posedge default_reset)` 块在 `@(posedge default_reset)` 沿（即 reset 0→1 边沿）才加载 INIT 值——verilator 从未收到这个沿，所以寄存器的初值永远是 verilator 上电零（默认 Vregulator 初始化）。
2. **伴随问题**：`print_reg` (`src/codegen_verilog.cpp:604-610`) 直接 `get_literal_str(lit->value())`，**字面量宽度 = init 字面量固有宽度**（如 `ch_reg<ch_uint<8>>(10_d)` 用 `10_d` 字面量宽 4 bit)，不是 reg 宽（8 bit）。这造成复位分支写 `reg <= 4'ha`（`8'h0a` Verilog 零扩展 → 值正确，但字面量窄于 reg 宽，verilator lint 可能 warn）。

**当前 workaround** (`tests/test_verilator_trace_parity.cpp:227-237` 注释明说为绕开 D8 (b) 全用 `0_d` 初始值) — 这限制了 fixture 设计空间，所有用 `reg(0x10_d)` / `reg(10_d)` 这类初值非零的 HDL pattern 都无法在 trace parity 单测中验证。

**根因**（ADR-035 §M5 e2e tier 没有定义 "reg 初值" 加载语义）：`VerilatorBackend` 在初始化时缺一步"复位脉冲"。interpreter 侧通过 `Regimpl::create_instruction` 在建指令时直接 `assign_truncate(*init_buf)`（`src/ast/ast_nodes.cpp:38-41`）天生带初值。

## What Changes

- **Modify** `src/core/verilator_backend.cpp::try_initialize()`：在现有 `*reset_field_ptr_ = 0` 块（lines 365-373）后追加一次**复位脉冲**（1→eval→0→eval），让 `@(posedge default_reset)` 沿触发，加载 ch_reg 的初值。复用 `VerilatorBackend::reset()` 的同款序列。
- **Modify** `src/codegen_verilog.cpp::print_reg()`（lines 604-610）：把 `reset_value = get_literal_str(lit->value())` 改为先用 `sdata_type(static_cast<uint64_t>(lit->value()), node->size())` 规范化到 reg 宽，再 `get_literal_str`。消除 `4'ha`/`5'h14` 这种窄宽复位字面量，让 Verilog 字面量形态稳定为 `<reg_width>'h<value>`。
- **Modify** `src/codegen_verilog.cpp::print_reg()`：对 `init_node` 非 `type_lit` 的分支加 `CHWARN`（当前静默回落到 `1'b0`，掩盖 simulator 一侧的 init 路径）。
- **Add** capability `verilator-reg-init` under `openspec/specs/verilator-reg-init/spec.md`：定义 reg init 在 verilator 仿真下的行为契约（reset 脉冲时序 + 字面量宽度规范化）。
- **Modify** `tests/test_verilator_trace_parity.cpp`：移除 fixture `reg(0_d)` workaround（lines 227-237 注释，reg_a/reg_b/reg_c 都改回 0/10/20 起始值），让 trace parity 在非零初值下也成立。
- **Modify** `tests/test_verilator_bundle_e2e.cpp`：移除 `BundleAssignmentChain` fixture 的 `ch_reg(0_d)` workaround，验证 reg 非零初值下 `A <<= B; B <<= C; C.a = reg` 链的 trace parity。
- **Preserve** 现有 5 个真仿真测试（`test_verilator_e2e_harness.cpp` + `test_axi_lite_verilator_e2e.cpp` + `test_verilator_backend.cpp::E2E CounterFiftyCycles` 等）不动；这些全部基于 `ch_reg(0_d)` 或 `ch_reg(ch_uint<N>(0))`，regression gate 自动覆盖。
- **Preserve** `tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` 的 documented SKIP 不动——RV32I 仿真由 follow-up `verilator-rv32i-pipeline-e2e` 单独负责。

## 目标

- **G1**：`VerilatorBackend::try_initialize()` 完成后，Vtop 内部所有 `ch_reg(N, INIT)` 在 tick(0) 时等于 INIT（与 interpreter 一致），不再依赖 Verilator 上电零。
- **G2**：`tests/test_verilator_trace_parity.cpp::TraceParity - BundleMultiFieldTwentyCycles` 在 `reg(0_d/10_d/20_d)` 三个寄存器非零起始值下，逐周期 parity 成立。
- **G3**：`tests/test_verilator_bundle_e2e.cpp::BundleE2E - BundleAssignmentChain` 在 `ch_reg<ch_uint<8>>(0_d)` + `next = reg + 1_d` 下，5 cycle 后 io().out == 5（interpreter 与 verilator 一致）。这其实是 G2 的子集，重点是确认 `ch_reg <<= 1` 加法链 trace parity 不被打扰。
- **G4**：所有现有 `ctest` 0 回归（153 baseline + 5 新 test = 158 个 + 旧名 alias 159）。
- **G5**：`openspec validate --specs --strict` 通过（含新 `verilator-reg-init` capability）。

## 非目标

- **NG1**：不修改 `Simulator::tick()` / `Simulator::reset()` 委托路径。本 change 只修 backend 在 `initialize()` 时刻的 reset 脉冲，不引入运行时 reset 路径（运行时 reset 行为由未来 change 负责）。
- **NG2**：不修改 interpreter 侧 reset 行为。interpreter 的 `instr_reg::eval` 在 rst 断言且 `rst_val_buf_ == nullptr` 时保持原值（`src/ast/instr_reg.cpp:33-39`）——这是 interpreter 的语义，与 Verilog 不等价，但本 change 不动 interpreter。
- **NG3**：不为内部 `ch_reg` 加 dlopen 可观测字段（违反 ADR-035 §M6 "只观测顶层 port" 约束）。
- **NG4**：不实现 RV32I pipeline e2e 真仿真（由 follow-up `verilator-rv32i-pipeline-e2e` 负责）。
- **NG5**：不引入 `initial begin` SystemVerilog 块作为第二条 init 路径（会增加 verilator PROCASSINIT 告警处理负担，单一事实源：reset 脉冲路径已足够）。

## 验证

- `cmake --build build -j$(nproc)` 0 errors, 0 warnings on changed files
- `ctest -E "perf_tests|perf_three_way" -j$(nproc)` 全过（158 个 + 旧名 alias 159 个）
- `ctest -R "test_verilator_trace_parity|test_verilator_bundle_e2e"` 至少 7 个 TEST_CASE 全部实际执行（非 SKIP），逐周期 parity 成立
- `ctest -L verilator -R perf_three_way` 全过（verilator 三路子集不变）
- `openspec validate verilator-reset-initial-value-mismatch --strict` valid
- `openspec validate --specs --strict` 10/10 passed（+1 新 capability `verilator-reg-init`）
- `openspec archive verilator-reset-initial-value-mismatch` 成功