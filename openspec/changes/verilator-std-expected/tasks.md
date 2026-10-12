# verilator-std-expected

> **STATUS (2026-10-12)**: 起草中。TDD 5 步结构 + 当前进度跟踪。

## 1. openspec spec 沉淀

- [ ] 1.1 创建 capability `verilator-error-flow` 在 `openspec/changes/verilator-std-expected/specs/verilator-error-flow/spec.md`
      ⏳ Pending（本 change 实施开始时写）
- [ ] 1.2 定义 `## Purpose` + `## Requirements`（含 SHALL/MUST 关键字）
      ⏳ Pending
- [ ] 1.3 定义 Scenario 覆盖 8 个错误码 + backward compat + ADR-035 风险映射
      ⏳ Pending
- [ ] 1.4 `openspec validate verilator-std-expected --strict` 通过
      ⏳ Pending

## 2. 头文件改造（include/core/verilator_backend.h）

- [ ] 2.1 添加 `#include <expected>` + `#include <source_location>`
      ⏳ Pending
- [ ] 2.2 添加 `enum class VerilatorErrorCode`（8 个变体）
      ⏳ Pending
- [ ] 2.3 添加 `struct VerilatorError { code, message, location }`
      ⏳ Pending
- [ ] 2.4 添加 `using VerilatorResult = std::expected<void, VerilatorError>`
      ⏳ Pending
- [ ] 2.5 添加 public `try_initialize(context*, data_map_t&)` 返回 `VerilatorResult`
      ⏳ Pending
- [ ] 2.6 把 5 个 private `bool` 方法改返回 `VerilatorResult`
      ⏳ Pending
- [ ] 2.7 把 `bool initialize()` override 改写为 `try_initialize().has_value()` wrapper
      ⏳ Pending
- [ ] 2.8 `cmake --build build` 通过头文件解析
      ⏳ Pending

## 3. 实现改造（src/core/verilator_backend.cpp）

- [ ] 3.1 改造 5 个 private 方法返回 `VerilatorResult`
      ⏳ Pending
- [ ] 3.2 改造 12 个 `CHERROR/CHWARN/return false` 站点为 `std::unexpected(VerilatorError{...})`
      ⏳ Pending
- [ ] 3.3 改造 `try_initialize()` 调用链用 `.and_then()` / `?` operator 链式传播错误
      ⏳ Pending
- [ ] 3.4 `cmake --build build` exit 0
      ⏳ Pending

## 4. RED：测试先行

- [ ] 4.1 创建 `tests/test_verilator_error_flow.cpp`
      ⏳ Pending
- [ ] 4.2 8 个 TEST_CASE 对应 8 个 `VerilatorErrorCode`（用 mock context / 假 .so 路径触发）
      ⏳ Pending
- [ ] 4.3 1 个 TEST_CASE 验证 `bool initialize()` wrapper 仍可用
      ⏳ Pending
- [ ] 4.4 1 个 TEST_CASE 验证 `try_initialize()` 返回的 `VerilatorError` 携带 `std::source_location`
      ⏳ Pending
- [ ] 4.5 `ctest -R test_verilator_error_flow` 全通过
      ⏳ Pending

## 5. 现有调用方兼容性

- [ ] 5.1 `src/simulator.cpp:536` 0 改动（`backend_->initialize()` 仍返回 bool）
      ⏳ Pending（compile 通过即验证）
- [ ] 5.2 4 个 test 文件（test_verilator_e2e_harness, test_axi_lite_verilator_e2e, test_verilator_backend, test_verilator_three_way）0 改动
      ⏳ Pending
- [ ] 5.3 `examples/verilator/01_minimal_counter.cpp` 升级示范 `try_initialize()` + `or_else()` 错误处理
      ⏳ Pending
- [ ] 5.4 `examples/verilator/02_counter_with_input.cpp` 升级示范
      ⏳ Pending
- [ ] 5.5 142/143 ctest 仍通过（pre-existing 1 失败不归本 change）
      ⏳ Pending

## 6. archive 前置检查

- [ ] 6.1 `openspec validate verilator-std-expected --strict` 通过
      ⏳ Pending
- [ ] 6.2 `openspec validate --specs --strict` 8/8 passed
      ⏳ Pending
- [ ] 6.3 `openspec archive verilator-std-expected`（CLI 自动合并 `specs/verilator-error-flow/spec.md` 的 `## ADDED Requirements` 到 `openspec/specs/verilator-error-flow/spec.md`）
      ⏳ Pending

## 7. 后续 PR（独立 change，不在本 scope）

- 引入 `std::format` for codegen（`get_literal_str` / 字面量格式化）
- 引入 deducing this for `ch_uint::value()` / `mask()` getter
- 引入 `std::flat_map` for AST `instructions_` 容器
- 引入 `std::span` for `data_map_` 视图
- 修 Verilator R7 风险（内存端口）
- 修 pre-existing `test_verilator_three_way` 失败（ch_uint<8>(256) CH_CHECK）
- 修 pre-existing `-Wswitch-enum` on `ast_nodes.cpp:81`
- `Simulator::set_backend()` 升级接受 `std::expected` 返回（IEvalBackend 跟进）

## 关联基线

- **C++23 baseline**：`f5e3479 chore(openspec): archive cpp23-standard-upgrade` 已确立
- **ADR-035 R1/R5/R8/R10**：4 个错误码变体直接对应
- **IEvalBackend 抽象**：`include/core/eval_backend.h` 保持 `bool initialize()` 签名不变
