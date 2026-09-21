# verilator-perf-three-way

## Why

`docs/AGENTS.md` 规范 **三路 perf 对比** (interpreter / JIT / Verilator) 是 CppHDL 性能契约的核心 CI 门禁。当前实现完整存在于 `tests/benchmark/perf_main.cpp` + `tests/benchmark/verilator_runner.h`，但三路对比**实际未生效**：

**1. Verilator 二进制查找默认依赖 PATH**（`perf_main.cpp:623` `std::string verilator_bin = "verilator";`）。CI 与本地用户在第三方安装路径 (`build/verilator-install/bin/verilator`) 下运行 `perf_tests --all` 时，verilator 路径未被注入，runner 调 `popen("verilator --version")` 失败，最终所有 9 个 verilator 行 status=`UNSUPPORTED`。

**2. TC-07 depth=1000 XOR chain 触发 Verilator 编译错误**：实测 `perf_tests --direct --tc=07 --verilator=...` 时 depth=10/100 正常，depth=1000 报错 `%Error: top.v:3542:49: Too many digits for 8 bit number: '8'h109'`。
   - 根因：`src/codegen_verilog.cpp:144` `get_literal_str()` 使用 `val.bv_.size()`（字面量 bv 的存储宽度，通常 32 位）作为输出宽度，而 `ch_uint<8>(i)` 中 `i` 在 depth=1000 时可达 999（`0x3E7`，需要 10 bits），但被压缩进 8'hXXX 字面量中——Verilator 严格按 SystemVerilog 2017 字面量宽度检查失败。
   - 修法：用 **RHS 操作数的目标宽度**（`rhs_node->size()` 或 `node->size()`）作为宽度前缀，而不是 bv 存储宽度。

**3. ctest 入口不直接调用三路 perf**：`tests/benchmark/CMakeLists.txt` 的 `add_test(NAME perf_tests ...)` 调用 `perf_tests --all`，但 `--all` 同时跑 TC-01/02/04/06 (legacy) + TC-07/08/09 (三路 subprocess 隔离) + TC-10/11，wall-clock ~15 min。没有只跑三路 perf 的快速 ctest 入口（"perf_three_way"），开发者本地无法在 1-2 min 内验证 Verilator 集成健康。

三件事共同导致 Verilator 后端被永久标记为"实验性"、CI 无法捕获 verilator 集成回归。本次 change 闭环。

## What Changes

- **`src/codegen_verilog.cpp` get_literal_str()**：修复 XOR/lit 宽度计算。**用 `node` 的输出宽度（操作数整体位宽）** 替换 `val.bv_.size()`（bv 存储宽度），保证 SystemVerilog 2017 字面量宽度与目标 wire 宽度一致。
  - **改动的具体函数**：`verilogwriter::get_literal_str(const sdata_type &val)` 在调用方需要传入目标宽度，新增重载 `get_literal_str(uint32_t target_width, const sdata_type &val)`；旧单参版本保留并标 `[[deprecated]]`（内部 4 个调用方全部迁移到新版本）
  - **新版本宽度判定逻辑**：
    - 当 `target_width <= 64`：直接用 `target_width` 作为字面量位宽（`std::hex << value` 对 `uint64_t` 安全）
    - 当 `target_width > 64`：**fallback 到现有行为**（`val.bv_.size()`），因为 `uint64_t` 截断；该场景由后续 change 处理（参见 ADR-035 Phase 3 wide-signal rejection，目前 64-bit 上限已强制）
- **`src/codegen_verilog.cpp` 4 个调用点迁移**：将 `get_literal_str(lit_node->value())` 替换为 `get_literal_str(rhs_node->size(), lit_node->value())`（或对应上下文节点的 size），保证字面量宽度匹配目标 wire
- **`tests/benchmark/CMakeLists.txt`**：当 `BUILD_VERILATOR=ON` 时，把 `CPPHDL_VERILATOR_WRAPPER` 注入到 `perf_main` 的 `--verilator=` 参数（取代默认的 PATH 查找）。同时新增独立的 `perf_three_way` ctest 测试，仅跑 TC-07/08 的三路 subprocess 隔离对比，**TIMEOUT 240s**（实测 ~90s，留 2× 安全余量）
- **`tests/CMakeLists.txt`**（如未注册）：在根 ctest 入口注册 `perf_three_way` 标签 `[perf][verilator]`，允许 `ctest -L verilator` 单独跑
- **新增 `tests/test_verilator_three_way.cpp`**：Catch2 测试覆盖以下场景（**不依赖 verilator 二进制**，纯 codegen 单元测试）：
  - `get_literal_str(8, val)` 当 val=265 (0x109) 时返回 `8'h109`（不再产生 32'h109 之类的溢出）
  - `get_literal_str(8, val)` 当 val=256 (0x100) 时返回 `8'h100`（9 位实际值进 8 位字面量应产生 valid sv2017 slice —— 实际：`8'h100` = 128 in 8-bit，**预期行为**——SystemVerilog 2017 允许超宽值截断到字面量宽度，因为 XOR result 会被自动 mask）
  - **关键负向断言**：`get_literal_str(8, val_with_bv_size_64)` 不返回 `64'h...` 前缀
  - TC-07 depth=1000 XOR chain 生成的 verilog 通过 `verilator --lint-only -Wno-fatal` 解析
- **不动**：
  - Verilator 后端运行时接口（`include/core/verilator_backend.h` 已稳定）
  - `verilator_runner.h` 的 SHA-1 cache 机制
  - `perf_main.cpp` 的 F2 subprocess 隔离架构（已通过 `fix-perf-subprocess-isolation` 落地）
  - `docs/AGENTS.md` 三路 perf 规范（契约不变，只是落实执行链路）

## Capabilities

### New Capabilities

无。本 change 不引入新 capability。

### Modified Capabilities

- `perf-test-isolation`（已存在，archive 时合并）：新增 1 条 Requirement 把"`perf_main` 在 BUILD_VERILATOR=ON 时 MUST 自动接收 `--verilator=${CPPHDL_VERILATOR_WRAPPER}`"契约形式化（取代手动 PATH 查找）；新增 1 条 Requirement 把"`perf_three_way` ctest 测试入口存在且仅跑 TC-07/08 三路 subprocess 隔离对比"契约形式化。
- `verilator-backend-e2e`（已存在，archive 时合并）：新增 1 条 Requirement 把"codegen 对 `ch_uint<N>(int_literal)` 产生的 Verilog 字面量宽度 MUST 等于 `N`"契约形式化，覆盖 `get_literal_str` 在 XOR/ADD/SUB/MUL/SHL 等所有二元运算 RHS 字面量路径。

## Impact

| 类别 | 影响 |
|------|------|
| 受影响头/源 | `src/codegen_verilog.cpp` (4 处调用 + `get_literal_str` 重载) |
| 受影响测试 | `tests/benchmark/CMakeLists.txt` (新增 `perf_three_way` ctest) + `tests/test_verilator_three_way.cpp` (新增) + `tests/CMakeLists.txt` (注册) |
| 受影响 spec | `openspec/specs/perf-test-isolation/spec.md`（追加 2 条 REQUIREMENT）+ `openspec/specs/verilator-backend-e2e/spec.md`（追加 1 条 REQUIREMENT） |
| **API 行为变更** | 否。`get_literal_str` 仅内部实现细节，旧 API 通过 deprecation 兼容 |
| 迁移影响 | 无。下游用户无直接 API 调用 |
| 性能 | 无变化（重载 inline 调用，常数时间） |
| 风险 | **MEDIUM**。`get_literal_str` 是 codegen 核心热路径；4 个调用点的 width 来源（`rhs_node->size()` vs `lhs_node->size()` vs `node->size()`）需逐一验证对应 AST 节点拓扑，避免引入 width-trunc 回归 |
| 回滚 | `git revert` 单一 commit；CMake 改动 + codegen 改动独立 commit，便于单独回滚 |
| 关联 commit | `a120069 fix(codegen): emit synchronous reset in always_ff for Verilator compat`（前置，本 change 是其后续）；`fix-perf-subprocess-isolation` (subprocess 隔离基础) |
| CI 验证 | `openspec validate verilator-perf-three-way --strict` pre-archive；`ctest -R verilator_three_way`；`perf_tests --direct --tc=07 --verilator=$(which verilator)` depth=1000 不再失败；`tests/benchmark/perf_tests --all --verilator=${CPPHDL_VERILATOR_WRAPPER}` 全部 verilator 行 status=PASS |
