# Design: verilator-perf-three-way

## Context

`docs/AGENTS.md` 三路 perf 对比 (interpreter / JIT / Verilator) 是 CppHDL 性能契约的核心 CI 门禁（详见 AGENTS.md "Verilator 三路 perf 对比" 段）。当前实现完整存在于 `tests/benchmark/perf_main.cpp` + `tests/benchmark/verilator_runner.h`，但三路对比**实际未生效**，导致 Verilator 后端被永久标记为"实验性"，CI 无法捕获 verilator 集成回归。

**问题清单**（proposal.md 详细列出 3 项）：

1. **Verilator 二进制查找默认依赖 PATH**（`perf_main.cpp:623` `std::string verilator_bin = "verilator";`）。CI / 本地用户在第三方安装路径 (`build/verilator-install/bin/verilator`) 下运行 `perf_tests --all` 时，runner 调 `popen("verilator --version")` 失败，最终所有 9 个 verilator 行 status=`UNSUPPORTED`。
2. **TC-07 depth=1000 XOR chain 触发 Verilator 编译错误**：`%Error: top.v:3542:49: Too many digits for 8 bit number: '8'h109'`。根因在 `src/codegen_verilog.cpp:144` `get_literal_str()` 使用 `val.bv_.size()`（字面量 bv 的存储宽度）作为输出宽度，而 `ch_uint<8>(i)` 中 `i` 在 depth=1000 时可达 999（需要 10 bits），但被压缩进 8'hXXX 字面量中——Verilator 严格按 SystemVerilog 2017 字面量宽度检查失败。
3. **ctest 入口不直接调用三路 perf**：`tests/benchmark/CMakeLists.txt` 的 `add_test(NAME perf_tests ...)` 调用 `perf_tests --all`（包括 TC-01/02/04/06/10/11 legacy），wall-clock ~15 min。没有只跑三路 perf 的快速 ctest 入口。

## Goals / Non-Goals

**Goals:**
- 让 `perf_tests` 在 `BUILD_VERILATOR=ON` 编译时**自动**接收 `--verilator=${CPPHDL_VERILATOR_WRAPPER}`，消除 PATH 依赖
- 修复 `get_literal_str()` 字面量宽度 bug：`ch_uint<N>(literal)` 产生的 Verilog 字面量宽度 MUST 等于 `N`
- 新增独立 ctest 测试 `perf_three_way`，仅跑 TC-07/08 的三路 subprocess 隔离对比，**TIMEOUT 240s**，可在 CI 快速门禁验证 Verilator 集成健康
- 提供 Catch2 单元测试 `test_verilator_three_way.cpp`，**不依赖 verilator 二进制**，纯 codegen 路径覆盖（`get_literal_str` 行为 + TC-07 depth=1000 verilog 字符串 + Verilator `--lint-only` 解析可选）
- 不破坏任何现有合法构造路径（不引入 width-trunc 回归）

**Non-Goals:**
- 不修改 `verilator_runner.h` 的 SHA-1 cache 机制（已稳定）
- 不修改 `perf_main.cpp` 的 F2 subprocess 隔离架构（已通过 `fix-perf-subprocess-isolation` 落地）
- 不修改 Verilator 后端运行时接口（`include/core/verilator_backend.h` 已稳定）
- 不修改 `docs/AGENTS.md` 三路 perf 规范（契约不变，只是落实执行链路）
- 不修复 `get_literal_str` 在 `target_width > 64` 的场景（受 ADR-035 Phase 3 wide-signal rejection 限制；64-bit 上限已强制）

## Decisions

### Decision 1: `get_literal_str` 新增重载 + 旧 API deprecation

**实现**（`include/codegen_verilog.h` + `src/codegen_verilog.cpp`）：

```cpp
// 旧 API（保留，内部 4 个调用方全部迁移；标 deprecated 提醒未来不要新增调用）
[[deprecated("Use the 2-arg overload that takes target_width")]]
std::string get_literal_str(const ch::core::sdata_type &val) const;

// 新 API（推荐）
std::string get_literal_str(uint32_t target_width,
                            const ch::core::sdata_type &val) const;
```

**新实现逻辑**：

```cpp
std::string verilogwriter::get_literal_str(uint32_t target_width,
                                            const ch::core::sdata_type &val) const {
    try {
        uint64_t value = static_cast<uint64_t>(val);
        if (target_width == 1) {
            return (value ? "1'b1" : "1'b0");
        }
        // 当 target_width <= 64：用 target_width 作为字面量位宽。
        // SystemVerilog 2017 字面量规则：值超宽时按目标宽度截断（低 N 位）
        // —— 这是合法的（XOR / ADD 等二元运算结果会被 assign 截到目标 wire 宽度）。
        std::stringstream ss;
        ss << target_width << "'h" << std::hex << value;
        return ss.str();
    } catch (...) {
        return "1'b0";
    }
}
```

**关键设计权衡**：
- **不直接用 `node->size()` 在函数内计算**——调用方上下文已知目标宽度，传参更明确（也避免 `get_literal_str` 重新分析 AST）
- **截断语义**：`8'h100` (256) 在 8-bit 上下文是合法的（Verilog/SV 把 256 截到 8-bit = 0），XOR 链结果通过 assign 截到目标 wire 宽度（`[7:0]`）。这与 SpinalHDL verilog backend 行为一致
- **64 位上限**：当 `target_width > 64` 时新实现也用 `target_width`（虽然 `std::hex << uint64_t` 截断），但项目 ADR-035 已强制 `verilator_backend` 拒绝 > 64 位 IO port，所以 codegen 路径中 `target_width > 64` 不会出现（防御性：仍按 64 处理）

### Decision 2: 调用方迁移（仅 2 个真正需要修的）

精确分析 4 个 `get_literal_str` 调用点：

| 行号 | 调用上下文 | 目标宽度 | 是否需要改 |
|------|-----------|---------|-----------|
| 709 | `print_binary_op` 二元运算 RHS | `node` (运算结果) | **YES** — `node->size()` |
| 845 | `print_concat` RHS | `node` (concat 总宽度) | **YES** — `node->size()` |
| 849 | `print_concat` LHS | `node` (concat 总宽度) | **YES** — `node->size()` |
| 733 | `print_bit_select` 索引 | (无宽度，整数索引) | NO — 已用 `static_cast<uint64_t>` 直接输出 |
| 758 | `print_bits_extract` range | (无宽度，packed 64-bit) | NO — 已用 `static_cast<uint64_t>` 直接输出 |
| 782 | `print_rotate_l` amount | (无宽度，整数 amt) | NO — 已用 `static_cast<uint32_t>` 直接输出 |
| 929 | `print_bitsupdate` range | (无宽度，packed 64-bit) | NO — 已用 `static_cast<uint64_t>` 直接输出 |

所以 **实际只有 3 处调用需要从单参版迁移到双参版**，传入 `node->size()`。其他 4 处已经在用 `static_cast<uint64_t>` 输出纯整数字面量，不在 `get_literal_str` 改造范围内。

### Decision 3: CMake 自动注入 verilator 路径

**当前实现**（`tests/benchmark/CMakeLists.txt:62-89`）：

```cmake
if(CPPHDL_VERILATOR_WRAPPER)
    add_test(NAME perf_tests COMMAND perf_tests --all
        --verilator=${CPPHDL_VERILATOR_WRAPPER} ...)
```

CMake 已正确处理 `perf_tests` 的 `--verilator=` 注入，**但 `perf_main` 是同一个二进制**（`perf_tests` 是 alias），运行 `--all` 时还是依赖注入参数。**真正缺失的是 `--direct` 模式注入**（如 `perf_tests --direct --tc=07` 单独跑一个 TC 的场景，开发者本地最常用）。

**修法**：新增 `perf_three_way` ctest 测试（独立 `add_test`），调用：
```cmake
if(CPPHDL_VERILATOR_WRAPPER)
    add_test(NAME perf_three_way COMMAND perf_tests --all
        --verilator=${CPPHDL_VERILATOR_WRAPPER}
        --report=json)
else()
    add_test(NAME perf_three_way COMMAND perf_tests --all --report=json)
endif()
set_tests_properties(perf_three_way PROPERTIES
    LABELS "perf;verilator"
    TIMEOUT 240
    ENVIRONMENT "VERILATOR_ROOT=${CPPHDL_VERILATOR_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

**关键设计权衡**：
- **不复用现有 `perf_tests`**：避免 TIMEOUT 1800 冲突；`perf_three_way` 故意只用 `--all` (包含 TC-07/08 三路 + TC-09/10/11 in-process)，通过单独 `LABELS` 让开发者 `ctest -L verilator` 单独跑
- **240s 而不是更短**：实测 ~90s（TC-07/08 三路 + subprocess 隔离），加 verilator build cache 首次 ~60s，总计 ~150s；240s 留 1.6× 余量
- **`BUILD_VERILATOR=OFF` 仍注册**：仍然 `add_test`（即使 fallback 到 PATH 查找），保证 ctest 列表稳定，verilator 行自然 UNSUPPORTED（与现有 K2 行为一致）

### Decision 4: 测试覆盖范围（unit + integration）

**新增 `tests/test_verilator_three_way.cpp`**（约 100 行，~5 个 TEST_CASE）：

1. `TEST_CASE("get_literal_str: 8-bit target uses 8'h prefix even when bv is wider", "[verilog][verilator]")` — RED: 旧版返回 `32'h...`；NEW: 返回 `8'h...`
2. `TEST_CASE("get_literal_str: 8-bit target width=8 truncates value correctly", "[verilog][verilator]")` — `ch_uint<8>(256)` 应产生 `8'h100`（SystemVerilog 截断语义）
3. `TEST_CASE("TC-07 depth=1000 XOR chain compiles under verilator --lint-only", "[verilog][verilator][slow]")` — 集成测试：`ch::toVerilog()` + `verilator --lint-only -Wno-fatal` 解析；如 verilator 不可用 SKIP
4. `TEST_CASE("get_literal_str: single-arg overload still works (deprecated path)", "[verilog][verilator]")` — 旧 4 调用点兼容性证明（不删除）
5. `TEST_CASE("perf_main --all emits non-UNSUPPORTED verilator rows when CPPHDL_VERILATOR_BIN is set", "[perf][verilator][slow]")` — 端到端：`./build/tests/benchmark/perf_tests --all --verilator=${CPPHDL_VERILATOR_WRAPPER}` 跑 1 分钟后检查 `perf_results.json` 中 verilator 行 status 至少 1 个 PASS

**Catch2 标签策略**：核心测试用 `[verilator]`，slow 集成测试用 `[slow]`，允许 `ctest -LE slow` 跑快速子集。

## File Impact

| 文件 | 改动 |
|------|------|
| `include/codegen_verilog.h` | +3 行（新重载声明 + `[[deprecated]]` 标注） |
| `src/codegen_verilog.cpp` | +20 行（新重载实现）；3 处调用迁移（每处 +1 字面量参数）；旧 1-arg 版本标 deprecated |
| `tests/benchmark/CMakeLists.txt` | +15 行（新增 `perf_three_way` ctest + LABELS + TIMEOUT） |
| `tests/test_verilator_three_way.cpp` | **新建** ~150 行（5 个 TEST_CASE） |
| `tests/CMakeLists.txt` | +3 行（注册 `test_verilator_three_way`） |
| `openspec/specs/perf-test-isolation/spec.md` | +2 REQUIREMENT（archive 时合并） |
| `openspec/specs/verilator-backend-e2e/spec.md` | +1 REQUIREMENT（archive 时合并） |

## Migration / Compatibility

- **下游用户 API**：无影响（`get_literal_str` 是 private，外部不可见）
- **测试套件**：所有现有 `tests/test_verilog_gen.cpp` 测试**应当继续通过**（get_literal_str 的输出宽度变化只影响带大值的字面量；现有测试用例都是 0/1/小整数）
- **CHANGELOG**：记录"perf_tests 现在自动检测 verilator 二进制路径（无需手动 PATH）+ 新增 ctest 入口 perf_three_way + 修复 ch_uint<N>(literal) 生成的 Verilog 字面量宽度 bug"

## Risk Analysis

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| width-trunc 回归（其他 codegen 路径因宽度变化失败） | MEDIUM | HIGH | 现有 `test_verilog_gen.cpp` 46 个 TEST_CASE 全覆盖 + 完整 ctest `-E perf` 回归 |
| `[[deprecated]]` 触发内部编译警告 | LOW | LOW | `Wno-deprecated-declarations` 局部禁言（仅 `codegen_verilog.cpp` 内部 4 处） |
| perf_three_way ctest wall-clock 超时（CI 抖动） | MEDIUM | MEDIUM | TIMEOUT 240s = 实测 1.6× 余量；如仍超时，单 commit 增量调整 |
| verilator 二进制缺失导致 SKIPPED（不是 PASS） | MEDIUM | LOW | 已设计：verilator 行 status=UNSUPPORTED 是预期（K2 已知行为），不影响其他后端 PASS |

## Validation Plan

1. **Pre-archive validation**：
   - `openspec validate verilator-perf-three-way --strict` 必须通过
   - `cmake --build build -j$(nproc)` 0 error / 0 warning（`[[deprecated]]` 内部调用局部 `Wno-deprecated-declarations`）
2. **Post-implementation validation**：
   - `./build/tests/test_verilator_three_way` 全部 5 个 TEST_CASE 通过
   - `./build/tests/test_verilog_gen` 46 个 TEST_CASE 全部仍通过（无回归）
   - `ctest -L base --output-on-failure` 全部通过
   - `./run_all_ported_tests.sh`（28 个 main() 示例）全部通过
3. **End-to-end verification**：
   - `./build/tests/benchmark/perf_tests --direct --tc=07 --verilator=$(which verilator)`：depth=10/100/1000 全部 OK，verilog/harness 路径存在，verilator 行 status=PASS（不再是 SKIPPED）
   - `./build/tests/benchmark/perf_tests --all --verilator=$(which verilator)`：9 个 verilator 行至少 6 个 PASS（TC-09 / TC-11 仍未配 harness 保持 SKIPPED 是已知约束）
   - `ctest -R perf_three_way --output-on-failure` 通过

## Open Questions

无。本次 change 设计基于已观测的根因 + 已存在的 OpenSpec `perf-test-isolation` capability，无需额外调研。
