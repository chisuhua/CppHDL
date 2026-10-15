# Proposal: strengthen-verilator-e2e-coverage

> 补足 11 个 verilator/verilog 相关单元测试的仿真验证覆盖深度，让真 verilator 仿真（而非字符串断言或 `--lint-only`）成为 codegen 回归的强约束。

## Why

2026-10-13 全量 ctest (154/154 PASS) 后做的覆盖度盘点发现，verilator 相关测试**结构严重不均**：

| 层级 | 测试数 | 真正跑了 verilator 仿真？ |
|------|-------|--------------------------|
| A：真仿真（dlopen + sim.tick + trace 验证） | 6 | ✓ |
| B：codegen 字符串断言 | 40+ | ✗（regex 字符串匹配） |
| C：verilator --lint-only 语法门 | 6 | △（编译通过 ≠ 语义正确） |
| D：纯单元（不依赖 verilator） | 8 | ✗ |

**5 个具体的覆盖盲点**：

1. **没有 trace parity 单测**：现有 6 个真仿真测试都只验证**终值**（`counter_value == 50`），没有逐 cycle 对比 interpreter 与 verilator 的 output trace。一个"verilator 仿真整体偏移 1 cycle"的 bug 不会被任何测试抓到。
2. **真仿真 fixture 单一**：6 个真仿真全部基于 `CounterFixture<N>`（ch_reg + ch_out 最朴素组合），覆盖**0 个** FIFO / Stream / bundle / 状态机 / 流水线 / SRAM。
3. **bundle 嵌套覆盖空白**：SpinalHDL-style bundle 是 CppHDL 的核心卖点，但 verilator 仿真只在最朴素的 ch_out 单字段上验证过。bundle 多字段、嵌套 bundle、bundle `<<=` 赋值在 verilator 后端**完全没仿真**。
4. **名字误导**：`tests/test_verilator_three_way.cpp` 名字带"verilator"，但文件头注释第 17 行明确说"不依赖 verilator 二进制"，实际只做 codegen hex literal 的 regex 检查。这违反 tests/AGENTS.md 的命名约定。
5. **40+ 字符串断言测试无仿真兜底**：`test_verilog_{arith,compare,shift,gen}` 共 40+ 个 TEST_CASE 只断言"verilog 文本里含 `+` / `<<` 等 token"，从未把 verilog 喂进 verilator 跑一 cycle。模板拼写错误、条件赋值反、`always_ff` 误用 `always_comb` 等 codegen bug 抓不到。

**根因**：ADR-035 §M5（e2e tier）定义的是"VerilatorBackend 能跑出正确结果"，但**没有定义"足够多的真实设计在 VerilatorBackend 上跑过"**。本次 change 把 e2e tier 从"counter 跑通即可"提升为"覆盖核心 HDL 构造 + interpreter trace parity"。

## What Changes

- **Add** capability `verilator-e2e-coverage` under `openspec/specs/verilator-e2e-coverage/spec.md` 定义 verilator 仿真验证的覆盖契约：trace parity、bundle 嵌套、FIFO/Stream、模板 lint 门、命名规范 5 类 SHALL/MUST Requirement
- **Add** `tests/test_verilator_trace_parity.cpp`：在同一 fixture 上跑 interpreter 和 verilator 两个后端，逐 cycle 对比每个 `ch_out` port 的 `data_map_` 值，发现不匹配立即 REQUIRE fail
- **Add** `tests/test_verilator_bundle_e2e.cpp`：覆盖 bundle 多字段赋值、嵌套 bundle 字段访问、bundle `<<=` 链的 verilator 仿真
- **Add** `tests/test_verilator_stream_e2e.cpp`：覆盖 `ch_stream<T>` 在 verilator 仿真下的 valid/ready/payload 三元组时序
- **Add** `tests/test_verilator_memory_e2e.cpp`：覆盖**代表性 Memory 设计**——sync FIFO（`ch_mem` + rd/wr pointers + full/empty）和 AXI4-Full Slave（5-channel + burst 验证）——在 verilator 仿真下的行为
- **Add** `tests/test_verilator_complex_e2e.cpp`：覆盖**代表性复杂设计**——RV32I 5-stage 流水线（IF/ID/EX/MEM/WB 短指令序列 trace parity）和 SPI 控制器 FSM（`ch_state_machine` 4-state）——在 verilator 仿真下的行为
- **Modify** `tests/test_verilog_{arith,compare,shift,gen}.cpp`：在**至少 8 个最有代表性的 TEST_CASE** 末尾原地追加 `popen("verilator --lint-only -Wno-fatal -Wno-WIDTH")` 真 lint 断言（保留 regex 断言作为 fast path，不新建 test 文件）
- **Modify** `tests/test_verilator_three_way.cpp` → `tests/test_codegen_literal_mask.cpp`（仅改名 + 修正 `[verilog][verilator]` tag 为 `[verilog][codegen][literal-mask]`，不动逻辑；test 实质仅是 codegen 字符串断言）
- **Modify** `tests/CMakeLists.txt`：注册 4 个新 test 目标；把 `test_verilator_three_way` 改名为 `test_codegen_literal_mask`，保留 ctest alias
- **Preserve** 现有 6 个 e2e 测试（`test_verilator_e2e_harness.cpp` + `test_axi_lite_verilator_e2e.cpp` + `test_verilator_backend.cpp::E2E CounterFiftyCycles` 等）不动

## 动机

- 6 个真仿真测试都基于 `CounterFixture<N>` 这一个 fixture。一个跑通不能说"VerilatorBackend 在所有设计上都对"——这是 spec-driven 工程的常识盲点
- 2026-10-14 设计类型穷举矩阵：`examples/` + `samples/` + `include/` + `tests/` 共识别出 **35 种**硬件设计类型，当前 verilator e2e 仿真只覆盖 **2 种**（basic counter、AXI4-Lite slave），覆盖度 2/35 ≈ **5.7%**。33 种设计（PWM / 同步 FIFO / SPI 控制器 / UART / 状态机 / 流水线 / Cache / 6 个 AXI4 外设 / ...）在 verilator 仿真下完全无回归门
- bundle / Stream / FIFO 是 SpinalHDL-style API 的**差异化竞争力**。这些构造的 verilator 仿真验证是 0，不是"未来工作"，是**当前回归门缺失**
- `test_verilator_three_way` 的命名是历史包袱（commit 567b752 引入时为 perf three-way 而非 verilator three-way）。改名是最低成本的诚信修复
- trace parity 是单测成本最低、信号最强的 verilator 回归门：interpreter 是 ground truth，verilator 是待验证；任何"verilator 整体偏移 N cycle"的 bug 立刻被 catch

## 目标

- **G1**：trace parity 单测覆盖至少 3 个 fixture（counter、AXI4-Lite 协议、bundle 多字段），每个跑 ≥ 20 cycles，逐 cycle 对比所有 output port
- **G2**：bundle 嵌套仿真覆盖至少 4 个场景（单层多字段、双层嵌套、bundle 数组、bundle `<<=` 链）
- **G3**：Stream/FIFO 仿真覆盖至少 2 个场景（handshake 单笔、back-pressure 多笔）
- **G4**：codegen 模板 lint 门升级至少 8 个 TEST_CASE，把"verilog 文本含 token"提升为"verilator 真 lint 通过"
- **G5**：`test_verilator_three_way.cpp` 改名 `test_codegen_literal_mask.cpp`，tag 修正，行为不变
- **G6**：154/154 ctest 全过 + 4 个新 test 全过 + `openspec validate` 通过
- **G7**（新增）：代表性 Memory 设计 verilator e2e 覆盖——sync FIFO（`ch_mem` + rd/wr pointer + full/empty 多 cycle 验证）和 AXI4-Full Slave（5-channel handshake + 至少 1 次 burst write+read）
- **G8**（新增）：代表性复杂设计 verilator e2e 覆盖——RV32I 5-stage 流水线（短指令序列 trace parity，比如 `addi x1, x0, 1; addi x2, x0, 2; add x3, x1, x2`）+ SPI 控制器 FSM（4-state `ch_state_machine` 状态转移 trace parity）

## 非目标

- **NG1**：**不修改 `VerilatorBackend` 实现**。本次 change 纯测试侧（c++ 测试 + CMake 注册 + 文件改名），不动 `src/core/verilator_backend.cpp` / `include/core/verilator_backend.h` 任何一行
- **NG2**：**不引入新依赖**。`popen("verilator --lint-only")` 走 subprocess（与 `test_verilog_external.cpp` 同模式），不引入 googletest 或 Catch2 插件
- **NG3**：**不重写 interpreter**。trace parity 单测以 interpreter 为 ground truth，不动 interpreter 实现
- **NG4**：**不修改 test_perf_subprocess_isolation**。perf 已有 3 路 trace 对比（interp / JIT / Verilator），但目标是 perf 数字不是 trace 等价性，本次不深化
- **NG5**：**不改 perf three-way 的 verilator 行为**。本次只针对 Catch2 单元测试层面
- **NG6**：**不引入 GPU / 并行仿真 / cycle-accurate 多 DUT 编排**。每个 e2e 测试是单 DUT 单进程
- **NG7**：**不修复 pre-existing `test_verilator_three_way` 失败**（ch_uint<8>(256) CH_CHECK 路径已通过 `fix-pre-existing-failures-and-warnings` 撤销 7 个不可达 case，剩 5 个 working case；本次改名不改行为）

## 关联变更 / 依赖

- **前置 ADR-035 §M5**：`docs/adr/ADR-035-verilator-backend.md` §5 e2e tier 定义"VerilatorBackend 能跑出正确结果"为真仿真基础；本 change 是 §M5 的覆盖深化
- **关联 verilator-backend-e2e spec**：`openspec/specs/verilator-backend-e2e/spec.md` 已定义 dlopen / data_map 同步 / 3 步时钟 / trace parity 的低层契约；本 change 在其上**新增覆盖广度**而非**修改低层契约**
- **关联 verilator-error-flow spec**：`openspec/specs/verilator-error-flow/spec.md` 提供 8 个错误码，本 change 的新测试在 verilator 编译失败时 SKIP（与现有 e2e 测试同模式）
- **关联 test_verilog_external.cpp**：本次 lint 升级的 `tests/test_verilog_{arith,compare,shift,gen}.cpp` 原地修改借鉴其 `popen("verilator --lint-only")` 模式，不创建新 test binary

## 验收

详见 `tasks.md` 的 TDD 7 阶段结构与 `specs/verilator-e2e-coverage/spec.md` 的 SHALL/MUST Scenario。
