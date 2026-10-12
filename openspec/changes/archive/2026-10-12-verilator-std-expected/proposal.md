# Proposal: verilator-std-expected

> 引入 C++23 `std::expected<T, E>` 替换 VerilatorBackend 当前的 `bool` 错误流，让 Verilator 错误成为类型契约的一部分（而非通过 `bool` 返回 + `CHERROR`/`CHWARN` 旁路日志隐式传达）。

## Why

`VerilatorBackend` 当前用 `bool` 表达 6 个失败点（`initialize()` 公共 + `generate_verilog()` / `invoke_verilator()` / `dlopen_top()` / `build_port_access_table()` / `writeback_to_cache()` 私有），错误细节全部旁路到 `CHERROR` / `CHWARN` 宏：

- **类型不安全**：`bool` 丢失了失败原因——调用方拿到 `false` 不知道是 verilator 编译失败、dlopen 失败、还是 port bitwidth 溢出
- **错误流不可组合**：每个 `CHERROR` 立即 abort / `return false` 后无法把错误向上传递给 test assertion 或用户层 handler
- **测试只能检查"成败"**：4 个 test 文件（`test_verilator_e2e_harness.cpp`, `test_axi_lite_verilator_e2e.cpp`, `test_verilator_backend.cpp`, `test_verilator_three_way.cpp`）只能 `REQUIRE(backend.initialize(...))`，无法断言"具体是哪个错误"
- **ADR-035 §5 风险登记 R1/R5/R8/R10** 描述的 4 类失败模式（ABI 不兼容 / IO 字段名错 / codegen 回归 / 符号冲突）目前用相同的 `bool false` 表示，调试时必须 grep log

C++23 `std::expected<T, E>`（P0323R12）让 `T` 表示成功值、`E` 表示错误值，类型系统强制调用方 `or_else()` / `error()` 处理失败，避免静默错误丢弃。

## What Changes

- **Add** capability `verilator-error-flow` under `openspec/specs/verilator-error-flow/spec.md` defining the error type contract: 8 error code variants, required fields, and propagation rules through VerilatorBackend's 6 failure points
- **Add** `struct VerilatorError { VerilatorErrorCode code; std::string message; std::source_location location; }` in `include/core/verilator_backend.h`
- **Add** public method `std::expected<void, VerilatorError> VerilatorBackend::try_initialize(context*, data_map_t&)` returning rich error info
- **Modify** the existing `bool initialize(...)` to be a thin wrapper around `try_initialize()` that discards the error and returns `bool` (backward compat)
- **Modify** 5 private `bool` methods to return `std::expected<void, VerilatorError>`:
  - `generate_verilog(context*)` → `expected<void, E>`
  - `invoke_verilator(string)` → `expected<void, E>`
  - `dlopen_top(string)` → `expected<void, E>`
  - `build_port_access_table()` → `expected<void, E>`
  - `writeback_to_cache(string)` → `expected<void, E>`
- **Modify** all 12 `CHERROR` / `CHWARN` / `return false` sites in `src/core/verilator_backend.cpp` to populate `VerilatorError` via `std::unexpected(...)` propagation
- **Modify** the 2 example files (`examples/verilator/01_minimal_counter.cpp`, `02_counter_with_input.cpp`) to demonstrate `try_initialize()` with `or_else()` error reporting (existing `bool initialize()` calls remain compilable)
- **Add** unit tests in `tests/test_verilator_error_flow.cpp` covering each of the 8 error code variants with synthetic context (missing .so, missing symbol, port bitwidth overflow, etc.)
- **Preserve** `IEvalBackend` interface unchanged — other backends (InterpreterBackend, JITBackend) keep `bool initialize()` signature; std::expected is a Verilator-specific enrichment

## 动机

- ADR-035 §5 R1 / R5 / R8 / R10 风险登记的 4 类失败模式目前用同质 `bool false` 表达
- C++23 baseline 已确立（`openspec/specs/build-cpp-standard/spec.md` archive commit `f5e3479`），但 codebase 仍 0 命中 C++23 特性（grep `std::expected|std::print|deducing this|std::flat_map|std::span` 全部 0）；本 change 作为**首个 C++23 特性采用**建立模板
- `cpp23-standard-upgrade` change 的 NG1（非目标）明确把 "引入 C++23 特性" 推给独立 change；本 change 即兑现

## 目标

- **G1**：定义 `VerilatorError` 类型契约（8 个错误码变体 + 错误消息 + `std::source_location`），固化在 `openspec/specs/verilator-error-flow/spec.md`
- **G2**：让 Verilator 错误的 6 个失败点全部以 `std::expected` 类型表达，调用方能区分"哪一类错误"
- **G3**：保持 100% 向后兼容——现有 `bool initialize()` 调用、test 断言、example 调用方式 0 改动
- **G4**：建立 C++23 特性采用模板（首个 PR），为后续 `std::format` (codegen) / `std::flat_map` (AST) / deducing this (ch_uint) 提供 spec + 实现参考
- **G5**：错误信息携带 `std::source_location` (C++20 已有特性)，让 Verilator 错误 log 直接指向 `src/core/verilator_backend.cpp:XXX` 的失败源

## 非目标

- **NG1**：**不修改 `IEvalBackend` 接口**。`bool initialize()` 是 `IEvalBackend` 虚函数约定，改它会牵连 InterpreterBackend / JITBackend。本 change 仅在 `VerilatorBackend` 内部 + 新增 `try_initialize()` 公共方法
- **NG2**：**不修改 `Simulator::set_backend()`** 接受 `std::expected` 返回。`backend_->initialize()` 在 `src/simulator.cpp:536` 当前忽略返回值，本次不改正（独立 PR）
- **NG3**：**不修 pre-existing `test_verilator_three_way` 失败**（`ch_uint<8>(256)` CH_CHECK 断言，commit `567b752` 自报"all PASS"已不可复现），该 issue 与 `std::expected` 改造正交
- **NG4**：**不引入 `std::format` / `std::print` / deducing this / `std::flat_map` / `std::span` 等其他 C++23 特性**。后续各自独立 change
- **NG5**：**不修 Verilator R7 风险**（内存端口 `instr_mem*` 在 Verilator 后端无意义），独立 workstream
- **NG6**：**不引入新依赖**。`std::expected` 在 `<expected>` 头（libstdc++ 13+ 已有，GCC 13.3 已验证）

## 关联变更 / 依赖

- **前置**：`f5e3479 chore(openspec): archive cpp23-standard-upgrade` 已确立 C++23 baseline（`CMAKE_CXX_STANDARD 23` + libstdc++ 13.3+），本 change 是首个 C++23 特性采用
- **关联 ADR-035**：`docs/adr/ADR-035-verilator-backend.md` §5 风险登记 R1 / R5 / R8 / R10 是 `VerilatorErrorCode` 变体设计的依据
- **关联 Verilator 实现**：`include/core/verilator_backend.h` + `src/core/verilator_backend.cpp` 是核心改造点
- **关联下游**：`src/simulator.cpp:536` + 4 个 test 文件 + 2 个 example 文件需要在 0 改动前提下保持编译

## 验收

详见 `tasks.md` 的 TDD 5 步结构与 `specs/verilator-error-flow/spec.md` 的 SHALL/MUST Scenario。
