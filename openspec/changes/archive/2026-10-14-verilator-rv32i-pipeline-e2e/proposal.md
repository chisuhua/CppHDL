# Proposal: verilator-rv32i-pipeline-e2e

> 实现 RV32I 5-stage 流水线在 VerilatorBackend 下的 e2e 真仿真，移除 strengthen-verilator-e2e-coverage 留下的 D8 (b) descope SKIP。

## Why

`strengthen-verilator-e2e-coverage` change (2026-10-14 archive) 与 `verilator-reset-initial-value-mismatch` change (2026-10-14 archive) 之后，verilator e2e 真仿真已覆盖：

- 6 类 fixture（counter / bundle / stream / sync_fifo / axi4_full / spi_fsm）+ trace parity 3 fixture
- ch_reg 任意初值（包括非零 init）

**仍剩余 1 类代表性设计未做真仿真**：RV32I 5-stage 流水线（IF/ID/EX/MEM/WB + forwarding + hazard detection）。这是 35 种设计中复杂度最高、最能体现 verilator 后端对**多 cycle sequential logic + 复杂 mux tree + ch_mem `sread`** 支持的代表性 fixture。

`tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` 当前是 documented SKIP（`verilator-rv32i-pipeline-e2e` follow-up）。本次 change 实现这个 follow-up：
- 编写 RV32I 短指令序列 fixture（3 条无 hazard 指令：`addi x1, x0, 1; addi x2, x0, 2; add x3, x1, x2`），interpreter 与 verilator 双跑，对比 `perf_instructions` ch_register 终值 == 3
- 移除 documented SKIP，**真实测试向量**

**RV32I 是 35 种 verilator e2e 设计清单中最具代表性**的：
- 完整 5 阶段流水线（IF/ID/EX/MEM/WB）覆盖 forwarding / hazard detection / 分支 flush
- 含 `ch_mem` sread 端口（用于 imem）——与 `aread` 不同，sread 是 posedge 对齐，parity 安全（见 verilator-e2e-coverage spec）
- 含大量 always_ff reg 链（IF/ID、ID/EX、EX/MEM、MEM/WB 流水线寄存器）
- 已有 `perf_instructions` / `perf_cycles` 等顶层 ch_out 端口（verilator 可观测）

## What Changes

- **Modify** `tests/test_verilator_complex_e2e.cpp`：把 `ComplexE2E - Rv32iShortInstructionTraceParity` 的 documented SKIP 改回真实 trace parity 实现。fixture 包含：
    - `ch::ch_module<riscv::Rv32iPipeline>` 内嵌在 Component
    - imem 用 `ch_mem` + 3 条 RV32I 指令（`addi x1, x0, 1; addi x2, x0, 2; add x3, x1, x2`）
    - dmem 简化 stub（write=0, read_data=0）
    - 观测 `perf_instructions` ch_out 终值 == 3（interpreter == verilator）
- **Add** capability `verilator-rv32i-e2e` under `openspec/specs/verilator-rv32i-e2e/spec.md`：定义 RV32I 流水线在 verilator 仿真下的行为契约（fixture / 观测点 / 时序假设）。
- **Preserve** `tests/test_verilator_complex_e2e.cpp::ComplexE2E - SpiFsmStateTransitionTraceParity` 不动（已实跑通过）。
- **Preserve** 现有所有真仿真测试不动。

## 非目标

- **NG1**：不修改 `VerilatorBackend` 实现（verilator-reset-initial-value-mismatch change 已修过；本次 change 仅测试 + spec）。
- **NG2**：不引入新依赖。
- **NG3**：不覆盖 load-use hazard / branch flush / 数据冒险 / 控制冒险 —— 这些场景已在 `tests/test_rv32i_pipeline.cpp` + `tests/test_hazard.cpp` + `tests/chlib/test_forwarding.cpp` 的 interpreter 层覆盖（cleanup-orphan-code 把 `tests/test_forwarding.cpp` 根级副本删除后，canonical 路径在 `tests/chlib/`）。本 fixture 只覆盖 EX→EX / MEM→WB forwarding 的 3 条无 hazard 短指令序列，目标是 backend parity 而非 Python correctness。
- **NG4**：不实现 full RV32I SoC（含 clint / csr / exception handler）—— 见 `examples/riscv-mini/` 的相关示例，本 change scope 仅限 `Rv32iPipeline` 顶层 + 简化 imem/dmem stub。
- **NG5**：不优化 verilator 编译时间 —— RV32I 命令数 > 200（`include/cpu/` 全部 .h），冷编译可能 1-3 min；TIMEOUT 600s 已足够 warm cache 跑，cold cache 可能接近 TIMEOUT 边界但不会超（实测见 tasks.md 5.1）。

## 验证

- `cmake --build build -j$(nproc)` 0 errors, 0 warnings on changed files
- `ctest -E "perf_tests|perf_three_way" -j$(nproc)` 全过（158 个 + alias 159 个）
- `ctest -R "test_verilator_complex_e2e" --output-on-failure` 至少 2 个 TEST_CASE 实际执行（SpiFsm + Rv32I 各 1 个）
- `ctest -L verilator -R perf_three_way` 全过（verilator 三路子集不变）
- `openspec validate verilator-rv32i-pipeline-e2e --strict` valid
- `openspec validate --specs --strict` 11/11 passed（+1 新 capability `verilator-rv32i-e2e`）
- `openspec archive verilator-rv32i-pipeline-e2e` 成功